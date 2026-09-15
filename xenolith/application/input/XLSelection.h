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

class SelectionOwner;

/* Marker for `:selected` and `:selection-within`. Not InteractiveComponent state, for the same
reason as FocusWithinComponent: its default Enabled would change `:enabled`/`:disabled` matching
on panels and plain nodes while something is selected.

Presence is the state. The new chain is retained before the old one is released, so a shared
ancestor goes 1 -> 2 -> 1 and is not restyled. Unlike FocusWithin, the last chain release must not
remove the component while `selected` is still set; removal is guarded on both. */
struct SP_PUBLIC SelectionComponent {
	static ComponentId Id;

	// How many selected descendants (or the node itself) are counting on it. Never 0 on a live
	// component unless `selected` is still set.
	int32_t withinCounter = 0;

	// This node is one of the selected items, not merely an ancestor of one.
	bool selected = false;
};

// Does a rule asking for `:selection-within` match this node?
SP_PUBLIC bool hasSelectionWithin(const Node *);

// Does a rule asking for `:selected` match this node?
SP_PUBLIC bool isNodeSelected(const Node *);

/* Collect the chain of `anchor` - the node itself and every ancestor up to the scene root, deepest
first. An `anchor` of null yields an empty chain. See updateSelectionChain for why Rc. */
SP_PUBLIC void buildSelectionChain(Node *anchor, Vector<Rc<Node>> &out);

/* Move the ancestor marker from one collected chain to another, retaining the new chain first.

Takes chains, not anchors: the old anchor may already be detached (e.g. a recycled virtualized
row), and walking getParent() from it would leave ancestor counters that are never released. Rc
keeps nodes that have left the graph addressable until their counters come down. */
SP_PUBLIC void updateSelectionChain(SpanView<Rc<Node>> from, SpanView<Rc<Node>> to);

/* The leaf half: this node is, or is no longer, a selected item. Separate from the chain: this
bit always restyles while counter moves do not, and item nodes are recycled independently of the
selection (TreeView re-applies it from updateRowNode). */
SP_PUBLIC void setNodeSelected(Node *, bool);

/* A candidate for arrow navigation (SelectionSystem::moveSelection), found through the committed
frame's hit-test registry, so only a drawn node is one. With an `owner` the selection enters it
through SelectionOwner::enterSelection; without one the node is selected with selectNode(). */
struct SP_PUBLIC SelectableComponent {
	static ComponentId Id;

	SelectionOwner *owner = nullptr;
};

// Attaches or removes SelectableComponent together with HitTestFlags::Selectable
SP_PUBLIC void setNodeSelectable(Node *, bool, SelectionOwner * = nullptr);

SP_PUBLIC const SelectableComponent *getNodeSelectable(const Node *);

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_INPUT_XLSELECTION_H_
