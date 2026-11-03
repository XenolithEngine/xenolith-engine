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
#include "XL3dMeshPack.h"

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
	// binding 1 is the layered array
	auto layered = info.descriptor && info.descriptor->index == 1;
	auto &views = layered ? _arrayViews : _textureViews;
	auto &empty = layered ? _emptyArrayView : _emptyView;
	auto &view = info.index < views.size() ? views[info.index] : empty;
	if (!view || !_sampler) {
		return false;
	}
	info.imageView = view;
	info.sampler = _sampler;
	info.layout = core::AttachmentLayout::ShaderReadOnlyOptimal;
	return true;
}

uint32_t Scene3dInputHandle::enumerateDirtyDescriptors(const core::QueuePassHandle &,
		const core::PipelineDescriptor &desc, const core::DescriptorBinding &,
		const Callback<void(uint32_t)> &cb) const {
	for (uint32_t i = 0; i < desc.count; ++i) { cb(i); }
	return desc.count;
}

void Scene3dInputHandle::setTextures(Vector<Rc<core::ImageView>> &&views,
		Rc<core::ImageView> &&empty, Vector<Rc<core::ImageView>> &&arrays,
		Rc<core::ImageView> &&emptyArray, Rc<core::Sampler> &&sampler) {
	_textureViews = sp::move(views);
	_emptyView = sp::move(empty);
	_arrayViews = sp::move(arrays);
	_emptyArrayView = sp::move(emptyArray);
	_sampler = sp::move(sampler);
}

static bool Scene3dInputHandle_isLayered(const Rc<Texture> &texture) {
	return texture && texture->getImageData() && texture->getImageData()->arrayLayers.get() > 1;
}

uint32_t Scene3dInputHandle::acquireTexture(const Rc<Texture> &texture, bool &overflow) {
	// a texture still loading draws as its colour alone
	if (!texture || !texture->isLoaded() || !texture->getImageData()
			|| !texture->getImageData()->image) {
		return NoTexture;
	}

	auto &image = texture->getImageData()->image;
	auto layered = Scene3dInputHandle_isLayered(texture);
	auto &images = layered ? _imageArrays : _images;
	for (uint32_t i = 0; i < images.size(); ++i) {
		if (images[i] == image) {
			return i;
		}
	}

	if (images.size() >= (layered ? MaxTextureArrays : MaxTextures)) {
		overflow = true;
		return NoTexture;
	}

	images.emplace_back(image);
	return uint32_t(images.size() - 1);
}

