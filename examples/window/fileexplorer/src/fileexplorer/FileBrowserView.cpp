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

#include "fileexplorer/FileBrowserView.h"
#include "XLDynamicStateSystem.h"
#include "XLUiLayoutSystem.h"
#include "XLUiStyleSystem.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

static constexpr StringView ColumnName = StringView("name");
static constexpr StringView ColumnSize = StringView("size");
static constexpr StringView ColumnTime = StringView("mtime");

} // namespace

bool FileBrowserView::init(ThumbnailCache *cache) {
	if (!ui::Panel::init()) {
		return false;
	}

	_cache = cache;

	setType("file-browser");
	removeStyleClass("xl-ui-panel");
	ui::Panel::registerStyleAppliers("file-browser");
	setComponent<ui::SystemManagedLayout>();

	// Nothing this pane holds draws outside it: a virtualized row is laid out whole, however
	// little of it is in view.
	addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll))->enableScissor();

	buildHeader();

	_grid = addChild(Rc<IconGridView>::create(cache), ZOrder(1));
	_grid->setAnchorPoint(Anchor::BottomLeft);
	_grid->setIconSize(_iconSize);
	_grid->setActivateCallback(
			[this](size_t, ui::FilesystemModel::Node *node) { handleActivate(node); });

	_table = addChild(Rc<ui::TableView>::create(), ZOrder(2));
	_table->setAnchorPoint(Anchor::BottomLeft);
	_table->setSelectionEnabled(true);
	_table->setRowHeight(_rowHeight);
	_table->setColumns(Vector<ui::TableView::Column>{
		ui::TableView::Column{ColumnName.str<Interface>(), String("@Locale:FE:Col:Name"),
			String("col-name")},
		ui::TableView::Column{ColumnSize.str<Interface>(), String("@Locale:FE:Col:Size"),
			String("col-size")},
		ui::TableView::Column{ColumnTime.str<Interface>(), String("@Locale:FE:Col:Modified"),
			String("col-time")},
	});

	/* The name cell carries the row's icon, and the two others are formatted rather than shown
	raw: a Value holding 1'048'576 and a Value holding a microsecond count are both integers, and
	neither reads as what it means. */
	_table->setCellCallback([](ui::TableView::CellBuilder &cell) {
		auto row = cell.getRow();
		if (!row || !row->node) {
			return;
		}

		if (cell.getColumn().key == ColumnName) {
			cell.setIcon(ui::FilesystemModel::getIcon(row->node));
			cell.addStyleClass("fe-name-cell");
			return;
		}

		const auto &data = row->getData();
		if (cell.getColumn().key == ColumnSize) {
			cell.setLabel(data.getBool("dir") ? StringView() : formatSize(data.getInteger("size")));
		} else if (cell.getColumn().key == ColumnTime) {
			cell.setLabel(formatTime(data.getInteger("mtime")));
		}
	});

	_table->setActivateCallback([this](size_t, const ui::TableView::Row &row) {
		handleActivate(row.node);
	});

	setMode(_mode);

	return true;
}

void FileBrowserView::buildHeader() {
	_header = addChild(Rc<Node>::create(), ZOrder(3));
	_header->setAnchorPoint(Anchor::TopLeft);
	_header->addStyleClass("fe-path-bar");
	_header->addSystem(Rc<ui::LayoutSystem>::create(ui::FlexLayoutInfo{
		.direction = ui::FlexDirection::Row,
		.alignItems = ui::FlexAlign::Center,
		.columnGap = 4.0f,
	}));

	_upButton = _header->addChild(Rc<ui::Button>::create([this] { navigateUp(); }), ZOrder(0));
	_upButton->setIcon(basic2d::IconName::Navigation_arrow_upward_solid);
	_upButton->addStyleClass("fe-icon-button");

	// Its own flex row, so the crumbs can be replaced wholesale without disturbing the button.
	_breadcrumbs = _header->addChild(Rc<Node>::create(), ZOrder(1));
	_breadcrumbs->addStyleClass("fe-breadcrumbs");
	_breadcrumbs->addSystem(Rc<ui::LayoutSystem>::create(ui::FlexLayoutInfo{
		.direction = ui::FlexDirection::Row,
		.alignItems = ui::FlexAlign::Center,
		.columnGap = 2.0f,
	}));
}

// --- navigation -------------------------------------------------------------------------------

