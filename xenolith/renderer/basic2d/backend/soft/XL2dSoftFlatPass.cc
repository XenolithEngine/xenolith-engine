/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XL2dSoftFlatPass.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_SOFT

#include "XLSoftPipeline.h"
#include "XLSoftObject.h"
#include "XLSoftTextureSet.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
#include "glsl/XL2dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::soft {

class IgnoredInputAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~IgnoredInputAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&cb) override {
		cb(true);
	}
};

Rc<core::AttachmentHandle> IgnoredInputAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	return Rc<IgnoredInputAttachmentHandle>::create(*this, queue);
}

bool VertexAttachment::init(AttachmentBuilder &builder, const core::AttachmentData *materials) {
	if (!core::GenericAttachment::init(builder)) {
		return false;
	}

	_materials = materials;
	return true;
}

Rc<core::AttachmentHandle> VertexAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	return Rc<VertexAttachmentHandle>::create(*this, queue);
}

void VertexAttachmentHandle::submitInput(core::FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	auto d = data.cast<FrameContextHandle2d>();
	if (!d || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](core::FrameHandle &handle,
					bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		// The material set is owned by the loop, so the vertex stage's input has to be resolved
		// on the loop thread, like every other backend does.
		handle.performOnGlThread(
				[this, d = sp::move(d), cb = sp::move(cb)](core::FrameHandle &handle) mutable {
			cb(loadVertexes(handle, d));
		}, this, true, "VertexAttachmentHandle::submitInput");
	});
}

bool VertexAttachmentHandle::loadVertexes(core::FrameHandle &fhandle,
		const Rc<FrameContextHandle2d> &commands) {
	auto attachment = static_cast<VertexAttachment *>(_attachment.get());

	auto materialAttachment =
			static_cast<core::MaterialAttachment *>(attachment->getMaterials()->attachment.get());
	_materialSet = materialAttachment->getMaterials();
	if (!_materialSet) {
		log::source().error("basic2d::soft", "No material set for vertex attachment");
		return false;
	}

	auto &constraints = fhandle.getFrameConstraints();

	// The plan is built in a scratch pool: deferred results are duplicated into it, and nothing
	// survives past pushAll - by then every vertex has been copied into the arrays below.
	auto pool = memory::pool::create(memory::pool::acquire());
	auto ret = mem_pool::perform([&] {
		auto plan = new (pool) VertexPlan;
		plan->surfaceExtent = constraints.extent;
		plan->transform = constraints.transform;
		plan->flatOrder = true;
		plan->pool = pool;

		// There is no shader to probe a glyph atlas, so the plan resolves it on the CPU. That is
		// the same branch a Vulkan device without buffer device addresses takes.
		plan->hasGpuSideAtlases = false;

		auto shadowExtent = commands->lights.getShadowExtent(constraints.getScreenSize());
		auto shadowSize = commands->lights.getShadowSize(constraints.getScreenSize());
		plan->shadowSize = Vec2(shadowSize.width / float(shadowExtent.width),
				shadowSize.height / float(shadowExtent.height));

		VertexPlanContext ctx;
		ctx.input = commands;
		ctx.materialSet = _materialSet;

		for (auto cmd = commands->commands->getFirst(); cmd; cmd = cmd->next) {
			plan->pushCommand(ctx, cmd);
		}

		// Sized exactly like the Vulkan backend's device buffers, prologue slack included: the
		// plan writes absolute offsets into them and pushInitial owns the first 8/12/1 entries.
		const uint32_t predefinedTransforms = commands->commands->getPredefinedTransforms();

		_vertexes.clear();
		_indexes.clear();
		_transforms.clear();
		_vertexes.resize(plan->globalWritePlan.vertexes + 8);
		_indexes.resize(plan->globalWritePlan.indexes + 12);
		_transforms.resize(predefinedTransforms + plan->globalWritePlan.transforms + 1);

		VertexWriteTarget writeTarget;
		writeTarget.vertexes = reinterpret_cast<uint8_t *>(_vertexes.data());
		writeTarget.indexes = reinterpret_cast<uint8_t *>(_indexes.data());
		writeTarget.transform = _transforms.data();
		writeTarget.transtormOffset = predefinedTransforms;

		if (plan->isEmpty()) {
			plan->pushInitial(writeTarget);
		} else {
			plan->updatePathsDepth();
			plan->pushAll(ctx, writeTarget);
		}

		_spans = sp::move(ctx.materialSpans);
		_drawStates = commands->states;

		delete plan;
		return true;
	}, pool);
	memory::pool::destroy(pool);
	return ret;
}


