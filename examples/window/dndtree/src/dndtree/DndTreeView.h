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
#include "XLUiInlineEditor.h"
#include "XLUiContextMenu.h"
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
engine's, and this class only attaches a DragSource to each row node as it is built, and one drop
target and one context menu to the view. That is deliberate - a demo whose interesting half lived in a subclass of the model or of
the row would not show how the two subsystems meet.

WHERE A DROP LANDS is ui::TreeView's answer, not this class's: the upper half of a leaf is "before
it", the lower half "after it", the whole of a category's row is "into it", and the empty space
below the last row is "append to the root". The insertion line, the highlight and the dwell that
opens a closed category under the pointer come with it. What is left here - and it is the only half
a tree cannot answer - is whether THIS payload may land at a given place and what moving it there
means, which is ui::TreeView::DropSlots.

WHY THE INDEX IS READ BACK AND NEVER CAPTURED. A row node survives a rebuild whenever its RowKey
still matches, and the rebuild hands it to whichever index that row moved to. A lambda that
captured its index at build time would, after a category above it opened, drag whatever row had
landed in the old slot - the same trap the stock expander avoids by reading getRowIndex(). */
class DndTreeView : public ui::TreeView {
public:
	using ModelNode = data::Model::Node;
	using MessageCallback = Function<void(StringView)>;

	using DropPosition = ui::TreeView::DropPosition;

	virtual ~DndTreeView() = default;

	virtual bool init(data::Model *, StringView title);

	StringView getTitle() const { return _title; }

	// The node a drag ghost is parked under. It must be INSIDE the subtree the StyleResolver
	// covers, or the ghost comes out unstyled - a resolver only ever sees its own subtree.
	void setGhostParent(Node *value) { _ghostParent = value; }

	// One line about what just happened, for the demo's status strip.
	void setMessageCallback(MessageCallback &&);

	// --- the drop model, callable without a drag -----------------------------------------------
	// Both below are the whole of what a drop does, and neither touches an input event. That is
	// what lets the self-check and the inspector commands exercise the REAL path instead of a
	// parallel implementation of it - the position they are given comes from the same
	// TreeView::getDropPositionForRow the pointer resolves.

	// Pure: it is called during hit testing, for positions the drag may never come to rest on.
	bool canAccept(const DndItemPayload *, const DropPosition &) const;

	// Apply the drop. A single resolved action, never a mask. False means nothing was done.
	bool applyTransfer(DndItemPayload *, const DropPosition &, DragActions);

	// What the SOURCE side of a drag owes the payload once the drop has been applied (or not):
	// a Move whose target did not relocate the element itself has to delete the original here.
	// Static, because by then the view the drag started in may be the one thing that is gone.
	static void finishTransfer(DndItemPayload *, DragActions);

	// Build the payload a row would be dragged with; null when that row is not draggable.
	Rc<DndItemPayload> makePayload(size_t index) const;

	// How many elements this view built from a copy - the self-check reads it to tell a clone
	// apart from a relocation.
	size_t getCloneCount() const { return _clones; }

	// --- the context menu, callable without a pointer ------------------------------------------

	/* What a right click on `index` offers, or on the empty space below the last row when it is
	maxOf<size_t>(). Null when there is nothing to offer at all.

	Public and index-driven for the same reason canAccept and applyTransfer are: the check drives
	the REAL menu rather than a parallel description of it. The row is resolved from the pointer by
	the widget (TreeView::getRowIndexAt), so what is left over here is only what the menu SAYS. */
	Rc<ui::MenuSource> buildContextMenu(size_t index);

	// Delete the element a row stands for, and whatever hangs under it. What the menu's own item
	// does; false when that row is not one that can go.
	bool removeRow(size_t index);

	/* Give the element a row stands for a new name.

	The model write, with no editor anywhere near it - which is what lets a check drive a rename
	without a scene, a pointer or a window, and what the editor's own commit calls. False for a row
	that is not an element and for a name with nothing but blanks in it; the caller decides what a
	refusal means, and the editor turns it into "the session stays open". */
	bool renameRow(size_t index, StringView name);

	/* Open an inline editor over a row and rename it with what is typed.

	What the menu's Rename item does. The editor is NOT a child of the row: a row is destroyed by
	scrolling and rebuilt whenever its RowKey changes, and the rename is the very edit that changes
	it - so it lives on an overlay, anchored to THIS view, over the rectangle the row occupies in
	this view's space (ui::beginInlineTextEdit). False when there is nothing to open over: no scene,
	no element, no rectangle. */
	bool beginRename(size_t index);

	// Must agree with `inline-editor { padding-left/right }` in the demo's stylesheet - see
	// beginRename, which grows the rect by it so the editor's text lands exactly on the label's.
	static constexpr float RenameEditorPadding = 6.0f;

	bool isRenaming() const { return _rename != nullptr; }
	ui::InlineEditSession *getRenameSession() const { return _rename; }

