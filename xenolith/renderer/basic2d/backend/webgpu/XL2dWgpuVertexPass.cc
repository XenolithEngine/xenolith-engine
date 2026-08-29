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

#include "XL2dWgpuVertexPass.h"
#include "XL2dFrameContext.h"
#include "SPFont.h"
#include "XLWgpuLoop.h"
#include "XLWgpuTextureSet.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::webgpu {

static constexpr auto s_materialVertWgsl = StringView(R"wgsl(
struct Vertex {
	pos : vec4<f32>,
	color : vec4<f32>,
	tex : vec2<f32>,
	material : u32,
	object : u32,
};

struct TransformData {
	transform : mat4x4<f32>,
	offset : vec4<f32>,
	instanceColor : vec4<f32>,
	outlineColor : vec4<f32>,
	shadowValue : f32,
	padding0 : f32,
	outlineOffset : f32,
	flags : u32,
};

struct SpanData {
	samplerImageIdx : u32,
	instanceTransformIdx : u32,
	colorMode : u32,
	depth : f32,
	atlasOffset : u32,
	atlasSlots : u32,
};

@group(0) @binding(0) var<storage, read> vertices : array<Vertex>;
@group(0) @binding(1) var<storage, read> transforms : array<TransformData>;
@group(0) @binding(2) var<storage, read> spans : array<SpanData>;
// combined data-atlas hash tables (core::DataAtlas::getBufferData), slot
// stride = 6 u32: {key, value, pos.xy, tex.xy}, empty key = 0xffffffff
@group(0) @binding(3) var<storage, read> atlasData : array<u32>;

// must match the CPU builder (core DataAtlas hash)
fn atlasHash(key : u32, capacity : u32) -> u32 {
	var k = key;
	k ^= k >> 16u;
	k *= 0x85ebca6bu;
	k ^= k >> 13u;
	k *= 0xc2b2ae35u;
	k ^= k >> 16u;
	return k & (capacity - 1u);
}

struct VertexOutput {
	@builtin(position) position : vec4<f32>,
	@location(0) color : vec4<f32>,
	@location(1) tex : vec2<f32>,
	@location(2) @interpolate(flat) samplerImageIdx : u32,
	@location(3) @interpolate(flat) colorMode : u32,
};

fn makeMask(value : u32) -> vec4<f32> {
	return vec4<f32>(
		select(0.0, 1.0, (value & 1u) != 0u),
		select(0.0, 1.0, (value & 2u) != 0u),
		select(0.0, 1.0, (value & 4u) != 0u),
		select(0.0, 1.0, (value & 8u) != 0u));
}

@vertex
fn main(@builtin(vertex_index) vertexIdx : u32, @builtin(instance_index) instanceIdx : u32)
		-> VertexOutput {
	let span = spans[instanceIdx];
	let vertex = vertices[vertexIdx];
	let transform = transforms[vertex.material >> 16u];
	let instance = transforms[span.instanceTransformIdx];

	var pos = vertex.pos;
	var tex = vertex.tex;
	var color = vertex.color;

	// GPU-side data-atlas lookup (glyph placement), see xl_2d_material.vert
	if (span.atlasSlots != 0u && vertex.object != 0u) {
		var slot = atlasHash(vertex.object, span.atlasSlots);
		var counter = 0u;
		loop {
			if (counter >= span.atlasSlots) {
				color = vec4<f32>(0.0, 1.0, 0.0, 1.0); // lookup overflow marker
				break;
			}
			let base = span.atlasOffset + slot * 6u;
			let key = atlasData[base];
			if (key == vertex.object) {
				pos += vec4<f32>(bitcast<f32>(atlasData[base + 2u]),
						bitcast<f32>(atlasData[base + 3u]), 0.0, 0.0);
				tex = vec2<f32>(bitcast<f32>(atlasData[base + 4u]),
						bitcast<f32>(atlasData[base + 5u]));
				break;
			}
			if (key == 0xffffffffu) {
				color = vec4<f32>(1.0, 0.0, 0.0, 1.0); // missing object marker
				break;
			}
			slot = (slot + 1u) & (span.atlasSlots - 1u);
			counter = counter + 1u;
		}
	}

	let mask = makeMask(transform.flags);

	var out : VertexOutput;
	out.position = (transform.transform * instance.transform * (pos * mask * mask))
			+ transform.offset + instance.offset;
	// engine projection targets Vulkan NDC (Y down); WebGPU NDC is Y up
	out.position.y = -out.position.y;
	// painter-order depth: later spans win the depth test over earlier ones
	out.position.z = span.depth * out.position.w;
	out.color = color * transform.instanceColor * instance.instanceColor;
	out.tex = tex;
	out.samplerImageIdx = span.samplerImageIdx;
	out.colorMode = span.colorMode;
	return out;
}
)wgsl");

