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

#include "widgets/ListSelectionLayout.h"
#include "XLSelection.h"
#include "XLSelectionSystem.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float s_rowHeight = 24.0f;
static constexpr size_t s_tableRows = 30;
static constexpr float s_listHeight = 288.0f;

static constexpr auto s_css = StringView(R"css(
table-view {
	display: table;
	background-color: #202026;
	grid-template-columns: 1fr;
}
table-row.selected { background-color: #3a3a5c; }
table-cell > label { color: #e8e8e8; font-size: 14px; }
tree-view { background-color: #202026; }
tree-row { background-color: transparent; height: var(--tree-row-h); }
tree-row.selected { background-color: #3a3a5c; }
text-input {
	background-color: #292929;
	color: #e8e8e8;
	font-size: 14px;
	padding: 0 8px;
}
label { color: #e8e8e8; font-size: 14px; }
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

StringView getOpName(ui::ListSelectionOp op) {
	switch (op) {
	case ui::ListSelectionOp::Replace: return StringView("replace");
	case ui::ListSelectionOp::Toggle: return StringView("toggle");
	case ui::ListSelectionOp::Range: return StringView("range");
	case ui::ListSelectionOp::AddRange: return StringView("add-range");
	}
	return StringView();
}

} // namespace

bool ListSelectionLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto columns = [] {
		return Vector<ui::TableView::Column>{
			{String("name"), String(), String("col-name"), ui::GridTrack()},
		};
	};

	_table = addChild(Rc<ui::TableView>::create(), ZOrder(1));
	_table->setName("table");
	_table->setHeaderVisible(false);
	_table->setRowHeight(s_rowHeight);
	_table->setColumns(columns());
	_table->setSource(makeTableModel("row"));
	_table->setSelectionMode(ui::ListSelectionMode::Multiple);
	_table->setSelectCallback([this](size_t, const ui::TableView::Row &) {
		++_selects;
		_lastOp = getOpName(_table->getLastSelectionOp()).str<Interface>();
		_lastKeyboard = _table->isSelectingFromKeyboard();
		_lastRows.clear();
		for (auto it : _table->getSelectedRows()) { _lastRows.emplace_back(int64_t(it)); }
	});
	_table->setActivateCallback([this](size_t, const ui::TableView::Row &) { ++_activates; });
	_table->setSelectionOwned(true);

	_tree = addChild(Rc<ui::TreeView>::create(makeTreeModel()), ZOrder(2));
	_tree->setName("tree");
	_tree->setLabelKey("title");
	_tree->setRowHeight(s_rowHeight);
	_tree->setSelectionMode(ui::ListSelectionMode::Multiple);
	_tree->setSelectCallback([](size_t, const ui::TreeView::Row &) { });
	_tree->setSelectionOwned(true);

	_single = addChild(Rc<ui::TableView>::create(), ZOrder(3));
	_single->setName("single");
	_single->setHeaderVisible(false);
	_single->setRowHeight(s_rowHeight);
	_single->setColumns(columns());
	_single->setSource(makeTableModel("item"));
	_single->setSelectCallback([](size_t, const ui::TableView::Row &) { });
	_single->setSelectionOwned(true);

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(4));
	_field->setName("field");
	_field->setText("abcdef");
	_field->setCaretBlink(false);

	return true;
}

Rc<data::Model> ListSelectionLayout::makeTableModel(StringView prefix) const {
	auto model = Rc<data::Model>::create();
	for (size_t i = 0; i < s_tableRows; ++i) {
		data::Model::Value value;
		value.setString(toString(prefix, i), "name");
		model->emplaceItem(model->getRoot(), maxOf<size_t>(), sp::move(value));
	}
	return model;
}

Rc<data::Model> ListSelectionLayout::makeTreeModel() const {
	data::Model::Value root;
	root.setString("tree", "title");
	auto model = Rc<data::Model>::create(sp::move(root));
	for (auto name : {StringView("cat-a"), StringView("cat-b")}) {
		data::Model::Value own;
		own.setString(name, "title");
		auto node = model->emplaceCategory(model->getRoot(), maxOf<size_t>(), sp::move(own));
		for (size_t i = 0; i < 4; ++i) {
			data::Model::Value item;
			item.setString(toString(name, "-", i), "title");
			model->emplaceItem(node, maxOf<size_t>(), sp::move(item));
		}
	}
	return model;
}

void ListSelectionLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 20.0f;
	auto place = [&](Node *node, float x, float y, Size2 size) {
		if (node) {
			node->setAnchorPoint(Vec2(0.0f, 1.0f));
			node->setPosition(Vec2(x, y));
			node->setContentSize(size);
		}
	};
	place(_table, 48.0f, top, Size2(280.0f, s_listHeight));
	place(_tree, 360.0f, top, Size2(260.0f, s_listHeight));
	place(_single, 652.0f, top, Size2(260.0f, s_listHeight));
	place(_field, 48.0f, top - s_listHeight - 30.0f, Size2(220.0f, 30.0f));
}

template <typename View>
Value ListSelectionLayout::encodeView(View *view) const {
	Value ret;
	if (!view) {
		return ret;
	}

	ret.setString(view->getSelectionMode() == ui::ListSelectionMode::Multiple ? "multiple"
																				 : "single",
			"mode");
	ret.setInteger(int64_t(view->getRowCount()), "rowCount");
	const auto current = view->getSelectedRow();
	ret.setInteger(current == maxOf<size_t>() ? -1 : int64_t(current), "current");

	auto &selected = ret.emplace("selected");
	selected.setArray(Value::ArrayType());
	for (auto it : view->getSelectedRows()) { selected.addInteger(int64_t(it)); }

	// Where each row is on screen, for a pointer to press it
	auto &rows = ret.emplace("rows");
	rows.setArray(Value::ArrayType());
	for (size_t i = 0; i < view->getRowCount(); ++i) {
		Rect rect;
		Value row;
		row.setInteger(int64_t(i), "index");
		if (view->getRowRect(i, rect)) {
			const auto at = view->convertToWorldSpace(Vec2(rect.getMidX(), rect.getMidY()));
			row.setDouble(at.x, "x");
			row.setDouble(at.y, "y");
			row.setBool(rect.getMaxY() <= view->getContentSize().height && rect.getMinY() >= 0.0f,
					"visible");
		}
		rows.addValue(sp::move(row));
	}

	// What the row nodes on screen say about themselves: the class, and the scene's :selected
	auto &nodes = ret.emplace("nodes");
	nodes.setArray(Value::ArrayType());
	if (auto scroll = view->getScroll()) {
		for (auto &child : scroll->getRoot()->getChildren()) {
			if (auto row = dynamic_cast<typename View::RowNode *>(child.get())) {
				Value node;
				node.setInteger(int64_t(row->getRowIndex()), "index");
				node.setBool(row->hasStyleClass("selected"), "selectedClass");
				node.setBool(isNodeSelected(row), "selectedState");
				nodes.addValue(sp::move(node));
			}
		}
	}
	return ret;
}

Value ListSelectionLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeView(_table), "table");
	ret.setValue(encodeView(_tree), "tree");
	ret.setValue(encodeView(_single), "single");

	if (auto system = SelectionSystem::findForNode(const_cast<ListSelectionLayout *>(this))) {
		ret.setString(system->getOwnerNode() ? system->getOwnerNode()->getName() : StringView(),
				"owner");
		auto anchor = system->getAnchorNode();
		ret.setString(anchor ? anchor->getName() : StringView(), "anchor");
		ret.setString(anchor ? anchor->getType() : StringView(), "anchorType");
		ret.setInteger(int64_t(system->getItems().size()), "items");
	}

	ret.setInteger(int64_t(_selects), "selects");
	ret.setInteger(int64_t(_activates), "activates");
	ret.setString(_lastOp, "lastOp");
	ret.setBool(_lastKeyboard, "lastKeyboard");
	auto &last = ret.emplace("lastRows");
	last.setArray(Value::ArrayType());
	for (auto it : _lastRows) { last.addInteger(it); }

	if (_field) {
		ret.setString(_field->getText(), "fieldText");
		ret.setInteger(int64_t(_field->getCursor().start), "fieldCursor");
		ret.setInteger(int64_t(_field->getCursor().length), "fieldSelection");
		ret.setBool(_field->isFocused(), "fieldFocused");
	}
	return ret;
}

void ListSelectionLayout::registerCommands() {
	addCommand("state", "Report the three lists, the scene's selection and the callbacks",
			[this](Value &&) { return encodeState(); });

	addCommand("select", "Select {rows} of {view}: table, tree or single, {current} optional",
			[this](Value &&args) {
		const Value &a = args;
		Vector<size_t> rows;
		for (auto &it : a.getArray("rows")) { rows.emplace_back(size_t(it.getInteger())); }
		const auto current = a.hasValue("current") ? size_t(a.getInteger("current")) : maxOf<size_t>();
		const auto view = a.getString("view");
		if (view == "tree" && _tree) {
			_tree->setSelectedRows(rows, current);
		} else if (view == "single" && _single) {
			_single->setSelectedRows(rows, current);
		} else if (_table) {
			_table->setSelectedRows(rows, current);
		}
		return ackValue(true);
	});

	addCommand("expand", "Expand or collapse a row of the tree: {row, open}", [this](Value &&args) {
		const Value &a = args;
		if (!_tree) {
			return ackValue(false);
		}
		const auto row = size_t(a.getInteger("row"));
		return ackValue(a.getBool("open") ? _tree->expandRow(row) : _tree->collapseRow(row));
	});

	addCommand("focus-field", "Put the caret in the text field", [this](Value &&) {
		if (!_field) {
			return ackValue(false);
		}
		_field->focus();
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero the callback counters", [this](Value &&) {
		_selects = _activates = 0;
		_lastOp.clear();
		_lastRows.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
