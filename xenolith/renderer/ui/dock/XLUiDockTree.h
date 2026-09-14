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

// One slot of the split tree: a split of two children, a leaf holding panels as tabs, or a Free
// slot on the free list.
//
// `node` is the scene node for the slot (DockFrame or DockSplitter). The tree only stores it and
// every pass works with a null one, so the tree is testable without a scene.
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

	// Share of `first` in the space left after both children got their minimums, not of the whole
	// extent, so the proportion holds while shrinking down to the minimums.
	float ratio = 0.5f;

	DockNodeHandle first;
	DockNodeHandle second;

	// --- leaf --------------------------------------------------------------
	DockFrameParams params;
	Vector<String> panels; // tab order
	size_t active = 0; // index into `panels`

	// Collapsed to its tab strip: the minimum is the strip's size, regardless of the panels. Kept
	// on the slot, not the node, so it is saved and survives node rebuilds.
	bool collapsed = false;

	// --- the scene node, opaque here ---------------------------------------
	Rc<Node> node;

	// --- computed; rewritten by every pass ---------------------------------
	Size2 minSize; // propagated minimum: own floor for a leaf, composed for a split
	Rect rect; // root-local, bottom-left origin

	// split only: the divider band carved out of `rect`; the geometry of the split's scene node
	Rect splitterRect;

	bool isLeaf() const { return kind == Kind::Leaf; }
	bool isSplit() const { return kind == Kind::Split; }
};

/** The logical structure of a dock: a binary tree of splits over parking places.

A free-list arena of DockTreeNode addressed by generational handles, so parent links cannot dangle
after a merge.

Three passes run in this order; handleMeasure may run only the first:

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
	// (left for Horizontal, top for Vertical). Returns the new leaf, or an empty handle when the
	// target is not a leaf or does not allow splitting.
	DockNodeHandle splitLeaf(DockNodeHandle leaf, DockAxis, bool firstIsNew, DockFrameParams &&,
			float ratio = 0.5f);

	// Remove a leaf and lift its sibling into the split's slot. Refuses on the root leaf, on a
	// Permanent one, and on a leaf that still holds panels.
	bool collapseLeaf(DockNodeHandle leaf);

	// --- queries -----------------------------------------------------------

	// The leaf whose rect contains `point`, as of the last distribute(); O(tree depth).
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

	// Top-down: a rect for every slot, from the ratios and the computed minimums. Writes only
	// DockTreeNode::rect and ::splitterRect. `rtl` reverses horizontal splits so `first` is the
	// inline start; this is the only place mirroring happens, saved layouts stay side-neutral.
	void distribute(const Rect &available, DockOverflowPolicy, float splitterThickness,
			bool rtl = false);

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

	// Shape and membership only; titles, icons and minimums come from the descriptors.
	Value save() const;

	// Rebuild from a saved layout. Ids rejected by `isPanelKnown` are dropped with a warning. The
	// new tree is swapped in only if the whole build succeeds; otherwise the layout is unchanged.
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

	// a saved node becomes a spec first, so restore() and setLayout() share one build path
	static bool readSpec(const Value &, DockLayoutSpec &, const Callback<bool(StringView)> &);

	// drop leaves that ended up empty, unless they were declared Permanent
	void pruneEmptyLeaves();

	void updateMinimumsAt(DockNodeHandle, const MeasureLeaf &, float thickness);
	void distributeAt(DockNodeHandle, const Rect &, DockOverflowPolicy, float thickness, bool rtl);
	void eachInOrderAt(DockNodeHandle, const Callback<void(const DockTreeNode &)> &) const;

	Vector<DockTreeNode> _nodes;
	Vector<uint32_t> _free;
	DockNodeHandle _root;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_DOCK_XLUIDOCKTREE_H_
