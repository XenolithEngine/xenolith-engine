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

#ifndef TESTS_WINDOW_SRC_DOCK_DOCKLAYOUTLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_DOCKLAYOUTLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// The placement pass of ui::DockSystem, with no interaction involved.
//
// What it is really checking is the load-bearing claim of the whole design: the frames are FLAT
// children of one root and the split tree only exists as data, yet the rects that come out tile
// that root exactly - no gaps, no overlap, the dividers accounted for. If the tree and the scene
// ever drift apart, that is where it shows.
//
// Beyond the tiling it pins down the four things that are easy to get backwards:
//
//   * a Vertical split puts `first` on TOP. The scene's Y axis points up while the CSS intuition
//     behind "first" points down, and exactly one of the two flips has to happen;
//
//   * `ratio` divides the space left AFTER both children got their minimums, not the whole extent.
//     That is why a pane with a large minimum does not eat the proportion of its neighbour;
//
//   * a panel's declared minimum raises the minimum of the frame it is parked in, and that raise
//     propagates up through every split above it;
//
//   * a root smaller than the tree's minimum scales every minimum down proportionally instead of
//     letting anything escape the root.
class DockLayoutLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);
	void expectNear(StringView phase, StringView what, float actual, float expected);

	// every leaf rect, plus the divider bands, must add up to the root and never overlap
	void expectTiling(StringView phase);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();

	Value dumpTree() const;

	// Height the tab strip of a frame takes off its body. Read rather than hard-coded: it comes
	// from the tabs' own measurement, so a change of font or padding must move the expectations
	// with it instead of breaking them.
	float stripHeight(ui::DockNodeHandle) const;

	Node *_root = nullptr;
	ui::DockSystem *_dock = nullptr;

	ui::DockNodeHandle _sidebar;
	ui::DockNodeHandle _main;
	ui::DockNodeHandle _bottom;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_DOCKLAYOUTLAYOUT_H_
