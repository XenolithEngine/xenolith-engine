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

#ifndef STAPPLER_DATA_SPDATAMODEL_H_
#define STAPPLER_DATA_SPDATAMODEL_H_

#include "SPMemory.h" // IWYU pragma: keep
#include "SPDataValue.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::data {

/* A tree of items for a virtualized view, addressed by identity rather than by index.

WHAT IT IS FOR. A view over a Model shows rows; a row stands for something that exists outside the
process — a file, a record, an installed component. Moving or deleting a row has to become moving or
deleting that thing, and the answer may take a while and may fail. Model is the seam where those two
meet: it holds the structure, it holds an opaque `Ref` per element pointing at the external object,
and it routes every mutation through a set of slots that can veto it and perform it.

HOW IT DIFFERS FROM data::Source, and why each difference exists:

- A `Source` category addresses its items by DENSE INDEX, and its display order is fixed:
  subcategories first, then its own items. A Model category holds one `children` vector in whatever
  order the owner put them in, with categories, items and spans INTERLEAVED. An owner that wants a
  leaf between two branches simply puts it there.

- A `Source` item has no identity apart from its index, so removing one silently renames every item
  after it. Model gives every node an `ItemId` that is allocated once and never reused, so a payload,
  an expansion state or a selection keyed by it survives any structural change. That is what makes
  moving an element expressible at all.

- A `Source` IS a category AND a Subscription at the same time, so a subcategory's `setDirty()`
  reaches only whoever subscribed to that subcategory — in practice nobody, since a view watches the
  root. Here the two are split: the Model is the one Subscription, the Node is pure structure, and a
  change anywhere reaches the view by construction.

- A `Source` item is a `data::Value` and nothing else. A Model node carries a `Value` AND an opaque
  `Rc<Ref>`, and so does a category — a branch is an element like any other, with its own record and
  its own external object.

WHAT IT KEEPS. The cursor shape: a `Kind::Span` child stands for `count` items that are NOT stored,
answered in slices by a callback. That is how a table of fifty thousand database rows still costs
nothing but the rows on screen. Spans and explicit nodes live side by side in the same `children`
vector; only explicit nodes can be moved, because only they exist.

It also keeps the lazy-children state machine verbatim, including the rule that the callback is
handed `self` as a PARAMETER rather than capturing it: the callback is stored in the node, so a
captured `Rc` would be a cycle the node could never break. */
class SP_PUBLIC Model : public SubscriptionTemplate<mem_std::Interface>,
						public InterfaceObject<mem_std::Interface> {
public:
	using Interface = mem_std::Interface;
	using Value = ValueTemplate<Interface>;
	using Subscription = SubscriptionTemplate<mem_std::Interface>;

	// Allocated from a monotone counter and never reused, so a stale id is always simply "not
	// found" and can never come back meaning a different element.
	using ItemId = ValueWrapper<uint64_t, class ModelItemIdClassFlag>;

	class Node;

	enum class Kind {
		Item, // a leaf
		Category, // has children, and may load them lazily
		Span, // stands for `count` items nobody stores; answered in slices
	};

	enum class ChildsState {
		Empty, // no lazy callback: the children are whatever was set explicitly
		Pending, // there is a callback and it has not run
		Loading, // the callback ran and its completion has not fired yet
		Loaded, // the completion fired
	};

	// Subscription flag bits. Bit 0 is Subscription::Initial and must stay free. A view that only
	// wants "something changed" can keep ignoring these; one that can react cheaply to a payload
	// edit uses Data to avoid re-deriving the whole model for it.
	enum Update : uint8_t {
		Structure = 1 << 1, // a node was added, removed, moved, or a span was resized
		Data = 1 << 2, // a node's Value or object changed; the shape did not
		ChildsLoaded = 1 << 3, // a lazy category finished loading
	};

	/* What happens to the model while the external action is still running.

	Optimistic is the default because the model is what a view draws: a drag that snapped back for
	the duration of a file copy, then jumped forward again, reads as a bug. The change is applied at
	once and UNDONE if the completion reports failure.

	Confirmed is for the cases where showing a state that may not happen is worse than showing a
	delay — the model does not move until the completion says the external action succeeded. */
	enum class MutationPolicy {
		Optimistic,
		Confirmed,
	};

	/* The address of one ROW, which is not the same thing as the address of a node: a span is a
	single node standing for many rows.

	`offset` is meaningful only when `id` names a Span; for every other kind it is zero. Keeping the
	two fields apart — rather than reserving a block of consecutive ItemIds per span — is what makes
	a span resize cheap: appending ten rows to a span of five thousand leaves the identity of the
	five thousand untouched. */
	struct SP_PUBLIC Position {
		ItemId id;
		uint64_t offset = 0;

		bool operator==(const Position &) const = default;

		// Hand-written: ValueWrapper declares ==/</> individually and has NO operator<=>, so a
		// defaulted spaceship here would be defined as deleted.
		bool operator<(const Position &other) const {
			return id != other.id ? id < other.id : offset < other.offset;
		}
	};

	// Answers a slice of a span. The keys are OFFSETS WITHIN THE SPAN, and the answer must carry
	// exactly `first .. first+size-1`. Nothing rebases them — that is the whole reason they are
	// span-relative rather than global, since rebasing on the smallest returned key is what makes a
	// sparse answer silently shift every value onto the wrong row in data::Source.
	//
	// An empty map is a valid answer and completes the request.
	using BatchCallback = Function<void(Map<uint64_t, Value> &)>;
	using SpanCallback = Function<void(const BatchCallback &, uint64_t first, size_t size)>;

	// Produces a category's children on its first request. Fill `self` in through the Model and then
	// invoke `complete` EXACTLY ONCE — inline for a source that can answer immediately, such as a
	// directory walk, later for one that has to fetch.
	//
	// `self` is a parameter rather than something the callback captures on purpose: the callback is
	// stored IN the node, so capturing it would build a cycle nothing could break.
	using ChildsCallback = Function<void(Node *self, const Function<void()> &complete)>;

	// Reports how the external action ended. May be called inline or long afterwards, but exactly
	// once. Anything other than a successful Status reverts an optimistic change.
	using CompletionCallback = Function<void(Status)>;

	/* How a mutation becomes an action on the outside world.

	A slots struct rather than a virtual interface, because that is how this codebase expresses
	pluggable behaviour (see DropTargetSlots) and because data::Source has no subclasses anywhere —
	it is always configured by composition, and nothing here should force a different habit.

	Every slot is optional. An unset `can*` means "allowed"; an unset `perform*` means there is no
	external object to act on and the model simply changes. So a Model with no slots at all is a
	plain tree, which is exactly what a test or an in-memory list wants. */
	struct SP_PUBLIC Slots {
		// Pure predicates: they may be asked speculatively — while a drag hovers, for every
		// candidate parent — so they must not have side effects.
		Function<bool(const Node *node, const Node *dstParent, size_t index)> canMove;
		Function<bool(const Node *node)> canRemove;

		Function<void(Node *node, Node *dstParent, size_t index, CompletionCallback &&)>
				performMove;
		Function<void(Node *node, CompletionCallback &&)> performRemove;
	};

	virtual ~Model();

	virtual bool init();
	virtual bool init(Value &&rootData);

	// The root is a Category and always exists; it is never removable and never movable.
	Node *getRoot() const { return _root; }

	// Null for an id that never existed, or that named a node which has since been removed. Ids are
	// never reused, so this can never answer with a different element than the caller meant.
	Node *getNode(ItemId) const;

	size_t getNodeCount() const { return _index.size(); }

	// --- structure -------------------------------------------------------------------------------
	//
	// All of these live on the Model rather than on the Node so that the slots and the notification
	// cannot be bypassed. `parent` must be a Category; null means the root. `index` is clamped to
	// the end of the child list, so `maxOf<size_t>()` reliably means "append".

	Node *emplaceItem(Node *parent, size_t index, Value &&, Ref *object = nullptr);
	Node *emplaceCategory(Node *parent, size_t index, Value &&, Ref *object = nullptr);
	Node *emplaceSpan(Node *parent, size_t index, size_t count, SpanCallback &&);

	/* Move a node to another place in the tree, possibly under another parent.

	Refused — returning false with nothing changed — when the node is the root, when `dstParent` is
	not a Category, when `dstParent` is the node itself or one of its descendants (which would
	detach a cycle from the tree and leak it), or when `canMove` says no.

	`index` is interpreted in the child list AS IT WILL BE AFTER the node is taken out of its current
	place, which is the only reading that makes "move this one slot down" expressible.

	TRUE MEANS ACCEPTED, NOT DONE. It says the change passed every check the model can make and was
	either applied (Optimistic) or handed to `performMove` (Confirmed). The external action may
	still fail afterwards and undo it — that answer arrives through the completion, because it has
	to be allowed to take as long as a file copy takes. A caller that needs the verdict watches the
	model, or hasPendingMutations(). */
	bool moveNode(Node *node, Node *dstParent, size_t index);

	// Refused for the root and when `canRemove` says no. Removing a Category removes its whole
	// subtree; every id in it stops resolving. `true` means accepted, with the same caveat as
	// moveNode above.
	bool removeNode(Node *node);

	/* Remove every child of a category at once.

	The slots are NOT consulted, and that is the difference between this and a loop of removeNode():
	this says "these rows no longer exist", which is the shape of a REFRESH — the branch is about to
	be refilled from a cache that has changed. A per-element deletion the outside world has to agree
	to is removeNode(), one at a time.

	Every id in the subtree stops resolving. */
	void clearChildren(Node *parent);

	// Reorder in place. Ids do not change, so nothing keyed by identity is invalidated — which is
	// the difference between this and rebuilding the children in the new order.
	void sortChildren(Node *parent, const Function<bool(const Node *, const Node *)> &);

	// --- payload ---------------------------------------------------------------------------------
	//
	// These bump the node's revision and post Update::Data. They never touch the slots: changing
	// what a row SAYS is not an action on the external object, and a view that wants to write
	// through to the object does it itself.

	void setNodeData(Node *, Value &&);
	void setNodeObject(Node *, Ref *);

	// Grow or shrink a span. Offsets below the new count keep their meaning, so a view only has to
	// fetch what it did not have.
	void setSpanCount(Node *, size_t count);

	// --- delegation ------------------------------------------------------------------------------

	void setSlots(Slots &&);
	const Slots &getSlots() const { return _slots; }

	void setMutationPolicy(MutationPolicy value) { _policy = value; }
	MutationPolicy getMutationPolicy() const { return _policy; }

	// True while an optimistic change is waiting for its completion. A caller that wants to know
	// whether what it sees is committed asks this.
	bool hasPendingMutations() const { return !_pending.empty(); }

	/* Hides SubscriptionTemplate::setDirty so that a caller must think about which bit it is
	posting; the default is the conservative one. `Subscription::setDirty()` is still reachable by
	qualification for the rare caller that wants the forwarding form. */
	void setDirty(Flags flags = Flags(Update::Structure));

protected:
	friend class Model::Node;

	struct PendingMutation;

	ItemId allocateId() { return ItemId(_nextId++); }

	// Register/unregister a whole subtree in the id index. Removal is a subtree operation because a
	// category takes its descendants with it, and every one of their ids has to stop resolving.
	void indexSubtree(Node *);
	void unindexSubtree(Node *);

	Node *resolveParent(Node *) const;
	static bool isAncestorOf(const Node *ancestor, const Node *node);

	// True when the node is still part of this tree. Every deferred completion asks this before it
	// touches anything: an answer that arrives after its subject was removed must not resurrect it.
	bool isLive(const Node *) const;

	// Detach from the current parent and answer where it was, so an optimistic change can be undone.
	// The CALLER must already hold an Rc on the node — the slot being erased is usually the last one.
	sprt::pair<Node *, size_t> detach(Node *);
	void attach(Node *node, Node *parent, size_t index);

	// The structural edits themselves, with no slots and no validation: everything public has
	// already decided that the change is allowed by the time it reaches these.
	void applyMove(Node *node, Node *dst, size_t index);
	void applyRemove(Node *node);

	// Retire a pending external action. On failure this is what puts an optimistic change back.
	void finishPending(PendingMutation *, bool success);

	Rc<Node> _root;
	Map<ItemId, Node *> _index;
	Slots _slots;

	// Nodes taken out of the tree by an optimistic removal, kept alive until the completion decides
	// whether they come back. Without this the node would be freed before the revert could use it.
	Vector<Rc<PendingMutation>> _pending;

	MutationPolicy _policy = MutationPolicy::Optimistic;

	// Never reused, and never zero: zero is the id of nothing, which is what makes a
	// default-constructed Position harmless.
	uint64_t _nextId = 1;
};

