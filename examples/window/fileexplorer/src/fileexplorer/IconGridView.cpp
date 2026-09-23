/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#include "XLCommon.h" // IWYU pragma: keep

#include "fileexplorer/IconGridView.h"
#include "XLDynamicStateSystem.h"
#include "XLUiDragScrollSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

bool IconGridView::init(ThumbnailCache *cache) {
	if (!Panel::init()) {
		return false;
	}

	_cache = cache;

	setType("icon-grid-view");
	removeStyleClass("xl-ui-panel");
	// Without this a `background-color` on the new type falls through to the node tint.
	ui::Panel::registerStyleAppliers("icon-grid-view");

	// This widget places its own scroll view, and its rows place their own tiles: a stylesheet
	// must not become a second writer of either.
	setComponent<ui::SystemManagedLayout>();

	// The rows at either end of the viewport are laid out whole, so the viewport clips them.
	// ApplyForAll: ApplyForNodesBelow covers only negative z-orders, not ordinary children.
	addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll))->enableScissor();

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical), ZOrder(0));
	_scroll->setName("grid-scroll");
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	_controller = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_controller);

	/* The reflow hook. The controller calls it from its own content-size handling, and only when
	the cross-axis extent actually changed - which is exactly when the column count may have moved.
	It is also the only path that puts the user back at the same relative scroll position
	afterwards, so a resize does not throw away where they were. */
	_controller->setRebuildCallback([this](basic2d::ScrollController *) -> bool {
		updateMetrics();
		return rebuildTiles();
	});

	ui::DragScrollSystem::acquireForNode(_scroll);
	ui::useStyledScrollIndicator(_scroll);

	_sourceListener = addSystem(Rc<DataListener<Model>>::create(
			[this](SubscriptionFlags flags) { handleSourceDirty(flags); }, nullptr));

	makeDefaultCallbackSystem()->setComponentsDirtyCallback(
			[this](CallbackSystem *, const ComponentMask &) {
		if (_rebuildPending) {
			_rebuildPending = false;
			rebuildTiles();
		}
	});

	updateMetrics();

	return true;
}

void IconGridView::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	if (_scroll) {
		_scroll->setContentSize(getContentSize());
	}
}

void IconGridView::setSource(Model *source) {
	_source = source;
	if (_sourceListener) {
		_sourceListener->setSubscription(source);
	}

	_selectedIndex = maxOf<size_t>();
	rebuildModel();
	requestRebuildNodes(true);
}

void IconGridView::setIconSize(float value) {
	if (_iconSize == value) {
		return;
	}
	_iconSize = value;
	updateMetrics();
	// Forced: every tile's box changed, so no pooled row is worth keeping.
	requestRebuildNodes(true);
}

void IconGridView::setLabelHeight(float value) {
	if (_labelHeight == value) {
		return;
	}
	_labelHeight = value;
	updateMetrics();
	requestRebuildNodes(true);
}

auto IconGridView::getEntry(size_t index) const -> ModelNode * {
	return index < _entries.size() ? _entries[index].get() : nullptr;
}

size_t IconGridView::getIndexForPath(StringView path) const {
	for (size_t i = 0; i < _entries.size(); ++i) {
		if (Model::getPath(_entries[i]) == path) {
			return i;
		}
	}
	return maxOf<size_t>();
}

void IconGridView::setSelectCallback(IndexFunction &&cb) { _selectCallback = sp::move(cb); }
void IconGridView::setActivateCallback(IndexFunction &&cb) { _activateCallback = sp::move(cb); }

// --- the model --------------------------------------------------------------------------------

void IconGridView::handleSourceDirty(SubscriptionFlags flags) {
	(void)flags;
	rebuildModel();
	requestRebuildNodes();
}

