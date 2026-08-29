/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
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

#ifndef EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEVIEW_H_
#define EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEVIEW_H_

#include "XLUiTreeView.h"
#include "XLDragTypes.h"
#include "XLFrameCapture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class DragSource;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// What one dragged row carries as the drag's in-process payload.
//
// A Ref rather than a bare id, because DragData's fast path hands over a live object - and because
// the drop has to know which MODEL the element came from. Two views over two separate models is
// what makes the difference between a relocation and a copy: an element that stays inside its own
// model is MOVED (its ItemId, and everything keyed by it, survives), one that crosses over has to
// be rebuilt on the other side and deleted here.
struct DndItemPayload : public Ref {
	// The drag's local type tag. A target checks this before touching anything else, and a drag
	// carrying anything else is simply not ours.
	static constexpr auto TypeName = StringView("xl/dndtree-item");

	Rc<data::Model> model; // the model the element lives in RIGHT NOW
	Rc<data::Model::Node> node; // the element itself; an Rc, so a removal cannot free it mid-drop
	String title; // what the row showed - the ghost and the status line read it
	String origin; // title of the tree it was picked up from

	// Set by a target that relocated the original itself (a move inside one model). It is what
	// keeps the source's completion from deleting an element that is already in its new place.
	bool consumed = false;
};

/* A ui::TreeView whose rows can be dragged out of it and dropped into another one.

Everything here is composition over the stock widget: the tree, its rows and its model are the
engine's, and this class only attaches a DragSource and a DropTarget to each row node as it is
built. That is deliberate - a demo whose interesting half lived in a subclass of the model or of
the row would not show how the two subsystems meet.

WHERE A DROP LANDS. A row answers for a place in the tree, not for itself: dropping ON a category
appends to it, dropping on a leaf inserts right AFTER that leaf in its parent. The view's own
background answers with "append to the root", which is what makes an empty tree a target at all.

WHY THE INDEX IS READ BACK AND NEVER CAPTURED. A row node survives a rebuild whenever its RowKey
still matches, and the rebuild hands it to whichever index that row moved to. A lambda that
captured its index at build time would, after a category above it opened, drag whatever row had
landed in the old slot - the same trap the stock expander avoids by reading getRowIndex(). */
class DndTreeView : public ui::TreeView {
public:
	using ModelNode = data::Model::Node;
	using MessageCallback = Function<void(StringView)>;

	// Where an element would be inserted. `parent` is always a Category; a null one means the spot
	// could not be resolved and nothing may be dropped there.
	struct DropSpot {
		ModelNode *parent = nullptr;
		size_t index = maxOf<size_t>(); // maxOf: append

		bool valid() const { return parent != nullptr; }
	};

	virtual ~DndTreeView() = default;

	virtual bool init(data::Model *, StringView title);

	StringView getTitle() const { return _title; }

	// The node a drag ghost is parked under. It must be INSIDE the subtree the StyleResolver
	// covers, or the ghost comes out unstyled - a resolver only ever sees its own subtree.
	void setGhostParent(Node *value) { _ghostParent = value; }

	// One line about what just happened, for the demo's status strip.
	void setMessageCallback(MessageCallback &&);

	// --- the drop model, callable without a drag -----------------------------------------------
	// The three below are the whole of what a drop does, and none of them touches an input event.
	// That is what lets the self-check and the inspector commands exercise the REAL path instead of
	// a parallel implementation of it.

	// Where a drop on row `index` would land; maxOf<size_t>() asks for the view's own background.
	DropSpot resolveDropSpot(size_t index) const;

	// Pure: it is called during hit testing, for targets that may never become current.
	bool canAccept(const DndItemPayload *, const DropSpot &) const;

	// Apply the drop. A single resolved action, never a mask. False means nothing was done.
	bool applyTransfer(DndItemPayload *, const DropSpot &, DragActions);

	// What the SOURCE side of a drag owes the payload once the drop has been applied (or not):
	// a Move whose target did not relocate the element itself has to delete the original here.
	// Static, because by then the view the drag started in may be the one thing that is gone.
	static void finishTransfer(DndItemPayload *, DragActions);

	// Build the payload a row would be dragged with; null when that row is not draggable.
	Rc<DndItemPayload> makePayload(size_t index) const;

	// How many elements this view built from a copy - the self-check reads it to tell a clone
	// apart from a relocation.
	size_t getCloneCount() const { return _clones; }

protected:
	using ui::TreeView::init;

	// The only hook needed: it runs for a row node that is genuinely new, which is exactly when the
	// two systems have to be attached. A reused node already carries them.
	virtual Rc<Node> buildRowNode(RowBuilder &) override;

	void attachRowHandlers(RowNode *);
	void attachViewTarget();

	// `source` is the DragSource that will run this drag: a capture-backed ghost is installed on
	// its session, long after this returns.
	bool fillOffer(size_t index, DragSource *source, DragOffer &offer);

	/* Ask the window for a cutout of the row at `index` and, when it lands, hand it to the drag as
	its decorator. False when no capture could be armed - and then the caller keeps the drawn ghost.

	The capture is armed BEFORE the drag begins, and the offer is marked decoratorDeferred, because
	the pointer at that moment sits on the very row being copied: a ghost built up front would be
	photographed along with it. */
	bool requestGhostCapture(size_t index, DragSource *source, DragOffer &offer);

	FrameCapture *getFrameCapture() const;

	void report(StringView);

	static DndItemPayload *payloadOf(const DragEvent &);

	String _title;
	Node *_ghostParent = nullptr; // raw: it is an ancestor of this view, so it outlives it
	MessageCallback _message;
	size_t _clones = 0;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEVIEW_H_
