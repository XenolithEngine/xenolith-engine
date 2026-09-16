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

#include "XLUiTreeView.h"
#include "XLUiDragScrollSystem.h"
#include "XLUiButton.h"
#include "XLAction.h" // Sequence: the auto-expand dwell, see armDropExpand
#include "XL2dLayer.h"
#include "XLInteractiveComponent.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

TreeView::~TreeView() { }

bool TreeView::init() { return init(nullptr); }

bool TreeView::init(Model *source) {
	if (!Panel::init()) {
		return false;
	}

	setType("tree-view");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-tree-view");
	// the same fill / outline / border-radius appliers Panel registers for itself, under "tree-view"
	registerStyleAppliers("tree-view");

	// The scroll view is sized by handleContentSizeDirty(); the marker keeps the style resolver from
	// adding a flex layout that would write the same size.
	setComponent<SystemManagedLayout>();

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
	_scroll->setName("tree-scroll");
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	_controller = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_controller);

	// On the scroll, not this node: the edge band is measured against the viewport.
	DragScrollSystem::acquireForNode(_scroll);

	// A CSS-styleable scroll bar (`scroll-indicator`, `scroll-indicator-track`).
	useStyledScrollIndicator(_scroll);

	// One listener for the whole model: the model is the single Subscription for all its nodes.
	_sourceListener = addSystem(Rc<DataListener<Model>>::create([this](SubscriptionFlags flags) {
		handleSourceDirty(flags); //
	}, source));

	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[this](CallbackSystem *, const ComponentMask &) {
		if (_rebuildPending) {
			_rebuildPending = false;
			rebuildRows();
		}
	});

	if (source) {
		refresh();
	}

	return true;
}

void TreeView::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	if (_scroll) {
		_scroll->setContentSize(_contentSize);
	}
}

void TreeView::setSource(Model *source) {
	if (getSource() == source) {
		return;
	}

	_sourceListener->setSubscription(source);
	_expanded.clear();
	_selectedRow = maxOf<size_t>();
	refresh();
}

auto TreeView::getSource() const -> Model * {
	return _sourceListener ? _sourceListener->getSubscription() : nullptr;
}

bool TreeView::isExpanded(const ModelNode *node) const {
	return node && _expanded.find(node->getId()) != _expanded.end();
}

void TreeView::setRootVisible(bool value) {
	if (_rootVisible == value) {
		return;
	}

	_rootVisible = value;
	refresh();
}

auto TreeView::getRow(size_t index) const -> const Row * {
	return index < _rows.size() ? &_rows[index] : nullptr;
}

bool TreeView::isRowExpanded(size_t index) const {
	return index < _rows.size() && _rows[index].expanded;
}

bool TreeView::expandRow(size_t index) {
	if (index >= _rows.size() || !_rows[index].isCategory() || _rows[index].expanded) {
		return false;
	}

	// The model is about to be re-derived, so hold the category rather than a reference to the row.
	Rc<ModelNode> cat = _rows[index].node;
	_expanded.emplace(cat->getId());

	// Lazily loaded children, inline or later. The completion holds an Rc; the node drops it once
	// fired, so there is no cycle.
	Rc<TreeView> self(this);

	++_deferRefresh;
	cat->requestChilds([self] { self->refresh(); });
	--_deferRefresh;

	refresh();
	return true;
}

bool TreeView::collapseRow(size_t index) {
	if (index >= _rows.size() || !_rows[index].isCategory() || !_rows[index].expanded) {
		return false;
	}

	Rc<ModelNode> cat = _rows[index].node;
	_expanded.erase(cat->getId());

	// Descendants keep their expansion by default; forgetting also releases lazily loaded children.
	if (!_keepExpanded) {
		forgetSubtree(cat);
	}

	refresh();
	return true;
}

bool TreeView::toggleRow(size_t index) {
	if (index >= _rows.size() || !_rows[index].isCategory()) {
		return false;
	}

	return _rows[index].expanded ? collapseRow(index) : expandRow(index);
}

void TreeView::forgetSubtree(ModelNode *cat) {
	for (auto &it : cat->getChildren()) {
		if (!it->isCategory()) {
			continue;
		}
		forgetSubtree(it);
		_expanded.erase(it->getId());
	}

	cat->resetChilds();
}

void TreeView::setKeepExpandedState(bool value) { _keepExpanded = value; }

