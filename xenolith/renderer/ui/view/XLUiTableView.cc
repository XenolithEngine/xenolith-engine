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
#include "XLUiDragScrollSystem.h"
#include "XLUiStyleSystem.h"
#include "XLInteractiveComponent.h"
#include "XL2dLabel.h"
#include "XL2dIconSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static const Value s_nullValue;

/* Cells are transparent so the row's ground, alternation, hover and selection show through (an
unstyled Panel is opaque white). Painted below the stylesheet (Panel::setPathColor), so a
`table-cell { background-color: ... }` rule still overrides it. */
static void TableView_paintCellDefaults(NotNull<Panel> cell) {
	cell->setPathColor(Color4B(0, 0, 0, 0), true);
}

TableView::~TableView() { }

bool TableView::init() { return init(nullptr); }

bool TableView::init(Model *source) {
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

	// On the scroll, not this node: the edge band is measured against the rows' viewport.
	DragScrollSystem::acquireForNode(_scroll);

	// A CSS-styleable scroll bar (`scroll-indicator`, `scroll-indicator-track`).
	useStyledScrollIndicator(_scroll);

	_sourceListener = addSystem(Rc<DataListener<Model>>::create(
			[this](SubscriptionFlags flags) { handleSourceDirty(flags); }, source));

	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[this](CallbackSystem *, const ComponentMask &) {
		if (_rebuildPending) {
			_rebuildPending = false;
			rebuildRows();
		}
	});

	// Content measurement, answered only in auto-height mode; otherwise falls through.
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

	// The header is a sibling of the scroll view, pinned to the top, so it is sticky.
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

void TableView::setSource(Model *source) {
	if (_sourceListener->getSubscription() == source) {
		return;
	}
	_sourceListener->setSubscription(source);
	_selectedRow = maxOf<size_t>();
	refresh();
}

auto TableView::getSource() const -> Model * { return _sourceListener->getSubscription(); }

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
		// getRowHeight(), not Row::height, which is filled only by the deferred rebuildRows().
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
		// Avoid nested scrollers claiming the same swipe.
		_scroll->setEnabled(!value);
	}
	markMeasureDirty();
	updateIntrinsicHeight();
}

void TableView::setIntrinsicHeightCallback(Function<void(float)> &&cb) {
	_intrinsicHeightCallback = sp::move(cb);
	// Report the current height to the new listener.
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
	// No early return on the index; see TreeView::setSelectedRow
	setSelectedIdentity((index < _rows.size()) ? _rows[index].getId() : ItemId(0));
}

void TableView::setSelectedIdentity(ItemId id) {
	if (_selectedId == id) {
		return;
	}

	const auto prev = _selectedRow;

	_selectedId = id;

	// The index is derived from the identity
	remapSelection();

	const auto index = _selectedRow;
	if (prev == index) {
		return;
	}

	// No rebuild: update the class on the two affected nodes, if they are on screen.
	if (auto node = getRowNode(prev)) {
		updateRowNode(node, prev);
	}
	if (auto node = getRowNode(index)) {
		updateRowNode(node, index);
	}

	// The select callback is sent by handleRowTap (user picks only), as in TreeView.

	publishSelection();
}

void TableView::bindReorderHotkeys() {
	if (!_reorderKeys) {
		return;
	}

	auto &hk = EngineHotkeys::get();

	/* SelectedOnly only when this table owns the scene's selection: a non-owner is never on the
	selection chain and would never receive the chord. handleReorderHotkey still returns false
	when there is nothing to move, so the chord falls through. */
	const auto flags = HotkeyFlags::Repeatable
			| (_selectionOwned ? HotkeyFlags::SelectedOnly : HotkeyFlags::None);

	_reorderKeys->removeHotkey(hk.moveItemUp);
	_reorderKeys->removeHotkey(hk.moveItemDown);

	_reorderKeys->addHotkey(hk.moveItemUp, [this](HotkeyId, const InputEvent &) {
		return handleReorderHotkey(false);
	}, flags);
	_reorderKeys->addHotkey(hk.moveItemDown, [this](HotkeyId, const InputEvent &) {
		return handleReorderHotkey(true);
	}, flags);

	// No touch filter: InputListener::handleHotkey consults neither canHandleEvent nor the filter.
}

