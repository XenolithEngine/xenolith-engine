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

	// The scroll view is sized by handleContentSizeDirty(), not by the flex engine. This marker is
	// what tells the style resolver to keep out: without it a `display:flex` on `tree-view` in a
	// sheet would add a second writer of the scroll's ContentSize beside us, and the two would
	// re-dirty each other every frame.
	setComponent<SystemManagedLayout>();

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
	_scroll->setName("tree-scroll");
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	_controller = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_controller);

	// On the SCROLL, not on this node: the edge band is measured against the viewport, and a list
	// you cannot drag past the bottom of is a list you cannot drop into the part you cannot see.
	DragScrollSystem::acquireForNode(_scroll);

	// The scroll bar is built by basic2d out of nodes that can paint a fill and one radius; this
	// hands it nodes a stylesheet can paint outlines and four corners on, under the types
	// `scroll-indicator` and `scroll-indicator-track`. Done here rather than left to the
	// application because a widget of this layer is expected to answer to CSS everywhere else.
	useStyledScrollIndicator(_scroll);

	// One listener for the WHOLE model. A Model node knows its parent, so the model is the single
	// Subscription and a change anywhere in the tree arrives here — which is why nothing in this
	// widget, or in its owner, has to re-publish a branch by hand.
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

	// Lazily loaded children: inline for a directory walk, later for a fetch. The completion holds
	// an Rc because it may outlive this widget; it is not a cycle, because the node drops its
	// completion list as soon as it fires.
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

	// By default the descendants keep their entries in _expanded, so re-opening this category shows
	// the subtree exactly as it was left. Forgetting is opt-in, and it also releases the children
	// that were loaded lazily.
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
	// No early return on the index: with the row hidden, the index is already maxOf while the
	// identity is still set, so clearing has to reach setSelectedIdentity to have any effect
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

	// The index is a PROJECTION of the identity, so it is derived rather than assigned - which also
	// covers the case the caller cannot: an identity that is real but not currently a row
	remapSelection();

	const auto index = _selectedRow;
	if (previous == index) {
		return;
	}

	// The selection is a style class and nothing else, and the two rows it moves between are on
	// screen with every node they need. Asking for a rebuild here - which is what this used to do -
	// threw away and remade every visible row on a plain click, which is precisely the redraw a
	// click should not cause. A row that is not materialized needs nothing: makeRow() reads
	// _selectedRow when it builds it.
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
		// Leaving takes the scene's selection with it only if it was OURS. A view that opts out
		// while another container holds the selection must not clear that container's
		if (system->getOwner() == this) {
			system->clear();
		}
	}
}

SelectionItem TreeView::makeSelectionItem(size_t index) const {
	if (index >= _rows.size()) {
		return SelectionItem();
	}
	// The ModelNode is the identity - a Ref whose ItemId is allocated once and never reused - and
	// the offset distinguishes the rows of one span from each other
	return SelectionItem{_rows[index].node.get(), _rows[index].offset};
}

void TreeView::publishSelection() {
	if (!_selectionOwned || _applyingSelection) {
		return;
	}

	// acquireForNode rather than findForNode: a view told to own the selection should not also
	// require the application to have installed the system first
	auto system = SelectionSystem::acquireForNode(this);
	if (!system) {
		return;
	}

	if (_selectedId == ItemId(0)) {
		// Only if it is still ours. Another container may have taken it in the meantime, and
		// clearing our own selection is not a reason to clear theirs
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
	// Applying what the SYSTEM decided, so publishSelection() must not hand it straight back
	_applyingSelection = true;

	if (items.empty()) {
		setSelectedIdentity(ItemId(0), 0);
	} else if (auto node = dynamic_cast<ModelNode *>(items.front().ref.get())) {
		setSelectedIdentity(node->getId(), items.front().index);
	}

	_applyingSelection = false;
}

/* Forget what the SPANS answered, so the next model pass asks again.

Only the spans: an explicit node keeps its payload in the model, so a view cannot hold a copy that
disagrees with it — it reads through. A span answers from somewhere else entirely, and nothing about
the model says when those answers went stale, which is why saying so is a call rather than a flag.

This is all that survives of the wholesale drop a Source-backed tree needed. That one existed
because identity WAS the index: removing one item shifted every payload after it onto its
neighbour's row, permanently, since the rows were marked loaded and nothing ever asked again. An
ItemId is allocated once and never reused, so the failure it defended against cannot happen. */
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
	/* Deliberately NOT forced. A payload edit bumps the node's revision, the revision is in the
	RowKey, so exactly the rows that changed fail to match and exactly those get new nodes; every
	other visible row keeps the one it has. Forcing here — which is what a Source-backed tree had to
	do, because an index cannot tell that its contents changed — threw away and rebuilt every visible
	row on any change at all.

	A span's answers are the exception, for the reason in dropSpanData(). */
	if (flags.hasFlag(Model::Update::Structure)) {
		dropSpanData();
	}

	refresh();
}