bool FlatPass::makeRenderQueue(Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	builder.setDamageFlags(info.damage);

	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<FlatPass>::create(builder, passBuilder, info);
	});

	return true;
}

bool FlatPass::init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
		const RenderQueueInfo &info) {
	using namespace core;

	// Sampler order is baked into materials - keep it identical to basic2d::vk::FlatPass.
	core::SamplerInfo samplers[] = {
		SamplerInfo{
			.magFilter = Filter::Nearest,
			.minFilter = Filter::Nearest,
			.addressModeU = SamplerAddressMode::Repeat,
			.addressModeV = SamplerAddressMode::Repeat,
			.addressModeW = SamplerAddressMode::Repeat,
		},
		SamplerInfo{
			.magFilter = Filter::Linear,
			.minFilter = Filter::Linear,
			.addressModeU = SamplerAddressMode::Repeat,
			.addressModeV = SamplerAddressMode::Repeat,
			.addressModeW = SamplerAddressMode::Repeat,
		},
		SamplerInfo{
			.magFilter = Filter::Linear,
			.minFilter = Filter::Linear,
			.addressModeU = SamplerAddressMode::ClampToEdge,
			.addressModeV = SamplerAddressMode::ClampToEdge,
			.addressModeW = SamplerAddressMode::ClampToEdge,
		},
	};

	auto texLayout = queueBuilder.addTextureSetLayout("General", samplers);

	_output = queueBuilder.addAttachemnt("Output",
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsOutput();

		return Rc<sf::ImageAttachment>::create(builder,
				ImageInfo(info.extent, core::ForceImageUsage(core::ImageUsage::ColorAttachment),
						info.target->getCommonFormat()),
				core::ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = info.backgroundColor,
				});
	});

	_materials = queueBuilder.addAttachemnt(FrameContext2d::MaterialAttachmentName,
			[&](AttachmentBuilder &builder) -> Rc<Attachment> {
		return Rc<sf::MaterialAttachment>::create(builder, texLayout);
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::VertexAttachmentName,
			[&, this](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<VertexAttachment>::create(builder, _materials);
	});

	// FrameContext2d submits lights and particle emitters unconditionally and refuses to
	// initialize when the attachments are missing - accept and drop them.
	auto lights = queueBuilder.addAttachemnt(FrameContext2d::LightDataAttachmentName,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(builder);
	});

	auto particles = queueBuilder.addAttachemnt(FrameContext2d::ParticleEmittersAttachment,
			[](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(builder);
	});

	auto colorAttachment = passBuilder.addAttachment(_output);

	passBuilder.addAttachment(_vertexes);
	passBuilder.addAttachment(_materials);
	passBuilder.addAttachment(lights);
	passBuilder.addAttachment(particles);

	auto layout2d =
			passBuilder.addDescriptorLayout("Layout2d", [&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.setTextureSetLayout(texLayout);
	});

	passBuilder.addSubpass([&, this](SubpassBuilder &subpassBuilder) {
		makeMaterialSubpass(queueBuilder, subpassBuilder, layout2d, colorAttachment);
	});

	return core::QueuePass::init(passBuilder);
}

Rc<core::QueuePassHandle> FlatPass::makeFrameHandle(const FrameQueue &handle) {
	return Rc<FlatPassHandle>::create(*this, handle);
}

