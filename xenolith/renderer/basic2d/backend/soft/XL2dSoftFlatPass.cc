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
#include "XLSoftGlyphStore.h"
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

		// Glyphs are drawn from their own storage rather than from an atlas image, so the object id
		// has to reach the renderer instead of being consumed by the atlas resolution.
		plan->keepAtlasObjects = true;

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

	_output =
			queueBuilder.addAttachemnt("Output", [&](AttachmentBuilder &builder) -> Rc<Attachment> {
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
	out.texture.format = sf::getRasterFormat(info.format);
	out.texture.swizzle[0] = viewInfo.r;
	out.texture.swizzle[1] = viewInfo.g;
	out.texture.swizzle[2] = viewInfo.b;
	out.texture.swizzle[3] = viewInfo.a;

	switch (viewInfo.type) {
	case core::ImageViewType::ImageView2DArray:
		out.kind = sf::raster::TextureKind::Texture2DArray;
		break;
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

// The glyph storage behind a font material, or null for everything else. The store rides on the
// dynamic image's instance, which is the same seam the Vulkan backend uses to carry its persistent
// glyph buffers from frame to frame.
const sf::GlyphStore *FlatPass_resolveGlyphStore(const core::Material *material) {
	if (!material->getAtlas()) {
		return nullptr;
	}
	auto &image = material->getImages().front();
	if (!image.dynamic) {
		return nullptr;
	}
	return image.dynamic->userdata.get_cast<sf::GlyphStore>();
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

struct GlyphEmitStats {
	uint32_t blits = 0;
	uint32_t sampled = 0;
	uint32_t missing = 0;
};

// One run of triangles that all name the same glyph, turned into draw work.
//
// The engine's typography guarantee makes the common case exact: a Label is normalized, so
// VertexPlan::applyNormalized rebuilds its model matrix as identity plus a floored translation, and
// the label's scale went into the font size rather than the quad. The glyph therefore covers a
// whole number of pixels at 1:1 and can simply be copied. Everything else - a caller that turned
// normalization off, an underline rectangle stretched from a single texel - falls back to sampling
// the glyph as an ordinary texture, and is counted so the fallback is never silent.
void FlatPass_emitGlyphRun(sf::raster::DrawList &list, const sf::GlyphStore::Glyph &glyph,
		uint32_t firstIndex, uint32_t indexCount, uint32_t base, uint32_t minIndex,
		SpanView<uint32_t> srcIndexes, uint32_t vertexOffset, sf::BlendMode blend,
		const URect &scissor, Map<const uint8_t *, uint32_t> &textureIndexes,
		GlyphEmitStats &stats) {
	// Corners of the run in target pixels, plus which of them carries the first texel.
	float minX = maxOf<float>(), minY = maxOf<float>();
	float maxX = -maxOf<float>(), maxY = -maxOf<float>();
	float originX = 0.0f, originY = 0.0f;
	float minU = maxOf<float>(), minV = maxOf<float>();
	Color4F color;
	bool uniformColor = true;
	bool first = true;

	for (uint32_t i = 0; i < indexCount; ++i) {
		auto idx = base + (srcIndexes[firstIndex + i] + vertexOffset - minIndex);
		auto &v = list.vertexes[idx];

		minX = sprt::min(minX, v.x);
		maxX = sprt::max(maxX, v.x);
		minY = sprt::min(minY, v.y);
		maxY = sprt::max(maxY, v.y);

		if (v.u <= minU) {
			minU = v.u;
			originX = v.x;
		}
		if (v.v <= minV) {
			minV = v.v;
			originY = v.y;
		}

		if (first) {
			color = v.color;
			first = false;
		} else if (v.color != color) {
			uniformColor = false;
		}
	}

	auto isIntegral = [](float value) {
		auto rounded = sprt::round(value);
		return sprt::fabs(value - rounded) < (1.0f / 512.0f);
	};

	// XL_SOFT_GLYPH_SAMPLING=1 forces every glyph down the fallback. The blit is supposed to
	// produce exactly what a nearest fetch of the same coverage produces, and this is how that is
	// checked: render a scene both ways and diff. It is a verification hook, not a feature.
	static const bool forceSampling = [] {
		auto value = ::getenv("XL_SOFT_GLYPH_SAMPLING");
		return value && StringView(value) != "0";
	}();

	const float width = maxX - minX;
	const float height = maxY - minY;

	// Every condition here is a property the blit relies on: one texel per pixel, no rotation or
	// mirroring (the first texel sits at the top-left corner), a single colour for the whole glyph,
	// and a destination that starts on a pixel boundary.
	const bool blittable = !forceSampling && uniformColor && glyph.metricWidth == glyph.width
			&& glyph.metricHeight == glyph.rows && isIntegral(width) && isIntegral(height)
			&& uint32_t(sprt::round(width)) == glyph.width
			&& uint32_t(sprt::round(height)) == glyph.rows && isIntegral(minX) && isIntegral(minY)
			&& originX == minX && originY == minY;

	if (blittable) {
		sf::raster::GlyphBlit blit;
		blit.coverage = glyph.pixels;
		blit.pitch = glyph.pitch;
		blit.x = int32_t(sprt::round(minX));
		blit.y = int32_t(sprt::round(minY));
		blit.width = glyph.width;
		blit.height = glyph.rows;
		blit.color = color;
		blit.blend = blend;
		blit.scissor = scissor;

		list.addGlyph(sp::move(blit));
		++stats.blits;
		return;
	}

	// Fallback: the glyph becomes an ordinary single-image texture and goes through the sampler.
	uint32_t textureIndex = 0;
	auto it = textureIndexes.find(glyph.pixels);
	if (it == textureIndexes.end()) {
		sf::raster::Texture texture;
		texture.pixels = glyph.pixels;
		texture.width = glyph.width;
		texture.height = glyph.rows;
		texture.stride = glyph.pitch;
		texture.layerSize = glyph.pitch * glyph.rows;
		texture.format = sf::raster::PixelFormat::R8;
		// what a font atlas view resolves to: the coverage is the alpha, the colour is the vertex's
		texture.swizzle[0] = core::ComponentMapping::One;
		texture.swizzle[1] = core::ComponentMapping::One;
		texture.swizzle[2] = core::ComponentMapping::One;
		texture.swizzle[3] = core::ComponentMapping::R;

		textureIndex = uint32_t(list.textures.size());
		list.textures.emplace_back(texture);
		textureIndexes.emplace(glyph.pixels, textureIndex);
	} else {
		textureIndex = it->second;
	}

	sf::raster::Command command;
	command.firstIndex = uint32_t(list.indexes.size());
	command.indexCount = indexCount;
	command.blend = blend;
	command.scissor = scissor;
	command.kind = sf::raster::TextureKind::Texture2D;
	command.texture = textureIndex;
	command.sampler.filter = sf::raster::Filter::Nearest;
	command.sampler.addressU = sf::raster::AddressMode::ClampToEdge;
	command.sampler.addressV = sf::raster::AddressMode::ClampToEdge;
	command.sampler.addressW = sf::raster::AddressMode::ClampToEdge;

	for (uint32_t i = 0; i < indexCount; ++i) {
		list.indexes.emplace_back(base + (srcIndexes[firstIndex + i] + vertexOffset - minIndex));
	}

	list.addCommand(sp::move(command));
	++stats.sampled;
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

	// Glyphs are their own textures and are keyed by their storage instead; only the sampling
	// fallback ever puts anything here.
	Map<const uint8_t *, uint32_t> glyphTextures;
	GlyphEmitStats glyphStats;

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

		auto glyphStore = FlatPass_resolveGlyphStore(material);

		auto resolved = FlatPass_resolveTexture(material, layout);
		if (!resolved.valid) {
			++missingMaterials;
			continue;
		}

		uint32_t textureIndex = 0;
		if (!glyphStore && resolved.kind != sf::raster::TextureKind::Solid) {
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

			// Text does not go through the sampler. Each glyph is drawn from its own coverage
			// bitmap, so the span is split into runs of triangles that name the same glyph - the
			// object id survives the vertex plan for exactly this (VertexPlan::keepAtlasObjects).
			// Splitting per glyph costs nothing: the triangle count is the same, only the source
			// pointer changes between them.
			if (glyphStore) {
				uint32_t i = 0;
				while (i + 2 < span.indexCount) {
					auto firstSrc = srcIndexes[span.firstIndex + i] + span.vertexOffset;
					auto glyphId = sf::GlyphStore::getGlyphId(srcVertexes[firstSrc].object);

					uint32_t runEnd = i;
					while (runEnd + 2 < span.indexCount) {
						auto src = srcIndexes[span.firstIndex + runEnd] + span.vertexOffset;
						if (sf::GlyphStore::getGlyphId(srcVertexes[src].object) != glyphId) {
							break;
						}
						runEnd += 3;
					}

					if (auto glyph = glyphStore->getGlyph(glyphId)) {
						FlatPass_emitGlyphRun(list, *glyph, span.firstIndex + i, runEnd - i, base,
								minIndex, srcIndexes, span.vertexOffset, blend, scissor,
								glyphTextures, glyphStats);
					} else {
						// Not in the store: it was never rasterized (an unsupported code point, or
						// storage ran out). Drawing the placeholder here would put a solid block
						// where a character belongs, so draw nothing and report it.
						++glyphStats.missing;
					}

					i = runEnd;
				}
				continue;
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

			list.addCommand(sp::move(command));
		}
	}

	if (missingMaterials > 0 || brokenSpans > 0) {
		log::source().warn("basic2d::soft", "Dropped ", missingMaterials,
				" span(s) with no usable material and ", brokenSpans,
				" span(s) with out-of-range indexes");
	}

	if (glyphStats.missing > 0) {
		log::source().warn("basic2d::soft", "Dropped ", glyphStats.missing,
				" glyph(s) missing from the store");
	}

#if DEBUG
	if (glyphStats.sampled > 0) {
		static bool s_loggedNonIntegralGlyph = false;
		if (!s_loggedNonIntegralGlyph) {
			s_loggedNonIntegralGlyph = true;
			log::source().debug("basic2d::soft", "Sampled ", glyphStats.sampled, " of ",
					glyphStats.sampled + glyphStats.blits,
					" glyph run(s): not an integral 1:1 placement (further frames omitted)");
		}
	}
#endif
}


} // namespace stappler::xenolith::basic2d::soft

#endif
