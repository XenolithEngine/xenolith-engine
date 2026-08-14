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

#include "XLUiTableView.h"
#include "XLUiStyleSystem.h"
#include "XLUiInteractiveComponent.h"
#include "XL2dLabel.h"
#include "XL2dIconSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static const Value s_nullValue;

TableView::~TableView() { }

bool TableView::init() { return init(nullptr); }

bool TableView::init(Source *source) {
	if (!Panel::init()) {
		return false;
	}

	setType("table-view");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-table-view");
	registerStyleAppliers("table-view");

	// This widget places its own children (a header and a scroll view); a stylesheet must not add a
	// second writer of their geometry. The TableLayoutInfo the sheet produces from `display: table`
	// is still read - it is parameters, not a layout system (see StyleResolver::applyLayout).
	setComponent<SystemManagedLayout>();

	_header = addChild(Rc<HeaderNode>::create(this), ZOrder(1));

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical), ZOrder(0));
	_scroll->setName("table-scroll");
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	_controller = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_controller);

	_sourceListener = addSystem(Rc<DataListener<Source>>::create(
			[this](SubscriptionFlags) { handleSourceDirty(); }, source));

	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[this](CallbackSystem *, const ComponentMask &) {
		if (_rebuildPending) {
			_rebuildPending = false;
			rebuildRows();
		}
	});

	// Content measurement, answered ONLY in auto-height mode: a table that scrolls inside a fixed
	// box has no intrinsic height to report, and returning false there lets the request fall
	// through to whatever else on this node can answer it.
	setMeasureCallback([this](const MeasureConstraints &c, Size2 &result) {
		if (!_autoHeight) {
			return false;
		}
		result.width = (c.maxWidth == maxOf<float>()) ? _contentSize.width : c.maxWidth;
		result.height = getIntrinsicHeight();
		return true;
	});

	if (source) {
		refresh();
	}
	return true;
}

void TableView::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	const float headerH = getHeaderHeight();

	// The header is a SIBLING of the scroll view, pinned to the top. That is the whole of "sticky":
	// it is not inside the scrolled content, so there is no scroll offset to compensate for.
	if (_header) {
		_header->setVisible(_headerVisible);
		_header->setAnchorPoint(Anchor::TopLeft);
		_header->setPosition(Vec2(0.0f, _contentSize.height));
		_header->setContentSize(Size2(_contentSize.width, headerH));
	}

	if (_scroll) {
		_scroll->setAnchorPoint(Anchor::BottomLeft);
		_scroll->setPosition(Vec2::ZERO);
		_scroll->setContentSize(
				Size2(_contentSize.width, sprt::max(_contentSize.height - headerH, 0.0f)));
	}

	resolveColumns();
}

void TableView::setSource(Source *source) {
	if (_sourceListener->getSubscription() == source) {
		return;
	}
	_sourceListener->setSubscription(source);
	refresh();
}

auto TableView::getSource() const -> Source * { return _sourceListener->getSubscription(); }

void TableView::setColumns(Vector<Column> &&columns) {
	if (_columns == columns) {
		return;
	}
	_columns = sp::move(columns);
	++_columnsRevision; // the set changed: every row node shows something different now
	resolveColumns();
	requestRebuildNodes(true);
	rebuildHeader();
}

void TableView::addColumn(Column &&column) {
	_columns.emplace_back(sp::move(column));
	++_columnsRevision;
	resolveColumns();
	requestRebuildNodes(true);
	rebuildHeader();
}

void TableView::clearColumns() {
	if (_columns.empty()) {
		return;
	}
	_columns.clear();
	++_columnsRevision;
	resolveColumns();
	requestRebuildNodes(true);
	rebuildHeader();
}

auto TableView::getRow(size_t index) const -> const Row * {
	return index < _rows.size() ? &_rows[index] : nullptr;
}

void TableView::setRowCallback(RowFunction &&cb) {
	_rowCallback = sp::move(cb);
	requestRebuildNodes(true);
}

void TableView::setCellCallback(CellFunction &&cb) {
	_cellCallback = sp::move(cb);
	requestRebuildNodes(true);
}

void TableView::setHeaderCellCallback(CellFunction &&cb) {
	_headerCellCallback = sp::move(cb);
	rebuildHeader();
}

