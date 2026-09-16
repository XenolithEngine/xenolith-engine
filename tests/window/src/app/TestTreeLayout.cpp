/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "app/TestTreeLayout.h"
#include "app/TestRegistry.h"
#include "XL2dSceneContent.h"
#include "XLUiButton.h"
#include "XLUiStyleResolver.h"
#include "XLScene.h"
#include "XLSceneInspector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// A group row is taller than a test row, so the two levels are told apart at a glance - the same
// distinction the pushed menu drew by being a different screen.
static constexpr float MenuGroupHeight = 40.0f;
static constexpr float MenuTestHeight = 32.0f;

// ui::TreeView takes its row geometry from CSS, and this app carries no global sheet, so the menu
// brings its own. Everything below the `tree-*` names is the widget's documented contract; the
// colours are this app's.
static constexpr StringView s_menuCss(R"css(
:root {
	--tree-indent: 20px;
	--menu-bg: #2b2f36;
	--menu-hover: #383e47;
	--menu-selected: #1e88e5;
	--menu-text: #eceff1;
	--menu-text-dim: #b0bec5;
}

tree-view {
	background-color: var(--menu-bg);
	border-radius: 6px;
}

tree-row {
	/* a row is a Panel, and an unstyled Panel is a WHITE surface - the tree-view's own background
	   is what should show through a row nobody is pointing at */
	background-color: transparent;
	display: flex;
	flex-direction: row;
	align-items: center;
	height: var(--tree-row-h);
	padding-left: calc(10px + var(--tree-depth, 0) * var(--tree-indent));
	padding-right: 10px;
	column-gap: 8px;
}

tree-row:hover { background-color: var(--menu-hover); }
tree-row.selected { background-color: var(--menu-selected); }

.tree-toggle {
	flex: 0 0 20px;
	height: 20px;
	border-radius: 10px;
	background-color: var(--menu-hover);
	display: flex;
	justify-content: center;
	align-items: center;
}

.tree-toggle:hover { background-color: #4a515c; }
.tree-toggle > icon { width: 18px; height: 18px; color: var(--menu-text); }

.tree-icon {
	flex: 0 0 18px;
	width: 18px;
	height: 18px;
	color: var(--menu-text-dim);
}

.tree-label {
	flex-grow: 1;
	color: var(--menu-text);
	font-size: 15px;
	white-space: nowrap;
}

/* A group reads as a heading over the entries under it. */
tree-row.expanded > .tree-label, tree-row.collapsed > .tree-label {
	font-weight: bold;
}

.xl-ui-tree-row:hover {
	background-color: #3949ab;
}

.menu-button {
	background-color: #3949ab;
	border-radius: 6px;
	display: flex;
	justify-content: center;
	align-items: center;
}

.menu-button > label { color: #ffffff; font-size: 15px; }
)css");

// Add one group's subgroups and tests under `parent`. There is no separate index space to keep and
// no container to share between two callbacks: an entry the menu does not show is simply a node
// that is never created.
static void fillGroup(data::Model *model, data::Model::Node *parent, const TestGroup &group) {
	// Subgroups first, then the group's own tests — the order this menu wants, chosen here rather
	// than imposed by the model.
	for (auto &it : group.groups) {
		data::Model::Value own;
		own.setString(toString(it.title, "  (", getTestCount(it), ")"), "title");
		own.setString(it.name, "name");
		own.setString(it.description, "description");

		auto node = model->emplaceCategory(parent, maxOf<size_t>(), sp::move(own));
		fillGroup(model, node, it);
	}

	// A record without an environment variable is not a test but the front page of the app itself,
	// and it has no business in a menu of tests.
	for (auto &it : group.tests) {
		if (it.env.empty()) {
			continue;
		}

		data::Model::Value value;
		value.setString(it.title, "title");
		value.setString(it.name, "name");
		value.setString(it.env, "env");
		model->emplaceItem(parent, maxOf<size_t>(), sp::move(value));
	}
}

static Rc<data::Model> makeGroupModel(const TestGroup &group) {
	data::Model::Value own;
	own.setString(toString(group.title, "  (", getTestCount(group), ")"), "title");
	own.setString(group.name, "name");
	own.setString(group.description, "description");

	auto model = Rc<data::Model>::create(sp::move(own));
	if (!model) {
		return nullptr;
	}

	fillGroup(model, model->getRoot(), group);
	return model;
}

} // namespace

bool TestTreeLayout::init() {
	using namespace ui;

	if (!TestLayout::init()) {
		return false;
	}

	setCaption("Tests", "Every registered test, as the tree the registry declares them in");
	setLayoutName("menu:tree");

	setStyleSheet(s_menuCss);

	// A stylesheet alone changes nothing: one recursive resolver on the layout is what applies it,
	// to the tree rows and to their label/icon children alike.
	addSystem(Rc<StyleResolver>::create(true));

	_back = addChild(Rc<Button>::create([this] { this->pop(); }), ZOrder(1));
	_back->setString("Go back");
	_back->addStyleClass("menu-button");

	// The root's own record is the app's front page, not a group anyone opens, so the tree starts
	// with the root's CHILDREN - the directories under src/.
	_tree = addChild(Rc<TreeView>::create(makeGroupModel(getTestRegistry())), ZOrder(1));
	_tree->setName("test-tree");
	_tree->setLabelKey("title");
	_tree->setRowHeight(MenuTestHeight);

	// Resolved per row, during the geometry pass - a group is taller than the tests under it.
	_tree->setRowHeightCallback([](const TreeView::Row &row) {
		return row.isCategory() ? MenuGroupHeight : MenuTestHeight;
	});

	_tree->setRowCallback([](TreeView::RowBuilder &builder) {
		if (builder.isExpandable()) {
			builder.setIcon(builder.isExpanded() ? basic2d::IconName::File_folder_open_solid
												 : basic2d::IconName::File_folder_solid);
		} else {
			// A circle, not a bare arrow: an arrow in the icon column reads as another disclosure
			// chevron next to the real ones one indent step to its left.
			builder.setIcon(basic2d::IconName::Av_play_circle_filled_outline);
		}
	});

	// A single tap is the whole interaction: nothing here needs a selection that survives it.
	_tree->setSelectCallback(
			[this](size_t index, const TreeView::Row &row) { handleRowSelected(index, row); });

	return true;
}

void TestTreeLayout::handleRowSelected(size_t index, const ui::TreeView::Row &row) {
	if (row.isCategory()) {
		_tree->toggleRow(index);
		return;
	}

	// The row carries the test's id rather than a pointer into the registry: the id is what the
	// `layout` inspector command takes too, so both ways in resolve the same way.
	if (auto test = findTest(row.getData().getString("name"))) {
		getSceneContent()->pushLayout(makeTestLayout(*test));
	}
}

void TestTreeLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);

	_inspectorScene = scene;
	addInspectorCommands(scene);
}

