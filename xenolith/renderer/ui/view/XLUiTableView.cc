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

	// On the SCROLL, not on this node: the header lives OUTSIDE it, and the edge band has to be
	// measured against the viewport rows actually scroll in.
	DragScrollSystem::acquireForNode(_scroll);

	// The scroll bar is built by basic2d out of nodes that can paint a fill and one radius; this
	// hands it nodes a stylesheet can paint outlines and four corners on, under the types
	// `scroll-indicator` and `scroll-indicator-track`. Done here rather than left to the
	// application because a widget of this layer is expected to answer to CSS everywhere else.
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

	/* The select callback deliberately does NOT fire here; handleRowTap sends it, exactly as
	TreeView does. It used to fire from here, which meant a programmatic setSelectedRow() notified
	on a TableView and stayed silent on a TreeView — the same call with two different meanings
	depending on which widget the caller happened to hold. "Moving the selection" and "the user
	picked a row" are different events, and only the widget that received the tap knows which one
	happened. */
}

// A model change no longer needs the whole table rebuilt: the node's revision is in the RowKey, so
// a replaced payload fails to match on its own row and only that row is remade. What is dropped
// here is what the key cannot see — a span's answers, which come from outside the model entirely.
void TableView::invalidateSource() {
	dropSpanData();
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

void TableView::dropSpanData() {
	for (auto &it : _rows) {
		if (it.node && it.node->isSpan()) {
			it.dataLoaded = false;
		}
	}
}

void TableView::handleSourceDirty(SubscriptionFlags flags) {
	// Unforced: a payload edit bumps the node's revision and the revision is in the RowKey, so
	// exactly the rows that changed get new nodes and every other visible row keeps the one it has.
	// Only a span's answers have to be dropped, because nothing about the model says when they went
	// stale.
	if (flags.hasFlag(Model::Update::Structure)) {
		dropSpanData();
	}

	refresh();
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
	// Harvested BEFORE _rows is cleared. This used to clear first and then walk the empty vector, so
	// the carry-over map was always empty and every payload was re-requested on every single
	// rebuild — a window resize re-fetched the whole visible table.
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

	/* The root's children, in order. A table is a tree read one level deep: an explicit child is one
	row that already holds its payload, and a span child is N rows that do not exist until they are
	asked for. Both kinds can sit in the same table. */
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

	// Suppresses the redundant node rebuild a synchronous delivery would schedule from inside this
	// loop: refresh() schedules one for the whole pass anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		// Everything that is not an unfetched span row already has its payload, in the model.
		if (_rows[i].dataLoaded || !_rows[i].node || !_rows[i].node->isSpan()) {
			++i;
			continue;
		}

		Rc<ModelNode> span = _rows[i].node;

		// One request per run of consecutive unloaded offsets of the same span - the cursor read
		// that makes a table of fifty thousand rows a handful of calls rather than fifty thousand.
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
			// The span planned no request, so no callback is coming. Mark the range resolved rather
			// than re-ask for it on every rebuild from now on.
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
		// Marked loaded even for an offset the span did not answer for: "loaded but empty" has to be
		// terminal, or an under-delivering source would be asked again on every rebuild forever.
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

		// The grip column is the view's to fill, and the caller's callback is not asked about it:
		// what goes there is a DragSource, and a cell node from outside would have replaced it.
		if (_reorderEnabled && StringView(_columns[i].key) == ReorderColumnKey) {
			Rc<Node> gripCell = header ? Rc<Node>(Rc<Panel>::create()) : makeReorderCell(index);
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

// ---- reorder ------------------------------------------------------------------------------------

namespace {

// What a row drag carries. The table pointer is identity, not convenience: two tables on one scene
// must not accept each other's rows just because both know how to reorder.
struct TableRowPayload : public Ref {
	static constexpr auto TypeName = StringView("xl/table-row");

	TableView *view = nullptr;
	size_t index = 0;
};

// Null unless the drag is one of ours AND came from this very table.
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
		if (!_dropTarget) {
			_dropTarget = addSystem(Rc<DropTarget>::create(DropTargetSlots{
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
			}));
		}

		if (!_reorderKeys) {
			_reorderKeys = addSystem(Rc<InputListener>::create());

			auto &hk = EngineHotkeys::get();
			_reorderKeys->addHotkey(hk.moveItemUp, [this](HotkeyId, const InputEvent &) {
				return handleReorderHotkey(false);
			}, HotkeyFlags::Repeatable);
			_reorderKeys->addHotkey(hk.moveItemDown, [this](HotkeyId, const InputEvent &) {
				return handleReorderHotkey(true);
			}, HotkeyFlags::Repeatable);

			// A key event carries the last pointer position, so the default filter would answer
			// Alt+Up only while the mouse happened to hover the table.
			_reorderKeys->setTouchFilter(
					[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
				if (event.data.isKeyEvent()) {
					return true;
				}
				return cb(event);
			});
		}
	} else {
		hideInsertionLine();
		if (_dropTarget) {
			removeSystem(_dropTarget);
			_dropTarget = nullptr;
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

	auto icon = panel->addChild(
			Rc<basic2d::IconSprite>::create(IconName::Editor_drag_handle_outline), ZOrder(0));
	icon->setType("icon");
	icon->addStyleClass("table-icon");

	/* The DragSource goes on the GRIP CELL, not on the row.

	This table scrolls on the same axis a row drag moves along, which no other drag source in the
	tree has had to contend with - the dock's tab strip is horizontal. A narrow grip is what keeps
	the two apart: a swipe that starts here reaches this listener, which sits deeper than the
	scroll view's, and DragSource takes the pointer exclusively on the first frame past the
	threshold. A swipe starting anywhere else in the row still scrolls, which is what it should do. */
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
			// A plain Layer, painted here: a decorator is parked outside this widget's subtree, so
			// no StyleResolver reaches it and anything expecting CSS would come up unstyled.
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

	// A boundary is a gap between rows; the row's FINAL index is one less when it came from above,
	// because taking it out closes the gap it used to occupy.
	const size_t to = (boundary > from) ? boundary - 1 : boundary;
	return reorderRow(from, to);
}

bool TableView::reorderRow(size_t from, size_t to) {
	if (!_reorderEnabled || !_reorderCallback) {
		return false;
	}
	if (from >= _rows.size() || to >= _rows.size() || from == to) {
		// A move onto itself is not a refusal, it is not a move: reporting it would put an entry in
		// somebody's undo history for a drag that changed nothing.
		return false;
	}

	if (!_reorderCallback(from, to)) {
		return false;
	}

	/* The selection follows the ROW, not the index.

	Computed here rather than left to the caller, because the caller answers in model terms and the
	selection is the view's. A selection left on its old number silently points at whatever slid
	into that place. */
	if (_selectedRow != maxOf<size_t>()) {
		size_t selected = _selectedRow;
		if (selected == from) {
			selected = to;
		} else if (from < selected && selected <= to) {
			--selected;
		} else if (to <= selected && selected < from) {
			++selected;
		}
		setSelectedRow(selected);
	}
	return true;
}

bool TableView::handleReorderHotkey(bool down) {
	if (!_reorderEnabled || !_reorderCallback) {
		return false;
	}

	// The gate is the selection: a table nobody has picked a row in has no claim on Alt+arrows, and
	// declining leaves the combination for whoever is below.
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

	// The column geometry is resolved by resolveColumns(), which needs a column set AND a non-zero
	// width. Before that there is nothing to report - and reporting the whole row would be a lie
	// that looks like an answer.
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

	// Sent here rather than from setSelectedRow(), so that it means "the user picked this row" and
	// not merely "the selection moved" — the same split TreeView makes.
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