void TableView::setRowHeightCallback(RowHeightFunction &&cb) {
	_rowHeightCallback = sp::move(cb);
	requestRebuildNodes(true);
	updateIntrinsicHeight();
}

void TableView::setRowHeight(float value) {
	if (_rowHeight == value) {
		return;
	}
	_rowHeight = value;
	requestRebuildNodes(true);
	updateIntrinsicHeight();
}

float TableView::getRowHeight(const Row &row) const {
	if (_rowHeightCallback) {
		const auto value = _rowHeightCallback(row);
		if (value > 0.0f) {
			return value;
		}
	}
	return _rowHeight;
}

void TableView::setHeaderVisible(bool value) {
	if (_headerVisible == value) {
		return;
	}
	_headerVisible = value;
	markContentSizeDirty();
	updateIntrinsicHeight();
}

void TableView::setHeaderHeight(float value) {
	if (_headerHeight == value) {
		return;
	}
	_headerHeight = value;
	markContentSizeDirty();
	updateIntrinsicHeight();
}

float TableView::getIntrinsicHeight() const {
	float height = getHeaderHeight();
	for (auto &it : _rows) {
		// getRowHeight(), not Row::height: the latter is only filled in by rebuildRows(), which is
		// deferred to the next visit - and the whole point of this is to answer BEFORE that.
		height += getRowHeight(it);
	}
	return height;
}

void TableView::setAutoHeight(bool value) {
	if (_autoHeight == value) {
		return;
	}
	_autoHeight = value;
	if (_scroll) {
		// Nested scrollers would otherwise both claim the swipe that scrolls whatever contains us.
		_scroll->setEnabled(!value);
	}
	markMeasureDirty();
	updateIntrinsicHeight();
}

void TableView::setIntrinsicHeightCallback(Function<void(float)> &&cb) {
	_intrinsicHeightCallback = sp::move(cb);
	// A fresh listener has been told nothing yet, so the current height is news to it.
	_reportedHeight = nan();
	updateIntrinsicHeight();
}

void TableView::updateIntrinsicHeight() {
	if (!_autoHeight) {
		return;
	}

	const auto height = getIntrinsicHeight();
	if (!sprt::isnan(_reportedHeight) && _reportedHeight == height) {
		return;
	}
	_reportedHeight = height;

	markMeasureDirty();
	if (_intrinsicHeightCallback) {
		_intrinsicHeightCallback(height);
	}
}

void TableView::setSelectCallback(RowEventFunction &&cb) {
	_selectCallback = sp::move(cb);
	setSelectionEnabled(true);
}

void TableView::setActivateCallback(RowEventFunction &&cb) {
	_activateCallback = sp::move(cb);
	setSelectionEnabled(true);
}

void TableView::setSelectionEnabled(bool value) {
	if (_selectionEnabled == value) {
		return;
	}
	_selectionEnabled = value;
	// whether a row gets an input listener at all is decided when its node is built
	requestRebuildNodes(true);
}

void TableView::setSelectedRow(size_t index) {
	if (_selectedRow == index) {
		return;
	}
	const auto prev = _selectedRow;
	_selectedRow = index;

	// Selection changes no row's SHAPE, so it never rebuilds: flip the class on the two nodes
	// involved, if they happen to be on screen.
	if (auto node = getRowNode(prev)) {
		updateRowNode(node, prev);
	}
	if (auto node = getRowNode(index)) {
		updateRowNode(node, index);
	}

	if (_selectCallback && index < _rows.size()) {
		_selectCallback(index, _rows[index]);
	}
}

// Forced, for the reason spelled out on TreeView::invalidateSource: a RowKey cannot see that a
// row's payload was replaced under the same id, so reusing its node would keep showing the old one.
// refresh() stays unforced - a resize goes through it and must not rebuild anything.
void TableView::invalidateSource() {
	refresh();
	requestRebuildNodes(true);
}

void TableView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it: a forced request coalesced into an already-pending
	// unforced one must still force, or the reuse pass would quietly ignore it.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
	// The components phase is opt-in per visit, so asking for the rebuild is also asking for the
	// phase that performs it.
	markComponentsDirty();
}

void TableView::handleSourceDirty() {
	refresh();
	requestRebuildNodes(true);
}

