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

#include "XLCommon.h" // IWYU pragma: keep

#include "dndtree/DndTreeView.h"
#include "XLDragSource.h"
#include "XLUiPanel.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XL2dLabel.h"
#include "XL2dSprite.h"
#include "XLAppWindow.h"
#include "director/XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The node that follows the pointer. A plain ui::Panel with a label: everything it looks like comes
// from `.drag-ghost` in the demo's stylesheet, which is why it MUST be parked inside the subtree
// that carries the StyleResolver (see DragOffer::decoratorParent).
static Rc<Node> makeDragGhost(StringView title) {
	auto ghost = Rc<ui::Panel>::create();
	ghost->addStyleClass("drag-ghost");
	// under the pointer, not beside it; the sheet says the same thing, so the ghost is centred
	// even before the first style pass runs
	ghost->setAnchorPoint(Anchor::Middle);

	auto label = ghost->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	label->setType("label");
	label->setString(title);
	return ghost;
}

// The captured row, as the node that follows the pointer. Nothing about it is drawn a second time:
// these are the pixels the row had on screen.
static Rc<Node> makeCaptureGhost(FrameCaptureTarget *target, float density) {
	auto sprite = Rc<basic2d::Sprite>::create(Rc<Texture>(target->getTexture()));
	if (!sprite) {
		return nullptr;
	}

	sprite->setAnchorPoint(Anchor::Middle);

	// A capture is measured in SURFACE pixels; the scene is not. At density 1 the two agree and at
	// density 2 a cutout drawn at its pixel size would come out twice too large.
	const auto extent = target->getExtent();
	sprite->setContentSize(Size2(float(extent.width) / density, float(extent.height) / density));

	// A captured frame carries no meaningful alpha - the compositor was told the surface is opaque,
	// so whatever the swapchain image holds in that channel is not a transparency to inherit.
	// Without this the ghost blends itself away to nothing.
	sprite->setColorMode(core::ColorMode(core::ComponentMapping::R, core::ComponentMapping::G,
			core::ComponentMapping::B, core::ComponentMapping::One));

	// Enough to read as a ghost rather than as a second copy of the row - and no more than that,
	// because the ghost is a FULL-WIDTH copy centred on the pointer, so it lies over the very row
	// the tree is drawing its insertion line or its highlight on. It has to be seen through.
	sprite->setOpacity(0.6f);
	return sprite;
}

/* Rebuild `src` and everything under it inside another model.

This is what a transfer between two models costs, and why a transfer INSIDE one does not pay it: an
ItemId belongs to the model that allocated it, so an element crossing over is a different element on
the other side, and everything keyed by the old id (an expansion, a selection) is gone with it.

Spans are skipped rather than recreated: a span stands for rows nobody stores, answered by a
callback that belongs to whoever installed it. There is nothing here to copy. */
static data::Model::Node *cloneInto(data::Model *model, data::Model::Node *src,
		data::Model::Node *dstParent, size_t index, size_t &created) {
	data::Model::Value payload = src->getData();

	if (!src->isCategory()) {
		auto leaf = model->emplaceItem(dstParent, index, sp::move(payload));
		if (leaf) {
			++created;
		}
		return leaf;
	}

	auto made = model->emplaceCategory(dstParent, index, sp::move(payload));
	if (!made) {
		return nullptr;
	}
	++created;

	// `src` is never an ancestor of `dstParent` - canAccept() refuses that - so the child list
	// being walked here cannot be the one being appended to.
	for (auto &child : src->getChildren()) {
		if (child->isSpan()) {
			continue;
		}
		cloneInto(model, child, made, maxOf<size_t>(), created);
	}
	return made;
}

// True when `node` is `candidate` or one of its descendants, i.e. dropping `candidate` into `node`
// would put a subtree inside itself. Pure: it only walks parent links.
static bool isSelfOrDescendantOf(const data::Model::Node *node,
		const data::Model::Node *candidate) {
	for (auto it = node; it; it = it->getParent()) {
		if (it == candidate) {
			return true;
		}
	}
	return false;
}

} // namespace

bool DndTreeView::init(data::Model *source, StringView title) {
	if (!ui::TreeView::init(source)) {
		return false;
	}

	_title = title.str<Interface>();
	setName(_title);

	attachDropSlots();

	return true;
}

void DndTreeView::handleEnter(Scene *scene) {
	ui::TreeView::handleEnter(scene);

	// NOT in init(): ui::setContextMenu acquires the scene's coordinator, and a node has no scene
	// until it is added to one
	attachContextMenu();
}