void FlatPass::makeMaterialSubpass(Queue::Builder &queueBuilder,
		core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d,
		const core::AttachmentPassData *colorAttachment) {
	using namespace core;

	// The SPIR-V is registered so the queue is described exactly as the Vulkan one, but it is
	// never read: soft::Shader keys the built-in C++ stages by program name.
	auto flatVert = queueBuilder.addProgramByRef("Loader_FlatVert", shaders::FlatVert);
	auto flatFrag = queueBuilder.addProgramByRef("Loader_FlatFrag", shaders::FlatFrag);

	Vector<SpecializationInfo> shaderSpecInfo({
		core::SpecializationInfo(flatVert),
		core::SpecializationInfo(flatFrag),
	});

	// PipelineMaterialInfo must stay byte-identical to basic2d::vk::FlatPass: materials are
	// matched to pipelines by this struct's value, and Sprite bakes DepthInfo into the request.
	// The depth state is inert here (there is no depth attachment), exactly as it is there.
	auto materialPipeline = subpassBuilder.addGraphicPipeline("Solid", layout2d->defaultFamily,
			shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView2D}));

	auto transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent",
			layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	// All six variants have to exist even though the kernels branch on the view type at record
	// time: a material is matched to a pipeline by the *value* of PipelineMaterialInfo, and one
	// that asks for an array or 3d view would otherwise find nothing - failing not with an error
	// but with an empty frame.
	auto blendInfo = BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendOp::Add,
			BlendFactor::Zero, BlendFactor::One, BlendOp::Add);

	subpassBuilder.addGraphicPipeline("Solid_Tex2dArrayFrag", layout2d->defaultFamily,
			shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView2DArray}));

	subpassBuilder.addGraphicPipeline("Transparent_Tex2dArrayFrag", layout2d->defaultFamily,
			shaderSpecInfo,
			PipelineMaterialInfo({blendInfo, DepthInfo(false, true, CompareOp::LessOrEqual),
				ImageViewType::ImageView2DArray}));

	subpassBuilder.addGraphicPipeline("Solid_Tex3dFrag", layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
				ImageViewType::ImageView3D}));

	subpassBuilder.addGraphicPipeline("Transparent_Tex3dFrag", layout2d->defaultFamily,
			shaderSpecInfo,
			PipelineMaterialInfo({blendInfo, DepthInfo(false, true, CompareOp::LessOrEqual),
				ImageViewType::ImageView3D}));

	// fallback materials for any Layer/Sprite that does not define its own
	static_cast<sf::MaterialAttachment *>(_materials->attachment.get())
			->addPredefinedMaterials(Vector<Rc<Material>>({
				Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
						layout2d->textureSetLayout->queue->emptyImage, ColorMode::IntensityChannel),
				Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
						layout2d->textureSetLayout->queue->solidImage, ColorMode::IntensityChannel),
				Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
						layout2d->textureSetLayout->queue->emptyImage, ColorMode()),
				Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
						layout2d->textureSetLayout->queue->solidImage, ColorMode()),
			}));

	subpassBuilder.addColor(colorAttachment,
			AttachmentDependencyInfo{
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				PipelineStage::ColorAttachmentOutput,
				AccessType::ColorAttachmentWrite,
				FrameRenderPassState::Submitted,
			},
			AttachmentLayout::ColorAttachmentOptimal);
}

bool FlatPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	if (auto vertexes = static_cast<FlatPass *>(_queuePass.get())->getVertexes()) {
		_vertexHandle = static_cast<VertexAttachmentHandle *>(getAttachmentHandle(vertexes));
	}

	return sf::QueuePassHandle::prepare(q, sp::move(cb));
}