/* One element. Structure only — every mutation goes through the Model.

Children are held by `Rc` and the parent by a raw pointer: the tree owns downwards, and an owning
back-reference would be a cycle no refcount could resolve. A node's parent outlives it by
construction, since the only way to lose a parent is to be removed from it. */
class SP_PUBLIC Model::Node : public Ref {
public:
	virtual ~Node();

	/* Public only because Rc<Node>::create() has to reach it; the Model is the only legitimate
	caller. A node built anywhere else holds an id that did not come from the model's counter, so it
	can never be attached to the tree and nothing can look it up. Build children with
	Model::emplaceItem / emplaceCategory / emplaceSpan instead — including from inside a
	lazy-children callback, which is exactly why there is no "adopt these nodes" entry point. */
	virtual bool init(Model *, ItemId, Kind);

	Kind getKind() const { return _kind; }
	bool isCategory() const { return _kind == Kind::Category; }
	bool isSpan() const { return _kind == Kind::Span; }

	ItemId getId() const { return _id; }
	Model *getModel() const { return _model; }
	Node *getParent() const { return _parent; }

	const Value &getData() const { return _data; }

	// The external object this element stands for — a file, a record, whatever the owner put here.
	// Opaque to the Model: it is carried, compared and handed back, never interpreted.
	Ref *getObject() const { return _object; }

