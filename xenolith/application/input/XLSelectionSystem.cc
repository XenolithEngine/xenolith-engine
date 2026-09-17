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

#include "XLSelectionSystem.h"
#include "XLInputDispatcher.h" // the storage the chain is published into
#include "XLScene.h"
#include "XLSceneContent.h"
#include "XLInputListener.h"
#include "XLHotkey.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t SelectionSystem::Id = System::GetNextSystemId();

bool getSelectionDirectionScore(SelectionDirection dir, const Rect &from, const Rect &to,
		double &score) {
	// Shared edges of adjacent nodes are rounded differently on either side
	static constexpr float Tolerance = 1.0f;

	float major = 0.0f;
	float minor = 0.0f;
	bool inBeam = false;

	// World space is Y-up: Up is towards larger y
	switch (dir) {
	case SelectionDirection::Left:
		if (to.getMaxX() > from.getMinX() + Tolerance) {
			return false;
		}
		major = from.getMinX() - to.getMaxX();
		break;
	case SelectionDirection::Right:
		if (to.getMinX() < from.getMaxX() - Tolerance) {
			return false;
		}
		major = to.getMinX() - from.getMaxX();
		break;
	case SelectionDirection::Up:
		if (to.getMinY() < from.getMaxY() - Tolerance) {
			return false;
		}
		major = to.getMinY() - from.getMaxY();
		break;
	case SelectionDirection::Down:
		if (to.getMaxY() > from.getMinY() + Tolerance) {
			return false;
		}
		major = from.getMinY() - to.getMaxY();
		break;
	}

	if (dir == SelectionDirection::Left || dir == SelectionDirection::Right) {
		inBeam = to.getMaxY() > from.getMinY() && to.getMinY() < from.getMaxY();
		minor = to.getMidY() - from.getMidY();
	} else {
		inBeam = to.getMaxX() > from.getMinX() && to.getMinX() < from.getMaxX();
		minor = to.getMidX() - from.getMidX();
	}

	major = sprt::max(major, 0.0f);

	static constexpr double OutOfBeam = 1.0e15;
	score = (inBeam ? 0.0 : OutOfBeam) + 13.0 * double(major) * double(major)
			+ double(minor) * double(minor);
	return true;
}

SelectionSystem *SelectionSystem::findForNode(Node *node) {
	while (node) {
		if (auto sel = node->getSystemByType<SelectionSystem>()) {
			return sel;
		}
		node = node->getParent();
	}
	return nullptr;
}

SelectionSystem *SelectionSystem::acquireForNode(Node *node) {
	if (auto sel = findForNode(node)) {
		return sel;
	}

	// None installed: create one on the scene content
	if (node) {
		if (auto scene = node->getScene()) {
			if (auto content = scene->getContent()) {
				return content->addSystem(Rc<SelectionSystem>::create());
			}
		}
	}

	log::source().warn("SelectionSystem",
			"acquireForNode: the node is not in a scene with a content node");
	return nullptr;
}

bool SelectionSystem::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = SelectionSystem::Id;

	// HandleVisitSelf re-resolves the projection against the frame's graph; the rest is lifetime
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitSelf;
	return true;
}

void SelectionSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// findForNode returns the nearest system, so a nested one would split the scene's selection
	sprt_passert(findForNode(owner->getParent()) == nullptr, "SelectionSystem must not be nested");

	_listener = Rc<InputListener>::create();

	auto &hotkeys = EngineHotkeys::get();
	auto onArrow = [this](HotkeyId id, const InputEvent &) {
		auto &hotkeys = EngineHotkeys::get();
		if (id == hotkeys.selectLeft) {
			return moveSelection(SelectionDirection::Left);
		} else if (id == hotkeys.selectRight) {
			return moveSelection(SelectionDirection::Right);
		} else if (id == hotkeys.selectUp) {
			return moveSelection(SelectionDirection::Up);
		} else if (id == hotkeys.selectDown) {
			return moveSelection(SelectionDirection::Down);
		}
		return false;
	};

	const HotkeyId ids[] = {hotkeys.selectLeft, hotkeys.selectRight, hotkeys.selectUp,
		hotkeys.selectDown};
	for (auto id : ids) {
		_listener->addHotkey(id, onArrow, HotkeyFlags::Repeatable | HotkeyFlags::Unhandled);
	}

	attachListeners();
}

void SelectionSystem::handleRemoved() {
	// System::getOwner: the node, not the SelectionOwner this class also names
	for (auto listener : {&_listener, &_pressListener}) {
		if (*listener) {
			auto node = System::getOwner();
			if (node && (*listener)->getOwner() == node) {
				node->removeSystem(listener->get());
			}
			*listener = nullptr;
		}
	}

	System::handleRemoved();
}

void SelectionSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	attachListeners();
}

void SelectionSystem::handleExit() {
	// Release the chain while parent pointers are still valid
	clear();

	System::handleExit();
}

// --- state -----------------------------------------------------------------

bool SelectionSystem::isSelected(const SelectionItem &item) const {
	return sprt::find(_items.begin(), _items.end(), item) != _items.end();
}

void SelectionSystem::setSelectionCallback(Function<void(const SelectionState &)> &&cb) {
	_callback = sp::move(cb);
}

bool SelectionSystem::select(NotNull<SelectionOwner> owner, SpanView<SelectionItem> items) {
	auto node = owner->getSelectionOwnerNode();
	if (!node) {
		log::source().error("SelectionSystem", "the owner of a selection must have a node");
		return false;
	}

	// An empty span is a clear, not an owner holding no items
	if (items.empty()) {
		return clear();
	}

	return applyState(owner, node, items);
}

bool SelectionSystem::selectNode(NotNull<Node> node) {
	// The node is the identity and its own owner, with no interface to resolve through
	SelectionItem item{Rc<Ref>(node.get()), 0};
	return applyState(nullptr, node, makeSpanView(&item, 1));
}

bool SelectionSystem::clear() { return applyState(nullptr, nullptr, SpanView<SelectionItem>()); }

void SelectionSystem::setSelectOnPress(bool value) {
	if (_selectOnPress == value) {
		return;
	}
	_selectOnPress = value;

	if (!value) {
		if (_pressListener) {
			auto node = System::getOwner();
			if (node && _pressListener->getOwner() == node) {
				node->removeSystem(_pressListener);
			}
			_pressListener = nullptr;
		}
		return;
	}

	attachListeners();
}

void SelectionSystem::attachListeners() {
	auto node = System::getOwner();
	if (!node || !node->isRunning()) {
		return;
	}

	if (_listener && !_listener->getOwner()) {
		node->addSystem(_listener);
	}

	if (!_selectOnPress) {
		return;
	}

	if (!_pressListener) {
		_pressListener = Rc<InputListener>::create(PressListenerPriority);
		_pressListener->addTouchRecognizer([this](const GestureData &data) {
			if (data.event == GestureEvent::Began) {
				handlePress(data.location());
			}
			// declined: the press goes on to the scene as if this listener were not there
			return false;
		}, InputTouchInfo{makeButtonMask({InputMouseButton::MouseLeft,
			   InputMouseButton::MouseRight, InputMouseButton::MouseMiddle})});
	}
	if (!_pressListener->getOwner()) {
		node->addSystem(_pressListener);
	}
}

static bool isAncestorOf(const Node *ancestor, const Node *node) {
	for (auto it = node; it; it = it->getParent()) {
		if (it == ancestor) {
			return true;
		}
	}
	return false;
}

Node *SelectionSystem::findPressTarget(const Vec2 &world) const {
	auto node = System::getOwner();
	auto director = node ? node->getDirector() : nullptr;
	auto dispatcher = director ? director->getInputDispatcher() : nullptr;
	if (!dispatcher) {
		return nullptr;
	}

	Node *target = nullptr;
	Node *owned = nullptr; // the topmost owner hit, while looking for its ancestor

	dispatcher->foreachHitTest(HitTestFlags::Selectable,
			[&](const InputListenerStorage::HitTestRec &rec) {
		auto selectable = getNodeSelectable(rec.node);
		if (!selectable || rec.opacity <= 0.0f || !rec.node->isRunning()
				|| !rec.contains(world)) {
			return true;
		}

		if (owned) {
			// only an ancestor of the owner stands in for it
			if (!selectable->owner && isAncestorOf(rec.node, owned)) {
				target = rec.node;
				return false;
			}
			return true;
		}

		if (selectable->owner) {
			owned = rec.node;
			return true;
		}

		target = rec.node;
		return false;
	});

	if (!target) {
		return nullptr;
	}

	// The selection already runs through the node the press would select, or through the owner
	// that will select on its own
	for (auto &it : _chain) {
		if (it.get() == target || (owned && it.get() == owned)) {
			return nullptr;
		}
	}
	return target;
}

bool SelectionSystem::selectEnclosing(NotNull<Node> node) {
	Node *target = nullptr;
	for (Node *it = node; it; it = it->getParent()) {
		auto selectable = getNodeSelectable(it);
		if (selectable && !selectable->owner) {
			target = it;
			break;
		}
	}
	if (!target) {
		return false;
	}
	for (auto &it : _chain) {
		if (it.get() == target) {
			return false;
		}
	}
	return selectNode(target);
}