void TestTreeLayout::handleExit() {
	// Before the base call: Node::handleExit() clears _scene at its very end, and a command whose
	// lambda captured a destroyed layout is a dangling call from the inspector socket.
	if (_inspectorScene) {
		if (auto i = inspector::get(_inspectorScene->getContent())) {
			for (auto &it : _inspectorCommands) { i->removeCommand(it); }
		}
		_inspectorScene = nullptr;
	}
	_inspectorCommands.clear();

	TestLayout::handleExit();
}

void TestTreeLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	auto cs = getContentSize();
	auto work = getWorkSize();

	// Same width limit as the menu it replaces, centred, filling what the caption leaves.
	const auto width = sprt::min(cs.width, 480.0f);
	const auto left = (cs.width - width) / 2.0f;

	_back->setAnchorPoint(Anchor::TopLeft);
	_back->setPosition(Vec2(left, getWorkTop() - 8.0f));
	_back->setContentSize(Size2(width, MenuTestHeight));

	_tree->setAnchorPoint(Anchor::TopLeft);
	_tree->setPosition(Vec2(left, getWorkTop() - MenuTestHeight - 24.0f));
	_tree->setContentSize(Size2(width, work.height - MenuTestHeight - 24.0f));
}

void TestTreeLayout::addInspectorCommands(Scene *scene) {
	auto content = scene->getContent();
	if (!content) {
		return;
	}

	if (inspector::addCommand(content, "menu.list",
				"List the visible menu rows: {offset, limit} (defaults 0, 40)",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const auto offset = size_t(sprt::max(args.getInteger("offset"), int64_t(0)));
		const auto requested = args.getInteger("limit");
		const auto limit = size_t(requested > 0 ? requested : 40);

		auto rows = _tree->getRows();

		Value list(Value::Type::ARRAY);
		for (size_t i = offset; i < rows.size() && i < offset + limit; ++i) {
			const auto &row = rows[i];

			Value entry;
			entry.setInteger(int64_t(i), "index");
			entry.setInteger(int64_t(row.depth), "depth");
			entry.setBool(row.isCategory(), "group");
			entry.setBool(row.expanded, "expanded");
			entry.setDouble(double(row.height), "height");
			entry.setString(row.getData().getString("title"), "title");
			entry.setString(row.getData().getString("name"), "name");
			list.addValue(sp::move(entry));
		}

		Value result;
		result.setInteger(int64_t(rows.size()), "count");
		result.setValue(sp::move(list), "rows");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("menu.list");
	}

	if (inspector::addCommand(content, "menu.toggle", "Open or close the group at {index}",
				[this](Value &&args, Function<void(Value &&)> &&done) {
		const auto index = size_t(sprt::max(args.getInteger("index"), int64_t(0)));

		// toggleRow() re-derives the model synchronously - only the row NODES wait for the next
		// frame - so everything reported below is already the post-toggle state.
		Value result;
		result.setBool(_tree->toggleRow(index), "ok");

		if (auto row = _tree->getRow(index)) {
			result.setBool(row->expanded, "expanded");
			result.setString(row->getData().getString("title"), "title");
		}
		result.setInteger(int64_t(_tree->getRowCount()), "count");
		done(sp::move(result));
	})) {
		_inspectorCommands.emplace_back("menu.toggle");
	}
}

} // namespace stappler::xenolith::app
