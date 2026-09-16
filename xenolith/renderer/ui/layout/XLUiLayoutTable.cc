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

// CSS table placement: the container pass (LayoutMode::Table), the row pass (LayoutMode::TableRow)
// and border collapsing. A subunit of XLUi.scu.cpp - see XLUiLayoutInternal.h.

#include "XLUiLayoutInternal.h"

#include "XLInheritedStyle.h" // isInlineRtl

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

namespace {

// One row of the container pass, with the cells it turned out to own.
struct TableRowEntry {
	Node *node = nullptr;
	TableRowInfo cfg;
	float height = 0.0f;
	uint32_t index = 0;
};

// A cell as the container pass places it, in column/row index space.
struct TablePlacedCell {
	Node *node = nullptr;
	TableCellInfo cfg;
	uint32_t column = 0;
	uint32_t row = 0;
	float measuredWidth = 0.0f;
	float measuredHeight = 0.0f;
};

// The container pass's result before commit: rows, placed cells, column tracks and row heights.
// Kept separate because `measure()` is a dry run and must not write any ContentSize.
struct TableSolution {
	Vector<TableRowEntry> rows;
	Vector<TablePlacedCell> cells;
	Vector<GridTrackSize> columns;

	// Per row: `occupancy` ends up holding every column the row uses, its own cells included;
	// `inherited` is the snapshot before the row's cursor ran (rowspans from above only). Rows are
	// stamped with `inherited`.
	Vector<Vector<uint8_t>> occupancy;
	Vector<Vector<uint8_t>> inherited;
	uint32_t columnCount = 0;
	float usedWidth = 0.0f;
	float usedHeight = 0.0f; // rows plus the vertical spacing between them
	float spacingH = 0.0f;
	float spacingV = 0.0f;
	bool collapse = false;
};

// The in-flow children of a table row, in placement order. `display:none` and out-of-flow nodes are
// skipped exactly as they are in flex and grid.
inline void collectCells(Node *row, const Callback<void(Node *, const TableCellInfo &)> &cb) {
	for (auto &child : row->getChildren()) {
		if (!child->isDisplayed() || child->getComponent<OutOfFlowComponent>()) {
			continue;
		}
		auto cfg = child->getComponent<TableCellInfo>();
		cb(child, cfg ? *cfg : TableCellInfo());
	}
}

// Assigns cells to the first free columns, honouring columnSpan and slots claimed by rowspans from
// earlier rows. `occupied` is indexed by column and grows past the template when needed.
struct ColumnCursor {
	Vector<uint8_t> *occupied = nullptr;
	uint32_t next = 0;

	uint32_t take(uint32_t span) {
		const uint32_t sp = sprt::max(span, 1u);
		while (true) {
			// grow the occupancy map lazily rather than guessing a column count up front
			while (occupied->size() < size_t(next) + sp) { occupied->emplace_back(0); }
			bool free = true;
			for (uint32_t i = 0; i < sp; ++i) {
				if ((*occupied)[next + i]) {
					free = false;
					break;
				}
			}
			if (free) {
				const uint32_t start = next;
				for (uint32_t i = 0; i < sp; ++i) { (*occupied)[start + i] = 1; }
				next = start + sp;
				return start;
			}
			++next;
		}
	}
};

// Border priority when two cells declare an edge on the same grid line. BorderStyle here has only
// four values, so the CSS 2.1 table (hidden > double > solid > dashed > dotted > ...) collapses to
// this: None loses to everything, then width, then Solid > Dashed > Dotted.
inline int borderStyleRank(BorderStyle s) {
	switch (s) {
	case BorderStyle::Solid: return 3;
	case BorderStyle::Dashed: return 2;
	case BorderStyle::Dotted: return 1;
	case BorderStyle::None: break;
	}
	return 0;
}

// Resolve one grid-line segment. `a` is the edge of the cell to the left / above, `b` of the one to
// the right / below; on a tie `a` wins, which makes the pass order-independent.
inline TableBorderEdge resolveEdge(const TableBorderEdge &a, const TableBorderEdge &b) {
	if (!a.isVisible()) {
		return b.isVisible() ? b : TableBorderEdge();
	}
	if (!b.isVisible()) {
		return a;
	}
	if (a.width != b.width) {
		return a.width > b.width ? a : b;
	}
	return borderStyleRank(b.style) > borderStyleRank(a.style) ? b : a;
}

} // namespace