void IconGridView::rebuildModel() {
	// The selected path, so a refresh that reorders the listing keeps the selection on the file
	// it was on rather than on whatever moved into its index.
	String selectedPath;
	if (_selectedIndex < _entries.size()) {
		selectedPath = Model::getPath(_entries[_selectedIndex]).str<Interface>();
	}

	_entries.clear();

	if (_source) {
		// One level: the root's children. FilesystemModel lists a directory whole, so there are
		// no span children to expand here.
		for (auto &child : _source->getRoot()->getChildren()) {
			if (!child->isSpan()) {
				_entries.emplace_back(child);
			}
		}
	}

	_selectedIndex = selectedPath.empty() ? maxOf<size_t>() : getIndexForPath(selectedPath);
	updateMetrics();
}

void IconGridView::updateMetrics() {
	_cellWidth = _iconSize + CellPadding * 2.0f;
	_cellHeight = _iconSize + CellPadding + _labelHeight;

	const float available = _scroll ? _scroll->getContentSize().width : getContentSize().width;
	if (available > 0.0f && _cellWidth > 0.0f) {
		auto fit = uint32_t((available + CellGap) / (_cellWidth + CellGap));
		_columns = sprt::max(fit, 1u);
	} else {
		_columns = 1;
	}

	_rowCount = _entries.empty() ? 0 : (_entries.size() + _columns - 1) / _columns;
}

void IconGridView::requestRebuildNodes(bool force) {
	// Sticky until the rebuild consumes it, so a coalesced forced request still forces.
	_forceRebuild = _forceRebuild || force;
	_rebuildPending = true;
	markComponentsDirty();
}

// --- the tiles --------------------------------------------------------------------------------

bool IconGridView::rebuildTiles() {
	if (!_controller) {
		return false;
	}

	const auto force = _forceRebuild;
	_forceRebuild = false;

	if (!force) {
		for (auto &it : _controller->getItems()) {
			auto row = dynamic_cast<TileRowNode *>(it.node);
			if (!row) {
				continue;
			}
			_reusableRows.emplace_back(row);
			// Detached without cleanup, which would strip the systems from a node that is about
			// to be re-attached.
			it.node->removeFromParent(false);
			it.node = nullptr;
			it.handle = nullptr;
		}
	}

	_controller->clear();

	for (size_t row = 0; row < _rowCount; ++row) {
		/* The float overload: a size, and a position chained from the previous item. No
		ScrollItemHandle is ever attached to a row node - a handle turns on the controller's
		resize path, which shifts the position of every item after the one that changed, and a
		grid has nothing to gain from it. */
		_controller->addItem([this, row](const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makeRow(row);
		}, _cellHeight + CellGap);
	}

	_controller->commitChanges();
	_reusableRows.clear();

	applySelectionToTiles();

	return true;
}

auto IconGridView::makeRowKey(size_t rowIndex) const -> RowKey {
	RowKey key;
	key.firstIndex = rowIndex * _columns;
	key.count = sprt::min(size_t(_columns), _entries.size() - sprt::min(key.firstIndex, _entries.size()));
	key.columns = _columns;
	key.cellWidth = _cellWidth;
	key.cellHeight = _cellHeight;
	key.iconSize = _iconSize;
	return key;
}

Rc<Node> IconGridView::makeRow(size_t rowIndex) {
	auto key = makeRowKey(rowIndex);

	auto row = takeReusableRow(key);
	if (!row) {
		row = Rc<TileRowNode>::create(this, _cache, key.count);
	}

	row->update(rowIndex, key);
	return row;
}

auto IconGridView::takeReusableRow(const RowKey &key) -> Rc<TileRowNode> {
	for (auto it = _reusableRows.begin(); it != _reusableRows.end(); ++it) {
		if ((*it)->getRowKey() == key) {
			auto ret = *it;
			_reusableRows.erase(it);
			return ret;
		}
	}

	// Nothing at this exact place; any row of the same shape will do, since a tile re-points
	// itself at another entry without being rebuilt.
	for (auto it = _reusableRows.begin(); it != _reusableRows.end(); ++it) {
		auto &other = (*it)->getRowKey();
		if (other.count == key.count && other.columns == key.columns
				&& other.cellWidth == key.cellWidth && other.cellHeight == key.cellHeight
				&& other.iconSize == key.iconSize) {
			auto ret = *it;
			_reusableRows.erase(it);
			return ret;
		}
	}
	return nullptr;
}

