/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dVkPass.h"
#include "XL3dMesh.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreLoop.h"
#include "XLCoreQueueData.h"
#include "XLVkDevice.h"
#include "XLVkAllocator.h"
#include "XLVkDeviceQueue.h"
#include "XLVkRenderPass.h"
#include "glsl/XL3dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d::vk {

void Scene3dInputHandle::submitInput(core::FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	auto input = data.cast<FrameContextHandle3d>();
	if (!input || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, input = sp::move(input), cb = sp::move(cb)](core::FrameHandle &frame,
					bool success) mutable {
		if (!success || !frame.isValidFlag()) {
			cb(false);
			return;
		}

		auto pool = static_cast<xvk::DeviceFrameHandle &>(frame).getMemPool(this);
		frame.performInQueue(
				[this, input = sp::move(input), pool, cb = sp::move(cb)](core::FrameHandle &) mutable {
			cb(pack(pool, *input));
		}, this, "basic3d::Scene3dInputHandle::submitInput");
	});
}

bool Scene3dInputHandle::writeDescriptor(const core::QueuePassHandle &,
		core::DescriptorImageInfo &info) {
	auto &view = info.index < _textureViews.size() ? _textureViews[info.index] : _emptyView;
	if (!view || !_sampler) {
		return false;
	}
	info.imageView = view;
	info.sampler = _sampler;
	info.layout = core::AttachmentLayout::ShaderReadOnlyOptimal;
	return true;
}

uint32_t Scene3dInputHandle::enumerateDirtyDescriptors(const core::QueuePassHandle &,
		const core::PipelineDescriptor &, const core::DescriptorBinding &,
		const Callback<void(uint32_t)> &cb) const {
	for (uint32_t i = 0; i < MaxTextures; ++i) { cb(i); }
	return MaxTextures;
}

void Scene3dInputHandle::setTextures(Vector<Rc<core::ImageView>> &&views,
		Rc<core::ImageView> &&empty, Rc<core::Sampler> &&sampler) {
	_textureViews = sp::move(views);
	_emptyView = sp::move(empty);
	_sampler = sp::move(sampler);
}

uint32_t Scene3dInputHandle::acquireTexture(const Rc<Texture> &texture) {
	// a texture still loading draws as its colour alone
	if (!texture || !texture->isLoaded() || !texture->getImageData()
			|| !texture->getImageData()->image) {
		return NoTexture;
	}

	auto &image = texture->getImageData()->image;
	for (uint32_t i = 0; i < _images.size(); ++i) {
		if (_images[i] == image) {
			return i;
		}
	}
	if (_images.size() >= MaxTextures) {
		return NoTexture;
	}
	_images.emplace_back(image);
	return uint32_t(_images.size() - 1);
}