bool Scene3dInputHandle::pack(xvk::DeviceMemoryPool *pool, const FrameContextHandle3d &input) {
	_views.clear();
	_draws.clear();
	_translucentDraws = 0;
	_images.clear();
	_imageArrays.clear();

	size_t vertexBytes = 0;
	size_t indexCount = 0;
	size_t drawCount = 0;
	for (auto &view : input.views) {
		for (auto &draw : view.draws) {
			if (draw.mesh && !draw.mesh->getIndexes().empty()) {
				// a mesh of a loaded pack is in GPU memory already
				if (!draw.gpu.vertexes) {
					vertexBytes += (draw.mesh->getVertexData().size() + 15) / 16 * 16;
					indexCount += draw.mesh->getIndexes().size();
				}
				++drawCount;
			}
		}
	}

	if (drawCount == 0) {
		setLastPassStats(Pass3dStats());
		return true;
	}

	Pass3dStats stats;
	stats.frameIndexBytes = indexCount * sizeof(uint32_t);

	// the meshes' vertexes, whatever their format, each from a 16-byte boundary
	Bytes vertexes;
	Vector<uint32_t> indexes;
	Vector<DrawData3d> draws;
	Vector<ViewData3d> views;
	vertexes.reserve(vertexBytes);
	indexes.reserve(indexCount);
	draws.reserve(drawCount);
	views.reserve(input.views.size());

	auto attachment = static_cast<Scene3dInputAttachment *>(_attachment.get());
	Vector<LightData3d> lights;

	bool overflow = false;

	// the materials and the blocks of a schema, each once, 16-byte aligned
	Bytes schemaData;
	Map<const MaterialData3d *, uint32_t> dataOffsets;
	bool materialMismatch = false;
	bool blockMismatch = false;
	bool formatMismatch = false;
	bool wrongKind = false;

	// the slot of a texture field: a texture of the other kind than the field's has none
	auto acquireField = [&](const MaterialTextureField3d &field, const Rc<Texture> &texture) {
		if (texture && texture->isLoaded() && texture->getImageData()
				&& Scene3dInputHandle_isLayered(texture)
						!= (field.kind == MaterialTextureKind3d::TextureArray)) {
			wrongKind = true;
			return NoTexture;
		}
		return acquireTexture(texture, overflow);
	};

	// the offset of a material or a block in the frame's buffer, written the first time a draw or
	// a view has it, with the frame's slots of its textures
	auto writeData = [&](const Rc<MaterialData3d> &data, bool block) {
		auto it = dataOffsets.find(data.get());
		if (it != dataOffsets.end()) {
			return it->second;
		}
		auto offset = uint32_t((schemaData.size() + 15) / 16 * 16);
		auto bytes = data->getData();
		schemaData.resize(offset + bytes.size());
		sprt::memcpy(schemaData.data() + offset, bytes.data(), bytes.size());

		auto schema = data->getSchema();
		auto textures = data->getTextures();
		for (uint32_t i = 0; i < schema->textureCount; ++i) {
			auto &field = schema->textures[i];
			auto slot = acquireField(field, textures[i]);
			sprt::memcpy(schemaData.data() + offset + field.offset, &slot, sizeof(uint32_t));
		}
		++(block ? stats.blocks : stats.materials);
		(block ? stats.blockBytes : stats.materialBytes) += bytes.size();
		dataOffsets.emplace(data.get(), offset);
		return offset;
	};

	bool tooManyLights = false;
	bool layeredMesh = false;
	for (auto &view : input.views) {
		// a record for every view, so a view's index is its place in the view buffer
		auto &record = _views.emplace_back(
				ViewRecord{view.viewport, uint32_t(_draws.size()), 0});
		auto &viewData = views.emplace_back(view.data);

		// its block, for the pipelines that read one
		auto viewBlock = view.block ? view.block->getSchema() : nullptr;
		if (view.block) {
			record.blockOffset = writeData(view.block, true);
		}

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

			auto meshVertexes = draw.mesh->getVertexData();
			auto meshIndexes = draw.mesh->getIndexes();

			// a pipeline that reads a material or a view's block of a schema draws only one of it:
			// otherwise the mesh pipeline draws the draw
			auto pipeline = draw.material.pipeline;
			auto schema = attachment->getMaterialSchema(pipeline);
			auto blockSchema = attachment->getBlockSchema(pipeline);
			auto noMaterial = schema
					&& (!draw.material.data || draw.material.data->getSchema() != schema);
			auto noBlock = blockSchema && blockSchema != viewBlock;
			if (noMaterial || noBlock) {
				materialMismatch = materialMismatch || noMaterial;
				blockMismatch = blockMismatch || noBlock;
				pipeline = MeshPipeline;
				schema = nullptr;
			}

			// a pipeline reads the vertexes of its format: a mesh of another one draws with the
			// mesh pipeline if it can - one of Vertex3d - and not at all otherwise
			if (draw.mesh->getFormat() != attachment->getVertexFormat(pipeline)) {
				formatMismatch = true;
				if (draw.mesh->getFormat() != &Vertex3dFormat) {
					continue;
				}
				pipeline = MeshPipeline;
				schema = nullptr;
			}

			if (draw.gpu.vertexes && draw.gpu.indexes) {
				_draws.emplace_back(DrawRecord{pipeline, uint32_t(draws.size()),
					uint32_t(meshIndexes.size()), draw.gpu.firstIndex, draw.gpu.vertexByteOffset,
					draw.gpu.vertexes, draw.gpu.indexes});
				++stats.packDraws;
			} else {
				auto offset = (vertexes.size() + 15) / 16 * 16;
				_draws.emplace_back(DrawRecord{pipeline, uint32_t(draws.size()),
					uint32_t(meshIndexes.size()), uint32_t(indexes.size()), uint32_t(offset)});
				vertexes.resize(offset);
				vertexes.insert(vertexes.end(), meshVertexes.data(),
						meshVertexes.data() + meshVertexes.size());
				indexes.insert(indexes.end(), meshIndexes.begin(), meshIndexes.end());
				stats.frameVertexBytes += meshVertexes.size();
				++stats.frameDraws;
			}
			if (attachment->isTranslucent(pipeline)) {
				_draws.back().translucent = true;
				++_translucentDraws;
			}

			// normals take the inverse transpose of the rotation and scale, without the move
			Mat4 normal = draw.transform;
			normal.m[12] = normal.m[13] = normal.m[14] = 0.0f;
			normal.inverse();
			normal.transpose();

			auto &c = draw.material.color;
			// the mesh pipeline samples no layered texture
			auto texture = draw.material.texture;
			if (Scene3dInputHandle_isLayered(texture)
					&& attachment->isDrawnAsMesh(pipeline)) {
				layeredMesh = true;
				texture = nullptr;
			}
			auto textureIdx = acquireTexture(texture, overflow);

			DrawData3d data;
			data.model = draw.transform;
			data.normal = normal;
			data.color = Vec4(c.r, c.g, c.b, c.a);
			data.textureIdx = textureIdx;
			data.uvScale = draw.material.uvScale;
			data.flags = draw.material.flags;
			data.textureIdx2 = acquireTexture(draw.material.texture2, overflow);
			for (uint32_t i = 0; i < 4; ++i) { data.params[i] = draw.material.params[i]; }
			data.textureIdx3 = acquireTexture(draw.material.texture3, overflow);
			draws.emplace_back(data);

			if (schema) {
				_draws.back().materialOffset = writeData(draw.material.data, false);
			}

			++record.drawCount;
		}
	}

	if (overflow) {
		log::source().warn("basic3d::Pass3d", "more than ", MaxTextures, " textures or ",
				MaxTextureArrays, " layered ones in a frame: the rest draw as their colour");
	}
	static sprt::atomic<bool> s_layeredMeshWarned = false;
	if (layeredMesh && !s_layeredMeshWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d",
				"the mesh pipeline samples no layered texture: such a material draws as its colour");
	}

	static sprt::atomic<bool> s_materialWarned = false;
	if (materialMismatch && !s_materialWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d",
				"a pipeline that reads a material of a schema has a draw without one, or with one "
				"of another schema: the mesh pipeline draws it");
	}
	static sprt::atomic<bool> s_blockWarned = false;
	if (blockMismatch && !s_blockWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d",
				"a pipeline that reads a view's block of a schema has a draw in a view without "
				"one, or with one of another schema: the mesh pipeline draws it");
	}
	static sprt::atomic<bool> s_formatWarned = false;
	if (formatMismatch && !s_formatWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d",
				"a mesh's vertexes are not of the format its pipeline reads: the mesh pipeline "
				"draws a mesh of Vertex3d, and one of another format is not drawn");
	}
	static sprt::atomic<bool> s_kindWarned = false;
	if (wrongKind && !s_kindWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d",
				"a material's or a block's texture is layered where its field is a texture2d, or "
				"the other way round: the field has no texture");
	}

	static sprt::atomic<bool> s_lightsWarned = false;
	if (tooManyLights && !s_lightsWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d", "a view has more than ", MaxLights,
				" lights: the rest are dropped");
	}

	// at least one record each, so the buffers exist for their pointers when every draw is from
	// a pack or there are no lights
	if (lights.empty()) {
		lights.emplace_back(LightData3d());
	}
	if (vertexes.empty()) {
		vertexes.resize(sizeof(Vertex3d));
		indexes.emplace_back(0);
	}
	stats.translucentDraws = _translucentDraws;
	stats.recordBytes = draws.size() * sizeof(DrawData3d) + views.size() * sizeof(ViewData3d)
			+ lights.size() * sizeof(LightData3d) + schemaData.size();
	if (schemaData.empty()) {
		schemaData.resize(16);
	}
	setLastPassStats(stats);

	_vertexes = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dVertexes"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, vertexes.size()));
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

	_materialData = pool->spawn(xvk::AllocationUsage::DeviceLocalHostVisible,
			core::BufferInfo(StringView("Basic3dMaterials"), core::BufferUsage::StorageBuffer,
					core::BufferUsage::ShaderDeviceAddress, schemaData.size()));

	if (!_vertexes || !_indexes || !_drawData || !_viewData || !_lightData || !_materialData) {
		_draws.clear();
		return false;
	}

	_vertexes->setData(vertexes);
	_indexes->setData(BytesView(reinterpret_cast<const uint8_t *>(indexes.data()),
			indexes.size() * sizeof(uint32_t)));
	_drawData->setData(BytesView(reinterpret_cast<const uint8_t *>(draws.data()),
			draws.size() * sizeof(DrawData3d)));
	_viewData->setData(BytesView(reinterpret_cast<const uint8_t *>(views.data()),
			views.size() * sizeof(ViewData3d)));
	_lightData->setData(BytesView(reinterpret_cast<const uint8_t *>(lights.data()),
			lights.size() * sizeof(LightData3d)));
	_materialData->setData(schemaData);
	return true;
}

