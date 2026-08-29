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

Carried to the pass as ordinary attachment input, for the same reason vertices and lights are: the
render queue is SHARED between scenes and windows (see QueueCache), so anything belonging to one
window - and every one of these images belongs to one capture on one window - cannot live in the
queue. It arrives per frame or it does not arrive at all, and a frame with no input records nothing,
which is what makes an idle capture free.

Every region is its own image, sized to its own rectangle, written at offset zero. There is no atlas
and no packing: the point of a cutout is that a Sprite shows it whole. */
struct SP_PUBLIC FrameCaptureInput : AttachmentInputData {
	struct Region {
		// Where the pixels go. Must be TransferDst|Sampled, the same format as the image being
		// copied out of (cmdCopyImage requires it), and at least `src`'s size.
		Rc<ImageObject> target;

		// Where they come from, in the presented image: PIXELS, y-DOWN, already clamped to the
		// frame extent by whoever asked. The pass does not re-clamp - a region that does not fit is
		// a bug in the caller, not a case to paper over.
		URect src;
	};

	Vector<Region> regions;

	/* Runs exactly once, when the frame this input belonged to is finalized, with the FRAME's
	success - not the copy's. The two are the same thing in practice (a copy is recorded into the
	frame's own command buffer and cannot fail on its own), and there is no finer signal to be had
	without a fence of its own.

	It runs on the LOOP thread. Whoever builds the input owns the hop to wherever it needs to be. */
	Function<void(bool)> completion;
};

// The handle exists only to close the loop above: nothing else about a capture needs per-frame state.
class SP_PUBLIC FrameCaptureAttachmentHandle : public AttachmentHandle {
public:
	virtual ~FrameCaptureAttachmentHandle() = default;

	virtual void finalize(FrameQueue &, bool successful) override;
};

/* The attachment FrameCaptureInput travels on.

Generic and input-only: it owns no GPU resource - every image involved belongs to the cutout that
asked for it - and it takes no framebuffer slot. It is in the graph so that the frame knows the
dependency exists, and so that its handle is finalized with the frame. */
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
