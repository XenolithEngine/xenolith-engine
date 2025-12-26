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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTLAYOUTTABLE_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTLAYOUTTABLE_H_

#include "SPDocLayoutBlock.h"
#include "SPDocNode.h"

namespace STAPPLER_VERSIONIZED stappler::document {

struct SP_PUBLIC LayoutTable : public InterfaceObject<memory::PoolInterface> {
	struct LayoutInfo : public memory::AllocPool {
		LayoutBlock::PositionInfo pos;
		LayoutBlock::NodeInfo node;

		float width = nan();
		float minWidth = nan();
		float maxWidth = nan();

		LayoutBlock *layout = nullptr;
	};

	struct ColGroup;
	struct RowGroup;
	struct Col;
	struct Row;
	struct Cell;

	struct Cell {
		const Node *node = nullptr;
		uint16_t rowIndex = 0;
		uint16_t colIndex = 0;
		uint16_t rowSpan = 1;
		uint16_t colSpan = 1;

		LayoutInfo *info;
	};

	struct Col {
		uint16_t index = 0;
		uint16_t group = 0;

		const Node *node = nullptr;

		float width = 0.0f;
		float xPos = 0.0f;

		LayoutInfo *info;
		Vector<Cell *> cells;
	};

	struct Row {
		uint16_t index = 0;
		uint16_t group = 0;

		const Node *node = nullptr;
		const StyleList *style = nullptr;

		float height = 0.0f;
		float yPos = 0.0f;

		Vector<Cell *> cells;
	};

	struct ColGroup {
		uint16_t index = 0;

		const Node *node = nullptr;
		const StyleList *style = nullptr;

		float width = 0.0f;
		float xPos = 0.0f;

		Vector<uint32_t> cols;
	};

	struct RowGroup {
		uint16_t index = 0;

		const Node *node = nullptr;
		const StyleList *style = nullptr;

		float height = 0.0f;
		float yPos = 0.0f;

		Vector<uint32_t> rows;
	};

	struct Borders {
		size_t numCols = 0;
		size_t numRows = 0;
		Vector<BorderParams> horizontal;
		Vector<BorderParams> vertical;
		bool solidBorder = false;
		Size2 size;

		Borders(LayoutTable &table);
		void make(LayoutTable &table, LayoutResult *res);

		BorderParams getHorz(size_t row, size_t col) const;
		BorderParams getVert(size_t row, size_t col) const;
	};

	Size2 parentSize;
	LayoutBlock *layout = nullptr;
	LayoutBlock *captionLayout = nullptr;
	float width = nan();
	float minWidth = nan();
	float maxWidth = nan();
	float origMinWidth = nan();
	float origMaxWidth = nan();
	float widthScale = 1.0f;

	Node phantomBody;
	Vector<Node> phantomCols;

	const Node *caption = nullptr;
	Vector<Cell> cells;
	Vector<Col> cols;
	Vector<Row> rows;
	Vector<ColGroup> colGroups;
	Vector<RowGroup> rowGroups;

	LayoutTable(LayoutBlock *, const Size2 &, NodeId nodeId);

	void addCol(const Node *group, const Node &node);
	void addRow(const Node *group, const Node &node);

	void processRow(const Row &row, size_t colsCount);
	uint32_t makeCells(NodeId);

	bool allocateSpace();

	void processTableLayouts();
	void processTableChilds();

	void processTableBackground();
	void processCaption(CaptionSide);
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTLAYOUTTABLE_H_ */
