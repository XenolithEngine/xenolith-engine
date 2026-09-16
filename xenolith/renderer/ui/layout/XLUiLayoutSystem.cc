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

// The LayoutSystem itself: component plumbing, mode dispatch and the measurement protocol. The
// placement backends live one per subunit beside this file (XLUiLayoutFlex.cc, XLUiLayoutGrid.cc,
// XLUiLayoutTable.cc); XLUiLayoutInternal.h carries what they share.

#include "XLUiLayoutInternal.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Components acquire their unique ids statically during program startup.
ComponentId FlexLayoutInfo::Id;
ComponentId FlexItemInfo::Id;
ComponentId GridLayoutInfo::Id;
ComponentId GridItemInfo::Id;
ComponentId TableLayoutInfo::Id;
ComponentId TableColumnsComponent::Id;
ComponentId TableRowInfo::Id;
ComponentId TableCellInfo::Id;
ComponentId TableBordersComponent::Id;
ComponentId OutOfFlowComponent::Id;
ComponentId OverflowComponent::Id;

// All LayoutSystems share one frame-stack tag, so a descendant's stack lookup resolves to the
// nearest ancestor container (back() of the tag's stack)
uint64_t LayoutSystem::SystemFrameTag = System::GetNextSystemId();

bool LayoutSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemPriority = LayoutDefaultPriority;

	// Lays out children, reacts to own components and descendant resizes, answers measurement.
	// AddToFrameStack + the shared tag let descendants reach the nearest container.
	setSystemFlags(SystemFlags::HandleLayoutChildren | SystemFlags::HandleComponents
			| SystemFlags::HandleSceneEvents | SystemFlags::HandleMeasure
			| SystemFlags::HandleChildNodeEvents | SystemFlags::HandleChildComponents
			| SystemFlags::AddToFrameStack);
	setFrameTag(SystemFrameTag);
	return true;
}

bool LayoutSystem::init(const FlexLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_mode = LayoutMode::Flex;
	_initialInfo = info;
	return true;
}

bool LayoutSystem::init(const GridLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_mode = LayoutMode::Grid;
	_initialGridInfo = info;
	return true;
}

bool LayoutSystem::init(const TableLayoutInfo &info) {
	if (!init()) {
		return false;
	}
	_mode = LayoutMode::Table;
	_initialTableInfo = info;
	return true;
}

bool LayoutSystem::init(LayoutMode mode) {
	if (!init()) {
		return false;
	}
	_mode = mode;
	return true;
}

// Ensures the container carries the component its mode reads. TableRow is exempt: its
// TableColumnsComponent comes from the row's owner, and an unstamped row lays nothing out.
void LayoutSystem::ensureModeComponent(Node *owner) {
	switch (_mode) {
	case LayoutMode::Grid:
		if (!owner->getComponent<GridLayoutInfo>()) {
			owner->setComponent<GridLayoutInfo>(_initialGridInfo);
		}
		break;
	case LayoutMode::Table:
		if (!owner->getComponent<TableLayoutInfo>()) {
			owner->setComponent<TableLayoutInfo>(_initialTableInfo);
		}
		break;
	case LayoutMode::TableRow: break;
	case LayoutMode::Flex:
		if (!owner->getComponent<FlexLayoutInfo>()) {
			owner->setComponent<FlexLayoutInfo>(_initialInfo);
		}
		break;
	}
}

void LayoutSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);
	ensureModeComponent(owner);
}

void LayoutSystem::handleLayoutChildren() {
	System::handleLayoutChildren();
	apply();
}

void LayoutSystem::handleComponentsDirty(const ComponentMask &mask) {
	System::handleComponentsDirty(mask);
	_owner->markLayoutChildrenDirty(); // container params changed -> re-lay-out children
}

void LayoutSystem::setMode(LayoutMode mode) {
	if (_mode == mode) {
		return;
	}
	_mode = mode;
	if (_owner) {
		ensureModeComponent(_owner);
		apply();
	}
}

const FlexLayoutInfo *LayoutSystem::getInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<FlexLayoutInfo>();
}

