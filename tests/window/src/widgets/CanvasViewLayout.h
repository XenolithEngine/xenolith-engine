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

#ifndef TESTS_WINDOW_SRC_WIDGETS_CANVASVIEWLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_CANVASVIEWLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiCanvasView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Three markers at known world positions inside one ui::CanvasView.
//
// WHAT THIS STAND IS FOR, AND WHAT IT DELIBERATELY IS NOT. The arithmetic of a viewport is checked
// with no window at all - it is `sprt::geom`'s, and a console harness asserts it on two ABIs. What
// only a window can answer is whether the WIDGET's transform agrees with that arithmetic: whether
// the place the math says a world point lands is the place the node actually is. So every command
// below reports BOTH numbers for the same marker, and the check compares them. A stand that
// reported only one would be a stand that cannot fail.
//
// The markers are three because two would not catch a sign: one at the origin, one up and to the
// right, one down and to the left.
class CanvasViewLayout : public TestLayout {
public:
	struct Marker {
		StringView name;
		Vec2 world;
		Size2 size;
		Color4B color;
	};

	// The stand's own geometry, repeated in canvas-check.py on purpose: a check that reads its
	// expectations out of the thing it is checking cannot fail.
	static constexpr float MarkerW = 120.0f;
	static constexpr float MarkerH = 80.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	// Everything a command answers with, in one place: the viewport read off the widget, and per
	// marker the two numbers that must agree.
	Value encodeState() const;

	ui::CanvasView *_canvas = nullptr;
	Vector<Pair<Marker, Node *>> _markers;
};

} // namespace stappler::xenolith::app

#endif /* TESTS_WINDOW_SRC_WIDGETS_CANVASVIEWLAYOUT_H_ */