void TableView::refresh() {
	rebuildModel();
	// before any node exists, so a source that answers inline has every payload in place by the
	// time the first row is built and no placeholder frame is ever drawn
	requestRowData();
	requestRebuildNodes();
	// The row COUNT is what usually moves the intrinsic height, and it is settled by now. Reported
	// here rather than from rebuildRows(), which is deferred to the next visit: an owner sizing us
	// from getIntrinsicHeight() has to learn about it before that frame, not after.
	updateIntrinsicHeight();
}

void TableView::resolveColumns() {
	auto source = getSource();
	(void)source;

	const uint32_t count = uint32_t(_columns.size());
	if (count == 0 || _contentSize.width <= 0.0f) {
		return;
	}

	// The track list comes from CSS (`grid-template-columns` on this node, mapped to
	// TableLayoutInfo by the style resolver); Column::track fills in for columns the sheet does not
	// mention. Both are GridTracks, so the same sizing routine the table layout uses applies here -
	// which is the point: the widget and a static `display: table` cannot disagree about widths.
	auto info = getComponent<TableLayoutInfo>();

	Vector<GridTrack> tracks;
	tracks.resize(count);
	for (uint32_t i = 0; i < count; ++i) {
		tracks[i] =
				(info && i < info->columnTracks.size()) ? info->columnTracks[i] : _columns[i].track;
	}

	const bool collapse = info && info->borderCollapse == BorderCollapse::Collapse;
	const float spacingH = (info && !collapse) ? info->borderSpacingH : 0.0f;
	const float spacingV = (info && !collapse) ? info->borderSpacingV : 0.0f;
	const Padding padding = info ? info->padding : Padding();

	const float available = sprt::max(_contentSize.width - padding.horizontal()
					- spacingH * static_cast<float>(count > 0 ? count - 1 : 0),
			0.0f);

	TableColumnsComponent next;
	// the same track sizing the Table pass runs - see resolveTableColumns on why a virtualized
	// table shares it rather than reimplementing it
	resolveTableColumns(tracks, available, spacingH, padding.left, next);

	next.borderCollapse = info ? info->borderCollapse : BorderCollapse::Separate;
	next.borderSpacingH = spacingH;
	next.borderSpacingV = spacingV;
	next.justifyItems = info ? info->justifyItems : GridAlign::Stretch;
	next.alignItems = info ? info->alignItems : GridAlign::Stretch;
	next.occupiedColumns.resize(count, 0);

	// The generation is the writer's to advance, and only on a real change - a row keys its layout
	// off it, and the node reuse must not see a resize as a new column set.
	next.generation = _geometry.generation;
	TableColumnsComponent compare = next;
	compare.rowHeight = _geometry.rowHeight;
	compare.spanRowHeights = _geometry.spanRowHeights;
	if (compare == _geometry) {
		return;
	}
	next.generation = _geometry.generation + 1;
	_geometry = sp::move(next);

	restampColumns();
}

void TableView::restampColumns() {
	if (_geometry.columns.empty()) {
		return;
	}

	if (_header) {
		auto stamp = _geometry;
		stamp.rowHeight = getHeaderHeight();
		LayoutSystem::setTableColumns(_header, stamp);
	}

	if (!_controller) {
		return;
	}
	// the CONST overload: the non-const one marks the controller dirty, and this is a pure read
	const auto &controller = *_controller;
	for (auto &it : controller.getItems()) {
		if (!it.node) {
			continue;
		}
		auto stamp = _geometry;
		stamp.rowHeight = it.size.height;
		LayoutSystem::setTableColumns(it.node, stamp);
	}
}

void TableView::rebuildModel() {
	_rows.clear();

	auto source = getSource();
	if (!source) {
		return;
	}

	// Keep what has already been loaded, keyed by identity, so a rebuild does not re-request every
	// payload it already has.
	Map<SourceId, Value> loaded;
	for (auto &row : _rows) {
		if (row.dataLoaded) {
			loaded.emplace(row.itemId, sp::move(row.data));
		}
	}

	const auto count = source->getChildsCount();
	_rows.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		Row row;
		row.itemId = SourceId(i);
		auto it = loaded.find(row.itemId);
		if (it != loaded.end()) {
			row.data = sp::move(it->second);
			row.dataLoaded = true;
		}
		_rows.emplace_back(sp::move(row));
	}
}