void DndTreeView::setSource(Model *source) {
	// The count is about THIS model: a clone made into a tree that has been replaced is not one of
	// the elements the new tree holds, and leaving it in would have the counter answering about two
	// different trees at once.
	_clones = 0;
	ui::TreeView::setSource(source);
}

void DndTreeView::setMessageCallback(MessageCallback &&cb) { _message = sp::move(cb); }

void DndTreeView::attachContextMenu() {
	ui::setContextMenu(this, [this](const ui::ContextMenuRequest &req) -> Rc<ui::MenuSource> {
		// The point arrives in THIS node's space, which is the space the row geometry answers in -
		// so resolving the row is the same call the drop uses, and a row scrolled out of sight is
		// answered for just as well as one on screen
		return buildContextMenu(getRowIndexAt(req.location));
	});
}

Rc<ui::MenuSource> DndTreeView::buildContextMenu(size_t index) {
	auto source = Rc<ui::MenuSource>::create();
	auto row = getRow(index);

	if (!row || !row->node) {
		// The empty space below the last row belongs to the root, and the only thing the root can
		// be asked is what to do with everything at once
		source->addButton("expand-all", "Expand all", [this](NotNull<ui::MenuSourceButton>) {
			for (size_t i = 0; i < getRowCount(); ++i) {
				auto it = getRow(i);
				if (it && it->node && it->node->isCategory() && !it->expanded) {
					expandRow(i);
				}
			}
			report("expanded every category");
		});
		source->addButton("collapse-all", "Collapse all", [this](NotNull<ui::MenuSourceButton>) {
			// Backwards, so collapsing one cannot renumber a row this walk has not reached yet
			for (size_t i = getRowCount(); i > 0; --i) {
				auto it = getRow(i - 1);
				if (it && it->node && it->node->isCategory() && it->expanded) {
					collapseRow(i - 1);
				}
			}
			report("collapsed every category");
		});
		source->addSeparator("sep-root");
		addCreateSubmenu(source, index);
		return source;
	}

	// The name of the element, for the message - read now, because the callback runs after the
	// menu has closed and the row it was built for may have moved by then
	auto label = row->node->getData().getString("name");

	if (row->node->isCategory()) {
		const bool expanded = row->expanded;

		/* The index is captured here and NOT read back, unlike everywhere else in this class.

		A menu is a decision about the row that was pointed at, and it is made while that row still
		exists: the popup is modal enough that nothing renumbers the tree under it. What the widget
		re-reads on every event is a different problem - a row node outliving its index. */
		source->addButton("toggle", expanded ? "Collapse" : "Expand",
				[this, index, expanded, label](NotNull<ui::MenuSourceButton>) {
			if (expanded) {
				collapseRow(index);
			} else {
				expandRow(index);
			}
			report(toString(expanded ? "collapsed " : "expanded ", label));
		});
		source->addSeparator("sep");
	}

	addCreateSubmenu(source, index);

	source->addButton("rename", "Rename", [this, index](NotNull<ui::MenuSourceButton>) {
		// No name is captured here, unlike the items around it: what the editor opens with is read
		// when it opens, and by then the menu that asked for it has closed
		beginRename(index);
	});

	source->addButton("delete", "Delete", [this, index, label](NotNull<ui::MenuSourceButton>) {
		if (removeRow(index)) {
			report(toString("deleted ", label));
		}
	});
	return source;
}

void DndTreeView::addCreateSubmenu(ui::MenuSource *source, size_t index) {
	/* A SUBMENU, not two items at the top level.

	`New` is one decision followed by a second - what kind - and a menu that spelt both out at the
	top level would put the two rarest items above Rename and Delete. Opening it on hover costs
	nothing to declare: ui::MenuConfig::hover is on by default and carries down every level, so a
	submenu here behaves like a submenu anywhere else in the application. */
	auto items = Rc<ui::MenuSource>::create();

	// The index is captured, like every other item's: a menu is a decision about the row that was
	// pointed at, taken while that row still exists
	items->addButton("new-item", "Item",
			[this, index](NotNull<ui::MenuSourceButton>) { beginCreate(index, false); });
	items->addButton("new-category", "Category",
			[this, index](NotNull<ui::MenuSourceButton>) { beginCreate(index, true); });

	source->addSubmenu("new", "New", sp::move(items));
}

bool DndTreeView::removeRow(size_t index) {
	auto row = getRow(index);
	if (!row || !row->node) {
		return false;
	}

	auto model = getSource();
	if (!model) {
		return false;
	}

	// The selection is an INDEX, not an identity: leaving it where it was would move it onto
	// whatever row slid up into that slot
	if (getSelectedRow() == index) {
		setSelectedRow(maxOf<size_t>());
	}

	// The model's own notification is what rebuilds the rows - the view listens to it - so there
	// is nothing to refresh here
	return model->removeNode(row->node.get());
}