void TreeView::setRowCallback(RowFunction &&cb) {
	_rowCallback = sp::move(cb);
	// Forced: a row's key is unchanged, but the function that turns a row into a node is not.
	requestRebuildNodes(true);
}

void TreeView::setRowHeightCallback(RowHeightFunction &&cb) {
	_rowHeightCallback = sp::move(cb);
	requestRebuildNodes(true);
}

void TreeView::setRowHeight(float value) {
	if (_rowHeight == value) {
		return;
	}

	_rowHeight = value;
	requestRebuildNodes();
}

float TreeView::getRowHeight(const Row &row) const {
	if (!_rowHeightCallback) {
		return _rowHeight;
	}

	auto ret = _rowHeightCallback(row);
	return (ret > 0.0f) ? ret : _rowHeight;
}

void TreeView::setLabelKey(StringView key) {
	_labelKey = key.str<Interface>();
	requestRebuildNodes(true);
}

void TreeView::setSelectCallback(RowEventFunction &&cb) {
	_selectCallback = sp::move(cb);
	setSelectionEnabled(true);
}

void TreeView::setActivateCallback(RowEventFunction &&cb) {
	_activateCallback = sp::move(cb);
	setSelectionEnabled(true);
}

void TreeView::setSelectionEnabled(bool value) {
	if (_selectionEnabled == value) {
		return;
	}

	_selectionEnabled = value;
	// Forced: whether a row carries an input listener at all is decided when the node is built.
	requestRebuildNodes(true);
}

void TreeView::setSelectedRow(size_t index) {
	// No early return on the index: a hidden row has index maxOf but a set identity, which clearing
	// must still reach
	if (index < _rows.size()) {
		setSelectedIdentity(_rows[index].getId(), _rows[index].offset);
	} else {
		setSelectedIdentity(ItemId(0), 0);
	}
}

void TreeView::setSelectedIdentity(ItemId id, uint64_t offset) {
	if (_selectedId == id && _selectedOffset == offset) {
		return;
	}

	const auto previous = _selectedRow;

	_selectedId = id;
	_selectedOffset = offset;

	// The index is derived from the identity, which may currently have no row
	remapSelection();

	const auto index = _selectedRow;
	if (previous == index) {
		return;
	}

	// Only the two affected row nodes are restyled; other rows read _selectedRow in makeRow().
	if (auto node = getRowNode(previous)) {
		updateRowNode(node, previous);
	}
	if (auto node = getRowNode(index)) {
		updateRowNode(node, index);
	}

	publishSelection();
}

void TreeView::setSelectionOwned(bool value) {
	if (_selectionOwned == value) {
		return;
	}
	_selectionOwned = value;

	if (_selectionOwned) {
		publishSelection();
	} else if (auto system = SelectionSystem::findForNode(this)) {
		// Clear the scene's selection only if this view owns it
		if (system->getOwner() == this) {
			system->clear();
		}
	}
}

SelectionItem TreeView::makeSelectionItem(size_t index) const {
	if (index >= _rows.size()) {
		return SelectionItem();
	}
	// The ModelNode is the identity; the offset distinguishes rows of one span
	return SelectionItem{_rows[index].node.get(), _rows[index].offset};
}

void TreeView::publishSelection() {
	if (!_selectionOwned || _applyingSelection) {
		return;
	}

	// acquireForNode: the application need not install the system first
	auto system = SelectionSystem::acquireForNode(this);
	if (!system) {
		return;
	}

	if (_selectedId == ItemId(0)) {
		// Only if it is still ours; another container may have taken it
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

Node *TreeView::resolveSelectionNode(const SelectionItem &item) const {
	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].node.get() == item.ref.get() && _rows[i].offset == item.index) {
			return getRowNode(i);
		}
	}
	return nullptr;
}

void TreeView::handleSelectionChanged(SpanView<SelectionItem> items) {
	// Applying the system's change, so publishSelection() must not echo it
	_applyingSelection = true;

	if (items.empty()) {
		setSelectedIdentity(ItemId(0), 0);
	} else if (auto node = dynamic_cast<ModelNode *>(items.front().ref.get())) {
		setSelectedIdentity(node->getId(), items.front().index);
	}

	_applyingSelection = false;
}