void collapseTableBorders(SpanView<TableCellBox> cells, uint32_t columnCount, uint32_t rowCount,
		const TableBorderEdge (&outer)[4], Vector<TableBorderRect> &out) {
	out.clear();
	if (columnCount == 0 || rowCount == 0 || cells.empty()) {
		return;
	}

	// Slot -> occupant, filled by span. A segment with the same cell on both sides is a span's
	// interior and has no border.
	Vector<const TableCellBox *> grid;
	grid.resize(size_t(columnCount) * rowCount, nullptr);
	for (auto &c : cells) {
		for (uint32_t r = c.row; r < sprt::min(c.row + sprt::max(c.rowSpan, 1u), rowCount); ++r) {
			for (uint32_t x = c.column;
					x < sprt::min(c.column + sprt::max(c.columnSpan, 1u), columnCount); ++x) {
				grid[size_t(r) * columnCount + x] = &c;
			}
		}
	}
	auto at = [&](uint32_t r, uint32_t c) -> const TableCellBox * {
		return (r < rowCount && c < columnCount) ? grid[size_t(r) * columnCount + c] : nullptr;
	};

	/* The x of vertical line `c` and the y of horizontal line `r`, taken only from a cell that
	starts or ends exactly on the line; a spanning cell's box says nothing about lines inside it. */
	auto lineX = [&](uint32_t c) -> float {
		for (uint32_t r = 0; r < rowCount; ++r) {
			if (auto cell = at(r, c); cell && cell->column == c) {
				return cell->box.origin.x; // a cell starting on this line
			}
			if (c > 0) {
				if (auto cell = at(r, c - 1);
						cell && cell->column + sprt::max(cell->columnSpan, 1u) == c) {
					return cell->box.origin.x + cell->box.size.width; // one ending on it
				}
			}
		}
		return 0.0f;
	};
	// y grows upward, so row r's top edge is the max y of the cells starting in row r
	auto lineY = [&](uint32_t r) -> float {
		for (uint32_t c = 0; c < columnCount; ++c) {
			if (auto cell = at(r, c); cell && cell->row == r) {
				return cell->box.origin.y + cell->box.size.height;
			}
			if (r > 0) {
				if (auto cell = at(r - 1, c);
						cell && cell->row + sprt::max(cell->rowSpan, 1u) == r) {
					return cell->box.origin.y;
				}
			}
		}
		return 0.0f;
	};

	// winner on vertical line `c` within row `r`
	auto verticalAt = [&](uint32_t c, uint32_t r) -> TableBorderEdge {
		auto left = (c > 0) ? at(r, c - 1) : nullptr;
		auto right = (c < columnCount) ? at(r, c) : nullptr;
		if (left && right && left == right) {
			return TableBorderEdge(); // inside a colspan
		}
		const TableBorderEdge a = left ? left->right : outer[3];
		const TableBorderEdge b = right ? right->left : outer[1];
		return resolveEdge(a, b);
	};
	auto horizontalAt = [&](uint32_t r, uint32_t c) -> TableBorderEdge {
		auto above = (r > 0) ? at(r - 1, c) : nullptr;
		auto below = (r < rowCount) ? at(r, c) : nullptr;
		if (above && below && above == below) {
			return TableBorderEdge(); // inside a rowspan
		}
		const TableBorderEdge a = above ? above->bottom : outer[0];
		const TableBorderEdge b = below ? below->top : outer[2];
		return resolveEdge(a, b);
	};

	// Verticals span junctions; horizontals are inset by half the vertical width at each end, so
	// rects never overlap and translucent colours do not double-blend.
	auto verticalWidthAtCorner = [&](uint32_t c, uint32_t r) -> float {
		// the widest vertical segment meeting horizontal line r on vertical line c
		float w = 0.0f;
		if (r > 0) {
			w = sprt::max(w, verticalAt(c, r - 1).width);
		}
		if (r < rowCount) {
			w = sprt::max(w, verticalAt(c, r).width);
		}
		return w;
	};

	// --- vertical lines, merging consecutive rows that resolve to the same edge ---
	for (uint32_t c = 0; c <= columnCount; ++c) {
		const float x = lineX(c);
		uint32_t runStart = 0;
		TableBorderEdge runEdge;
		auto flush = [&](uint32_t endRow) {
			if (!runEdge.isVisible() || endRow <= runStart) {
				return;
			}
			const float yTop = lineY(runStart);
			const float yBottom = lineY(endRow);
			out.emplace_back(TableBorderRect{
				Rect(x - runEdge.width / 2.0f, yBottom, runEdge.width, yTop - yBottom),
				runEdge.color});
		};
		for (uint32_t r = 0; r < rowCount; ++r) {
			const auto edge = verticalAt(c, r);
			if (r == 0) {
				runEdge = edge;
				runStart = 0;
			} else if (edge != runEdge) {
				flush(r);
				runEdge = edge;
				runStart = r;
			}
		}
		flush(rowCount);
	}

	// --- horizontal lines, merging consecutive columns ---
	for (uint32_t r = 0; r <= rowCount; ++r) {
		const float y = lineY(r);
		uint32_t runStart = 0;
		TableBorderEdge runEdge;
		auto flush = [&](uint32_t endCol) {
			if (!runEdge.isVisible() || endCol <= runStart) {
				return;
			}
			const float xLeft = lineX(runStart) + verticalWidthAtCorner(runStart, r) / 2.0f;
			const float xRight = lineX(endCol) - verticalWidthAtCorner(endCol, r) / 2.0f;
			if (xRight <= xLeft) {
				return;
			}
			out.emplace_back(TableBorderRect{
				Rect(xLeft, y - runEdge.width / 2.0f, xRight - xLeft, runEdge.width),
				runEdge.color});
		};
		for (uint32_t c = 0; c < columnCount; ++c) {
			const auto edge = horizontalAt(r, c);
			if (c == 0) {
				runEdge = edge;
				runStart = 0;
			} else if (edge != runEdge) {
				flush(c);
				runEdge = edge;
				runStart = c;
			}
		}
		flush(columnCount);
	}
}