bool DndTreeView::renameNode(ModelNode *node, StringView name) {
	auto model = getSource();
	if (!node || !model) {
		return false;
	}

	auto trimmed = name;
	trimmed.trimChars<StringView::WhiteSpace>();
	if (trimmed.empty()) {
		// Not a name. Said here rather than in either caller, because both mean the same thing by
		// it and the editor's commit turns this one `false` into "the session stays open"
		return false;
	}

	// The whole payload is copied and one key rewritten, rather than a fresh Value holding a name:
	// a row's data belongs to the element, and this demo only happens to keep nothing else in it.
	// A rename must not be what deletes the rest.
	auto value = node->getData();
	value.setString(trimmed, "name");

	// setNodeData bumps the element's revision and posts Update::Data; the view listens to the
	// model, so the row rebuilds itself with the new label and nothing here has to refresh
	model->setNodeData(node, sp::move(value));
	return true;
}

bool DndTreeView::renameRow(size_t index, StringView name) {
	auto row = getRow(index);
	return row && row->node && renameNode(row->node.get(), name);
}

bool DndTreeView::beginRename(size_t index) { return beginEdit(index, ItemId(0)); }

bool DndTreeView::beginEdit(size_t index, ItemId provisional) {
	/* The outgoing editor is settled FIRST, before a single row is read.

	One at a time is the engine's rule (ui::InlineEditSession cancels whatever is open when the next
	one opens), so what is left to decide here is the ENDING: a second Rename is a decision about
	another row, not a reason to throw away what was typed into this one, so it COMMITS. A refused
	commit - a name of nothing but blanks - is cancelled on the spot rather than left standing:
	leaving it open would hand its ending to the session opened below, and by then this call has
	already read the row geometry that a cancel can renumber. */
	if (auto previous = _rename) {
		if (!previous->commit()) {
			previous->cancel();
		}
	}

	auto row = getRow(index);
	if (!row || !row->node) {
		return false;
	}

	/* The row's rectangle, cut back to where its NAME starts.

	Not the whole row: a row is an indent, an expander slot and an icon before the label, and an
	editor over all of it would put the text being edited several columns left of the text it
	replaces. The vertical extent is still the row's - the editor stands in for the row, not for the
	line of text inside it. */
	Rect rect;
	if (!getRowContentRect(index, rect) || rect.size.height <= 0.0f) {
		return false;
	}

	// Cut back to where the scroll bar begins. Only to reject a row with nothing left of it; the
	// box the field actually gets is clamped again below, once its own padding is known.
	const auto right = sprt::min(rect.getMaxX(), getEditorRightLimit());
	rect.size.width = sprt::max(right - rect.origin.x, 0.0f);
	if (rect.size.width <= 0.0f) {
		return false;
	}

	/* The element, by id, and never the index again.

	An edit lasts as long as the author takes over it, and the model is live underneath: a drop, a
	delete or a sort renumbers the rows while the editor sits over one of them. The id is what still
	means the same element when the commit finally arrives. */
	const auto id = row->node->getId();

	ui::InlineTextEditConfig config;
	config.text = row->node->getData().getString("name");
	config.onCommit = [this, id](StringView value) {
		auto model = getSource();
		auto node = model ? model->getNode(id) : nullptr;
		if (!node) {
			// The element went away while it was being typed over. Refusing would leave an editor
			// open over a row that no longer exists, so the value is dropped and the session ends
			report("the element being renamed is gone");
			return true;
		}

		if (!renameNode(node, value)) {
			// A refused commit keeps the session open with the text still in it, which is the only
			// way the author is told the name was not taken
			report("a name cannot be empty");
			return false;
		}

		report(toString("renamed to ", value));
		return true;
	};
	/* Escape is what makes a created element PROVISIONAL, and the only thing that does.

	Not "the name was left empty": an empty name is refused by the commit and the session stays
	open, so the element is still being named. Not a press outside or a scroll either - both commit,
	and an element named `New item` is a perfectly ordinary element. Only an explicit cancel says
	the author changed their mind about creating it at all. */
	config.onCancel = [this, provisional] {
		if (provisional == ItemId(0)) {
			return;
		}
		auto model = getSource();
		if (auto node = model ? model->getNode(provisional) : nullptr) {
			// The selection is an INDEX, so taking THIS row out from under it would move it onto
			// whatever slid up into that slot - and a selection on any other row is none of this
			// call's business. Exactly removeRow's rule, asked of the row being removed.
			if (getSelectedRow() == rowIndexForId(provisional)) {
				setSelectedRow(maxOf<size_t>());
			}
			model->removeNode(node);
			report("discarded the new element");
		}
	};

	// Unconditional, because only one session can be open at a time and this view closes the
	// outgoing one itself before opening the next: whatever is closing here IS what _rename holds.
	config.onClose = [this] {
		_rename = nullptr;
		_provisional = ItemId(0);
	};

	// THIS VIEW is the anchor: its space is where the rectangle keeps its meaning while the rows
	// underneath are destroyed and rebuilt
	_rename = ui::beginInlineTextEdit(this, rect, sp::move(config));
	if (!_rename) {
		return false;
	}

	_provisional = provisional;

	/* The sheet has to be carried to the editor, because the editor is not down here.

	It sits on an overlay of the scene's CONTENT node, outside the subtree the application's
	ui::StyleResolver walks - so without this it comes up as the unstyled white field a bare
	ui::TextInput is. A StyleSystem of its own supplies the rules and a recursive resolver applies
	them to it and its children, which is the whole of what needs styling. */
	if (auto editor = _rename->getEditor()) {
		if (!_stylesheet.empty()) {
			editor->addSystem(Rc<ui::StyleSystem>::create(_stylesheet));
			editor->addSystem(Rc<ui::StyleResolver>::create(true));
		}

		/* The Overlay LEVEL, not merely a high ZOrder - the same thing the drag ghost asks for.

		A label is drawn on RenderingLevel::Surface and an opaque box on Solid, and the levels are
		painted in that order for the whole scene: without this the row's own text comes back on top
		of the field that is supposed to have replaced it, however late the field is in z. */
		editor->setOverlay(true);

		/* Now that the field exists and carries the sheet, ask the sheet how wide its padding is
		and give the session the box that puts the TEXT on the label. Doing it here rather than in
		the config above is what lets the number be READ instead of repeated - see makeEditorRect. */
		_rename->setRect(makeEditorRect(rect, editor));
	}
	return true;
}