/* Forget span answers so the next model pass asks again. Explicit nodes read their payload from
the model; span answers come from outside it and the model cannot tell when they are stale. */
void TreeView::dropSpanData() {
	for (auto &it : _rows) {
		if (it.node && it.node->isSpan()) {
			it.dataLoaded = false;
		}
	}
}

void TreeView::invalidateSource() {
	dropSpanData();
	refresh();
	requestRebuildNodes(true);
}

void TreeView::handleSourceDirty(SubscriptionFlags flags) {
	// Not forced: changed rows fail their RowKey revision match. Span answers are dropped on a
	// structure change (see dropSpanData()).
	if (flags.hasFlag(Model::Update::Structure)) {
		dropSpanData();
	}

	refresh();
}

void TreeView::refresh() {
	// An inline lazy-children completion lands here during expandRow() or appendChildRows();
	// skipped, since the caller refreshes or picks the children up itself.
	if (_deferRefresh > 0) {
		return;
	}

	rebuildModel();
	requestRowData();
	requestRebuildNodes();
}

void TreeView::rebuildModel() {
	// Carry span payloads across the rebuild, keyed by (span ItemId, offset).
	Map<Model::Position, Value> loaded;
	for (auto &it : _rows) {
		if (it.dataLoaded && it.node && it.node->isSpan()) {
			loaded.emplace(Model::Position{it.node->getId(), it.offset}, sp::move(it.spanData));
		}
	}

	_rows.clear();

	auto source = getSource();
	if (!source) {
		return;
	}

	auto root = source->getRoot();
	if (_rootVisible) {
		const auto expanded = isExpanded(root);

		Row row;
		row.node = root;
		row.depth = 0;
		row.expanded = expanded;
		row.revision = root->getRevision();
		row.dataLoaded = true; // the model holds it
		_rows.emplace_back(sp::move(row));

		if (expanded) {
			appendChildRows(root, 1, loaded);
		}
	} else {
		appendChildRows(root, 0, loaded);
	}
}

void TreeView::appendChildRows(ModelNode *cat, uint32_t depth, Map<Model::Position, Value> &loaded) {
	// One pass over the children, in model order, whatever their kinds.
	for (auto &child : cat->getChildren()) {
		if (child->isSpan()) {
			// A span contributes one row per offset; its rows cannot be expanded or moved.
			const auto count = child->getSpanCount();
			for (uint64_t i = 0; i < count; ++i) {
				Row row;
				row.node = child;
				row.offset = i;
				row.depth = depth;
				row.revision = child->getRevision();

				auto it = loaded.find(Model::Position{child->getId(), i});
				if (it != loaded.end()) {
					row.spanData = sp::move(it->second);
					row.dataLoaded = true;
					loaded.erase(it);
				}

				_rows.emplace_back(sp::move(row));
			}
			continue;
		}

		const auto expanded = child->isCategory() && isExpanded(child);

		Row row;
		row.node = child;
		row.depth = depth;
		row.expanded = expanded;
		row.revision = child->getRevision();
		row.dataLoaded = true;
		_rows.emplace_back(sp::move(row));

		if (expanded) {
			/* An open category back in Pending (e.g. after resetChilds()) requests children again.
			_deferRefresh blocks re-entering refresh() from an inline answer; the recursion below
			picks those children up, and a later answer arrives through the listener. */
			if (child->getChildsState() == Model::ChildsState::Pending) {
				++_deferRefresh;
				child->requestChilds(nullptr);
				--_deferRefresh;
			}

			appendChildRows(child, depth + 1, loaded);
		}
	}
}

void TreeView::requestRowData() {
	// Suppresses the rebuild a synchronous delivery would schedule; refresh() schedules one anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		// Only unfetched span rows need a request.
		if (_rows[i].dataLoaded || !_rows[i].node || !_rows[i].node->isSpan()) {
			++i;
			continue;
		}

		Rc<TreeView> self(this);
		Rc<ModelNode> span = _rows[i].node;

		// One request per run of consecutive unloaded offsets of the same span.
		const auto first = _rows[i].offset;
		size_t count = 1;
		while (i + count < _rows.size()) {
			auto &next = _rows[i + count];
			if (next.node != span || next.dataLoaded || next.offset != first + count) {
				break;
			}
			++count;
		}

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

void TreeView::handleSliceData(ModelNode *span, uint64_t first, size_t count,
		Map<uint64_t, Value> &data) {
	bool updated = false;

	for (auto &it : _rows) {
		if (it.node != span || it.offset < first || it.offset >= first + count) {
			continue;
		}

		auto iit = data.find(it.offset);
		if (iit != data.end()) {
			it.spanData = sp::move(iit->second);
		}

		// Loaded even without an answer, so an under-delivering source is not asked again.
		it.dataLoaded = true;
		updated = true;
	}

	if (updated && !_inDataRequest) {
		requestRebuildNodes();
	}
}

bool TreeView::moveRow(size_t index, ModelNode *dstParent, size_t childIndex) {
	auto source = getSource();
	if (!source || index >= _rows.size()) {
		return false;
	}

	// A span row is not an element and cannot be moved.
	auto &row = _rows[index];
	if (!row.node || row.node->isSpan()) {
		return false;
	}

	// The model decides whether the move is allowed.
	return source->moveNode(row.node, dstParent, childIndex);
}

void TreeView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it, so a coalesced forced request still forces.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
	// The rebuild runs in the components phase, which is opt-in per visit.
	markComponentsDirty();
}

void TreeView::requestRebuildNodes(Function<void()> &&cb, bool force) {
	if (cb) {
		_rebuildCallbacks.emplace_back(sp::move(cb));
	}
	requestRebuildNodes(force);
}

void TreeView::remapSelection() {
	if (_selectedId == ItemId(0)) {
		_selectedRow = maxOf<size_t>();
		return;
	}

	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].getId() == _selectedId && _rows[i].offset == _selectedOffset) {
			_selectedRow = i;
			return;
		}
	}

	/* No row shows this identity (hidden or removed). Only the index is dropped: the identity lets
	re-expanding restore the selection, and a removed ItemId is never reused. The selection is
	never moved to a neighbouring row. */
	_selectedRow = maxOf<size_t>();
}

