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

#ifndef XENOLITH_APPLICATION_INPUT_XLSELECTION_H_
#define XENOLITH_APPLICATION_INPUT_XLSELECTION_H_

#include "XLInteractiveComponent.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/* `:selected` and `:selection-within` - what the scene's current selection looks like in CSS.

WHY A MARKER COMPONENT AND NOT INTERACTIVE STATE. The same reason FocusWithinComponent is one, and
it applies to BOTH halves here rather than only to the ancestor half. `:selection-within` belongs to
whatever happens to be above a selected thing - a panel, a card, the layout root - and `:selected`
belongs to the item, which for a canvas object or a virtualized row is an ordinary Node with no
interactive state at all. InteractiveComponent defaults to Enabled, so writing either bit there
would switch `:enabled` on for such a node and `:disabled` off, and only while something happened to
be selected. See XLInteractiveComponent.h and XLFocusWithin.h for the trap in full.

PRESENCE IS THE STATE, and the counter is why the component can be trusted to disappear again. A
selection moves as a pair - the new chain is retained BEFORE the old one is released - so a shared
ancestor goes 1 -> 2 -> 1 and never blinks. Its style is not recomputed, and nothing below it is
either.

THE ONE PLACE THIS DIFFERS FROM FocusWithin, and a literal copy gets it wrong: the component carries
two independent facts, so the last release may NOT simply remove it. A node that is itself selected
has `withinCounter == 1` from its own chain and `selected == true`; releasing the chain must leave
the component behind while the bool is still set, or `:selected` would vanish the moment anything
recomputed the chain. Removal is guarded on both. */
struct SP_PUBLIC SelectionComponent {
	static ComponentId Id;

	// How many selected descendants (or the node itself) are counting on it. Never 0 on a live
	// component unless `selected` is still set.
	int32_t withinCounter = 0;

	// This node IS one of the selected items, not merely an ancestor of one.
	bool selected = false;
};

// Does a rule asking for `:selection-within` match this node?
SP_PUBLIC bool hasSelectionWithin(const Node *);

// Does a rule asking for `:selected` match this node?
SP_PUBLIC bool isNodeSelected(const Node *);

/* Collect the chain of `anchor` - the node itself and every ancestor up to the scene root, deepest
first. An `anchor` of null yields an empty chain.

Walks to the ROOT rather than to some widget boundary: `:selection-within` is a claim about ancestry,
not about any one widget, and a stylesheet is free to put the rule on any container above the
selected item.

Rc, and that is not caution: see updateSelectionChain. */
SP_PUBLIC void buildSelectionChain(Node *anchor, Vector<Rc<Node>> &out);

/* Move the ancestor marker from one COLLECTED chain to another. Retains the new chain first, so a
common ancestor keeps its component, and its style, throughout.

TAKES THE CHAINS, NOT THE ANCHORS, and this is the whole reason the caller has to keep one. A chain
released by walking getParent() from the old anchor is a chain read from the live graph at a moment
that is not the moment it was retained - and between those two moments the anchor may have been
DETACHED, which is not an exotic case but the ordinary one: a virtualized row is removed from its
parent, and only then does anything notice that the selection has to move. The walk would stop at the
detached node, and every ancestor above it would keep a count nothing will ever release again. The
symptom is a container that stays `:selection-within` forever, with no selection anywhere in it.

Holding Rc for the same reason it holds the chain at all: the nodes to be released may already have
left the graph, and something has to keep them addressable until their counters come down. */
SP_PUBLIC void updateSelectionChain(SpanView<Rc<Node>> from, SpanView<Rc<Node>> to);

/* The leaf half: this node is, or is no longer, a selected item.

Separate from the chain because the two dirty differently - an ancestor's counter moving from 1 to 2
must NOT re-style it, while this bit flipping always must - and because a selected item's node comes
and goes independently of the selection: a virtualized row is recycled while the selection stands
still, which is why TreeView re-applies this from updateRowNode rather than once at selection time. */
SP_PUBLIC void setNodeSelected(Node *, bool);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLSELECTION_H_
