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

/* the whole view is the drop target while the pointer is over its empty space */
tree-view.drop-root { outline-color: var(--drop); outline-width: 2px; outline-style: solid; }

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
tree-row.drop-into  { background-color: var(--drop); }
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

	addGroup(model, "Widgets", "Widget", 5);
	addGroup(model, "Shapes", "Shape", 4);
	addGroup(model, "Icons", "Icon", 6);
	return model;
}

static Rc<data::Model> makeProjectModel() {
	data::Model::Value root;
	root.setString("Project", "name");

	auto model = Rc<data::Model>::create(sp::move(root));
	if (!model) {
		return nullptr;
	}

	addGroup(model, "Scene A", "Node", 3);
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

	// The self-check drives real transfers through the real models and leaves them where it
	// finished; resetContent() is what puts the demo's declared content back - the same call the
	// Reset button makes, so there is exactly one description of what "the starting state" is.
	runSelfCheck();
	resetContent();
	refreshStatus("drag a row from one tree into the other");

	return true;
}

void DndTreeDemoLayout::buildDock() {
	_libraryModel = makeLibraryModel();
	_projectModel = makeProjectModel();

	// The dock's owner must NOT carry a LayoutSystem of its own - the system writes every frame's
	// geometry directly, and two writers would fight (handleAdded asserts it). A plain Node is
	// exactly that. DockSystem distributes over _owner->getContentSize(), so this node has to be
	// sized in handleContentSizeDirty().
	if (!_dockRoot) {
		_dockRoot = addChild(Rc<Node>::create(), ZOrder(1));
		_dockRoot->setAnchorPoint(Anchor::BottomLeft);
	}

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
	_statusLabel->setString(toString("Library: ", left ? left - 1 : 0,
			"   Project: ", right ? right - 1 : 0, "   |   Ctrl = copy, Shift = move   |   ",
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
		// a category is a PLACE: the drop goes inside it, at the end
		auto spot = _left->resolveDropSpot(0);
		expect(spot.parent == widgets && spot.index == maxOf<size_t>(),
				"a drop on a category does not append into it");

		// a leaf is a POSITION: the drop goes right after it, among its siblings
		auto after = _right->resolveDropSpot(2);
		expect(after.parent == root(_right) && after.index == 3,
				"a drop on a leaf does not land after it");

		// the view's background answers for the root
		auto background = _right->resolveDropSpot(maxOf<size_t>());
		expect(background.parent == root(_right) && background.index == maxOf<size_t>(),
				"a drop on the background does not append to the root");
	}

	// 3. A subtree may not be dropped inside itself: the predicate refuses it, so the cursor says
	//    NoDrop rather than the drop silently doing nothing.
	auto widgetsPayload = _left->makePayload(0);
	expect(widgetsPayload != nullptr, "row 0 of 'Library' produced no payload");
	if (widgetsPayload) {
		expect(!_left->canAccept(widgetsPayload, _left->resolveDropSpot(0)),
				"a category was accepted as a drop target for itself");
	}

	// 4. Across two models a Move is a rebuild plus a delete: the clone lands in the target, the
	//    original goes away, and the two are different elements.
	if (widgetsPayload && sceneB) {
		const auto clonesBefore = _right->getCloneCount();
		const auto movedId = widgetsPayload->node->getId();

		auto spot = _right->resolveDropSpot(1); // into the empty 'Scene B'
		expect(_right->applyTransfer(widgetsPayload, spot, DragActions::Move),
				"a cross-model move was refused");
		expect(!widgetsPayload->consumed,
				"a cross-model move claimed to have relocated the original");

		// what the source owes the payload once the target is done with it
		DndTreeView::finishTransfer(widgetsPayload, DragActions::Move);

		expect(root(_left)->getChildCount() == 2, "the moved group is still in 'Library'");
		expect(_left->getSource()->getNode(movedId) == nullptr,
				"the moved element still resolves in the source model");
		// the group itself lands in 'Scene B', and its five leaves land under the group - a
		// transfer carries a SUBTREE, it does not spill its contents into the target
		expect(sceneB->getChildCount() == 1, "'Scene B' did not receive the moved group");
		expect(sceneB->getChildCount() == 1 && sceneB->getChildren().front()->getChildCount() == 5,
				"the moved group arrived without its five leaves");
		expect(_right->getCloneCount() - clonesBefore == 6,
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

			expect(_right->applyTransfer(payload, DndTreeView::DropSpot{sceneB, maxOf<size_t>()},
						   DragActions::Move),
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
			expect(_left->applyTransfer(payload, _left->resolveDropSpot(maxOf<size_t>()),
						   DragActions::Copy),
					"a cross-model copy was refused");
			DndTreeView::finishTransfer(payload, DragActions::Copy);

			expect(root(_left)->getChildCount() == before + 1, "the copy did not reach 'Library'");
			expect(payload->node->getParent() == root(_right),
					"the copy removed the element it was made from");
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
				"Drag a row between the trees: {from: left|right, row, to: left|right, " "targe" "t"
																								 " " "(row " "index; " "omit for " "the " "backgroun" "d), " "action: " "move|" "copy}",
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

		const bool ok = to->applyTransfer(payload, to->resolveDropSpot(target), action);
		DndTreeView::finishTransfer(payload, ok ? action : DragActions::None);

		result.setBool(ok, "ok");
		result.setString(payload->title, "element");
		result.setBool(payload->consumed, "relocated");
		result.setInteger(int64_t(from->getSource()->getNodeCount() - 1), "sourceCount");
		result.setInteger(int64_t(to->getSource()->getNodeCount() - 1), "targetCount");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.transfer");
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

	if (inspector::addCommand(content, "dndtree.selfcheck",
				"Report the result of the layout's own structural self-check",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		Value result;
		result.setBool(_selfCheckDone && _failures == 0, "ok");
		result.setInteger(int64_t(_checks), "checks");
		result.setInteger(int64_t(_failures), "failures");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("dndtree.selfcheck");
	}
}

} // namespace stappler::xenolith::examples
