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

#include "XLCommon.h"

#include "layout/TableLayout.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using namespace ui;

namespace {

// The three column templates the "Template" button cycles. Each exercises a different branch of
// track sizing: fr sharing, a fixed column beside flexible ones, and pure content sizing.
struct TemplateDef {
	StringView name;
	StringView tracks;
};

static const TemplateDef s_templates[] = {
	{StringView("1fr 2fr 1fr"), StringView("1fr 2fr 1fr")},
	{StringView("120px 1fr 1fr"), StringView("120px 1fr 1fr")},
	{StringView("auto auto auto"), StringView("auto auto auto")},
};

constexpr uint32_t TemplateCount = uint32_t(sizeof(s_templates) / sizeof(s_templates[0]));

// A cell: a coloured box with a centred caption. Same shape as the flex demo's makeBox, so the two
// tests read alike.
Rc<Layer> makeCell(const Color4F &color, StringView text) {
	auto layer = Rc<Layer>::create(color);

	auto label = layer->addChild(Rc<Label>::create(), ZOrder(1));
	label->setAnchorPoint(Anchor::Middle);
	label->setAlignment(font::TextAlign::Center);
	label->setFontSize(14);
	label->setString(text);
	label->setColor(Color::White);

	layer->setName(text);
	layer->setContentSizeDirtyCallback([box = layer.get(), label] {
		auto cs = box->getContentSize();
		label->setPosition(cs / 2.0f);
		label->setWidth(sprt::max(cs.width - 6.0f, 1.0f));
	});
	return layer;
}

TableBorderEdge edge(float width, const Color4B &color) {
	return TableBorderEdge{width, BorderStyle::Solid, color};
}

// Every cell declares all four edges, so the collapse pass has a conflict to resolve on every
// interior line rather than only where one side happens to be set.
void setCellBorders(Node *node, TableCellInfo &cfg, float width, const Color4B &color) {
	cfg.borderTop = cfg.borderRight = cfg.borderBottom = cfg.borderLeft = edge(width, color);
	LayoutSystem::setTableCell(node, cfg);
}

/* The second table on screen is built from CSS alone: no TableLayoutInfo, no TableCellInfo, no
LayoutSystem in code. Everything below has to travel `display: table` -> StyleResolver::applyLayout
-> the layout components, which is the path the widget kit and any application actually use. If the
hand-built table above works and this one does not, the bridge is what broke. */
static const StringView s_css = R"CSS(
.css-table {
	display: table;
	grid-template-columns: 100px 1fr 2fr;
	border-collapse: collapse;
	padding: 8px;
	border: 3px solid #000000;
	background-color: #bdbdbd;
}
.css-table > .row { display: table-row; height: 40px; }
.css-table .cell {
	border: 1px solid #ffffff;
	vertical-align: middle;
}
/* the span properties have no CSS-standard spelling: colspan/rowspan are HTML attributes, and
   attribute selectors never match in this engine */
.css-table .wide { -xl-column-span: 2; }
)CSS";

Value rectToValue(const Rect &r) {
	Value v;
	v.addDouble(r.origin.x);
	v.addDouble(r.origin.y);
	v.addDouble(r.size.width);
	v.addDouble(r.size.height);
	return v;
}

} // namespace