bool FileBrowserView::navigate(StringView path) {
	if (path.empty()) {
		return false;
	}

	/* An owned copy FIRST. `path` is often a view into something this call is about to destroy -
	navigateUp() hands in a view into _path, and an activated row hands in one into the FileRef of
	the model that is replaced below. */
	auto target = path.str<Interface>();

	filesystem::Stat stat;
	if (!filesystem::stat(FileInfo{target}, stat) || stat.type != FileType::Dir) {
		return false;
	}

	_path = sp::move(target);

	/* A new model, not a new root on the old one: FilesystemModel hangs its listing callback on
	the root it was built with, and this pane only ever reads that one level. */
	_model = Rc<ui::FilesystemModel>::create(FileInfo{_path});
	if (!_model) {
		return false;
	}

	_model->setShowHidden(_showHidden);
	_model->setDirectoriesFirst(true);
	_model->setSortField(ui::FilesystemModel::SortField::Name);
	_model->setAsyncEnabled(true);
	_model->setMoveEnabled(false);
	_model->setRemoveMode(ui::FilesystemModel::RemoveMode::Deny);

	// The thumbnails of the directory being left are not worth finishing.
	if (_cache) {
		_cache->invalidate();
	}

	applyModel();
	rebuildBreadcrumbs();

	if (_navigateCallback) {
		_navigateCallback(_path);
	}
	return true;
}

bool FileBrowserView::navigateUp() {
	if (_path.empty() || _path == "/") {
		return false;
	}
	auto parent = filepath::root(StringView(_path));
	return navigate(parent.empty() ? StringView("/") : parent);
}

void FileBrowserView::applyModel() {
	// Only the active view is given the model, so the inactive one holds no nodes and costs
	// nothing while it is hidden.
	if (_mode == Mode::Icons) {
		_grid->setSource(_model);
		_table->setSource(nullptr);
	} else {
		_table->setSource(_model);
		_grid->setSource(nullptr);
	}

	/* The listing has to be asked for. TableView reads the root's children and never calls
	requestChilds() itself - a tree does it on expand, and a pane with no expander has to do it
	here or it stays empty for good. */
	if (_model) {
		_model->getRoot()->requestChilds(nullptr);
	}
}

void FileBrowserView::rebuildBreadcrumbs() {
	_breadcrumbs->removeAllChildren();

	// One crumb per component, each navigating to its own prefix. The first is the root itself,
	// which is how a pane deep in a tree gets back to "/" in one click.
	auto addCrumb = [&](StringView title, StringView target, int16_t z) {
		auto path = target.str<Interface>();
		auto button = _breadcrumbs->addChild(
				Rc<ui::Button>::create([this, path] { navigate(path); }), ZOrder(z));
		button->setString(title);
		button->getLabel()->setLocaleEnabled(false); // a directory name is content
		button->addStyleClass("fe-crumb");
	};

	addCrumb("/", "/", 0);

	// Split first, so the count is known before anything is built.
	Vector<Pair<String, String>> components; // title, prefix
	StringView rest(_path);
	rest.trimChars<StringView::Chars<'/'>>();

	String prefix;
	while (!rest.empty()) {
		auto component = rest.readUntil<StringView::Chars<'/'>>();
		rest.skipChars<StringView::Chars<'/'>>();
		if (component.empty()) {
			continue;
		}
		prefix.append("/").append(component.data(), component.size());
		components.emplace_back(component.str<Interface>(), prefix);
	}

	int16_t z = 1;
	size_t first = 0;
	if (components.size() > MaxCrumbs) {
		first = components.size() - MaxCrumbs;
		// Not a button: there is no one directory an ellipsis stands for.
		auto label = _breadcrumbs->addChild(Rc<basic2d::Label>::create(), ZOrder(z++));
		label->setString("…");
		label->setLocaleEnabled(false);
		label->addStyleClass("fe-crumb-ellipsis");
	}

	for (size_t i = first; i < components.size(); ++i) {
		addCrumb(components[i].first, components[i].second, z++);
	}

	_upButton->setEnabled(_path != "/" && !_path.empty());
}

void FileBrowserView::handleActivate(ui::FilesystemModel::Node *node) {
	auto ref = ui::FilesystemModel::getFileRef(node);
	if (ref && ref->dir) {
		navigate(ref->path);
	}
	// A file is not opened: this example does not hand a path to another application.
}

// --- settings ---------------------------------------------------------------------------------

void FileBrowserView::setMode(Mode mode) {
	_mode = mode;

	_grid->setVisible(_mode == Mode::Icons);
	_table->setVisible(_mode == Mode::Table);

	if (_model) {
		applyModel();
	}
	applySelectionOwnership();
	markContentSizeDirty();
}

void FileBrowserView::applySelectionOwnership() {
	if (!isRunning()) {
		return;
	}
	_grid->setSelectionOwned(_mode == Mode::Icons);
	_table->setSelectionOwned(_mode == Mode::Table);
}