void Scene3dInputAttachment::setPipelines(Vector<bool> &&pipelines, Vector<bool> &&translucent,
		Vector<const MaterialSchema3d *> &&materials, Vector<const MaterialSchema3d *> &&blocks,
		Vector<const VertexFormat3d *> &&formats) {
	_pipelines = sp::move(pipelines);
	_translucent = sp::move(translucent);
	_materials = sp::move(materials);
	_blocks = sp::move(blocks);
	_formats = sp::move(formats);
}

const VertexFormat3d *Scene3dInputAttachment::getVertexFormat(uint32_t pipeline) const {
	if (isDrawnAsMesh(pipeline) || pipeline >= _formats.size() || !_formats[pipeline]) {
		return &Vertex3dFormat;
	}
	return _formats[pipeline];
}

bool Scene3dInputAttachment::isDrawnAsMesh(uint32_t pipeline) const {
	return pipeline == MeshPipeline || pipeline >= _pipelines.size() || !_pipelines[pipeline];
}

bool Scene3dInputAttachment::isTranslucent(uint32_t pipeline) const {
	// one drawn as a mesh is opaque
	return !isDrawnAsMesh(pipeline) && pipeline < _translucent.size() && _translucent[pipeline];
}

const MaterialSchema3d *Scene3dInputAttachment::getBlockSchema(uint32_t pipeline) const {
	return !isDrawnAsMesh(pipeline) && pipeline < _blocks.size() ? _blocks[pipeline] : nullptr;
}