static constexpr auto s_materialFragWgsl = StringView(R"wgsl(
struct VertexOutput {
	@builtin(position) position : vec4<f32>,
	@location(0) color : vec4<f32>,
	@location(1) tex : vec2<f32>,
	@location(2) @interpolate(flat) samplerImageIdx : u32,
	@location(3) @interpolate(flat) colorMode : u32,
};

@group(1) @binding(0) var samplers : binding_array<sampler, 2>;
@group(1) @binding(1) var textures : binding_array<texture_2d<f32>, 8>;

// core::ComponentMapping: 0=identity 1=zero 2=one 3=R 4=G 5=B 6=A
fn swizzleComponent(c : vec4<f32>, m : u32, identity : f32) -> f32 {
	switch m {
		case 1u: { return 0.0; }
		case 2u: { return 1.0; }
		case 3u: { return c.r; }
		case 4u: { return c.g; }
		case 5u: { return c.b; }
		case 6u: { return c.a; }
		default: { return identity; }
	}
}

fn applyColorMode(c : vec4<f32>, mode : u32) -> vec4<f32> {
	if (mode == 0u) {
		return c;
	}
	return vec4<f32>(
		swizzleComponent(c, mode & 0xFu, c.r),
		swizzleComponent(c, (mode >> 4u) & 0xFu, c.g),
		swizzleComponent(c, (mode >> 8u) & 0xFu, c.b),
		swizzleComponent(c, (mode >> 12u) & 0xFu, c.a));
}

@fragment
fn main(in : VertexOutput) -> @location(0) vec4<f32> {
	let imageIdx = in.samplerImageIdx & 0xFFFFu;
	let samplerIdx = (in.samplerImageIdx >> 16u) & 0xFFFFu;
	let sampled = textureSampleLevel(textures[imageIdx], samplers[samplerIdx], in.tex, 0.0);
	return in.color * applyColorMode(sampled, in.colorMode);
}
)wgsl");

// standard-WebGPU variant (no binding arrays): one texture is bound per
// material draw, samplerImageIdx is unused
static constexpr auto s_materialFragWgslSingle = StringView(R"wgsl(
struct VertexOutput {
	@builtin(position) position : vec4<f32>,
	@location(0) color : vec4<f32>,
	@location(1) tex : vec2<f32>,
	@location(2) @interpolate(flat) samplerImageIdx : u32,
	@location(3) @interpolate(flat) colorMode : u32,
};

@group(1) @binding(0) var texSampler : sampler;
@group(1) @binding(1) var tex : texture_2d<f32>;

// core::ComponentMapping: 0=identity 1=zero 2=one 3=R 4=G 5=B 6=A
fn swizzleComponent(c : vec4<f32>, m : u32, identity : f32) -> f32 {
	switch m {
		case 1u: { return 0.0; }
		case 2u: { return 1.0; }
		case 3u: { return c.r; }
		case 4u: { return c.g; }
		case 5u: { return c.b; }
		case 6u: { return c.a; }
		default: { return identity; }
	}
}

fn applyColorMode(c : vec4<f32>, mode : u32) -> vec4<f32> {
	if (mode == 0u) {
		return c;
	}
	return vec4<f32>(
		swizzleComponent(c, mode & 0xFu, c.r),
		swizzleComponent(c, (mode >> 4u) & 0xFu, c.g),
		swizzleComponent(c, (mode >> 8u) & 0xFu, c.b),
		swizzleComponent(c, (mode >> 12u) & 0xFu, c.a));
}

@fragment
fn main(in : VertexOutput) -> @location(0) vec4<f32> {
	let sampled = textureSampleLevel(tex, texSampler, in.tex, 0.0);
	return in.color * applyColorMode(sampled, in.colorMode);
}
)wgsl");

StringView getMaterialVertexShader() { return s_materialVertWgsl; }

StringView getMaterialFragmentShader(bool bindlessTextures) {
	return bindlessTextures ? s_materialFragWgsl : s_materialFragWgslSingle;
}

bool VertexAttachment::init(AttachmentBuilder &builder, const core::AttachmentData *materials) {
	if (!core::GenericAttachment::init(builder)) {
		return false;
	}
	_materials = materials;
	return true;
}

Rc<core::AttachmentHandle> VertexAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}
	return Rc<VertexAttachmentHandle>::create(*this, queue);
}