ui::RowGeometrySource IconGridView::makeGeometrySource() const {
	return ui::RowGeometrySource{this, _scroll, _controller};
}

// --- selection --------------------------------------------------------------------------------

void IconGridView::handleTileTap(size_t index, uint32_t count) {
	if (index >= _entries.size()) {
		return;
	}

	setSelectedIndex(index);

	if (count >= 2 && _activateCallback) {
		_activateCallback(index, _entries[index]);
	}
}

void IconGridView::setSelectedIndex(size_t index) {
	if (index >= _entries.size()) {
		index = maxOf<size_t>();
	}
	if (_selectedIndex == index) {
		return;
	}

	_selectedIndex = index;
	applySelectionToTiles();
	publishSelection();

	if (_selectCallback && _selectedIndex < _entries.size()) {
		_selectCallback(_selectedIndex, _entries[_selectedIndex]);
	}
}

void IconGridView::applySelectionToTiles() {
	if (!_controller) {
		return;
	}

	// The const overload: the non-const getItems() marks the controller dirty.
	const auto &controller = *_controller;
	for (auto &it : controller.getItems()) {
		auto row = dynamic_cast<TileRowNode *>(it.node);
		if (!row) {
			continue;
		}
		for (auto &tile : row->getTiles()) {
			const auto selected = tile->isVisible() && tile->getIndex() == _selectedIndex;
			tile->setSelected(selected);
			// Re-applied on every rebuild: a row node is recycled, and the marker travels with
			// the node rather than with the entry.
			setNodeSelected(tile, selected);
		}
	}
}

void IconGridView::setSelectionOwned(bool value) {
	if (_selectionOwned == value) {
		return;
	}
	_selectionOwned = value;

	setNodeSelectable(this, _selectionOwned, this);

	if (_selectionOwned) {
		publishSelection();
	} else if (auto system = SelectionSystem::findForNode(this)) {
		if (system->getOwner() == this) {
			system->clear();
		}
	}
}

SelectionItem IconGridView::makeSelectionItem(size_t index) const {
	if (index >= _entries.size()) {
		return SelectionItem();
	}
	return SelectionItem{_entries[index].get(), 0};
}

void IconGridView::publishSelection() {
	if (!_selectionOwned || _applyingSelection) {
		return;
	}

	// Nothing selected: find the system rather than acquire one. Acquiring is what a widget does
	// when it has something to say, and before this node is in a scene there is none to acquire.
	if (_selectedIndex >= _entries.size()) {
		if (auto system = SelectionSystem::findForNode(this)) {
			if (system->getOwner() == this) {
				system->clear();
			}
		}
		return;
	}

	auto system = SelectionSystem::acquireForNode(this);
	if (!system) {
		return;
	}

	auto item = makeSelectionItem(_selectedIndex);
	system->select(this, makeSpanView(&item, 1));
}

Node *IconGridView::resolveSelectionNode(const SelectionItem &item) const {
	if (!_controller) {
		return nullptr;
	}
	const auto &controller = *_controller;
	for (auto &it : controller.getItems()) {
		auto row = dynamic_cast<TileRowNode *>(it.node);
		if (!row) {
			continue;
		}
		for (auto &tile : row->getTiles()) {
			if (tile->isVisible() && tile->getNode() == item.ref.get()) {
				return tile;
			}
		}
	}
	return nullptr;
}

void IconGridView::handleSelectionChanged(SpanView<SelectionItem> items) {
	_applyingSelection = true;

	size_t index = maxOf<size_t>();
	if (!items.empty()) {
		for (size_t i = 0; i < _entries.size(); ++i) {
			if (_entries[i].get() == items.front().ref.get()) {
				index = i;
				break;
			}
		}
	}

	if (_selectedIndex != index) {
		_selectedIndex = index;
		applySelectionToTiles();
		if (_selectCallback && _selectedIndex < _entries.size()) {
			_selectCallback(_selectedIndex, _entries[_selectedIndex]);
		}
	}

	_applyingSelection = false;
}