void TreeView::rebuildRows() {
	if (!_controller) {
		return;
	}

	// Before the nodes are made: makeRow() reads _selectedRow
	remapSelection();

	// Keep live row nodes alive across the rebuild until makeRow() claims them by key.
	const auto force = _forceRebuild;
	_forceRebuild = false;

	if (!force) {
		for (auto &it : _controller->getItems()) {
			// A row replaced via RowBuilder::setNode is not a RowNode and is always rebuilt.
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

	// Unclaimed nodes belonged to rows that are gone or changed.
	_reusableRows.clear();

	/* New rows are already laid out here (Node::runPendingPhases, commitChanges). Callbacks are
	taken off the list before they run, so a new request is served by the next rebuild. */
	auto callbacks = sp::move(_rebuildCallbacks);
	_rebuildCallbacks.clear();
	for (auto &it : callbacks) { it(); }
}

auto TreeView::makeRowKey(const Row &row) -> RowKey {
	RowKey key;
	key.node = row.node;
	key.offset = row.offset;
	key.depth = row.depth;
	key.revision = row.revision;
	key.height = row.height;
	key.expanded = row.expanded;
	key.dataLoaded = row.dataLoaded;
	return key;
}

auto TreeView::takeReusableRow(size_t index) -> Rc<RowNode> {
	if (_reusableRows.empty()) {
		return nullptr;
	}

	// Linear: the vector holds only the rows that fit the viewport.
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

auto TreeView::getRowNode(size_t index) const -> RowNode * {
	if (!_controller) {
		return nullptr;
	}

	// Const: the non-const getItems() marks the controller's layout info dirty.
	const basic2d::ScrollController *controller = _controller;
	auto &items = controller->getItems();
	if (index >= items.size()) {
		return nullptr;
	}

	return dynamic_cast<RowNode *>(items[index].node);
}

void TreeView::updateRowNode(RowNode *node, size_t index) {
	const auto selected = (index == _selectedRow);

	if (selected) {
		node->addStyleClass("selected");
	} else {
		node->removeStyleClass("selected");
	}

	/* The scene-wide `:selected` state, distinct from the `.selected` class (which every view
	writes, owned or not). Applied per node, since virtualized row nodes are recycled. */
	if (_selectionOwned) {
		setNodeSelected(node, selected);
	}
}

Rc<Node> TreeView::makeRow(size_t index) {
	if (index >= _rows.size()) {
		return nullptr;
	}

	if (auto node = takeReusableRow(index)) {
		// The same row; only its index, used by the expander and tap routing, may have moved.
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

Rc<Node> TreeView::buildRowNode(RowBuilder &builder) {
	const auto &row = *builder._row;
	const auto index = builder._index;

	Rc<Node> node = builder._node;

	if (!node) {
		auto rowNode = Rc<RowNode>::create(this, index, _selectionEnabled);

		// What this node was built from, for reuse by a later rebuild.
		rowNode->setRowKey(makeRowKey(row));

		rowNode->addStyleClass(
				row.isCategory() ? (row.expanded ? "expanded" : "collapsed") : "leaf");
		if (!row.dataLoaded) {
			rowNode->addStyleClass("loading");
		}
		if (index == _selectedRow) {
			rowNode->addStyleClass("selected");
		}

		// The expander column. A leaf keeps the slot empty rather than closing the gap, so names
		// stay on one column whatever their neighbours are.
		if (builder._expander) {
			rowNode->setExpanderNode(rowNode->addChild(builder._expander, ZOrder(1)));
		} else if (builder._expanderVisible) {
			if (row.isCategory()) {
				// The index is read from the row node, not captured: a reused node can move.
				auto toggle = rowNode->addChild(Rc<Button>::create([this, owner = rowNode.get()] {
					toggleRow(owner->getRowIndex());
				}),
						ZOrder(1));
				toggle->addStyleClass("tree-toggle");
				toggle->setIcon(row.expanded ? builder._iconExpanded : builder._iconCollapsed);
				rowNode->setExpanderNode(toggle);
			} else {
				auto spacer = rowNode->addChild(Rc<Node>::create(), ZOrder(1));
				spacer->addStyleClass("tree-toggle");
			}
		}

		if (builder._iconNode) {
			rowNode->addChild(builder._iconNode, ZOrder(2));
		} else if (builder._icon != IconName::None) {
			auto icon =
					rowNode->addChild(Rc<basic2d::IconSprite>::create(builder._icon), ZOrder(2));
			icon->setType("icon");
			icon->addStyleClass("tree-icon");
		}

		// Remembered either way for getRowContentRect
		if (builder._content) {
			rowNode->setContentNode(rowNode->addChild(builder._content, ZOrder(3)));
		} else {
			auto label = rowNode->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
			label->setType("label");
			label->addStyleClass("tree-label");
			label->setString(builder._hasLabel ? StringView(builder._label)
											   : StringView(row.getData().getString(_labelKey)));
			rowNode->setContentNode(label);
		}

		ZOrder z(4);
		for (auto &it : builder._trailing) { rowNode->addChild(it, z++); }

		node = rowNode;
	}

	for (auto &it : builder._classes) { node->addStyleClass(it); }

	if (!builder._name.empty()) {
		node->setName(builder._name);
	}

	// Per-row depth and height for the stylesheet, which computes the indent and box with calc().
	setStyleVariable(node, "--tree-depth", mem_std::toString(row.depth));
	setStyleVariable(node, "--tree-row-h", mem_std::toString(row.height, "px"));

	return node;
}

// ---- geometry -----------------------------------------------------------------------------------

RowGeometrySource TreeView::makeGeometrySource() const {
	return RowGeometrySource{this, _scroll, _controller};
}

bool TreeView::getRowRect(size_t index, Rect &out) const {
	return ui::getRowRect(makeGeometrySource(), index, out);
}

bool TreeView::getRowContentRect(size_t index, Rect &out) const {
	if (!getRowRect(index, out)) {
		return false;
	}

	auto node = getRowNode(index);
	auto content = node ? node->getContentNode() : nullptr;
	if (!content) {
		// The row is not materialized; report the whole row
		return true;
	}

	// All four corners, so no assumption about rotation is made
	const auto size = content->getContentSize();
	const Vec2 corners[4] = {
		convertToNodeSpace(content->convertToWorldSpace(Vec2::ZERO)),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(size.width, 0.0f))),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(0.0f, size.height))),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(size.width, size.height))),
	};

	float left = corners[0].x;
	for (uint32_t i = 1; i < 4; ++i) { left = sprt::min(left, corners[i].x); }

	// The right edge stays the row's, not the label's
	const float right = out.origin.x + out.size.width;
	out.origin.x = sprt::min(left, right);
	out.size.width = sprt::max(right - out.origin.x, 0.0f);
	return true;
}

