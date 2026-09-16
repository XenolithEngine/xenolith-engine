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

#ifndef XENOLITH_APPLICATION_INPUT_XLSELECTIONSYSTEM_H_
#define XENOLITH_APPLICATION_INPUT_XLSELECTIONSYSTEM_H_

#include "XLSelection.h"
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SelectionOwner;

/* One item of a selection, as the owner names it; opaque here, only the owner interprets it.
Two fields because a pointer alone cannot name a row: a TreeView row is (ModelNode, offset) within a
Kind::Span node, and data::Model::Node's never-reused ItemId makes the pair durable across rebuilds.
Never a Node *: rows are virtualized, and a node is only a per-frame projection of the identity. */
struct SP_PUBLIC SelectionItem {
	Rc<Ref> ref;
	uint64_t index = 0;

	bool operator==(const SelectionItem &) const = default;
	bool operator!=(const SelectionItem &) const = default;
};

/* The scene's current selection, as handed to a selection callback. `items` views the system's
storage and is valid only during the call. `owner` is null for a selection made by selectNode(). */
struct SP_PUBLIC SelectionState {
	// The container that handed the items out, or null for a plain selectNode()
	SelectionOwner *owner = nullptr;

	// The container's node - the chain anchor whenever no single item is materialized. Null
	// exactly when the selection is empty
	Node *ownerNode = nullptr;

	// Opaque identities, meaningful only to `owner`
	SpanView<SelectionItem> items;

	bool empty() const { return ownerNode == nullptr; }
};

/* A container that can hold the scene's selection. A pure interface without a Ref base (like
ui::PanelHost); the system keeps an Rc to the owner node and a raw pointer to this interface.
resolveSelectionNode is called every frame and must be cheap and side-effect-free;
handleSelectionChanged fires only on change and may restructure. */
class SP_PUBLIC SelectionOwner {
public:
	virtual ~SelectionOwner() = default;

	// The node this owner is. Must never be null while the owner holds a selection
	virtual Node *getSelectionOwnerNode() = 0;

	/* The node currently showing this item, or null when it is not materialized (scrolled out,
	collapsed, not built). Called once per item per frame; must not build the node. */
	virtual Node *resolveSelectionNode(const SelectionItem &) const = 0;

	// The selection changed. Fires on the owner that gained it and on the one that lost it (with
	// an empty span)
	virtual void handleSelectionChanged(SpanView<SelectionItem>) = 0;
};

/* The scene's single selection: what the user is working on, as opposed to where typing goes.

One per scene, on the SceneContent (like DragSystem); handleAdded asserts it is not nested. There is
one owner with a set of its items at a time; selecting into another owner clears the previous one.
Lives in xenolith_application because the hotkey pass in InputDispatcher reads the chain.

Not derived from focus: a selection changes only by explicit calls. A widget that selects on focus
makes the call in its own focus-in path. */
class SP_PUBLIC SelectionSystem : public System {
public:
	static uint64_t Id;

	// Walks the parent chain to the nearest one. Use this everywhere except inside a visit
	static SelectionSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node
	static SelectionSystem *acquireForNode(Node *);

	virtual ~SelectionSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleExit() override;

	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags) override;

	/* Make `items` the scene's selection, owned by `owner`, replacing any previous selection.
	Returns false when nothing changed (same owner and items). An empty span clears. */
	virtual bool select(NotNull<SelectionOwner> owner, SpanView<SelectionItem> items);

	/* Select a plain node that is its own identity (a canvas object, a card, a dock panel): the
	node is both the owner and the only item. */
	virtual bool selectNode(NotNull<Node>);

	// Drop the selection. Answers false when there was none
	virtual bool clear();

	SelectionOwner *getOwner() const { return _owner; }
	Node *getOwnerNode() const { return _ownerNode; }
	SpanView<SelectionItem> getItems() const { return _items; }
	bool empty() const { return _ownerNode == nullptr; }

	bool isSelected(const SelectionItem &) const;

	/* The node the chain is walked up from: the single selected item's node when there is exactly
	one and it is materialized, the owner's node otherwise (so scrolled-away or multi-item
	selections still reach the container). Null when nothing is selected. */
	Node *getAnchorNode() const { return _anchor; }

	/* The nodes currently carrying `:selection-within`, deepest first - the anchor, then every
	ancestor up to the scene root. Stored, so it stays valid after the anchor leaves the graph. */
	SpanView<Rc<Node>> getChain() const { return _chain; }

	// Fires after the state changed and the owners were told; a callback may select again
	// (see _applying)
	void setSelectionCallback(Function<void(const SelectionState &)> &&);

protected:
	// Re-resolve the item nodes and the anchor against the live graph, moving the markers to match.
	// Cheap and idempotent; run on every change and once per frame
	virtual void syncProjection();

	// The single application point for a new state, with the reentrancy guard on it
	virtual bool applyState(SelectionOwner *, Node *ownerNode, SpanView<SelectionItem>);

	SelectionOwner *_owner = nullptr;

	/* Rc: change callbacks may remove the owner while the system talks to it. Dropped in the visit
	once it stops running, so a dead node is retained for at most one frame */
	Rc<Node> _ownerNode;

	Vector<SelectionItem> _items;

	// The materialized projection of _items, one entry per item, null where not materialized.
	// Recomputed every frame
	Vector<Rc<Node>> _itemNodes;

	// The anchor the chain was built from; the markers follow _chain, not this
	Rc<Node> _anchor;

	/* The retained chain, deepest first, kept to release exactly it: the anchor may be detached by
	release time (see updateSelectionChain). Also published into InputListenerStorage for the
	hotkey pass. */
	Vector<Rc<Node>> _chain;

	/* How many times one select() may be redirected by its own change callbacks. Redirects are
	legitimate, but two owners selecting each other never match the current state, so the loop
	is bounded. */
	static constexpr size_t MaxRedirects = 8;

	/* Reentrancy guard: a select() from inside a change callback is stored and applied after the
	current delivery returns, so callbacks never nest. Terminates by MaxRedirects and the equality
	check in applyState. */
	bool _applying = false;
	bool _hasPending = false;
	SelectionOwner *_pendingOwner = nullptr;
	Rc<Node> _pendingOwnerNode;
	Vector<SelectionItem> _pendingItems;

	Function<void(const SelectionState &)> _callback;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLSELECTIONSYSTEM_H_
