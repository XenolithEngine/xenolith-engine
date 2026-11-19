/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_

#include "XL2dVkVertexPass.h"
#include "XL2dVkShadow.h"

#if MODULE_XENOLITH_BACKEND_VK

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class SP_PUBLIC ShadowPass : public VertexPass {
public:
	static constexpr StringView ShadowPipeline = "ShadowPipeline";
	static constexpr StringView FrameClipperPipeline = "FrameClipperPipeline";
	static constexpr StringView PseudoSdfPipeline = "PseudoSdfPipeline";
	static constexpr StringView PseudoSdfSolidPipeline = "PseudoSdfSolidPipeline";
	static constexpr StringView PseudoSdfBackreadPipeline = "PseudoSdfBackreadPipeline";

	// The frame distortion pair, Particle-52: the shift draw and the four blend twins of the
	// composite draw, in the ParticleBlendMode order alpha, add, sub, mul
	static constexpr StringView DistortShiftPipeline = "DistortShiftPipeline";
	static constexpr StringView DistortCompositePipelines[4] = {
		"DistortCompositeAlpha",
		"DistortCompositeAdd",
		"DistortCompositeSub",
		"DistortCompositeMul",
	};

	// The HDR world's own chain, Vfx-2: the bright draw, the tonemap composite (which walks
	// the multi-scale kernel over the bright pass's own output) - and the material twins the
	// world's draws bind, the scene subpass's pairs pointed at the floating-point target, in
	// the same blend order
	static constexpr StringView BloomBrightPipeline = "BloomBrightPipeline";
	static constexpr StringView BloomCompositePipeline = "BloomCompositePipeline";
	static constexpr StringView BloomMaterialPipelines[4] = {
		"BloomMaterialAlpha",
		"BloomMaterialAdd",
		"BloomMaterialSub",
		"BloomMaterialMul",
	};
	static constexpr StringView BloomParticlePipelines[4] = {
		"BloomParticleAlpha",
		"BloomParticleAdd",
		"BloomParticleSub",
		"BloomParticleMul",
	};

	enum class Flags : uint32_t {
		None = 0,
		UsePseudoSdf = 1 << 0,
	};

	struct RenderQueueInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Flags flags = Flags::None;
		Color4F backgroundColor = Color4F::WHITE;
		core::QueueDamageFlags damage = core::QueueDamageFlags::None;
	};

	struct PassCreateInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Flags flags;
		Color4F backgroundColor = Color4F::WHITE;

		const AttachmentData *particleAttachment = nullptr;
		core::QueueDamageFlags damage = core::QueueDamageFlags::None;
	};

	static bool makeRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~ShadowPass() { }

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const PassCreateInfo &info);

	const AttachmentData *getLightsData() const { return _lightsData; }
	const AttachmentData *getSdf() const { return _sdf; }

	Flags getFlags() const { return _flags; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	Flags _flags = Flags::None;

	void makeMaterialSubpass(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d,
			const core::AttachmentPassData *colorAttachment,
			const core::AttachmentPassData *shadowAttachment,
			const core::AttachmentPassData *depth2dAttachment);

	// The HDR world subpass's material twins: the scene subpass's blend pairs and the mask
	// path's particle fragment, declared against the floating-point target under the
	// ParticlePost variant no material resolves to
	void makeBloomMaterialPipelines(Queue::Builder &queueBuilder,
			core::SubpassBuilder &subpassBuilder, const core::PipelineLayoutData *layout2d);

	// shadows
	const AttachmentData *_lightsData = nullptr;
	const AttachmentData *_sdf = nullptr;

	// The HDR world's own targets, Vfx-2: the world's floating-point accumulation and the
	// bright pass's thresholded output the composite's kernel walks
	const AttachmentData *_hdr = nullptr;
	const AttachmentData *_bloomA = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(ShadowPass::Flags)

class SP_PUBLIC ShadowPassHandle : public VertexPassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

protected:
	virtual void prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &buf) override;

	virtual void recordOverlaySubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans) override;

private:
	// The trailing pair's draws: the distorted spans through the shift pipeline into the
	// intermediate, then through the composite pipeline their blend mode names. Called with
	// the subpass walk standing before the shift subpass in both render pass instances
	void recordDistortionSubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans);

	// The HDR world's own recording, Vfx-2: the world's emitter spans into the floating-point
	// target, the bright pass at the document's threshold, the three fixed blur levels, the
	// tonemap composite at the document's strength. A frame without the post draws no world
	// and the subpasses are walked empty, which is the old frame exactly
	void recordPostSubpasses(CommandBuffer &buf, SpanView<VertexSpan> spans);

	// Set in prepare() from the frame's emitter render infos: any emitter with the post words
	// moves the whole world into the target - the document is the one window's whole world
	bool _postOn = false;
	float _postThreshold = 1.0f;
	float _postStrength = 1.0f;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ImageAttachmentHandle *_sdfImage = nullptr;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_ */
