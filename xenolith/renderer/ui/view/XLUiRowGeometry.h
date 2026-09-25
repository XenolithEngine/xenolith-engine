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
#include "XLSelectionSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** Row geometry of a virtualized list (a ScrollView with one ScrollController), shared by
TableView and TreeView. The controller holds an item per row, so rows without a node still
have a rectangle.

Width comes from the scroll root (`Item::size.width` is nan()). Only the const getItems() is
used: the non-const one sets _infoDirty. Before the deferred rebuildRows() there are no items
and the functions report failure. */
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
The point is read in content space, so a point outside the viewport names the row that would be
there (consistent with getRowRect). Only a point before the first or past the last row misses. */
SP_PUBLIC size_t getRowIndexAt(const RowGeometrySource &, const Vec2 &viewLocation);

/* The rows a rectangle in the view's space reaches across, by their extent along the list: a row
is in when its span meets [minY, maxY]. The rectangle may reach outside the viewport, where rows
without a node still have a place. False when it meets no row. */
SP_PUBLIC bool getRowRangeIn(const RowGeometrySource &, const Rect &viewRect, size_t &first,
		size_t &last);

/* The boundary an insertion would snap to: 0..rowCount, not a row index.
`boundaryRect`, when given, receives a thin rectangle on that boundary for drawing. */
SP_PUBLIC size_t getRowBoundaryAt(const RowGeometrySource &, const Vec2 &viewLocation,
		Rect *boundaryRect = nullptr, float thickness = 2.0f);

// The rectangle of a boundary index, for a caller that already knows which one it wants.
SP_PUBLIC bool getRowBoundaryRect(const RowGeometrySource &, size_t boundary, Rect &out,
		float thickness = 2.0f);

// Scroll by the least distance that shows the whole row. False when there is no such row.
SP_PUBLIC bool scrollRowIntoView(basic2d::ScrollView *, const basic2d::ScrollController *,
		size_t index);

/* The row a selection arriving from `fromWorld` in `dir` lands on: the closest visible row by
getSelectionDirectionScore, else the first or last visible one. maxOf<size_t>() with no rows. */
SP_PUBLIC size_t getEnteringRow(const RowGeometrySource &, SelectionDirection dir,
		const Rect &fromWorld);

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUIROWGEOMETRY_H_ */