void TableView::requestRowData() {
	auto source = getSource();
	if (!source || _rows.empty()) {
		return;
	}

	// Suppresses the redundant node rebuild a synchronous delivery would schedule from inside this
	// loop: refresh() schedules one for the whole pass anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		if (_rows[i].dataLoaded) {
			++i;
			continue;
		}

		// One request per run of consecutive unloaded rows - the cursor read that makes a table of
		// fifty thousand rows a handful of calls rather than fifty thousand.
		const auto first = _rows[i].itemId;
		size_t count = 1;
		while (i + count < _rows.size() && !_rows[i + count].dataLoaded
				&& _rows[i + count].itemId == first + SourceId(count)) {
			++count;
		}

		Rc<TableView> self(this);
		if (source->getSliceData(
					[self, first, count](Map<SourceId, Value> &data) {
			self->handleSliceData(first, count, data);
		}, first, count, 0, false)
				== 0) {
			// The source planned no request, so no callback is coming. Mark the range resolved
			// rather than re-ask for it on every rebuild from now on.
			for (size_t j = 0; j < count; ++j) { _rows[i + j].dataLoaded = true; }
		}

		i += count;
	}

	_inDataRequest = false;
}

void TableView::handleSliceData(SourceId first, size_t count, Map<SourceId, Value> &data) {
	bool updated = false;
	for (auto &row : _rows) {
		if (row.itemId < first || row.itemId >= first + SourceId(count)) {
			continue;
		}
		auto it = data.find(row.itemId);
		if (it != data.end()) {
			row.data = sp::move(it->second);
		}
		// Marked loaded even for an index the source did not answer for: "loaded but empty" has to
		// be terminal, or an under-delivering source would be asked again on every rebuild forever.
		row.dataLoaded = true;
		updated = true;
	}

	if (updated && !_inDataRequest) {
		requestRebuildNodes();
		// A row-height callback may key off the payload, so a late answer can resize the table.
		updateIntrinsicHeight();
	}
}

void TableView::makeTableRow(Node *node) {
	// A row lays its cells out with LayoutMode::TableRow, reading the geometry stamped on it. The
	// marker keeps the style resolver from adding a second layout system on top; the cells still
	// get their TableCellInfo from CSS, because the resolver's ITEM mapping keys off the parent
	// carrying a TableColumnsComponent and runs regardless of the marker.
	node->addSystem(Rc<LayoutSystem>::create(LayoutMode::TableRow));
	node->setComponent<SystemManagedLayout>();
	if (!_geometry.columns.empty()) {
		LayoutSystem::setTableColumns(node, _geometry);
	}
}

void TableView::rebuildHeader() {
	if (!_header) {
		return;
	}
	_header->removeAllChildren();
	buildCells(_header, nullptr, 0, true);
}

void TableView::rebuildRows() {
	if (!_controller) {
		return;
	}

	const auto force = _forceRebuild;
	_forceRebuild = false;

	if (!force) {
		for (auto &it : _controller->getItems()) {
			// a row the callback took over completely is not a RowNode and carries no key
			auto row = dynamic_cast<RowNode *>(it.node);
			if (!row) {
				continue;
			}
			_reusableRows.emplace_back(row);
			// Detached HERE rather than by clear(), and without the cleanup: removeFromParent()
			// defaults to stripping every system and component off the subtree, which for a node
			// about to be re-attached would leave a Sprite whose scissor system is a dangling
			// pointer. Nulling the item keeps clear() from doing it again.
			it.node->removeFromParent(false);
			it.node = nullptr;
			it.handle = nullptr;
		}
	}

	_controller->clear();

	for (size_t i = 0; i < _rows.size(); ++i) {
		// Resolved here, once, and remembered on the Row: the factory below runs only when the row
		// scrolls into view, and must publish to CSS the same number the controller laid out with.
		_rows[i].height = getRowHeight(_rows[i]);
		// `this` captured raw on purpose: this node owns _controller, which owns this factory - an
		// Rc back would be a cycle. The index is safe because every change to _rows rebuilds.
		_controller->addItem([this, i](const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makeRow(i);
		}, _rows[i].height);
	}

	_controller->commitChanges();
	_reusableRows.clear();
}

auto TableView::makeRowKey(const Row &row) const -> RowKey {
	RowKey key;
	key.itemId = row.itemId;
	key.columnsRevision = _columnsRevision;
	key.height = row.height;
	key.dataLoaded = row.dataLoaded;
	return key;
}