void TableView::setSelectionOwned(bool value) {
	if (_selectionOwned == value) {
		return;
	}
	_selectionOwned = value;

	// A candidate for arrow navigation only while it can hold the selection
	setNodeSelectable(this, _selectionOwned, this);

	// The reorder bindings are gated on ownership, so they have to be re-flagged when it changes
	bindReorderHotkeys();

	if (_selectionOwned) {
		publishSelection();
	} else if (auto system = SelectionSystem::findForNode(this)) {
		// Only if it was ours; see TreeView::setSelectionOwned
		if (system->getOwner() == this) {
			system->clear();
		}
	}
}

SelectionItem TableView::makeSelectionItem(size_t index) const {
	if (index >= _rows.size()) {
		return SelectionItem();
	}
	// A table row is one model node, so unlike a tree there is no span offset to carry
	return SelectionItem{_rows[index].node.get(), 0};
}

void TableView::publishSelection() {
	if (!_selectionOwned || _applyingSelection) {
		return;
	}

	auto system = SelectionSystem::acquireForNode(this);
	if (!system) {
		return;
	}

	if (_selectedId == ItemId(0)) {
		if (system->getOwner() == this) {
			system->clear();
		}
		return;
	}

	if (_selectedRow < _rows.size()) {
		auto item = makeSelectionItem(_selectedRow);
		system->select(this, makeSpanView(&item, 1));
	}
}

Node *TableView::resolveSelectionNode(const SelectionItem &item) const {
	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].node.get() == item.ref.get()) {
			return getRowNode(i);
		}
	}
	return nullptr;
}

bool TableView::moveSelection(SelectionDirection dir) {
	if (!_selectionOwned || _selectedRow >= _rows.size()) {
		return false;
	}

	if (dir == SelectionDirection::Up && _selectedRow > 0) {
		selectRowFromKeyboard(_selectedRow - 1);
		return true;
	} else if (dir == SelectionDirection::Down && _selectedRow + 1 < _rows.size()) {
		selectRowFromKeyboard(_selectedRow + 1);
		return true;
	}
	return false;
}

bool TableView::enterSelection(SelectionDirection dir, const Rect &fromWorld) {
	if (!_selectionOwned || _rows.empty()) {
		return false;
	}

	auto index = getEnteringRow(makeGeometrySource(), dir, fromWorld);
	if (index >= _rows.size()) {
		return false;
	}
	selectRowFromKeyboard(index);
	return true;
}

void TableView::selectRowFromKeyboard(size_t index) {
	if (index >= _rows.size()) {
		return;
	}

	setSelectedRow(index);
	scrollRowIntoView(_scroll, _controller, index);

	if (_selectCallback && index < _rows.size()) {
		_selectCallback(index, _rows[index]);
	}
}

void TableView::handleSelectionChanged(SpanView<SelectionItem> items) {
	_applyingSelection = true;

	if (items.empty()) {
		setSelectedIdentity(ItemId(0));
	} else if (auto node = dynamic_cast<ModelNode *>(items.front().ref.get())) {
		setSelectedIdentity(node->getId());
	}

	_applyingSelection = false;
}

// Explicit rows are rebuilt through the revision in the RowKey; span answers come from outside the
// model, so they are dropped here.
void TableView::invalidateSource() {
	dropSpanData();
	refresh();
	requestRebuildNodes(true);
}

void TableView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it, so a coalesced forced request still forces.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
	// The rebuild runs in the components phase, which is opt-in per visit.
	markComponentsDirty();
}

void TableView::requestRebuildNodes(Function<void()> &&cb, bool force) {
	if (cb) {
		_rebuildCallbacks.emplace_back(sp::move(cb));
	}
	requestRebuildNodes(force);
}

void TableView::dropSpanData() {
	for (auto &it : _rows) {
		if (it.node && it.node->isSpan()) {
			it.dataLoaded = false;
		}
	}
}

void TableView::handleSourceDirty(SubscriptionFlags flags) {
	// Unforced: changed rows fail their RowKey revision match. Span answers are dropped on a
	// structure change, since the model cannot tell when they went stale.
	if (flags.hasFlag(Model::Update::Structure)) {
		dropSpanData();
	}

	refresh();
}