void FileBrowserView::handleEnter(Scene *scene) {
	ui::Panel::handleEnter(scene);
	applySelectionOwnership();
}

void FileBrowserView::setIconSize(float value) {
	_iconSize = value;
	_grid->setIconSize(value);
}

void FileBrowserView::setRowHeight(float value) {
	_rowHeight = value;
	_table->setRowHeight(value);
	// Two lines of a name plus the gap under the picture.
	_grid->setLabelHeight(value * 1.4f);
}

void FileBrowserView::setShowHidden(bool value) {
	if (_showHidden == value) {
		return;
	}
	_showHidden = value;
	if (_model) {
		_model->setShowHidden(value);
		_model->refreshAll();
	}
}

void FileBrowserView::refresh() {
	if (_model) {
		_model->refreshAll();
	}
}

void FileBrowserView::setNavigateCallback(PathFunction &&cb) { _navigateCallback = sp::move(cb); }

// --- what is shown ----------------------------------------------------------------------------

size_t FileBrowserView::getEntryCount() const {
	if (!_model) {
		return 0;
	}
	size_t count = 0;
	for (auto &child : _model->getRoot()->getChildren()) {
		if (!child->isSpan()) {
			++count;
		}
	}
	return count;
}

size_t FileBrowserView::getViewRowCount() const {
	return _mode == Mode::Icons ? _grid->getEntryCount() : _table->getRowCount();
}

Value FileBrowserView::encodeEntries() const {
	Value ret(Value::Type::ARRAY);
	if (!_model) {
		return ret;
	}
	for (auto &child : _model->getRoot()->getChildren()) {
		if (child->isSpan()) {
			continue;
		}
		const auto &data = child->getData();
		Value entry;
		entry.setString(data.getString("name"), "name");
		entry.setString(data.getString("path"), "path");
		entry.setBool(data.getBool("dir"), "dir");
		entry.setInteger(data.getInteger("size"), "size");
		entry.setInteger(data.getInteger("mtime"), "mtime");
		ret.addValue(sp::move(entry));
	}
	return ret;
}

size_t FileBrowserView::getSelectedIndex() const {
	return _mode == Mode::Icons ? _grid->getSelectedIndex() : _table->getSelectedRow();
}

StringView FileBrowserView::getSelectedPath() const {
	if (_mode == Mode::Icons) {
		if (auto node = _grid->getEntry(_grid->getSelectedIndex())) {
			return ui::FilesystemModel::getPath(node);
		}
	} else if (auto row = _table->getRow(_table->getSelectedRow())) {
		return ui::FilesystemModel::getPath(row->node);
	}
	return StringView();
}

bool FileBrowserView::selectPath(StringView path) {
	if (_mode == Mode::Icons) {
		auto index = _grid->getIndexForPath(path);
		if (index == maxOf<size_t>()) {
			return false;
		}
		_grid->setSelectedIndex(index);
		return true;
	}

	for (size_t i = 0; i < _table->getRowCount(); ++i) {
		auto row = _table->getRow(i);
		if (row && ui::FilesystemModel::getPath(row->node) == path) {
			_table->setSelectedRow(i);
			return true;
		}
	}
	return false;
}

// --- geometry and formatting ------------------------------------------------------------------

void FileBrowserView::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();

	auto size = getContentSize();
	if (size.width <= 0.0f || size.height <= 0.0f) {
		return;
	}

	_header->setPosition(Vec2(0.0f, size.height));
	_header->setContentSize(Size2(size.width, HeaderHeight));

	const auto bodySize = Size2(size.width, sprt::max(0.0f, size.height - HeaderHeight));
	_grid->setPosition(Vec2::ZERO);
	_grid->setContentSize(bodySize);
	_table->setPosition(Vec2::ZERO);
	_table->setContentSize(bodySize);
}

String FileBrowserView::formatSize(int64_t bytes) {
	if (bytes < 0) {
		return String();
	}
	static constexpr StringView units[] = {"B", "KiB", "MiB", "GiB", "TiB"};

	double value = double(bytes);
	size_t unit = 0;
	while (value >= 1'024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
		value /= 1'024.0;
		++unit;
	}

	if (unit == 0) {
		return toString(bytes, " ", units[0]);
	}
	// One decimal, which is as much as a listing can use and still line up.
	return toString(uint64_t(value), ".", uint64_t(value * 10.0) % 10, " ", units[unit]);
}

String FileBrowserView::formatTime(int64_t micros) {
	if (micros <= 0) {
		return String();
	}
	return Time::microseconds(uint64_t(micros)).toFormat<Interface::StringType>("%Y-%m-%d %H:%M");
}

} // namespace stappler::xenolith::examples