void LayoutSystem::setInfo(const FlexLayoutInfo &info) {
	if (!_owner) {
		_initialInfo = info;
		return;
	}
	_owner->setComponent<FlexLayoutInfo>(info);
}

void LayoutSystem::updateInfo(const Callback<bool(FlexLayoutInfo &)> &cb) {
	if (!_owner) {
		cb(_initialInfo);
		return;
	}
	_owner->setOrUpdateComponent<FlexLayoutInfo>(
			[&](NotNull<FlexLayoutInfo> info) { return cb(*info); });
}

void LayoutSystem::setDirection(FlexDirection value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.direction == value) {
			return false;
		}
		info.direction = value;
		return true;
	});
}

void LayoutSystem::setWrap(FlexWrap value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.wrap == value) {
			return false;
		}
		info.wrap = value;
		return true;
	});
}

void LayoutSystem::setJustifyContent(FlexJustify value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.justifyContent == value) {
			return false;
		}
		info.justifyContent = value;
		return true;
	});
}

void LayoutSystem::setAlignItems(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignItems == value) {
			return false;
		}
		info.alignItems = value;
		return true;
	});
}

void LayoutSystem::setAlignContent(FlexAlign value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.alignContent == value) {
			return false;
		}
		info.alignContent = value;
		return true;
	});
}

void LayoutSystem::setGap(float row, float column) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.rowGap == row && info.columnGap == column) {
			return false;
		}
		info.rowGap = row;
		info.columnGap = column;
		return true;
	});
}

void LayoutSystem::setPadding(Padding value) {
	updateInfo([&](FlexLayoutInfo &info) {
		if (info.padding == value) {
			return false;
		}
		info.padding = value;
		return true;
	});
}

bool LayoutSystem::canMeasure(NotNull<Node> node) { return LayoutSystem_canMeasure(node); }

void LayoutSystem::markItemDirty(NotNull<Node> node) {
	if (auto parent = node->getParent()) {
		parent->markLayoutChildrenDirty();
	}
}

/* Unlike markItemDirty (one level, enough for placement changes), an intrinsic size change can
stale every fit-content ancestor, so the flag goes to the root: a node cannot tell which ancestors
measured it. */
void LayoutSystem::markMeasureDirty(NotNull<Node> node) { node->markIntrinsicSizeDirty(); }

// Equality-guarded write that marks the parent only on a real change; an unguarded mark would make
// the container re-lay-out forever.
template <typename Info>
static void LayoutSystem_setItemInfo(NotNull<Node> node, const Info &info) {
	bool changed = false;
	node->setOrUpdateComponent<Info>([&](NotNull<Info> c) {
		if (*c == info) {
			return false;
		}
		*c = info;
		changed = true;
		return true;
	});
	if (changed) {
		LayoutSystem::markItemDirty(node);
	}
}

const FlexItemInfo *LayoutSystem::getItem(NotNull<Node> node) {
	return node->getComponent<FlexItemInfo>();
}

void LayoutSystem::setItem(NotNull<Node> node, const FlexItemInfo &info) {
	LayoutSystem_setItemInfo(node, info);
}

const GridLayoutInfo *LayoutSystem::getGridInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<GridLayoutInfo>();
}

void LayoutSystem::setGridInfo(const GridLayoutInfo &info) {
	if (!_owner) {
		_initialGridInfo = info;
		return;
	}
	_owner->setComponent<GridLayoutInfo>(info);
}

const GridItemInfo *LayoutSystem::getGridItem(NotNull<Node> node) {
	return node->getComponent<GridItemInfo>();
}

void LayoutSystem::setGridItem(NotNull<Node> node, const GridItemInfo &info) {
	LayoutSystem_setItemInfo(node, info);
}

const TableLayoutInfo *LayoutSystem::getTableInfo() const {
	if (!_owner) {
		return nullptr;
	}
	return _owner->getComponent<TableLayoutInfo>();
}

void LayoutSystem::setTableInfo(const TableLayoutInfo &info) {
	if (!_owner) {
		_initialTableInfo = info;
		return;
	}
	_owner->setComponent<TableLayoutInfo>(info);
}

const TableRowInfo *LayoutSystem::getTableRow(NotNull<Node> node) {
	return node->getComponent<TableRowInfo>();
}

