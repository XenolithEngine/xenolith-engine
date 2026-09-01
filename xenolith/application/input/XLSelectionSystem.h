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

/* One item of a selection, as the OWNER names it. Opaque here on purpose: the system stores and
compares these, and only the owner ever interprets one.

TWO FIELDS RATHER THAN A BARE Rc<Ref>, because one pointer is not enough to name a row. A TreeView
row is (ModelNode, offset) - a Kind::Span node stands for getSpanCount() rows that all share the
node and differ only by their offset within it - so a selection keyed on the pointer alone could not
tell the third element of a span from the fourth. `data::Model::Node` is a Ref carrying an ItemId
allocated once and never reused, which is what makes the pair a durable identity across a rebuild.

NEVER a Node *. Rows are virtualized: the row the user selected may have scrolled out of the window
and have no node at all, while the node it used to occupy now shows a different row. The node is a
PROJECTION of the identity, recomputed every frame; the identity is what is stored. */
struct SP_PUBLIC SelectionItem {
	Rc<Ref> ref;
	uint64_t index = 0;

	bool operator==(const SelectionItem &) const = default;
	bool operator!=(const SelectionItem &) const = default;
};

/* What the scene's current selection IS, as handed to a selection callback.

`items` is a view into the system's own storage and is valid only for the duration of the call -
copy what you need to outlive it. `owner` is null for a selection made with selectNode(): a plain
node that is its own identity has no container to interpret it. */
struct SP_PUBLIC SelectionState {
	// The container that handed the items out, or null for a plain selectNode()
	SelectionOwner *owner = nullptr;

	// The container's node - the chain anchor whenever no single item is materialized, and the node
	// an undo hotkey ultimately lands on. Null exactly when the selection is empty
	Node *ownerNode = nullptr;

	// Opaque identities, meaningful only to `owner`
	SpanView<SelectionItem> items;

	bool empty() const { return ownerNode == nullptr; }
};

/* A container that can hold the scene's selection.

A PURE INTERFACE with no Ref base, mixed in the way ui::PanelHost is - the implementors are already
Nodes, and a second refcounted base would give them two counts. The system keeps an Rc to the owner
NODE for lifetime and a raw pointer to this interface beside it.

The two halves are deliberately asymmetric: resolveSelectionNode is asked EVERY FRAME and must be
cheap and side-effect-free, while handleSelectionChanged fires only on an actual change and is
allowed to restructure. */
class SP_PUBLIC SelectionOwner {
public:
	virtual ~SelectionOwner() = default;

	// The node this owner is. Must never be null while the owner holds a selection
	virtual Node *getSelectionOwnerNode() = 0;

	/* The node currently showing this item, or null when it is not materialized - scrolled out of a
	virtualized list, inside a collapsed branch, or simply not built yet.

	Called once per item per frame, so keep it to a map lookup. It must NOT build the node: a
	selection is not a reason to materialize a row the user cannot see. */
	virtual Node *resolveSelectionNode(const SelectionItem &) const = 0;

	// The selection changed. Fires on the owner that GAINED it and on the one that LOST it (with an
	// empty span), so a view can drop its own highlight without watching the system
	virtual void handleSelectionChanged(SpanView<SelectionItem>) = 0;
};

/* The scene's single selection: what the user is working ON, as opposed to where typing goes.

ONE PER SCENE, on the SceneContent, exactly like DragSystem - findForNode walks up to the nearest,
so a second one deeper in the tree would run a selection of its own for half the scene, and
handleAdded asserts that it is not nested.

WHAT "ONE SELECTION" MEANS. At any moment there is one owner and a set of items belonging to it.
Selecting into a different owner clears the previous selection wholesale - that is the whole content
of "one element, or one group of homogeneous elements": homogeneity is defined by the container, and
two containers cannot both be current.

WHY IT LIVES IN xenolith_application AND NOT IN ui::. The hotkey pass that reads the chain is in
InputDispatcher, whose module cannot see xenolith_renderer_ui. FocusGroup is the exact precedent -
it lives here even though its only real subclass, ui::FormSystem, is two layers above.

NOTHING IS DERIVED FROM FOCUS, and nothing here watches it. A selection is set by an explicit call
and by nothing else. A widget that wants "taking focus also selects me" makes that call itself, in
its own focus-in path; see the note on reentrancy below for why that terminates. The alternative -
an engine rule tying the two - is not expressible: commitStorage commits EVERY FocusGroup in the
frame, so there is no single "the focus" to be inside anything, ui::FormSystem answers `true` for
every listener while nothing is focused, and the focus swap is deferred past the visit, so a
selection reacting to it would answer one commit late, forever. */
class SP_PUBLIC SelectionSystem : public System {
public:
	static uint64_t Id;

	// Walks the parent chain to the nearest one. Use this everywhere except inside a visit
	static SelectionSystem *findForNode(Node *);

	// findForNode, and if there is none, installs one on the scene's content node - so a widget can
	// select without the application having arranged anything
	static SelectionSystem *acquireForNode(Node *);

