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

#include "widgets/MarqueeLayout.h"
#include "XLSelection.h"
#include "XLSelectionSystem.h"
#include "XLUiStyleResolver.h"
#include "XLDynamicStateSystem.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr float s_rowHeight = 24.0f;
static constexpr size_t s_tableRows = 60;
static constexpr float s_listHeight = 288.0f;
static constexpr float s_headerHeight = 32.0f;

static constexpr auto s_css = StringView(R"css(
table-view {
	display: table;
	background-color: transparent;
	grid-template-columns: 24px 1fr;
}
table-header { background-color: #2a2a30; }
table-row { background-color: #202026; }
table-row.selected { background-color: #3a3a5c; }
table-cell > label { color: #e8e8e8; font-size: 14px; }
table-cell.xl-ui-table-drag-handle > icon { width: 16px; height: 16px; color: #9a9aa4; }
tree-view { background-color: transparent; }
tree-row { background-color: #202026; height: var(--tree-row-h); }
tree-row.selected { background-color: #3a3a5c; }
marquee { background-color: rgba(0, 120, 255, 0.2); outline-color: #0078ff; outline-width: 1px; }
cover-panel { background-color: #505060; }
label { color: #e8e8e8; font-size: 14px; }
)css");

static const Color4F s_tile(0.25f, 0.25f, 0.28f, 1.0f);
static const Color4F s_tileLit(0.23f, 0.23f, 0.61f, 1.0f);

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

Value encodeRect(const Rect &rect) {
	Value ret;
	ret.setDouble(rect.origin.x, "x");
	ret.setDouble(rect.origin.y, "y");
	ret.setDouble(rect.size.width, "width");
	ret.setDouble(rect.size.height, "height");
	return ret;
}

Value encodeIndices(SpanView<size_t> indices) {
	Value ret;
	ret.setArray(Value::ArrayType());
	for (auto it : indices) { ret.addInteger(int64_t(it)); }
	return ret;
}

// The band of a system: where it is drawn, in the host's space and in the world, and its paint.
Value encodeBand(const ui::MarqueeSystem *marquee) {
	Value ret;
	if (!marquee) {
		return ret;
	}
	ret.setBool(marquee->isActive(), "active");
	ret.setString(getOpName(marquee->getEvent().op), "op");
	ret.setValue(encodeRect(marquee->getRect()), "rect");
	ret.setValue(encodeRect(marquee->getViewport()), "viewport");
	if (marquee->isActive()) {
		// Where the band last heard the pointer, so a reader can wait for its own move to land
		ret.setDouble(marquee->getEvent().location.x, "x");
		ret.setDouble(marquee->getEvent().location.y, "y");
	}

	const auto band = marquee->getBandRect();
	auto node = marquee->getBand();
	ret.setBool(node && node->isVisible() && band.size.width > 0.0f, "visible");
	ret.setValue(encodeRect(band), "band");
	if (auto owner = marquee->getOwner()) {
		const auto low = owner->convertToWorldSpace(band.origin);
		const auto high = owner->convertToWorldSpace(
				Vec2(band.getMaxX(), band.getMaxY()));
		ret.setValue(encodeRect(Rect(low.x, low.y, high.x - low.x, high.y - low.y)), "world");
	}
	if (node) {
		const auto color = node->getPathColor();
		Value paint;
		paint.addInteger(color.r);
		paint.addInteger(color.g);
		paint.addInteger(color.b);
		paint.addInteger(color.a);
		ret.setValue(sp::move(paint), "color");
	}
	return ret;
}

Value encodeScroll(const basic2d::ScrollViewBase *scroll) {
	Value ret;
	if (scroll) {
		ret.setDouble(scroll->getScrollPosition(), "pos");
		ret.setDouble(scroll->getScrollMinPosition(), "min");
		ret.setDouble(scroll->getScrollMaxPosition(), "max");
	}
	return ret;
}

} // namespace

bool MarqueeTileGrid::init() {
	if (!Node::init()) {
		return false;
	}

	addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll))->enableScissor();

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical), ZOrder(1));
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	_controller = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_controller);
	const auto rows = (Count + Columns - 1) / Columns;
	for (size_t row = 0; row < rows; ++row) {
		_controller->addItem([this, row](const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makeRow(row);
		}, TileHeight + Gap);
	}
	_controller->commitChanges();

	// `this` captured raw: the grid owns the system
	_marquee = addSystem(Rc<ui::MarqueeSystem>::create(ui::MarqueeSlots{
		[this](const ui::MarqueeEvent &ev) {
		_sweep = ui::ListSweep();
		_sweep.op = ev.op;
		_sweep.active = true;
		return true;
	},
		[this](const ui::MarqueeEvent &ev) {
		_sweep.hits.clear();
		_sweep.anchor = _sweep.current = maxOf<size_t>();
		float toOrigin = maxOf<float>();
		float toPoint = maxOf<float>();
		for (size_t i = 0; i < Count; ++i) {
			Rect rect;
			if (!getTileRect(i, rect) || !rect.intersectsRect(ev.rect)) {
				continue;
			}
			_sweep.hits.emplace_back(i);
			const auto mid = Vec2(rect.getMidX(), rect.getMidY());
			if (mid.distance(ev.origin) < toOrigin) {
				toOrigin = mid.distance(ev.origin);
				_sweep.anchor = i;
			}
			if (mid.distance(ev.point) < toPoint) {
				toPoint = mid.distance(ev.point);
				_sweep.current = i;
			}
		}
		syncTiles();
	},
		[this](const ui::MarqueeEvent &, bool commit) {
		auto sweep = sp::move(_sweep);
		_sweep = ui::ListSweep();
		if (commit) {
			ui::applyListSweep(_state, sweep.hits, sweep.op, Count, sweep.anchor, sweep.current);
			++_commits;
		}
		syncTiles();
	},
		nullptr,
	}, _scroll));
	return true;
}

void MarqueeTileGrid::handleContentSizeDirty() {
	Node::handleContentSizeDirty();
	if (_scroll) {
		_scroll->setContentSize(_contentSize);
	}
}

bool MarqueeTileGrid::getTileRect(size_t index, Rect &out) const {
	if (!_controller || index >= Count) {
		return false;
	}
	const auto &controller = *_controller;
	auto &items = controller.getItems();
	const auto row = index / Columns;
	const auto col = index % Columns;
	if (row >= items.size() || sprt::isnan(items[row].pos.y)) {
		return false;
	}

	// A row sits at the negated position with its top-left anchor; a tile is `Gap` above its bottom
	auto &item = items[row];
	const float left = sprt::isnan(item.pos.x) ? 0.0f : item.pos.x;
	const float bottom = -item.pos.y - item.size.height;
	out = Rect(left + float(col) * (TileWidth + Gap), bottom + Gap, TileWidth, TileHeight);
	return true;
}

void MarqueeTileGrid::foreachTile(const Callback<void(size_t, basic2d::Layer *)> &cb) const {
	if (!_controller) {
		return;
	}
	const auto &controller = *_controller;
	for (auto &item : controller.getItems()) {
		if (!item.node) {
			continue;
		}
		for (auto &child : item.node->getChildren()) {
			if (auto tile = dynamic_cast<basic2d::Layer *>(child.get())) {
				cb(size_t(tile->getTag()), tile);
			}
		}
	}
}

void MarqueeTileGrid::setSelected(SpanView<size_t> rows) {
	_state = ui::ListSelectionState();
	for (auto it : rows) {
		if (it < Count && !_state.contains(it)) {
			_state.rows.emplace_back(it);
			sprt::sort(_state.rows.begin(), _state.rows.end());
		}
	}
	syncTiles();
}

Rc<Node> MarqueeTileGrid::makeRow(size_t row) {
	auto node = Rc<Node>::create();
	for (uint32_t col = 0; col < Columns; ++col) {
		const size_t index = row * Columns + col;
		if (index >= Count) {
			break;
		}
		const bool lit = _sweep.isShown(index, _state.contains(index));
		auto tile = node->addChild(Rc<basic2d::Layer>::create(lit ? s_tileLit : s_tile),
				ZOrder(int16_t(col + 1)));
		tile->setTag(index);
		if (lit) {
			tile->addStyleClass("lit");
		}
		tile->setAnchorPoint(Anchor::BottomLeft);
		tile->setPosition(Vec2(float(col) * (TileWidth + Gap), Gap));
		tile->setContentSize(Size2(TileWidth, TileHeight));
	}
	return node;
}

void MarqueeTileGrid::syncTiles() {
	foreachTile([&](size_t index, basic2d::Layer *tile) {
		const bool lit = _sweep.isShown(index, _state.contains(index));
		tile->setColor(lit ? s_tileLit : s_tile, true);
		if (lit) {
			tile->addStyleClass("lit");
		} else {
			tile->removeStyleClass("lit");
		}
	});
}

bool MarqueeLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_tableModel = makeTableModel();
	_table = addChild(Rc<ui::TableView>::create(), ZOrder(1));
	_table->setName("table");
	_table->setRowHeight(s_rowHeight);
	_table->setHeaderHeight(s_headerHeight);
	_table->setColumns(Vector<ui::TableView::Column>{
		{ui::TableView::ReorderColumnKey.str<Interface>(), String(), String("col-grip"),
			ui::GridTrack()},
		{String("name"), String("Name"), String("col-name"), ui::GridTrack()},
	});
	_table->setSource(_tableModel);
	_table->setSelectionMode(ui::ListSelectionMode::Multiple);
	_table->setSelectCallback([this](size_t, const ui::TableView::Row &) { ++_selects; });
	_table->setActivateCallback([this](size_t, const ui::TableView::Row &) { ++_activates; });
	_table->setReorderCallback([this](size_t from, size_t to) { return applyMove(from, to); });
	_table->setMarqueeFilter([](size_t, const ui::TableView::Row &row) {
		return !row.getData().getBool("locked");
	});
	_table->setMarqueeCallback([this](ui::ListSelectionOp op) {
		++_sweeps;
		_lastSweepOp = getOpName(_table->getLastSelectionOp()).str<Interface>();
		if (op != _table->getLastSelectionOp()) {
			_lastSweepOp = String("mismatch");
		}
		_lastSweepRows.clear();
		for (auto it : _table->getSelectedRows()) { _lastSweepRows.emplace_back(int64_t(it)); }
	});
	_table->setMarqueeEnabled(true);
	_table->setSelectionOwned(true);

	_tree = addChild(Rc<ui::TreeView>::create(makeTreeModel()), ZOrder(2));
	_tree->setName("tree");
	_tree->setLabelKey("title");
	_tree->setRowHeight(s_rowHeight);
	_tree->setSelectionMode(ui::ListSelectionMode::Multiple);
	_tree->setSelectCallback([](size_t, const ui::TreeView::Row &) { });
	_tree->setMarqueeCallback([this](ui::ListSelectionOp) { ++_treeSweeps; });
	_tree->setMarqueeEnabled(true);
	_tree->setSelectionOwned(true);

	_grid = addChild(Rc<MarqueeTileGrid>::create(), ZOrder(3));
	_grid->setName("grid");

	// Drawn over a corner of the table, with a listener of its own
	auto cover = Rc<ui::Panel>::create();
	cover->setType("cover-panel");
	cover->removeStyleClass("xl-ui-panel");
	ui::Panel::registerStyleAppliers("cover-panel");
	cover->setName("cover");
	auto listener = cover->addSystem(Rc<InputListener>::create());
	listener->addTapRecognizer([this](const GestureTap &) {
		++_coverTaps;
		return true;
	});
	_cover = addChild(cover, ZOrder(5));

	return true;
}

Rc<data::Model> MarqueeLayout::makeTableModel() const {
	auto model = Rc<data::Model>::create();
	for (size_t i = 0; i < s_tableRows; ++i) {
		data::Model::Value value;
		value.setString(toString("row", i), "name");
		if (i == 4) {
			value.setBool(true, "locked");
		}
		model->emplaceItem(model->getRoot(), maxOf<size_t>(), sp::move(value));
	}
	return model;
}

Rc<data::Model> MarqueeLayout::makeTreeModel() const {
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

bool MarqueeLayout::applyMove(size_t from, size_t to) {
	auto children = _tableModel->getRoot()->getChildren();
	if (from >= children.size() || to >= children.size()) {
		return false;
	}
	_tableModel->moveNode(children.at(from).get(), _tableModel->getRoot(), to);
	++_reorders;
	return true;
}

void MarqueeLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 20.0f;
	auto place = [&](Node *node, float x, float y, Size2 size) {
		if (node) {
			node->setAnchorPoint(Vec2(0.0f, 1.0f));
			node->setPosition(Vec2(x, y));
			node->setContentSize(size);
		}
	};
	place(_table, 40.0f, top, Size2(300.0f, s_headerHeight + s_listHeight));
	place(_tree, 370.0f, top, Size2(240.0f, s_listHeight));
	place(_grid, 640.0f, top,
			Size2(float(MarqueeTileGrid::Columns) * (MarqueeTileGrid::TileWidth + MarqueeTileGrid::Gap),
					s_listHeight));
	place(_cover, 250.0f, top - s_headerHeight - s_listHeight + 50.0f, Size2(80.0f, 40.0f));
}

template <typename View>
Value MarqueeLayout::encodeView(View *view) const {
	Value ret;
	if (!view) {
		return ret;
	}

	ret.setString(view->getSelectionMode() == ui::ListSelectionMode::Multiple ? "multiple"
																				 : "single",
			"mode");
	ret.setBool(view->isMarqueeEnabled(), "enabled");
	ret.setBool(view->isMarqueeActive(), "active");
	ret.setInteger(int64_t(view->getRowCount()), "rowCount");
	const auto current = view->getSelectedRow();
	ret.setInteger(current == maxOf<size_t>() ? -1 : int64_t(current), "current");
	ret.setValue(encodeIndices(view->getSelectedRows()), "selected");
	ret.setValue(encodeIndices(view->getMarqueeHits()), "hits");
	ret.setValue(encodeBand(view->getMarquee()), "marquee");
	ret.setValue(encodeScroll(view->getScroll()), "scroll");

	// Where each row is, for a pointer to press it; `visible` means inside the scrolled viewport
	const auto viewport = view->getMarquee() ? view->getMarquee()->getViewport()
											 : Rect(Vec2::ZERO, view->getContentSize());
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
			row.setBool(rect.getMaxY() <= viewport.getMaxY() + 0.5f
							&& rect.getMinY() >= viewport.getMinY() - 0.5f,
					"visible");
		}
		rows.addValue(sp::move(row));
	}

	// The rows drawn as selected: the class, which a band lights before its release
	auto &shown = ret.emplace("shown");
	shown.setArray(Value::ArrayType());
	auto &states = ret.emplace("states");
	states.setArray(Value::ArrayType());
	if (auto scroll = view->getScroll()) {
		Vector<int64_t> lit;
		Vector<int64_t> marked;
		for (auto &child : scroll->getRoot()->getChildren()) {
			if (auto row = dynamic_cast<typename View::RowNode *>(child.get())) {
				if (row->hasStyleClass("selected")) {
					lit.emplace_back(int64_t(row->getRowIndex()));
				}
				if (isNodeSelected(row)) {
					marked.emplace_back(int64_t(row->getRowIndex()));
				}
			}
		}
		sprt::sort(lit.begin(), lit.end());
		sprt::sort(marked.begin(), marked.end());
		for (auto it : lit) { shown.addInteger(it); }
		for (auto it : marked) { states.addInteger(it); }
	}
	return ret;
}

Value MarqueeLayout::encodeGrid() const {
	Value ret;
	if (!_grid) {
		return ret;
	}
	auto &sweep = _grid->getSweep();
	ret.setBool(sweep.active, "active");
	ret.setValue(encodeIndices(sweep.hits), "hits");
	ret.setValue(encodeIndices(_grid->getState().rows), "selected");
	ret.setInteger(int64_t(_grid->getCommits()), "commits");
	ret.setValue(encodeBand(_grid->getMarquee()), "marquee");
	ret.setValue(encodeScroll(_grid->getScroll()), "scroll");

	Vector<int64_t> lit;
	_grid->foreachTile([&](size_t index, basic2d::Layer *tile) {
		if (tile->hasStyleClass("lit")) {
			lit.emplace_back(int64_t(index));
		}
	});
	sprt::sort(lit.begin(), lit.end());
	auto &shown = ret.emplace("shown");
	shown.setArray(Value::ArrayType());
	for (auto it : lit) { shown.addInteger(it); }

	auto &tiles = ret.emplace("tiles");
	tiles.setArray(Value::ArrayType());
	auto root = _grid->getScroll()->getRoot();
	const auto &size = _grid->getContentSize();
	for (size_t i = 0; i < MarqueeTileGrid::Count; ++i) {
		Rect rect;
		Value tile;
		tile.setInteger(int64_t(i), "index");
		if (_grid->getTileRect(i, rect)) {
			tile.setValue(encodeRect(rect), "content");
			const auto at = _grid->convertToNodeSpace(
					root->convertToWorldSpace(Vec2(rect.getMidX(), rect.getMidY())));
			const auto world = _grid->convertToWorldSpace(at);
			tile.setDouble(world.x, "x");
			tile.setDouble(world.y, "y");
			tile.setBool(at.y - rect.size.height / 2.0f >= 0.0f
							&& at.y + rect.size.height / 2.0f <= size.height,
					"visible");
		}
		tiles.addValue(sp::move(tile));
	}
	return ret;
}

Value MarqueeLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeView(_table), "table");
	ret.setValue(encodeView(_tree), "tree");
	ret.setValue(encodeGrid(), "grid");

	if (_table) {
		// The header's band in the world, and the grip column's middle
		if (auto header = _table->getHeader()) {
			const auto low = header->convertToWorldSpace(Vec2::ZERO);
			const auto high = header->convertToWorldSpace(
					Vec2(header->getContentSize().width, header->getContentSize().height));
			ret.setValue(encodeRect(Rect(low.x, low.y, high.x - low.x, high.y - low.y)), "header");
		}
		Rect cell;
		if (_table->getCellRect(0, 0, cell)) {
			ret.setDouble(_table->convertToWorldSpace(Vec2(cell.getMidX(), cell.getMidY())).x,
					"gripX");
		}
	}
	if (_cover) {
		const auto low = _cover->convertToWorldSpace(Vec2::ZERO);
		const auto high = _cover->convertToWorldSpace(
				Vec2(_cover->getContentSize().width, _cover->getContentSize().height));
		ret.setValue(encodeRect(Rect(low.x, low.y, high.x - low.x, high.y - low.y)), "cover");
	}

	if (auto system = SelectionSystem::findForNode(const_cast<MarqueeLayout *>(this))) {
		ret.setString(system->getOwnerNode() ? system->getOwnerNode()->getName() : StringView(),
				"owner");
		ret.setInteger(int64_t(system->getItems().size()), "items");
	}

	ret.setInteger(int64_t(_selects), "selects");
	ret.setInteger(int64_t(_sweeps), "sweeps");
	ret.setInteger(int64_t(_treeSweeps), "treeSweeps");
	ret.setInteger(int64_t(_reorders), "reorders");
	ret.setInteger(int64_t(_activates), "activates");
	ret.setInteger(int64_t(_coverTaps), "coverTaps");
	ret.setString(_lastSweepOp, "lastSweepOp");
	auto &last = ret.emplace("lastSweepRows");
	last.setArray(Value::ArrayType());
	for (auto it : _lastSweepRows) { last.addInteger(it); }
	return ret;
}