bool Scene3dInputHandle::pack(xvk::DeviceMemoryPool *pool, const FrameContextHandle3d &input) {
	_views.clear();
	_draws.clear();
	_images.clear();

	size_t vertexCount = 0;
	size_t indexCount = 0;
	size_t drawCount = 0;
	for (auto &view : input.views) {
		for (auto &draw : view.draws) {
			if (draw.mesh && !draw.mesh->getIndexes().empty()) {
				vertexCount += draw.mesh->getVertexes().size();
				indexCount += draw.mesh->getIndexes().size();
				++drawCount;
			}
		}
	}

	if (drawCount == 0) {
		return true;
	}

	Vector<Vertex3d> vertexes;
	Vector<uint32_t> indexes;
	Vector<DrawData3d> draws;
	Vector<ViewData3d> views;
	vertexes.reserve(vertexCount);
	indexes.reserve(indexCount);
	draws.reserve(drawCount);
	views.reserve(input.views.size());

	Vector<LightData3d> lights;
	bool overflow = false;
	bool tooManyLights = false;
	for (auto &view : input.views) {
		// a record for every view, so a view's index is its place in the view buffer
		auto &record = _views.emplace_back(
				ViewRecord{view.viewport, uint32_t(_draws.size()), 0});
		auto &viewData = views.emplace_back(view.data);

		// its lights, after those of the views before it
		viewData.firstLight = uint32_t(lights.size());
		viewData.lightCount = uint32_t(sprt::min(view.lights.size(), size_t(MaxLights)));
		viewData.padding0 = viewData.padding1 = 0;
		tooManyLights = tooManyLights || view.lights.size() > MaxLights;
		for (uint32_t i = 0; i < viewData.lightCount; ++i) {
			auto &l = view.lights[i];
			lights.emplace_back(LightData3d{
				Vec4(l.position.x, l.position.y, l.position.z, l.radius),
				Vec4(l.color.r, l.color.g, l.color.b, 1.0f),
			});
		}

		for (auto &draw : view.draws) {
			if (!draw.mesh || draw.mesh->getIndexes().empty()) {
				continue;
			}

			auto meshVertexes = draw.mesh->getVertexes();
			auto meshIndexes = draw.mesh->getIndexes();

			_draws.emplace_back(DrawRecord{draw.material.pipeline, uint32_t(draws.size()),
				uint32_t(meshIndexes.size()), uint32_t(indexes.size()), int32_t(vertexes.size())});
			vertexes.insert(vertexes.end(), meshVertexes.begin(), meshVertexes.end());
			indexes.insert(indexes.end(), meshIndexes.begin(), meshIndexes.end());

			// normals take the inverse transpose of the rotation and scale, without the move
			Mat4 normal = draw.transform;
			normal.m[12] = normal.m[13] = normal.m[14] = 0.0f;
			normal.inverse();
			normal.transpose();

			auto &c = draw.material.color;
			auto textureIdx = acquireTexture(draw.material.texture);
			if (draw.material.texture && textureIdx == NoTexture
					&& _images.size() >= MaxTextures) {
				overflow = true;
			}

			DrawData3d data;
			data.model = draw.transform;
			data.normal = normal;
			data.color = Vec4(c.r, c.g, c.b, c.a);
			data.textureIdx = textureIdx;
			data.uvScale = draw.material.uvScale;
			data.flags = draw.material.flags;
			data.textureIdx2 = acquireTexture(draw.material.texture2);
			if (draw.material.texture2 && data.textureIdx2 == NoTexture
					&& _images.size() >= MaxTextures) {
				overflow = true;
			}
			for (uint32_t i = 0; i < 4; ++i) { data.params[i] = draw.material.params[i]; }
			draws.emplace_back(data);

			++record.drawCount;
		}
	}

	if (overflow) {
		log::source().warn("basic3d::Pass3d", "more than ", MaxTextures,
				" textures in a frame: the rest draw as their colour");
	}

	static sprt::atomic<bool> s_lightsWarned = false;
	if (tooManyLights && !s_lightsWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d", "a view has more than ", MaxLights,
				" lights: the rest are dropped");
	}

	// at least one record, so the buffer exists for the pointer
	if (lights.empty()) {
		lights.emplace_back(LightData3d());
	}

	_vertexes = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dVertexes"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, vertexes.size() * sizeof(Vertex3d)));
	_indexes = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dIndexes"), core::BufferUsage::IndexBuffer,
					indexes.size() * sizeof(uint32_t)));
	_drawData = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dDraws"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, draws.size() * sizeof(DrawData3d)));
	_viewData = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dViews"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, views.size() * sizeof(ViewData3d)));

	_lightData = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dLights"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, lights.size() * sizeof(LightData3d)));

	if (!_vertexes || !_indexes || !_drawData || !_viewData || !_lightData) {
		_draws.clear();
		return false;
	}

	_vertexes->setData(BytesView(reinterpret_cast<const uint8_t *>(vertexes.data()),
			vertexes.size() * sizeof(Vertex3d)));
	_indexes->setData(BytesView(reinterpret_cast<const uint8_t *>(indexes.data()),
			indexes.size() * sizeof(uint32_t)));
	_drawData->setData(BytesView(reinterpret_cast<const uint8_t *>(draws.data()),
			draws.size() * sizeof(DrawData3d)));
	_viewData->setData(BytesView(reinterpret_cast<const uint8_t *>(views.data()),
			views.size() * sizeof(ViewData3d)));
	_lightData->setData(BytesView(reinterpret_cast<const uint8_t *>(lights.data()),
			lights.size() * sizeof(LightData3d)));
	return true;
}

bool Pass3dHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<Pass3d *>(_queuePass.get());
	auto device = static_cast<xvk::Device *>(q.getFrame()->getDevice());

	if (auto input = static_cast<Scene3dInputHandle *>(getAttachmentHandle(pass->getInput()))) {
		Vector<Rc<core::ImageView>> views;
		for (auto &image : input->getImages()) {
			views.emplace_back(device->makeImageView(image,
					image->getViewInfo(core::ImageViewInfo(image->getInfo()))));
		}

		Rc<core::ImageView> empty;
		if (auto emptyImage = _data->queue->emptyImage) {
			if (!emptyImage->views.empty()) {
				empty = emptyImage->views.front()->view;
			}
		}

		input->setTextures(sp::move(views), sp::move(empty),
				Rc<core::Sampler>(pass->acquireSampler(*device)));
	}

	return xvk::QueuePassHandle::prepare(q, sp::move(cb));
}

