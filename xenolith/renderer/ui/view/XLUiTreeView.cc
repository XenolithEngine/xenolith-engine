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
#include "XLUiButton.h"
#include "XLUiInteractiveComponent.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Move a payload that a previous model already had onto the row that carries the same identity.
// This is what keeps a rebuild from re-requesting everything that is already on screen.
static void takeLoadedData(TreeView::Row &row,
		Map<data::Source *, Map<data::Source::Id, Value>> &loaded) {
	auto catIt = loaded.find(row.source.get());
	if (catIt == loaded.end()) {
		return;
	}

	auto it = catIt->second.find(row.itemId);
	if (it == catIt->second.end()) {
		return;
	}

	row.data = sp::move(it->second);
	row.dataLoaded = true;
	catIt->second.erase(it);
}

TreeView::~TreeView() { }

bool TreeView::init() { return init(nullptr); }

bool TreeView::init(Source *source) {
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

	// The ROOT only: Source has no parent links, so a subcategory's setDirty() never reaches this
	// listener. The lazy-children path carries its own completion for that reason, and everything
	// else goes through invalidateSource().
	_sourceListener = addSystem(Rc<DataListener<Source>>::create(
			[this](SubscriptionFlags) { handleSourceDirty(); }, source));

	// The rows are (re)built HERE, at the start of the visit that will draw them, rather than from a
	// queued task. Two things fall out of that. A rebuild can destroy the row node it was asked for
	// from — an expander's tap is still on the stack when toggleRow() runs — and the visit is far
	// enough away from that. And a node attached while a frame is in flight catches up on the
	// visit's phases immediately (Node::runPendingPhases), so a row and its children are styled,
	// measured and laid out on the frame they appear rather than the one after it. A widget that is
	// not visited is not drawn either, so leaving its nodes for its next visit costs nothing.
	makeDefaultCallbackSystem()->setVisitBeginCallback([this](CallbackSystem *, FrameInfo &) {
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

void TreeView::setSource(Source *source) {
	if (getSource() == source) {
		return;
	}

	_sourceListener->setSubscription(source);
	_expanded.clear();
	_selectedRow = maxOf<size_t>();
	refresh();
}

auto TreeView::getSource() const -> Source * {
	return _sourceListener ? _sourceListener->getSubscription() : nullptr;
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
	Rc<Source> cat = _rows[index].source;
	_expanded.emplace(cat);

	// Lazily loaded children: inline for a directory walk, later for a fetch. The completion holds
	// an Rc because it may outlive this widget; it is not a cycle, because the Source drops its
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

	Rc<Source> cat = _rows[index].source;
	_expanded.erase(cat);

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

void TreeView::forgetSubtree(Source *cat) {
	for (auto &it : cat->getSubCategories()) {
		forgetSubtree(it);
		_expanded.erase(it);
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
	if (_selectedRow == index) {
		return;
	}

	const auto previous = _selectedRow;
	_selectedRow = index;

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
}

/* Both of these mean "the DATA changed", and that is exactly what a RowKey cannot see: it is
(source, itemId, depth, height, expanded, dataLoaded), so a row whose payload was replaced in place
keeps its id and its key and would be answered with the node built from the old payload. Hence the
forced rebuild - it is the whole point of Source::setDirty().

refresh() itself stays unforced: expandRow/collapseRow reach it too, and there the reuse is correct
and is what keeps an expand from redrawing the rows it did not touch. */
void TreeView::invalidateSource() {
	refresh();
	requestRebuildNodes(true);
}

void TreeView::handleSourceDirty() {
	refresh();
	requestRebuildNodes(true);
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
	// Carry the payloads that are already in hand across the rebuild, keyed by identity rather than
	// by index — an index does not survive an expand above it, an identity does.
	Map<Source *, Map<SourceId, Value>> loaded;
	for (auto &it : _rows) {
		if (!it.dataLoaded) {
			continue;
		}

		auto catIt = loaded.find(it.source.get());
		if (catIt == loaded.end()) {
			catIt = loaded.emplace(it.source.get(), Map<SourceId, Value>()).first;
		}
		catIt->second.emplace(it.itemId, sp::move(it.data));
	}

	_rows.clear();

	auto source = getSource();
	if (!source) {
		return;
	}

	if (_rootVisible) {
		const auto expanded = _expanded.find(Rc<Source>(source)) != _expanded.end();

		Row row;
		row.source = source;
		row.itemId = Source::Self;
		row.depth = 0;
		row.expanded = expanded;
		takeLoadedData(row, loaded);
		_rows.emplace_back(sp::move(row));

		if (expanded) {
			appendChildRows(source, 1, loaded);
		}
	} else {
		appendChildRows(source, 0, loaded);
	}
}

void TreeView::appendChildRows(Source *cat, uint32_t depth,
		Map<Source *, Map<SourceId, Value>> &loaded) {
	// Subcategories first, then the category's own items — the same order Source's own flattening
	// uses, so a tree and a flat list over one Source never disagree about what comes first. For a
	// filesystem source it is also, for free, "directories first, then files".
	for (auto &sub : cat->getSubCategories()) {
		const auto expanded = _expanded.find(sub) != _expanded.end();

		Row row;
		row.source = sub;
		row.itemId = Source::Self;
		row.depth = depth;
		row.expanded = expanded;
		takeLoadedData(row, loaded);
		_rows.emplace_back(sp::move(row));

		if (expanded) {
			appendChildRows(sub, depth + 1, loaded);
		}
	}

	const auto count = cat->getChildsCount();
	for (size_t i = 0; i < count; ++i) {
		Row row;
		row.source = cat;
		row.itemId = SourceId(i);
		row.depth = depth;
		takeLoadedData(row, loaded);
		_rows.emplace_back(sp::move(row));
	}
}

void TreeView::requestRowData() {
	// Suppresses the redundant node rebuild a synchronous delivery would otherwise schedule from
	// inside this loop: refresh() schedules one for the whole pass anyway.
	_inDataRequest = true;

	size_t i = 0;
	while (i < _rows.size()) {
		if (_rows[i].dataLoaded) {
			++i;
			continue;
		}

		Rc<TreeView> self(this);
		Rc<Source> cat = _rows[i].source;

		if (_rows[i].isCategory()) {
			// A category's own record. `false` means there is nothing behind it to load, which is a
			// terminal answer, not a reason to ask again on the next rebuild.
			if (!cat->getItemData([self, cat](Value &&val) {
				self->handleItemData(cat, Source::Self, sp::move(val));
			}, Source::Self)) {
				_rows[i].dataLoaded = true;
			}
			++i;
			continue;
		}

		// A run of consecutive items of the same category, asked for in one call — that is what
		// turns a ten-thousand-file directory into a single request.
		const auto first = _rows[i].itemId;
		size_t count = 1;
		while (i + count < _rows.size()) {
			auto &next = _rows[i + count];
			if (next.isCategory() || next.source != cat || next.dataLoaded
					|| next.itemId != first + SourceId(count)) {
				break;
			}
			++count;
		}

		if (cat->getSliceData(
					[self, cat, first, count](Map<SourceId, Value> &data) {
			self->handleSliceData(cat, first, count, data);
		}, first, count, 0, false)
				== 0) {
			// The source planned no request at all, so no callback is coming. Mark the range
			// resolved rather than re-ask for it on every rebuild from now on.
			for (size_t j = 0; j < count; ++j) { _rows[i + j].dataLoaded = true; }
		}

		i += count;
	}

	_inDataRequest = false;
}

void TreeView::handleItemData(Source *cat, SourceId id, Value &&val) {
	for (auto &it : _rows) {
		if (it.source == cat && it.itemId == id) {
			it.data = sp::move(val);
			it.dataLoaded = true;

			if (!_inDataRequest) {
				requestRebuildNodes();
			}
			return;
		}
	}
}

void TreeView::handleSliceData(Source *cat, SourceId first, size_t count,
		Map<SourceId, Value> &data) {
	bool updated = false;

	for (auto &it : _rows) {
		if (it.isCategory() || it.source != cat || it.itemId < first
				|| it.itemId >= first + SourceId(count)) {
			continue;
		}

		auto iit = data.find(it.itemId);
		if (iit != data.end()) {
			it.data = sp::move(iit->second);
		}

		// Marked loaded even for an index the source did not answer for: "loaded but empty" has to
		// be terminal, or an under-delivering source would be asked again on every rebuild forever.
		it.dataLoaded = true;
		updated = true;
	}

	if (updated && !_inDataRequest) {
		requestRebuildNodes();
	}
}

void TreeView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it: a forced request coalesced into an already-pending
	// unforced one must still force, or the reuse pass would quietly ignore it.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
}

void TreeView::rebuildRows() {
	if (!_controller) {
		return;
	}

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
}

auto TreeView::makeRowKey(const Row &row) -> RowKey {
	RowKey key;
	key.source = row.source;
	key.itemId = row.itemId;
	key.depth = row.depth;
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
	if (index == _selectedRow) {
		node->addStyleClass("selected");
	} else {
		node->removeStyleClass("selected");
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

		if (builder._content) {
			rowNode->addChild(builder._content, ZOrder(3));
		} else {
			auto label = rowNode->addChild(Rc<basic2d::Label>::create(), ZOrder(3));
			label->setType("label");
			label->addStyleClass("tree-label");
			label->setString(
					builder._hasLabel ? StringView(builder._label) : row.data.getString(_labelKey));
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
		}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 1});
	}

	return true;
}

} // namespace stappler::xenolith::ui