void TreeView::refresh() {
	// An inline lazy-children completion lands here in the middle of expandRow(), before the
	// category's own children have been spliced into the model by the refresh that call makes for
	// itself. Skipping is not a lost update: the caller always refreshes once afterwards.
	if (_deferRefresh > 0) {
		return;
	}

	rebuildModel();
	requestRowData();
	requestRebuildNodes();
}

void TreeView::rebuildModel() {
	/* Carry the span payloads across the rebuild, keyed by (span, offset).

	Only spans appear here: an explicit node's Value is read out of the model when the row is drawn,
	so there is nothing to carry and nothing that could go stale. And the key is an ItemId, so it
	means the same element on the other side of the rebuild whatever happened to the tree in
	between — which is why this cannot mis-deliver a payload the way an index-keyed carry-over
	could. */
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
	/* ONE pass over the children, in the order the model holds them.

	A category, an item and a span are all just children here, and that is the whole point: the owner
	decides the order, so a leaf between two branches is expressible. A Source-backed tree could not
	say that — it emitted every subcategory first and then every item, because that is the only order
	a Source can represent. */
	for (auto &child : cat->getChildren()) {
		if (child->isSpan()) {
			// A span contributes its length in rows and no node of its own: it is a count, not an
			// element, and nothing in it can be expanded, selected as a branch, or moved.
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
			/* An OPEN category that has gone back to Pending has to ask again.

			Only expandRow() used to request children, so a category whose children were dropped
			while it was open - resetChilds() after the thing behind it changed, which is the honest
			response to "your listing is stale" - stayed visibly empty until the user collapsed and
			reopened it.

			_deferRefresh is held because a model that answers inline completes before this returns
			and would re-enter refresh() in the middle of the pass that is building _rows; the
			children it just produced are picked up by the recursion below, in this same pass. One
			that answers later dirties the model when it lands, and the listener brings us back. */
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
	// Suppresses the redundant node rebuild a synchronous delivery would otherwise schedule from
	// inside this loop: refresh() schedules one for the whole pass anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		// Everything that is not an unfetched span row already has its payload, in the model.
		if (_rows[i].dataLoaded || !_rows[i].node || !_rows[i].node->isSpan()) {
			++i;
			continue;
		}

		Rc<TreeView> self(this);
		Rc<ModelNode> span = _rows[i].node;

		// A run of consecutive offsets of the same span, asked for in one call — that is what turns
		// a fifty-thousand-row table into a handful of requests.
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
			// The span planned no request at all, so no callback is coming. Mark the range resolved
			// rather than re-ask for it on every rebuild from now on.
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

		// Marked loaded even for an offset the span did not answer for: "loaded but empty" has to be
		// terminal, or an under-delivering source would be asked again on every rebuild forever.
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

	// A span row is an offset into a length, not an element, so there is nothing to move.
	auto &row = _rows[index];
	if (!row.node || row.node->isSpan()) {
		return false;
	}

	// The model decides. It is the one that knows whether the element may move at all and what the
	// move means to whatever the element stands for outside this process.
	return source->moveNode(row.node, dstParent, childIndex);
}

void TreeView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it: a forced request coalesced into an already-pending
	// unforced one must still force, or the reuse pass would quietly ignore it.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
	// The components phase is opt-in per visit, so asking for the rebuild is also asking for the
	// phase that performs it.
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

	/* No row shows this identity right now. Only the INDEX is dropped - never the identity.

	Two situations reach here and they must not be told apart, because they need the same answer:
	the row is HIDDEN (its category collapsed, or a parent of it), or the element is GONE. Keeping
	the identity makes the first case work - re-expanding the category brings the selection back
	rather than making the user pick again - and costs nothing in the second, where a stale ItemId
	simply never matches again. It cannot come to mean a different element: an ItemId is allocated
	once and never reused, which is the property this whole mechanism rests on.

	What is NOT done here is moving the selection to whatever is nearest. That is how an operation
	silently lands on the wrong element. */
	_selectedRow = maxOf<size_t>();
}

void TreeView::rebuildRows() {
	if (!_controller) {
		return;
	}

	// BEFORE the nodes are made: makeRow() reads _selectedRow to decide whether the row it is
	// building wears the selection, so the index has to already mean the right row by then
	remapSelection();

	// Carry the live row nodes across the rebuild. A rebuild changes WHICH rows are on screen far
	// more often than WHAT any one of them shows: opening a category leaves every row above it
	// untouched and only moves the ones below, and a payload arriving changes the rows it arrived
	// for. clear() detaches the nodes; holding them here is what keeps them alive until makeRow()
	// claims one by its key - or lets it go, when the pass ends.
	const auto force = _forceRebuild;
	_forceRebuild = false;

	if (!force) {
		for (auto &it : _controller->getItems()) {
			// A row the factory took over completely (RowBuilder::setNode) is not a RowNode and
			// carries no key, so it is rebuilt like before.
			auto row = dynamic_cast<RowNode *>(it.node);
			if (!row) {
				continue;
			}

			_reusableRows.emplace_back(row);

			// Detached HERE rather than by clear(), and without the cleanup: removeFromParent()
			// defaults to stripping every system and component off the subtree, which for a node
			// that is about to be re-attached would leave a Sprite whose own scissor system is a
			// dangling pointer. Nulling the item is what keeps clear() from doing it again.
			it.node->removeFromParent(false);
			it.node = nullptr;
			it.handle = nullptr;
		}
	}

	_controller->clear();

	for (size_t i = 0; i < _rows.size(); ++i) {
		// Resolved here, once, and remembered on the Row: the factory below runs later — only when
		// the row scrolls into view — and must publish to CSS the same number the controller used
		// for the layout, or the box and the content inside it disagree.
		_rows[i].height = getRowHeight(_rows[i]);

		// `this` is captured raw on purpose: this node owns _controller, which owns this factory —
		// an Rc back would be a reference cycle. The index is safe because every change to _rows
		// goes through a full rebuild.
		_controller->addItem([this, i](const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makeRow(i);
		}, _rows[i].height);
	}

	_controller->commitChanges();

	// Whatever was not claimed belonged to a row that is gone, or to one that now looks different.
	_reusableRows.clear();

	/* The answer, delivered here and not a hop later.

	Every row this pass built was attached while the frame is in flight, so each caught up on the
	visit's phases as it was attached (Node::runPendingPhases) and commitChanges() above has placed
	it - which makes this the first moment the new rows can be measured, and therefore the last
	moment worth waiting for. Taken off the list BEFORE they run: a callback that asks for another
	rebuild is answered by that one. */
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

	// Linear: the vector holds the rows that fit the viewport, a few tens at most, and a rebuild
	// walks it once per row.
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

	// Const on purpose: the non-const getItems() marks the controller's layout info dirty, which
	// would schedule a scroll re-commit for a read.
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

	/* And the scene-wide half, which is a different claim from the class above and must stay one:
	`.selected` is "this widget's current row" and is written by every TreeView, owned or not, while
	`:selected` is "this is an item of the SCENE's selection". Two of the three widgets that write
	the class - ui::Chip, a nav tab - are not the scene selection at all.

	Applied HERE rather than once when the selection moves, because a virtualized row's node comes
	and goes while the selection stands perfectly still: the node showing row 7 a moment ago may be
	showing row 30 now. */
	if (_selectionOwned) {
		setNodeSelected(node, selected);
	}
}

Rc<Node> TreeView::makeRow(size_t index) {
	if (index >= _rows.size()) {
		return nullptr;
	}

	if (auto node = takeReusableRow(index)) {
		// The same row showing the same thing. What moved is its INDEX - a category above it opened
		// or closed - and the index is what its expander and its tap route through.
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

		// What this node was built from, so a later rebuild can tell whether it still shows the
		// right thing and hand it to whichever index that row moved to.
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
				// The index is read back from the row node rather than captured: a rebuild that
				// keeps this node moves the row it belongs to, and a captured index would then
				// toggle whatever row had landed in the old slot.
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

		// Remembered either way: it is where the name is, which is what an inline editor has to be
		// placed over (getRowContentRect)
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

	// The row's own depth and the height it was laid out with, as node-local custom properties. A
	// stylesheet rule reaches a set of nodes and so cannot carry a per-row number; this is the
	// per-element channel, and the sheet turns them into an indent and a box with calc().
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
		// The row is not on screen, so where its content starts is not knowable - the whole row is
		// the honest answer rather than a guess at the indent
		return true;
	}

	// Four corners rather than the origin: a row is not rotated today, but this is the same box the
	// drop feedback and the tooltip anchor both derive, and none of them may assume that
	const auto size = content->getContentSize();
	const Vec2 corners[4] = {
		convertToNodeSpace(content->convertToWorldSpace(Vec2::ZERO)),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(size.width, 0.0f))),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(0.0f, size.height))),
		convertToNodeSpace(content->convertToWorldSpace(Vec2(size.width, size.height))),
	};

	float left = corners[0].x;
	for (uint32_t i = 1; i < 4; ++i) { left = sprt::min(left, corners[i].x); }

	// The right edge stays the row's: an editor sized to the label's own width would be as wide as
	// the name that is being replaced, which is the one width it must not be
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

	// No row: the empty space below the last one, and everything dropped there joins the root at
	// the end. This is the only way into an EMPTY tree, which has no row to answer for it.
	auto row = getRow(index);
	if (!row || !row->node) {
		return DropPosition{DropPosition::Kind::Into, maxOf<size_t>(), root, maxOf<size_t>()};
	}

	const bool category = row->isCategory();

	// A category is a PLACE as well as a position, so the wide middle of its row is "into it" and a
	// band at each end is "beside it". A leaf is only ever a position, and its halves are its two
	// answers - there is no third thing a leaf could mean.
	if (category && offset >= CategoryDropBand && offset < 1.0f - CategoryDropBand) {
		return DropPosition{DropPosition::Kind::Into, index, row->node, maxOf<size_t>()};
	}

	// A span row has no element of its own - the items inside a span are a length, not elements -
	// but the span ITSELF is a child of its parent, so it still answers for a place among siblings.
	auto parent = row->node->getParent();
	if (!parent) {
		// the root shown as row 0 (setRootVisible) has no parent to insert beside, so the whole of
		// its row can only mean "into it"
		return DropPosition{DropPosition::Kind::Into, index, root, maxOf<size_t>()};
	}

	// Which side of the row, from the same number. One test serves both kinds: a leaf is split at
	// its middle, and a category that reached this line is already outside its middle, so which
	// half the point falls in is which band it came from.
	const bool after = (offset >= 0.5f);
	const auto child = row->node->getChildIndex();
	return after ? DropPosition{DropPosition::Kind::After, index, parent, child + 1}
				 : DropPosition{DropPosition::Kind::Before, index, parent, child};
}