auto TableView::takeReusableRow(size_t index) -> Rc<RowNode> {
	if (index >= _rows.size()) {
		return nullptr;
	}
	const auto key = makeRowKey(_rows[index]);
	for (auto it = _reusableRows.begin(); it != _reusableRows.end(); ++it) {
		if ((*it)->getRowKey() == key) {
			auto ret = *it;
			_reusableRows.erase(it);
			return ret;
		}
	}
	return nullptr;
}

auto TableView::getRowNode(size_t index) const -> RowNode * {
	if (!_controller || index >= _rows.size()) {
		return nullptr;
	}
	// the CONST overload: the non-const getItems() marks the controller dirty on a pure read
	const auto &controller = *_controller;
	for (auto &it : controller.getItems()) {
		if (auto row = dynamic_cast<RowNode *>(it.node)) {
			if (row->getRowIndex() == index) {
				return row;
			}
		}
	}
	return nullptr;
}

void TableView::updateRowNode(RowNode *node, size_t index) {
	if (index == _selectedRow) {
		node->addStyleClass("selected");
	} else {
		node->removeStyleClass("selected");
	}
}

Rc<Node> TableView::makeRow(size_t index) {
	if (index >= _rows.size()) {
		return nullptr;
	}

	if (auto node = takeReusableRow(index)) {
		// the same row showing the same thing; only its index moved
		node->setRowIndex(index);
		updateRowNode(node, index);
		return node;
	}

	RowBuilder builder;
	builder._view = this;
	builder._row = &_rows[index];
	builder._index = index;

	if (_rowCallback) {
		_rowCallback(builder);
	}

	return buildRowNode(builder);
}

Rc<Node> TableView::buildRowNode(RowBuilder &builder) {
	const auto &row = *builder._row;
	const auto index = builder._index;

	Rc<Node> node;
	Rc<RowNode> rowNode;

	if (builder._node) {
		// the callback took the row over completely: no cells, no key, rebuilt every pass
		node = sp::move(builder._node);
	} else {
		rowNode = Rc<RowNode>::create(this, index, _selectionEnabled);
		rowNode->setRowKey(makeRowKey(row));
		makeTableRow(rowNode);
		buildCells(rowNode, &row, index, false);
		node = rowNode;
	}

	node->addStyleClass((index % 2) == 0 ? "even" : "odd");
	if (!row.dataLoaded) {
		node->addStyleClass("loading");
	}
	if (index == _selectedRow) {
		node->addStyleClass("selected");
	}
	for (auto &it : builder._classes) { node->addStyleClass(it); }
	if (!builder._name.empty()) {
		node->setName(builder._name);
	}

	// The height the controller laid this row out with, published for the sheet: a rule reaches a
	// SET of nodes and so cannot carry a per-row number.
	setStyleVariable(node, "--table-row-h", mem_std::toString(row.height, "px"));
	setStyleVariable(node, "--table-row-index", mem_std::toString(index));
	return node;
}

void TableView::buildCells(Node *node, const Row *row, size_t index, bool header) {
	if (!node) {
		return;
	}

	uint32_t column = 0;
	for (size_t i = 0; i < _columns.size(); ++i) {
		if (column >= uint32_t(_columns.size())) {
			break;
		}

		CellBuilder builder;
		builder._view = this;
		builder._column = &_columns[i];
		builder._row = row;
		builder._columnIndex = i;
		builder._rowIndex = index;

		if (header) {
			if (_headerCellCallback) {
				_headerCellCallback(builder);
			}
		} else if (_cellCallback) {
			_cellCallback(builder);
		}

		Rc<Node> cell;
		if (builder._node) {
			cell = sp::move(builder._node);
		} else {
			auto panel = Rc<Panel>::create();
			panel->setType("table-cell");
			panel->removeStyleClass("xl-ui-panel");
			panel->addStyleClass("xl-ui-table-cell");
			Panel::registerStyleAppliers("table-cell");

			// The icon goes in first and carries a lower ZOrder, so that a cell laid out as a flex
			// row puts it before the label - document order is what the row layout reads.
			if (builder._icon != IconName::None) {
				auto icon =
						panel->addChild(Rc<basic2d::IconSprite>::create(builder._icon), ZOrder(0));
				icon->setType("icon");
				icon->addStyleClass("table-icon");
			}

			auto label = panel->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
			label->setType("label");
			label->addStyleClass("table-label");
			if (builder._hasLabel) {
				label->setString(builder._label);
			} else if (header) {
				label->setString(_columns[i].title);
			} else {
				label->setString(builder.getValue().asString());
			}
			cell = panel;
		}

		if (header) {
			cell->addStyleClass("header-cell");
		}
		if (!_columns[i].styleClass.empty()) {
			cell->addStyleClass(_columns[i].styleClass);
		}
		for (auto &it : builder._classes) { cell->addStyleClass(it); }
		if (!builder._name.empty()) {
			cell->setName(builder._name);
		}

		// The cell's placement: its span, in the row's own column cursor. Everything else about
		// where it lands comes from the stamped geometry.
		TableCellInfo cfg;
		if (auto existing = LayoutSystem::getTableCell(cell)) {
			cfg = *existing;
		}
		cfg.columnSpan = sprt::max(builder._columnSpan, 1u);
		LayoutSystem::setTableCell(cell, cfg);

		setStyleVariable(cell, "--table-col-index", mem_std::toString(i));

		node->addChild(cell, ZOrder(int16_t(i + 1)));
		column += cfg.columnSpan;
	}
}