float DndTreeView::getEditorRightLimit() const {
	/* Where the scroll bar begins.

	A row spans the whole width of the view, bar included - the bar is drawn OVER the rows, which is
	right for a hairline that fades away and wrong for a field the author is typing into. The strip
	to keep clear is the scroll view's own answer, which reserves the bar's WIDEST thickness rather
	than the one in force, so the editor is not re-laid out when the bar swells under a pointer. */
	auto scroll = getScroll();
	return scroll ? _contentSize.width - scroll->getIndicatorReservedSize() : _contentSize.width;
}

Rect DndTreeView::makeEditorRect(Rect rect, Node *editor) const {
	/* The field's own horizontal padding, READ FROM THE SHEET rather than repeated here.

	It has to be known, because the field draws its text at `left + padding-left` while `rect.left`
	is already on the label: without moving the box out by the same amount, the text would sit one
	padding to the right of the text it replaces. Growing it on both sides cancels that exactly and
	leaves a box slightly wider than the name, which is what a rename box looks like everywhere.

	And it has to be read, because the number belongs to `inline-editor { padding-left/right }` in
	the demo's stylesheet. A constant here would be a second declaration of it, in another file,
	that nobody editing the first would think to change. ui::StyleResolver::resolveStyleForNode
	answers against the sheet installed on the field a moment ago, in this call - a StyleSystem
	publishes itself as it is added, so there is no pass to wait for. */
	auto style = ui::StyleResolver::resolveStyleForNode(editor);
	if (style) {
		const auto fontSize = float(style.font().fontSize.get());
		const auto read = [&](document::ParameterName name, const document::Metric &m) {
			// `auto` is not a length; a sheet that leaves the padding out leaves the box alone
			return style.has(name) && !m.isAuto()
					? style.media().computeValueAuto(m, rect.size.width, fontSize)
					: 0.0f;
		};

		const auto left = read(document::ParameterName::CssPaddingLeft, style.paddingLeft());
		const auto right = read(document::ParameterName::CssPaddingRight, style.paddingRight());
		if (!sprt::isnan(left) && !sprt::isnan(right)) {
			rect.origin.x -= left;
			rect.size.width += left + right;
		}
	}

	// Clamped again, on the grown box this time: what must stay clear of the bar is what is drawn.
	const auto right = sprt::min(rect.getMaxX(), getEditorRightLimit());
	rect.size.width = sprt::max(right - rect.origin.x, 0.0f);
	return rect;
}

size_t DndTreeView::rowIndexForId(ItemId id) const {
	if (id == ItemId(0)) {
		return maxOf<size_t>();
	}
	auto rows = getRows();
	for (size_t i = 0; i < rows.size(); ++i) {
		if (rows[i].node && rows[i].node->getId() == id) {
			return i;
		}
	}
	return maxOf<size_t>();
}

DndTreeView::InsertPoint DndTreeView::getInsertPoint(size_t index) const {
	InsertPoint ret;

	auto model = getSource();
	if (!model) {
		return ret;
	}

	auto row = getRow(index);
	if (!row || !row->node || row->node->isSpan()) {
		// The empty space below the last row, or a row that is not an element: the root is what
		// answers, exactly as it does for a drop there
		ret.parent = model->getRoot();
		return ret; // index stays maxOf: append
	}

	if (row->node->isCategory()) {
		/* Into it, and FIRST rather than appended.

		Appending is what a file manager does, but a category here may be long and already open, and
		the row the author is about to type into would then be built somewhere below the fold - an
		editor over a row that is not on screen. First puts the new row directly under the one that
		was clicked, which is where the author is looking. */
		ret.parent = row->node;
		ret.index = 0;
		return ret;
	}

	// A leaf is a position: the new element stands directly after it, among its siblings
	ret.parent = row->node->getParent();
	ret.index = row->node->getChildIndex() + 1;
	return ret;
}

bool DndTreeView::beginCreate(size_t index, bool category) {
	auto model = getSource();
	if (!model) {
		return false;
	}

	// The outgoing editor, settled before anything is read or written - the same rule and the same
	// reason as beginEdit, and here it also matters that a cancel can take a provisional element
	// back out, which renumbers the very rows the insert point below is resolved against.
	if (auto previous = _rename) {
		if (!previous->commit()) {
			previous->cancel();
		}
	}

	// A closed category has to open before anything is put inside it, or the row the author is
	// about to name is created where they cannot see it. Done before the insert point is resolved
	// only because expanding renumbers the rows BELOW this one - this row's own index is unmoved.
	if (auto row = getRow(index); row && row->isCategory() && !row->expanded) {
		expandRow(index);
	}

	auto point = getInsertPoint(index);
	if (!point.parent) {
		return false;
	}

	/* A real name, not an empty one.

	The editor opens with it selected, so the first keystroke replaces it and it costs the author
	nothing; and it means that every ending EXCEPT an explicit cancel leaves a named element behind.
	An empty placeholder would make a press outside produce a row with no label. */
	Value data;
	data.setString(category ? "New category" : "New item", "name");

	auto node = category ? model->emplaceCategory(point.parent, point.index, sp::move(data))
						 : model->emplaceItem(point.parent, point.index, sp::move(data));
	if (!node) {
		return false;
	}

	/* Re-derive the rows NOW rather than waiting for the model's own notification.

	That notification is a scheduler tick, and a tick only happens on a frame: a model change on its
	own does not dirty the scene, so an idle window would sit on the old rows until something else
	woke it. This both re-derives the rows in this call - which is what gives the new element a row
	index to be found by - and marks the view dirty, which is what asks for the frame that builds
	the row node the editor is waiting for. */
	invalidateSource();

	_pendingEdit = node->getId();
	_pendingStage = PendingStage::Rows;

	/* Asked for, with an answer - not watched for.

	The call above re-derives the rows here and now, but the row NODES are rebuilt at the start of
	the view's next visit, and where a row's content starts is a layout result. requestRebuildNodes
	says "tell me when that has happened", and the answer arrives at the one moment it is complete:
	inside the rebuild, with the new rows already styled, sized and placed. Nothing polls, and
	nothing counts frames - what the callback finds is final. */
	requestRebuildNodes([this] { settlePendingEdit(); });

	report(toString("naming a new ", category ? "category" : "item"));
	return true;
}

