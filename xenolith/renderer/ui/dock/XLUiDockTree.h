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

#ifndef XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTREE_H_
#define XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTREE_H_

#include "XLUiDockTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One slot of the split tree: either a binary SPLIT of two children, or a LEAF - a parking place
// holding panels as tabs. A Free slot is one the free list has handed back.
//
// `node` is the flat scene node this slot materializes (a DockFrame for a leaf, a DockSplitter for
// a split). The tree stores it and nothing more: it never reads it, never calls anything on it,
// and every pass below works with a null one. That is deliberate - the whole tree, including the
// minimum propagation and the distribution, is testable without a scene.
struct SP_PUBLIC DockTreeNode {
	enum class Kind : uint8_t {
		Free,
		Split,
		Leaf,
	};

	Kind kind = Kind::Free;

	// bumped on release, so a handle to a reused slot stops resolving instead of retargeting
	uint32_t generation = 1;

	DockNodeHandle self;
	DockNodeHandle parent;

	// --- split -------------------------------------------------------------
	DockAxis axis = DockAxis::Horizontal;

	// Share of `first` in the space left after BOTH children got their minimums - not a share of
	// the whole extent. That is what makes a resize feel right: the proportion survives shrinking
	// all the way down to the minimums instead of crushing the smaller pane to nothing.
	float ratio = 0.5f;

	DockNodeHandle first;
	DockNodeHandle second;

	// --- leaf --------------------------------------------------------------
	DockFrameParams params;
	Vector<String> panels; // tab order
	size_t active = 0; // index into `panels`

	// --- the scene node, opaque here ---------------------------------------
	Rc<Node> node;

	// --- computed; rewritten by every pass ---------------------------------
	Size2 minSize; // propagated minimum: own floor for a leaf, composed for a split
	Rect rect; // root-local, bottom-left origin

	// split only: the divider band between the two children, carved out of `rect`. It is the
	// geometry of the split's own scene node, the one thing a split materializes.
	Rect splitterRect;

	bool isLeaf() const { return kind == Kind::Leaf; }
	bool isSplit() const { return kind == Kind::Split; }
};

/** The logical structure of a dock: a binary tree of splits over parking places.

It is a free-list arena of DockTreeNode addressed by generational handles, not a graph of
ref-counted objects with parent pointers - a split tree has to be walked upwards as well as down,
and `Rc` in both directions is a cycle while a raw parent pointer left dangling by a merge is
exactly the bug the generation catches instead.

Three passes run over it, in this order, and they are kept strictly separate because handleMeasure
is only allowed to run the first:

  updateMinimums()  bottom-up, pure: the effective minimum of every slot
  distribute()      top-down, writes only into the arena: a rect for every slot
  <commit>          the owner writes those rects onto the scene nodes

The tree neither owns nor knows the panels' content: `panels` is a list of ids, and the descriptor
registry lives in DockSystem. */
class SP_PUBLIC DockTree {
public:
	// Reports the content minimum of one leaf - what its panels and its tab strip need, before the
	// leaf's own declared floor is applied. Called by updateMinimums for every leaf.
	using MeasureLeaf = Callback<Size2(const DockTreeNode &)>;

	void clear();

	bool empty() const { return _root.empty(); }

	DockNodeHandle getRoot() const { return _root; }
	void setRoot(DockNodeHandle);

	bool isValid(DockNodeHandle) const;

	// nullptr when the handle is empty or stale; `at` asserts instead, for the callers that have
	// just validated the handle and would only repeat the check
	DockTreeNode *get(DockNodeHandle);
	const DockTreeNode *get(DockNodeHandle) const;
	DockTreeNode &at(DockNodeHandle);
	const DockTreeNode &at(DockNodeHandle) const;

	// --- construction ------------------------------------------------------

	DockNodeHandle makeLeaf(DockFrameParams &&, Vector<String> &&panels, size_t active = 0);
	DockNodeHandle makeSplit(DockAxis, float ratio, DockNodeHandle first, DockNodeHandle second);

	// Build the whole tree from a declarative spec, replacing whatever is there. Returns false and
	// leaves the tree untouched when the spec is malformed (a split without exactly two children).
	bool build(const DockLayoutSpec &);

	// --- structural operations ---------------------------------------------