void TableView::handleRowTap(size_t index, uint32_t count) {
	if (index >= _rows.size()) {
		return;
	}
	if (count > 1) {
		if (_activateCallback) {
			_activateCallback(index, _rows[index]);
		}
		return;
	}
	setSelectedRow(index);
}

// --- RowBuilder ------------------------------------------------------------

bool TableView::RowBuilder::isSelected() const { return _view->getSelectedRow() == _index; }

void TableView::RowBuilder::setNode(Rc<Node> &&node) { _node = sp::move(node); }

void TableView::RowBuilder::addStyleClass(StringView value) {
	_classes.emplace_back(value.str<Interface>());
}

void TableView::RowBuilder::setName(StringView value) { _name = value.str<Interface>(); }

// --- CellBuilder -----------------------------------------------------------

const Value &TableView::CellBuilder::getValue() const {
	if (!_row || _column->key.empty()) {
		return s_nullValue;
	}
	return _row->data.getValue(_column->key);
}

void TableView::CellBuilder::setNode(Rc<Node> &&node) { _node = sp::move(node); }

void TableView::CellBuilder::setLabel(StringView value) {
	_label = value.str<Interface>();
	_hasLabel = true;
}

void TableView::CellBuilder::setIcon(IconName value) { _icon = value; }

void TableView::CellBuilder::setColumnSpan(uint32_t value) { _columnSpan = sprt::max(value, 1u); }

void TableView::CellBuilder::addStyleClass(StringView value) {
	_classes.emplace_back(value.str<Interface>());
}

void TableView::CellBuilder::setName(StringView value) { _name = value.str<Interface>(); }

// --- RowNode ---------------------------------------------------------------

TableView::RowNode::~RowNode() { }

bool TableView::RowNode::init(TableView *view, size_t index, bool interactive) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;
	_index = index;

	setType("table-row");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-table-row");
	registerStyleAppliers("table-row");

	// No listener at all unless the view wants selection: an always-present one would swallow the
	// hover and the swipe the scroll view wants, for a widget that ignores both.
	if (interactive) {
		_listener = addSystem(Rc<InputListener>::create());
		_listener->addMouseOverRecognizer([this](const GestureData &data) {
			switch (data.event) {
			case GestureEvent::Began:
				setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
					return state->handleHover(1); //
				});
				break;
			case GestureEvent::Activated: break;
			case GestureEvent::Ended:
			case GestureEvent::Cancelled:
				setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
					return state->handleHover(-1); //
				});
				break;
			}
			return true;
		}, false);

		_listener->addTapRecognizer([this](const GestureTap &tap) {
			if (tap.event == GestureEvent::Activated) {
				_view->handleRowTap(_index, tap.count);
			}
			return true;
		}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 1});
	}

	return true;
}

// --- HeaderNode ------------------------------------------------------------

bool TableView::HeaderNode::init(TableView *view) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;

	setType("table-header");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-table-header");
	registerStyleAppliers("table-header");

	// The header is laid out by exactly the same machinery as a row - same mode, same component -
	// which is why its cells cannot drift out of alignment with the rows below it.
	view->makeTableRow(this);
	return true;
}

} // namespace stappler::xenolith::ui