void MarqueeLayout::registerCommands() {
	addCommand("state", "Report the three lists, their bands and the callbacks",
			[this](Value &&) { return encodeState(); });

	addCommand("set", "Switch a band: {view: table|tree|grid, enabled?, mode?: single|multiple}",
			[this](Value &&args) {
		const Value &a = args;
		const auto view = a.getString("view");
		const auto mode = a.getString("mode") == "single" ? ui::ListSelectionMode::Single
														  : ui::ListSelectionMode::Multiple;
		if (view == "grid" && _grid) {
			if (a.hasValue("enabled")) {
				_grid->getMarquee()->setEnabled(a.getBool("enabled"));
			}
		} else if (view == "tree" && _tree) {
			if (a.hasValue("enabled")) {
				_tree->setMarqueeEnabled(a.getBool("enabled"));
			}
			if (a.hasValue("mode")) {
				_tree->setSelectionMode(mode);
			}
		} else if (_table) {
			if (a.hasValue("enabled")) {
				_table->setMarqueeEnabled(a.getBool("enabled"));
			}
			if (a.hasValue("mode")) {
				_table->setSelectionMode(mode);
			}
		}
		return ackValue(true);
	});

	addCommand("select", "Select {rows} of {view}: table, tree or grid, {current} optional",
			[this](Value &&args) {
		const Value &a = args;
		Vector<size_t> rows;
		for (auto &it : a.getArray("rows")) { rows.emplace_back(size_t(it.getInteger())); }
		const auto current =
				a.hasValue("current") ? size_t(a.getInteger("current")) : maxOf<size_t>();
		const auto view = a.getString("view");
		if (view == "grid" && _grid) {
			_grid->setSelected(rows);
		} else if (view == "tree" && _tree) {
			_tree->setSelectedRows(rows, current);
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

	addCommand("scroll", "Scroll {view}: table, tree or grid, to {position}", [this](Value &&args) {
		const Value &a = args;
		const auto view = a.getString("view");
		basic2d::ScrollViewBase *scroll = nullptr;
		if (view == "grid" && _grid) {
			scroll = _grid->getScroll();
		} else if (view == "tree" && _tree) {
			scroll = _tree->getScroll();
		} else if (_table) {
			scroll = _table->getScroll();
		}
		if (!scroll) {
			return ackValue(false);
		}
		scroll->setScrollPosition(float(a.getDouble("position")));
		return ackValue(true);
	});

	addCommand("insert", "Insert a row into the table's model {at}", [this](Value &&args) {
		const Value &a = args;
		data::Model::Value value;
		value.setString(toString("new", a.getInteger("at")), "name");
		_tableModel->emplaceItem(_tableModel->getRoot(), size_t(a.getInteger("at")),
				sp::move(value));
		return ackValue(true);
	});

	addCommand("reset", "Zero the counters", [this](Value &&) {
		_selects = _sweeps = _treeSweeps = _reorders = _activates = _coverTaps = 0;
		_lastSweepOp.clear();
		_lastSweepRows.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