	// Bumped by every payload edit. A view puts it in the key it rebuilds row nodes from, so editing
	// one row's data rebuilds one row's node instead of every visible one.
	uint32_t getRevision() const { return _revision; }

	SpanView<Rc<Node>> getChildren() const { return _children; }
	size_t getChildCount() const { return _children.size(); }

	// The node's own index in its parent's child list; maxOf<size_t>() for the root.
	size_t getChildIndex() const;

	// How many ROWS this subtree contributes at one level: a span counts as its length, anything
	// else as one. A view walking `children` needs this to size itself before it materializes
	// anything.
	size_t getRowCount() const { return _kind == Kind::Span ? _spanCount : 1; }

	size_t getSpanCount() const { return _spanCount; }

	// --- lazy children ---------------------------------------------------------------------------

	ChildsState getChildsState() const { return _childsState; }
	bool hasChildsCallback() const { return _childsCallback != nullptr; }
	void setChildsCallback(ChildsCallback &&);

	/* Which load the current children came from. Bumped by every resetChilds().

	A loader that answers on another thread reads this when it starts and compares on the way back:
	a different value means the branch was refreshed while it was listing, so its answer describes a
	state that no longer exists and must be thrown away instead of written into the node. A loader
	that answers inline can ignore it entirely. */
	uint32_t getChildsGeneration() const { return _childsGeneration; }