void TableView::refresh() {
	rebuildModel();
	// before any node exists, so an inline source needs no placeholder frame
	requestRowData();
	requestRebuildNodes();
	// Reported here, not from the deferred rebuildRows(), so an owner learns before that frame.
	updateIntrinsicHeight();
}

void TableView::resolveColumns() {
	auto source = getSource();
	(void)source;

	const uint32_t count = uint32_t(_columns.size());
	if (count == 0 || _contentSize.width <= 0.0f) {
		return;
	}

	// Tracks come from CSS (`grid-template-columns` -> TableLayoutInfo), with Column::track as the
	// fallback, sized by the same routine as a static `display: table`.
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
	resolveTableColumns(tracks, available, spacingH, padding.left, next);

	next.borderCollapse = info ? info->borderCollapse : BorderCollapse::Separate;
	next.borderSpacingH = spacingH;
	next.borderSpacingV = spacingV;
	next.justifyItems = info ? info->justifyItems : GridAlign::Stretch;
	next.alignItems = info ? info->alignItems : GridAlign::Stretch;
	next.occupiedColumns.resize(count, 0);

	// Advance the generation only on a real change; rows key their layout off it.
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
	// the const overload: the non-const one marks the controller dirty
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
	// Harvested before _rows is cleared, carrying loaded span payloads over the rebuild.
	Map<Model::Position, Value> loaded;
	for (auto &row : _rows) {
		if (row.dataLoaded && row.node && row.node->isSpan()) {
			loaded.emplace(Model::Position{row.node->getId(), row.offset}, sp::move(row.spanData));
		}
	}

	_rows.clear();

	auto source = getSource();
	if (!source) {
		return;
	}

	// The root's children, in order: an explicit child is one row, a span child is N rows.
	for (auto &child : source->getRoot()->getChildren()) {
		if (!child->isSpan()) {
			Row row;
			row.node = child;
			row.revision = child->getRevision();
			row.dataLoaded = true; // the model holds it
			_rows.emplace_back(sp::move(row));
			continue;
		}

		const auto count = child->getSpanCount();
		_rows.reserve(_rows.size() + count);
		for (uint64_t i = 0; i < count; ++i) {
			Row row;
			row.node = child;
			row.offset = i;
			row.revision = child->getRevision();

			auto it = loaded.find(Model::Position{child->getId(), i});
			if (it != loaded.end()) {
				row.spanData = sp::move(it->second);
				row.dataLoaded = true;
				loaded.erase(it);
			}

			_rows.emplace_back(sp::move(row));
		}
	}
}

void TableView::requestRowData() {
	auto source = getSource();
	if (!source || _rows.empty()) {
		return;
	}

	// Suppresses the rebuild a synchronous delivery would schedule; refresh() schedules one anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		// Only unfetched span rows need a request.
		if (_rows[i].dataLoaded || !_rows[i].node || !_rows[i].node->isSpan()) {
			++i;
			continue;
		}

		Rc<ModelNode> span = _rows[i].node;

		// One request per run of consecutive unloaded offsets of the same span.
		const auto first = _rows[i].offset;
		size_t count = 1;
		while (i + count < _rows.size() && !_rows[i + count].dataLoaded
				&& _rows[i + count].node == span && _rows[i + count].offset == first + count) {
			++count;
		}

		Rc<TableView> self(this);
		if (span->getSpanData([self, span, first, count](Map<uint64_t, Value> &data) {
			self->handleSliceData(span, first, count, data);
		}, first, count) == 0) {
			// No request planned, so no callback: mark the range resolved to avoid re-asking.
			for (size_t j = 0; j < count; ++j) { _rows[i + j].dataLoaded = true; }
		}

		i += count;
	}

	_inDataRequest = false;
}