// Work out the whole table without touching a single node. Returns false when there is nothing to
// lay out. Reads node state (and asks cells to measure themselves), writes none.
static bool solveTable(Node *owner, const TableLayoutInfo &info, float contentW,
		TableSolution &out) {
	out.collapse = info.borderCollapse == BorderCollapse::Collapse;
	// border-spacing only exists in the separate model, as in CSS
	out.spacingH = out.collapse ? 0.0f : info.borderSpacingH;
	out.spacingV = out.collapse ? 0.0f : info.borderSpacingV;

	// 1. Collect the rows, in z-order, then stable by `order`.
	for (auto &child : owner->getChildren()) {
		if (!child->isDisplayed() || child->getComponent<OutOfFlowComponent>()) {
			continue;
		}
		TableRowEntry row;
		row.node = child;
		if (auto cfg = child->getComponent<TableRowInfo>()) {
			row.cfg = *cfg;
		}
		out.rows.emplace_back(row);
	}
	if (out.rows.empty()) {
		return false;
	}
	auto &rows = out.rows;
	for (size_t i = 1; i < rows.size(); ++i) { // insertion sort by order; tiny counts
		TableRowEntry key = rows[i];
		size_t j = i;
		while (j > 0 && rows[j - 1].cfg.order > key.cfg.order) {
			rows[j] = rows[j - 1];
			--j;
		}
		rows[j] = key;
	}
	for (uint32_t i = 0; i < rows.size(); ++i) { rows[i].index = i; }

	const uint32_t rowCount = uint32_t(rows.size());

	// 2. Place the cells. `occupancy[r]` is the set of columns row r has already given away, either
	// to its own cells or to a rowspan reaching down from above.
	out.occupancy.resize(rowCount);
	out.inherited.resize(rowCount);
	auto &cells = out.cells;
	uint32_t columnCount = 0;

	for (uint32_t r = 0; r < rowCount; ++r) {
		// snapshot what earlier rows' rowspans already claimed, before this row consumes anything
		out.inherited[r] = out.occupancy[r];
		ColumnCursor cursor{&out.occupancy[r], 0};
		collectCells(rows[r].node, [&](Node *node, const TableCellInfo &cfg) {
			TablePlacedCell cell;
			cell.node = node;
			cell.cfg = cfg;
			cell.row = r;
			cell.column = cursor.take(cfg.columnSpan);

			// claim the same columns in the rows this cell spans down into
			const uint32_t rowSpan = sprt::max(cfg.rowSpan, 1u);
			const uint32_t colSpan = sprt::max(cfg.columnSpan, 1u);
			for (uint32_t rr = r + 1; rr < sprt::min(r + rowSpan, rowCount); ++rr) {
				auto &occ = out.occupancy[rr];
				while (occ.size() < size_t(cell.column) + colSpan) { occ.emplace_back(0); }
				for (uint32_t i = 0; i < colSpan; ++i) { occ[cell.column + i] = 1; }
			}
			columnCount = sprt::max(columnCount, cell.column + colSpan);
			cells.emplace_back(cell);
		});
	}
	columnCount = sprt::max(columnCount, uint32_t(info.columnTracks.size()));
	if (columnCount == 0) {
		return false;
	}
	out.columnCount = columnCount;

	// 3. Size the columns. Only Auto tracks are measured, never under `table-layout: fixed`.
	auto &columns = out.columns;
	columns.resize(columnCount);
	for (uint32_t i = 0; i < columnCount; ++i) {
		columns[i].def = (i < info.columnTracks.size()) ? info.columnTracks[i] : info.autoColumn;
	}

	bool anyAuto = false;
	for (auto &t : columns) {
		if (t.def.type == GridTrack::Auto) {
			anyAuto = true;
			break;
		}
	}

	Vector<TrackContribution> contributions;
	if (anyAuto && info.algorithm == TableLayout::Auto) {
		contributions.reserve(cells.size());
		for (auto &cell : cells) {
			// max-content size if measurable, otherwise the intrinsic size
			const Size2 m = LayoutSystem_canMeasure(cell.node)
					? LayoutSystem::measureNode(cell.node,
							  MeasureConstraints{MeasureMode::MaxContent})
					: intrinsicSize(cell.node);
			cell.measuredWidth = m.width + cell.cfg.margin.horizontal();
			cell.measuredHeight = m.height + cell.cfg.margin.vertical();
			contributions.emplace_back(TrackContribution{cell.column,
				sprt::max(cell.cfg.columnSpan, 1u), cell.measuredWidth});
		}
	} else {
		for (auto &cell : cells) {
			const Size2 m = intrinsicSize(cell.node);
			cell.measuredWidth = m.width + cell.cfg.margin.horizontal();
			cell.measuredHeight = m.height + cell.cfg.margin.vertical();
		}
	}

	// The available width excludes the spacing between the columns (separate model).
	const float spacingTotal =
			out.spacingH * static_cast<float>(columnCount > 0 ? columnCount - 1 : 0);
	resolveTrackSizes(columns, contributions, sprt::max(contentW - spacingTotal, 0.0f), 0.0f);

	// Lay the columns out left to right; resolveTrackSizes worked with gap 0, so the spacing is
	// applied here rather than folded into the track sizes.
	float x = 0.0f;
	for (auto &t : columns) {
		t.position = x;
		x += t.base + out.spacingH;
	}
	out.usedWidth = (columnCount > 0) ? (x - out.spacingH) : 0.0f;

	// 4. Row heights: an explicit height wins, otherwise the tallest non-spanning cell in the row.
	// Rowspan cells are applied afterwards as a deficit spread over the rows they cover.
	for (uint32_t r = 0; r < rowCount; ++r) {
		if (rows[r].cfg.height >= 0.0f) {
			rows[r].height = rows[r].cfg.height;
			continue;
		}
		float h = 0.0f;
		for (auto &cell : cells) {
			if (cell.row == r && sprt::max(cell.cfg.rowSpan, 1u) == 1) {
				h = sprt::max(h, cell.measuredHeight);
			}
		}
		rows[r].height = h;
	}
	// spanning cells: grow the rows they cover to cover the deficit, evenly
	for (auto &cell : cells) {
		const uint32_t rowSpan = sprt::max(cell.cfg.rowSpan, 1u);
		if (rowSpan <= 1 || cell.row + rowSpan > rowCount) {
			continue;
		}
		float covered = out.spacingV * static_cast<float>(rowSpan - 1);
		for (uint32_t r = cell.row; r < cell.row + rowSpan; ++r) { covered += rows[r].height; }
		const float deficit = cell.measuredHeight - covered;
		if (deficit > 0.0f) {
			const float add = deficit / static_cast<float>(rowSpan);
			for (uint32_t r = cell.row; r < cell.row + rowSpan; ++r) { rows[r].height += add; }
		}
	}

	out.usedHeight = 0.0f;
	for (auto &row : rows) { out.usedHeight += row.height + out.spacingV; }
	out.usedHeight = sprt::max(out.usedHeight - out.spacingV, 0.0f);
	return true;
}