size_t TreeView::getRowIndexAt(const Vec2 &nodeLocation) const {
	return ui::getRowIndexAt(makeGeometrySource(), nodeLocation);
}

// ---- dropping into the tree ---------------------------------------------------------------------

auto TreeView::getDropPositionForRow(size_t index, float offset) const -> DropPosition {
	auto model = getSource();
	if (!model) {
		return DropPosition();
	}

	auto root = model->getRoot();

	// No row: the empty space below the last one appends to the root (also for an empty tree).
	auto row = getRow(index);
	if (!row || !row->node) {
		return DropPosition{DropPosition::Kind::Into, maxOf<size_t>(), root, maxOf<size_t>()};
	}

	const bool category = row->isCategory();

	// A category's middle means "into it", its end bands "beside it"; a leaf has only halves.
	if (category && offset >= CategoryDropBand && offset < 1.0f - CategoryDropBand) {
		return DropPosition{DropPosition::Kind::Into, index, row->node, maxOf<size_t>()};
	}

	// A span row positions relative to the span node among its siblings.
	auto parent = row->node->getParent();
	if (!parent) {
		// the visible root (setRootVisible) has no parent, so its whole row means "into it"
		return DropPosition{DropPosition::Kind::Into, index, root, maxOf<size_t>()};
	}

	// Which side: a leaf splits at its middle, and a category here is already in an end band.
	const bool after = (offset >= 0.5f);
	const auto child = row->node->getChildIndex();
	return after ? DropPosition{DropPosition::Kind::After, index, parent, child + 1}
				 : DropPosition{DropPosition::Kind::Before, index, parent, child};
}

auto TreeView::getDropPositionAt(const Vec2 &nodeLocation) const -> DropPosition {
	const auto index = getRowIndexAt(nodeLocation);

	// How far down the row the pointer is, from its top edge (Y points up).
	float offset = 0.5f;
	Rect rect;
	if (index != maxOf<size_t>() && getRowRect(index, rect) && rect.size.height > 0.0f) {
		offset = (rect.origin.y + rect.size.height - nodeLocation.y) / rect.size.height;
	}

	return getDropPositionForRow(index, offset);
}

float TreeView::getRowIndentX(size_t index) const {
	auto node = getRowNode(index);
	if (!node) {
		return nan();
	}

	// The leftmost visible child by box, not the first by order.
	float left = nan();
	for (auto &child : node->getChildren()) {
		if (!child->isEffectivelyVisible()) {
			continue;
		}
		const auto x = child->getBoundingBox().origin.x;
		left = sprt::isnan(left) ? x : sprt::min(left, x);
	}

	if (sprt::isnan(left)) {
		return nan();
	}

	// Through world space: rows live under a moving ScrollView root.
	return convertToNodeSpace(node->convertToWorldSpace(Vec2(left, 0.0f))).x;
}

bool TreeView::getDropPositionRect(const DropPosition &pos, Rect &out) const {
	if (pos.row == maxOf<size_t>()) {
		return false; // the root has no rectangle of its own; the whole view stands for it
	}

	Rect rect;
	switch (pos.kind) {
	case DropPosition::Kind::None: return false;

	case DropPosition::Kind::Into:
		if (!getRowRect(pos.row, rect)) {
			return false;
		}
		break;

	// Boundaries are numbered by the row below: "before r" is r, "after r" is r + 1.
	case DropPosition::Kind::Before:
		if (!ui::getRowBoundaryRect(makeGeometrySource(), pos.row, rect, InsertionLineThickness)) {
			return false;
		}
		break;
	case DropPosition::Kind::After:
		if (!ui::getRowBoundaryRect(makeGeometrySource(), pos.row + 1, rect,
					InsertionLineThickness)) {
			return false;
		}
		break;
	}

	// Cut back to the anchor row's indent; without a row node the indicator spans the full width.
	const auto left = getRowIndentX(pos.row);
	if (!sprt::isnan(left) && left > rect.origin.x && left < rect.getMaxX()) {
		rect.size.width -= left - rect.origin.x;
		rect.origin.x = left;
	}

	out = rect;
	return true;
}

void TreeView::setDropSlots(DropSlots &&slots) {
	_dropSlots = sp::move(slots);
	setDropEnabled(true);
}

