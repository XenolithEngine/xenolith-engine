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

#ifndef TESTS_WINDOW_SRC_MULTIWINDOWLAYOUT_H_
#define TESTS_WINDOW_SRC_MULTIWINDOWLAYOUT_H_

#include "TestLayout.h"
#include "XLWindowSceneInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Two top-level windows in one process, sharing one font atlas.
//
// The point is the sharing: the FontController, its FontFaceSet::_required set and the atlas image
// belong to the process, not to a window, while the gating DependencyEvent that says "these glyphs
// are on the GPU now" is attached to a frame - and there is one frame per window. So the second
// window is where a glyph can be laid out, drawn, and yet not be in the atlas the shader samples.
//
// Phase 1 (t=1.2s): the primary window shows a label; its glyphs enter _required for the first time.
// Phase 2 (t=2.4s): a second Root window opens with a label carrying the SAME text - every glyph it
//   needs is already `required`, so nothing about it is new to the font controller.
// Phase 3 (t=4.0s): both labels must have laid out identically, and the second window's label must
//   have been gated on a font dependency of its own (that is what makes its first frame wait for
//   the atlas instead of sampling empty slots).
class MultiWindowLayout : public TestLayout {
public:
	static constexpr auto kSecondWindowId = StringView("testapp-secondary");

	// Same text in both windows: the second window must not contribute a single new glyph.
	static constexpr auto kSharedText =
			StringView("Quartz glyph vex: WJMB 0123 - shared atlas probe");

	// Phase 3 text: glyphs neither window has ever laid out. It has to be BIG - a few hundred
	// distinct code points at a large size - so that rasterising and uploading them still is not
	// done a tick later, which is the window the secondary window has to land in.
	static WideStringView getRaceText();

	// Large enough that each glyph is an expensive bitmap, which is what stretches the upload past
	// the tick boundary.
	static constexpr uint16_t kRaceFontSize = 72;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleExit() override;

protected:
	void openSecondWindow();
	void raceStepPrimary();
	void raceStepSecondary();
	// Renders the secondary window offscreen and counts the pixels the text lit up. That is the only
	// way to see this bug: the label lays out and reports a correct size either way - what differs is
	// whether the shader found its glyphs in the atlas.
	void captureSecondWindow();
	void runChecks();

	void expect(bool cond, StringView what);

	basic2d::Label *_primaryLabel = nullptr;

	// Handle of the second window: the object SecondaryWindow::open returned. It is what the test
	// reaches the other window through, in place of a lookup by id.
	Rc<WindowSceneInfo> _secondWindow;

	bool _raceApplied = false;
	bool _captureDone = false;
	uint64_t _litPixels = 0;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_MULTIWINDOWLAYOUT_H_