void SelectionSystem::handlePress(const Vec2 &world) {
	if (auto target = findPressTarget(world)) {
		selectNode(target);
	}
}

static bool isRelatedNode(const Node *a, const Node *b) {
	for (auto node = a; node; node = node->getParent()) {
		if (node == b) {
			return true;
		}
	}
	for (auto node = b; node; node = node->getParent()) {
		if (node == a) {
			return true;
		}
	}
	return false;
}

bool SelectionSystem::moveSelection(SelectionDirection dir) {
	if (!_ownerNode) {
		return false;
	}

	// Rc: the owner's own step may deliver a change that drops the node
	Rc<Node> ownerNode = _ownerNode;

	if (_owner && _owner->moveSelection(dir)) {
		return true;
	}

	auto scene = ownerNode->getScene();
	auto director = scene ? scene->getDirector() : nullptr;
	auto dispatcher = director ? director->getInputDispatcher() : nullptr;
	if (!dispatcher) {
		return false;
	}

	// As drawn, like the candidates: both come from the frame the user saw
	Node *source = _anchor ? _anchor.get() : ownerNode.get();
	const Rect from =
			TransformRect(Rect(Vec2(0, 0), source->getContentSize()), source->getModelTransform());

	if (!_owner) {
		// A plain node (a panel) hands the step to a container inside it before looking outside
		Vector<Rc<Node>> inner;
		dispatcher->foreachHitTest(HitTestFlags::Selectable,
				[&](const InputListenerStorage::HitTestRec &rec) {
			auto selectable = getNodeSelectable(rec.node);
			if (selectable && selectable->owner && rec.opacity > 0.0f && rec.node != ownerNode
					&& isAncestorOf(ownerNode, rec.node)) {
				inner.emplace_back(rec.node);
			}
			return true;
		});
		for (auto &it : inner) {
			auto selectable = getNodeSelectable(it);
			if (selectable && selectable->owner && it->isRunning()
					&& selectable->owner->enterSelection(dir, from)) {
				return true;
			}
		}
	}

	struct Candidate {
		Rc<Node> node;
		Rect rect;
		double score;
	};

	Vector<Candidate> candidates;
	dispatcher->foreachHitTest(HitTestFlags::Selectable,
			[&](const InputListenerStorage::HitTestRec &rec) {
		if (rec.opacity <= 0.0f || isRelatedNode(rec.node, ownerNode)
				|| !getNodeSelectable(rec.node)) {
			return true;
		}

		Rect rect = rec.worldRect;
		if (rec.scissorEnabled) {
			const Rect clip(float(rec.scissor.x), float(rec.scissor.y), float(rec.scissor.width),
					float(rec.scissor.height));
			const float minX = sprt::max(rect.getMinX(), clip.getMinX());
			const float minY = sprt::max(rect.getMinY(), clip.getMinY());
			const float maxX = sprt::min(rect.getMaxX(), clip.getMaxX());
			const float maxY = sprt::min(rect.getMaxY(), clip.getMaxY());
			if (maxX <= minX || maxY <= minY) {
				return true; // clipped away entirely
			}
			rect = Rect(minX, minY, maxX - minX, maxY - minY);
		}

		double score = 0.0;
		if (getSelectionDirectionScore(dir, from, rect, score)) {
			candidates.emplace_back(Candidate{rec.node, rect, score});
		}
		return true;
	});

	sprt::sort(candidates.begin(), candidates.end(),
			[](const Candidate &l, const Candidate &r) { return l.score < r.score; });

	for (auto &it : candidates) {
		// Re-read: an owner refusing may have changed components on the way
		auto selectable = getNodeSelectable(it.node);
		if (!selectable || !it.node->isRunning()) {
			continue;
		}
		if (selectable->owner) {
			if (selectable->owner->enterSelection(dir, from)) {
				return true;
			}
		} else {
			selectNode(it.node);
			return true;
		}
	}
	return false;
}