void VertexAttachmentHandle::submitInput(core::FrameQueue &q,
		Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
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

		handle.performOnGlThread(
				[this, d = sp::move(d), cb = sp::move(cb)](core::FrameHandle &handle) mutable {
			cb(loadVertexes(handle, d));
		}, this, true, "VertexAttachmentHandle::submitInput");
	});
}

bool VertexAttachmentHandle::loadVertexes(core::FrameHandle &fhandle,
		const Rc<FrameContextHandle2d> &commands) {
	auto attachment = static_cast<VertexAttachment *>(_attachment.get());

	auto materialAttachment = static_cast<core::MaterialAttachment *>(
			attachment->getMaterials()->attachment.get());
	_materialSet = materialAttachment->getMaterials();
	if (!_materialSet) {
		log::source().error("basic2d::webgpu", "No material set for vertex attachment");
		return false;
	}

	// copy draw states (scissor rects) for record time; do not retain the
	// context - see getDrawStates
	_drawStates = commands->states;

	auto dev = static_cast<wg::Device *>(fhandle.getDevice());

	Vector<Vertex> vertexes;
	Vector<uint32_t> indexes;
	Vector<TransformData> transforms;
	Vector<SpanData> spanData;

	// slot 0: identity (baked per-vertex transform slot for the initial slice)
	transforms.emplace_back(TransformData());

	// depth by hierarchical z-order (as the vk write plan): commands arrive in
	// scene-graph traversal order, drawing order is defined by CmdInfo::zPath
	Map<SpanView<ZOrder>, float, ZOrderLess> paths;
	auto cmd = commands->commands->getFirst();
	while (cmd) {
		if (cmd->type == CommandType::VertexArray || cmd->type == CommandType::Deferred) {
			auto info = reinterpret_cast<const CmdInfo *>(cmd->data);
			paths.emplace(info->zPath, 0.0f);
		}
		cmd = cmd->next;
	}

	const float depthScale = 1.0f / float(paths.size() + 1);
	float depthOffset = 1.0f - depthScale;
	for (auto &it : paths) {
		it.second = depthOffset;
		depthOffset -= depthScale;
	}

	// data atlases referenced by this frame's spans: assign each a region in
	// the combined atlas buffer (offsets in u32 units, deterministic by first
	// use, so an unchanged atlas set reuses the cached buffer)
	Vector<const core::DataAtlas *> frameAtlases;
	Vector<uint32_t> frameAtlasOffsets;
	uint32_t atlasTotalWords = 0;

	auto acquireAtlasRegion = [&](const core::DataAtlas *atlas) -> sprt::pair<uint32_t, uint32_t> {
		constexpr uint32_t SlotStride = 6; // {key, value, pos.xy, tex.xy}

		for (size_t i = 0; i < frameAtlases.size(); ++i) {
			if (frameAtlases[i] == atlas) {
				return sprt::pair(frameAtlasOffsets[i],
						uint32_t(atlas->getBufferData().size() / (SlotStride * 4)));
			}
		}

		auto data = atlas->getBufferData();
		if (data.empty() || (data.size() % (SlotStride * 4)) != 0) {
			return sprt::pair(uint32_t(0), uint32_t(0));
		}

		frameAtlases.emplace_back(atlas);
		frameAtlasOffsets.emplace_back(atlasTotalWords);
		atlasTotalWords += uint32_t(data.size() / 4);
		return sprt::pair(frameAtlasOffsets.back(), uint32_t(data.size() / (SlotStride * 4)));
	};

	// shared span emitter for direct and deferred vertex data
	auto emitVertexes = [&](core::MaterialId materialId, SpanView<ZOrder> zPath,
								StateId state, const Rc<VertexData> &data,
								SpanView<TransformData> instances) {
		auto material = _materialSet->getMaterialById(materialId);
		if (!material || material->getImages().empty()) {
			log::source().warn("basic2d::webgpu", "No material for command: ", materialId);
			return;
		}

		auto &image = material->getImages().front();
		const uint32_t samplerImageIdx = image.descriptor | (uint32_t(image.sampler) << 16);
		// per-view ComponentMapping applied in the shader (no view swizzle)
		const uint32_t colorMode = toInt(image.info.r) | (toInt(image.info.g) << 4)
				| (toInt(image.info.b) << 8) | (toInt(image.info.a) << 12);

		// data atlases (glyph placement) resolve on the GPU against the
		// combined atlas buffer, like the vk backend; take the atlas from the
		// image data - it always matches the bound texture generation
		const core::DataAtlas *atlas = image.image ? image.image->atlas.get() : nullptr;
		if (!atlas) {
			atlas = material->getAtlas();
		}
		if (atlas
				&& (atlas->getType() != core::DataAtlas::ImageAtlas
						|| atlas->getObjectSize() != sizeof(font::FontAtlasValue))) {
			atlas = nullptr;
		}

		uint32_t atlasOffset = 0, atlasSlots = 0;
		if (atlas) {
			auto region = acquireAtlasRegion(atlas);
			atlasOffset = region.first;
			atlasSlots = region.second;
		}

		float spanDepth = 0.0f;
		auto pathIt = paths.find(zPath);
		if (pathIt != paths.end()) {
			spanDepth = pathIt->second;
		}

		const uint32_t vertexOffset = uint32_t(vertexes.size());
		const uint32_t firstIndex = uint32_t(indexes.size());
		const uint32_t indexCount = uint32_t(data->indexes.size());

		for (auto &v : data->data) {
			auto vertex = v;
			// per-vertex transform slot: identity (0) in the initial slice
			vertex.material = vertex.material & 0xFFFF;
			vertexes.emplace_back(vertex);
		}
		for (auto &index : data->indexes) { indexes.emplace_back(index); }

		for (auto &instance : instances) {
			const uint32_t instanceIdx = uint32_t(transforms.size());
			transforms.emplace_back(instance);

			const uint32_t spanIdx = uint32_t(spanData.size());
			spanData.emplace_back(SpanData{samplerImageIdx, instanceIdx, colorMode,
				spanDepth, atlasOffset, atlasSlots});

			VertexSpan span;
			span.material = materialId;
			span.indexCount = indexCount;
			span.instanceCount = 1;
			span.firstIndex = firstIndex;
			span.vertexOffset = vertexOffset;
			span.firstInstance = spanIdx;
			span.state = state;
			_spans.emplace_back(span);
		}
	};

	// Two walks, content then overlay: this backend emits spans straight in list order, so the only
	// way the Overlay level gets to be last is to visit it last. There is no frame capture here to
	// record in between, which is why one pass is enough for both.
	for (int overlayPass = 0; overlayPass < 2; ++overlayPass) {
		cmd = commands->commands->getFirst();
		while (cmd) {
			if (cmd->type == CommandType::VertexArray) {
				auto vertexCmd = reinterpret_cast<const CmdVertexArray *>(cmd->data);

				if ((vertexCmd->renderingLevel == RenderingLevel::Overlay) != bool(overlayPass)) {
					cmd = cmd->next;
					continue;
				}

				for (auto &iv : vertexCmd->vertexes) {
					emitVertexes(vertexCmd->material, vertexCmd->zPath, vertexCmd->state, iv.data,
							iv.instances);
				}
			} else if (cmd->type == CommandType::Deferred) {
				auto deferredCmd = reinterpret_cast<const CmdDeferred *>(cmd->data);

				if ((deferredCmd->renderingLevel == RenderingLevel::Overlay) != bool(overlayPass)) {
					cmd = cmd->next;
					continue;
				}

				// deferred tesselation results (e.g. labels): skip if not ready
				// and the producer did not request waiting
				if (!deferredCmd->deferred->isWaitOnReady() && !deferredCmd->deferred->isReady()) {
					cmd = cmd->next;
					continue;
				}

				deferredCmd->deferred->acquireResult(
						[&](SpanView<InstanceVertexData> result, DeferredVertexResult::Flags) {
					for (auto &iv : result) {
						// bake view/model transforms into per-span instances
						Vector<TransformData> instances;
						if (iv.instances.empty()) {
							instances.emplace_back(TransformData());
						} else {
							for (auto &inst : iv.instances) { instances.emplace_back(inst); }
						}

						for (auto &inst : instances) {
							if (deferredCmd->normalized) {
								auto modelTransform = deferredCmd->modelTransform * inst.transform;

								Mat4 newMV;
								newMV.m[12] = sprt::floor(modelTransform.m[12]);
								newMV.m[13] = sprt::floor(modelTransform.m[13]);
								newMV.m[14] = sprt::floor(modelTransform.m[14]);

								inst.transform = deferredCmd->viewTransform * newMV;
							} else {
								inst.transform = deferredCmd->viewTransform
										* deferredCmd->modelTransform * inst.transform;
							}
						}

						emitVertexes(deferredCmd->material, deferredCmd->zPath, deferredCmd->state,
								iv.data, instances);
					}
				});
			}
			cmd = cmd->next;
		}
	} // overlayPass

	if (auto path = ::getenv("XL_DUMP_QUADS")) {
		// steady-state frame dump: overwritten every frame, last write wins
		if (auto f = ::fopen(path, "w")) {
			for (size_t q = 0; q + 3 < vertexes.size(); q += 4) {
				float minU = 2, maxU = -2, minV = 2, maxV = -2, minX = 1e9, maxX = -1e9,
					  minY = 1e9, maxY = -1e9;
				for (size_t k = 0; k < 4; ++k) {
					auto &vv = vertexes[q + k];
					minU = sprt::min(minU, vv.tex.x); maxU = sprt::max(maxU, vv.tex.x);
					minV = sprt::min(minV, vv.tex.y); maxV = sprt::max(maxV, vv.tex.y);
					minX = sprt::min(minX, vv.pos.x); maxX = sprt::max(maxX, vv.pos.x);
					minY = sprt::min(minY, vv.pos.y); maxY = sprt::max(maxY, vv.pos.y);
				}
				::fprintf(f, "obj=%u ch=%u src=%u pos=%.1f,%.1f..%.1f,%.1f uv=%.4f,%.4f..%.4f,%.4f a=%.2f\n",
						vertexes[q].object, vertexes[q].object & 0xFFFF,
						uint32_t(vertexes[q].object) >> 18, minX, minY, maxX, maxY, minU, minV,
						maxU, maxV, vertexes[q].color.w);
			}
			::fclose(f);
		}
	}

	if (_spans.empty() || vertexes.empty()) {
		// empty scene: nothing to draw, pass still clears the output
		_indexes = nullptr;
		clearBufferViews();
		return true;
	}

	auto makeStorage = [&](StringView key, BytesView data) {
		return Rc<wg::Buffer>::create(*dev,
				core::BufferInfo(core::BufferUsage::StorageBuffer, uint64_t(data.size()), key),
				data);
	};

	auto vertexBuffer = makeStorage("2dVertexes",
			BytesView(reinterpret_cast<const uint8_t *>(vertexes.data()),
					vertexes.size() * sizeof(Vertex)));
	auto transformBuffer = makeStorage("2dTransforms",
			BytesView(reinterpret_cast<const uint8_t *>(transforms.data()),
					transforms.size() * sizeof(TransformData)));
	auto spanBuffer = makeStorage("2dSpans",
			BytesView(reinterpret_cast<const uint8_t *>(spanData.data()),
					spanData.size() * sizeof(SpanData)));

	_indexes = Rc<wg::Buffer>::create(*dev,
			core::BufferInfo(core::BufferUsage::IndexBuffer,
					uint64_t(indexes.size() * sizeof(uint32_t)), StringView("2dIndexes")),
			BytesView(reinterpret_cast<const uint8_t *>(indexes.data()),
					indexes.size() * sizeof(uint32_t)));

	if (!vertexBuffer || !transformBuffer || !spanBuffer || !_indexes) {
		return false;
	}

	// combined data-atlas buffer: reuse the cached one when the frame's
	// atlas set is unchanged (atlases are immutable after compile())
	auto &atlasCache = attachment->getAtlasCache();
	if (!atlasCache.buffer || atlasCache.atlases != frameAtlases) {
		Bytes combined;
		combined.resize(sprt::max(uint32_t(1), atlasTotalWords) * sizeof(uint32_t), 0);
		for (size_t i = 0; i < frameAtlases.size(); ++i) {
			auto data = frameAtlases[i]->getBufferData();
			sprt::memcpy(combined.data() + frameAtlasOffsets[i] * sizeof(uint32_t),
					data.data(), data.size());
		}

		atlasCache.buffer = Rc<wg::Buffer>::create(*dev,
				core::BufferInfo(core::BufferUsage::StorageBuffer,
						uint64_t(combined.size()), StringView("2dAtlases")),
				combined);
		atlasCache.atlases = sp::move(frameAtlases);
		atlasCache.offsets = sp::move(frameAtlasOffsets);
	}

	if (!atlasCache.buffer) {
		return false;
	}

	// order matches storage-buffer descriptors in the pass layout set 0
	clearBufferViews();
	addBufferView(Rc<wg::Buffer>(vertexBuffer));
	addBufferView(Rc<wg::Buffer>(transformBuffer));
	addBufferView(Rc<wg::Buffer>(spanBuffer));
	addBufferView(Rc<wg::Buffer>(atlasCache.buffer));
	return true;
}

class IgnoredInputAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~IgnoredInputAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&cb) override {
		cb(true);
	}
};

Rc<core::AttachmentHandle> IgnoredInputAttachment::makeFrameHandle(
		const core::FrameQueue &queue) {
	return Rc<IgnoredInputAttachmentHandle>::create(*this, queue);
}

bool MaterialVertexPass::makeRenderQueue(core::Queue::Builder &builder, RenderQueueInfo &info) {
	using namespace core;

	SamplerInfo samplers[] = {
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

	auto texLayout = builder.addTextureSetLayout("General", makeSpanView(samplers, 3));

	ProgramInfo vertInfo;
	vertInfo.stage = ProgramStage::Vertex;

	ProgramInfo fragInfo;
	fragInfo.stage = ProgramStage::Fragment;

	auto packWgsl = [](StringView code) {
		Vector<uint32_t> ret;
		ret.resize((code.size() + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
		sprt::memcpy(ret.data(), code.data(), code.size());
		return ret;
	};

	const bool bindlessTextures = static_cast<wg::Loop *>(info.target.get())
										  ->getDevice()
										  ->getBackendFeatures()
										  .textureBindingArrays;

	auto vertData = packWgsl(getMaterialVertexShader());
	auto fragData = packWgsl(getMaterialFragmentShader(bindlessTextures));

	auto vertProg = builder.addProgram("2dMaterialVert", vertData, &vertInfo);
	auto fragProg = builder.addProgram("2dMaterialFrag", fragData, &fragInfo);

	const AttachmentData *materials = builder.addAttachemnt(
			FrameContext2d::MaterialAttachmentName,
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		return Rc<wg::MaterialAttachment>::create(attachmentBuilder, texLayout);
	});

	auto vertexes = builder.addAttachemnt(FrameContext2d::VertexAttachmentName,
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		return Rc<VertexAttachment>::create(attachmentBuilder, materials);
	});

	// scene inputs the slice does not process yet, accepted and dropped
	auto lights = builder.addAttachemnt(FrameContext2d::LightDataAttachmentName,
			[](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(attachmentBuilder);
	});

	auto particles = builder.addAttachemnt(FrameContext2d::ParticleEmittersAttachment,
			[](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		return Rc<IgnoredInputAttachment>::create(attachmentBuilder);
	});

	auto depth = builder.addAttachemnt("CommonDepth2d",
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		return Rc<wg::ImageAttachment>::create(attachmentBuilder,
				ImageInfo(info.extent,
						ForceImageUsage(ImageUsage::DepthStencilAttachment),
						info.target->getSupportedDepthStencilFormat().front()),
				ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::DepthStencilAttachmentOptimal,
					.clearOnLoad = true,
					.clearColor = Color4F::WHITE, // depth cleared to 1.0
				});
	});

	auto output = builder.addAttachemnt("Output",
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsOutput();
		return Rc<wg::ImageAttachment>::create(attachmentBuilder,
				ImageInfo(info.extent,
						ForceImageUsage(ImageUsage::ColorAttachment | ImageUsage::TransferSrc),
						info.target->getCommonFormat()),
				ImageAttachment::AttachmentInfo{
					.initialLayout = AttachmentLayout::Undefined,
					.finalLayout = AttachmentLayout::PresentSrc,
					.clearOnLoad = true,
					.clearColor = info.backgroundColor,
				});
	});

	builder.addPass("MaterialSwapchainPass", PassType::Graphics, RenderOrderingHighest,
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto colorAtt = passBuilder.addAttachment(output);
		auto depthAtt = passBuilder.addAttachment(depth);
		auto vertexesAtt = passBuilder.addAttachment(vertexes);
		passBuilder.addAttachment(materials);
		passBuilder.addAttachment(lights);
		passBuilder.addAttachment(particles);

		auto layout = passBuilder.addDescriptorLayout("Layout2d",
				[&](PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](DescriptorSetBuilder &setBuilder) {
				// vertices, transforms, spans, combined data atlases
				setBuilder.addDescriptor(vertexesAtt, DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesAtt, DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesAtt, DescriptorType::StorageBuffer);
				setBuilder.addDescriptor(vertexesAtt, DescriptorType::StorageBuffer);
			});
			layoutBuilder.setTextureSetLayout(texLayout);
		});

		const core::GraphicPipelineData *materialPipeline = nullptr;
		const core::GraphicPipelineData *transparentPipeline = nullptr;

		passBuilder.addSubpass([&](SubpassBuilder &subpassBuilder) {
			subpassBuilder.addColor(colorAtt,
					AttachmentDependencyInfo{
						PipelineStage::ColorAttachmentOutput,
						AccessType::ColorAttachmentWrite,
						PipelineStage::ColorAttachmentOutput,
						AccessType::ColorAttachmentWrite,
						FrameRenderPassState::Submitted,
					},
					AttachmentLayout::ColorAttachmentOptimal);

			subpassBuilder.setDepthStencil(depthAtt,
					AttachmentDependencyInfo{
						PipelineStage::EarlyFragmentTest,
						AccessType::DepthStencilAttachmentRead
								| AccessType::DepthStencilAttachmentWrite,
						PipelineStage::LateFragmentTest,
						AccessType::DepthStencilAttachmentRead
								| AccessType::DepthStencilAttachmentWrite,
						FrameRenderPassState::Submitted,
					});

			auto shaderSpecInfo = Vector<SpecializationInfo>({
				SpecializationInfo(vertProg),
				SpecializationInfo(fragProg),
			});

			// PipelineMaterialInfo must byte-match the specs nodes request
			// (material pipeline lookup is hash+equality), mirror the vk set;
			// depth flags are accepted but ignored (no depth attachment yet)
			materialPipeline = subpassBuilder.addGraphicPipeline("Solid",
					layout->defaultFamily, shaderSpecInfo,
					PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
						ImageViewType::ImageView2D}));

			transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent",
					layout->defaultFamily, shaderSpecInfo,
					PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha,
							BlendFactor::OneMinusSrcAlpha, BlendOp::Add, BlendFactor::Zero,
							BlendFactor::One, BlendOp::Add),
						DepthInfo(false, true, CompareOp::LessOrEqual),
						ImageViewType::ImageView2D}));
		});

		static_cast<wg::MaterialAttachment *>(materials->attachment.get())
				->addPredefinedMaterials(Vector<Rc<Material>>({
					Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
							texLayout->queue->emptyImage, ColorMode::IntensityChannel),
					Rc<Material>::create(Material::MaterialIdInitial, materialPipeline,
							texLayout->queue->solidImage, ColorMode::IntensityChannel),
					Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
							texLayout->queue->emptyImage, ColorMode()),
					Rc<Material>::create(Material::MaterialIdInitial, transparentPipeline,
							texLayout->queue->solidImage, ColorMode()),
				}));

		return Rc<MaterialVertexPass>::create(passBuilder, vertexes, materials);
	});

	return true;
}

