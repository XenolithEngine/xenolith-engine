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

#include "dndtree/DndTreeDemoLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiStyleSystem.h" // StyleSystem: the rule-supplying half of the stylesheet pair
#include "XLUiButton.h"
#include "XLUiLayoutSystem.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLScene.h"
#include "XL2dSceneContent.h"
#include "XLSceneInspector.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

static constexpr float RowHeight = 26.0f;
static constexpr float CategoryHeight = 30.0f;
static constexpr float BarHeight = 44.0f;

// The stylesheet. `tree-row` / `.tree-toggle` / `.tree-label` and the two per-node variables
// (--tree-depth, --tree-row-h) are ui::TreeView's documented contract; `.drop-into` and
// `.drop-root` are this demo's own, flipped by the drop targets' enter/leave slots and by nothing
// else - which is what keeps the highlight out of the `accept` predicate.
static constexpr auto s_css = StringView(R"css(
:root {
	--tree-indent: 18px;
	--surface: #262a31;
	--row-hover: #333a45;
	--row-selected: #1565c0;
	--drop: #2e7d32;
	--text: #e8eaed;
	--text-dim: #9aa4b2;
}

/* --- the dock chrome ------------------------------------------------------------------------ */
/* `dock-frame-body` is deliberately NOT styled: DockFrame builds the flex run inside it itself,
   with the direction its tab strip needs, and a `display:flex` here would reconfigure that run
   from the sheet's defaults. The inset the body would have given is a margin on the tree instead. */
dock-frame          { background-color: #1f2227; }
dock-tab-bar        { background-color: #1a1d22; }
dock-tab            { display: flex; padding: 5px 14px; background-color: #2b3038; }
dock-tab.active     { background-color: #3a414d; }
dock-tab > label    { color: var(--text); font-size: 13px; }
dock-splitter       { background-color: #14161a; }
dock-drop-indicator { background-color: #3d7ecf; }

/* --- the two trees -------------------------------------------------------------------------- */
/* A tree cannot measure itself, so it would come out of the frame's column with no height at all:
   `flex: 1` is what makes it take the whole body. */
tree-view {
	background-color: var(--surface);
	border-radius: 4px;
	margin: 6px;
	flex-grow: 1;
	flex-shrink: 1;
	flex-basis: 0px;
}

/* ui::TreeView carries the drop feedback itself; the sheet only says what it looks like.
   `drop-root` is on the VIEW while the pointer is over the empty space below the last row, which is
   the one place that answers for the root. The others are nodes the widget puts up over the row it
   resolved: a fill for "into this category", and a bar with an upright at its left end for "between
   these two rows" - both starting at that row's own indent rather than at the view's edge. */
tree-view.drop-root  { outline-color: var(--drop); outline-width: 2px; outline-style: solid; }
tree-drop-highlight  { background-color: var(--drop); opacity: 0.55; }
/* the upright is a CHILD of the line, not a node that inherits from it, so it is named too */
tree-insertion-line,
tree-insertion-stem { background-color: #fcb400; }

tree-row {
	/* a row is a Panel, and an unstyled Panel is an opaque WHITE surface - the view's own
	   background is what should show through a row nobody is pointing at */
	background-color: transparent;
	display: flex;
	flex-direction: row;
	align-items: center;
	height: var(--tree-row-h);
	padding-left: calc(6px + var(--tree-depth, 0) * var(--tree-indent));
	padding-right: 8px;
	column-gap: 6px;
	border-radius: 3px;
}

tree-row:hover      { background-color: var(--row-hover); }
tree-row.selected   { background-color: var(--row-selected); }
tree-row.expanded > .tree-label, tree-row.collapsed > .tree-label { font-weight: bold; }

.tree-toggle {
	flex: 0 0 18px;
	height: 18px;
	border-radius: 9px;
	background-color: transparent;
	display: flex;
	justify-content: center;
	align-items: center;
}
.tree-toggle:hover  { background-color: #48515f; }
.tree-toggle > icon { width: 16px; height: 16px; color: var(--text); }

.tree-icon  { flex: 0 0 16px; width: 16px; height: 16px; color: var(--text-dim); }
.tree-label { flex-grow: 1; color: var(--text); font-size: 14px; white-space: nowrap; }

/* --- the context menu ----------------------------------------------------------------------- */
/* The trees declare a context menu; the menu opens as a popup, and a popup is a SCENE of its own -
   the ui::StyleSystem carrying this sheet lives on THIS window's content and does not reach it. The
   demo hands the same literal to ui::ContextMenuSystem, which is why these rules can be here and
   why the variables above resolve inside the menu as well. */
menu {
	background-color: #2b3038;
	border-radius: 6px;
	outline-color: #14161a;
	outline-width: 1px;
	outline-style: solid;
}

/* A row is a ui::Button under the type `menu-item`, so the interactive pseudo-classes are the
   ordinary ones. `transparent` rather than the menu's own colour: the surface is what shows
   through a row nobody is pointing at, exactly as with `tree-row` above. */
menu-item          { background-color: transparent; border-radius: 4px; }
menu-item:hover    { background-color: var(--row-hover); }
menu-item:active   { background-color: #3f4854; }
menu-item:disabled { background-color: transparent; }

menu-item > label            { color: var(--text); font-size: 13px; }
menu-item:disabled > label   { color: #5c6470; }

/* The four slots a row can carry, each its own node and its own type: the leading icon, the
   accelerator, the second line, and the chevron on a row that opens a submenu. */
.xl-ui-menu-item-icon { color: var(--text); }
menu-item-shortcut    { color: var(--text-dim); font-size: 12px; }
menu-item-subtitle    { color: var(--text-dim); font-size: 11px; }
menu-item-trailing    { color: var(--text-dim); }
menu-item:hover > menu-item-trailing { color: var(--text); }

/* The separator is a Panel of its own inside the row that stands for it, so the colour goes on the
   line and not on the row. */
menu-separator { background-color: #3f4854; }

/* --- renaming a row ------------------------------------------------------------------------- */
/* The editor is an overlay in THIS scene, not a popup, so these ordinary rules reach it - unlike
   the menu rules above, which have to be handed to the popup separately. It is sized by the
   rectangle it was opened over (the row's), so there is no width or height here: what a sheet has
   to say is only what it looks like. */
inline-editor {
	background-color: #1b1e24;
	outline-color: #fcb400;
	outline-width: 1px;
	outline-style: solid;
	border-radius: 3px;
	padding-left: 6px;
	padding-right: 6px;
	color: #ffffff;
	font-size: 14px;
}

/* --- the scroll bars ------------------------------------------------------------------------ */
/* ui::TreeView hands its indicator to the stylesheet for itself (ui::useStyledScrollIndicator), so
   these are ordinary rules on two ordinary nodes. The TRACK is what the pointer is over - it is the
   node carrying the input - which is why `:hover` belongs on it and not on the thumb. `.active` is
   on both while a pointing device is attached, i.e. while the bar can be grabbed at all; without
   one it is thin, inert and fades away after a scroll, which is what a touch list does.

   Two things are deliberately NOT here. The WIDTH: the view rewrites the content size of both nodes
   on every scroll, so a width from a sheet would be overwritten within the frame - that one is
   setIndicatorThickness()'s to say. And the resting OPACITY, which the view animates: these colours
   are what it fades in and out, so they are opaque and the fade does the blending. */
scroll-indicator-track       { background-color: #0d0f12; border-radius: 4px; }
scroll-indicator-track:hover { background-color: #05070a; }

scroll-indicator             { background-color: #6f7b8d; border-radius: 4px; }
scroll-indicator.active      { background-color: #97a4b6; }

/* --- what follows the pointer --------------------------------------------------------------- */
/* An explicit size, because the ghost is parked under a node with no layout of its own: there is
   nothing above it to resolve a `fit-content` against. */
.drag-ghost {
	-xl-anchor-point: 0.5 0.5;
	width: 220px;
	height: 28px;
	display: flex;
	flex-direction: row;
	align-items: center;
	padding-left: 10px;
	padding-right: 10px;
	background-color: #1e88e5;
	border-radius: 4px;
}
.drag-ghost > label { color: #ffffff; font-size: 13px; white-space: nowrap; }

/* --- the demo's own chrome ------------------------------------------------------------------ */
.demo-bar {
	display: flex;
	flex-direction: row;
	align-items: center;
	column-gap: 12px;
	padding-left: 14px;
	padding-right: 14px;
	background-color: #14161a;
}

.demo-button {
	flex: 0 0 130px;
	height: 30px;
	background-color: #3949ab;
	border-radius: 5px;
	display: flex;
	justify-content: center;
	align-items: center;
}
.demo-button:hover  { background-color: #4a5ac4; }
.demo-button > label { color: #ffffff; font-size: 13px; }

.status { flex-grow: 1; color: #9ecbff; font-size: 14px; white-space: nowrap; }
)css");

static data::Model::Node *addCategory(data::Model *model, data::Model::Node *parent,
		StringView name) {
	data::Model::Value value;
	value.setString(name, "name");
	return model->emplaceCategory(parent, maxOf<size_t>(), sp::move(value));
}

static data::Model::Node *addItem(data::Model *model, data::Model::Node *parent, StringView name) {
	data::Model::Value value;
	value.setString(name, "name");
	return model->emplaceItem(parent, maxOf<size_t>(), sp::move(value));
}

// `count` generated leaves under a generated category. Everything the demo shows comes from here:
// there is no resource to load and nothing to keep in sync with the tree.
static void addGroup(data::Model *model, StringView group, StringView prefix, size_t count) {
	auto category = addCategory(model, model->getRoot(), group);
	for (size_t i = 1; i <= count; ++i) {
		addItem(model, category, toString(prefix, " ", (i < 10 ? "0" : ""), i));
	}
}

static Rc<data::Model> makeLibraryModel() {
	data::Model::Value root;
	root.setString("Library", "name");

	auto model = Rc<data::Model>::create(sp::move(root));
	if (!model) {
		return nullptr;
	}

	// Deliberately more rows than fit: a list that does not overflow has no scroll bar and no edge
	// to auto-scroll at, so a demo of dragging BETWEEN two trees would be showing neither. Three
	// groups is what the self-check reads, so the length is in the leaves under them.
	addGroup(model, "Widgets", "Widget", 12);
	addGroup(model, "Shapes", "Shape", 10);
	addGroup(model, "Icons", "Icon", 14);
	return model;
}

static Rc<data::Model> makeProjectModel() {
	data::Model::Value root;
	root.setString("Project", "name");

	auto model = Rc<data::Model>::create(sp::move(root));
	if (!model) {
		return nullptr;
	}

	// Long enough to overflow as well, so the RECEIVING tree has an edge to auto-scroll at: a drop
	// past the bottom of what is visible is the case the whole scroll-while-dragging path exists
	// for. The rest of this model is shape rather than length - the self-check reads all of it.
	addGroup(model, "Scene A", "Node", 26);
	// deliberately empty: a category with no children is the case a per-ROW drop target has to
	// handle on its own, since there is no child row under the pointer to answer for it
	addCategory(model, model->getRoot(), "Scene B");
	addItem(model, model->getRoot(), "Loose item 01");
	addItem(model, model->getRoot(), "Loose item 02");
	return model;
}

// Open every category, including the ones that only appear once their parent is open. expandRow()
// re-derives the row list synchronously, so the rows added by one expansion land after `i` and are
// picked up by the same walk.
static void expandAll(ui::TreeView *view) {
	for (size_t i = 0; i < view->getRowCount(); ++i) {
		if (auto row = view->getRow(i); row && row->isCategory() && !row->expanded) {
			view->expandRow(i);
		}
	}
}

// Backwards, so collapsing a category cannot renumber a row this walk has not reached yet.
static void collapseAll(ui::TreeView *view) {
	for (size_t i = view->getRowCount(); i > 0; --i) {
		if (auto row = view->getRow(i - 1); row && row->isCategory() && row->expanded) {
			view->collapseRow(i - 1);
		}
	}
}

} // namespace

bool DndTreeDemoLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	// The stylesheet is a system that only SUPPLIES rules; the recursive resolver below is what
	// applies them to every node in this subtree - frames, tabs, rows, the drag ghost and their
	// labels alike.
	addSystem(Rc<ui::StyleSystem>::create(s_css));
	addSystem(Rc<ui::StyleResolver>::create(true));

	_background = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(0));
	_background->setAnchorPoint(Anchor::BottomLeft);

	makeControlBar();
	buildDock();

	/* XL_DNDTREE_SELFCHECK gates the AUTOMATIC run, and nothing else.

	A test that runs on every start of an example costs everyone who opens it a full round of
	transfers through both models before they see anything, and then undoes them. So it is off by
	default - but the check itself is always compiled in and always reachable: `dndtree.selfcheck`
	with `run` performs it on demand, from the same call below, at any point in the session. */
	if (auto value = ::getenv("XL_DNDTREE_SELFCHECK"); value && StringView(value) != "0") {
		performSelfCheck();
	} else {
		// The demo's declared starting state - the same call the Reset button makes, and the same
		// one performSelfCheck() ends with, so there is one description of what that state is.
		resetContent();
	}

	refreshStatus("drag a row from one tree into the other");

	return true;
}

void DndTreeDemoLayout::buildDock() {
	// Exactly once. A second call would hang a SECOND DockSystem on the same node - two systems
	// writing one set of frame rectangles - and would leave the panel builders below registered
	// against models the trees they already built are not showing.
	if (_dock) {
		return;
	}

	_libraryModel = makeLibraryModel();
	_projectModel = makeProjectModel();

	// The dock's owner must NOT carry a LayoutSystem of its own - the system writes every frame's
	// geometry directly, and two writers would fight (handleAdded asserts it). A plain Node is
	// exactly that. DockSystem distributes over _owner->getContentSize(), so this node has to be
	// sized in handleContentSizeDirty().
	_dockRoot = addChild(Rc<Node>::create(), ZOrder(1));
	_dockRoot->setAnchorPoint(Anchor::BottomLeft);

	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(6.0f);

	// The builder runs at most once, on first show, and the node it returns is kept by the dock
	// across every move - so a tree dragged to the other side of the split keeps its scroll
	// position, its expansion and its selection.
	ui::DockPanelDescriptor library;
	library.id = String("library");
	library.title = String("Library");
	library.icon = basic2d::IconName::File_folder_solid;
	library.minSize = Size2(240.0f, 200.0f);
	// Neither Closable nor Movable: a tab that can be closed or dragged into the other frame is a
	// SECOND drag & drop in an app about the first one, and it can end with a tree that is not on
	// screen at all. The split itself is still draggable, which is the part worth keeping.
	library.flags = ui::DockPanelFlags::None;
	library.builder = [this]() -> Rc<Node> {
		auto tree = makeTree(_libraryModel, "Library");
		_left = tree;
		return tree;
	};
	_dock->registerPanel(sp::move(library));

	ui::DockPanelDescriptor project;
	project.id = String("project");
	project.title = String("Project");
	project.icon = basic2d::IconName::Action_list_solid;
	project.minSize = Size2(240.0f, 200.0f);
	project.flags = ui::DockPanelFlags::None;
	project.builder = [this]() -> Rc<Node> {
		auto tree = makeTree(_projectModel, "Project");
		_right = tree;
		return tree;
	};
	_dock->registerPanel(sp::move(project));

	// One split, two parking places. Both frames are Permanent: closing the last panel of a frame
	// would otherwise collapse it, and a demo about moving things BETWEEN two trees should not be
	// able to end up with one.
	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(0.5f,
			Spec::leaf({String("library")},
					{.name = String("left-frame"),
						.flags = ui::DockFrameFlags::Default | ui::DockFrameFlags::Permanent}),
			Spec::leaf({String("project")},
					{.name = String("right-frame"),
						.flags = ui::DockFrameFlags::Default | ui::DockFrameFlags::Permanent})));
}

Rc<DndTreeView> DndTreeDemoLayout::makeTree(data::Model *model, StringView title) {
	auto tree = Rc<DndTreeView>::create(model, title);

	tree->setLabelKey("name");
	tree->setRowHeight(RowHeight);
	// resolved during the geometry pass, before any row node exists - a group is taller than the
	// leaves under it
	tree->setRowHeightCallback([](const ui::TreeView::Row &row) {
		return row.isCategory() ? CategoryHeight : RowHeight;
	});

	tree->setRowCallback([](ui::TreeView::RowBuilder &builder) {
		if (builder.isExpandable()) {
			builder.setIcon(builder.isExpanded() ? basic2d::IconName::File_folder_open_solid
												 : basic2d::IconName::File_folder_solid);
		} else {
			builder.setIcon(basic2d::IconName::Action_label_solid);
		}
	});

	// Selection is what arms `.tree-row:hover` and `.tree-row.selected`: without a callback a row
	// gets no input listener at all, and neither rule can ever match.
	tree->setSelectionEnabled(true);
	tree->setSelectCallback([tree = tree.get()](size_t index, const ui::TreeView::Row &row) {
		if (row.isCategory()) {
			tree->toggleRow(index);
		}
	});

	// The ghost takes its look from `.drag-ghost` in the sheet above, and a StyleResolver only ever
	// sees its own subtree - so it has to be parked under the node that carries this one.
	tree->setGhostParent(this);
	tree->setMessageCallback([this](StringView message) { refreshStatus(message); });

	return tree;
}

void DndTreeDemoLayout::makeControlBar() {
	if (_controlBarRow) {
		return;
	}

	// A flex row: it sizes itself from its items, so nothing here has to know how many buttons
	// there are or how wide they turned out.
	_controlBarRow = addChild(Rc<Node>::create(), ZOrder(2));
	_controlBarRow->setAnchorPoint(Anchor::TopLeft);
	_controlBarRow->addStyleClass("demo-bar");
	_controlBarRow->addSystem(Rc<ui::LayoutSystem>::create(ui::FlexLayoutInfo{
		.direction = ui::FlexDirection::Row,
		.alignItems = ui::FlexAlign::Center,
		.columnGap = 12.0f,
	}));

	_statusLabel = _controlBarRow->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_statusLabel->addStyleClass("status");

	makeControl("Expand all", [this] {
		expandAll(_left);
		expandAll(_right);
		refreshStatus("every category opened");
	});
	makeControl("Collapse all", [this] {
		collapseAll(_left);
		collapseAll(_right);
		refreshStatus("every category closed");
	});
	makeControl("Reset content", [this] { resetContent(); });
}

ui::Button *DndTreeDemoLayout::makeControl(StringView label, Function<void()> &&action) {
	auto button = _controlBarRow->addChild(Rc<ui::Button>::create(sp::move(action)), ZOrder(2));
	button->setString(label);
	button->addStyleClass("demo-button");
	return button;
}

void DndTreeDemoLayout::refreshStatus(StringView lastAction) {
	if (!_statusLabel) {
		return;
	}

	if (!lastAction.empty()) {
		_lastAction = lastAction.str<Interface>();
	}

	const auto left = _left && _left->getSource() ? _left->getSource()->getNodeCount() : 0;
	const auto right = _right && _right->getSource() ? _right->getSource()->getNodeCount() : 0;

	// getNodeCount() counts the root too, and a root is not something anybody dragged there.
	_statusLabel->setString(toString("Library: ", left ? left - 1 : 0, "   Project: ",
			right ? right - 1 : 0, "   |   Ctrl = copy, Shift = move, right click = menu   |   ",
			_lastAction.empty() ? StringView("idle") : StringView(_lastAction)));
}

void DndTreeDemoLayout::resetContent() {
	_libraryModel = makeLibraryModel();
	_projectModel = makeProjectModel();

	// setSource() drops the expansion and the selection with the model they were keyed by, and
	// re-derives the rows synchronously - the panel node itself is untouched.
	if (_left) {
		_left->setSource(_libraryModel);
	}
	if (_right) {
		_right->setSource(_projectModel);
	}

	// Open: a collapsed tree shows three folder rows and none of the leaves the demo is about, and
	// every drop target below the first level would have to be found before it could be tried.
	expandAll(_left);
	expandAll(_right);

	// the status line counts what the models hold, so the one place that replaces them is the one
	// place that has to say so
	refreshStatus("content regenerated");
}

DndTreeView *DndTreeDemoLayout::viewByName(StringView name) const {
	if (name == "left" || name == "library") {
		return _left;
	}
	if (name == "right" || name == "project") {
		return _right;
	}
	return nullptr;
}

void DndTreeDemoLayout::performSelfCheck() {
	if (!_left || !_right) {
		return;
	}

	/* What the check reads: the demo's declared content, with both trees CLOSED.

	Section 1 asserts exactly that, and every later section is written against what it leaves. So
	the state is established here rather than assumed from whatever the session had reached - which
	is what makes a run from the inspector, an hour into a session with elements dragged all over,
	mean the same thing as the run at start-up. */
	resetContent();
	collapseAll(_left);
	collapseAll(_right);

	_checks = 0;
	_failures = 0;
	_selfCheckDone = false;
	runSelfCheck();

	// ...and the demo back to what an author expects to be looking at. runSelfCheck() leaves the
	// models where its transfers put them, on purpose: it is the caller who knows what to do next.
	resetContent();
}

void DndTreeDemoLayout::runSelfCheck() {
	if (_selfCheckDone || !_left || !_right) {
		return;
	}

	auto expect = [this](bool cond, StringView what) {
		++_checks;
		if (!cond) {
			++_failures;
			log::source().error("DndTreeExample", "self-check: ", what);
		}
	};

	auto root = [](DndTreeView *view) { return view->getSource()->getRoot(); };

	// 1. The generated content is what the rest of the check reads. Both trees start collapsed, so
	//    a row per top-level element and nothing else.
	expect(root(_left)->getChildCount() == 3, "'Library' does not hold three groups");
	expect(root(_right)->getChildCount() == 4, "'Project' does not hold four elements");
	expect(_left->getRowCount() == 3, "collapsed 'Library' derived the wrong number of rows");
	expect(_right->getRowCount() == 4, "collapsed 'Project' derived the wrong number of rows");

	// 2. Where a row says a drop would land. Every spot below is resolved BEFORE anything moves:
	//    a mutation reaches the row list through the model's subscription, on a later frame.
	auto widgets = _left->getRow(0) ? _left->getRow(0)->node.get() : nullptr;
	auto sceneB = _right->getRow(1) ? _right->getRow(1)->node.get() : nullptr;
	auto loose01 = _right->getRow(2) ? _right->getRow(2)->node.get() : nullptr;
	auto loose02 = _right->getRow(3) ? _right->getRow(3)->node.get() : nullptr;
	expect(widgets && widgets->isCategory(), "row 0 of 'Library' is not the 'Widgets' category");
	expect(sceneB && sceneB->isCategory() && sceneB->getChildCount() == 0,
			"row 1 of 'Project' is not the empty 'Scene B' category");
	expect(loose01 && !loose01->isCategory(), "row 2 of 'Project' is not a leaf");

	{
		using Kind = ui::TreeView::DropPosition::Kind;
		const auto band = ui::TreeView::CategoryDropBand;

		// A LEAF is only ever a position, and its two halves are the two insertion points around
		// it. 'Loose item 01' is child 2 of the root, so before it is index 2 and after it index 3.
		auto before = _right->getDropPositionForRow(2, 0.25f);
		expect(before.kind == Kind::Before && before.parent == root(_right) && before.index == 2,
				"the upper half of a leaf does not insert before it");

		auto after = _right->getDropPositionForRow(2, 0.75f);
		expect(after.kind == Kind::After && after.parent == root(_right) && after.index == 3,
				"the lower half of a leaf does not insert after it");

		// A CATEGORY is both, so its row is three zones: a band at each end to stand beside it, and
		// the wide middle to go inside. 'Widgets' is child 0 of the Library root.
		auto intoCat = _left->getDropPositionForRow(0, 0.5f);
		expect(intoCat.kind == Kind::Into && intoCat.parent == widgets
						&& intoCat.index == maxOf<size_t>(),
				"the middle of a category does not append into it");

		auto beforeCat = _left->getDropPositionForRow(0, band / 2.0f);
		expect(beforeCat.kind == Kind::Before && beforeCat.parent == root(_left)
						&& beforeCat.index == 0,
				"the top band of a category does not insert before it");

		auto afterCat = _left->getDropPositionForRow(0, 1.0f - band / 2.0f);
		expect(afterCat.kind == Kind::After && afterCat.parent == root(_left)
						&& afterCat.index == 1,
				"the bottom band of a category does not insert after it");

		// The bands are the edges and nothing more: just inside either one is still 'into'. This is
		// the assertion that would catch the ratio being read the wrong way round.
		expect(_left->getDropPositionForRow(0, band + 0.01f).kind == Kind::Into
						&& _left->getDropPositionForRow(0, 1.0f - band - 0.01f).kind == Kind::Into,
				"a category's middle band does not reach its declared edges");

		// the empty space below the last row answers for the root
		auto background = _right->getDropPositionForRow(maxOf<size_t>(), 0.5f);
		expect(background.kind == Kind::Into && background.parent == root(_right)
						&& background.row == maxOf<size_t>() && background.index == maxOf<size_t>(),
				"a drop on the background does not append to the root");
	}

	// 3. A subtree may not be dropped inside itself: the predicate refuses it, so the cursor says
	//    NoDrop rather than the drop silently doing nothing.
	auto widgetsPayload = _left->makePayload(0);
	expect(widgetsPayload != nullptr, "row 0 of 'Library' produced no payload");
	if (widgetsPayload) {
		expect(!_left->canAccept(widgetsPayload, _left->getDropPositionForRow(0, 0.5f)),
				"a category was accepted as a drop target for itself");
	}

	// 4. Across two models a Move is a rebuild plus a delete: the clone lands in the target, the
	//    original goes away, and the two are different elements.
	if (widgetsPayload && sceneB) {
		const auto clonesBefore = _right->getCloneCount();
		const auto movedId = widgetsPayload->node->getId();
		// read from the group rather than written as a literal: how many leaves it holds is the
		// demo's content, and the claim here is that ALL of them travelled, whatever the number
		const auto movedLeaves = widgetsPayload->node->getChildCount();

		auto spot = _right->getDropPositionForRow(1, 0.5f); // into the empty 'Scene B'
		expect(_right->applyTransfer(widgetsPayload, spot, DragActions::Move),
				"a cross-model move was refused");
		expect(!widgetsPayload->consumed,
				"a cross-model move claimed to have relocated the original");

		// what the source owes the payload once the target is done with it
		DndTreeView::finishTransfer(widgetsPayload, DragActions::Move);

		expect(root(_left)->getChildCount() == 2, "the moved group is still in 'Library'");
		expect(_left->getSource()->getNode(movedId) == nullptr,
				"the moved element still resolves in the source model");
		// the group itself lands in 'Scene B', and its leaves land under the group - a transfer
		// carries a SUBTREE, it does not spill its contents into the target
		expect(sceneB->getChildCount() == 1, "'Scene B' did not receive the moved group");
		expect(sceneB->getChildCount() == 1
						&& sceneB->getChildren().front()->getChildCount() == movedLeaves,
				"the moved group arrived without all of its leaves");
		expect(_right->getCloneCount() - clonesBefore == movedLeaves + 1,
				"the moved subtree was not rebuilt element by element");
	}

	// 5. Inside ONE model a Move keeps the element: same ItemId, new parent, and the target - not
	//    the source - is what relocated it.
	if (loose01 && sceneB) {
		auto payload = _right->makePayload(2);
		expect(payload != nullptr, "row 2 of 'Project' produced no payload");
		if (payload) {
			const auto id = payload->node->getId();
			const auto clonesBefore = _right->getCloneCount();

			// built rather than resolved from a row: the transfer above has already moved a group
			// into 'Scene B', and the row list catches up with the model on a later frame
			auto into = ui::TreeView::DropPosition{ui::TreeView::DropPosition::Kind::Into,
				maxOf<size_t>(), sceneB, maxOf<size_t>()};
			expect(_right->applyTransfer(payload, into, DragActions::Move),
					"a move inside one model was refused");
			expect(payload->consumed, "a move inside one model did not claim the relocation");
			expect(_right->getCloneCount() == clonesBefore, "a move inside one model cloned");

			// the source's half of the drag must now do nothing at all
			DndTreeView::finishTransfer(payload, DragActions::Move);
			expect(_right->getSource()->getNode(id) == payload->node.get(),
					"a move inside one model lost the element's identity");
			expect(payload->node->getParent() == sceneB,
					"the moved element is in the wrong parent");
		}
	}

	// 6. A Copy leaves the original exactly where it was, in either direction.
	if (loose02) {
		auto payload = _right->makePayload(3);
		expect(payload != nullptr, "row 3 of 'Project' produced no payload");
		if (payload) {
			const auto before = root(_left)->getChildCount();
			expect(_left->applyTransfer(payload,
						   _left->getDropPositionForRow(maxOf<size_t>(), 0.5f), DragActions::Copy),
					"a cross-model copy was refused");
			DndTreeView::finishTransfer(payload, DragActions::Copy);

			expect(root(_left)->getChildCount() == before + 1, "the copy did not reach 'Library'");
			expect(payload->node->getParent() == root(_right),
					"the copy removed the element it was made from");
		}
	}

	/* 7. The context menu: what a right click would OFFER, and what choosing it does.

	Driven through buildContextMenu, which is the very call the coordinator makes once it has
	resolved the row from the point - so what is skipped here is the pointer and the popup window,
	not the menu. */
	{
		auto leafMenu = _right->buildContextMenu(2); // 'Loose item 01', a leaf
		expect(leafMenu != nullptr, "a leaf offered no menu at all");
		if (leafMenu) {
			// New, Rename, Delete - a leaf is not somewhere to expand or collapse, so those are
			// the only three
			expect(leafMenu->count() == 3, "a leaf's menu is not New, Rename and Delete");
			expect(leafMenu->getItem("rename") != nullptr, "a leaf cannot be renamed");
			expect(leafMenu->getItem("delete") != nullptr, "a leaf cannot be deleted");
		}

		// A category offers the toggle as well, and the toggle SAYS which way it goes: the row is
		// collapsed here, so the item has to read Expand
		auto catMenu = _right->buildContextMenu(0); // 'Scene A', collapsed
		expect(catMenu != nullptr, "a category offered no menu");
		if (catMenu) {
			auto toggle = dynamic_cast<ui::MenuSourceButton *>(catMenu->getItem("toggle"));
			expect(toggle != nullptr, "a category's menu has no expand/collapse item");
			expect(toggle && toggle->getTitle() == "Expand",
					"a collapsed category does not offer to expand");
			expect(catMenu->getItem("delete") != nullptr, "a category cannot be deleted");

			// Choosing it runs the item's own callback - the same one the row would have run
			if (toggle && toggle->getCallback()) {
				toggle->getCallback()(toggle);
			}
			expect(_right->isRowExpanded(0), "choosing Expand did not open the category");

			// ...and the next menu says the opposite, because it is built from the state, not
			// remembered from the last time. The source is held in a LOCAL: getItem hands back a
			// raw pointer into it, and a temporary would be gone by the time it is read
			auto reopened = _right->buildContextMenu(0);
			auto again = reopened
					? dynamic_cast<ui::MenuSourceButton *>(reopened->getItem("toggle"))
					: nullptr;
			expect(again && again->getTitle() == "Collapse",
					"an expanded category still offers to expand");
			if (again && again->getCallback()) {
				again->getCallback()(again);
			}
			expect(!_right->isRowExpanded(0), "choosing Collapse did not close the category");
		}

		// The empty space below the last row belongs to the root, which is not an element: what it
		// offers is about the whole tree - and it cannot be renamed, because there is nothing there
		// to give a name to
		auto rootMenu = _right->buildContextMenu(maxOf<size_t>());
		expect(rootMenu && rootMenu->getItem("expand-all") && rootMenu->getItem("collapse-all"),
				"the background does not offer the whole-tree commands");
		expect(rootMenu && rootMenu->getItem("rename") == nullptr,
				"the background offers to rename something that is not an element");

		/* Renaming writes through to the MODEL, and the row follows through the model's own
		notification - the same shape as Delete below, and the same reason: nothing here refreshes
		anything.

		Driven through renameRow rather than through the editor: this runs before the layout is in
		a scene, so there is no overlay to open one on. What the editor's commit does with the text
		is this call, so what is skipped is the typing. */
		if (auto row = _right->getRow(1); row && row->node) {
			const auto id = row->node->getId();
			const auto before = row->node->getData().getString("name");

			expect(!_right->renameRow(1, "  "), "a name of nothing but blanks was accepted");
			expect(_right->getSource()->getNode(id)->getData().getString("name") == before,
					"a refused rename changed the element anyway");

			expect(_right->renameRow(1, "  Renamed row  "), "a rename was refused");
			expect(_right->getSource()->getNode(id)->getData().getString("name") == "Renamed row",
					"the new name did not reach the model, or kept its blanks");
			expect(_right->getRow(1) && _right->getRow(1)->node
							&& _right->getRow(1)->node->getData().getString("name")
									== "Renamed row",
					"the row still stands for the old name");
		}

		/* Deleting takes the element out of the MODEL, and the row list follows through the
		model's own notification rather than through anything the menu does.

		Everything here is read at the moment of the deletion rather than assumed: the sections
		above have moved elements around, so what row 2 stands for now - and whose child it is -
		is not what it was when this check was written. */
		if (auto victim = _right->getRow(2); victim && victim->node) {
			auto parent = victim->node->getParent();
			const auto id = victim->node->getId();
			const auto before = parent ? parent->getChildCount() : 0;

			auto menu = _right->buildContextMenu(2);
			auto item = menu ? menu->getItem("delete") : nullptr;
			auto deleteItem = dynamic_cast<ui::MenuSourceButton *>(item);
			expect(deleteItem != nullptr, "the row offered no Delete to choose");
			if (deleteItem && deleteItem->getCallback()) {
				deleteItem->getCallback()(deleteItem);
			}

			expect(parent && parent->getChildCount() == before - 1,
					"Delete did not remove the element from its parent");
			expect(_right->getSource()->getNode(id) == nullptr,
					"the deleted element still resolves in the model");
		}
	}

	/* 8. Creating an element from the menu: where it goes, and what it leaves behind.

	Only the half that has no editor in it. `New` puts a REAL element into the model and then waits
	for its row before opening one - and this runs before the layout is in a scene, exactly as the
	rename above does, so there is no overlay for an editor to live on. What is checkable here is
	everything up to that point: what the menu offers, where the element lands, and that the
	waiting editor gives up cleanly when the element is taken away again. The editor half is driven
	over the inspector instead, with `dndtree.create` followed by `dndtree.rename`. */
	{
		auto menu = _left->buildContextMenu(1); // a leaf
		auto entry = menu ? dynamic_cast<ui::MenuSourceButton *>(menu->getItem("new")) : nullptr;
		expect(entry != nullptr, "a row offers nothing to create");
		expect(entry && entry->hasSubmenu(), "New is not a submenu, so nothing opens on hover");

		auto items = entry ? entry->getSubmenu() : nullptr;
		expect(items && items->getItem("new-item") && items->getItem("new-category"),
				"New does not offer both kinds");

		// The empty space is not an element, so it offers no Rename and no Delete - but it is
		// still somewhere to put something, which is the whole point of it answering for the root
		auto rootMenu = _left->buildContextMenu(maxOf<size_t>());
		expect(rootMenu && rootMenu->getItem("new") != nullptr,
				"the background offers nowhere to create");

		if (auto cat = _left->getRow(0); cat && cat->node && cat->node->isCategory()) {
			auto point = _left->getInsertPoint(0);
			expect(point.parent == cat->node, "a new element on a category did not go into it");
			expect(point.index == 0, "a new element in a category did not go in first");

			// ...and open it, so that the leaf rows below exist for the rest of this section. The
			// sections above leave the trees in a state this one may not assume anything about.
			if (!cat->expanded) {
				_left->expandRow(0);
			}
		}

		/* The row is SEARCHED for rather than named: the sections above have moved elements between
		the trees and opened and closed categories, so which index holds a leaf is not something
		this check may assume - and a category would send the new element somewhere else entirely,
		which is the rule just above. */
		size_t leafRow = maxOf<size_t>();
		for (size_t i = 0; i < _left->getRowCount(); ++i) {
			auto it = _left->getRow(i);
			if (it && it->node && !it->node->isCategory() && it->node->getParent()) {
				leafRow = i;
				break;
			}
		}
		expect(leafRow != maxOf<size_t>(), "the left tree has no leaf to create beside");

		if (auto leaf = _left->getRow(leafRow); leaf && leaf->node) {
			auto point = _left->getInsertPoint(leafRow);
			expect(point.parent == leaf->node->getParent(),
					"a new element beside a leaf did not land among its siblings");
			expect(point.index == leaf->node->getChildIndex() + 1,
					"a new element beside a leaf did not land directly after it");
		}

		auto rootPoint = _left->getInsertPoint(maxOf<size_t>());
		expect(rootPoint.parent == _left->getSource()->getRoot(),
				"the background sends a new element somewhere other than the root");
		expect(rootPoint.index == maxOf<size_t>(),
				"a new element from the background is not appended");

		// Choosing the item is what a click does, and the only way to observe that the callback
		// hanging off it is the one that creates something
		if (auto leaf = _left->getRow(leafRow); leaf && leaf->node) {
			auto parent = leaf->node->getParent();
			const auto before = parent ? parent->getChildCount() : 0;
			const auto at = leaf->node->getChildIndex() + 1;

			auto rowMenu = _left->buildContextMenu(leafRow);
			auto rowEntry = rowMenu ? dynamic_cast<ui::MenuSourceButton *>(rowMenu->getItem("new"))
									: nullptr;
			auto rowItems = rowEntry ? rowEntry->getSubmenu() : nullptr;
			auto item = rowItems
					? dynamic_cast<ui::MenuSourceButton *>(rowItems->getItem("new-item"))
					: nullptr;
			if (item && item->getCallback()) {
				item->getCallback()(item);
			}

			expect(parent && parent->getChildCount() == before + 1,
					"choosing New did not add an element");
			expect(parent && at < parent->getChildCount()
							&& parent->getChildren()[at]->getData().getString("name") == "New item",
					"the new element is not where the menu said it would go, or has no name");
			expect(_left->isEditorPending(),
					"nothing is waiting to name the new element, so it never will be");

			/* Taking it away again must call the waiting editor off.

			The element is what the editor was waiting for; with it gone there is nothing left to
			name, and a wait that outlived its subject would open an editor over whatever row had
			moved into that slot. */
			if (parent && at < parent->getChildCount()) {
				_left->getSource()->removeNode(parent->getChildren()[at]);
			}
			_left->settlePendingEdit();
			expect(!_left->isEditorPending(),
					"the editor is still waiting for an element that is gone");
			expect(parent && parent->getChildCount() == before,
					"removing the new element left something behind");
		}
	}

	// The models are left where the transfers above put them: the CALLER restores them, so there
	// is one place that decides what the demo's starting state is.
	_selfCheckDone = true;
	log::source().warn("DndTreeExample", "self-check: ", _checks, " checks, ", _failures,
			" failures");
}

void DndTreeDemoLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	/* The context menus the trees declare open as popups of their own, and a popup is a SCENE of
	its own: the ui::StyleSystem carrying the sheet above lives on this window's content and does
	not reach it. Handing the same literal to the menu is what keeps it from coming up unstyled -
	which on a dark demo means black on black. */
	if (auto menus = ui::ContextMenuSystem::acquireForNode(this)) {
		menus->setMenuConfigCallback([](ui::MenuConfig &config) {
			config.stylesheetSource = s_css.str<Interface>();
			config.idPrefix = String("dnd");
		});
	}

	// The same boundary, crossed a second time: the inline editor a Rename opens lives on an
	// overlay of the scene's content node, which is outside the subtree this layout's resolver
	// walks. See DndTreeView::setStylesheetSource.
	for (auto view : {_left, _right}) {
		if (view) {
			view->setStylesheetSource(s_css);
		}
	}

	_inspectorScene = scene;
	addInspectorCommands(scene);
}

void DndTreeDemoLayout::handleExit() {
	// Before the base call: Node::handleExit() clears _scene at its very end, and a command whose
	// lambda captured a destroyed layout is a dangling call from the inspector socket.
	if (_inspectorScene) {
		if (auto i = inspector::get(_inspectorScene->getContent())) {
			for (auto &it : _inspectorCommands) { i->removeCommand(it); }
		}
		_inspectorScene = nullptr;
	}
	_inspectorCommands.clear();

	basic2d::SceneLayout2d::handleExit();
}

void DndTreeDemoLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	auto size = getContentSize();
	if (_background) {
		_background->setContentSize(size);
	}

	// The bar is a self-managed flex container, but nothing above it lays out its owner -
	// SceneLayout2d has no LayoutSystem of its own - so it gets an explicit box here: full width at
	// the top, and the dock takes whatever is left below.
	if (_controlBarRow) {
		_controlBarRow->setPosition(Vec2(0.0f, size.height));
		_controlBarRow->setContentSize(Size2(size.width, BarHeight));
	}

	if (_dockRoot) {
		_dockRoot->setContentSize(Size2(size.width, sprt::max(0.0f, size.height - BarHeight)));
	}
}

void DndTreeDemoLayout::addInspectorCommands(Scene *scene) {
	auto content = scene->getContent();
	if (!content) {
		return;
	}

	if (inspector::addCommand(content, "dndtree.rows",
				"List a tree's visible rows: {tree: left|right, offset, limit}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		// Read through a const reference: the non-const getters assert in debug on a missing key,
		// and a command invoked with no arguments at all is an ordinary thing to do.
		const Value &in = args;
		Value result;
		auto view = viewByName(in.getString("tree"));
		if (!view) {
			result.setString("unknown tree; use 'left' or 'right'", "error");
			done(sp::move(result));
			return;
		}

		const auto offset = size_t(sprt::max(in.getInteger("offset"), int64_t(0)));
		const auto requested = in.getInteger("limit");
		const auto limit = size_t(requested > 0 ? requested : 60);

		auto rows = view->getRows();

		Value list(Value::Type::ARRAY);
		for (size_t i = offset; i < rows.size() && i < offset + limit; ++i) {
			const auto &row = rows[i];

			Value entry;
			entry.setInteger(int64_t(i), "index");
			entry.setInteger(int64_t(row.depth), "depth");
			entry.setBool(row.isCategory(), "category");
			entry.setBool(row.expanded, "expanded");
			entry.setString(row.getData().getString("name"), "name");
			list.addValue(sp::move(entry));
		}

		result.setString(view->getTitle(), "tree");
		result.setInteger(int64_t(rows.size()), "count");
		result.setValue(sp::move(list), "rows");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.rows");
	}

	// The very path a pointer drives, minus the pointer: it builds the same payload the DragSource
	// would, hands it to the same applyTransfer, and runs the same completion. So a headless run
	// exercises the drop itself rather than a second implementation of it.
	if (inspector::addCommand(content, "dndtree.transfer",
				"Move a row: {from, row, to, target, side: before|into|after, action}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		Value result;

		auto from = viewByName(in.getString("from"));
		auto to = viewByName(in.getString("to"));
		if (!from || !to) {
			result.setString("unknown tree; use 'left' or 'right'", "error");
			done(sp::move(result));
			return;
		}

		const auto action =
				in.getString("action") == "copy" ? DragActions::Copy : DragActions::Move;

		auto payload = from->makePayload(size_t(sprt::max(in.getInteger("row"), int64_t(0))));
		if (!payload) {
			result.setString("that row cannot be dragged", "error");
			done(sp::move(result));
			return;
		}

		const auto target = args.isInteger("target")
				? size_t(sprt::max(in.getInteger("target"), int64_t(0)))
				: maxOf<size_t>();

		// Where in the target row the pointer would have been. A leaf reads this as a half and a
		// category as one of its three bands, which is why it is named rather than numbered.
		const auto side = in.getString("side");
		const float offset = (side == "before") ? 0.05f : (side == "into" ? 0.5f : 0.95f);

		const auto pos = to->getDropPositionForRow(target, offset);
		const bool ok = to->applyTransfer(payload, pos, action);
		DndTreeView::finishTransfer(payload, ok ? action : DragActions::None);

		result.setBool(ok, "ok");
		result.setString(payload->title, "element");
		result.setBool(payload->consumed, "relocated");
		// what the tree said the drop meant, so a headless run can assert the zone and not only
		// the outcome
		switch (pos.kind) {
		case ui::TreeView::DropPosition::Kind::Into: result.setString("into", "spot"); break;
		case ui::TreeView::DropPosition::Kind::Before: result.setString("before", "spot"); break;
		case ui::TreeView::DropPosition::Kind::After: result.setString("after", "spot"); break;
		default: result.setString("none", "spot"); break;
		}
		result.setInteger(int64_t(pos.index), "spotIndex");
		result.setInteger(int64_t(from->getSource()->getNodeCount() - 1), "sourceCount");
		result.setInteger(int64_t(to->getSource()->getNodeCount() - 1), "targetCount");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.transfer");
	}

	// Again the pointer's own path without the pointer: buildContextMenu is what the coordinator
	// calls once it has resolved the row, and `activate` runs the item's own callback - the very
	// one the menu row would have run.
	if (inspector::addCommand(content, "dndtree.context-menu",
				"What a right click offers: {tree, row, activate}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		Value result;

		auto view = viewByName(in.getString("tree"));
		if (!view) {
			result.setString("unknown tree", "error");
			done(sp::move(result));
			return;
		}

		// The very menu a pointer would get, from the very call the pointer makes - not a
		// description of it written twice
		const auto row = in.isInteger("row") ? size_t(in.getInteger("row")) : maxOf<size_t>();
		auto source = view->buildContextMenu(row);
		if (!source) {
			result.setBool(false, "ok");
			done(sp::move(result));
			return;
		}

		Value items(Value::Type::ARRAY);
		for (auto &item : source->getItems()) {
			Value entry;
			entry.setString(item->getName(), "name");
			if (auto button = dynamic_cast<ui::MenuSourceButton *>(item.get())) {
				entry.setString(button->getTitle(), "title");
			} else {
				entry.setString("separator", "type");
			}
			entry.setBool(item->isEnabled(), "enabled");
			items.addValue(sp::move(entry));
		}
		result.setValue(sp::move(items), "items");

		// Choosing one runs the item's OWN callback, which is what the menu row would have run
		auto activate = in.getString("activate");
		if (!activate.empty()) {
			bool ran = false;
			if (auto item = source->getItem(activate)) {
				if (auto button = dynamic_cast<ui::MenuSourceButton *>(item)) {
					if (auto &cb = button->getCallback()) {
						cb(button);
						ran = true;
					}
				}
			}
			result.setBool(ran, "activated");
		}

		result.setBool(true, "ok");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.context-menu");
	}

	/* The rename, from the outside: open the editor, read what it holds, type into it and commit.

	The same session the menu's Rename item opens - this command only reaches into it, so what is
	driven here is the real editor over the real row and not a second path that happens to write the
	same field. */
	if (inspector::addCommand(content, "dndtree.rename",
				"Drive an inline rename: {tree, row, text, commit, cancel}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		Value result;

		auto view = viewByName(in.getString("tree"));
		if (!view) {
			result.setString("unknown tree", "error");
			done(sp::move(result));
			return;
		}

		if (in.isInteger("row")) {
			result.setBool(view->beginRename(size_t(in.getInteger("row"))), "opened");
		}

		auto session = view->getRenameSession();
		if (session) {
			/* The editor is a widget like any other, and this is what a keystroke would have left
			in it.

			`text` and `commit` are separate arguments on purpose, and a caller has to step a frame
			between them: a focused ui::TextInput does not take a write, it REQUESTS one from the
			platform's text-input processor and receives it back as an echo. Committing in the same
			call would commit what the field still held. */
			if (auto editor = dynamic_cast<ui::TextInput *>(session->getEditor())) {
				if (in.isString("text")) {
					editor->setText(in.getString("text"));
				}
				result.setString(editor->getText(), "text");
			}

			if (in.getBool("cancel")) {
				session->cancel();
			} else if (in.getBool("commit")) {
				// False means the commit was REFUSED and the session is still open - the one
				// outcome a caller cannot see from the model
				result.setBool(session->commit(), "committed");
			}
		}

		result.setBool(view->isRenaming(), "editing");
		// Whether a cancel would take an element back out - the difference between naming a new
		// element and renaming an existing one, which is invisible in every other field here
		result.setBool(view->isCreating(), "creating");
		result.setBool(true, "ok");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.rename");
	}

	if (inspector::addCommand(content, "dndtree.create",
				"Create an element from the menu: {tree, row, kind=item|category}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const Value &in = args;
		Value result;

		auto view = viewByName(in.getString("tree"));
		if (!view) {
			result.setString("unknown tree", "error");
			done(sp::move(result));
			return;
		}

		/* Through the MENU, not through beginCreate.

		The menu is the whole of what this feature is: an item that hangs off a submenu nobody can
		reach is a feature that does not exist. Running the item's own callback is what the popup
		does when it is chosen, so this drives the same path a click does - and it is also the only
		way to observe that the two items are where they are said to be. */
		const auto row = in.isInteger("row") ? size_t(in.getInteger("row")) : maxOf<size_t>();
		auto menu = view->buildContextMenu(row);
		auto entry = menu ? dynamic_cast<ui::MenuSourceButton *>(menu->getItem("new")) : nullptr;
		auto items = entry ? entry->getSubmenu() : nullptr;
		if (!items) {
			result.setString("the menu offers nothing to create", "error");
			done(sp::move(result));
			return;
		}

		result.setBool(entry->hasSubmenu(), "submenu");

		const auto kind = in.getString("kind");
		auto item = dynamic_cast<ui::MenuSourceButton *>(
				items->getItem(kind == "category" ? "new-category" : "new-item"));
		if (!item || !item->getCallback()) {
			result.setString("no such item", "error");
			done(sp::move(result));
			return;
		}

		item->getCallback()(item);

		/* The editor is NOT open yet, and saying so is the point.

		A new row has no node until the view has rebuilt and laid it out, so the session opens a
		frame or two later - which is why `pending` is reported here and every caller has to step a
		frame before asking for `dndtree.rename`. */
		result.setBool(view->isEditorPending(), "pending");
		result.setBool(view->isRenaming(), "editing");
		result.setBool(true, "ok");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.create");
	}

	if (inspector::addCommand(content, "dndtree.reset", "Regenerate the content of both trees",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		resetContent();

		Value result;
		result.setBool(true, "ok");
		result.setInteger(int64_t(_left->getRowCount()), "leftRows");
		result.setInteger(int64_t(_right->getRowCount()), "rightRows");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.reset");
	}

	/* Reporting the self-check, and RUNNING it.

	XL_DNDTREE_SELFCHECK decides only whether the check runs by itself at start-up; it does not take
	the check out of the build and it does not take it away from here. `run` performs it now, from
	the state it needs and back to the demo's own, so a headless caller can ask for it whenever it
	likes instead of having to have set an environment variable before the app started. */
	if (inspector::addCommand(content, "dndtree.selfcheck",
				"Report the layout's structural self-check, or run it now: {run}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		if (static_cast<const Value &>(args).getBool("run")) {
			performSelfCheck();
		}

		Value result;
		// `ran` apart from `ok`: a check nobody performed is not one that failed, and zero checks
		// alone cannot tell those two apart
		result.setBool(_selfCheckDone, "ran");
		result.setBool(_selfCheckDone && _failures == 0, "ok");
		result.setInteger(int64_t(_checks), "checks");
		result.setInteger(int64_t(_failures), "failures");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.selfcheck");
	}
}

} // namespace stappler::xenolith::examples