	// Subdivide a leaf: it keeps its own panels, a new empty leaf is created beside it, and a split
	// takes their place in the parent. `firstIsNew` puts the new leaf on the low side of the axis
	// (left for Horizontal, TOP for Vertical). Returns the new leaf, or an empty handle when the
	// target is not a leaf or does not allow splitting.
	DockNodeHandle splitLeaf(DockNodeHandle leaf, DockAxis, bool firstIsNew, DockFrameParams &&,
			float ratio = 0.5f);

	// Remove a leaf and lift its sibling into the split's slot. Refuses on the root leaf, on a
	// Permanent one, and on a leaf that still holds panels.
	bool collapseLeaf(DockNodeHandle leaf);

	// --- queries -----------------------------------------------------------

	// The leaf whose rect contains `point`, as of the last distribute(). O(tree depth): it walks
	// down through the splits instead of testing every leaf.
	DockNodeHandle findLeafAt(const Vec2 &point) const;

	DockNodeHandle findFrameByName(StringView) const;
	DockNodeHandle findFrameForPanel(StringView panelId) const;
	DockNodeHandle findLargestLeaf() const;

	size_t getLeafCount() const;

	// --- passes ------------------------------------------------------------

	// Bottom-up: a leaf's minimum is its declared floor raised to what the callback reports; a
	// split's is the sum of its children along its axis (plus the divider) and their maximum
	// across it. Pure - it writes only DockTreeNode::minSize.
	void updateMinimums(const MeasureLeaf &, float splitterThickness);

	// Top-down: a rect for every slot, from the tree's ratios and the minimums the pass above
	// computed. Writes only DockTreeNode::rect and ::splitterRect.
	void distribute(const Rect &available, DockOverflowPolicy, float splitterThickness);

	Size2 getRootMinSize() const;

	// The propagated minimum of a slot projected onto one axis
	float minAlongAxis(DockNodeHandle, DockAxis) const;

	// --- iteration ---------------------------------------------------------

	// Every live slot, in arena order. The order is arbitrary and must not be relied on for
	// anything but a per-slot operation (the geometry commit, a node sweep).
	void each(const Callback<void(DockTreeNode &)> &);
	void each(const Callback<void(const DockTreeNode &)> &) const;

	// Depth-first from the root: a split is visited before its children, `first` before `second`.
	// This is the order a dump or a serializer wants.
	void eachInOrder(const Callback<void(const DockTreeNode &)> &) const;

	// --- persistence -------------------------------------------------------

	static constexpr int64_t SaveVersion = 1;

	// The SHAPE and the MEMBERSHIP, and nothing else. A panel's title, icon and minimum size are
	// never written: they come from the descriptor registry, and a saved copy of them would go
	// stale the moment the application is updated.
	Value save() const;

	// Rebuild from a saved layout. `isPanelKnown` decides which ids the registry still has; the
	// ones it rejects are dropped from their frame with a warning, because a layout naming a panel
	// this build no longer ships is the normal case for a downgrade, not a failure.
	//
	// A candidate tree is built first and swapped in only when the whole build succeeds, so a
	// malformed save leaves the live layout exactly as it was.
	bool restore(const Value &, const Callback<bool(StringView)> &isPanelKnown);

protected:
	DockNodeHandle allocate();
	void release(DockNodeHandle);
	void releaseSubtree(DockNodeHandle);

	// replace `oldChild` with `newChild` in whatever slot of `parent` it occupies
	void replaceChild(DockNodeHandle parent, DockNodeHandle oldChild, DockNodeHandle newChild);

	DockNodeHandle buildSpec(const DockLayoutSpec &, DockNodeHandle parent);
	static bool validateSpec(const DockLayoutSpec &);

	Value saveNode(DockNodeHandle) const;

	// A saved node becomes a spec first, so restore() and setLayout() converge on one build path
	static bool readSpec(const Value &, DockLayoutSpec &, const Callback<bool(StringView)> &);

	// drop leaves that ended up empty, unless they were declared Permanent
	void pruneEmptyLeaves();

	void updateMinimumsAt(DockNodeHandle, const MeasureLeaf &, float thickness);
	void distributeAt(DockNodeHandle, const Rect &, DockOverflowPolicy, float thickness);
	void eachInOrderAt(DockNodeHandle, const Callback<void(const DockTreeNode &)> &) const;

	Vector<DockTreeNode> _nodes;
	Vector<uint32_t> _free;
	DockNodeHandle _root;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTREE_H_
