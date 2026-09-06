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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_GLES_XL2DGLESCLEARPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_GLES_XL2DGLESCLEARPASS_H_

#include "XL2d.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_GLES

#include "XLGlesQueuePass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::gles {

// From inside basic2d::gles an unqualified `gles::` resolves to this very namespace, so the
// backend is referred by its full name through the alias.
namespace glesb = stappler::xenolith::gles;

// Clear-only queue: one graphics pass that clears its output attachment to the background colour
// and presents it. The scene's draw commands travel input attachments (VertexInput2d, lights,
// particles), but this queue declares none of them - a submit with no target returns false in
// FrameRequest::addInput and is dropped without an error, so a screenshot reads exactly the
// cleared colour. A scene draws through FlatPass instead; this one is the bring-up minimum.
class SP_PUBLIC ClearPass : public core::QueuePass {
public:
	struct RenderQueueInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Color4F backgroundColor = Color4F::WHITE;
		core::QueueDamageFlags damage = core::QueueDamageFlags::None;
	};

	static bool makeRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~ClearPass() = default;

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const RenderQueueInfo &info);

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using core::QueuePass::init;

	const core::AttachmentData *_output = nullptr;
};

// The executor is the backend's own handle: it binds the framebuffer, applies the clear and
// submits through a GL fence. Nothing to add - there are no subpasses to record.
class SP_PUBLIC ClearPassHandle : public glesb::QueuePassHandle {
public:
	virtual ~ClearPassHandle() = default;
};

} // namespace stappler::xenolith::basic2d::gles

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_GLES_XL2DGLESCLEARPASS_H_ */
