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

	// Nobody installed one. Put it where it belongs rather than making every widget demand that the
	// application arrange a selection system before anything can be selected
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

	// HandleVisitSelf is the one that matters: the projection is re-resolved there, against the
	// graph as the frame actually sees it. The rest is lifetime
	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents
			| SystemFlags::HandleVisitSelf;
	return true;
}

void SelectionSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);

	// findForNode hands a widget the NEAREST system above it, so a second one deeper in the tree
	// would run a selection of its own for half the scene - and the two would disagree about which
	// single thing is current, which is the one invariant this whole system exists to keep
	sprt_passert(findForNode(owner->getParent()) == nullptr, "SelectionSystem must not be nested");
}

void SelectionSystem::handleExit() {
	// The scene is going away and the markers go with it, but the chain is walked over live parent
	// pointers - so release it while those still lead somewhere
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

	// An empty span is a clear, not a selection with nothing in it: an owner holding no items is a
	// state nothing below could interpret, and getAnchorNode would have to answer with a container
	// the user is not working in
	if (items.empty()) {
		return clear();
	}

	return applyState(owner, node, items);
}

bool SelectionSystem::selectNode(NotNull<Node> node) {
	// The node IS the identity. It is also its own owner, with no interface: there is nothing to
	// resolve, because the projection of a node is itself
	SelectionItem item{Rc<Ref>(node.get()), 0};
	return applyState(nullptr, node, makeSpanView(&item, 1));
}

bool SelectionSystem::clear() { return applyState(nullptr, nullptr, SpanView<SelectionItem>()); }

bool SelectionSystem::applyState(SelectionOwner *owner, Node *ownerNode,
		SpanView<SelectionItem> items) {
	// Equality FIRST, before the guard and before any callback. This is what makes a cycle
	// terminate rather than merely be detected: a widget that selects itself on focus and focuses
	// itself on selection comes back here with exactly what is already set, and stops
	if (_owner == owner && _ownerNode == ownerNode && _items.size() == items.size()
			&& sprt::equal(_items.begin(), _items.end(), items.begin())) {
		return false;
	}

	if (_applying) {
		// Re-entered from inside a change callback. Remember it and let the outer call apply it
		// once the current delivery has returned, so the stack never nests and no owner is told
		// about a state that is already stale
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

		// Take the markers off what is leaving BEFORE the state changes: syncProjection below
		// resolves against the new state and would no longer know which nodes to clean
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

		// The owner that LOST it is told too, and told first - it is dropping a highlight, and
		// doing that before the new owner paints one keeps the two from overlapping for a frame
		if (prevOwner && prevOwner != _owner) {
			prevOwner->handleSelectionChanged(SpanView<SelectionItem>());
		}
		if (_owner) {
			_owner->handleSelectionChanged(_items);
		}

		if (_callback) {
			_callback(SelectionState{_owner, _ownerNode, _items});
		}

		// Anything a callback asked for is applied here, in the loop, rather than by recursion
		if (_hasPending) {
			if (++redirects > MaxRedirects) {
				/* Two owners selecting each other. Equality cannot break this - neither request
				ever equals the current state - so the bound does, and it complains rather than
				stopping quietly: the selection is left wherever the last applied request put it,
				which is a defensible state but not the one anybody asked for. */
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

		// prevOwnerNode goes out of scope HERE, and it is load-bearing rather than tidy: prevOwner
		// is an interface pointer INTO that node, and _ownerNode was overwritten above. Without
		// this Rc holding the old owner up, telling it that it lost the selection would be a
		// use-after-free in exactly the case that matters - the last reference to a view that was
		// removed in the same gesture that selected something else
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
			// No owner interface means the node IS the identity - selectNode's case, where there is
			// nothing to ask and nothing that can change under us
			Node *resolved = _owner ? _owner->resolveSelectionNode(_items[i]) : _ownerNode.get();

			auto &slot = _itemNodes[i];
			if (slot.get() == resolved) {
				continue;
			}

			// The row moved to a different node, or gained or lost one. This is the virtualized
			// case and it happens while the selection stands perfectly still: a list recycles the
			// node under a scrolling window, and the marker has to follow the identity, not the node
			if (slot) {
				setNodeSelected(slot, false);
			}
			slot = resolved;
			if (resolved) {
				setNodeSelected(resolved, true);
			}
		}

		/* The anchor is the single item's node, or the owner.

		NOT "the deepest materialized item": with more than one item selected, no single one may
		speak for the rest, and anchoring on one of them would quietly offer that row's own listener
		a hotkey the other selected rows never see - a bug that reads as "Delete only works on one
		of them". A multi-item operation belongs to the container, so the container is the anchor. */
		if (_items.size() == 1 && _itemNodes.size() == 1 && _itemNodes[0]) {
			anchor = _itemNodes[0];
		} else {
			anchor = _ownerNode;
		}
	} else {
		_itemNodes.clear();
	}

	if (_anchor != anchor) {
		/* Build the NEW chain from the live graph - correct, because this is the moment it is being
		retained - and release the OLD one from what was stored, never by re-walking. The anchor
		that is leaving may already be detached: recycling a row removes it from its parent, and
		only then does anything notice the selection has to move. */
		Vector<Rc<Node>> next;
		buildSelectionChain(anchor, next);

		// Retain-before-release lives inside this call, so a shared ancestor of the two chains
		// never loses its component and nothing under it is restyled for a move that did not leave
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

	// The owner left the scene. Nothing told us - a Node does not announce its removal to a system
	// on a distant ancestor - so this is checked where the answer is read, once a frame, instead of
	// being watched from everywhere. Until then the Rc above has kept the dead owner addressable
	if (!_ownerNode->isRunning()) {
		clear();
		return;
	}

	/* Re-resolve against the graph as this frame sees it.

	Read at SceneContent's own visit, so a row materialized LATER in this same pass is not yet the
	anchor - the chain falls back to the owner for one frame and sharpens on the next. That is
	correct rather than merely tolerable: the owner is on the chain either way, so a hotkey still
	reaches the right history; only the row's own listener joins a frame late. */
	syncProjection();

	/* Publish it into the frame being built, where the dispatcher will read it.

	The storage is the ONLY place a hotkey pass may read the chain from. Not this system: by the
	time an event is dispatched, the selection here may already have moved, and a callback offered
	the key is entitled to restructure the scene under it. The frame the user was looking at when
	they pressed the key is the one that decides. */
	if (frame.input) {
		frame.input->setSelectionChain(_chain);
	}
}

} // namespace stappler::xenolith