/* Run from ui::TreeView's rebuild callback, which is the moment the rows are current AND laid out.

There is nothing left to wait for by then, so every branch below DECIDES. That is what the frame
counter this used to carry was hiding: it served two questions with one guess - "the rebuild has not
run yet", which is now answered by being called at all, and "this row will never have a node", which
is answered here, by scrolling to it. */
void DndTreeView::settlePendingEdit() {
	if (_pendingEdit == ItemId(0)) {
		return;
	}

	/* Gone from the MODEL, not merely absent from the rows.

	The two are different questions and the model is the one that matters: a row holds an Rc to its
	element, so a removal is still listed until the next rebuild. Whoever took the element out -
	another view, a delete, the self-check - has already answered what should happen to it. */
	auto model = getSource();
	if (!model || !model->getNode(_pendingEdit)) {
		_pendingEdit = ItemId(0);
		return;
	}

	const auto index = rowIndexForId(_pendingEdit);
	if (index == maxOf<size_t>()) {
		/* In the model and not among the rows. Unreachable rather than slow: beginCreate re-derives
		the rows in its own call, and the parent it inserted under is either a category it opened,
		the parent of a row that is on the list, or the root. Saying so beats waiting for a row that
		is not coming. */
		_pendingEdit = ItemId(0);
		report("the new element has no row - use Rename to name it");
		return;
	}

	// The row's NODE is what carries the answer: without one getRowContentRect falls back to the
	// whole row, and the editor would be placed at the view's edge rather than over the label.
	if (!getRowNode(index)) {
		if (_pendingStage == PendingStage::Rows && revealRow(index)) {
			/* The only reason a row that EXISTS has no node: it is outside the scroll window, and
			only what is inside gets built. So this is not something to wait through - the view is
			pointed at the row, which is what the author expects of a row they just made anyway, and
			the rebuild that follows brings the answer back here. */
			_pendingStage = PendingStage::Reveal;
			requestRebuildNodes([this] { settlePendingEdit(); });
			return;
		}

		/* Revealed and still not built. Not a wait: there is nothing left to try, and an editor
		placed on a rectangle off the screen is one the author can neither see nor answer. The
		element STAYS - it is a real element with a real name, which is exactly why it was given
		one - and Rename is how it gets another. Removing it would be the worse answer: silently
		undoing what was asked for because the view could not show it. */
		_pendingEdit = ItemId(0);
		report("the new element is out of view - use Rename to name it");
		return;
	}

	const auto id = _pendingEdit;
	_pendingEdit = ItemId(0);

	if (beginEdit(index, id)) {
		setSelectedRow(index);
	}
}

bool DndTreeView::revealRow(size_t index) {
	auto scroll = getScroll();
	// The CONST overload: the other one marks the item list dirty, and reading a position is not a
	// reason to cost a rebuild
	const auto controller = static_cast<const basic2d::ScrollController *>(getController());
	if (!scroll || !controller) {
		return false;
	}

	auto &items = controller->getItems();
	if (index >= items.size()) {
		return false;
	}

	/* The row's own offset along the scroll axis, straight off its item.

	Not derived from getRowRect: that answers in THIS node's space, for drawing over, while a scroll
	position is the controller's own axis - and the item carries exactly that number for every row,
	built or not. `pos` puts the row at the TOP of the window, which is where a row created below
	the fold should appear; clamped to the end so the last one does not scroll into empty space. */
	const auto length = scroll->getScrollLength();
	const auto target = scroll->getNodeScrollPosition(items.at(index).pos);
	if (sprt::isnan(target) || sprt::isnan(length)) {
		return false;
	}

	scroll->setScrollPosition(
			sprt::clamp(target, 0.0f, sprt::max(length - scroll->getScrollSize(), 0.0f)));
	return true;
}

void DndTreeView::report(StringView message) {
	if (_message) {
		_message(message);
	}
}

auto DndTreeView::payloadOf(const DragEvent &event) -> DndItemPayload * {
	if (!event.data || !event.data->isLocal(DndItemPayload::TypeName)) {
		return nullptr;
	}
	// The type tag has already answered whose payload this is, so the cast cannot be wrong - but
	// a dynamic_cast is what keeps that true if a second local type is ever added here.
	return dynamic_cast<DndItemPayload *>(event.data->getLocal());
}

bool DndTreeView::canAccept(const DndItemPayload *payload, const DropPosition &spot) const {
	if (!payload || !payload->node || !payload->model || !spot.valid()) {
		return false;
	}

	auto model = getSource();
	if (!model || !spot.parent->isCategory()) {
		return false;
	}

	// Inside one model this would detach a cycle from the tree (Model::moveNode refuses it anyway);
	// across two it would make cloneInto() walk a child list it is appending to. Refusing in the
	// PREDICATE is what turns both into a NoDrop cursor instead of a silent no-op.
	if (payload->model.get() == model
			&& isSelfOrDescendantOf(spot.parent.get(), payload->node.get())) {
		return false;
	}

	// A drop that would put the element back exactly where it already is does nothing, and saying so
	// in the PREDICATE matters beyond tidiness: at the moment a drag begins the pointer sits on the
	// source row, so accepting would light that row up as a drop target - and, for a ghost made of
	// the frame, that highlight would be copied straight into the ghost.
	if (payload->model.get() == model && payload->node->getParent() == spot.parent.get()) {
		const auto current = payload->node->getChildIndex();
		const auto target =
				(spot.index == maxOf<size_t>()) ? spot.parent->getChildCount() : spot.index;
		if (target == current || target == current + 1) {
			return false;
		}
	}

	return true;
}