void LayoutSystem::setTableRow(NotNull<Node> node, const TableRowInfo &info) {
	LayoutSystem_setItemInfo(node, info);
}

const TableCellInfo *LayoutSystem::getTableCell(NotNull<Node> node) {
	return node->getComponent<TableCellInfo>();
}

void LayoutSystem::setTableCell(NotNull<Node> node, const TableCellInfo &info) {
	LayoutSystem_setItemInfo(node, info);
}

void LayoutSystem::setTableColumns(NotNull<Node> node, const TableColumnsComponent &value) {
	node->setOrUpdateComponent<TableColumnsComponent>([&](NotNull<TableColumnsComponent> c) {
		// Compare all but the generation; keep the old one and bump it only on a real change, so
		// two writers never share a number for different geometry.
		TableColumnsComponent next = value;
		next.generation = c->generation;
		if (next == *c) {
			return false;
		}
		next.generation = c->generation + 1;
		*c = sp::move(next);
		return true;
	});
}

// Union of the in-flow children's boxes, for the modes whose backend does not publish an extent of
// its own. The scroll offset is added back, because the extent is defined unscrolled.
Size2 LayoutSystem::measureChildrenExtent() const {
	Size2 ret;
	for (auto &child : _owner->getChildren()) {
		if (child->getComponent<OutOfFlowComponent>()) {
			continue;
		}
		const auto box = child->getBoundingBox();
		ret.width = sprt::max(ret.width, box.getMaxX() + _scrollOffset.x);
		// engine Y grows up, CSS scroll Y grows down: the extent runs from the box's top edge
		// down to the lowest child edge
		ret.height = sprt::max(ret.height,
				_owner->getContentSize().height - box.getMinY() - _scrollOffset.y);
	}
	return ret;
}

void LayoutSystem::apply() {
	if (!_owner || _inApply) {
		return;
	}
	_inApply = true;
	LayoutSystem_settleChildren(_owner);
	switch (_mode) {
	// layoutFlex publishes _contentExtent itself, from the flow coordinates it already has;
	// the other backends do not track one, so it is recovered from the placed boxes
	case LayoutMode::Flex: layoutFlex(); break;
	case LayoutMode::Grid:
		layoutGrid();
		_contentExtent = measureChildrenExtent();
		break;
	case LayoutMode::Table:
		layoutTable();
		_contentExtent = measureChildrenExtent();
		break;
	case LayoutMode::TableRow:
		layoutTableRow();
		_contentExtent = measureChildrenExtent();
		break;
	}
	_inApply = false;
}

void LayoutSystem::setOverflowAxes(bool horizontal, bool vertical) {
	if (_overflowX == horizontal && _overflowY == vertical) {
		return;
	}
	_overflowX = horizontal;
	_overflowY = vertical;
	if (_owner) {
		_owner->markLayoutChildrenDirty();
	}
}

void LayoutSystem::setScrollOffset(Vec2 value) {
	if (_scrollOffset == value) {
		return;
	}
	_scrollOffset = value;
	for (auto &it : _placement) {
		auto node = it.first.get();
		if (node->getParent() != _owner) {
			continue; // removed or re-parented since the pass that cached this placement
		}
		const auto size = node->getContentSize();
		const auto anchor = node->getAnchorPoint();
		// CSS scroll orientation is y-down, the engine's is y-up: scrolling down moves content up
		const Vec2 bottomLeft = it.second - Vec2(_scrollOffset.x, -_scrollOffset.y);
		node->setPosition(bottomLeft + Vec2(anchor.x * size.width, anchor.y * size.height));
	}
}

bool LayoutSystem::handleMeasure(const MeasureConstraints &c, Size2 &result) {
	if (!_owner || _mode == LayoutMode::Grid) {
		// grid measurement is not implemented yet: fall back to the node's size
		return false;
	}
	result = measure(c);

	// On an overflow axis the intrinsic size is the box already given, so neither a fit-content
	// parent nor the self-measure on addChild swallows the scroll range. Consequence:
	// `height: fit-content` with `overflow-y: auto` never scrolls.
	const auto own = _owner->getContentSize();
	if (_overflowX && own.width > 0.0f) {
		result.width = own.width;
	}
	if (_overflowY && own.height > 0.0f) {
		result.height = own.height;
	}
	return true;
}