bool SelectionSystem::applyState(SelectionOwner *owner, Node *ownerNode,
		SpanView<SelectionItem> items) {
	// Equality first, before the guard and any callback: a request equal to the current state
	// stops here, which terminates self-selection cycles
	if (_owner == owner && _ownerNode == ownerNode && _items.size() == items.size()
			&& sprt::equal(_items.begin(), _items.end(), items.begin())) {
		return false;
	}

	if (_applying) {
		// Re-entered from a change callback: the outer call applies it after the current delivery
		_hasPending = true;
		_pendingOwner = owner;
		_pendingOwnerNode = ownerNode;
		_pendingItems = Vector<SelectionItem>(items.begin(), items.end());
		return true;
	}

	_applying = true;

	size_t redirects = 0;

	do {
		_hasPending = false;

		auto prevOwner = _owner;
		auto prevOwnerNode = _ownerNode;

		// Clear markers before the state changes: syncProjection resolves against the new state
		for (auto &node : _itemNodes) {
			if (node) {
				setNodeSelected(node, false);
			}
		}
		_itemNodes.clear();

		_owner = owner;
		_ownerNode = ownerNode;
		_items = Vector<SelectionItem>(items.begin(), items.end());

		syncProjection();

		// The owner that lost the selection is told first, so highlights do not overlap
		if (prevOwner && prevOwner != _owner) {
			prevOwner->handleSelectionChanged(SpanView<SelectionItem>());
		}
		if (_owner) {
			_owner->handleSelectionChanged(_items);
		}

		if (_callback) {
			_callback(SelectionState{_owner, _ownerNode, _items});
		}

		// Requests from callbacks are applied here, in the loop, rather than by recursion
		if (_hasPending) {
			if (++redirects > MaxRedirects) {
				/* Two owners selecting each other: the bound stops it and logs; the
				selection stays at the last applied request. */
				log::source().error("SelectionSystem",
						"a selection callback keeps redirecting the selection; giving up after ",
						MaxRedirects,
						" redirects. Two owners are almost certainly selecting each other from "
						"handleSelectionChanged");
				_hasPending = false;
			} else {
				owner = _pendingOwner;
				ownerNode = _pendingOwnerNode;
				items = _pendingItems;
			}
		}

		// prevOwnerNode is released only here: prevOwner points into that node, and this Rc may be
		// its last reference while it is told it lost the selection
	} while (_hasPending);

	_pendingOwner = nullptr;
	_pendingOwnerNode = nullptr;
	_pendingItems.clear();
	_applying = false;
	return true;
}

// --- projection ------------------------------------------------------------

// The same anchor reparented (a panel dragged to another frame) needs a new chain as well
static bool isChainCurrent(SpanView<Rc<Node>> chain, Node *anchor) {
	auto node = anchor;
	for (auto &it : chain) {
		if (it.get() != node) {
			return false;
		}
		node = node->getParent();
	}
	return node == nullptr;
}

void SelectionSystem::syncProjection() {
	Node *anchor = nullptr;

	if (_ownerNode) {
		_itemNodes.resize(_items.size());

		for (size_t i = 0; i < _items.size(); ++i) {
			// No owner interface: the node is the identity (selectNode)
			Node *resolved = _owner ? _owner->resolveSelectionNode(_items[i]) : _ownerNode.get();

			auto &slot = _itemNodes[i];
			if (slot.get() == resolved) {
				continue;
			}

			// The item moved to a different node, or gained or lost one (virtualized recycling):
			// the marker follows the identity, not the node
			if (slot) {
				setNodeSelected(slot, false);
			}
			slot = resolved;
			if (resolved) {
				setNodeSelected(resolved, true);
			}
		}

		/* The anchor is the single item's node, or the owner. With several items the container
		is the anchor, so no one row's listener gets hotkeys the others do not. */
		if (_items.size() == 1 && _itemNodes.size() == 1 && _itemNodes[0]) {
			anchor = _itemNodes[0];
		} else {
			anchor = _ownerNode;
		}
	} else {
		_itemNodes.clear();
	}

	if (_anchor != anchor || (anchor && !isChainCurrent(_chain, anchor))) {
		/* Build the new chain from the live graph and release the old one from storage, never by
		re-walking: the leaving anchor may already be detached. */
		Vector<Rc<Node>> next;
		buildSelectionChain(anchor, next);

		// Retains before releasing, so a shared ancestor is not restyled
		updateSelectionChain(_chain, next);

		_chain = sp::move(next);
		_anchor = anchor;
	}
}

void SelectionSystem::handleVisitSelf(FrameInfo &frame, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(frame, node, flags);

	// acquireForNode from a descendant's handleEnter adds this system before its owner runs
	if ((_listener && !_listener->getOwner())
			|| (_selectOnPress && (!_pressListener || !_pressListener->getOwner()))) {
		attachListeners();
	}

	if (!_ownerNode) {
		return;
	}

	// The owner left the scene. Node removal is not announced to the system, so it is checked
	// once a frame; until then _ownerNode keeps the owner addressable
	if (!_ownerNode->isRunning()) {
		clear();
		return;
	}

	/* Re-resolve against the graph as this frame sees it. A row materialized later in this pass
	becomes the anchor next frame; until then the chain falls back to the owner. */
	syncProjection();

	/* Publish into the frame being built. The hotkey pass reads the chain only from the storage,
	never from this system, whose selection may have moved by dispatch time. */
	if (frame.input) {
		frame.input->setSelectionChain(_chain);
	}
}

} // namespace stappler::xenolith
