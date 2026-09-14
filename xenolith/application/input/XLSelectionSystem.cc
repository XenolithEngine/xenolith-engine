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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t SelectionSystem::Id = System::GetNextSystemId();

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

	if (_anchor != anchor) {
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