Size2 LayoutSystem::measure(const MeasureConstraints &c) {
	if (!_owner) {
		return Size2();
	}
	switch (_mode) {
	case LayoutMode::Flex: return measureFlex(c);
	case LayoutMode::Table: return measureTable(c);
	case LayoutMode::TableRow: return measureTableRow(c);
	case LayoutMode::Grid: break; // no grid measurement yet: the node's own size stands in
	}
	return _owner->getContentSize();
}

// True when a layout engine computes this node's size from its content instead of taking the
// node's own content size as given: a fit-content flex item, or a grid item left to its content
// on either axis.
static bool LayoutSystem_isContentSized(Node *node) {
	if (auto item = node->getComponent<FlexItemInfo>()) {
		if (item->basis == FlexItemInfo::FitContent
				|| item->crossSize == FlexItemInfo::FitContent) {
			return true;
		}
	}
	if (auto item = node->getComponent<GridItemInfo>()) {
		return item->width == GridItemInfo::Auto || item->height == GridItemInfo::Auto;
	}
	return false;
}

// Re-arms ancestor containers whose measured size depends on this subtree. The content-size bubble
// stops at a fit-content item (its ContentSize is set by its parent, so re-layout leaves it
// unchanged); walk out explicitly until a container not sized by its content.
static void LayoutSystem_invalidateMeasuredAncestors(Node *node) {
	while (LayoutSystem_isContentSized(node)) {
		auto parent = node->getParent();
		if (!parent) {
			return;
		}
		parent->markLayoutChildrenDirty();
		node = parent;
	}
}

void LayoutSystem::handleChildContentSizeDirty(Node *child) {
	if (_inApply || !_owner) {
		return;
	}

	if (child->getParent() == _owner) {
		// coalesced: any number of child changes per frame ends up as a single
		// re-layout through the layout-children phase on the next visit; convergence
		// is guaranteed by setContentSize's equal-size early-out.
		_owner->markLayoutChildrenDirty();
		LayoutSystem_invalidateMeasuredAncestors(_owner);
	}
}

void LayoutSystem::handleChildComponentsDirty(Node *child, const ComponentMask &mask) {
	if (_inApply || !_owner) {
		return;
	}

	// a direct child flipped between display:none and displayed (VisibilityComponent
	// written or removed) - the set of in-flow items changes, re-lay-out the container
	if (child->getParent() == _owner && mask.contains(VisibilityComponent::Id.value)) {
		_owner->markLayoutChildrenDirty();
		// the item set changed, so our own measured size did too
		LayoutSystem_invalidateMeasuredAncestors(_owner);
	}
}

Size2 LayoutSystem::measureNode(Node *node, const MeasureConstraints &c) {
	// the node answers with what its style asks for, not with what it was carrying: the systems
	// iterated below are themselves installed by the style (a nested flex container's LayoutSystem
	// among them), so this has to happen before the list is read
	node->settleForMeasure();

	// copy the list - a handler may mutate the node's systems while we iterate
	auto span = node->getSystems();
	Vector<Rc<System>> tmpSystems(span.begin(), span.end());
	for (auto &it : tmpSystems) {
		if (it->isEnabled() && hasFlag(it->getSystemFlags(), SystemFlags::HandleMeasure)) {
			Size2 result;
			if (it->handleMeasure(c, result)) {
				return result;
			}
		}
	}
	// fallback for a node with no measuring system: its intrinsic size - an explicit per-axis size
	// from a MeasureComponent (published by the style resolver) merged over its current ContentSize
	// (an axis left unspecified, value < 0, keeps the current size). Mirrors intrinsicSize() above.
	Size2 cs = node->getContentSize();
	if (auto mc = node->getComponent<MeasureComponent>()) {
		const Size2 req = mc->measure(c);
		if (req.width >= 0.0f) {
			cs.width = req.width;
		}
		if (req.height >= 0.0f) {
			cs.height = req.height;
		}
	}
	return cs;
}

} // namespace stappler::xenolith::ui