void TableView::handleSliceData(ModelNode *span, uint64_t first, size_t count,
		Map<uint64_t, Value> &data) {
	bool updated = false;
	for (auto &row : _rows) {
		if (row.node != span || row.offset < first || row.offset >= first + count) {
			continue;
		}
		auto it = data.find(row.offset);
		if (it != data.end()) {
			row.spanData = sp::move(it->second);
		}
		// Loaded even without an answer, so an under-delivering source is not asked again.
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
	// LayoutMode::TableRow reads the stamped geometry. The marker stops the resolver adding another
	// layout; cells still get TableCellInfo from CSS via the parent's TableColumnsComponent.
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

void TableView::remapSelection() {
	if (_selectedId == ItemId(0)) {
		_selectedRow = maxOf<size_t>();
		return;
	}

	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].getId() == _selectedId) {
			_selectedRow = i;
			return;
		}
	}

	// No row shows this identity now; only the index is dropped (see TreeView::remapSelection)
	_selectedRow = maxOf<size_t>();
}

void TableView::rebuildRows() {
	if (!_controller) {
		return;
	}

	// Before the nodes are made: makeRow() reads _selectedRow
	remapSelection();

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
			// Detached here without cleanup, which would strip systems from a node about to be
			// re-attached. Nulling the item keeps clear() from cleaning it.
			it.node->removeFromParent(false);
			it.node = nullptr;
			it.handle = nullptr;
		}
	}

	_controller->clear();

	for (size_t i = 0; i < _rows.size(); ++i) {
		// Resolved once and stored, so the factory publishes the height the controller used.
		_rows[i].height = getRowHeight(_rows[i]);
		// `this` captured raw: this node owns _controller, which owns the factory. The index is
		// safe because every change to _rows rebuilds.
		_controller->addItem([this, i](const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makeRow(i);
		}, _rows[i].height);
	}

	_controller->commitChanges();
	_reusableRows.clear();

	/* New rows are already laid out here (Node::runPendingPhases, commitChanges). Callbacks are
	taken off the list before they run, so a new request is served by the next rebuild. */
	auto callbacks = sp::move(_rebuildCallbacks);
	_rebuildCallbacks.clear();
	for (auto &it : callbacks) { it(); }
}