void resolveTableColumns(SpanView<GridTrack> tracks, float available, float spacingH,
		float paddingLeft, TableColumnsComponent &out) {
	const uint32_t count = uint32_t(tracks.size());
	if (count == 0) {
		out.columns.clear();
		out.contentWidth = 0.0f;
		return;
	}

	Vector<GridTrackSize> sizes;
	sizes.resize(count);
	for (uint32_t i = 0; i < count; ++i) { sizes[i].def = tracks[i]; }

	resolveTrackSizes(sizes, SpanView<TrackContribution>(), available, 0.0f);

	out.columns.resize(count);
	float x = paddingLeft;
	for (uint32_t i = 0; i < count; ++i) {
		out.columns[i].position = x;
		out.columns[i].width = sizes[i].base;
		x += sizes[i].base + spacingH;
	}
	out.contentWidth = sprt::max(x - spacingH - paddingLeft, 0.0f);
}

void LayoutSystem::layoutTable() {
	auto infoPtr = _owner->getComponent<TableLayoutInfo>();
	const TableLayoutInfo info = infoPtr ? *infoPtr : TableLayoutInfo();

	const Size2 containerSize = _owner->getContentSize();
	const float contentW = sprt::max(containerSize.width - info.padding.horizontal(), 0.0f);

	// Columns are in logical inline coordinates; rtl mirrors rows and border boxes at projection.
	// Each row mirrors its own cells in layoutTableRow.
	const bool rtl = isInlineRtl(_owner);

	TableSolution sol;
	if (!solveTable(_owner, info, contentW, sol)) {
		return;
	}
	auto &rows = sol.rows;
	auto &cells = sol.cells;
	auto &columns = sol.columns;
	const uint32_t rowCount = uint32_t(rows.size());
	const uint32_t columnCount = sol.columnCount;
	const float usedWidth = sol.usedWidth;
	const float spacingV = sol.spacingV;

	// 5. Stamp the resolved geometry on every row and commit its box. The stamp must precede the
	// row's own pass; writing it marks the row dirty, which re-arms that pass.
	TableColumnsComponent stamp;
	stamp.columns.resize(columnCount);
	for (uint32_t i = 0; i < columnCount; ++i) {
		stamp.columns[i].width = columns[i].base;
		stamp.columns[i].position = columns[i].position;
	}
	stamp.contentWidth = usedWidth;
	stamp.borderCollapse = info.borderCollapse;
	stamp.borderSpacingH = sol.spacingH;
	stamp.borderSpacingV = sol.spacingV;
	stamp.justifyItems = info.justifyItems;
	stamp.alignItems = info.alignItems;

	float y = 0.0f; // distance from the container's content-box top, CSS-style
	for (uint32_t r = 0; r < rowCount; ++r) {
		auto &row = rows[r];

		stamp.rowHeight = row.height;
		stamp.occupiedColumns.clear();
		// only what this row inherited from rowspans above (see TableSolution)
		stamp.occupiedColumns.resize(columnCount, 0);
		for (uint32_t c = 0; c < columnCount && c < sol.inherited[r].size(); ++c) {
			stamp.occupiedColumns[c] = sol.inherited[r][c];
		}
		stamp.spanRowHeights.clear();
		for (uint32_t rr = r; rr < rowCount; ++rr) { stamp.spanRowHeights.emplace_back(rows[rr].height); }

		LayoutSystem::setTableColumns(row.node, stamp);

		const Size2 rowSize(usedWidth, row.height);
		// a row narrower than the content box sits at its inline start (the right edge under rtl)
		const float rowX = info.padding.left + (rtl ? contentW - usedWidth : 0.0f);
		const Vec2 bottomLeft(rowX, containerSize.height - info.padding.top - y - row.height);
		row.node->setContentSize(rowSize);
		const Vec2 anchor = row.node->getAnchorPoint();
		row.node->setPosition(
				bottomLeft + Vec2(anchor.x * rowSize.width, anchor.y * rowSize.height));
		dispatchLayoutApplied(row.node, rowSize);

		y += row.height + spacingV;
	}

	// 6. Collapsed borders, over the whole grid, in the container's own content-box space.
	if (sol.collapse) {
		Vector<TableCellBox> boxes;
		boxes.reserve(cells.size());
		for (auto &cell : cells) {
			const uint32_t colSpan = sprt::max(cell.cfg.columnSpan, 1u);
			const uint32_t rowSpan = sprt::max(cell.cfg.rowSpan, 1u);
			const uint32_t lastCol = sprt::min(cell.column + colSpan, columnCount) - 1;
			const float bx = columns[cell.column].position;
			const float bw = columns[lastCol].position + columns[lastCol].base - bx;

			float top = 0.0f;
			for (uint32_t r = 0; r < cell.row; ++r) { top += rows[r].height + spacingV; }
			float bh = 0.0f;
			for (uint32_t r = cell.row; r < sprt::min(cell.row + rowSpan, rowCount); ++r) {
				bh += rows[r].height + spacingV;
			}
			bh = sprt::max(bh - spacingV, 0.0f);

			TableCellBox box;
			box.column = cell.column;
			box.row = cell.row;
			box.columnSpan = colSpan;
			box.rowSpan = rowSpan;
			box.box = Rect(info.padding.left + (rtl ? contentW - (bx + bw) : bx),
					containerSize.height - info.padding.top - top - bh, bw, bh);
			box.top = cell.cfg.borderTop;
			box.right = cell.cfg.borderRight;
			box.bottom = cell.cfg.borderBottom;
			box.left = cell.cfg.borderLeft;
			boxes.emplace_back(box);
		}

		const TableBorderEdge outer[4] = {info.borderTop, info.borderRight, info.borderBottom,
			info.borderLeft};
		Vector<TableBorderRect> rects;
		collapseTableBorders(boxes, columnCount, rowCount, outer, rects);

		_owner->setOrUpdateComponent<TableBordersComponent>(
				[&](NotNull<TableBordersComponent> c) {
			if (c->rects == rects) {
				return false;
			}
			c->rects = sp::move(rects);
			++c->generation;
			return true;
		});
	} else {
		_owner->removeComponent<TableBordersComponent>();
	}
}