namespace {

// A material's first image, resolved to what the rasterizer samples. This is the software
// counterpart of what vk::VertexPassHandle::prepareMaterialCommands puts into push constants
// (imageIdx/samplerIdx) - except that with no bindless array in the way, the view can simply be
// dereferenced.
struct ResolvedTexture {
	sf::raster::TextureKind kind = sf::raster::TextureKind::Solid;
	sf::raster::Texture texture;
	sf::raster::Sampler sampler;
	Color4F constant = Color4F::WHITE;
	const core::ImageView *view = nullptr;
	bool valid = false;
};

sf::raster::AddressMode FlatPass_addressMode(core::SamplerAddressMode mode) {
	switch (mode) {
	case core::SamplerAddressMode::Repeat: return sf::raster::AddressMode::Repeat;
	case core::SamplerAddressMode::MirroredRepeat: return sf::raster::AddressMode::MirroredRepeat;
	case core::SamplerAddressMode::ClampToEdge: return sf::raster::AddressMode::ClampToEdge;
	case core::SamplerAddressMode::ClampToBorder: return sf::raster::AddressMode::ClampToBorder;
	default: return sf::raster::AddressMode::ClampToEdge;
	}
}

ResolvedTexture FlatPass_resolveTexture(const core::Material *material,
		const core::TextureSetLayoutData *layout) {
	ResolvedTexture out;

	auto &image = material->getImages().front();
	auto view = image.view.get();
	if (!view) {
		return out;
	}

	auto img = view->getImage().get_cast<sf::Image>();
	if (!img) {
		return out;
	}

	auto &info = img->getInfo();
	auto &viewInfo = view->getInfo();

	out.view = view;
	out.texture.pixels = img->getData();
	out.texture.width = sprt::max(info.extent.width, 1U);
	out.texture.height = sprt::max(info.extent.height, 1U);
	out.texture.depth = sprt::max(info.extent.depth, 1U);
	out.texture.layers = sprt::max(info.arrayLayers.get(), 1U);
	out.texture.stride = img->getStride();
	out.texture.layerSize = img->getStride() * out.texture.height;
	out.texture.baseLayer = viewInfo.baseArrayLayer.get();
	out.texture.format = info.format;
	out.texture.swizzle[0] = viewInfo.r;
	out.texture.swizzle[1] = viewInfo.g;
	out.texture.swizzle[2] = viewInfo.b;
	out.texture.swizzle[3] = viewInfo.a;

	switch (viewInfo.type) {
	case core::ImageViewType::ImageView2DArray: out.kind = sf::raster::TextureKind::Texture2DArray; break;
	case core::ImageViewType::ImageView3D: out.kind = sf::raster::TextureKind::Texture3D; break;
	default: out.kind = sf::raster::TextureKind::Texture2D; break;
	}

	if (layout && image.sampler < layout->compiledSamplers.size()) {
		auto &samplerInfo = layout->compiledSamplers[image.sampler]->getInfo();
		out.sampler.filter = (samplerInfo.magFilter == core::Filter::Linear)
				? sf::raster::Filter::Linear
				: sf::raster::Filter::Nearest;
		out.sampler.addressU = FlatPass_addressMode(samplerInfo.addressModeU);
		out.sampler.addressV = FlatPass_addressMode(samplerInfo.addressModeV);
		out.sampler.addressW = FlatPass_addressMode(samplerInfo.addressModeW);
	}

	// A 1x1 image samples to the same value at every coordinate, so the fetch collapses into a
	// per-command constant. Every predefined material of the flat queue lands here, which is what
	// keeps a plain Layer byte-identical to the GPU - and it is not an approximation: the empty
	// image is not white, and folding its actual texel is exactly what the shader would compute.
	if (out.texture.width == 1 && out.texture.height == 1 && out.texture.depth == 1
			&& out.texture.layers == 1) {
		out.kind = sf::raster::TextureKind::Solid;
		out.constant = sf::raster::sampleConstant(out.texture);
	}

	out.valid = true;
	return out;
}

// Intersection of two clip rectangles, collapsing to empty rather than wrapping around zero.
URect FlatPass_intersect(const URect &l, const URect &r) {
	auto left = sprt::max(l.x, r.x);
	auto top = sprt::max(l.y, r.y);
	auto right = sprt::min(l.x + l.width, r.x + r.width);
	auto bottom = sprt::min(l.y + l.height, r.y + r.height);

	if (left >= right || top >= bottom) {
		return URect{0, 0, 0, 0};
	}
	return URect{left, top, right - left, bottom - top};
}

// One vertex through xl_2d_flat.vert, then through the viewport transform the GPU does in fixed
// function. `transformIndex` is the shader's `(vertex.material >> 16) + gl_InstanceIndex`.
sf::raster::Vertex FlatPass_runVertexStage(const Vertex &v, const TransformData &t,
		const sf::raster::Target &target) {
	// The layer is the vertex's z BEFORE the transform: makeMask clears z by default, and
	// transform.offset.z then carries the painter-order depth instead. Reading z afterwards would
	// silently sample a different array layer.
	float layer = v.pos.z;

	auto mask = glsl::makeMask(t.flags);

	auto pos = t.transform * v.pos;
	pos.x *= mask.x;
	pos.y *= mask.y;
	pos.z *= mask.z;
	pos.w *= mask.w;
	pos += t.offset;

	sf::raster::Vertex out;
	// Clip space to framebuffer pixels. The 2d pipeline is affine (w == 1), so there is no
	// perspective divide; a general path with one is a later concern.
	out.x = (pos.x * 0.5f + 0.5f) * float(target.width);
	out.y = (pos.y * 0.5f + 0.5f) * float(target.height);
	out.u = v.tex.x;
	out.v = v.tex.y;
	out.layer = layer;
	out.color = Color4F(v.color.x * t.instanceColor.x, v.color.y * t.instanceColor.y,
			v.color.z * t.instanceColor.z, v.color.w * t.instanceColor.w);
	return out;
}

} // namespace

void FlatPassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		sf::CommandBuffer &buf) {
	if (!_vertexHandle || _vertexHandle->empty()) {
		return;
	}

	auto materials = _vertexHandle->getMaterialSet();
	if (!materials) {
		return;
	}

	auto &target = buf.getTarget();
	auto &list = buf.getDrawList();
	auto &constraints = q.getFrame()->getFrameConstraints();

	auto srcVertexes = _vertexHandle->getVertexes();
	auto srcIndexes = _vertexHandle->getIndexes();
	auto transforms = _vertexHandle->getTransforms();
	auto states = _vertexHandle->getDrawStates();
	auto layout = materials->getTargetLayout();

	// A scene draws from a handful of images, so spans share textures; dedup by view.
	Map<const core::ImageView *, uint32_t> textureIndexes;

	uint32_t missingMaterials = 0;
	uint32_t brokenSpans = 0;

	for (auto &span : _vertexHandle->getSpans()) {
		// particle spans are degenerate by construction; the plan drops particles in flat mode
		if (span.indexCount == 0) {
			continue;
		}

		auto material = materials->getMaterialById(span.material);
		if (!material || material->getImages().empty()) {
			++missingMaterials;
			continue;
		}

		auto resolved = FlatPass_resolveTexture(material, layout);
		if (!resolved.valid) {
			++missingMaterials;
			continue;
		}

		uint32_t textureIndex = 0;
		if (resolved.kind != sf::raster::TextureKind::Solid) {
			auto it = textureIndexes.find(resolved.view);
			if (it == textureIndexes.end()) {
				textureIndex = uint32_t(list.textures.size());
				list.textures.emplace_back(resolved.texture);
				textureIndexes.emplace(resolved.view, textureIndex);
			} else {
				textureIndex = it->second;
			}
		}

		// The blend state is a property of the material's pipeline, and the pipeline object is
		// ours - so the rasterizer's mode comes straight out of it, with nothing to re-derive.
		auto blend = sf::BlendMode::Solid;
		if (auto pipelineData = material->getPipeline()) {
			if (auto pipeline = pipelineData->pipeline.get_cast<sf::GraphicPipeline>()) {
				blend = pipeline->getBlendMode();
			}
		}

		auto scissor = buf.getScissor();
		if (span.state != StateIdNone && span.state < states.size()) {
			auto &state = states[span.state];
			if (state.isScissorEnabled()) {
				scissor = FlatPass_intersect(scissor,
						sf::QueuePassHandle::rotateScissor(constraints, state.scissor));
			}
		}

		// gl_InstanceIndex is firstInstance + the instance number, and a packed draw has both at
		// zero. Each instance selects a different transform, so the vertex stage runs per instance.
		auto instanceCount = sprt::max(span.instanceCount, 1U);
		for (uint32_t instance = 0; instance < instanceCount; ++instance) {
			auto transformBase = span.firstInstance + instance;

			// The span's indexes address one contiguous block of the plan's vertex array; find it
			// once and transform it once, instead of per triangle corner.
			uint32_t minIndex = maxOf<uint32_t>();
			uint32_t maxIndex = 0;
			for (uint32_t i = 0; i < span.indexCount; ++i) {
				auto idx = srcIndexes[span.firstIndex + i] + span.vertexOffset;
				minIndex = sprt::min(minIndex, idx);
				maxIndex = sprt::max(maxIndex, idx);
			}

			if (size_t(span.firstIndex) + span.indexCount > srcIndexes.size()
					|| maxIndex >= srcVertexes.size()) {
				++brokenSpans;
				break;
			}

			const uint32_t base = uint32_t(list.vertexes.size());
			for (uint32_t idx = minIndex; idx <= maxIndex; ++idx) {
				auto &v = srcVertexes[idx];
				auto transformIndex = (v.material >> 16) + transformBase;
				if (transformIndex >= transforms.size()) {
					++brokenSpans;
					transformIndex = 0;
				}
				list.vertexes.emplace_back(
						FlatPass_runVertexStage(v, transforms[transformIndex], target));
			}

			sf::raster::Command command;
			command.firstIndex = uint32_t(list.indexes.size());
			command.indexCount = span.indexCount;
			command.blend = blend;
			command.scissor = scissor;
			command.kind = resolved.kind;
			command.texture = textureIndex;
			command.sampler = resolved.sampler;
			command.constantColor = resolved.constant;

			for (uint32_t i = 0; i < span.indexCount; ++i) {
				list.indexes.emplace_back(
						base + (srcIndexes[span.firstIndex + i] + span.vertexOffset - minIndex));
			}

			list.commands.emplace_back(command);
		}
	}

	if (missingMaterials > 0 || brokenSpans > 0) {
		log::source().warn("basic2d::soft", "Dropped ", missingMaterials,
				" span(s) with no usable material and ", brokenSpans,
				" span(s) with out-of-range indexes");
	}
}


} // namespace stappler::xenolith::basic2d::soft

#endif
