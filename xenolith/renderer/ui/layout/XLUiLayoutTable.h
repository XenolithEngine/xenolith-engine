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

#ifndef XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTTABLE_H_
#define XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTTABLE_H_

#include "XLUiLayoutGrid.h" // GridTrack / GridAlign / parseGridTemplate are reused verbatim

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* CSS table placement: a column template (`GridTrack`s from `parseGridTemplate`) shared by every
row, colspan / rowspan, and collapsed borders.

- `LayoutMode::Table` runs on the container: measures the cells of every row, resolves the column
  tracks and stamps them onto each row as a `TableColumnsComponent`.
- `LayoutMode::TableRow` runs on a row: places its cells into the stamped columns. Rows of a
  virtualized `ui::TableView` get the same component from the view instead.

Borders are published as rects in a `TableBordersComponent`; a LayoutSystem never creates nodes,
so drawing is up to a consumer such as `ui::TableBorderPainter`. */

using stappler::document::BorderCollapse;
using stappler::document::BorderStyle;
using stappler::document::TableLayout;

// One declared cell edge, before collapse resolution.
struct SP_PUBLIC TableBorderEdge {
	float width = 0.0f;
	BorderStyle style = BorderStyle::None;
	Color4B color = Color4B::BLACK;

	// An edge with no style or no width takes part in nothing and loses every conflict.
	bool isVisible() const { return style != BorderStyle::None && width > 0.0f; }

	bool operator==(const TableBorderEdge &) const = default;
	bool operator!=(const TableBorderEdge &) const = default;
};

// Component attached to the *container* node in table mode (CSS `display: table`).
struct SP_PUBLIC TableLayoutInfo {
	static ComponentId Id;

	// Column template shared by every row, already repeat()-expanded. Columns past its end are
	// sized by `autoColumn`.
	Vector<GridTrack> columnTracks;
	GridTrack autoColumn;

	// `table-layout`. Fixed never measures a cell - the tracks alone decide the widths.
	TableLayout algorithm = TableLayout::Auto;

	BorderCollapse borderCollapse = BorderCollapse::Separate;

	// `border-spacing`. Ignored under Collapse, as in CSS.
	float borderSpacingH = 0.0f;
	float borderSpacingV = 0.0f;

	// inner padding of the container's content box
	Padding padding;

	// default per-cell alignment, inherited by every row through TableColumnsComponent
	GridAlign justifyItems = GridAlign::Stretch;
	GridAlign alignItems = GridAlign::Stretch;

	// the table's own outer border: the stand-in participant at the edges of the collapse pass
	TableBorderEdge borderTop, borderRight, borderBottom, borderLeft;

	bool operator==(const TableLayoutInfo &) const = default;
	bool operator!=(const TableLayoutInfo &) const = default;
};

/* Resolved column geometry: the output of a Table pass and the input of a TableRow pass.
Single writer: either the table (`LayoutMode::Table`) or the row owner (`ui::TableView`), never
both, or rows would disagree on column positions. */
struct SP_PUBLIC TableColumnsComponent {
	static ComponentId Id;

	struct Column {
		float width = 0.0f;
		float position = 0.0f; // x of the column's left edge, from the row content box's left

		bool operator==(const Column &) const = default;
	};

	Vector<Column> columns;

	float contentWidth = 0.0f; // total width the row occupies (columns + spacing)
	float rowHeight = 0.0f; // the height the writer committed to this row

	BorderCollapse borderCollapse = BorderCollapse::Separate;
	float borderSpacingH = 0.0f;
	float borderSpacingV = 0.0f;
	GridAlign justifyItems = GridAlign::Stretch;
	GridAlign alignItems = GridAlign::Stretch;

	// columns taken by a rowspan cell from an earlier row, skipped by this row's cells;
	// one byte per column
	Vector<uint8_t> occupiedColumns;

	// Heights of this row and the rows below it, so a `rowSpan > 1` cell can size itself. [0] is
	// this row. A virtualized view fills in only the rows it has materialized.
	Vector<float> spanRowHeights;

	// Bumped by the writer only when the geometry changed; a row re-lays-out when it moves.
	// Don't bump it on an unchanged pass: a view keys node reuse off it.
	uint64_t generation = 0;

	bool operator==(const TableColumnsComponent &) const = default;
	bool operator!=(const TableColumnsComponent &) const = default;
};

// Component attached to a *row* node (CSS `display: table-row`), a direct child of the container.
struct SP_PUBLIC TableRowInfo {
	static ComponentId Id;

	// sentinel for "measure the row from its cells"
	static constexpr float Auto = -1.0f;

	float height = Auto;
	int32_t order = 0; // CSS `order`, applied after the z-order sort, as in flex/grid

	bool operator==(const TableRowInfo &) const = default;
	bool operator!=(const TableRowInfo &) const = default;
};

// Component attached to a *cell* node, a direct child of a row. A cell's column is its position
// in the row, after the columns occupied by rowspans from above (as in HTML).
struct SP_PUBLIC TableCellInfo {
	static ComponentId Id;

	uint32_t columnSpan = 1;
	uint32_t rowSpan = 1;

	// per-cell override of the container's justify/align-items (Auto inherits)
	GridAlign justifySelf = GridAlign::Auto;
	GridAlign alignSelf = GridAlign::Auto;

	// outer margin, kept inside the cell box
	Padding margin;

	TableBorderEdge borderTop, borderRight, borderBottom, borderLeft;

	bool operator==(const TableCellInfo &) const = default;
	bool operator!=(const TableCellInfo &) const = default;
};

// One rect to fill, in the content-box coordinates of the node that owns the component (Xenolith
// bottom-left origin, the same space a child's position lives in).
struct SP_PUBLIC TableBorderRect {
	Rect rect;
	Color4B color;

	bool operator==(const TableBorderRect &) const = default;
};

/* Collapsed-border geometry produced by a layout pass, painted by a consumer (e.g.
`ui::TableBorderPainter`). Written on the node whose content box the rects use: the container
for a static table (all lines), or each row of a virtualized one (that row's lines). */
struct SP_PUBLIC TableBordersComponent {
	static ComponentId Id;

	Vector<TableBorderRect> rects;
	uint64_t generation = 0;

	bool operator==(const TableBordersComponent &) const = default;
	bool operator!=(const TableBordersComponent &) const = default;
};

// One cell as the collapse pass sees it: its place in the grid and its border box in the output
// coordinate space.
struct SP_PUBLIC TableCellBox {
	uint32_t column = 0;
	uint32_t row = 0;
	uint32_t columnSpan = 1;
	uint32_t rowSpan = 1;
	Rect box;
	TableBorderEdge top, right, bottom, left;
};

/* Resolve CSS border conflicts between adjacent cells into rects in the `TableCellBox::box` space.
`outer` is the table's own border (top / right / bottom / left), used at the grid edges.
`out` is cleared first. */
SP_PUBLIC void collapseTableBorders(SpanView<TableCellBox> cells, uint32_t columnCount,
		uint32_t rowCount, const TableBorderEdge (&outer)[4], Vector<TableBorderRect> &out);

/* Resolve a column track list into `out.columns` / `out.contentWidth` with the Table pass's track
sizing, for rows without a table container (ui::TableView). `available` excludes padding and
column spacing. No content contributions: `Auto` tracks resolve to zero, so use `fr` or lengths.
Nothing else in `out` is touched, `generation` included. */
SP_PUBLIC void resolveTableColumns(SpanView<GridTrack> tracks, float available, float spacingH,
		float paddingLeft, TableColumnsComponent &out);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_LAYOUT_XLUILAYOUTTABLE_H_