void LayoutSystem::layoutTableRow() {
	auto colsPtr = _owner->getComponent<TableColumnsComponent>();
	if (!colsPtr || colsPtr->columns.empty()) {
		// not stamped yet (normal before the table's or TableView's first pass); the stamp marks
		// the row dirty and re-runs this
		return;
	}
	const TableColumnsComponent cols = *colsPtr;
	const Size2 rowSize = _owner->getContentSize();
	// the row's own direction, inherited from the table
	const bool rtl = isInlineRtl(_owner);
	const uint32_t columnCount = uint32_t(cols.columns.size());

	Vector<uint8_t> occupied = cols.occupiedColumns;
	occupied.resize(columnCount, 0);
	ColumnCursor cursor{&occupied, 0};

	Vector<TableCellBox> boxes;

	collectCells(_owner, [&](Node *node, const TableCellInfo &cfg) {
		const uint32_t colSpan = sprt::max(cfg.columnSpan, 1u);
		const uint32_t start = cursor.take(colSpan);
		if (start >= columnCount) {
			return; // overflowed the template; nothing sensible to place it in
		}
		const uint32_t last = sprt::min(start + colSpan, columnCount) - 1;

		const float cellX = cols.columns[start].position;
		const float cellW = cols.columns[last].position + cols.columns[last].width - cellX
				+ cols.borderSpacingH * static_cast<float>(last - start);

		// a rowspan cell overflows this row's box downward; rows below skip its columns via
		// occupiedColumns
		const uint32_t rowSpan = sprt::max(cfg.rowSpan, 1u);
		float cellH = rowSize.height;
		if (rowSpan > 1 && !cols.spanRowHeights.empty()) {
			cellH = 0.0f;
			for (uint32_t i = 0; i < rowSpan && i < cols.spanRowHeights.size(); ++i) {
				cellH += cols.spanRowHeights[i] + cols.borderSpacingV;
			}
			cellH = sprt::max(cellH - cols.borderSpacingV, 0.0f);
		}

		// inset by the cell's margin, then align; under rtl the physical margins are swapped here
		// because the mirror below flips the box
		const float availX = cellX + (rtl ? cfg.margin.right : cfg.margin.left);
		const float availW = sprt::max(cellW - cfg.margin.horizontal(), 0.0f);
		const float availY = cfg.margin.top;
		const float availH = sprt::max(cellH - cfg.margin.vertical(), 0.0f);

		auto selfAlign = [](GridAlign a, GridAlign fallback, float start, float size, float natural,
								 float &outStart, float &outSize) {
			GridAlign use = (a == GridAlign::Auto) ? fallback : a;
			if (use == GridAlign::Stretch || use == GridAlign::Auto) {
				outStart = start;
				outSize = size;
				return;
			}
			outSize = natural;
			switch (use) {
			case GridAlign::End: outStart = start + (size - natural); break;
			case GridAlign::Center: outStart = start + (size - natural) / 2.0f; break;
			default: outStart = start; break; // Start
			}
		};

		/* The cell's natural size for non-stretch alignment. Measured at the width it is about to
		get, not read from ContentSize, which holds the box the previous pass committed. */
		const Size2 natural = LayoutSystem_canMeasure(node)
				? LayoutSystem::measureNode(node, MeasureConstraints{MeasureMode::Normal, availW})
				: intrinsicSize(node);
		float bx = availX, bw = availW, by = availY, bh = availH;
		selfAlign(cfg.justifySelf, cols.justifyItems, availX, availW, natural.width, bx, bw);
		selfAlign(cfg.alignSelf, cols.alignItems, availY, availH, natural.height, by, bh);
		bw = sprt::max(bw, 0.0f);
		bh = sprt::max(bh, 0.0f);

		// Project into the row's bottom-left space (by is measured from the row's top). The
		// columns were positioned in logical inline coordinates, so rtl mirrors once, here.
		const Vec2 bottomLeft(rtl ? rowSize.width - (bx + bw) : bx, rowSize.height - by - bh);
		const Size2 size(bw, bh);
		node->setContentSize(size);
		const Vec2 anchor = node->getAnchorPoint();
		node->setPosition(bottomLeft + Vec2(anchor.x * size.width, anchor.y * size.height));
		dispatchLayoutApplied(node, size);

		if (cols.borderCollapse == BorderCollapse::Collapse) {
			TableCellBox box;
			box.column = start;
			box.row = 0;
			box.columnSpan = colSpan;
			box.rowSpan = 1;
			box.box = Rect(rtl ? rowSize.width - (cellX + cellW) : cellX, rowSize.height - cellH,
					cellW, cellH);
			box.top = cfg.borderTop;
			box.right = cfg.borderRight;
			box.bottom = cfg.borderBottom;
			box.left = cfg.borderLeft;
			boxes.emplace_back(box);
		}
	});

	// A row collapses only its own lines: verticals between cells, its top and bottom. The line
	// shared with the next row should be declared once, via `border-bottom` (see TableView).
	if (cols.borderCollapse == BorderCollapse::Collapse && !boxes.empty()) {
		const TableBorderEdge outer[4] = {};
		Vector<TableBorderRect> rects;
		collapseTableBorders(boxes, columnCount, 1, outer, rects);
		_owner->setOrUpdateComponent<TableBordersComponent>(
				[&](NotNull<TableBordersComponent> c) {
			if (c->rects == rects) {
				return false;
			}
			c->rects = sp::move(rects);
			++c->generation;
			return true;
		});
	}
}