const MaterialSchema3d *Scene3dInputAttachment::getMaterialSchema(uint32_t pipeline) const {
	return !isDrawnAsMesh(pipeline) && pipeline < _materials.size() ? _materials[pipeline]
																	 : nullptr;
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

		Vector<Rc<core::ImageView>> arrays;
		for (auto &image : input->getImageArrays()) {
			arrays.emplace_back(device->makeImageView(image,
					image->getViewInfo(core::ImageViewInfo(image->getInfo(),
							core::ImageViewType::ImageView2DArray))));
		}
		Rc<core::ImageView> empty;
		Rc<core::ImageView> emptyArray;
		if (auto emptyImage = _data->queue->emptyImage) {
			if (!emptyImage->views.empty()) {
				empty = emptyImage->views.front()->view;
			}
			// the empty image's one layer, seen as an array, for the layered array's free slots
			if (emptyImage->image) {
				emptyArray = pass->acquireEmptyArrayView(*device, emptyImage->image);
			}
		}
		input->setTextures(sp::move(views), sp::move(empty), sp::move(arrays),
				sp::move(emptyArray), Rc<core::Sampler>(pass->acquireSampler(*device)));
	}

	return xvk::QueuePassHandle::prepare(q, sp::move(cb));
}

// Depth alone: the translucent subpass reads it as an input attachment, and a view of a format with
// stencil has both aspects, which a descriptor can't take
static core::ImageFormat Pass3d_selectDepthFormat(SpanView<core::ImageFormat> formats) {
	for (auto format : {core::ImageFormat::D32_SFLOAT, core::ImageFormat::X8_D24_UNORM_PACK32,
			 core::ImageFormat::D16_UNORM}) {
		for (auto &it : formats) {
			if (it == format) {
				return it;
			}
		}
	}
	log::source().error("basic3d::Pass3d", "the device has no depth-only format: the translucent ",
			"pipelines can't read the depth");
	return formats.empty() ? core::ImageFormat::Undefined : formats.front();
}

