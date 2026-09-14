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

#ifndef XENOLITH_CORE_XLCOREFRAMECAPTURE_H_
#define XENOLITH_CORE_XLCOREFRAMECAPTURE_H_

#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

/* What one frame is asked to copy out of the image it has just drawn.

Carried as per-frame attachment input because the render queue is shared between scenes and windows
(see QueueCache); a frame with no input records nothing. Each region is its own image, sized to its
rectangle and written at offset zero. */
struct SP_PUBLIC FrameCaptureInput : AttachmentInputData {
	struct Region {
		// Where the pixels go. Must be TransferDst|Sampled, the same format as the image being
		// copied out of (cmdCopyImage requires it), and at least `src`'s size.
		Rc<ImageObject> target;

		// Source in the presented image: pixels, y-down, already clamped to the frame extent by the
		// caller. The pass does not re-clamp.
		URect src;
	};

	Vector<Region> regions;

	/* Runs exactly once, when the frame is finalized, with the frame's success (the copy is recorded
	into the frame's command buffer). Runs on the loop thread; the builder owns any thread hop. */
	Function<void(bool)> completion;
};

// The handle exists only to close the loop above: nothing else about a capture needs per-frame state.
class SP_PUBLIC FrameCaptureAttachmentHandle : public AttachmentHandle {
public:
	virtual ~FrameCaptureAttachmentHandle() = default;

	virtual void finalize(FrameQueue &, bool successful) override;
};

/* The attachment FrameCaptureInput travels on. Input-only: owns no GPU resource and takes no
framebuffer slot; it exists so its handle is finalized with the frame. */
class SP_PUBLIC FrameCaptureAttachment
: public AttachmentTyped<FrameCaptureAttachmentHandle, GenericAttachment> {
public:
	virtual ~FrameCaptureAttachment() = default;
};

// One per render queue that can capture; the name is how a FrameContext finds it, the way it finds
// the material and vertex attachments.
constexpr auto FrameCaptureAttachmentName = StringView("FrameCapture");

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMECAPTURE_H_ */