auto TreeView::getDropPositionAt(const Vec2 &nodeLocation) const -> DropPosition {
	const auto index = getRowIndexAt(nodeLocation);

	// How far DOWN the row the pointer is. Y is UP here, so it counts from the row's TOP edge -
	// which is the edge "before" is on, a row's predecessor being drawn above it.
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

	// The leftmost child, not the first one: the children are ordered by ZOrder and a row built by
	// a caller may put anything anywhere, while "where the content starts" is a question about the
	// boxes rather than about the order. A row with no children at all has no indent to report.
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

	// Through the world rather than by assuming the row shares an origin with this node: the rows
	// live under a ScrollView whose root moves as it scrolls.
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

	// A boundary is a gap between two rows, numbered by the row BELOW it - so "before row r" is
	// boundary r and "after row r" is boundary r + 1, which for the last row is one past the end
	// and is answered with its bottom edge.
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

	// Cut back to the ANCHOR row's indent - pos.row either way, since that is the element the
	// position is expressed against. A row with no node reports none, and then the indicator spans
	// the width as it did before; that is the honest answer rather than a guessed indent.
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
	// Whatever is running was armed with the old delay; the next hover arms the new one.
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
			return DragResponse(); // nothing was wired up: the view is inert, not surprising
		}
		// PURE, and it has to be: this runs during hit testing. Resolving a position is a query
		// over the geometry and the model, and the caller's predicate is held to the same rule -
		// so nothing here moves, highlights or remembers.
		return DragResponse{_dropSlots.accept(event, getDropPositionAt(event.location))};
	},
				.enter = [this](const DragEvent &event) { updateDropPosition(event); },
				.over = [this](const DragEvent &event) { updateDropPosition(event); },
				.leave = [this](const DragEvent &) { clearDropPosition(); },
				.drop =
						[this](const DragEvent &event, DragActions action) {
		// Re-resolved from the event rather than read out of _dropPosition: `leave` fires BEFORE
		// `drop` and has already cleared it, which is what keeps enter/leave an exact bracket.
		return _dropSlots.drop ? _dropSlots.drop(event, getDropPositionAt(event.location), action)
							   : false;
	},
			});
}