static core::ImageFormat Pass3d_selectDepthFormat(SpanView<core::ImageFormat> formats) {
	for (auto &it : formats) {
		if (it == core::ImageFormat::D32_SFLOAT) {
			return it;
		}
	}
	return formats.empty() ? core::ImageFormat::Undefined : formats.front();
}

static void Pass3d_record(core::FrameQueue &q, const core::SubpassData &subpass,
		xvk::CommandBuffer &buf, Pass3d *pass) {
	auto attachment = q.getAttachment(pass->getInput());
	auto input = attachment ? static_cast<Scene3dInputHandle *>(attachment->handle.get()) : nullptr;
	if (!input || input->empty()) {
		return;
	}

	// by a material's pipeline index: the mesh pipeline first, then the app's
	Vector<const core::GraphicPipelineData *> pipelines;
	for (auto &name : pass->getPipelineNames()) {
		pipelines.emplace_back(subpass.graphicPipelines.get(name));
	}
	if (pipelines.empty() || !pipelines.front()) {
		return;
	}

	auto extent = q.getFrame()->getFrameConstraints().extent;

	const core::GraphicPipelineData *bound = nullptr;
	buf.cmdBindIndexBuffer(input->getIndexes(), 0, VK_INDEX_TYPE_UINT32);

	auto viewBase = buf.bindBufferAddress(input->getViewData().get());

	Constants3d pcb;
	pcb.drawPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getDrawData().get()));
	pcb.vertexPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getVertexes().get()));
	pcb.lightPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getLightData().get()));
	pcb.padding = 0;

	auto draws = input->getDraws();
	uint32_t viewIdx = 0;
	for (auto &view : input->getViews()) {
		auto &r = view.viewport;

		// The viewport is the view's whole rect, which the camera's projection covers; the
		// scissor is that rect inside the frame. The scene's y is up, the framebuffer's is down.
		auto top = float(extent.height) - (r.origin.y + r.size.height);
		VkViewport viewport{r.origin.x, top, r.size.width, r.size.height, 0.0f, 1.0f};

		auto x0 = sprt::max(r.origin.x, 0.0f);
		auto y0 = sprt::max(top, 0.0f);
		auto x1 = sprt::min(r.origin.x + r.size.width, float(extent.width));
		auto y1 = sprt::min(top + r.size.height, float(extent.height));

		pcb.viewPointer = UVec2::convertFromPacked(viewBase + viewIdx * sizeof(ViewData3d));
		++viewIdx;

		if (x1 <= x0 || y1 <= y0 || view.drawCount == 0) {
			continue;
		}

		VkRect2D scissor{{int32_t(x0), int32_t(y0)}, {uint32_t(x1 - x0), uint32_t(y1 - y0)}};
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));
		buf.cmdSetScissor(0, makeSpanView(&scissor, 1));

		for (uint32_t i = 0; i < view.drawCount; ++i) {
			auto &d = draws[view.firstDraw + i];
			auto pipeline = d.pipeline < pipelines.size() ? pipelines[d.pipeline] : nullptr;
			if (!pipeline) {
				pass->warnUnknownPipeline(d.pipeline);
				pipeline = pipelines.front();
			}
			if (pipeline != bound) {
				// one layout for all of them: the descriptors stay bound across the switch
				buf.cmdBindPipelineWithDescriptors(pipeline);
				bound = pipeline;
			}
			pcb.drawIndex = d.drawIndex;
			buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&pcb), sizeof(Constants3d)));
			buf.cmdDrawIndexed(d.indexCount, 1, d.firstIndex, d.vertexOffset, 0);
		}
	}
}