Size2 LayoutSystem::measureTable(const MeasureConstraints &c) {
	LayoutSystem_settleChildren(_owner);

	auto infoPtr = _owner->getComponent<TableLayoutInfo>();
	const TableLayoutInfo info = infoPtr ? *infoPtr : TableLayoutInfo();

	// Same solve as the placement pass, without committing. MaxContent gives fr tracks no free
	// space, so Auto tracks report their content size.
	float contentW = maxOf<float>();
	if (c.mode != MeasureMode::MaxContent && c.maxWidth != maxOf<float>()) {
		contentW = sprt::max(c.maxWidth - info.padding.horizontal(), 0.0f);
	} else if (c.mode == MeasureMode::MaxContent) {
		contentW = 0.0f; // no free space to hand to fr tracks
	}

	TableSolution sol;
	if (!solveTable(_owner, info, contentW, sol)) {
		return Size2(info.padding.horizontal(), info.padding.vertical());
	}

	Size2 ret(sol.usedWidth + info.padding.horizontal(),
			sol.usedHeight + info.padding.vertical());
	if (c.maxWidth != maxOf<float>()) {
		ret.width = sprt::min(ret.width, c.maxWidth);
	}
	return ret;
}

Size2 LayoutSystem::measureTableRow(const MeasureConstraints &) {
	auto cols = _owner->getComponent<TableColumnsComponent>();
	if (!cols) {
		return _owner->getContentSize();
	}

	// The width is the table's to decide; the height is the tallest cell that does not span.
	float height = 0.0f;
	collectCells(_owner, [&](Node *node, const TableCellInfo &cfg) {
		if (sprt::max(cfg.rowSpan, 1u) != 1) {
			return;
		}
		const Size2 m = LayoutSystem_canMeasure(node)
				? LayoutSystem::measureNode(node, MeasureConstraints{MeasureMode::Normal})
				: intrinsicSize(node);
		height = sprt::max(height, m.height + cfg.margin.vertical());
	});
	return Size2(cols->contentWidth, height);
}

} // namespace stappler::xenolith::ui