bool DndTreeView::applyTransfer(DndItemPayload *payload, const DropPosition &spot,
		DragActions action) {
	if (!canAccept(payload, spot)) {
		return false;
	}

	auto model = getSource();
	const bool sameModel = (payload->model.get() == model);

	if (action == DragActions::Move && sameModel) {
		size_t index = spot.index;

		// Model::moveNode reads `index` in the child list AS IT WILL BE once the node is taken out
		// of its current place. A spot derived from a sibling BELOW the node is therefore one slot
		// too far - and this is the only place that knows both numbers.
		if (payload->node->getParent() == spot.parent.get() && index != maxOf<size_t>()
				&& payload->node->getChildIndex() < index) {
			--index;
		}

		if (!model->moveNode(payload->node.get(), spot.parent.get(), index)) {
			return false;
		}

		// the element is already where it belongs; the source must not delete it afterwards
		payload->consumed = true;
		report(toString("moved '", payload->title, "' inside ", _title));
		return true;
	}

	size_t created = 0;
	if (!cloneInto(model, payload->node.get(), spot.parent.get(), spot.index, created)) {
		return false;
	}
	_clones += created;

	if (action == DragActions::Move) {
		// a cross-model move: the copy is in place, and the source's completion deletes the
		// original - which is what `consumed` staying false says.
		report(toString("moved '", payload->title, "' from ", payload->origin, " to ", _title));
	} else {
		report(toString("copied '", payload->title, "' from ", payload->origin, " to ", _title));
	}
	return true;
}

void DndTreeView::finishTransfer(DndItemPayload *payload, DragActions action) {
	if (!payload || !payload->model || !payload->node) {
		return;
	}

	// DragActions::None means the drag ended without a drop - cancelled, refused, or dropped
	// nowhere - and then nothing at all happened to the original.
	if (action != DragActions::Move || payload->consumed) {
		return;
	}

	payload->model->removeNode(payload->node.get());
}

Rc<DndItemPayload> DndTreeView::makePayload(size_t index) const {
	auto row = getRow(index);
	if (!row || !row->node || row->isSpanItem()) {
		return nullptr; // the items inside a span are a length, not elements: nothing to carry
	}

	auto model = getSource();
	if (!model || row->node.get() == model->getRoot()) {
		return nullptr;
	}

	auto payload = Rc<DndItemPayload>::create();
	payload->model = model;
	payload->node = row->node;
	payload->title = row->getData().getString(getLabelKey());
	payload->origin = _title;
	return payload;
}

FrameCapture *DndTreeView::getFrameCapture() const {
	auto director = getDirector();
	auto server = director ? director->getRenderServer() : nullptr;
	// In local mode the render server IS the window; later it may be a network proxy, which has no
	// capture to offer. A dynamic_cast is what turns that into a null rather than into a call
	// through a pointer to something else - the same shape ui::ColorField and ui::ContextMenuSystem
	// use to reach the window.
	auto window = dynamic_cast<AppWindow *>(server);
	return window ? window->getFrameCapture() : nullptr;
}

bool DndTreeView::requestGhostCapture(size_t index, DragSource *source, DragOffer &offer) {
	auto capture = getFrameCapture();
	if (!capture || !capture->isAvailable() || !source) {
		return false;
	}

	auto row = getRowNode(index);
	auto director = getDirector();
	if (!row || !director) {
		return false;
	}

	// The row's box in WORLD space - not its world origin plus its content size, which mixes two
	// spaces and silently halves the region on a HiDPI surface. makeRegion does the projection and
	// the y flip, and clamps a row that is half-scrolled out of the view.
	const auto &constraints = director->getFrameConstraints();

	const auto region =
			FrameCapture::makeRegion(row->getWorldBoundingBox(), director->getGeneralProjection(),
					Extent2(constraints.extent.width, constraints.extent.height));

	const auto density = constraints.density > 0.0f ? constraints.density : 1.0f;

	auto target = capture->request(region,
			[source = Rc<DragSource>(source), density](FrameCaptureTarget *target) {
		// Null once the drop has already happened - a capture that lands late is a no-op, not a
		// node parked on the scene forever.
		auto session = source->getSession();
		if (!session || !target->isReady()) {
			return;
		}
		session->setDecorator(makeCaptureGhost(target, density));
	});

	if (!target) {
		return false;
	}

	/* Nothing follows the pointer until the copy lands - not for safety, but because there is
	nothing to show yet: the sprite has no texture until the capture arrives.

	It used to be the safety too. The pointer sits on the row being copied, so a ghost parked now
	would be drawn over it and photographed with it. That is no longer possible: the decorator goes
	on the Overlay level, which draws after the frame has been captured, so the exclusion holds
	however and whenever the ghost was made. */
	offer.decoratorDeferred = true;
	return true;
}

