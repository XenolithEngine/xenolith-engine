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

#include "widgets/TableViewLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float s_rowHeight = 28.0f;
static constexpr size_t s_rowCount = 40;

// The grip track is declared HERE, in the sheet, exactly as an application would declare it: the
// view fills that cell in but never invents the column, so the track list stays the author's.
static constexpr auto s_tableCss = StringView(R"css(
table-view {
	display: table;
	width: 360px;
	height: 280px;
	background-color: #202026;
	outline-width: 1px;
	outline-color: #3d3d3d;
	grid-template-columns: 24px 1fr 80px;
}
table-row.selected { background-color: #3a3a5c; }
table-cell > label { color: #e8e8e8; font-size: 14px; }
table-cell.xl-ui-table-drag-handle > icon { width: 16px; height: 16px; color: #9a9aa4; }
table-insertion-line { background-color: #fcb400; }
text-input {
	width: 220px;
	height: 30px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 4px;
	padding: 0 8px;
	color: #e8e8e8;
	font-size: 14px;
}
label { color: #e8e8e8; font-size: 14px; }
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool TableViewLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_tableCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_table = addChild(Rc<ui::TableView>::create(), ZOrder(1));
	_table->setName("fields");
	_table->setHeaderVisible(false);
	_table->setRowHeight(s_rowHeight);
	_table->setSelectionEnabled(true);
	_table->setColumns(Vector<ui::TableView::Column>{
		{ui::TableView::ReorderColumnKey.str<Interface>(), String(), String("col-grip"),
			ui::GridTrack()},
		{String("name"), String(), String("col-name"), ui::GridTrack()},
		{String("kind"), String(), String("col-kind"), ui::GridTrack()},
	});

	_table->setReorderCallback([this](size_t from, size_t to) { return applyMove(from, to); });

	rebuildModel();

	_neighbour = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setText("abcdef");
	_neighbour->setCaretBlink(false);

	return true;
}

void TableViewLayout::rebuildModel() {
	_model = Rc<data::Model>::create();
	_values.clear();

	auto root = _model->getRoot();
	for (uint32_t i = 0; i < s_rowCount; ++i) {
		auto name = toString("field", i);
		Value value;
		value.setString(name, "name");
		value.setString((i % 2) ? "int" : "float", "kind");
		_model->emplaceItem(root, maxOf<size_t>(), sp::move(value));
		_values.emplace_back(sp::move(name));
	}

	if (_table) {
		_table->setSource(_model);
	}
}

bool TableViewLayout::applyMove(size_t from, size_t to) {
	if (_refuse) {
		++_refusals;
		return false;
	}
	if (from >= _values.size() || to >= _values.size()) {
		return false;
	}

	// The model is the owner's, and moving it is the owner's answer to the view's question. The
	// index means the row's FINAL place, counted after it has been taken out of the old one - the
	// same convention data::Model::moveNode states.
	auto children = _model->getRoot()->getChildren();
	if (from < children.size()) {
		_model->moveNode(children.at(from).get(), _model->getRoot(), to);
	}

	auto value = sp::move(_values[from]);
	_values.erase(_values.begin() + from);
	_values.emplace(_values.begin() + to, sp::move(value));

	++_moves;
	_lastMove = toString(from, "->", to);
	return true;
}

void TableViewLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 20.0f;

	if (_table) {
		_table->setAnchorPoint(Vec2(0.0f, 1.0f));
		_table->setPosition(Vec2(48.0f, top));
		_table->setContentSize(Size2(360.0f, 280.0f));
	}
	if (_neighbour) {
		_neighbour->setAnchorPoint(Vec2(0.0f, 1.0f));
		_neighbour->setPosition(Vec2(460.0f, top));
		_neighbour->setContentSize(Size2(220.0f, 30.0f));
	}
}

Value TableViewLayout::encodeRect(const Rect &rect) const {
	Value ret;
	// Rounded to hundredths: the ORDER and the boundaries are what is asserted, and a raw float
	// would make an expected value depend on formatting.
	ret.setInteger(int64_t(std::lround(rect.origin.x * 100.0f)), "x");
	ret.setInteger(int64_t(std::lround(rect.origin.y * 100.0f)), "y");
	ret.setInteger(int64_t(std::lround(rect.size.width * 100.0f)), "w");
	ret.setInteger(int64_t(std::lround(rect.size.height * 100.0f)), "h");
	return ret;
}

Value TableViewLayout::encodeState() const {
	Value ret;

	if (_table) {
		ret.setInteger(int64_t(_table->getRowCount()), "rowCount");
		auto selected = _table->getSelectedRow();
		ret.setInteger(selected == maxOf<size_t>() ? -1 : int64_t(selected), "selected");
		ret.setBool(_table->isReorderEnabled(), "reorderEnabled");
		if (auto scroll = _table->getScroll()) {
			ret.setInteger(int64_t(std::lround(scroll->getScrollPosition() * 100.0f)), "scroll");
		}
	}

	ret.setInteger(int64_t(_moves), "moves");
	ret.setInteger(int64_t(_refusals), "refusals");
	ret.setString(_lastMove, "lastMove");
	ret.setBool(_refuse, "refusing");

	Value values;
	for (auto &it : _values) { values.addString(it); }
	ret.setValue(sp::move(values), "values");

	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}
	return ret;
}

void TableViewLayout::registerCommands() {
	addCommand("state", "Report the order, the selection and the move counters",
			[this](Value &&) { return encodeState(); });

	addCommand("reorder", "Move a row: {from, to}", [this](Value &&args) {
		const Value &a = args;
		return ackValue(_table
				&& _table->reorderRow(size_t(a.getInteger("from")), size_t(a.getInteger("to"))));
	});

	addCommand("select", "Select a row: {row}, or -1 to clear", [this](Value &&args) {
		if (!_table) {
			return ackValue(false);
		}
		auto row = static_cast<const Value &>(args).getInteger("row");
		_table->setSelectedRow(row < 0 ? maxOf<size_t>() : size_t(row));
		return ackValue(true);
	});

	addCommand("row-rect", "The rectangle of a row, built or not: {row}", [this](Value &&args) {
		Rect rect;
		if (!_table
				|| !_table->getRowRect(size_t(static_cast<const Value &>(args).getInteger("row")),
						rect)) {
			return ackValue(false);
		}
		auto ret = encodeRect(rect);
		ret.setBool(true, "ok");
		return ret;
	});

	addCommand("cell-rect", "The rectangle of a cell: {row, column}", [this](Value &&args) {
		const Value &a = args;
		Rect rect;
		if (!_table
				|| !_table->getCellRect(size_t(a.getInteger("row")), size_t(a.getInteger("column")),
						rect)) {
			return ackValue(false);
		}
		auto ret = encodeRect(rect);
		ret.setBool(true, "ok");
		return ret;
	});

	addCommand("index-at", "Which row is at a point of the table: {x, y}", [this](Value &&args) {
		const Value &a = args;
		Value ret;
		auto index = _table
				? _table->getRowIndexAt(Vec2(float(a.getInteger("x")), float(a.getInteger("y"))))
				: maxOf<size_t>();
		ret.setInteger(index == maxOf<size_t>() ? -1 : int64_t(index), "index");
		return ret;
	});

	addCommand("boundary-at", "Which boundary an insertion would snap to: {x, y}",
			[this](Value &&args) {
		const Value &a = args;
		Value ret;
		Rect rect;
		auto boundary = _table
				? _table->getRowBoundaryAt(Vec2(float(a.getInteger("x")), float(a.getInteger("y"))),
						  &rect)
				: maxOf<size_t>();
		ret.setInteger(boundary == maxOf<size_t>() ? -1 : int64_t(boundary), "boundary");
		if (boundary != maxOf<size_t>()) {
			ret.setValue(encodeRect(rect), "rect");
		}
		return ret;
	});

	addCommand("scroll", "Scroll the table by {delta}", [this](Value &&args) {
		if (!_table || !_table->getScroll()) {
			return ackValue(false);
		}
		auto scroll = _table->getScroll();
		scroll->setScrollPosition(scroll->getScrollPosition()
				+ float(static_cast<const Value &>(args).getInteger("delta")));
		return ackValue(true);
	});

	addCommand("refuse", "Refuse every reorder, or stop: {value}", [this](Value &&args) {
		_refuse = static_cast<const Value &>(args).getBool("value");
		return ackValue(true);
	});

	addCommand("set-reorder", "Turn reordering on or off: {value}", [this](Value &&args) {
		if (!_table) {
			return ackValue(false);
		}
		_table->setReorderEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the table", [this](Value &&) {
		if (!_neighbour) {
			return ackValue(false);
		}
		_neighbour->focus();
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero the move counters", [this](Value &&) {
		_moves = _refusals = 0;
		_lastMove.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
