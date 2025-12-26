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

	enum class Flags : uint32_t {
		None = 0,
		UsePseudoSdf = 1 << 0,
	};

	struct RenderQueueInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Flags flags = Flags::None;
		Color4F backgroundColor = Color4F::WHITE;
	};

	struct PassCreateInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Flags flags;
		Color4F backgroundColor = Color4F::WHITE;

		const AttachmentData *particleAttachment = nullptr;
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

	// shadows
	const AttachmentData *_lightsData = nullptr;
	const AttachmentData *_sdf = nullptr;
};

SP_DEFINE_ENUM_AS_MASK(ShadowPass::Flags)

class SP_PUBLIC ShadowPassHandle : public VertexPassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &q, Function<void(bool)> &&cb) override;

protected:
	virtual void prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &buf) override;

	// shadows
	const ShadowLightDataAttachmentHandle *_shadowData = nullptr;
	const ImageAttachmentHandle *_sdfImage = nullptr;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKSHADOWPASS_H_ */