	/* Run the lazy-children callback unless it has already run. `onComplete` fires when the children
	are available: inline when they already are, at the end of the callback otherwise, and after the
	pending load when one is already in flight. The completion also dirties the Model, so a
	subscriber that never called this still learns about the new children.

	Returns true when children were loaded or a load is in flight. */
	bool requestChilds(Function<void()> &&onComplete = nullptr);

	// Drop the loaded children and go back to Pending, so the next requestChilds() reloads. Pending
	// completions are dropped WITHOUT being called — that is how an owner that is going away
	// releases a Loading category's hold on it.
	void resetChilds();

	// --- span ------------------------------------------------------------------------------------

	void setSpanCallback(SpanCallback &&);

	/* Ask for span offsets `[first, first+size)`, clamped to the span's length.

	Returns the number of offsets actually asked for; ZERO means no request was planned and no
	callback is ever coming, so the caller must treat that range as resolved rather than re-ask for
	it on every rebuild. */
	size_t getSpanData(const BatchCallback &, uint64_t first, size_t size);

protected:
	friend class Model;

	Model *_model = nullptr;
	Node *_parent = nullptr;
	ItemId _id;
	Kind _kind = Kind::Item;

	Value _data;
	Rc<Ref> _object;
	uint32_t _revision = 0;

	Vector<Rc<Node>> _children;

	size_t _spanCount = 0;
	SpanCallback _spanCallback = nullptr;

	ChildsCallback _childsCallback = nullptr;
	Vector<Function<void()>> _childsComplete;
	ChildsState _childsState = ChildsState::Empty;
	uint32_t _childsGeneration = 0;
};

/* An external action that has been asked for and has not answered yet.

It exists for one reason: an optimistic REMOVAL takes the node out of the tree, and the tree is what
was keeping it alive. Without a reference parked here the node would be freed the moment it was
detached, and the revert would have nothing to put back. A move needs no such help — the completion
already holds the node — but it is recorded here too, so hasPendingMutations() answers for both.

Defined out of line, after Node, rather than left as a forward declaration: `Rc<PendingMutation>` is
a member of Model, so every translation unit that so much as creates a Model has to instantiate its
destructor, and that needs the complete type. */
struct SP_PUBLIC Model::PendingMutation : public Ref {
	Rc<Node> node;
	Rc<Node> parent; // where it was, so a refused action can put it back
	size_t index = 0;
	bool remove = false;
};

} // namespace stappler::data

#endif /* STAPPLER_DATA_SPDATAMODEL_H_ */