bool TableLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// --- control bar -------------------------------------------------------
	_controls = addChild(Rc<Layer>::create(Color::Grey_200), ZOrder(1));

	FlexLayoutInfo controlsInfo;
	controlsInfo.direction = FlexDirection::Row;
	controlsInfo.wrap = FlexWrap::NoWrap;
	controlsInfo.justifyContent = FlexJustify::FlexStart;
	controlsInfo.alignItems = FlexAlign::Stretch;
	controlsInfo.columnGap = 6.0f;
	controlsInfo.padding = Padding(6.0f);
	_controlsFlex = _controls->addSystem(Rc<LayoutSystem>::create(controlsInfo));

	addControlButton("Back", [this] { pop(); });
	_btnTemplate = addControlButton("Template", [this] { cycleTemplate(); });
	_btnAlgorithm = addControlButton("Algorithm", [this] { cycleAlgorithm(); });
	_btnCollapse = addControlButton("Borders", [this] { cycleCollapse(); });

	// --- the table ---------------------------------------------------------
	_table = addChild(Rc<Layer>::create(Color::Grey_400), ZOrder(0));
	_table->setName("table-demo");
	_tableSystem = _table->addSystem(Rc<LayoutSystem>::create(TableLayoutInfo()));

	rebuildTable();

	// --- the same thing again, but declared in CSS -------------------------
	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_cssTable = addChild(Rc<Layer>::create(Color::Grey_400), ZOrder(0));
	_cssTable->setName("css-table");
	_cssTable->addStyleClass("css-table");

	static const StringView cellNames[][3] = {
		{StringView("ca"), StringView("cb"), StringView("cc")},
		{StringView("cd"), StringView("ce"), StringView()},
	};
	for (uint32_t r = 0; r < 2; ++r) {
		auto row = _cssTable->addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
		row->setName(toString("css-row-", r));
		row->addStyleClass("row");
		for (uint32_t c = 0; c < 3; ++c) {
			if (cellNames[r][c].empty()) {
				continue;
			}
			auto cell = row->addChild(makeCell(Color::Blue_300, cellNames[r][c]), ZOrder(1));
			cell->addStyleClass("cell");
			// row 1 cell 0 spans two columns, declared in the sheet
			if (r == 1 && c == 0) {
				cell->addStyleClass("wide");
			}
		}
	}
	_cssTable->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));

	updateControlLabels();
	return true;
}