	virtual ~SelectionSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleExit() override;

	virtual void handleVisitSelf(FrameInfo &, Node *, NodeVisitFlags) override;

	/* Make `items` the scene's selection, owned by `owner`. Replaces whatever was selected before,
	in whatever container.

	Answers false when nothing changed - the same owner with the same items - which is what makes a
	widget free to call this on every tap without checking first. An empty span clears. */
	virtual bool select(NotNull<SelectionOwner> owner, SpanView<SelectionItem> items);

	/* Select a plain node that is its own identity: a canvas object, a card, a dock panel.

	There is no owner interface, so the node is both the owner and the only item, and the chain is
	simply that node up to the root. */
	virtual bool selectNode(NotNull<Node>);

	// Drop the selection. Answers false when there was none
	virtual bool clear();

	SelectionOwner *getOwner() const { return _owner; }
	Node *getOwnerNode() const { return _ownerNode; }
	SpanView<SelectionItem> getItems() const { return _items; }
	bool empty() const { return _ownerNode == nullptr; }

	bool isSelected(const SelectionItem &) const;

	/* The node the chain is walked up from.

	The single selected item's node when there is exactly one item AND it is materialized;
	the owner's node otherwise - which covers both a row scrolled out of its window and a
	multi-item selection, where no one item may speak for the rest. Null when nothing is selected.

	This is the whole reason the OWNER is recorded rather than only the items: without it, a
	selection whose items are all scrolled away would have no chain at all, and the hotkey that is
	supposed to reach the container's undo history would reach nobody. */
	Node *getAnchorNode() const { return _anchor; }

	/* The nodes currently carrying `:selection-within`, deepest first - the anchor, then every
	ancestor up to the scene root.

	This is the chain a hotkey is offered along, and it is stored rather than re-walked precisely so
	that it stays the chain that was RETAINED even after the anchor has left the graph. */
	SpanView<Rc<Node>> getChain() const { return _chain; }

	// Fires after the state has changed and after the owners have been told, so a callback sees a
	// settled system and may select again from inside it (see the reentrancy note)
	void setSelectionCallback(Function<void(const SelectionState &)> &&);

protected:
	// Re-resolve the item nodes and the anchor against the live graph, moving the markers to match.
	// Cheap and idempotent; run on every change and once per frame
	virtual void syncProjection();

	// The single application point for a new state, with the reentrancy guard on it
	virtual bool applyState(SelectionOwner *, Node *ownerNode, SpanView<SelectionItem>);

	SelectionOwner *_owner = nullptr;

	/* Rc, like InputListenerStorage::HitTestRec's node and for a related reason:
	handleSelectionChanged is allowed to restructure the scene, and a selection callback even more
	so, so the owner may be removed while this system is in the middle of talking to it. Dropped in
	the visit as soon as it stops running, so this retains a dead node for at most one frame */
	Rc<Node> _ownerNode;

	Vector<SelectionItem> _items;

	// The materialized projection of _items, one entry per item, null where not materialized.
	// Recomputed every frame - the row a virtualized list shows for an identity changes under us
	Vector<Rc<Node>> _itemNodes;

	// The anchor the chain was built from. Read-only bookkeeping: the chain below is what the
	// markers actually follow
	Rc<Node> _anchor;

	/* The nodes currently carrying :selection-within, deepest first - what was RETAINED, kept so
	that exactly it can be released.

	Not re-derived from _anchor at release time. Between the retain and the release the anchor may
	have been detached - which is the ordinary case, not an exotic one: a virtualized row is removed
	from its parent, and only then does anything notice that the selection must move. A getParent()
	walk would stop at the detached node and strand every counter above it. See updateSelectionChain.

	It is also, unchanged, what a later increment publishes into InputListenerStorage for the hotkey
	pass to walk. */
	Vector<Rc<Node>> _chain;

	/* How many times one call to select() may be redirected by its own change callbacks before the
	system declares the owners to be fighting and stops.

	A redirect is legitimate: a widget that selects itself on focus, an owner that answers "you
	selected me" by narrowing the selection to a sub-item. Chained redirects are legitimate too, up
	to a point. What is not is a CYCLE, and the equality check alone does not catch one: it stops
	A -> A, because the second request equals the current state, but two owners that each select the
	other are never equal to the current state and would spin inside a single frame forever. So the
	loop is bounded and says so. */
	static constexpr size_t MaxRedirects = 8;

	/* Guards against a select() issued from inside a change callback re-entering this one.

	The request is remembered instead and applied once the current delivery returns, so the stack
	never nests - a callback is never running on top of another one, and no owner is ever told about
	a state that is already stale. Termination is the bound above plus the equality check in
	applyState, which drops a request equal to the current state before any callback runs. */
	bool _applying = false;
	bool _hasPending = false;
	SelectionOwner *_pendingOwner = nullptr;
	Rc<Node> _pendingOwnerNode;
	Vector<SelectionItem> _pendingItems;

	Function<void(const SelectionState &)> _callback;
};

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLSELECTIONSYSTEM_H_
