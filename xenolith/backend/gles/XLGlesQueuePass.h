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

#ifndef XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_
#define XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_

#include "XLGlesDevice.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// The render pass has no state to hold: it only identifies the pass inside the frame graph, the
// same way the soft one does. Its index is what the framebuffer cache keys on.
class SP_PUBLIC RenderPass final : public core::RenderPass {
public:
	virtual ~RenderPass() = default;

	bool init(Device &, const core::QueuePassData &);
};

// M1 records nothing here: a subpass callback that appends draw commands is rejected at submit,
// and the DrawList of §2.1 lands with the draw path in M2. The object exists so that a pass can
// name one without the backend having to invent a type for it.
class SP_PUBLIC CommandBuffer final : public core::CommandBuffer {
public:
	virtual ~CommandBuffer() = default;

	bool init(Device &);

protected:
	Device *_device = nullptr;
};

// A queue attachment that carries an image. The backend adds nothing to it in M1 - like the soft
// one, it exists so a pass names its output with the backend's own type instead of core's.
class SP_PUBLIC ImageAttachment : public core::AttachmentTyped<core::AttachmentHandle, core::ImageAttachment> {
public:
	virtual ~ImageAttachment() = default;
};

class SP_PUBLIC QueuePassHandle : public core::QueuePassHandle {
public:
	virtual ~QueuePassHandle() = default;

	// Scene-space scissor -> target pixels, honouring the surface pre-rotation. Ported from soft:
	// the transform is a property of the presented surface, not of the API, so both backends have
	// to agree on it or clipped content would land in different places. M1 does not clip at all -
	// there are no draw commands - but the helper goes with its executor and the M2 pass will use
	// it for glScissor.
	static URect rotateScissor(const core::FrameConstraints &constraints, const URect &scissor);

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&,
			Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

protected:
	// Bind the framebuffer and apply the pass's load ops. M1 stops there: a subpass that wants to
	// record draw commands is refused rather than silently dropped.
	bool runPass(core::FrameQueue &);

	Device *_device = nullptr;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESQUEUEPASS_H_ */
