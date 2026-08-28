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

#include "XL2dGlesFlatPass.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_GLES

#include "XLCoreFrameQueue.h"
#include "XLCoreFrameHandle.h"
// The object classes the pass talks to (Buffer/Image/Sampler) live in the backend's object
// header, which its queue-pass header does not pull in.
#include "XLGlesObject.h"
#include "XLGlesMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::gles {

// --- The flat shaders, in GLSL ES 3.0 ---------------------------------------------------------
//
// The Vulkan pair (glsl/shaders/xl_2d_flat.*) cannot be used here: it leans on buffer references
// and push constants, neither of which exists in the ES API. These are its direct counterpart -
// the same vertex math, the same single transform lookup, one 2D sample in the fragment stage -
// with the per-draw values that a Vulkan pipeline would carry as dynamic state (texture, sampler,
// view swizzle, first instance) arriving as uniforms and bound attributes instead.

static const char kFlatVertexShader[] = R"GLSL(
#version 310 es
precision highp float;
precision highp int;

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex;
layout(location = 3) in uint materialId;
layout(location = 4) in uint object;

// Member order and types must match the C++ TransformData (glsl/include/XL2dGlslVertexData.h):
// under the shared offset layout a GLSL ES storage block uses, this declaration lands byte for
// byte on top of it (128 elements per entry).
struct Transform {
	mat4 transform;
	vec4 offset;
	vec4 instanceColor;
	vec4 outlineColor;
	float shadowValue;
	float padding0;
	float outlineOffset;
	uint flags;
};

layout(binding = 0) buffer TransformBlock {
	Transform transforms[];
};

// int (not uint): the executor writes it with glUniform1i, and this driver raises
// INVALID_OPERATION when an int uniform call hits a uint declaration. The value is a small
// non-negative index either way - main() does its arithmetic in int regardless.
uniform int uFirstInstance;

out vec4 fragColor;
out vec2 fragTexCoord;

void main() {
	// xl_2d_flat.vert reads exactly one transform: a packed draw bakes its slot into the vertex's
	// material word and draws a single instance, an instanced draw bakes zero there and
	// gl_InstanceID carries the offset. uFirstInstance adds span.firstInstance to both cases -
	// the software rasterizer does the same lookup on the CPU. The slot is a small index (never
	// an address), so plain int math keeps every term and the array subscript in one base type;
	// ES 3.0 will not mix int and uint operands for us. (No `const` on the locals: this driver
	// rejects non-constant initializers behind a const declaration.)
	int slot = (int(materialId) >> 16) + int(uFirstInstance) + int(gl_InstanceID);
	Transform transform = transforms[slot];

	vec4 p = pos;

	uint f = transform.flags;
	// ES forbids comparing a uint with an int literal, so the zero is spelled as one. Build the mask
	// from boolean-to-float conversions: this driver's GLSL frontend has been seen to mis-lex a
	// ternary in exactly this position (a spurious "unexpected '?'" / EOF error that drops the whole
	// material), so keep the construct out of the shader.
	vec4 mask = vec4(float((f & uint(1)) != uint(0)),
			float((f & uint(2)) != uint(0)),
			float((f & uint(4)) != uint(0)),
			float((f & uint(8)) != uint(0)));

	gl_Position = (transform.transform * p * mask) + transform.offset;
	// The transform matrix is built once by the shared vertex plan in Vulkan's NDC convention,
	// where clip +Y points toward the top of the surface. OpenGL's clip space is flipped (+Y
	// toward the bottom), so negate Y here to land the geometry where the Vulkan/soft backends
	// put it. Without this the whole frame renders as a vertical mirror - invisible for
	// vertically symmetric content, wrong for anything that is not (stacked sprites, dash
	// patterns, alpha gradients).
	gl_Position.y = -gl_Position.y;
	fragColor = color * transform.instanceColor;
	fragTexCoord = vec2(tex.x, tex.y);
}
)GLSL";

static const char kFlatFragmentShader[] = R"GLSL(
#version 310 es
precision highp float;
precision highp int;

in vec4 fragColor;
in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D textureSampler;

// Per-draw component mapping (color.h's ComponentMapping): 0 = same slot, 1 = zero, 2 = one,
// 3..6 = R,G,B,A. It replaces the image-view swizzle a Vulkan view would apply in fixed function.
uniform ivec4 uSwizzle;

out vec4 outColor;

vec4 applySwizzle(vec4 s, ivec4 m) {
	vec4 r;
	for (int i = 0; i < 4; ++i) {
		int map = m[i];
		if (map == 1) {
			r[i] = 0.0;
		} else if (map == 2) {
			r[i] = 1.0;
		} else if (map == 3) {
			r[i] = s.x;
		} else if (map == 4) {
			r[i] = s.y;
		} else if (map == 5) {
			r[i] = s.z;
		} else if (map == 6) {
			r[i] = s.w;
		} else {
			r[i] = s[i]; // identity: the same slot
		}
	}
	return r;
}

void main() {
	// The counterpart of xl_2d_flat.frag: the vertex color multiplied by the sampled material
	// image. Solid-color spans sample the queue's solid image (a 1x1 white texture), so the
	// multiply is a no-op for them; sprites and labels get their real pixels here.
	vec4 s = texture(textureSampler, fragTexCoord);
	outColor = fragColor * applySwizzle(s, uSwizzle);
}
)GLSL";

// Program data is a word array; these are text sources, so pack them the way the Metal pass
// packs its MSL.
static Vector<uint32_t> glesFlatPackShader(StringView code) {
	Vector<uint32_t> ret;
	ret.resize((code.size() + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
	sprt::memcpy(ret.data(), code.data(), code.size());
	return ret;
}

// --- The attachments ---------------------------------------------------------------------------

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

bool VertexAttachment::init(AttachmentBuilder &builder, const core::AttachmentData *materials,
		bool damageTracked) {
	if (!core::GenericAttachment::init(builder)) {
		return false;
	}

	_materials = materials;
	_damageTracked = damageTracked;
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
		log::source().error("basic2d::gles", "No material set for vertex attachment");
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

		// There is no shader to probe a glyph atlas, so the plan resolves it on the CPU - the same
		// branch a Vulkan device without buffer device addresses takes. Glyph quads come out with
		// real UVs into the font atlas image, which the fragment stage samples like any other
		// texture; no per-glyph storage is needed here.
		plan->hasGpuSideAtlases = false;
		plan->keepAtlasObjects = false;

		auto shadowExtent = commands->lights.getShadowExtent(constraints.getScreenSize());
		auto shadowSize = commands->lights.getShadowSize(constraints.getScreenSize());
		plan->shadowSize = Vec2(shadowSize.width / float(shadowExtent.width),
				shadowSize.height / float(shadowExtent.height));

		VertexPlanContext ctx;
		ctx.input = commands;
		ctx.materialSet = _materialSet;
		ctx.damage = &_damage;
		ctx.collectDamage = isDamageTracked();
		if (ctx.collectDamage) {
			_damage.init(commands, constraints);
		}

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


		// Publish before the attachment reports readiness: the pass reads it back when it decides
		// how much of the image it has to repaint.
		if (ctx.collectDamage) {
			if (auto request = fhandle.getRequest()) {
				request->setDamageState(_damage.finalize());
			}
		}

		delete plan;
		return true;
	}, pool);
	memory::pool::destroy(pool);
	return ret;
}