void TreeView::setDropEnabled(bool value) {
	if (_dropEnabled == value) {
		return;
	}
	_dropEnabled = value;
	updateDropSystems();
}

void TreeView::setDropExpandDelay(TimeInterval value) {
	_dropExpandDelay = value;
	// A running dwell used the old delay; the next hover arms the new one.
	cancelDropExpand();
}

void TreeView::updateDropSystems() {
	if (!_dropEnabled) {
		clearDropPosition();
		if (_hasDropTarget) {
			removeDropTarget(this);
			_hasDropTarget = false;
		}
		return;
	}

	if (_hasDropTarget) {
		return;
	}

	_hasDropTarget = true;
	setDropTarget(this,
			DropTargetSlots{
				.accept = [this](const DragEvent &event) -> DragResponse {
		if (!_dropSlots.accept) {
			return DragResponse(); // no slots: the view does not accept
		}
		// Pure: runs during hit testing, so nothing here changes state.
		return DragResponse{_dropSlots.accept(event, getDropPositionAt(event.location))};
	},
				.enter = [this](const DragEvent &event) { updateDropPosition(event); },
				.over = [this](const DragEvent &event) { updateDropPosition(event); },
				.leave = [this](const DragEvent &) { clearDropPosition(); },
				.drop =
						[this](const DragEvent &event, DragActions action) {
		// Re-resolved from the event: `leave` fires before `drop` and has cleared _dropPosition.
		return _dropSlots.drop ? _dropSlots.drop(event, getDropPositionAt(event.location), action)
							   : false;
	},
			});
}

void TreeView::updateDropPosition(const DragEvent &event) {
	auto pos = getDropPositionAt(event.location);
	if (pos == _dropPosition) {
		// Same zone: nothing to redraw, and the dwell is not restarted.
		return;
	}

	_dropPosition = sp::move(pos);
	showDropFeedback();
	armDropExpand();
}

void TreeView::clearDropPosition() {
	_dropPosition = DropPosition();
	cancelDropExpand();
	hideDropFeedback();
}

void TreeView::showDropFeedback() {
	Rect rect;
	const bool into = (_dropPosition.kind == DropPosition::Kind::Into);
	const bool hasRect = getDropPositionRect(_dropPosition, rect);

	// A drop onto the root has no rectangle; the view gets the `drop-root` class for the sheet.
	if (_dropPosition.kind != DropPosition::Kind::None && _dropPosition.row == maxOf<size_t>()) {
		addStyleClass("drop-root");
	} else {
		removeStyleClass("drop-root");
	}

	if (hasRect && into) {
		if (!_dropHighlight) {
			// Translucent default paint, so an unstyled highlight does not hide the row; a rule on
			// `tree-drop-highlight` overrides it.
			_dropHighlight = addChild(Rc<basic2d::Layer>::create(Color4B(0x2E, 0x7D, 0x32, 0x80)),
					ZOrder(64));
			_dropHighlight->setType("tree-drop-highlight");
			_dropHighlight->setAnchorPoint(Anchor::BottomLeft);
		}
		_dropHighlight->setPosition(rect.origin);
		_dropHighlight->setContentSize(rect.size);
	} else if (_dropHighlight) {
		_dropHighlight->removeFromParent(true);
		_dropHighlight = nullptr;
	}

	if (hasRect && !into) {
		if (!_insertionLine) {
			_insertionLine = addChild(Rc<basic2d::Layer>::create(Color4B(0xFC, 0xB4, 0x00, 0xFF)),
					ZOrder(64));
			_insertionLine->setType("tree-insertion-line");
			_insertionLine->setAnchorPoint(Anchor::BottomLeft);

			// The upright is a child of the line, so it follows the line's left end.
			auto stem = _insertionLine->addChild(
					Rc<basic2d::Layer>::create(Color4B(0xFC, 0xB4, 0x00, 0xFF)), ZOrder(1));
			stem->setType("tree-insertion-stem");
			stem->setAnchorPoint(Anchor::MiddleLeft);
			stem->setPosition(Vec2(0.0f, InsertionLineThickness / 2.0f));
			stem->setContentSize(Size2(InsertionLineThickness, InsertionStemHeight));
		}
		_insertionLine->setPosition(rect.origin);
		_insertionLine->setContentSize(rect.size);
	} else if (_insertionLine) {
		_insertionLine->removeFromParent(true);
		_insertionLine = nullptr;
	}
}