	/* Create an element and hand it straight to the editor.

	What the menu's New submenu does. The element goes into the MODEL at once, rather than being
	drawn beside the tree as a preview: the row it makes is the very thing being named, so the
	author sees where the new item is going before typing anything, and everything else that reads
	the tree - the drop model, the other view, the self-check - sees one consistent model
	throughout. What makes it provisional is only what Escape does: a CANCELLED edit takes the
	element back out again. Every other ending keeps it under whatever name it has by then, which
	is the same rule a rename follows.

	`index` is the row the menu was opened over, maxOf<size_t>() for the empty space below the last
	one. False when there is nowhere to put it.

	The editor does not open in this call. A brand-new row has no NODE yet - the model's
	notification re-derives the rows at once, but the nodes are rebuilt in the view's next
	components phase - and where a row's content starts is decided by the stylesheet, so it does
	not exist until that node has been laid out. So the opening waits for the row; see
	settlePendingEdit. */
	bool beginCreate(size_t index, bool category);

	/* Where beginCreate puts a new element for a menu opened over `index`, in MODEL terms - ready
	for Model::emplaceItem. A null parent means there is nowhere to put one.

	NOT the drop model's three zones, and deliberately so: a drop happens at a POINT inside a row
	and can therefore read which half of it was aimed at, while a menu is opened over a row as a
	whole. What is left is the same vocabulary with the halves collapsed - a category is somewhere
	to go INTO, anything else is something to stand beside, and the empty space below the last row
	answers for the root. */
	struct InsertPoint {
		Rc<ModelNode> parent;
		size_t index = maxOf<size_t>();
	};
	InsertPoint getInsertPoint(size_t index) const;

	// The element the open editor would take back out if the edit were cancelled, or 0 for a
	// rename. What tells the two apart from outside.
	ItemId getProvisionalId() const { return _provisional; }
	bool isCreating() const { return _provisional != ItemId(0); }

	// True between beginCreate and the editor actually opening - normally one frame.
	bool isEditorPending() const { return _pendingEdit != ItemId(0); }

	/* Settle whatever beginCreate is waiting for, now: open the editor once the row has a node,
	give up when the element is gone.

	Called once per visit of this view, and public because a wait that spans frames cannot be
	observed from a check that has none - the demo's self-check runs before the layout is even in a
	scene. Doing nothing is the normal answer. */
	void settlePendingEdit();

	/* The sheet to carry to whatever this view opens OUTSIDE its own subtree.

	An inline editor is not a child of the row it edits - it lives on an overlay of the scene's
	content node, which is a sibling of the layout whose ui::StyleResolver covers this view. So the
	rules that style everything else here do not reach it, and it comes up as a bare white field
	unless the sheet is handed over. Exactly the same boundary as ui::MenuConfig::stylesheetSource,
	which the demo already crosses for the context menu; this is the second place it has to. */
	void setStylesheetSource(StringView value) { _stylesheet = value.str<Interface>(); }
	StringView getStylesheetSource() const { return _stylesheet; }

	virtual void handleEnter(Scene *) override;

protected:
	using ui::TreeView::init;

	// The only hook needed: it runs for a row node that is genuinely new, which is exactly when the
	// two systems have to be attached. A reused node already carries them.
	virtual Rc<Node> buildRowNode(RowBuilder &) override;

	void attachRowHandlers(RowNode *);

	// Hands the tree the two answers only this class can give. Everything else about the drop -
	// where it lands, what is drawn for it, when a closed category opens - is the widget's.
	void attachDropSlots();

	/* One context menu, ON THE VIEW, never one per row - the same arrangement, and the same reason,
	as the single drop target above it: a row that scrolled out of sight is no longer a node to
	carry anything, while the geometry still answers for it, and the empty space below the last row
	has no row at all. The builder resolves the row from the point it is given. */
	void attachContextMenu();

	// The `New` submenu, added to both menus this class builds - the one over a row and the one
	// over the empty space, which differ in everything else.
	void addCreateSubmenu(ui::MenuSource *, size_t index);

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

	// The one write. Both entry points above end here, so "what a rename does to the model" is said
	// once - including the rule that an empty name is not a name.
	bool renameNode(ModelNode *, StringView name);

	/* Open the editor over row `index`. `provisional` is the element a CANCEL removes - the whole
	difference between creating and renaming, which is otherwise the same editor over the same
	rectangle with the same commit. */
	bool beginEdit(size_t index, ItemId provisional);

	// The row showing `id`, or maxOf<size_t>(). Rows are re-derived by every model change, so an
	// index taken before one means nothing afterwards - the id is what still names the element.
	size_t rowIndexForId(ItemId) const;


	String _title;
	Node *_ghostParent = nullptr; // raw: it is an ancestor of this view, so it outlives it
	MessageCallback _message;
	size_t _clones = 0;

	// The open rename, or null. Held because the demo has to be able to say whether one is open -
	// and because a second Rename commits the first rather than abandoning it
	Rc<ui::InlineEditSession> _rename;
	String _stylesheet;

	// The element the open editor would take back out on a cancel; 0 while a plain rename is open
	ItemId _provisional;

	// The element created by beginCreate whose editor has not opened yet, and how many more visits
	// it may wait for its row before the wait gives up on it
	ItemId _pendingEdit;
	uint32_t _pendingEditFrames = 0;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_DNDTREE_SRC_DNDTREE_DNDTREEVIEW_H_
