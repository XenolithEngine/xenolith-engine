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

#include "fileexplorer/FileExplorerLayout.h"
#include "fileexplorer/FileExplorerLocale.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"
#include "XLUiButton.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XLScene.h"
#include "XL2dSceneContent.h" // the definition Scene::getContent() needs to become a Node here
#include "XLSceneInspector.h"
#include "XLAction.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"

#include <stdlib.h> // getenv

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

/* The sheet. Three contracts meet here and only one of them is this example's own:

  * `tree-row` / `.tree-label` / `--tree-depth` / `--tree-row-h` belong to ui::TreeView, and
    `table-view` / `table-row` / `table-cell` / `--table-row-h` to ui::TableView. The column widths
    of a table come from `grid-template-columns` and from nowhere else.
  * `file-tile`, `file-tile-row` and `icon-grid-view` are the grid this example adds. Each is a
    Panel under a type of its own, which is why the code calls Panel::registerStyleAppliers for
    them - without it a `background-color` here would fall through to the node tint.
  * `--fe-font-size` is declared by the layout with ui::setStyleVariable and read below. A
    virtualized row is built and destroyed as the user scrolls, so the text size has to reach it
    through the cascade rather than through a call on every label.

An unstyled Panel is an opaque WHITE surface, so every row, cell and tile says `transparent`. */
static constexpr auto s_css = StringView(R"css(
:root {
	--tree-indent: 18px;
	--fe-font-size: 13px;
	--surface: #22262c;
	--surface-alt: #1b1e23;
	--row-hover: #2f3742;
	--row-selected: #1565c0;
	--text: #e8eaed;
	--text-dim: #9aa4b2;
}

/* --- the dock chrome ------------------------------------------------------------------------ */
/* `dock-frame-body` is deliberately not styled: the frame builds the flex run inside it itself. */
dock-frame       { background-color: #1f2227; }
dock-tab-bar     { background-color: #1a1d22; }
dock-tab         { display: flex; padding: 5px 14px; background-color: #2b3038; }
dock-tab.active  { background-color: #3a414d; }
dock-tab > label { color: var(--text); font-size: 13px; }
dock-splitter    { background-color: #14161a; }

/* --- the places tree ------------------------------------------------------------------------ */
/* A tree cannot measure itself, so `flex: 1` is what makes it fill the frame's body. */
tree-view {
	background-color: var(--surface);
	border-radius: 4px;
	margin: 6px;
	flex-grow: 1;
	flex-shrink: 1;
	flex-basis: 0px;
}

tree-row {
	background-color: transparent;
	display: flex;
	flex-direction: row;
	align-items: center;
	height: var(--tree-row-h);
	/* Logical, so the indent that carries the hierarchy follows the writing direction. */
	padding-inline-start: calc(6px + var(--tree-depth, 0) * var(--tree-indent));
	padding-right: 8px;
	column-gap: 6px;
	border-radius: 3px;
}

tree-row:hover    { background-color: var(--row-hover); }
tree-row.selected { background-color: var(--row-selected); }

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

.tree-icon { flex: 0 0 18px; width: 18px; height: 18px; color: #d7a640; }

/* `text-align: start` because this engine's initial value is Left, and `unicode-bidi: normal` to
   opt out of the blanket `plaintext` at the end of the sheet: a name belongs against the row's
   own edge, whatever script it is in. */
.tree-label {
	flex-grow: 1;
	color: var(--text);
	font-size: var(--fe-font-size);
	white-space: nowrap;
	text-align: start;
	unicode-bidi: normal;
}

/* --- the browser ---------------------------------------------------------------------------- */
file-browser {
	background-color: var(--surface);
	border-radius: 4px;
	margin: 6px;
	flex-grow: 1;
	flex-shrink: 1;
	flex-basis: 0px;
}

.fe-path-bar {
	display: flex;
	flex-direction: row;
	align-items: center;
	column-gap: 4px;
	padding-left: 8px;
	padding-right: 8px;
	background-color: var(--surface-alt);
}

.fe-breadcrumbs {
	flex-grow: 1;
	display: flex;
	flex-direction: row;
	align-items: center;
	column-gap: 2px;
}

.fe-crumb {
	flex-shrink: 0;
	height: 24px;
	padding-left: 8px;
	padding-right: 8px;
	background-color: transparent;
	border-radius: 3px;
	display: flex;
	align-items: center;
}
.fe-crumb:hover   { background-color: var(--row-hover); }
.fe-crumb-ellipsis { flex-shrink: 0; color: var(--text-dim); font-size: var(--fe-font-size); }
.fe-crumb > label { color: var(--text); font-size: var(--fe-font-size); white-space: nowrap; }

.fe-icon-button {
	flex: 0 0 28px;
	height: 28px;
	background-color: #2f3742;
	border-radius: 4px;
	display: flex;
	justify-content: center;
	align-items: center;
}
.fe-icon-button:hover    { background-color: #3d4854; }
.fe-icon-button:disabled { background-color: #22262c; }
.fe-icon-button > icon   { width: 18px; height: 18px; color: var(--text); }

/* --- the table ------------------------------------------------------------------------------ */
/* The widths are here and only here: ui::TableView reads grid-template-columns and falls back to
   Column::track only when this list is shorter than the column set. Horizontal rules are declared
   with border-bottom ALONE - a virtualized row collapses only its own borders, so a line declared
   on both sides of a row boundary is drawn twice. */
table-view {
	background-color: transparent;
	display: table;
	grid-template-columns: 3fr 1fr 1.4fr;
}

table-header      { background-color: var(--surface-alt); }
table-row         { background-color: transparent; height: var(--table-row-h); }
table-row.odd     { background-color: rgba(255, 255, 255, 0.03); }
table-row:hover   { background-color: var(--row-hover); }
table-row.selected { background-color: var(--row-selected); }

table-cell {
	display: flex;
	flex-direction: row;
	align-items: center;
	column-gap: 6px;
	padding-left: 8px;
	padding-right: 8px;
	border-bottom: 1px solid #2c323a;
}

.header-cell > label { color: var(--text-dim); font-size: var(--fe-font-size); font-weight: bold; }

.table-label {
	flex-grow: 1;
	color: var(--text);
	font-size: var(--fe-font-size);
	white-space: nowrap;
	text-align: start;
	unicode-bidi: normal;
}

.table-icon { flex: 0 0 18px; width: 18px; height: 18px; color: var(--text-dim); }

.col-size > .table-label { color: var(--text-dim); }
.col-time > .table-label { color: var(--text-dim); }

/* --- the grid of tiles ---------------------------------------------------------------------- */
icon-grid-view { background-color: transparent; }
file-tile-row  { background-color: transparent; }

file-tile          { background-color: transparent; border-radius: 5px; }
file-tile:hover    { background-color: var(--row-hover); }
file-tile.selected { background-color: var(--row-selected); }

.fe-tile-icon     { color: #c3cad3; }
.fe-tile-icon.dir { color: #d7a640; }

.fe-tile-label { color: var(--text); font-size: var(--fe-font-size); text-align: center; }

/* --- the toolbar ---------------------------------------------------------------------------- */
.fe-bar {
	display: flex;
	flex-direction: row;
	align-items: center;
	column-gap: 12px;
	padding-left: 14px;
	padding-right: 14px;
	background-color: #14161a;
}

.fe-button {
	flex: 0 0 auto;
	padding-left: 12px;
	padding-right: 12px;
	height: 30px;
	background-color: #333b47;
	border-radius: 5px;
	display: flex;
	justify-content: center;
	align-items: center;
}
.fe-button:hover     { background-color: #414b59; }
.fe-button.selected  { background-color: #3949ab; }
.fe-button > label   { color: #ffffff; font-size: 13px; white-space: nowrap; }

.fe-label { flex-shrink: 0; color: var(--text-dim); font-size: 12px; white-space: nowrap; }

slider       { flex: 0 0 110px; height: 20px; }
slider-fill  { background-color: #3949ab; border-radius: 2px; }
slider-thumb { background-color: #ffffff; border-radius: 7px; }

checkbox          { flex: 0 0 18px; height: 18px; background-color: #2f3742; border-radius: 3px; }
checkbox:checked  { background-color: #3949ab; }

/* --- the scroll bars ------------------------------------------------------------------------ */
scroll-indicator-track { background-color: #0d0f12; border-radius: 4px; }
scroll-indicator       { background-color: #6f7b8d; border-radius: 4px; }
scroll-indicator.active { background-color: #97a4b6; }

/* A file name carries no direction of its own, so its own script decides. The two labels that
   name a place rather than a file opt out above. */
* { unicode-bidi: plaintext; }
)css");

} // namespace

StringView getFileExplorerStylesheet() { return s_css; }

// --- construction -------------------------------------------------------------------------------

bool FileExplorerLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	_thumbnails = Rc<ThumbnailCache>::create();

	_background = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), ZOrder(0));
	_background->setAnchorPoint(Anchor::BottomLeft);

	buildToolbar();
	buildDock();

	applyFontSize();
	navigate(getStartPath());

	/* The engine renders on demand, and this example is meant to be watched and driven: a
	thumbnail landing from a worker, or an inspector command answering after a settle, would
	otherwise wait for something else to wake the loop. RenderContinuously draws nothing and
	damages nothing - it only keeps frames coming. */
	runAction(Rc<RenderContinuously>::create());

	return true;
}

void FileExplorerLayout::buildDock() {
	if (_dock) {
		return;
	}

	_placesModel = makePlacesModel();

	/* The dock's owner must carry no LayoutSystem of its own: the system writes every frame's
	geometry directly, and two writers would fight over it (handleAdded asserts). A plain Node is
	exactly that, and it is sized in handleContentSizeDirty below. */
	_dockRoot = addChild(Rc<Node>::create(), ZOrder(1));
	_dockRoot->setAnchorPoint(Anchor::BottomLeft);

	_dock = _dockRoot->addSystem(Rc<ui::DockSystem>::create());
	_dock->setSplitterThickness(6.0f);

	// The builder runs at most once, on first show, and the node it returns is kept for good - so
	// the tree's expansion and the browser's scroll position survive everything the dock does.
	ui::DockPanelDescriptor places;
	places.id = String("places");
	places.title = String("@Locale:FE:Places:Title");
	places.icon = basic2d::IconName::File_folder_solid;
	places.minSize = Size2(200.0f, 200.0f);
	// Neither closable nor movable: a pane the user can close is a pane this example can end up
	// without, and there is nothing here about arranging panes.
	places.flags = ui::DockPanelFlags::None;
	places.builder = [this]() -> Rc<Node> {
		auto tree = makePlacesTree();
		_places = tree;
		return tree;
	};
	_dock->registerPanel(sp::move(places));

	ui::DockPanelDescriptor browser;
	browser.id = String("browser");
	browser.title = String("@Locale:FE:Browser:Title");
	browser.icon = basic2d::IconName::File_grid_view_solid;
	browser.minSize = Size2(420.0f, 200.0f);
	browser.flags = ui::DockPanelFlags::None;
	browser.builder = [this]() -> Rc<Node> {
		auto view = Rc<FileBrowserView>::create(_thumbnails);
		_browser = view;
		view->setMode(_settings.mode);
		view->setIconSize(_settings.iconSize);
		view->setRowHeight(_settings.fontSize * 2.0f);
		view->setShowHidden(_settings.showHidden);
		return view;
	};
	_dock->registerPanel(sp::move(browser));

	using Spec = ui::DockLayoutSpec;
	_dock->setLayout(Spec::hsplit(0.24f,
			Spec::leaf({String("places")},
					{.name = String("places-frame"),
						.flags = ui::DockFrameFlags::Default | ui::DockFrameFlags::Permanent}),
			Spec::leaf({String("browser")},
					{.name = String("browser-frame"),
						.flags = ui::DockFrameFlags::Default | ui::DockFrameFlags::Permanent})));
}

Rc<ui::TreeView> FileExplorerLayout::makePlacesTree() {
	auto tree = Rc<ui::TreeView>::create(_placesModel);
	if (!tree) {
		return nullptr;
	}

	tree->setRootVisible(false);
	tree->setSelectionEnabled(true);
	tree->setRowHeight(_settings.fontSize * 2.0f);

	// After it is in a scene: the dock builds a panel's node before parking it, and a widget that
	// opts into the scene's selection has to have a scene to opt into.
	tree->setEnterCallback([tree = tree.get()](Scene *) { tree->setSelectionOwned(true); });

	// The model's own icon table: the open folder for an expanded row, the closed one otherwise.
	tree->setRowCallback([](ui::TreeView::RowBuilder &row) {
		row.setIcon(ui::FilesystemModel::getIcon(row.getNode(), row.isExpanded()));
	});

	/* A tap selects and navigates; an arrow key only selects. Navigating on every arrow press
	would walk the right pane through every directory between where the selection started and
	where the user was heading. */
	tree->setSelectCallback([this, tree = tree.get()](size_t, const ui::TreeView::Row &row) {
		if (tree->isSelectingFromKeyboard()) {
			return;
		}
		navigate(ui::FilesystemModel::getPath(row.node));
	});

	tree->setActivateCallback([this](size_t, const ui::TreeView::Row &row) {
		navigate(ui::FilesystemModel::getPath(row.node));
	});

	return tree;
}

void FileExplorerLayout::buildToolbar() {
	_toolbar = addChild(Rc<Node>::create(), ZOrder(2));
	_toolbar->setAnchorPoint(Anchor::TopLeft);
	_toolbar->addStyleClass("fe-bar");
	_toolbar->addSystem(Rc<ui::LayoutSystem>::create(ui::FlexLayoutInfo{
		.direction = ui::FlexDirection::Row,
		.alignItems = ui::FlexAlign::Center,
		.columnGap = 12.0f,
	}));

	/* Every item gets a ZOrder of its own, and they are the bar's reading order: the layout walks
	children in z-order and sortAllChildren is not stable, so siblings sharing one would come out
	in whatever order the sort left them. */

	// Two buttons rather than a ui::Select: the mode is a pair, and a pair reads better as two
	// places to press than as a list to open.
	_iconsButton = makeToolButton("@Locale:FE:Bar:Icons", 0,
			[this] { setMode(FileBrowserView::Mode::Icons); });
	_iconsButton->setIcon(basic2d::IconName::File_grid_view_solid);

	_tableButton = makeToolButton("@Locale:FE:Bar:Table", 1,
			[this] { setMode(FileBrowserView::Mode::Table); });
	_tableButton->setIcon(basic2d::IconName::Action_view_list_solid);

	auto iconLabel = _toolbar->addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	iconLabel->setString("@Locale:FE:Bar:TileSize");
	iconLabel->addStyleClass("fe-label");

	_iconSlider = _toolbar->addChild(Rc<ui::Slider>::create(), ZOrder(3));
	_iconSlider->setRange(MinIconSize, MaxIconSize, IconSizeStep);
	_iconSlider->setInteger(true);
	_iconSlider->setValue(_settings.iconSize, true);
	_iconSlider->setCallback([this](int64_t) { setIconSize(float(_iconSlider->getValue())); });

	auto fontLabel = _toolbar->addChild(Rc<basic2d::Label>::create(), ZOrder(4));
	fontLabel->setString("@Locale:FE:Bar:FontSize");
	fontLabel->addStyleClass("fe-label");

	_fontSlider = _toolbar->addChild(Rc<ui::Slider>::create(), ZOrder(5));
	_fontSlider->setRange(MinFontSize, MaxFontSize, 1.0f);
	_fontSlider->setInteger(true);
	_fontSlider->setValue(_settings.fontSize, true);
	_fontSlider->setCallback([this](int64_t) { setFontSize(float(_fontSlider->getValue())); });

	_hiddenCheck = _toolbar->addChild(Rc<ui::Checkbox>::create(), ZOrder(6));
	_hiddenCheck->setCallback([this](bool value) { setShowHidden(value); });

	auto hiddenLabel = _toolbar->addChild(Rc<basic2d::Label>::create(), ZOrder(7));
	hiddenLabel->setString("@Locale:FE:Bar:Hidden");
	hiddenLabel->addStyleClass("fe-label");

	makeToolButton("@Locale:FE:Bar:Refresh", 8, [this] {
		if (_browser) {
			_browser->refresh();
		}
		if (_placesModel) {
			_placesModel->refreshAll();
		}
	});

	// Its caption names the language it switches TO, in that language - the one string here that
	// must not be translated, so it is re-assigned by hand rather than re-expanded from a tag.
	_localeButton = makeToolButton(currentFileExplorerLocaleName(), 9, [this] {
		auto name = cycleFileExplorerLocale();
		if (_localeButton) {
			_localeButton->setString(name);
		}
	});
	_localeButton->getLabel()->setLocaleEnabled(false);

	setMode(_settings.mode);
}

ui::Button *FileExplorerLayout::makeToolButton(StringView label, int16_t order,
		Function<void()> &&action) {
	auto button = _toolbar->addChild(Rc<ui::Button>::create(sp::move(action)), ZOrder(order));
	button->setString(label);
	button->addStyleClass("fe-button");
	return button;
}

// --- settings -----------------------------------------------------------------------------------

void FileExplorerLayout::setMode(FileBrowserView::Mode mode) {
	_settings.mode = mode;

	// The CSS subset has no `:checked` for a button, so which of the two is current is said with
	// a style class.
	if (_iconsButton) {
		if (mode == FileBrowserView::Mode::Icons) {
			_iconsButton->addStyleClass("selected");
		} else {
			_iconsButton->removeStyleClass("selected");
		}
	}
	if (_tableButton) {
		if (mode == FileBrowserView::Mode::Table) {
			_tableButton->addStyleClass("selected");
		} else {
			_tableButton->removeStyleClass("selected");
		}
	}

	if (_browser) {
		_browser->setMode(mode);
	}
}

void FileExplorerLayout::setIconSize(float value) {
	_settings.iconSize = sprt::clamp(value, MinIconSize, MaxIconSize);
	if (_browser) {
		_browser->setIconSize(_settings.iconSize);
	}
}

void FileExplorerLayout::setFontSize(float value) {
	_settings.fontSize = sprt::clamp(value, MinFontSize, MaxFontSize);
	applyFontSize();
}

void FileExplorerLayout::applyFontSize() {
	/* One declaration, inherited by the whole subtree. A row of a virtualized view exists only
	while it is on screen, so the text size has to be part of the cascade rather than a call on
	every label that happens to exist right now. */
	ui::setStyleVariable(this, "--fe-font-size", toString(uint32_t(_settings.fontSize), "px"));

	// The row height is not a style: both views need it before a row node exists.
	const auto rowHeight = _settings.fontSize * 2.0f;
	if (_places) {
		_places->setRowHeight(rowHeight);
	}
	if (_browser) {
		_browser->setRowHeight(rowHeight);
	}
}

void FileExplorerLayout::setShowHidden(bool value) {
	_settings.showHidden = value;
	if (_browser) {
		_browser->setShowHidden(value);
	}
	if (_placesModel) {
		_placesModel->setShowHidden(value);
		_placesModel->refreshAll();
	}
}

String FileExplorerLayout::getStartPath() {
	auto home = filesystem::findPath<Interface>(FileCategory::UserHome);
	if (!home.empty() && filesystem::exists(FileInfo{home})) {
		return home;
	}
	return String("/");
}

void FileExplorerLayout::navigate(StringView path) {
	if (_browser) {
		_browser->navigate(path);
	}
}

void FileExplorerLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();

	auto size = getContentSize();
	if (_background) {
		_background->setContentSize(size);
	}

	// Nothing above lays out this layout's children - SceneLayout2d has no LayoutSystem - so the
	// toolbar and the dock get explicit boxes here.
	if (_toolbar) {
		_toolbar->setPosition(Vec2(0.0f, size.height));
		_toolbar->setContentSize(Size2(size.width, BarHeight));
	}

	if (_dockRoot) {
		_dockRoot->setContentSize(Size2(size.width, sprt::max(0.0f, size.height - BarHeight)));
	}
}

void FileExplorerLayout::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	// The ResourceCache lives on the Director, which a node only has once it is in a scene.
	if (_thumbnails) {
		_thumbnails->setDirector(getDirector());
	}

	_inspectorScene = scene;
	addInspectorCommands(scene);

	/* XL_FILEEXPLORER_SELFCHECK gates the AUTOMATIC run and nothing else: the check itself is
	always compiled in and always reachable through `fileexplorer.selfcheck`. */
	if (auto value = ::getenv("XL_FILEEXPLORER_SELFCHECK"); value && StringView(value) != "0") {
		runSelfCheck();
	}
}

void FileExplorerLayout::handleExit() {
	// Before the base call: Node::handleExit() clears _scene at its very end, and a command whose
	// lambda captured a destroyed layout is a dangling call from the inspector socket.
	removeInspectorCommands();

	basic2d::SceneLayout2d::handleExit();
}

// --- the inspector ------------------------------------------------------------------------------

void FileExplorerLayout::addInspectorCommands(Scene *scene) {
	auto content = scene->getContent();
	if (!content) {
		return;
	}

	/* Every command answers after a short settle rather than straight away. Each one of them
	changes something that is a RESULT - a listing that is walked on a worker, a column count that
	the next layout pass computes, a row height a rebuild picks up - so a reply sent now would be
	read against the previous frame. */
	auto add = [&](StringView name, StringView description, Function<Value(const Value &)> &&fn) {
		auto full = toString("fileexplorer.", name);
		if (!inspector::addCommand(content, full, description,
					[this, fn = sp::move(fn)](Value &&args,
							Function<void(Value &&)> &&done) mutable {
			// Const, so a command invoked with no arguments reads empty values instead of
			// asserting on a missing key.
			const Value &in = args;
			auto result = fn(in);
			runAction(Rc<Sequence>::create(0.2f,
					Function<void()>([done = sp::move(done), result = sp::move(result)]() mutable {
				done(sp::move(result));
			})));
		})) {
			return; // no inspector on this scene - the app was not built with one
		}
		_inspectorCommands.emplace_back(sp::move(full));
	};

	add("state", "What the explorer is showing and what every setting is worth",
			[this](const Value &) { return encodeState(); });

	add("places", "The rows of the places tree: {offset, limit}",
			[this](const Value &) { return encodePlaces(); });

	add("navigate", "Show a directory in the right pane: {path}", [this](const Value &in) {
		Value result;
		auto path = in.getString("path");
		if (!_browser || !_browser->navigate(path)) {
			result.setString("not a readable directory", "error");
			result.setString(path, "path");
			return result;
		}
		return encodeState();
	});

	add("up", "One level up", [this](const Value &) {
		Value result;
		if (!_browser || !_browser->navigateUp()) {
			result.setString("already at the root", "error");
			return result;
		}
		return encodeState();
	});

	add("entries", "What the right pane is listing: {offset, limit}", [this](const Value &in) {
		Value result;
		if (!_browser) {
			result.setString("no browser", "error");
			return result;
		}

		auto all = _browser->encodeEntries();
		const auto offset = size_t(sprt::max(in.getInteger("offset"), int64_t(0)));
		const auto requested = in.getInteger("limit");
		const auto limit = size_t(requested > 0 ? requested : 200);

		Value list(Value::Type::ARRAY);
		const auto &array = all.asArray();
		for (size_t i = offset; i < array.size() && i < offset + limit; ++i) {
			list.addValue(Value(array[i]));
		}

		result.setString(_browser->getPath(), "path");
		result.setInteger(int64_t(array.size()), "count");
		result.setValue(sp::move(list), "entries");
		return result;
	});

	add("mode", "Switch the right pane: {mode: icons|table}", [this](const Value &in) {
		auto mode = in.getString("mode");
		if (mode == "icons") {
			setMode(FileBrowserView::Mode::Icons);
		} else if (mode == "table") {
			setMode(FileBrowserView::Mode::Table);
		} else {
			Value result;
			result.setString("unknown mode; use 'icons' or 'table'", "error");
			return result;
		}
		return encodeState();
	});

	add("tilesize", "Set the picture size in the grid: {value}", [this](const Value &in) {
		setIconSize(float(in.getInteger("value")));
		if (_iconSlider) {
			_iconSlider->setValue(_settings.iconSize, true);
		}
		return encodeState();
	});

	add("fontsize", "Set the text size: {value}", [this](const Value &in) {
		setFontSize(float(in.getInteger("value")));
		if (_fontSlider) {
			_fontSlider->setValue(_settings.fontSize, true);
		}
		return encodeState();
	});

	add("hidden", "Show or hide dot-files: {value}", [this](const Value &in) {
		setShowHidden(in.getBool("value"));
		if (_hiddenCheck) {
			_hiddenCheck->setChecked(_settings.showHidden, true);
		}
		return encodeState();
	});

	add("select", "Select an entry in the right pane: {path}", [this](const Value &in) {
		Value result;
		if (!_browser || !_browser->selectPath(in.getString("path"))) {
			result.setString("no such entry in this listing", "error");
			return result;
		}
		return encodeState();
	});

	add("expand", "Open a row of the places tree: {path}", [this](const Value &in) {
		Value result;
		auto path = in.getString("path");
		if (!_places) {
			result.setString("no places tree", "error");
			return result;
		}
		auto rows = _places->getRows();
		for (size_t i = 0; i < rows.size(); ++i) {
			if (ui::FilesystemModel::getPath(rows[i].node) == path) {
				_places->expandRow(i);
				result.setBool(true, "expanded");
				result.setInteger(int64_t(i), "row");
				return result;
			}
		}
		result.setString("no such row; expand its parent first", "error");
		return result;
	});

	add("refresh", "Re-walk both panes", [this](const Value &) {
		if (_browser) {
			_browser->refresh();
		}
		if (_placesModel) {
			_placesModel->refreshAll();
		}
		return encodeState();
	});

	add("thumbnails", "Counters of the thumbnail cache", [this](const Value &) {
		Value result;
		if (!_thumbnails) {
			result.setString("no cache", "error");
			return result;
		}
		auto stats = _thumbnails->getStats();
		result.setInteger(int64_t(stats.queued), "queued");
		result.setInteger(int64_t(stats.inFlight), "inFlight");
		result.setInteger(int64_t(stats.cached), "cached");
		result.setInteger(int64_t(stats.failed), "failed");
		result.setInteger(int64_t(stats.folders), "folders");
		return result;
	});

	add("locale", "Move to the next language", [this](const Value &) {
		auto name = cycleFileExplorerLocale();
		if (_localeButton) {
			_localeButton->setString(name);
		}
		Value result;
		result.setString(name, "locale");
		return result;
	});

	add("selfcheck", "Run the built-in checks and answer their counts", [this](const Value &) {
		runSelfCheck();
		Value result;
		result.setBool(true, "started");
		return result;
	});
}

void FileExplorerLayout::removeInspectorCommands() {
	if (_inspectorScene) {
		if (auto i = inspector::get(_inspectorScene->getContent())) {
			for (auto &it : _inspectorCommands) { i->removeCommand(it); }
		}
		_inspectorScene = nullptr;
	}
	_inspectorCommands.clear();
}

Value FileExplorerLayout::encodeState() const {
	Value result;
	result.setString(_browser ? _browser->getPath() : StringView(), "path");
	result.setString(_settings.mode == FileBrowserView::Mode::Icons ? "icons" : "table", "mode");
	result.setInteger(int64_t(_settings.iconSize), "tileSize");
	result.setInteger(int64_t(_settings.fontSize), "fontSize");
	result.setBool(_settings.showHidden, "hidden");
	result.setInteger(int64_t(_browser ? _browser->getColumnCount() : 0), "columns");
	result.setInteger(int64_t(_browser ? _browser->getEntryCount() : 0), "entryCount");
	result.setInteger(int64_t(_browser ? _browser->getViewRowCount() : 0), "rows");
	result.setInteger(int64_t(_browser ? _browser->getRowHeight() : 0.0f), "rowHeight");
	result.setString(_browser ? _browser->getSelectedPath() : StringView(), "selection");
	result.setString(currentFileExplorerLocaleName(), "locale");
	result.setInteger(int64_t(_places ? _places->getRowCount() : 0), "placeRows");
	result.setInteger(int64_t(_checks), "checks");
	result.setInteger(int64_t(_failures), "failures");
	return result;
}

Value FileExplorerLayout::encodePlaces() const {
	Value result;
	Value list(Value::Type::ARRAY);

	if (_places) {
		auto rows = _places->getRows();
		for (size_t i = 0; i < rows.size(); ++i) {
			const auto &row = rows[i];
			Value entry;
			entry.setInteger(int64_t(i), "index");
			entry.setInteger(int64_t(row.depth), "depth");
			entry.setBool(row.expanded, "expanded");
			entry.setString(row.getData().getString("name"), "title");
			entry.setString(row.getData().getString("path"), "path");
			list.addValue(sp::move(entry));
		}
	}

	result.setInteger(int64_t(list.size()), "count");
	result.setValue(sp::move(list), "places");
	return result;
}

// --- the self-check -----------------------------------------------------------------------------

void FileExplorerLayout::expect(bool condition, StringView what) {
	++_checks;
	if (!condition) {
		++_failures;
		log::source().error("FileExplorerExample", "self-check: ", what);
	}
}

void FileExplorerLayout::runSelfCheck() {
	_checks = 0;
	_failures = 0;

	/* Everything this check reads is a RESULT: the listing is walked on a worker, the column count
	is computed once the grid has a width, and the row height is picked up by a rebuild. So it
	waits for a few frames rather than asking on the one it was started from. */
	runAction(Rc<Sequence>::create(0.6f, Function<void()>([this] { performChecks(); })));
}

void FileExplorerLayout::performChecks() {
	// 1. The places. The standard locations differ per machine and per platform, so what is
	//    asserted is the one root every target has.
	expect(_placesModel != nullptr, "no places model");
	expect(_places != nullptr, "the places tree was never built");

	bool hasFilesystemRoot = false;
	if (_placesModel) {
		for (auto &child : _placesModel->getRoot()->getChildren()) {
			if (ui::FilesystemModel::getPath(child) == "/") {
				hasFilesystemRoot = true;
			}
		}
		expect(_placesModel->getRoot()->getChildCount() > 0, "the places model has no roots");
	}
	expect(hasFilesystemRoot, "the places model does not carry the root of the filesystem");

	// 2. The right pane opened where it said it would.
	expect(_browser != nullptr, "the browser was never built");
	if (!_browser) {
		log::source().warn("FileExplorerExample", "self-check: ", _checks, " checks, ", _failures,
				" failures");
		return;
	}
	/* That the pane stands somewhere readable - not that it stands where it started. This check
	is reachable at any point in a session through `fileexplorer.selfcheck`, and by then the pane
	has usually been navigated somewhere on purpose. */
	expect(!_browser->getPath().empty(), "the browser is not showing any directory");
	expect(filesystem::exists(FileInfo{_browser->getPath()}),
			"the browser is showing a directory that is not there");

	// 3. Directories before files, which is what setDirectoriesFirst asked the model for. Read
	//    over whatever the listing has produced by now - an empty one passes vacuously, and the
	//    check script is what asserts a known set.
	bool orderHolds = true;
	bool seenFile = false;
	auto entries = _browser->encodeEntries();
	for (auto &entry : entries.asArray()) {
		if (entry.getBool("dir")) {
			if (seenFile) {
				orderHolds = false;
			}
		} else {
			seenFile = true;
		}
	}
	expect(orderHolds, "a directory is listed after a file");

	// 4. The mode switch reports what it did.
	const auto mode = _settings.mode;
	setMode(FileBrowserView::Mode::Table);
	expect(_browser->getMode() == FileBrowserView::Mode::Table, "the pane did not switch to table");
	setMode(FileBrowserView::Mode::Icons);
	expect(_browser->getMode() == FileBrowserView::Mode::Icons, "the pane did not switch to icons");
	setMode(mode);

	// 5. The text size reaches the table's row height, which is not a style: both views need it
	//    before a row node exists.
	const auto fontSize = _settings.fontSize;
	setFontSize(18.0f);
	expect(_browser->getRowHeight() == 36.0f, "the text size did not move the table row height");
	setFontSize(fontSize);

	// 6. The picture size moves the column count, and in the direction it should. This is the one
	//    assertion that needs the grid to have been laid out.
	const auto iconSize = _settings.iconSize;
	setIconSize(MinIconSize);
	const auto wide = _browser->getColumnCount();
	setIconSize(MaxIconSize);
	const auto narrow = _browser->getColumnCount();
	setIconSize(iconSize);

	expect(wide > 0, "the grid reported no columns at all");
	expect(wide > narrow, "a larger picture did not mean fewer columns");

	log::source().warn("FileExplorerExample", "self-check: ", _checks, " checks, ", _failures,
			" failures");
}

} // namespace stappler::xenolith::examples