void TreeView::hideDropFeedback() {
	removeStyleClass("drop-root");
	if (_dropHighlight) {
		_dropHighlight->removeFromParent(true);
		_dropHighlight = nullptr;
	}
	if (_insertionLine) {
		_insertionLine->removeFromParent(true);
		_insertionLine = nullptr;
	}
}

void TreeView::armDropExpand() {
	// Only a collapsed category with an Into position.
	Rc<ModelNode> candidate;
	if (_dropPosition.kind == DropPosition::Kind::Into && _dropPosition.row != maxOf<size_t>()) {
		if (auto row = getRow(_dropPosition.row); row && row->isCategory() && !row->expanded) {
			candidate = row->node;
		}
	}

	if (candidate == _dropExpandCandidate) {
		return; // already running for this category; do not restart
	}

	cancelDropExpand();

	_dropExpandCandidate = sp::move(candidate);
	if (!_dropExpandCandidate || !_dropExpandDelay) {
		return;
	}

	// Rc on the view: the action outlives the current frame.
	runAction(Rc<Sequence>::create(_dropExpandDelay,
					  [self = Rc<TreeView>(this)] { self->fireDropExpand(); }),
			DropExpandActionTag);
}

void TreeView::cancelDropExpand() {
	_dropExpandCandidate = nullptr;
	stopAllActionsByTag(DropExpandActionTag);
}

void TreeView::fireDropExpand() {
	auto candidate = sp::move(_dropExpandCandidate);
	_dropExpandCandidate = nullptr;
	if (!candidate) {
		return;
	}

	// Looked up again: rows may have been re-derived while the dwell ran.
	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].node == candidate) {
			// Expanding moves only rows below, so the current feedback stays valid.
			expandRow(i);
			return;
		}
	}
}

void TreeView::handleRowTap(size_t index, uint32_t count) {
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

	if (_selectCallback) {
		_selectCallback(index, _rows[index]);
	}
}

bool TreeView::RowBuilder::isSelected() const { return _view->getSelectedRow() == _index; }

void TreeView::RowBuilder::setNode(Rc<Node> &&node) { _node = sp::move(node); }

void TreeView::RowBuilder::setExpander(Rc<Node> &&node) { _expander = sp::move(node); }

void TreeView::RowBuilder::setExpanderIcons(IconName collapsed, IconName expanded) {
	_iconCollapsed = collapsed;
	_iconExpanded = expanded;
}

void TreeView::RowBuilder::setExpanderVisible(bool value) { _expanderVisible = value; }

void TreeView::RowBuilder::setIcon(IconName name) { _icon = name; }

void TreeView::RowBuilder::setIcon(Rc<Node> &&node) { _iconNode = sp::move(node); }

void TreeView::RowBuilder::setLabel(StringView str) {
	_label = str.str<Interface>();
	_hasLabel = true;
}

void TreeView::RowBuilder::setContent(Rc<Node> &&node) { _content = sp::move(node); }

void TreeView::RowBuilder::addTrailing(Rc<Node> &&node) { _trailing.emplace_back(sp::move(node)); }

void TreeView::RowBuilder::addStyleClass(StringView cls) {
	_classes.emplace_back(cls.str<Interface>());
}

void TreeView::RowBuilder::setName(StringView name) { _name = name.str<Interface>(); }

TreeView::RowNode::~RowNode() { }

bool TreeView::RowNode::init(TreeView *view, size_t index, bool interactive) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;
	_index = index;

	setType("tree-row");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-tree-row");
	registerStyleAppliers("tree-row");

	// No listener unless the view wants selection, so hover reaches the scroll view and expander.
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
			// Both listeners fire for a tap on the expander; the row must not also select, or a
			// select callback that toggles would cancel the expander.
			if (tap.event == GestureEvent::Activated
					&& (!_expander || !_expander->isTouched(tap.pos))) {
				_view->handleRowTap(_index, tap.count);
			}
			return true;
			// Up to two taps, so handleRowTap can tell select from activate by `count`; Immediate
			// reports each tap without waiting for the double-tap interval.
		}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 2, InputTapFlags::Immediate});
	}

	return true;
}

} // namespace stappler::xenolith::ui
