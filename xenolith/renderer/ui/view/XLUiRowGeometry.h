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


#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIROWGEOMETRY_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIROWGEOMETRY_H_

#include "XLUiConfig.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** Where a row of a virtualized list LIES, whether or not its node was ever built.

WHY THIS IS A FREE FUNCTION AND NOT A METHOD. ui::TableView and ui::TreeView have no common
ancestor - both derive straight from ui::Panel - and their row machinery is duplicated on purpose,
line for line. But the arithmetic below is the same in both, because the thing underneath is the
same: one basic2d::ScrollView holding one ScrollController. Writing it twice inside the engine
would be the very thing this code exists to prevent a CALLER from doing.

WHY IT CAN ANSWER FOR A ROW THAT HAS NO NODE. rebuildRows() hands the controller one item per row -
every row, not every visible row - with the height it resolved before any node existed. Only the
NODES are virtualized. So `Item::pos` and `Item::size` are the committed geometry of the whole list,
and a row scrolled far out of sight still has a rectangle to report.

THREE THINGS THAT ARE EASY TO GET WRONG HERE, AND ARE GOT RIGHT ONCE:

 1. `Item::size.width` is nan(). The width is resolved only when a node is updated, from the scroll
    root - so the rectangle takes its width from the root, never from the item.
 2. ScrollController::getItems() has a const and a non-const overload, and the NON-const one sets
    _infoDirty. Reading geometry through it would mark the list dirty and cost a rebuild, so
    everything here goes through the const one.
 3. The controller has no items until rebuildRows() runs, and that is deferred to the next visit.
    Until then there is no answer - and saying so is the answer, rather than reporting a zero. */
struct SP_PUBLIC RowGeometrySource {
	// Whose coordinate space the answers are in. Normally the view widget itself.
	const Node *view = nullptr;

	const basic2d::ScrollView *scroll = nullptr;
	const basic2d::ScrollController *controller = nullptr;

	bool empty() const { return !view || !scroll || !controller; }
	size_t getRowCount() const;
};

// False when there is no such row, or when the list has not been laid out yet.
SP_PUBLIC bool getRowRect(const RowGeometrySource &, size_t index, Rect &out);

/* Which row lies at a point, or maxOf<size_t>() when none does.

The point is read in CONTENT space, not in the visible box: a point above or below the viewport
names the row that WOULD be there, exactly as getRowRect reports a rectangle for a row that
scrolled out of sight. The two have to agree, and answering "none" here for a position getRowRect
happily describes would be the pair contradicting each other.

Only a point outside the content altogether - before the first row or past the last - is a miss. */
SP_PUBLIC size_t getRowIndexAt(const RowGeometrySource &, const Vec2 &viewLocation);

/* The BOUNDARY an insertion would snap to: 0..rowCount, and never a row index.

A separate question from getRowIndexAt, and separate on purpose: "which row is the pointer over"
and "between which two rows would it land" have different answers for the same point, and confusing
them is exactly how an insertion line ends up drawn through the middle of a row.

`boundaryRect`, when given, receives a thin rectangle on that boundary - what a caller draws. */
SP_PUBLIC size_t getRowBoundaryAt(const RowGeometrySource &, const Vec2 &viewLocation,
		Rect *boundaryRect = nullptr, float thickness = 2.0f);

// The rectangle of a boundary index, for a caller that already knows which one it wants.
SP_PUBLIC bool getRowBoundaryRect(const RowGeometrySource &, size_t boundary, Rect &out,
		float thickness = 2.0f);

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUIROWGEOMETRY_H_ */