bool IconGridView::moveSelection(SelectionDirection dir) {
	if (!_selectionOwned || _selectedIndex >= _entries.size()) {
		return false;
	}

	// The one place a grid differs from a list: a row is a step of _columns, a column a step of
	// one, and both ends of a row lead out of the widget rather than wrapping.
	size_t next = maxOf<size_t>();
	switch (dir) {
	case SelectionDirection::Left:
		if (_selectedIndex % _columns != 0) {
			next = _selectedIndex - 1;
		}
		break;
	case SelectionDirection::Right:
		if ((_selectedIndex + 1) % _columns != 0 && _selectedIndex + 1 < _entries.size()) {
			next = _selectedIndex + 1;
		}
		break;
	case SelectionDirection::Up:
		if (_selectedIndex >= _columns) {
			next = _selectedIndex - _columns;
		}
		break;
	case SelectionDirection::Down:
		if (_selectedIndex + _columns < _entries.size()) {
			next = _selectedIndex + _columns;
		}
		break;
	default: break;
	}

	if (next >= _entries.size()) {
		return false;
	}

	selectFromKeyboard(next);
	return true;
}

bool IconGridView::enterSelection(SelectionDirection dir, const Rect &fromWorld) {
	if (!_selectionOwned || _entries.empty()) {
		return false;
	}

	// getEnteringRow answers in ROWS, which is what the controller holds; the first tile of that
	// row is where a selection arriving from outside lands.
	auto row = ui::getEnteringRow(makeGeometrySource(), dir, fromWorld);
	if (row >= _rowCount) {
		return false;
	}

	selectFromKeyboard(sprt::min(row * _columns, _entries.size() - 1));
	return true;
}

void IconGridView::selectFromKeyboard(size_t index) {
	setSelectedIndex(index);
	if (_columns > 0) {
		ui::scrollRowIntoView(_scroll, _controller, index / _columns);
	}
}

// --- TileRowNode ------------------------------------------------------------------------------

bool IconGridView::TileRowNode::init(IconGridView *view, ThumbnailCache *cache, size_t tileCount) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;

	setType("file-tile-row");
	removeStyleClass("xl-ui-panel");
	ui::Panel::registerStyleAppliers("file-tile-row");
	setComponent<ui::SystemManagedLayout>();

	// A row always holds a full set of tiles; the ones past the end of the listing are hidden
	// rather than absent, so the row's shape never depends on where it is.
	// From 1, as in FileTile: a child at z 0 shares the plane its parent's background draws in.
	for (size_t i = 0; i < sprt::max(tileCount, size_t(1)); ++i) {
		auto tile = addChild(Rc<FileTile>::create(view, cache), ZOrder(int16_t(i + 1)));
		_tiles.emplace_back(tile);
	}

	return true;
}

void IconGridView::TileRowNode::update(size_t rowIndex, const RowKey &key) {
	_key = key;

	for (size_t i = 0; i < _tiles.size(); ++i) {
		const auto index = key.firstIndex + i;
		auto entry = _view ? _view->getEntry(index) : nullptr;
		if (!entry) {
			_tiles[i]->setVisible(false);
			continue;
		}
		_tiles[i]->setVisible(true);
		_tiles[i]->update(entry, index, key.iconSize);
	}

	(void)rowIndex;
	markContentSizeDirty();
}

void IconGridView::TileRowNode::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	auto size = getContentSize();
	if (size.width <= 0.0f) {
		return;
	}

	for (size_t i = 0; i < _tiles.size(); ++i) {
		_tiles[i]->setPosition(Vec2(float(i) * (_key.cellWidth + CellGap), 0.0f));
		_tiles[i]->setContentSize(Size2(_key.cellWidth, _key.cellHeight));
	}
}

} // namespace stappler::xenolith::examples