auto TableView::makeRowKey(const Row &row) const -> RowKey {
	RowKey key;
	key.node = row.node;
	key.offset = row.offset;
	key.revision = row.revision;
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
	// the const overload: the non-const getItems() marks the controller dirty
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
	const auto selected = (index == _selectedRow);

	if (selected) {
		node->addStyleClass("selected");
	} else {
		node->removeStyleClass("selected");
	}

	// The scene-wide selection flag, applied per node since row nodes are recycled; see
	// TreeView::updateRowNode
	if (_selectionOwned) {
		setNodeSelected(node, selected);
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

	// Per-row values for the stylesheet.
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

		// The grip column is filled by the view (with a DragSource); the cell callback is skipped.
		if (_reorderEnabled && StringView(_columns[i].key) == ReorderColumnKey) {
			Rc<Node> gripCell;
			if (header) {
				// the header's grip cell is a bare transparent Panel with no handle
				auto panel = Rc<Panel>::create();
				TableView_paintCellDefaults(panel);
				gripCell = panel;
			} else {
				gripCell = makeReorderCell(index);
			}
			if (gripCell) {
				gripCell->setType("table-cell");
				gripCell->addStyleClass("xl-ui-table-cell");
				if (!_columns[i].styleClass.empty()) {
					gripCell->addStyleClass(_columns[i].styleClass);
				}
				TableCellInfo cfg;
				cfg.columnSpan = 1;
				LayoutSystem::setTableCell(gripCell, cfg);
				setStyleVariable(gripCell, "--table-col-index", mem_std::toString(i));
				node->addChild(gripCell, ZOrder(int16_t(i + 1)));
			}
			++column;
			continue;
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
			TableView_paintCellDefaults(panel);

			// The icon has a lower ZOrder, so a flex row puts it before the label.
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

		// The cell's span; the rest of its placement comes from the stamped geometry.
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

// ---- reorder ------------------------------------------------------------------------------------

namespace {

// What a row drag carries. The table pointer keeps tables from accepting each other's rows.
struct TableRowPayload : public Ref {
	static constexpr auto TypeName = StringView("xl/table-row");

	TableView *view = nullptr;
	size_t index = 0;
};

// Null unless the drag is a row drag from this table.
static TableRowPayload *TableView_payloadOf(const DragEvent &event, const TableView *view) {
	if (!event.data || !event.data->isLocal(TableRowPayload::TypeName)) {
		return nullptr;
	}
	auto payload = static_cast<TableRowPayload *>(event.data->getLocal());
	return (payload && payload->view == view) ? payload : nullptr;
}

} // namespace

void TableView::setReorderCallback(Function<bool(size_t, size_t)> &&cb) {
	_reorderCallback = sp::move(cb);
	setReorderEnabled(true);
}

void TableView::setReorderEnabled(bool value) {
	if (_reorderEnabled == value) {
		return;
	}
	_reorderEnabled = value;
	updateReorderSystems();

	// The grip lives in a cell, so the rows have to be built again to gain or lose it.
	requestRebuildNodes(true);
}

void TableView::updateReorderSystems() {
	if (_reorderEnabled) {
		if (!_hasDropTarget) {
			_hasDropTarget = true;
			setDropTarget(this,
					DropTargetSlots{
						.accept = [this](const DragEvent &event) -> DragResponse {
				if (!TableView_payloadOf(event, this)) {
					return DragResponse();
				}
				return DragResponse{DragActions::Move};
			},
						.enter =
								[this](const DragEvent &event) {
				showInsertionLine(getRowBoundaryAt(event.location));
			},
						.over =
								[this](const DragEvent &event) {
				showInsertionLine(getRowBoundaryAt(event.location));
			},
						.leave = [this](const DragEvent &) { hideInsertionLine(); },
						.drop =
								[this](const DragEvent &event, DragActions) {
				auto payload = TableView_payloadOf(event, this);
				if (!payload) {
					return false;
				}
				// Read the index out before anything moves: the drop is what invalidates it.
				return handleReorderDrop(payload->index, event.location);
			},
					});
		}

		if (!_reorderKeys) {
			_reorderKeys = addSystem(Rc<InputListener>::create());
			bindReorderHotkeys();
		}
	} else {
		hideInsertionLine();
		if (_hasDropTarget) {
			removeDropTarget(this);
			_hasDropTarget = false;
		}
		if (_reorderKeys) {
			removeSystem(_reorderKeys);
			_reorderKeys = nullptr;
		}
	}
}

Rc<Node> TableView::makeReorderCell(size_t index) {
	auto panel = Rc<Panel>::create();
	panel->setType("table-cell");
	panel->removeStyleClass("xl-ui-panel");
	panel->addStyleClass("xl-ui-table-cell");
	panel->addStyleClass("xl-ui-table-drag-handle");
	Panel::registerStyleAppliers("table-cell");
	TableView_paintCellDefaults(panel);

	auto icon = panel->addChild(
			Rc<basic2d::IconSprite>::create(IconName::Editor_drag_handle_outline), ZOrder(0));
	icon->setType("icon");
	icon->addStyleClass("table-icon");

	/* The DragSource is on the grip cell, not the row: the drag and the scroll share an axis, so
	only a swipe starting on the grip drags (this listener is deeper than the scroll view's and
	captures the pointer past the threshold); elsewhere the row scrolls. */
	panel->addSystem(Rc<DragSource>::create([this, index](DragOffer &offer) -> bool {
		if (!_reorderEnabled || index >= _rows.size()) {
			return false;
		}

		auto payload = Rc<TableRowPayload>::alloc();
		payload->view = this;
		payload->index = index;

		offer.local = payload.get();
		offer.localType = TableRowPayload::TypeName.str<Interface>();
		offer.allowedActions = DragActions::Move;
		offer.defaultAction = DragActions::Move;

		Rect rect;
		const Size2 size = getRowRect(index, rect) ? rect.size : Size2(120.0f, _rowHeight);
		offer.decorator = [size]() -> Rc<Node> {
			// Painted in code: the decorator lives outside this subtree, out of reach of CSS.
			auto ghost = Rc<basic2d::Layer>::create(Color4B(0xFC, 0xB4, 0x00, 0x60));
			ghost->setContentSize(size);
			ghost->setAnchorPoint(Anchor::MiddleLeft);
			return ghost;
		};
		return true;
	}));

	return panel;
}

void TableView::showInsertionLine(size_t boundary) {
	Rect rect;
	if (boundary == maxOf<size_t>()
			|| !ui::getRowBoundaryRect(makeGeometrySource(), boundary, rect)) {
		hideInsertionLine();
		return;
	}

	if (!_insertionLine) {
		_insertionLine = addChild(Rc<basic2d::Layer>::create(), ZOrder(64));
		_insertionLine->setType("table-insertion-line");
		_insertionLine->setAnchorPoint(Anchor::BottomLeft);
	}

	_insertionLine->setVisible(true);
	_insertionLine->setPosition(rect.origin);
	_insertionLine->setContentSize(rect.size);
}

void TableView::hideInsertionLine() {
	if (_insertionLine) {
		_insertionLine->removeFromParent(true);
		_insertionLine = nullptr;
	}
}

bool TableView::handleReorderDrop(size_t from, const Vec2 &nodeLocation) {
	const size_t boundary = getRowBoundaryAt(nodeLocation);
	hideInsertionLine();

	if (boundary == maxOf<size_t>() || from >= _rows.size()) {
		return false;
	}

	// The final index is one less than the boundary when the row came from above.
	const size_t to = (boundary > from) ? boundary - 1 : boundary;
	return reorderRow(from, to);
}

bool TableView::reorderRow(size_t from, size_t to) {
	if (!_reorderEnabled || !_reorderCallback) {
		return false;
	}
	if (from >= _rows.size() || to >= _rows.size() || from == to) {
		// A move onto itself is not reported to the callback.
		return false;
	}

	if (!_reorderCallback(from, to)) {
		return false;
	}

	// The selection follows the row via remapSelection() on the rebuild; no index shift here.
	return true;
}

bool TableView::handleReorderHotkey(bool down) {
	if (!_reorderEnabled || !_reorderCallback) {
		return false;
	}

	// Without a selected row, decline so the chord falls through.
	const size_t selected = _selectedRow;
	if (selected == maxOf<size_t>() || selected >= _rows.size()) {
		return false;
	}

	if (down) {
		if (selected + 1 >= _rows.size()) {
			return false;
		}
		return reorderRow(selected, selected + 1);
	}
	if (selected == 0) {
		return false;
	}
	return reorderRow(selected, selected - 1);
}

// ---- geometry -----------------------------------------------------------------------------------

RowGeometrySource TableView::makeGeometrySource() const {
	return RowGeometrySource{this, _scroll, _controller};
}

bool TableView::getRowRect(size_t index, Rect &out) const {
	return ui::getRowRect(makeGeometrySource(), index, out);
}

bool TableView::getCellRect(size_t row, size_t column, Rect &out) const {
	Rect rowRect;
	if (!getRowRect(row, rowRect)) {
		return false;
	}

	// No column geometry until resolveColumns() has a column set and a non-zero width.
	if (column >= _geometry.columns.size()) {
		return false;
	}

	auto &col = _geometry.columns.at(column);
	out = Rect(rowRect.origin.x + col.position, rowRect.origin.y, col.width, rowRect.size.height);
	return out.size.width > 0.0f;
}

size_t TableView::getRowIndexAt(const Vec2 &nodeLocation) const {
	return ui::getRowIndexAt(makeGeometrySource(), nodeLocation);
}

size_t TableView::getRowBoundaryAt(const Vec2 &nodeLocation, Rect *boundaryRect) const {
	return ui::getRowBoundaryAt(makeGeometrySource(), nodeLocation, boundaryRect);
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

	// Sent here, not from setSelectedRow(): it reports a user pick, as in TreeView.
	if (_selectCallback) {
		_selectCallback(index, _rows[index]);
	}
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
	return _row->getData().getValue(_column->key);
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

	// No listener unless the view wants selection, so hover and swipes reach the scroll view.
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
			// Up to two taps, so `count` can reach the activate callback; Immediate reports the
			// first tap without waiting for the double-tap interval.
		}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 2, InputTapFlags::Immediate});
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

	// Laid out like a row, so header cells align with the rows.
	view->makeTableRow(this);
	return true;
}

} // namespace stappler::xenolith::ui
