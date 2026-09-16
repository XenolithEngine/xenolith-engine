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

#ifndef TESTS_WINDOW_SRC_RENDER_FRAMECAPTURELAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_FRAMECAPTURELAYOUT_H_

#include "app/TestLayout.h"
#include "XLFrameCapture.h"
#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for FrameCapture: a rectangle of the frame copied into a Texture, drawn back
// beside the thing it was copied from.
//
// The target is a 2x2 block of flat colours rather than one colour, because that is what makes the
// check strict: a copy with the wrong origin, a flipped y or a swapped colour channel all still
// produce a plausible-looking rectangle, and only the arrangement of the four quadrants tells them
// apart. The mirror sits at the same size directly to the right, so ONE screenshot answers the
// whole question - the two blocks must be identical.
//
// The OVERLAY veil is the second half of the check. It is a subtree marked with Node::setOverlay
// that covers the target exactly, and it exists to be excluded: the overlay draws after the frame
// has been copied out, so with the veil up the screen shows the veil while the cutout still shows
// the four quadrants underneath it. Only its root is marked - the coloured layers inside it inherit
// the level - so the same run also proves the inheritance.
//
// Drive it over the inspector: `frame-capture.request` arms a capture and answers with the region
// it resolved, `frame-capture.state` reports what became of it, `frame-capture.overlay` raises or
// lowers the veil. The request needs a frame to happen before the copy can land, so a headless run
// is: request -> step_frame -> screenshot.
class FrameCaptureLayout : public TestLayout {
public:
	static constexpr float BlockWidth = 240.0f;
	static constexpr float BlockHeight = 160.0f;
	static constexpr float Gap = 40.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	AppWindow *getAppWindow() const;

	// World-space rect of the target block, which is what a capture region is derived from.
	Rect getTargetRect() const;

	// Arm one capture of the target block. The previous one is dropped: a target is immutable, so
	// re-requesting means a new image rather than a refill.
	Value requestCapture();

	Value encodeState() const;

	Node *_target = nullptr; // the four coloured quadrants
	Node *_veil = nullptr; // the overlay subtree covering the target; hidden by default
	basic2d::Sprite *_mirror = nullptr; // what the capture produced
	Rc<FrameCaptureTarget> _capture;
	URect _lastRegion;
	size_t _requests = 0;
	size_t _completions = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_RENDER_FRAMECAPTURELAYOUT_H_