// --- The pass ------------------------------------------------------------------------------------

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

	// Sampler order is baked into materials - keep it identical to basic2d::vk/soft's FlatPass.
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

	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsOutput();

		return Rc<glesb::ImageAttachment>::create(builder,
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
		return Rc<glesb::MaterialAttachment>::create(builder, texLayout);
	});

	_vertexes = queueBuilder.addAttachemnt(FrameContext2d::VertexAttachmentName,
			[&, this](AttachmentBuilder &builder) -> Rc<Attachment> {
		builder.defineAsInput();
		return Rc<VertexAttachment>::create(builder, _materials,
				hasFlag(info.damage, core::QueueDamageFlags::PresentHint));
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

	// The sources are text (GLSL ES), not SPIR-V: the backend's Shader compiles them at queue
	// compile time and needs the stage declared explicitly - exactly what the Metal pass does for
	// its MSL, since there is nothing to reflect.
	ProgramInfo vertInfo;
	vertInfo.stage = ProgramStage::Vertex;

	ProgramInfo fragInfo;
	fragInfo.stage = ProgramStage::Fragment;

	auto flatVert = queueBuilder.addProgram("Gles_FlatVert", glesFlatPackShader(kFlatVertexShader),
			&vertInfo);
	auto flatFrag = queueBuilder.addProgram("Gles_FlatFrag", glesFlatPackShader(kFlatFragmentShader),
			&fragInfo);

	Vector<SpecializationInfo> shaderSpecInfo({
		core::SpecializationInfo(flatVert),
		core::SpecializationInfo(flatFrag),
	});

	// PipelineMaterialInfo must stay byte-identical to basic2d::vk/soft's FlatPass: materials are
	// matched to pipelines by this struct's value, and Sprite bakes DepthInfo into the request.
	// The depth state is inert here (there is no depth attachment), exactly as it is there.
	auto materialPipeline =
			subpassBuilder.addGraphicPipeline("Solid", layout2d->defaultFamily, shaderSpecInfo,
					PipelineMaterialInfo({BlendInfo(), DepthInfo(true, true, CompareOp::Less),
						ImageViewType::ImageView2D}));

	auto transparentPipeline = subpassBuilder.addGraphicPipeline("Transparent",
			layout2d->defaultFamily, shaderSpecInfo,
			PipelineMaterialInfo({BlendInfo(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha,
										  BlendOp::Add, BlendFactor::Zero, BlendFactor::One,
										  BlendOp::Add),
				DepthInfo(false, true, CompareOp::LessOrEqual), ImageViewType::ImageView2D}));

	// All six variants have to exist even though the fragment stage samples a 2D view in every
	// case (M2): a material is matched to a pipeline by the *value* of PipelineMaterialInfo, and
	// one that asks for an array or 3d view would otherwise find nothing - failing not with an
	// error but with an empty frame.
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
	static_cast<core::MaterialAttachment *>(_materials->attachment.get())
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

// --- The frame handle ----------------------------------------------------------------------------

bool FlatPassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	if (auto vertexes = static_cast<FlatPass *>(_queuePass.get())->getVertexes()) {
		_vertexHandle = static_cast<VertexAttachmentHandle *>(getAttachmentHandle(vertexes));
	}

	return glesb::QueuePassHandle::prepare(q, sp::move(cb));
}

namespace {

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

// One host array as a GL buffer for this frame: dynamic usage (re-uploaded every submit) and the
// bytes copied through at creation. The returned reference is what keeps the name alive while the
// pass draws from it; dropping it queues the delete for the next drain, which only happens after
// this frame's fence has been waited on - so the copy out of `data` is safe even though nothing
// else holds onto the source array. The usage names the buffer's role: GLES must create each one
// with a matching target (a storage buffer created as an array buffer reads back silently empty).
static Rc<glesb::Buffer> FlatPass_upload(glesb::Device &dev, const void *data, uint64_t bytes,
		core::BufferUsage usage) {
	core::BufferData info;
	info.size = bytes;
	info.persistent = false;
	info.usage = usage;
	info.data = BytesView(reinterpret_cast<const uint8_t *>(data), size_t(bytes));
	return Rc<glesb::Buffer>::create(dev, sp::move(info));
}

} // namespace

void FlatPassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &,
		glesb::CommandBuffer &buf) {
	if (!_vertexHandle || _vertexHandle->empty()) {
		return;
	}

	auto materials = _vertexHandle->getMaterialSet();
	if (!materials) {
		return;
	}

	auto &constraints = q.getFrame()->getFrameConstraints();

	auto srcVertexes = _vertexHandle->getVertexes();
	auto srcIndexes = _vertexHandle->getIndexes();
	auto transforms = _vertexHandle->getTransforms();
	auto states = _vertexHandle->getDrawStates();
	auto layout = materials->getTargetLayout();

	uint32_t missingMaterials = 0;
	uint32_t brokenSpans = 0;

	// The span's indexes are relative to its vertex block (span.vertexOffset); GLES has no
	// base-vertex draw, so they are rewritten here into absolute ids and drawn with a plain
	// glDrawElements - exactly what the software rasterizer does with its index list.
	Vector<uint32_t> recordedIndexes;

	for (auto &span : _vertexHandle->getSpans()) {
		if (span.indexCount == 0) {
			continue; // particle spans are degenerate by construction; the plan drops them anyway
		}

		auto material = materials->getMaterialById(span.material);
		const core::GraphicPipelineData *pipelineData = material ? material->getPipeline() : nullptr;
		if (!material || !pipelineData || !pipelineData->pipeline || material->getImages().empty()) {
			++missingMaterials;
			continue;
		}

		const auto &image = material->getImages().front();
		auto view = image.view.get();
		auto glImage = view ? view->getImage().get_cast<glesb::Image>() : nullptr;
		if (!glImage) {
			++missingMaterials;
			continue;
		}

		GLuint samplerName = 0; // 0 leaves the unit's sampler unbound (the texture's own state)
		if (layout && image.sampler < layout->compiledSamplers.size()) {
			if (auto glSampler = layout->compiledSamplers[image.sampler].get_cast<glesb::Sampler>()) {
				samplerName = glSampler->getGlName();
			}
		}

		auto scissor = URect{0, 0, constraints.extent.width, constraints.extent.height};
		if (span.state != StateIdNone && span.state < states.size()) {
			const auto &state = states[span.state];
			if (state.isScissorEnabled()) {
				scissor = FlatPass_intersect(scissor,
						glesb::QueuePassHandle::rotateScissor(constraints, state.scissor));
			}
		}

		// The span's indexes address one contiguous block of the plan's vertex array; find its
		// extent once for the bounds check.
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
			continue;
		}

		const uint32_t instanceCount = sprt::max(span.instanceCount, 1U);

		// Every vertex's transform slot must exist in the uploaded table: an out-of-range read is
		// undefined behaviour in GL, so check it here and report like the software backend.
		uint32_t maxTransform = 0;
		for (uint32_t idx = minIndex; idx <= maxIndex; ++idx) {
			auto transformIndex = (srcVertexes[idx].material >> 16) + span.firstInstance
					+ instanceCount - 1;
			maxTransform = sprt::max(maxTransform, transformIndex);
		}
		if (maxTransform >= transforms.size()) {
			++brokenSpans;
			continue;
		}

		const uint32_t firstIndex = uint32_t(recordedIndexes.size());
		for (uint32_t i = 0; i < span.indexCount; ++i) {
			recordedIndexes.push_back(srcIndexes[span.firstIndex + i] + span.vertexOffset);
		}

		glesb::GlesDraw draw;
		draw.pipeline = pipelineData->pipeline;
		draw.texture = glImage->getGlName();
		draw.sampler = samplerName;

		// The view's component mapping is applied in the fragment stage (a GLES image has no
		// per-view swizzle): Identity(0) means "the same slot".
		const auto &viewInfo = image.info;
		draw.swizzle[0] = int(viewInfo.r);
		draw.swizzle[1] = int(viewInfo.g);
		draw.swizzle[2] = int(viewInfo.b);
		draw.swizzle[3] = int(viewInfo.a);

		draw.indexCount = span.indexCount;
		draw.firstIndex = firstIndex;
		draw.instanceCount = instanceCount;
		draw.firstInstance = span.firstInstance;
		draw.scissor = scissor;

		buf.addDraw(sp::move(draw));
	}

	if (missingMaterials > 0 || brokenSpans > 0) {
		log::source().warn("basic2d::gles", "Dropped ", missingMaterials,
				" span(s) with no usable material and ", brokenSpans,
				" span(s) with out-of-range indexes");
	}

	if (buf.getDraws().empty()) {
		return; // nothing to draw: the pass's load ops stand as is
	}

	// The flat vertex format is fixed by the shader's attribute layout - the same five fields of
	// basic2d::Vertex on a 48-byte stride, so describe it once and let the executor stay generic.
	buf.vertexStride = sizeof(Vertex);
	Vector<glesb::GlesAttribute> attributes({
		glesb::GlesAttribute{0, 4, GL_FLOAT, false, 0}, // pos
		glesb::GlesAttribute{1, 4, GL_FLOAT, false, 16}, // color
		glesb::GlesAttribute{2, 2, GL_FLOAT, false, 32}, // tex
		glesb::GlesAttribute{3, 1, GL_UNSIGNED_INT, false, 40}, // material
		glesb::GlesAttribute{4, 1, GL_UNSIGNED_INT, false, 44}, // object
	});
	buf.vertexAttributes = sp::move(attributes);

	auto vertexBuffer = FlatPass_upload(*_device, _vertexHandle->getVertexes().data(),
			uint64_t(_vertexHandle->getVertexes().size()) * sizeof(Vertex),
			core::BufferUsage::VertexBuffer);
	if (!vertexBuffer) {
		log::source().error("basic2d::gles", "Fail to upload the vertex array");
		return;
	}

	auto indexBuffer = FlatPass_upload(*_device, recordedIndexes.data(),
			uint64_t(recordedIndexes.size()) * sizeof(uint32_t), core::BufferUsage::IndexBuffer);
	if (!indexBuffer) {
		log::source().error("basic2d::gles", "Fail to upload the index array");
		return;
	}

	auto transformBuffer = FlatPass_upload(*_device, _vertexHandle->getTransforms().data(),
			uint64_t(_vertexHandle->getTransforms().size()) * sizeof(TransformData),
			core::BufferUsage::StorageBuffer);
	if (!transformBuffer) {
		log::source().error("basic2d::gles", "Fail to upload the transform table");
		return;
	}

	buf.vertexBuffer = vertexBuffer->getGlName();
	buf.indexBuffer = indexBuffer->getGlName();
	buf.transformBuffer = transformBuffer->getGlName();

	// The execution runs after this function returns: hold every buffer through it.
	buf.hold(sp::move(vertexBuffer));
	buf.hold(sp::move(indexBuffer));
	buf.hold(sp::move(transformBuffer));
}

} // namespace stappler::xenolith::basic2d::gles

#endif
