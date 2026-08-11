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

#ifndef TESTS_WINDOW_SRC_DOCK_DOCKSPLITTERLAYOUT_H_
#define TESTS_WINDOW_SRC_DOCK_DOCKSPLITTERLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Resizing a dock by dragging the divider between two frames.
//
// The dividers are separate flat nodes, one per split, and they must sit in a ZOrder band ABOVE
// the frames - sortAllChildren is not stable, so at an equal band a frame could end up over a
// divider and swallow the drag. That is asserted here rather than left to chance.
//
// The interesting property of the drag is that it is a FIXED POINT of the placement pass: the
// ratio is derived back out of where the divider landed with the exact inverse of the formula
// that placed it, so a hundred small deltas land in the same place as one large one. The test
// checks that by dragging in steps and comparing against a single jump.
//
// And the clamp: pulling a divider past a frame's propagated minimum must stop at that minimum
// and must never push the frame on the other side below its own.
class DockSplitterLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	void expect(bool cond, StringView phase, StringView what);
	void expectNear(StringView phase, StringView what, float actual, float expected);

	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();

	Node *_root = nullptr;
	ui::DockSystem *_dock = nullptr;

	ui::DockNodeHandle _split; // the horizontal split between left and right
	ui::DockNodeHandle _left;
	ui::DockNodeHandle _right;

	// a second dock, whose right frame forbids resizing
	Node *_frozenRoot = nullptr;
	ui::DockSystem *_frozenDock = nullptr;
	ui::DockNodeHandle _frozenSplit;

	float _widthAfterSteps = 0.0f;

	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DOCK_DOCKSPLITTERLAYOUT_H_