// The draws of one subpass, the opaque or the translucent ones, view by view
static void Pass3d_record(core::FrameQueue &q, const core::SubpassData &subpass,
		xvk::CommandBuffer &buf, Pass3d *pass, bool translucent) {
	auto attachment = q.getAttachment(pass->getInput());
	auto input = attachment ? static_cast<Scene3dInputHandle *>(attachment->handle.get()) : nullptr;
	if (!input || input->empty() || (translucent && !input->hasTranslucent())) {
		return;
	}

	// by a material's pipeline index: the mesh pipeline first, then the app's; a subpass has only
	// its own, null for the rest
	Vector<const core::GraphicPipelineData *> pipelines;
	for (auto &name : pass->getPipelineNames()) {
		pipelines.emplace_back(name.empty() ? nullptr : subpass.graphicPipelines.get(name));
	}
	if (pipelines.empty() || (!translucent && !pipelines.front())) {
		return;
	}

	auto extent = q.getFrame()->getFrameConstraints().extent;

	const core::GraphicPipelineData *bound = nullptr;
	buf.cmdBindIndexBuffer(input->getIndexes(), 0, VK_INDEX_TYPE_UINT32);

	// the frame's buffers, and those of the packs the draws come from, bound when they change
	core::BufferObject *frameIndexes = input->getIndexes().get();
	core::BufferObject *boundIndexes = frameIndexes;
	core::BufferObject *frameVertexes = input->getVertexes().get();
	core::BufferObject *boundVertexes = frameVertexes;
	auto vertexBase = buf.bindBufferAddress(frameVertexes);

	auto viewBase = buf.bindBufferAddress(input->getViewData().get());

	Constants3d pcb;
	pcb.drawPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getDrawData().get()));
	pcb.vertexPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getVertexes().get()));
	pcb.lightPointer = UVec2::convertFromPacked(buf.bindBufferAddress(input->getLightData().get()));
	pcb.padding = 0;
	auto materialBase = buf.bindBufferAddress(input->getMaterialData().get());

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
		pcb.blockPointer = UVec2::convertFromPacked(materialBase + view.blockOffset);
		++viewIdx;

		if (x1 <= x0 || y1 <= y0 || view.drawCount == 0) {
			continue;
		}

		bool any = false;
		for (uint32_t i = 0; i < view.drawCount && !any; ++i) {
			any = draws[view.firstDraw + i].translucent == translucent;
		}
		if (!any) {
			continue;
		}

		VkRect2D scissor{{int32_t(x0), int32_t(y0)}, {uint32_t(x1 - x0), uint32_t(y1 - y0)}};
		buf.cmdSetViewport(0, makeSpanView(&viewport, 1));
		buf.cmdSetScissor(0, makeSpanView(&scissor, 1));

		for (uint32_t i = 0; i < view.drawCount; ++i) {
			auto &d = draws[view.firstDraw + i];
			if (d.translucent != translucent) {
				continue;
			}
			// a translucent draw's pipeline is always in its subpass: one the pass has
			auto pipeline = d.pipeline < pipelines.size() ? pipelines[d.pipeline] : nullptr;
			if (!pipeline) {
				pass->warnUnknownPipeline(d.pipeline);
				pipeline = pipelines.front();
			}
			if (pipeline != bound) {
				// one layout for all of a subpass's: the descriptors stay bound across the switch
				buf.cmdBindPipelineWithDescriptors(pipeline);
				bound = pipeline;
			}
			// the mesh's vertexes from where they are in their buffer, so gl_VertexIndex is the
			// mesh's own index
			auto vertexes = d.vertexes ? d.vertexes.get() : frameVertexes;
			if (vertexes != boundVertexes) {
				vertexBase = buf.bindBufferAddress(vertexes);
				boundVertexes = vertexes;
			}
			pcb.vertexPointer = UVec2::convertFromPacked(vertexBase + d.vertexByteOffset);
			auto indexes = d.indexes ? d.indexes.get() : frameIndexes;
			if (indexes != boundIndexes) {
				buf.bindBuffer(indexes);
				buf.cmdBindIndexBuffer(static_cast<xvk::Buffer *>(indexes), 0, VK_INDEX_TYPE_UINT32);
				boundIndexes = indexes;
			}
			pcb.drawIndex = d.drawIndex;
			pcb.materialPointer = UVec2::convertFromPacked(materialBase + d.materialOffset);
			buf.cmdPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					BytesView(reinterpret_cast<const uint8_t *>(&pcb), sizeof(Constants3d)));
			buf.cmdDrawIndexed(d.indexCount, 1, d.firstIndex, 0, 0);
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
			builder.addProgramByRef(toString(it.name, "Frag"), it.frag), it.translucent,
			it.material, it.block, it.vertex});
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
	Vector<bool> available;
	Vector<bool> translucent;
	Vector<const MaterialSchema3d *> materials;
	Vector<const MaterialSchema3d *> blocks;
	Vector<const VertexFormat3d *> formats;
	bool anyTranslucent = false;
	for (auto &it : pipelines) {
		_pipelineNames.emplace_back(it.name);
		available.emplace_back(it.vert && it.frag);
		translucent.emplace_back(it.vert && it.frag && it.translucent);
		materials.emplace_back(it.material);
		blocks.emplace_back(it.block);
		formats.emplace_back(it.vertex);
		anyTranslucent = anyTranslucent || translucent.back();
	}
	if (auto attachment = dynamic_cast<Scene3dInputAttachment *>(input->attachment.get())) {
		attachment->setPipelines(sp::move(available), sp::move(translucent), sp::move(materials),
				sp::move(blocks), sp::move(formats));
	}

	auto colorPass = passBuilder.addAttachment(output);
	auto depthPass = passBuilder.addAttachment(depth);
	auto inputPass = passBuilder.addAttachment(input);

	auto addTextures = [&](DescriptorSetBuilder &setBuilder) {
		setBuilder.addDescriptorArray(inputPass, MaxTextures, DescriptorType::CombinedImageSampler,
				AttachmentLayout::ShaderReadOnlyOptimal);
		setBuilder.addDescriptorArray(inputPass, MaxTextureArrays,
				DescriptorType::CombinedImageSampler, AttachmentLayout::ShaderReadOnlyOptimal);
	};

	auto layout = passBuilder.addDescriptorLayout("Basic3dLayout",
			[&](PipelineLayoutBuilder &layoutBuilder) { layoutBuilder.addSet(addTextures); });

	auto addPipelines = [&](SubpassBuilder &subpassBuilder, const PipelineLayoutData *layout,
								bool translucent) {
		for (auto &it : pipelines) {
			if (!it.vert || !it.frag || it.translucent != translucent) {
				continue;
			}
			// a translucent one blends source over, the alpha too, so the output stays opaque
			// where it was
			subpassBuilder.addGraphicPipeline(it.name, layout->defaultFamily,
					Vector<SpecializationInfo>({
						SpecializationInfo(it.vert),
						SpecializationInfo(it.frag),
					}),
					PipelineMaterialInfo(translucent
									? BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
											  BlendOp::Add, BlendFactor::One,
											  BlendFactor::OneMinusSrcAlpha, BlendOp::Add)
									: BlendInfo(),
							DepthInfo(!translucent, true, CompareOp::Less),
							RasterizationInfo(CullMode::Back, FrontFace::CounterClockwise)));
		}
	};

	auto opaque = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
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

		addPipelines(subpassBuilder, layout, false);

		subpassBuilder.setCommandsCallback([this](FrameQueue &q, const SubpassData &subpass,
												  core::CommandBuffer &commands) {
			Pass3d_record(q, subpass, static_cast<xvk::CommandBuffer &>(commands), this, false);
		});
	});

	// with no translucent pipeline, the pass is one subpass, as before them
	if (!anyTranslucent) {
		return xvk::QueuePass::init(passBuilder);
	}

	// the depth at binding 2, after the texture arrays, which keep their bindings
	auto translucentLayout = passBuilder.addDescriptorLayout("Basic3dTranslucentLayout",
			[&](PipelineLayoutBuilder &layoutBuilder) {
		layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
			addTextures(setBuilder);
			setBuilder.addDescriptor(depthPass, DescriptorType::InputAttachment,
					AttachmentLayout::DepthStencilReadOnlyOptimal);
		});
	});

	auto second = passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
		subpassBuilder.addColor(colorPass,
				AttachmentDependencyInfo{
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentRead | AccessType::ColorAttachmentWrite,
					PipelineStage::ColorAttachmentOutput,
					AccessType::ColorAttachmentRead | AccessType::ColorAttachmentWrite,
					FrameRenderPassState::Submitted,
				});

		// the depth is tested and read as an input in the same subpass: both in one read-only
		// layout, as an attachment can't be in two in a subpass
		subpassBuilder.setDepthStencil(depthPass,
				AttachmentDependencyInfo{
					PipelineStage::EarlyFragmentTest,
					AccessType::DepthStencilAttachmentRead,
					PipelineStage::LateFragmentTest,
					AccessType::DepthStencilAttachmentRead,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::DepthStencilReadOnlyOptimal);
		subpassBuilder.addInput(depthPass,
				AttachmentDependencyInfo{
					PipelineStage::FragmentShader,
					AccessType::InputAttachmantRead,
					PipelineStage::FragmentShader,
					AccessType::InputAttachmantRead,
					FrameRenderPassState::Submitted,
				},
				AttachmentLayout::DepthStencilReadOnlyOptimal);

		addPipelines(subpassBuilder, translucentLayout, true);

		subpassBuilder.setCommandsCallback([this](FrameQueue &q, const SubpassData &subpass,
												  core::CommandBuffer &commands) {
			Pass3d_record(q, subpass, static_cast<xvk::CommandBuffer &>(commands), this, true);
		});
	});

	passBuilder.addSubpassDependency(opaque,
			PipelineStage::ColorAttachmentOutput | PipelineStage::LateFragmentTest,
			AccessType::ColorAttachmentWrite | AccessType::DepthStencilAttachmentWrite, second,
			PipelineStage::EarlyFragmentTest | PipelineStage::FragmentShader
					| PipelineStage::ColorAttachmentOutput,
			AccessType::DepthStencilAttachmentRead | AccessType::InputAttachmantRead
					| AccessType::ColorAttachmentRead | AccessType::ColorAttachmentWrite,
			true);

	return xvk::QueuePass::init(passBuilder);
}

void Pass3d::warnUnknownPipeline(uint32_t index) {
	if (!_unknownPipelineWarned.exchange(true)) {
		log::source().warn("basic3d::Pass3d", "a material picks the pipeline ", index,
				", which the pass does not have: it draws with the mesh pipeline");
	}
}

const Rc<core::ImageView> &Pass3d::acquireEmptyArrayView(xvk::Device &device,
		const Rc<core::ImageObject> &image) {
	if (!_emptyArrayView) {
		_emptyArrayView = device.makeImageView(image,
				image->getViewInfo(core::ImageViewInfo(image->getInfo(),
						core::ImageViewType::ImageView2DArray)));
	}
	return _emptyArrayView;
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