void TreeView::updateDropPosition(const DragEvent &event) {
	auto pos = getDropPositionAt(event.location);
	if (pos == _dropPosition) {
		// The pointer moved inside the same zone. Nothing to redraw - and, crucially, nothing to
		// restart: the dwell measures time over a category, not time since the last movement.
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

	// The root - what the empty space below the last row answers for - has no rectangle of its own,
	// because it is the WHOLE view that would take the drop. A style class is how that is said, so
	// a sheet decides what it looks like; there is no node here to paint.
	if (_dropPosition.kind != DropPosition::Kind::None && _dropPosition.row == maxOf<size_t>()) {
		addStyleClass("drop-root");
	} else {
		removeStyleClass("drop-root");
	}

	if (hasRect && into) {
		if (!_dropHighlight) {
			// Translucent, and painted HERE rather than left to the sheet: an unstyled fill over a
			// row would hide the very row it is pointing at. A rule on `tree-drop-highlight`
			// overrides it.
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

			// The upright, as a CHILD of the line: the two are one indicator, so they are built,
			// moved and taken down as one thing, and the upright needs no position of its own after
			// this - the line's left end is where it belongs, and that is the origin it sits at.
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
	// Only a CLOSED category, and only where the drop would go INTO it. A drag resting between two
	// leaves is aiming at a place that already exists; there is nothing to open for it.
	Rc<ModelNode> candidate;
	if (_dropPosition.kind == DropPosition::Kind::Into && _dropPosition.row != maxOf<size_t>()) {
		if (auto row = getRow(_dropPosition.row); row && row->isCategory() && !row->expanded) {
			candidate = row->node;
		}
	}

	if (candidate == _dropExpandCandidate) {
		return; // already counting for this very category - restarting it would mean it never fires
	}

	cancelDropExpand();

	_dropExpandCandidate = sp::move(candidate);
	if (!_dropExpandCandidate || !_dropExpandDelay) {
		return;
	}

	// Rc on the view, not `this`: the ActionManager holds the action, and the run below outlives
	// any single frame of the drag.
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

	// Found again rather than remembered: the row list may have been re-derived while the dwell ran
	// - by another expansion, or by a change in the model - and the index would name another row.
	for (size_t i = 0; i < _rows.size(); ++i) {
		if (_rows[i].node == candidate) {
			// The category stays at its own index and keeps its rectangle; only rows BELOW it move,
			// so what is on screen for this position is still right and nothing is re-drawn here.
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

	// No listener at all unless the view wants selection: an always-present one would swallow the
	// hover the scroll view and the expander below it want, for a widget that ignores both.
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
			// The expander is a listener of its own over a part of this row, and nothing below
			// arbitrates between the two: both fire for a tap on the expander. Selecting on it as
			// well is wrong on its face, and in a tree whose select callback toggles the row (the
			// usual single-click tree) the two cancel out, which reads as an expander that does
			// nothing at all.
			if (tap.event == GestureEvent::Activated
					&& (!_expander || !_expander->isTouched(tap.pos))) {
				_view->handleRowTap(_index, tap.count);
			}
			return true;
			/* TWO taps, reported IMMEDIATELY. handleRowTap reads `count` to tell "the user moved
			the selection" from "the user opened this row", so a recognizer capped at one tap left
			setActivateCallback unreachable from any pointer - the count could never be anything
			but 1. Immediate is what keeps the ordinary single click free of the double-tap
			interval: tap 1 is reported the moment it happens and tap 2 when it arrives, instead of
			every click waiting to find out whether a second one follows. */
		}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 2, InputTapFlags::Immediate});
	}

	return true;
}

} // namespace stappler::xenolith::ui