void TableLayout::rebuildTable() {
	// Every cycle rebuilds from scratch; without this the rows accumulate on each press.
	_table->removeAllChildren();

	// The container parameters. `columnTracks` comes from parseGridTemplate - the same parser CSS
	// `grid-template-columns` goes through, which is the whole reason a table reuses that property.
	TableLayoutInfo info;
	info.columnTracks = parseGridTemplate(s_templates[_templateIndex].tracks);
	info.autoColumn = GridTrack{GridTrack::Auto, 0.0f};
	// qualified: this class is also called TableLayout, and inside it the unqualified name is the
	// class, not the CSS `table-layout` enum
	info.algorithm = _fixedAlgorithm ? document::TableLayout::Fixed : document::TableLayout::Auto;
	info.borderCollapse = _collapse ? BorderCollapse::Collapse : BorderCollapse::Separate;
	info.borderSpacingH = 6.0f;
	info.borderSpacingV = 4.0f;
	info.padding = Padding(16.0f);
	info.borderTop = info.borderRight = info.borderBottom = info.borderLeft =
			edge(4.0f, Color::Black.asColor4B());
	_tableSystem->setTableInfo(info);

	// Rows are separate nodes, each with its own LayoutSystem in TableRow mode. Nothing here tells
	// a row how wide its columns are - the container stamps that on every pass.
	auto addRow = [this](StringView name, float height) {
		auto row = _table->addChild(Rc<Layer>::create(Color::Grey_300), ZOrder(1));
		row->setName(name);
		row->addSystem(Rc<LayoutSystem>::create(LayoutMode::TableRow));
		TableRowInfo cfg;
		cfg.height = height;
		LayoutSystem::setTableRow(row, cfg);
		return row;
	};

	// The layout publishes the collapsed borders as geometry; this is what turns them into a draw.
	// z-order above the rows, because the lines sit on the cell boundaries.
	_table->addChild(Rc<TableBorderPainter>::create(), ZOrder(10));

	// Row 1: three plain cells, one per column.
	{
		auto row = addRow("row-0", 60.0f);
		static const Color4F colors[] = {Color::Red_400, Color::Blue_400, Color::Green_400};
		static const StringView names[] = {StringView("A"), StringView("B"), StringView("C")};
		for (uint32_t i = 0; i < 3; ++i) {
			auto cell = row->addChild(makeCell(colors[i], names[i]), ZOrder(1));
			TableCellInfo cfg;
			// A thick red left edge on B, so the B|A line has a genuine conflict to resolve: A's
			// right edge is thin, B's left edge is thick, and the thick one must win.
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
			if (i == 1) {
				cfg.borderLeft = edge(5.0f, Color::Red_500.asColor4B());
				LayoutSystem::setTableCell(cell, cfg);
			}
		}
	}

	// Row 2: a cell spanning two columns, then one plain cell. The span is what proves the column
	// cursor is per-row rather than per-cell-index.
	{
		auto row = addRow("row-1", 60.0f);
		{
			auto cell = row->addChild(makeCell(Color::Purple_400, "D span2"), ZOrder(1));
			TableCellInfo cfg;
			cfg.columnSpan = 2;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
		{
			auto cell = row->addChild(makeCell(Color::Teal_400, "E"), ZOrder(1));
			TableCellInfo cfg;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
	}

	// Row 3: a cell spanning two ROWS. It is a child of row 3 and simply overflows downward; row 4
	// skips its column through the occupiedColumns the container stamps.
	{
		auto row = addRow("row-2", 60.0f);
		{
			auto cell = row->addChild(makeCell(Color::Orange_400, "F rowspan2"), ZOrder(1));
			TableCellInfo cfg;
			cfg.rowSpan = 2;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
		{
			auto cell = row->addChild(makeCell(Color::Brown_400, "G"), ZOrder(1));
			TableCellInfo cfg;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
		{
			auto cell = row->addChild(makeCell(Color::Indigo_400, "H"), ZOrder(1));
			TableCellInfo cfg;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
	}

	// Row 4: only two cells, because column 0 is still owned by F.
	{
		auto row = addRow("row-3", 60.0f);
		{
			auto cell = row->addChild(makeCell(Color::Cyan_400, "I"), ZOrder(1));
			TableCellInfo cfg;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
		{
			auto cell = row->addChild(makeCell(Color::Lime_400, "J"), ZOrder(1));
			TableCellInfo cfg;
			setCellBorders(cell, cfg, 1.0f, Color::White.asColor4B());
		}
	}
}

void TableLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto cs = getContentSize();

	_controls->setAnchorPoint(Anchor::BottomLeft);
	_controls->setPosition(Vec2(0.0f, getWorkTop() - _controlsHeight));
	_controls->setContentSize(Size2(cs.width, _controlsHeight));

	// the code-built table takes the upper half of the work area, the CSS-built one the lower half
	const float area = sprt::max(getWorkTop() - _controlsHeight, 0.0f);
	const float half = area / 2.0f;

	_table->setAnchorPoint(Anchor::BottomLeft);
	_table->setPosition(Vec2(0.0f, half));
	_table->setContentSize(Size2(cs.width, half));

	_cssTable->setAnchorPoint(Anchor::BottomLeft);
	_cssTable->setPosition(Vec2(0.0f, 0.0f));
	_cssTable->setContentSize(Size2(cs.width, half));
}

ui::Button *TableLayout::addControlButton(StringView title, Function<void()> &&cb) {
	auto btn = _controls->addChild(makeButton(title, sp::move(cb)), ZOrder(1));

	FlexItemInfo item;
	item.basis = 110.0f;
	item.grow = 1.0f;
	item.shrink = 1.0f;
	LayoutSystem::setItem(btn, item);
	return btn;
}

void TableLayout::updateControlLabels() {
	_btnTemplate->setString(toString("Cols: ", s_templates[_templateIndex].name));
	_btnAlgorithm->setString(_fixedAlgorithm ? "Fixed" : "Auto");
	_btnCollapse->setString(_collapse ? "Collapse" : "Separate");
}

void TableLayout::cycleTemplate() {
	_templateIndex = (_templateIndex + 1) % TemplateCount;
	rebuildTable();
	updateControlLabels();
}

void TableLayout::cycleAlgorithm() {
	_fixedAlgorithm = !_fixedAlgorithm;
	rebuildTable();
	updateControlLabels();
}

void TableLayout::cycleCollapse() {
	_collapse = !_collapse;
	rebuildTable();
	updateControlLabels();
}

Value TableLayout::getLayoutState() const {
	Value result;
	result.setString(s_templates[_templateIndex].name, "template");
	result.setString(_fixedAlgorithm ? "fixed" : "auto", "algorithm");
	result.setString(_collapse ? "collapse" : "separate", "borderCollapse");

	// Read the columns back off the FIRST ROW rather than recomputing them: what the assertion
	// should see is what the rows were actually handed. A mismatch between the two is exactly the
	// bug this test exists to catch.
	Value columns;
	Value rows;
	for (auto &child : _table->getChildren()) {
		Value row;
		row.setString(child->getName(), "name");
		// The row's own layout system and its stamped columns: if the cells are unplaced, this says
		// whether the row never got a TableRow system or never got its geometry.
		if (auto sys = child->getSystemByType<LayoutSystem>()) {
			row.setInteger(int64_t(sys->getMode()), "mode");
		} else {
			row.setString("none", "mode");
		}
		row.setBool(child->getComponent<TableColumnsComponent>() != nullptr, "hasColumns");
		row.setValue(rectToValue(Rect(child->getPosition().x, child->getPosition().y,
							  child->getContentSize().width, child->getContentSize().height)),
				"box");

		Value cells;
		for (auto &cell : child->getChildren()) {
			Value c;
			c.setString(cell->getName(), "name");
			c.setValue(rectToValue(Rect(cell->getPosition().x, cell->getPosition().y,
							   cell->getContentSize().width, cell->getContentSize().height)),
					"box");
			cells.addValue(sp::move(c));
		}
		row.setValue(sp::move(cells), "cells");

		if (columns.empty()) {
			if (auto cols = child->getComponent<TableColumnsComponent>()) {
				for (auto &col : cols->columns) {
					Value c;
					c.addDouble(col.position);
					c.addDouble(col.width);
					columns.addValue(sp::move(c));
				}
			}
		}
		rows.addValue(sp::move(row));
	}
	result.setValue(sp::move(columns), "columns");
	result.setValue(sp::move(rows), "rows");

	// The collapsed borders as geometry - the whole point of the handoff: the layout publishes
	// rects, a consumer paints them, and a test can assert on them without a screenshot.
	Value borders;
	if (auto b = _table->getComponent<TableBordersComponent>()) {
		for (auto &r : b->rects) {
			Value v;
			v.setValue(rectToValue(r.rect), "rect");
			v.setString(toString(r.color.r, ",", r.color.g, ",", r.color.b, ",", r.color.a),
					"color");
			borders.addValue(sp::move(v));
		}
	}
	// count BEFORE the move; reading size() off a moved-from Value always answers 0
	result.setInteger(int64_t(borders.size()), "borderCount");
	result.setValue(sp::move(borders), "borders");

	// The CSS-declared table, reported the same way. Its columns come from the stylesheet's
	// `grid-template-columns: 100px 1fr 2fr`, so if the bridge works they resolve to
	// 100 / (rest / 3) / (2 * rest / 3) and the second row's first cell is twice as wide as
	// column 0.
	Value cssRows;
	Value cssColumns;
	for (auto &child : _cssTable->getChildren()) {
		// only real rows: the table stamps its columns on those, and on nothing else (the border
		// painter is a child too, and is out of the flow)
		if (!child->getComponent<TableColumnsComponent>()) {
			continue;
		}
		Value row;
		row.setString(child->getName(), "name");
		row.setValue(rectToValue(Rect(child->getPosition().x, child->getPosition().y,
							  child->getContentSize().width, child->getContentSize().height)),
				"box");
		Value cells;
		for (auto &cell : child->getChildren()) {
			Value c;
			c.setString(cell->getName(), "name");
			c.setValue(rectToValue(Rect(cell->getPosition().x, cell->getPosition().y,
							   cell->getContentSize().width, cell->getContentSize().height)),
					"box");
			cells.addValue(sp::move(c));
		}
		row.setValue(sp::move(cells), "cells");
		if (cssColumns.empty()) {
			if (auto cols = child->getComponent<TableColumnsComponent>()) {
				for (auto &col : cols->columns) {
					Value c;
					c.addDouble(col.position);
					c.addDouble(col.width);
					cssColumns.addValue(sp::move(c));
				}
			}
		}
		cssRows.addValue(sp::move(row));
	}
	result.setBool(_cssTable->getComponent<TableLayoutInfo>() != nullptr, "cssHasTableInfo");
	result.setValue(sp::move(cssColumns), "cssColumns");
	result.setValue(sp::move(cssRows), "cssRows");
	return result;
}

void TableLayout::registerCommands() {
	addCommand("template", "Cycle the column track template", [this](Value &&) -> Value {
		cycleTemplate();
		return getLayoutState();
	});
	addCommand("algorithm", "Toggle table-layout between auto and fixed",
			[this](Value &&) -> Value {
		cycleAlgorithm();
		return getLayoutState();
	});
	addCommand("borders", "Toggle border-collapse between collapse and separate",
			[this](Value &&) -> Value {
		cycleCollapse();
		return getLayoutState();
	});
	addCommand("state", "Report the resolved columns, row/cell boxes and collapsed border rects",
			[this](Value &&) -> Value { return getLayoutState(); });
}

} // namespace stappler::xenolith::app
