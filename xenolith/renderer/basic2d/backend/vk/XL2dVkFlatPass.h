/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKFLATPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKFLATPASS_H_

#include "XL2dVkVertexPass.h"

#if MODULE_XENOLITH_BACKEND_VK

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

// Input attachment that accepts and drops its input.
//
// FrameContext2d::initWithQueue requires the lights and particle-emitter attachments to exist by
// name even when the queue does not render them; see the same pattern in the WebGPU and Metal
// backends.
class SP_PUBLIC IgnoredInputAttachment : public core::GenericAttachment {
public:
	virtual ~IgnoredInputAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

// Lightweight alternative to ShadowPass: a single graphics pass with a single subpass drawing
// straight into the swapchain image.
//
// Dropped relative to ShadowPass: the particle compute pass, the shadow accumulation buffer, the
// pseudo-SDF field and its depth buffer, the 2d depth buffer, and the post-processing subpass
// (shadow merge + frame clipper). Correct ordering without a depth buffer is provided by the
// painter-order span emission in VertexAttachment's flat mode.
class SP_PUBLIC FlatPass : public VertexPass {
public:
	struct RenderQueueInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Color4F backgroundColor = Color4F::WHITE;
		core::QueueDamageFlags damage = core::QueueDamageFlags::PresentHint;
	};

	static bool makeRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~FlatPass() = default;

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const RenderQueueInfo &info);

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	void makeMaterialSubpass(Queue::Builder &queueBuilder, core::SubpassBuilder &subpassBuilder,
			const core::PipelineLayoutData *layout2d,
			const core::AttachmentPassData *colorAttachment);
};

class SP_PUBLIC FlatPassHandle : public VertexPassHandle {
public:
	virtual ~FlatPassHandle() = default;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKFLATPASS_H_ */