bool DndTreeView::fillOffer(size_t index, DragSource *source, DragOffer &offer) {
	auto payload = makePayload(index);
	if (!payload) {
		return false; // a plain refusal, not an error: this row is not draggable
	}

	offer.local = payload;
	offer.localType = DndItemPayload::TypeName.str<Interface>();
	offer.label = payload->title;

	// No `types`/`encode`: this drag is between two trees of one application and never leaves it,
	// so the OS-shaped half of the payload would be a MIME type nothing asks for and a copy of the
	// row's name made for nobody. A row that wanted to be droppable into a ui::TextInput, or
	// copyable to the clipboard, would declare them - and would have a reader for them.

	// Move is what a tree of elements does by default; Ctrl asks for a Copy, and the target has the
	// last word on both.
	offer.allowedActions = DragActions::Move | DragActions::Copy;
	offer.defaultAction = DragActions::Move;

	// Parked inside the styled subtree, not on the scene content: a StyleResolver only sees its
	// own, and the drawn ghost below takes its whole look from the sheet. It is also where the
	// captured ghost goes, so this is set whichever of the two is used.
	offer.decoratorParent = _ghostParent;

	// A cutout of the row itself where the window can produce one; the drawn ghost is the fallback
	// for a backend or a surface that cannot, and looks like a row rather than being one.
	if (!requestGhostCapture(index, source, offer)) {
		offer.decorator = [title = payload->title]() -> Rc<Node> { return makeDragGhost(title); };
	}

	// Runs exactly once, whatever ended the drag. An Rc on the view, because the completion may run
	// after the row that started the drag has been rebuilt out of existence by the drop itself.
	offer.completion = [self = Rc<DndTreeView>(this), payload](DragActions action) {
		finishTransfer(payload, action);
		if (action == DragActions::None) {
			self->report(toString("'", payload->title, "' was dropped nowhere"));
		} else {
			// the target has already said what it did, but it said so BEFORE the line above took
			// the original away: an empty message re-reads the counts and keeps the words
			self->report(StringView());
		}
	};

	return true;
}

Rc<Node> DndTreeView::buildRowNode(RowBuilder &builder) {
	auto node = ui::TreeView::buildRowNode(builder);

	// A row callback that took the row over completely (RowBuilder::setNode) yields something that
	// is not a RowNode, and then there is no index to read back. This demo never does that, but the
	// hook has to survive one that would.
	if (auto row = dynamic_cast<RowNode *>(node.get())) {
		attachRowHandlers(row);
	}
	return node;
}

void DndTreeView::attachRowHandlers(RowNode *row) {
	// `row` is captured raw and `this` with it: the systems below are owned BY that node, which is
	// owned by the controller this view owns, so neither pointer can outlive the lambda holding it.
	// An Rc either way would be a cycle.

	// The builder is installed after the source exists because it needs the source itself: a
	// capture-backed ghost is handed to that source's session once the copy lands.
	auto source = row->addSystem(Rc<DragSource>::create(nullptr));
	source->setOfferBuilder([this, row, source](DragOffer &offer) {
		return fillOffer(row->getRowIndex(), source, offer);
	});
}

void DndTreeView::attachDropSlots() {
	/* ONE seam for the whole tree, rows and empty space alike.

	There used to be a drop target per row plus one on the view, and every one of them had to
	re-derive where a drop would land and light itself up for it. All of that is ui::TreeView's now:
	it resolves the zone from the pointer, draws the insertion line or the highlight, and opens a
	closed category the drag rests on. What is genuinely this demo's - whether a payload may go to a
	place, and what putting it there means for two models - is what is left. */
	setDropSlots(ui::TreeView::DropSlots{
		.accept = [this](const DragEvent &event, const DropPosition &pos) -> DragActions {
		auto payload = payloadOf(event);
		if (!payload || !canAccept(payload, pos)) {
			return DragActions::None; // not here: the search continues under this view
		}
		// never a bare constant: answering with more than the source offered would claim an
		// action it cannot perform
		return event.allowed & (DragActions::Move | DragActions::Copy);
	},
		.drop =
				[this](const DragEvent &event, const DropPosition &pos, DragActions action) {
		auto payload = payloadOf(event);
		return payload && applyTransfer(payload, pos, action);
	},
	});
}

} // namespace stappler::xenolith::examples