bool Pass3d::addToQueue(core::Loop *loop, core::Queue::Builder &builder, Extent2 extent,
		SpanView<Pipeline3dInfo> appPipelines) {
	using namespace core;

	auto output = builder.getAttachment("Output");
	if (!output) {
		log::source().error("basic3d::Pass3d", "the queue has no Output: build basic2d's first");
		return false;
	}

	auto depth = builder.addAttachemnt(DepthAttachmentName,
			[&](AttachmentBuilder &b) -> Rc<Attachment> {
		return Rc<xvk::ImageAttachment>::create(b,
				ImageInfo(extent, ForceImageUsage(ImageUsage::DepthStencilAttachment),
						Pass3d_selectDepthFormat(loop->getSupportedDepthStencilFormat())),
				ImageAttachment::AttachmentInfo{.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::WHITE});
	});

	auto input = builder.addAttachemnt(FrameContext3d::InputAttachmentName,
			[](AttachmentBuilder &b) -> Rc<Attachment> {
		b.defineAsInput();
		b.setInputValidationCallback([](const AttachmentInputData *data) {
			return dynamic_cast<const FrameContextHandle3d *>(data) != nullptr;
		});
		return Rc<Scene3dInputAttachment>::create(b);
	});

	Vector<PipelineProgram> pipelines;
	pipelines.emplace_back(PipelineProgram{MeshPipelineName.str<Interface>(),
		builder.addProgramByRef("Basic3dMeshVert", shaders::MeshVert),
		builder.addProgramByRef("Basic3dMeshFrag", shaders::MeshFrag)});

	for (auto &it : appPipelines) {
		auto taken = sprt::find_if(pipelines.begin(), pipelines.end(),
				[&](const PipelineProgram &p) { return p.name == it.name; });
		if (it.name.empty() || taken != pipelines.end() || it.vert.empty() || it.frag.empty()) {
			log::source().error("basic3d::Pass3d", "the pipeline '", it.name,
					"' has no name, a taken one or no shaders: its materials draw as meshes");
			// keeps its index, with no pipeline under it
			pipelines.emplace_back(PipelineProgram{String(), nullptr, nullptr});
			continue;
		}
		pipelines.emplace_back(PipelineProgram{it.name,
			builder.addProgramByRef(toString(it.name, "Vert"), it.vert),
			builder.addProgramByRef(toString(it.name, "Frag"), it.frag)});
	}

	// before basic2d's pass (RenderOrderingHighest), which loads what this one drew
	builder.addPass("Basic3dPass", PassType::Graphics, RenderOrdering(10),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<Pass3d>::create(passBuilder, output, depth, input, sp::move(pipelines));
	});

	return true;
}

bool Pass3d::init(core::QueuePassBuilder &passBuilder, const core::AttachmentData *output,
		const core::AttachmentData *depth, const core::AttachmentData *input,
		Vector<PipelineProgram> &&pipelines) {
	using namespace core;

	_input = input;
	for (auto &it : pipelines) { _pipelineNames.emplace_back(it.name); }

	auto colorPass = passBuilder.addAttachment(output);
	auto depthPass = passBuilder.addAttachment(depth);
	auto inputPass = passBuilder.addAttachment(input);

	auto layout = passBuilder.addDescriptorLayout("Basic3dLayout",
			[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			setBuilder.addDescriptorArray(inputPass, MaxTextures,
					DescriptorType::CombinedImageSampler, AttachmentLayout::ShaderReadOnlyOptimal);
		});
	});

	passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorPass,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				});

		subpassBuilder.setDepthStencil(depthPass,
				AttachmentDependencyInfo{
					PipelineStage::EarlyFragmentTest,
					AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
					PipelineStage::LateFragmentTest,
					AccessType::DepthStencilAttachmentRead | AccessType::DepthStencilAttachmentWrite,
					FrameRenderPassState::Submitted,
				});

		for (auto &it : pipelines) {
			if (!it.vert || !it.frag) {
				continue;
			}
			subpassBuilder.addGraphicPipeline(it.name, layout->defaultFamily,
					Vector<SpecializationInfo>({
						SpecializationInfo(it.vert),
						SpecializationInfo(it.frag),
					}),
					PipelineMaterialInfo(BlendInfo(), DepthInfo(true, true, CompareOp::Less),
							RasterizationInfo(CullMode::Back, FrontFace::CounterClockwise)));
		}

		subpassBuilder.setCommandsCallback([this](FrameQueue &q, const SubpassData &subpass,
												  core::CommandBuffer &commands) {
			Pass3d_record(q, subpass, static_cast<xvk::CommandBuffer &>(commands), this);
		});
	});

	return xvk::QueuePass::init(passBuilder);
}

void Pass3d::warnUnknownPipeline(uint32_t index) {
	if (!_unknownPipelineWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d", "a material picks the pipeline ", index,
				", which the pass does not have: it draws with the mesh pipeline");
	}
}

const Rc<core::Sampler> &Pass3d::acquireSampler(xvk::Device &device) {
	if (!_sampler) {
		static const bool noMips = [] {
			auto value = ::getenv("BASIC3D_NOMIPS");
			return value && StringView(value) != "0";
		}();

		_sampler = Rc<xvk::Sampler>::create(device,
				core::SamplerInfo{
					.magFilter = core::Filter::Linear,
					.minFilter = core::Filter::Linear,
					.mipmapMode = core::SamplerMipmapMode::Linear,
					.addressModeU = core::SamplerAddressMode::Repeat,
					.addressModeV = core::SamplerAddressMode::Repeat,
					.addressModeW = core::SamplerAddressMode::Repeat,
					.anisotropyEnable = !noMips,
					.maxAnisotropy = 8.0f,
					.maxLod = noMips ? 0.0f : core::SamplerLodClampNone,
				});
	}
	return _sampler;
}

} // namespace stappler::xenolith::basic3d::vk