bool MaterialVertexPass::init(QueuePassBuilder &builder, const core::AttachmentData *vertexes,
		const core::AttachmentData *materials) {
	if (!QueuePass::init(builder)) {
		return false;
	}
	_vertexes = vertexes;
	_materials = materials;
	return true;
}

Rc<core::QueuePassHandle> MaterialVertexPass::makeFrameHandle(const core::FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}
	return Rc<MaterialVertexPassHandle>::create(*this, queue);
}

bool MaterialVertexPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	auto pass = static_cast<MaterialVertexPass *>(_queuePass.get());

	if (auto a = q.getAttachment(pass->getVertexes())) {
		_vertexHandle = static_cast<VertexAttachmentHandle *>(a->handle.get());
	}

	auto materialAttachment =
			static_cast<core::MaterialAttachment *>(pass->getMaterials()->attachment.get());
	_materialSet = materialAttachment->getMaterials();

	if (!_vertexHandle || !_materialSet) {
		log::source().error("basic2d::webgpu", "MaterialVertexPassHandle: no input data");
		return wg::QueuePassHandle::prepare(q, sp::move(cb));
	}

	return wg::QueuePassHandle::prepare(q, sp::move(cb));
}

void MaterialVertexPassHandle::recordSubpass(core::FrameQueue &q,
		const core::SubpassData &subpass, wg::CommandBuffer &buf) {
	if (!_vertexHandle || _vertexHandle->empty() || subpass.graphicPipelines.empty()) {
		return;
	}

	buf.cmdBindIndexBuffer(_vertexHandle->getIndexes());

	uint32_t boundLayoutIndex = maxOf<uint32_t>();
	const core::GraphicPipelineData *boundPipeline = nullptr;

	// dynamic draw states: scissor (scene rects are bottom-left based and
	// pre-rotation, like the vk backend - see vk rotateScissor); clamp against
	// the actual render target, frame constraints may be empty (e.g. tests)
	const auto &constraints = q.getFrame()->getFrameConstraints();
	const Extent2 targetExtent = buf.getRenderExtent();
	StateId boundStateId = maxOf<StateId>();
	bool scissorActive = false;

	auto applyState = [&](StateId stateId) {
		if (stateId == boundStateId) {
			return;
		}
		boundStateId = stateId;

		auto drawStates = _vertexHandle->getDrawStates();
		const DrawStateValues *state = stateId < drawStates.size() ? &drawStates[stateId]
																   : nullptr;
		if (!state || !state->isScissorEnabled()) {
			if (scissorActive) {
				buf.cmdSetScissor(0, 0, targetExtent.width, targetExtent.height);
				scissorActive = false;
			}
			return;
		}

		auto &scissor = state->scissor;
		int32_t x = int32_t(scissor.x);
		int32_t y = int32_t(targetExtent.height) - int32_t(scissor.y) - int32_t(scissor.height);
		int32_t w = int32_t(scissor.width);
		int32_t h = int32_t(scissor.height);

		switch (core::getPureTransform(constraints.transform)) {
		case core::SurfaceTransformFlags::Rotate90:
			x = int32_t(scissor.y);
			y = int32_t(scissor.x);
			sprt::swap(w, h);
			break;
		case core::SurfaceTransformFlags::Rotate180: y = int32_t(scissor.y); break;
		case core::SurfaceTransformFlags::Rotate270:
			x = int32_t(targetExtent.width) - int32_t(scissor.y) - int32_t(scissor.height);
			y = int32_t(targetExtent.height) - int32_t(scissor.x) - int32_t(scissor.width);
			sprt::swap(w, h);
			break;
		default: break;
		}

		// WebGPU validates the rect against the render target: clamp both sides
		if (x < 0) {
			w += x;
			x = 0;
		}
		if (y < 0) {
			h += y;
			y = 0;
		}
		w = sprt::min(w, int32_t(targetExtent.width) - x);
		h = sprt::min(h, int32_t(targetExtent.height) - y);

		if (w <= 0 || h <= 0) {
			// empty region: degenerate scissor still must be valid
			buf.cmdSetScissor(0, 0, 1, 1);
		} else {
			buf.cmdSetScissor(uint32_t(x), uint32_t(y), uint32_t(w), uint32_t(h));
		}
		scissorActive = true;
	};

	for (auto &span : _vertexHandle->getSpans()) {
		applyState(span.state);

		auto material = _materialSet->getMaterialById(span.material);
		if (!material) {
			continue;
		}

		// blend/depth state differs per material (Solid vs Transparent)
		auto pipeline = material->getPipeline();
		if (!pipeline || !pipeline->pipeline) {
			continue;
		}
		if (pipeline != boundPipeline) {
			buf.cmdBindPipeline(pipeline);
			boundPipeline = pipeline;
			// pipeline switch resets bind groups on some backends, rebind set
			boundLayoutIndex = maxOf<uint32_t>();
		}

		// verify the material's texture slot is still valid in the CURRENT
		// layout: a material that missed a dynamic-image update points into
		// a slot that no longer holds its view (vk samples it as empty)
		{
			auto &image = material->getImages().front();
			auto matLayout = _materialSet->getLayout(material->getLayoutIndex());
			bool valid = matLayout && image.descriptor < matLayout->imageSlots.size()
					&& matLayout->imageSlots[image.descriptor].image == image.view;
			if (!valid) {
				log::source().debug("basic2d::webgpu", "SKIP stale material ", span.material,
						" descriptor=", image.descriptor, " slots=",
						matLayout ? matLayout->usedImageSlots : 0);
				continue;
			}
		}

		auto layoutIndex = material->getLayoutIndex();
		auto layout = _materialSet->getLayout(layoutIndex);
		if (!layout || !layout->set) {
			log::source().error("basic2d::webgpu",
					"No texture set for material layout: ", layoutIndex);
			continue;
		}

		auto textureSet = static_cast<wg::TextureSet *>(layout->set.get());
		if (textureSet->isBindless()) {
			if (layoutIndex != boundLayoutIndex) {
				buf.cmdBindTextureSet(boundPipeline->layout, textureSet);
				boundLayoutIndex = layoutIndex;
			}
		} else {
			// standard path: one texture per draw, rebind on material change
			auto &image = material->getImages().front();
			const uint32_t samplerImageIdx =
					image.descriptor | (uint32_t(image.sampler) << 16);
			if (auto group = textureSet->acquireBindGroup(samplerImageIdx)) {
				buf.cmdBindMaterialGroup(boundPipeline->layout, group);
			} else {
				continue;
			}
			boundLayoutIndex = maxOf<uint32_t>();
		}

		buf.cmdDrawIndexed(span.indexCount, span.instanceCount, span.firstIndex,
				int32_t(span.vertexOffset), span.firstInstance);
	}
}

} // namespace stappler::xenolith::basic2d::webgpu
