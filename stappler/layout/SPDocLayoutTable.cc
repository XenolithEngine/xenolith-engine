/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPDocLayoutTable.h"
#include "SPDocLayoutEngine.h"
#include "SPDocLayoutResult.h"

namespace STAPPLER_VERSIONIZED stappler::document {

static uint16_t Cell_getColSpan(const Node &node) {
	auto colSpan = node.getAttribute("colspan");
	if (!colSpan.empty()) {
		return min(uint16_t(32), uint16_t(colSpan.readInteger().get(1)));
	}
	return 1;
}

static uint16_t Cell_getRowSpan(const Node &node) {
	auto colSpan = node.getAttribute("rowspan");
	if (!colSpan.empty()) {
		return min(uint16_t(32), uint16_t(colSpan.readInteger().get(1)));
	}
	return 1;
}

LayoutTable::LayoutTable(LayoutBlock *l, const Size2 &size, NodeId nodeId)
: parentSize(size), layout(l), phantomBody(StringView("tbody")) {
	phantomBody.setNodeId(nodeId);
}

void LayoutTable::addCol(const Node *group, const Node &node) {
	uint16_t colGroup = maxOf<uint16_t>();
	bool init = false;
	size_t idx = 0;
	for (auto &it : colGroups) {
		if (it.node == group) {
			init = true;
			colGroup = idx;
			break;
		}
		++idx;
	}

	if (!init && group) {
		colGroups.emplace_back(ColGroup{uint16_t(colGroups.size()), group});
		colGroup = colGroups.size() - 1;
	}

	cols.emplace_back(Col{uint16_t(cols.size()), colGroup, &node});

	if (colGroup != maxOf<uint16_t>()) {
		colGroups[colGroup].cols.emplace_back(cols.size() - 1);
	}
}

void LayoutTable::addRow(const Node *group, const Node &node) {
	if (!group) {
		group = &phantomBody;
	}

	uint16_t rowGroup = maxOf<uint16_t>();
	bool init = false;
	size_t idx = 0;
	for (auto &it : rowGroups) {
		if (it.node == group) {
			init = true;
			rowGroup = idx;
			break;
		}
		++idx;
	}

	if (!init) {
		rowGroups.emplace_back(RowGroup{uint16_t(rowGroups.size()), group});
		rowGroup = rowGroups.size() - 1;
	}

	rows.emplace_back(Row{uint16_t(rows.size()), rowGroup, &node});

	if (rowGroup != maxOf<uint16_t>()) {
		rowGroups[rowGroup].rows.emplace_back(rows.size() - 1);
	}
}

static uint16_t nextRowIndex(const LayoutTable::Row &row, uint16_t idx) {
	while (idx < row.cells.size() && row.cells[idx] != nullptr) { ++idx; }
	return idx;
}

void LayoutTable::processRow(const Row &row, size_t colsCount) {
	auto rowsCount = uint16_t(rows.size());
	uint16_t colIndex = nextRowIndex(row, 0);
	for (auto &it : row.node->getNodes()) {
		if (it->getHtmlName() == "td" || it->getHtmlName() == "th") {
			auto rowIndex = row.index;
			auto colSpan = Cell_getColSpan(*it);
			auto rowSpan = Cell_getRowSpan(*it);

			cells.emplace_back(Cell{it, rowIndex, colIndex, rowSpan, colSpan});

			for (uint16_t i = 0; i < colSpan; ++i) {
				if (colIndex + i < colsCount) {
					for (uint16_t j = 0; j < rowSpan; ++j) {
						if (rowIndex + j < rowsCount) {
							rows[rowIndex + j].cells[colIndex + i] = &cells.back();
							cols[colIndex + i].cells[rowIndex + j] = &cells.back();
						}
					}
				}
			}

			colIndex = nextRowIndex(row, colIndex + colSpan);
		}
	}
}

uint32_t LayoutTable::makeCells(NodeId nodeId) {
	if (rows.empty()) {
		return 0;
	}

	auto colsAttr = layout->node.node->getAttribute("cols");
	if (!colsAttr.empty()) {
		colsAttr.readInteger().unwrap([&](int64_t colNum) {
			if (colNum > 0 && colNum <= 32) {
				for (size_t i = cols.size(); i < size_t(colNum); ++i) {
					cols.emplace_back(Col{uint16_t(cols.size()), maxOf<uint16_t>(), nullptr});
				}
			}
		});
	}

	size_t phantomColsCount = 0;

	size_t colsCount = cols.size();
	for (auto &it : rows) {
		size_t count = 0;
		for (auto &n : it.node->getNodes()) {
			if (n->getHtmlName() == "td" || n->getHtmlName() == "th") {
				count += Cell_getColSpan(*n);
			}
		}
		colsCount = max(count, colsCount);
	}

	if (colsCount != cols.size()) {
		auto extra = colsCount - cols.size();
		for (size_t i = 0; i < extra; ++i) {
			cols.emplace_back(Col{uint16_t(cols.size()), maxOf<uint16_t>(), nullptr});
			++phantomColsCount;
		}
	}

	if (colsCount == 0) {
		return 0;
	}

	if (phantomColsCount > 0) {
		phantomCols.reserve(phantomColsCount);
	}

	for (auto &it : cols) {
		if (!it.node) {
			phantomCols.emplace_back("col");
			phantomCols.back().setNodeId(nodeId);
			it.node = &phantomCols.back();
			++nodeId;
		}
	}

	size_t cellsCount = 0;

	for (auto &it : rows) {
		it.cells.resize(colsCount, nullptr);
		for (auto &iit : it.node->getNodes()) {
			if (iit->getHtmlName() == "td" || iit->getHtmlName() == "th") {
				++cellsCount;
			}
		}
	}

	for (auto &it : cols) { it.cells.resize(rows.size(), nullptr); }

	cells.reserve(cellsCount);
	for (auto &it : rows) { processRow(it, colsCount); }

	return phantomColsCount;
}

bool LayoutTable::allocateSpace() {
	float calcMinWidth = 0.0f;
	float calcMaxWidth = 0.0f;
	if (isnan(width)) {
		for (auto &col : cols) {
			if (!isnan(col.info->width)) {
				calcMinWidth += col.info->width;
				calcMaxWidth += col.info->width;
			} else {
				calcMinWidth += col.info->minWidth;
				calcMaxWidth += col.info->maxWidth;
			}
		}

		if (!isnan(minWidth)) {
			calcMinWidth = max(minWidth, calcMinWidth);
		}

		if (!isnan(maxWidth)) {
			calcMaxWidth = max(maxWidth, calcMaxWidth);
		}

		if (isnan(origMinWidth)) {
			origMinWidth = calcMinWidth;
		}

		if (isnan(origMaxWidth)) {
			origMaxWidth = calcMaxWidth;
		}

		if (calcMinWidth == calcMaxWidth || layout->pos.size.width == calcMinWidth) {
			width = calcMinWidth;
		} else if (layout->pos.size.width < calcMinWidth) {
			widthScale *= (layout->pos.size.width / (calcMinWidth * 1.1f));

			for (auto &col : cols) {
				col.info->width = nan();
				col.info->minWidth = nan();
				col.info->maxWidth = nan();

				for (auto &cell : col.cells) {
					cell->info->width = nan();
					cell->info->minWidth = nan();
					cell->info->maxWidth = nan();
				}
			}

			return false;
		} else if (layout->pos.size.width <= calcMaxWidth) {
			width = layout->pos.size.width;
		} else {
			width = calcMaxWidth;
		}
	} else {
		origMinWidth = origMaxWidth = calcMinWidth = calcMaxWidth = width;
	}

	// calc extraspace
	float maxExtraSpace = 0.0f;
	for (auto &col : cols) {
		if (isnan(col.info->width)) {
			maxExtraSpace += col.info->maxWidth - col.info->minWidth;
		}
	}

	// allocate column space
	const float extraSpace = width - calcMinWidth;
	float xPos = 0.0f;
	for (auto &col : cols) {
		if (isnan(col.info->width)) {
			col.xPos = xPos;
			auto space = (maxExtraSpace != 0.0f)
					? ((col.info->maxWidth - col.info->minWidth) / maxExtraSpace) * extraSpace
					: 0.0f;
			col.width = col.info->width = col.info->minWidth + space;
			xPos += col.width;
		}
	}

	// allocate cell space
	for (auto &cell : cells) {
		if (isnan(cell.info->width)) {
			cell.info->width = 0.0f;
			for (size_t i = cell.colIndex; i < cell.colIndex + cell.colSpan; ++i) {
				cell.info->width += cols[i].info->width;
			}
			// log::format("Col", "%d %d %f", cell.colIndex, cell.rowIndex, cell.info->width);
		}
	}

	return true;
}

static void Table_processTableChildsNodes(LayoutTable &table,
		const mem_pool::Vector<Node *> &nodes) {
	for (auto &it : nodes) {
		if (it->getHtmlName() == "caption") {
			table.caption = it;
		} else if (it->getHtmlName() == "colgroup") {
			for (auto &iit : it->getNodes()) {
				if (iit->getHtmlName() == "col") {
					table.addCol(it, *iit);
				}
			}
		} else if (it->getHtmlName() == "col") {
			table.addCol(nullptr, *it);
		} else if (it->getHtmlName() == "thead") {
			for (auto &iit : it->getNodes()) {
				if (iit->getHtmlName() == "tr") {
					table.addRow(it, *iit);
				}
			}
		} else if (it->getHtmlName() == "tfoot") {
			for (auto &iit : it->getNodes()) {
				if (iit->getHtmlName() == "tr") {
					table.addRow(it, *iit);
				}
			}
		} else if (it->getHtmlName() == "tbody") {
			for (auto &iit : it->getNodes()) {
				if (iit->getHtmlName() == "tr") {
					table.addRow(it, *iit);
				}
			}
		} else if (it->getHtmlName() == "tr") {
			table.addRow(nullptr, *it);
		}
	}
}

template <typename PushCallback, typename PopCallback, typename CompileCallback>
static void Table_processTableChildsStyle(const StyleInterface *iface, LayoutTable &table,
		const PushCallback &push, const PopCallback &pop, const CompileCallback &compile) {
	uint16_t tmpColGroup = maxOf<uint16_t>();
	for (auto &col : table.cols) {
		if (col.group != tmpColGroup) {
			if (tmpColGroup != maxOf<uint16_t>()) {
				pop();
			}
			if (col.group != maxOf<uint16_t>()) {
				push(table.colGroups[col.group].node);
				table.colGroups[col.group].style = compile(table.colGroups[col.group].node);
			}
			tmpColGroup = col.group;
		}

		if (col.node) {
			push(col.node);

			const StyleList *colStyle = compile(col.node);
			col.info = new (std::nothrow) LayoutTable::LayoutInfo;
			col.info->node = LayoutBlock::NodeInfo(col.node, colStyle, iface);

			pop();
		}
	}

	if (tmpColGroup) {
		pop();
	}

	uint16_t tmpRowGroup = maxOf<uint16_t>();
	const Node *tmpRow = nullptr;
	for (LayoutTable::Cell &cell : table.cells) {
		auto &row = table.rows[cell.rowIndex];
		if (row.group != maxOf<uint16_t>() && row.group != tmpRowGroup) {
			if (tmpRow) {
				pop();
				tmpRow = nullptr;
			}
			if (tmpRowGroup != maxOf<uint16_t>()) {
				pop();
			}
			auto &g = table.rowGroups[row.group];
			push(g.node);
			g.style = compile(g.node);
			tmpRowGroup = row.group;
		}

		if (tmpRow != row.node) {
			if (tmpRow) {
				pop();
			}
			tmpRow = row.node;
			push(row.node);
			row.style = compile(row.node);
		}

		push(cell.node);

		const StyleList *cellStyle = compile(cell.node);
		cell.info = new (std::nothrow) LayoutTable::LayoutInfo;
		cell.info->node = LayoutBlock::NodeInfo(cell.node, cellStyle, iface);

		pop(); // cell
	}

	if (tmpRow) {
		pop();
	}
	if (tmpRowGroup) {
		pop();
	}
}

template <typename Callback>
static void Table_prepareTableCell(const LayoutEngine *b, const LayoutTable &table,
		LayoutTable::Col &col, LayoutTable::Cell &cell, const MediaParameters &media,
		const Callback &cb) {
	cell.info->pos.padding.right = media.computeValueAuto(cell.info->node.block.paddingRight, 0.0f);
	cell.info->pos.padding.left = media.computeValueAuto(cell.info->node.block.paddingLeft, 0.0f);

	if (cell.info->node.block.width.isFixed()) {
		// fixed width - no need to calculate column width
		cell.info->minWidth = cell.info->maxWidth = cell.info->width =
				media.computeValueStrong(cell.info->node.block.width, 0.0f);
		return;
	} else if (!isnan(table.width) && cell.info->node.block.width.metric == Metric::Percent) {
		cell.info->minWidth = cell.info->maxWidth = cell.info->width =
				table.width * cell.info->node.block.width.value;
		return;
	} else {
		if (cell.info->node.block.minWidth.isFixed()) {
			cell.info->minWidth = media.computeValueStrong(cell.info->node.block.minWidth, 0.0f);
		} else if (!isnan(table.width)
				&& cell.info->node.block.minWidth.metric == Metric::Percent) {
			cell.info->minWidth = table.width * cell.info->node.block.minWidth.value;
		}

		if (cell.info->node.block.maxWidth.isFixed()) {
			cell.info->maxWidth = media.computeValueStrong(cell.info->node.block.maxWidth, 0.0f);
		} else if (!isnan(table.width)
				&& cell.info->node.block.maxWidth.metric == Metric::Percent) {
			cell.info->maxWidth = table.width * cell.info->node.block.maxWidth.value;
		}

		cb(cell);
	}
}

static void Table_processTableColSpan(LayoutTable &table,
		const mem_pool::Vector<LayoutTable::Cell *> &vec) {
	for (auto &cell : vec) {
		float minCellWidth = 0.0f;
		float maxCellWidth = 0.0f;
		float fixedWidth = 0.0f;

		struct ColInfo {
			LayoutTable::Col *col;
			float colMin;
			float colMax;
			bool fixed;
			size_t idx;
			float minWeight = 0.0f;
			float maxWeight = 0.0f;
		};

		bool allFixed = true;
		mem_pool::Vector<ColInfo> cols;
		cols.reserve(cell->colSpan);

		for (size_t i = 0; i < cell->colSpan; ++i) {
			auto minW = table.cols[cell->colIndex + i].info->minWidth;
			auto maxW = table.cols[cell->colIndex + i].info->maxWidth;

			if (!isnan(table.cols[cell->colIndex + i].info->width)) {
				float f = table.cols[cell->colIndex + i].info->width;
				fixedWidth += f;
				cols.emplace_back(
						ColInfo{&table.cols[cell->colIndex + i], f, f, true, cell->colIndex + i});
			} else {
				cols.emplace_back(ColInfo{&table.cols[cell->colIndex + i], minW, maxW, false,
					cell->colIndex + i});
				allFixed = false;
			}

			if (!isnan(minW) && !isnan(minCellWidth)) {
				minCellWidth += minW;
			} else {
				minCellWidth = nan();
			}
			if (!isnan(maxW) && !isnan(maxCellWidth)) {
				maxCellWidth += maxW;
			} else {
				maxCellWidth = nan();
			}
		}

		if (allFixed) {
			// all fixed
			cell->info->maxWidth = cell->info->minWidth = cell->info->width = fixedWidth;
			continue;
		}

		if (!isnan(minCellWidth)) {
			if (minCellWidth < cell->info->minWidth) {
				float spaceToAllocate = cell->info->minWidth;
				spaceToAllocate -= fixedWidth;
				minCellWidth -= fixedWidth;
				for (auto &it : cols) {
					if (!it.fixed) {
						it.col->info->minWidth = (it.colMin / minCellWidth) * spaceToAllocate;
					}
				}
			}
		}

		if (!isnan(maxCellWidth)) {
			if (maxCellWidth < cell->info->maxWidth) {
				float spaceToAllocate = cell->info->maxWidth;
				spaceToAllocate -= fixedWidth;
				maxCellWidth -= fixedWidth;
				for (auto &it : cols) {
					if (!it.fixed) {
						it.col->info->maxWidth = (it.colMax / maxCellWidth) * spaceToAllocate;
					}
				}
			}
		}

		if (isnan(minCellWidth) || isnan(maxCellWidth)) {
			float minW = 0.0f;
			size_t minWeightCount = 0;
			float maxW = 0.0f;
			size_t maxWeightCount = 0;
			for (auto &it : cols) {
				if (!isnan(it.colMin)) {
					minW = std::max(minW, it.colMin);
					++minWeightCount;
				}
				if (!isnan(it.colMax)) {
					maxW = std::max(maxW, it.colMax);
					++maxWeightCount;
				}
			}

			float minWeightSum = 0.0f;
			float maxWeightSum = 0.0f;
			for (auto &it : cols) {
				if (!isnan(it.colMin)) {
					it.minWeight = (it.colMin / minW);
					minWeightSum += it.minWeight;
				}
				if (!isnan(it.colMax)) {
					it.maxWeight = (it.colMax / maxW);
					maxWeightSum += it.maxWeight;
				}
			}

			const float minMod = (minWeightCount > 0)
					? (minWeightSum / float(minWeightCount)) / float(cell->colSpan)
					: 1.0f;
			const float maxMod = (maxWeightCount > 0)
					? (maxWeightSum / float(maxWeightCount)) / float(cell->colSpan)
					: 1.0f;
			for (auto &it : cols) {
				if (!isnan(it.colMin)) {
					it.minWeight *= minMod;
				}
				if (!isnan(it.colMax)) {
					it.maxWeight *= maxMod;
				}
			}

			minWeightSum *= minMod;
			maxWeightSum *= maxMod;

			if (isnan(minCellWidth)) {
				// has undefined min-width
				float weightToAllocate = (1.0f - minWeightSum) / (cell->colSpan - minWeightCount);
				for (auto &it : cols) {
					const float w = isnan(it.colMin) ? cell->info->minWidth * weightToAllocate
													 : cell->info->minWidth * it.minWeight;
					if (isnan(it.col->info->minWidth) || it.col->info->minWidth < w) {
						it.col->info->minWidth = w;
					}
				}
			}

			if (isnan(maxCellWidth)) {
				// has undefined max-width
				float weightToAllocate = (1.0f - maxWeightSum) / (cell->colSpan - maxWeightCount);
				for (auto &it : cols) {
					const float w = isnan(it.colMax) ? cell->info->maxWidth * weightToAllocate
													 : cell->info->maxWidth * it.maxWeight;
					if (isnan(it.col->info->maxWidth) || it.col->info->maxWidth < w) {
						it.col->info->maxWidth = w;
					}
				}
			}
		}
	}
}

void LayoutTable::processTableChilds() {
	// build and fill grid
	Table_processTableChildsNodes(*this, layout->node.node->getNodes());

	auto &_media = layout->engine->getMedia();
	layout->engine->incrementNodeId(makeCells(layout->engine->getMaxNodeId()));

	// compile styles
	Table_processTableChildsStyle(layout->engine, *this,
			[this](const Node *node) { layout->engine->pushNode(node); }, [this] {
		layout->engine->popNode();
	}, [this](const Node *node) { return layout->engine->compileStyle(*node); });

	if (layout->node.block.width.isFixed()) {
		width = layout->pos.size.width;
	}

	auto calcSizes = [&](float scale) {
		auto media = _media;
		media.fontScale *= scale;

		if (isnan(width)) {
			if (layout->node.block.minWidth.isFixed()) {
				minWidth = media.computeValueStrong(layout->node.block.minWidth, 0.0f);
			}
			if (layout->node.block.maxWidth.isFixed()) {
				maxWidth = media.computeValueStrong(layout->node.block.maxWidth, 0.0f);
			}
		}

		Vector<Cell *> colSpans;
		// calc column sizes
		for (auto &col : cols) {
			if (col.info->node.node) {
				if (col.info->node.block.width.isFixed()) {
					// fixed width - no need to calculate column width
					col.info->maxWidth = col.info->minWidth = col.info->width =
							media.computeValueStrong(col.info->node.block.width, 0.0f);
					continue;
				} else if (!isnan(width) && col.info->node.block.width.metric == Metric::Percent) {
					col.info->maxWidth = col.info->minWidth = col.info->width =
							width * col.info->node.block.width.value;
				}

				if (col.info->node.block.minWidth.isFixed()) {
					col.info->minWidth =
							media.computeValueStrong(col.info->node.block.minWidth, 0.0f);
				} else if (!isnan(width)
						&& col.info->node.block.minWidth.metric == Metric::Percent) {
					col.info->minWidth = width * col.info->node.block.minWidth.value;
				}

				if (col.info->node.block.maxWidth.isFixed()) {
					col.info->maxWidth =
							media.computeValueStrong(col.info->node.block.maxWidth, 0.0f);
				} else if (!isnan(width)
						&& col.info->node.block.maxWidth.metric == Metric::Percent) {
					col.info->maxWidth = width * col.info->node.block.maxWidth.value;
				}
			}

			bool fixed = false;

			// check if some cell has fixed width
			for (auto &cell : col.cells) {
				if (!cell || !cell->node) {
					continue;
				}
				Table_prepareTableCell(layout->engine, *this, col, *cell, media,
						[&](LayoutTable::Cell &cell) {
					auto &row = rows[cell.rowIndex];
					layout->engine->pushNode(rowGroups[row.group].node);
					layout->engine->pushNode(row.node);
					layout->engine->pushNode(cell.node);

					auto minW = ceilf(LayoutBlock::requestWidth(layout->engine, cell.info->node,
										LayoutBlock::ContentRequest::Minimize, media))
							+ 1.0f;
					auto maxW = ceilf(LayoutBlock::requestWidth(layout->engine, cell.info->node,
										LayoutBlock::ContentRequest::Maximize, media))
							+ 1.0f;

					auto minWidth = minW + cell.info->pos.padding.horizontal();
					auto maxWidth = maxW + cell.info->pos.padding.horizontal();

					//log::format("Table", "min: %f (%f + %f) max: %f (%f + %f)",
					//		minWidth, minW, cell.info->pos.padding.horizontal(),
					//		maxWidth, maxW, cell.info->pos.padding.horizontal());

					if (isnan(cell.info->minWidth) || minWidth > cell.info->minWidth) {
						cell.info->minWidth = minWidth;
					}

					if (isnan(cell.info->maxWidth) || maxWidth > cell.info->maxWidth) {
						cell.info->maxWidth = maxWidth;
					}

					layout->engine->popNode();
					layout->engine->popNode();
					layout->engine->popNode();
				});
				if (cell->colSpan == 1) {
					if (!isnan(cell->info->width)) {
						col.info->maxWidth = col.info->minWidth = col.info->width =
								cell->info->width;
						fixed = true;
						break;
					} else {
						if (isnan(col.info->minWidth)
								|| col.info->minWidth < cell->info->minWidth) {
							col.info->minWidth = cell->info->minWidth;
						}
						if (isnan(col.info->maxWidth)
								|| col.info->maxWidth < cell->info->maxWidth) {
							col.info->maxWidth = cell->info->maxWidth;
						}
					}
				} else {
					colSpans.emplace_back(cell);
				}
			}

			if (fixed) {
				continue;
			}
		}

		if (!colSpans.empty()) {
			Table_processTableColSpan(*this, colSpans);
		}
	};

	size_t limit = 4;
	do {
		calcSizes(widthScale);
		--limit;
	} while (!allocateSpace() && limit > 0);
}

static void Table_procesRowCell(LayoutTable &table, LayoutTable::Row &row, LayoutTable::Cell &cell,
		LayoutBlock &newL, float xPos) {
	auto &parentPos = table.layout->pos.position;
	auto &_media = table.layout->engine->getMedia();
	float height = _media.computeValueStrong(newL.node.block.height, table.layout->pos.size.height);
	const float minHeight =
			_media.computeValueStrong(newL.node.block.minHeight, table.layout->pos.size.height);
	const float maxHeight =
			_media.computeValueStrong(newL.node.block.maxHeight, table.layout->pos.size.height);

	newL.pos.margin = Margin(0.0f);

	if (!std::isnan(height)) {
		if (!std::isnan(minHeight) && height < minHeight) {
			height = minHeight;
		}

		if (!std::isnan(maxHeight) && height > maxHeight) {
			height = maxHeight;
		}

		newL.pos.minHeight = nan();
		newL.pos.maxHeight = height;
	} else {
		newL.pos.minHeight = minHeight;
		newL.pos.maxHeight = maxHeight;
	}

	newL.pos.size = Size2(cell.info->width - cell.info->pos.padding.horizontal(), height);
	newL.pos.padding.top = _media.computeValueAuto(newL.node.block.paddingTop, newL.pos.size.width);
	newL.pos.padding.bottom =
			_media.computeValueAuto(newL.node.block.paddingBottom, newL.pos.size.width);

	if (isnan(table.layout->pos.position.y)) {
		newL.pos.position = Vec2(parentPos.x + xPos + newL.pos.padding.left, newL.pos.padding.top);
	} else {
		newL.pos.position = Vec2(parentPos.x + xPos + newL.pos.padding.left,
				parentPos.y + newL.pos.padding.top);
	}

	newL.node.context = Display::Block;

	table.layout->engine->processChilds(newL, *newL.node.node);

	newL.finalizeInlineContext();

	if (!std::isnan(newL.pos.maxHeight) && newL.pos.size.height > newL.pos.maxHeight) {
		newL.pos.size.height = newL.pos.maxHeight;
	}
}

static void Table_processTableRowSpan(LayoutTable &table, LayoutTable::Cell &cell) {
	float fullHeight = 0.0f;
	size_t undefinedCount = 0;
	mem_pool::Vector<float> heights;
	heights.reserve(cell.rowSpan);
	for (size_t i = 0; i < cell.rowSpan; ++i) {
		heights.emplace_back(table.rows[cell.rowIndex + i].height);
		fullHeight += table.rows[cell.rowIndex + i].height;
		if (heights.back() == 0) {
			++undefinedCount;
		}
	}

	if (fullHeight < cell.info->layout->getBoundingBox().size.height) {
		const auto diff = cell.info->layout->getBoundingBox().size.height - fullHeight;
		if (undefinedCount > 0) {
			// allocate space to undefined rows;
			for (size_t i = 0; i < cell.rowSpan; ++i) {
				if (table.rows[cell.rowIndex + i].height == 0.0f) {
					table.rows[cell.rowIndex + i].height = diff / undefinedCount;
				}
			}
		} else {
			// allocate to all rows;
			for (size_t i = 0; i < cell.rowSpan; ++i) {
				table.rows[cell.rowIndex + i].height += diff * (heights[i] / fullHeight);
			}
		}
	}
}

static void Table_finalizeRowCell(LayoutTable &table, LayoutBlock &newL, float yPos, float height) {
	float diff = height - newL.getBoundingBox().size.height;

	VerticalAlign a = VerticalAlign::Middle;
	auto vAlign = newL.node.style->get(ParameterName::CssVerticalAlign, newL.engine);
	if (!vAlign.empty()) {
		a = vAlign.front().value.verticalAlign;
	}

	switch (a) {
	case VerticalAlign::Baseline:
	case VerticalAlign::Middle:
		newL.pos.padding.top += diff / 2.0f;
		newL.pos.padding.bottom += diff / 2.0f;
		newL.pos.position.y += diff / 2.0f;
		break;
	case VerticalAlign::Super:
	case VerticalAlign::Top: newL.pos.padding.bottom += diff; break;
	case VerticalAlign::Sub:
	case VerticalAlign::Bottom:
		newL.pos.padding.top += diff;
		newL.pos.position.y += diff;
		break;
	}

	newL.pos.origin.y = roundf(newL.pos.position.y * newL.engine->getMedia().density);

	newL.processBackground(table.layout->pos.position.y);
	newL.processOutline(table.layout->node.block.borderCollapse == BorderCollapse::Separate);
	newL.cancelInlineContext();

	newL.updatePosition(yPos);

	// log::format("Builder", "%f %f", newL.pos.position.x, newL.pos.position.y);

	table.layout->layouts.emplace_back(&newL);
}

void LayoutTable::processTableLayouts() {
	if (caption) {
		captionLayout = layout->engine->makeLayout(caption, layout->node.context, true);
	}

	if (width < layout->pos.size.width) {
		if (layout->node.block.marginLeft.metric == Metric::Auto
				&& layout->node.block.marginRight.metric == Metric::Auto) {
			auto diff = layout->pos.size.width - width;
			layout->pos.margin.left += diff / 2.0f;
			layout->pos.margin.right += diff / 2.0f;
			layout->pos.position.x += diff / 2.0f;
		}
		layout->pos.size.width = width;
	}

	if (captionLayout && captionLayout->node.block.captionSide == CaptionSide::Top) {
		processCaption(captionLayout->node.block.captionSide);
	}

	bool hookMedia = false;
	if (widthScale != 1.0f) {
		auto newMedia = layout->engine->getMedia();
		newMedia.fontScale *= widthScale;
		layout->engine->hookMedia(newMedia);
		hookMedia = true;
	}

	Vector<LayoutTable::Cell *> rowSpans;
	for (auto &row : rows) {
		float xPos = 0.0f;

		// process layouts
		size_t colIdx = 0;
		for (auto &cell : row.cells) {
			if (!cell) {
				continue;
			}
			if (cell->rowIndex != row.index || cell->colIndex != colIdx) {
				++colIdx;
				continue;
			}

			auto newL = layout->engine->makeLayout(move(cell->info->node), move(cell->info->pos));
			newL->depth = layout->depth + 5; // colgroup + col + rowgroup + row
			cell->info->layout = newL;
			Table_procesRowCell(*this, row, *cell, *newL, xPos);

			xPos += cell->info->width;
			++colIdx;

			if (cell->rowSpan == 1) {
				row.height = std::max(row.height, newL->getBoundingBox().size.height);
			} else {
				rowSpans.emplace_back(cell);
			}
		}
	}

	for (auto &cell : rowSpans) { Table_processTableRowSpan(*this, *cell); }

	// vertical align
	float currentYPos = 0.0f;
	for (auto &row : rows) {
		size_t colIdx = 0;
		row.yPos = currentYPos;
		for (auto &cell : row.cells) {
			if (!cell) {
				continue;
			}
			if (cell->rowIndex != row.index || cell->colIndex != colIdx) {
				++colIdx;
				continue;
			}

			float yPos = row.yPos;
			float rowHeight = 0.0f;
			for (size_t i = 0; i < cell->rowSpan; ++i) {
				rowHeight += rows[cell->rowIndex + i].height;
			}

			Table_finalizeRowCell(*this, *cell->info->layout, yPos, rowHeight);
			++colIdx;
		}

		currentYPos += row.height;
	}

	layout->pos.size.height = currentYPos;

	float groupXPos = 0.0f;
	for (auto &it : colGroups) {
		it.xPos = groupXPos;
		float groupWidth = 0.0f;
		for (auto &col : it.cols) { groupWidth += cols[col].width; }
		it.width = groupWidth;
		groupXPos += groupWidth;
	}

	float groupYPos = 0.0f;
	for (auto &it : rowGroups) {
		it.yPos = groupYPos;
		float groupHeight = 0.0f;
		for (auto &row : it.rows) { groupHeight += rows[row].height; }
		it.height = groupHeight;
		groupYPos += groupHeight;
	}

	processTableBackground();

	if (hookMedia) {
		layout->engine->restoreMedia();
	}

	if (captionLayout && captionLayout->node.block.captionSide == CaptionSide::Bottom) {
		processCaption(captionLayout->node.block.captionSide);
	}
}

static void Table_Borders_fillExternalBorder(LayoutTable::Borders &b, const BorderParams &params) {
	b.solidBorder = true;
	for (size_t i = 0; i < b.numRows; ++i) {
		b.vertical.at(i * (b.numCols + 1)) = params;
		b.vertical.at((i + 1) * (b.numCols + 1) - 1) = params;
	}
	for (size_t i = 0; i < b.numCols; ++i) {
		b.horizontal.at(i) = params;
		b.horizontal.at(i + b.numCols * b.numRows) = params;
	}
}

static void Table_Borders_fillExternalBorder(LayoutTable::Borders &b, const BorderParams &left,
		const BorderParams &top, const BorderParams &right, const BorderParams &bottom) {
	b.solidBorder = true;
	for (size_t i = 0; i < b.numRows; ++i) {
		b.vertical.at(i * (b.numCols + 1)) = left;
		b.vertical.at((i + 1) * (b.numCols + 1) - 1) = right;
	}
	for (size_t i = 0; i < b.numCols; ++i) {
		b.horizontal.at(i) = top;
		b.horizontal.at(i + b.numCols * b.numRows) = bottom;
	}
}

static void Table_Borders_fillCellBorder(LayoutTable::Borders &b, const LayoutTable::Cell &cell) {
	auto &media = cell.info->layout->engine->getMedia();
	auto style = cell.info->layout->node.style->compileOutline(cell.info->layout->engine);

	if (style.top.style != BorderStyle::None) {
		auto top = BorderParams{style.top.style,
			media.computeValueAuto(style.top.width, b.size.width), style.top.color};
		if (top.isVisible()) {
			for (size_t i = 0; i < cell.colSpan; ++i) {
				if (b.horizontal.at(cell.rowIndex * b.numCols + cell.colIndex + i).merge(top)) {
					if (cell.rowIndex == 0) {
						b.solidBorder = false;
					}
				}
			}
		}
	}
	if (style.right.style != BorderStyle::None) {
		auto right = BorderParams{style.right.style,
			media.computeValueAuto(style.right.width, b.size.width), style.right.color};
		if (right.isVisible()) {
			for (size_t i = 0; i < cell.rowSpan; ++i) {
				if (b.vertical.at((cell.rowIndex + i) * (b.numCols + 1) + cell.colIndex
									  + cell.colSpan)
								.merge(right)) {
					if (cell.colIndex + cell.colSpan > b.numCols) {
						b.solidBorder = false;
					}
				}
			}
		}
	}
	if (style.bottom.style != BorderStyle::None) {
		auto bottom = BorderParams{style.bottom.style,
			media.computeValueAuto(style.bottom.width, b.size.width), style.bottom.color};
		if (bottom.isVisible()) {
			for (size_t i = 0; i < cell.colSpan; ++i) {
				if (b.horizontal.at((cell.rowIndex + cell.rowSpan) * b.numCols + cell.colIndex + i)
								.merge(bottom)) {
					if (cell.rowIndex + cell.rowSpan > b.numRows) {
						b.solidBorder = false;
					}
				}
			}
		}
	}
	if (style.left.style != BorderStyle::None) {
		auto left = BorderParams{style.left.style,
			media.computeValueAuto(style.left.width, b.size.width), style.left.color};
		if (left.isVisible()) {
			for (size_t i = 0; i < cell.rowSpan; ++i) {
				if (b.vertical.at((cell.rowIndex + i) * (b.numCols + 1) + cell.colIndex)
								.merge(left)) {
					if (cell.colIndex == 0) {
						b.solidBorder = false;
					}
				}
			}
		}
	}
}

LayoutTable::Borders::Borders(LayoutTable &table) {
	numRows = table.rows.size();
	numCols = table.cols.size();

	horizontal.resize(numCols * (numRows + 1));
	vertical.resize((numCols + 1) * numRows);

	size = table.layout->pos.size;
	auto &media = table.layout->engine->getMedia();
	auto style = table.layout->node.style->compileOutline(table.layout->engine);

	BorderParams left, top, right, bottom;
	if (style.top.style != BorderStyle::None) {
		top = BorderParams{style.top.style, media.computeValueAuto(style.top.width, size.width),
			style.top.color};
	}
	if (style.right.style != BorderStyle::None) {
		right = BorderParams{style.right.style,
			media.computeValueAuto(style.right.width, size.width), style.right.color};
	}
	if (style.bottom.style != BorderStyle::None) {
		bottom = BorderParams{style.bottom.style,
			media.computeValueAuto(style.bottom.width, size.width), style.bottom.color};
	}
	if (style.left.style != BorderStyle::None) {
		left = BorderParams{style.left.style, media.computeValueAuto(style.left.width, size.width),
			style.left.color};
	}

	if (left.compare(right) && top.compare(bottom) && left.compare(top)) {
		Table_Borders_fillExternalBorder(*this, left);
	} else {
		Table_Borders_fillExternalBorder(*this, left, top, right, bottom);
	}

	for (auto &it : table.cells) { Table_Borders_fillCellBorder(*this, it); }
}

static float Border_getLeftHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col > 0) {
		auto b = border.getHorz(row, col - 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getRightHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col < border.numCols - 1) {
		auto b = border.getHorz(row, col + 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getLeftTopVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row > 0) {
		auto b = border.getVert(row - 1, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getRightTopVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row > 0) {
		auto b = border.getVert(row - 1, col + 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getLeftBottomVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row < border.numRows) {
		auto b = border.getVert(row, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getRightBottomVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row < border.numRows) {
		auto b = border.getVert(row, col + 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getTopVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row > 0) {
		auto b = border.getVert(row - 1, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getBottomVert(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (row < border.numRows - 1) {
		auto b = border.getVert(row + 1, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getTopLeftHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col > 0) {
		auto b = border.getHorz(row, col - 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getBottomLeftHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col > 0) {
		auto b = border.getHorz(row + 1, col - 1);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getTopRightHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col < border.numCols) {
		auto b = border.getHorz(row, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}

static float Border_getBottomRightHorz(const LayoutTable::Borders &border, size_t row, size_t col) {
	if (col < border.numCols) {
		auto b = border.getHorz(row + 1, col);
		if (b.isVisible()) {
			return b.width;
		}
	}
	return 0.0f;
}


void LayoutTable::Borders::make(LayoutTable &table, LayoutResult *res) {
	auto pos = table.layout->pos.position;
	for (size_t i = 0; i < horizontal.size(); ++i) {
		size_t row = i / numCols;
		size_t col = i % numCols;

		Vec2 origin = pos;
		if (row != 0) {
			origin.y += table.rows[row - 1].yPos + table.rows[row - 1].height;
		}
		origin.x += table.cols[col].xPos;

		auto &border = horizontal[i];
		if (horizontal[i].isVisible()) {
			auto path = res->emplacePath(*table.layout, ZOrderBorder);
			path->depth = table.layout->depth + 5;
			path->drawHorizontalLineSegment(origin, table.cols[col].width, border.color,
					border.width, border.style, Border_getLeftBottomVert(*this, row, col),
					Border_getLeftHorz(*this, row, col), Border_getLeftTopVert(*this, row, col),
					Border_getRightTopVert(*this, row, col), Border_getRightHorz(*this, row, col),
					Border_getRightBottomVert(*this, row, col));
		}
	}

	for (size_t i = 0; i < vertical.size(); ++i) {
		size_t row = i / (numCols + 1);
		size_t col = i % (numCols + 1);

		Vec2 origin = pos;
		if (col != 0) {
			origin.x += table.cols[col - 1].xPos + table.cols[col - 1].width;
		}
		origin.y += table.rows[row].yPos;

		auto &border = vertical[i];
		if (vertical[i].isVisible()) {
			auto path = res->emplacePath(*table.layout, ZOrderBorder);
			path->depth = table.layout->depth + 5;
			path->drawVerticalLineSegment(origin, table.rows[row].height, border.color,
					border.width, border.style, Border_getTopLeftHorz(*this, row, col),
					Border_getTopVert(*this, row, col), Border_getTopRightHorz(*this, row, col),
					Border_getBottomRightHorz(*this, row, col),
					Border_getBottomVert(*this, row, col),
					Border_getBottomLeftHorz(*this, row, col));
		}
	}
}

BorderParams LayoutTable::Borders::getHorz(size_t row, size_t col) const {
	if (col < numCols && row <= numCols) {
		return horizontal[row * numCols + col];
	}
	return BorderParams();
}

BorderParams LayoutTable::Borders::getVert(size_t row, size_t col) const {
	if (col <= numCols && row < numCols) {
		return vertical[row * (numCols + 1) + col];
	}
	return BorderParams();
}

void LayoutTable::processTableBackground() {
	auto flushRect = [this](LayoutBlock &l, const Rect &rect, const Color4B &color) {
		if (rect.size.width > 0.0f && rect.size.height > 0.0f) {
			auto path = layout->engine->getResult()->emplacePath(*layout, ZOrderBackground);
			path->depth = layout->depth + 2;
			path->drawRect(rect, color);
			l.objects.emplace_back(path);
		}
	};

	for (auto &col : cols) {
		if (auto style = col.info->node.style) {
			auto bg = style->compileBackground(layout->engine);
			if (bg.backgroundColor.a != 0 && col.width > 0 && layout->pos.size.height > 0) {
				float yPos = 0.0f;
				float height = 0.0f;
				size_t rowIdx = 0;
				for (auto &cell : col.cells) {
					if (cell->rowIndex != rowIdx) {
						++rowIdx;
						continue;
					}
					auto &pos = cell->info->layout->pos;
					auto rect = pos.getInsideBoundingBox();
					if (cell->colIndex == col.index) {
						if (cell->colSpan == 1) {
							height += rect.size.height;
						} else {
							flushRect(*layout, Rect(col.xPos, yPos, col.width, height),
									bg.backgroundColor);
							flushRect(*cell->info->layout, rect, bg.backgroundColor);
							yPos = height + rect.size.height;
							height = 0.0f;
						}
					} else {
						flushRect(*layout, Rect(col.xPos, yPos, col.width, height),
								bg.backgroundColor);
						yPos = height + rect.size.height;
						height = 0.0f;
					}
					++rowIdx;
				}
				flushRect(*layout, Rect(col.xPos, yPos, col.width, height), bg.backgroundColor);
			}
		}
	}

	for (auto &row : rows) {
		if (auto style = row.style) {
			auto bg = style->compileBackground(layout->engine);
			if (bg.backgroundColor.a != 0 && row.height > 0 && layout->pos.size.width > 0) {
				float xPos = 0.0f;
				float bgWidth = 0.0f;
				size_t colIdx = 0;
				for (auto &cell : row.cells) {
					if (!cell) {
						continue;
					}
					if (cell->colIndex != colIdx) {
						++colIdx;
						continue;
					}
					auto &pos = cell->info->layout->pos;
					auto rect = pos.getInsideBoundingBox();
					if (cell->rowIndex == row.index) {
						if (cell->rowSpan == 1) {
							bgWidth += rect.size.width;
						} else {
							flushRect(*layout, Rect(xPos, row.yPos, bgWidth, row.height),
									bg.backgroundColor);
							flushRect(*cell->info->layout, rect, bg.backgroundColor);
							xPos = bgWidth + rect.size.width;
							bgWidth = 0.0f;
						}
					} else {
						flushRect(*layout, Rect(xPos, row.yPos, bgWidth, row.height),
								bg.backgroundColor);
						xPos = bgWidth + rect.size.width;
						bgWidth = 0.0f;
					}
					++colIdx;
				}
				flushRect(*layout, Rect(xPos, row.yPos, bgWidth, row.height), bg.backgroundColor);
			}
		}
	}

	if (layout->node.block.borderCollapse == BorderCollapse::Collapse) {
		Borders borders(*this);
		borders.make(*this, layout->engine->getResult());
	}

	if (widthScale < 1.0f) {
		String caption;
		if (captionLayout) {
			if (!captionLayout->objects.empty()) {
				for (auto &it : captionLayout->objects) {
					if (it->type == Object::Type::Label) {
						WideString str;
						str.reserve(it->asLabel()->layout.chars.size());
						it->asLabel()->layout.str(
								[&](char32_t ch) { unicode::utf16Encode(str, ch); });
						caption = string::toUtf8<Interface>(str);
					}
				}
			} else {
				captionLayout->node.node->foreach ([&](const Node &node, size_t level) {
					if (node.hasValue()) {
						caption += string::toUtf8<Interface>(node.getValue());
					}
				});
			}
		}

		auto href = caption.empty()
				? string::toString<Interface>("#", layout->node.node->getHtmlId(),
						  "?min=", size_t(ceilf(origMinWidth)),
						  "&max=", size_t(ceilf(origMaxWidth)))
				: string::toString<Interface>("#", layout->node.node->getHtmlId(),
						  "?min=", size_t(ceilf(origMinWidth)),
						  "&max=", size_t(ceilf(origMaxWidth)), "&caption=", caption);
		auto target = "table";
		auto res = layout->engine->getResult();
		if (!href.empty()) {
			layout->objects.emplace_back(res->emplaceLink(*layout,
					Rect(-layout->pos.padding.left, -layout->pos.padding.top,
							layout->pos.size.width + layout->pos.padding.left
									+ layout->pos.padding.right,
							layout->pos.size.height + layout->pos.padding.top
									+ layout->pos.padding.bottom),
					href, target, WideStringView()));
		}

		layout->processRef();
	}
}

void LayoutTable::processCaption(CaptionSide side) {
	layout->engine->pushNode(caption);
	Vec2 parentPos = layout->pos.position;
	if (side == CaptionSide::Bottom) {
		parentPos.y += layout->pos.size.height;
	}

	if (captionLayout->init(parentPos, layout->pos.size, 0.0f)) {
		captionLayout->node.context = Display::TableCaption;

		layout->engine->processChilds(*captionLayout, *captionLayout->node.node);
		captionLayout->finalize(parentPos, 0.0f);

		layout->layouts.emplace_back(captionLayout);

		float nodeHeight = captionLayout->getBoundingBox().size.height;
		if (side == CaptionSide::Top) {
			layout->pos.margin.top += nodeHeight;
			layout->pos.position.y += nodeHeight;
		} else {
			layout->pos.margin.bottom += nodeHeight;
		}
	}
	layout->engine->popNode();
}

} // namespace stappler::document
