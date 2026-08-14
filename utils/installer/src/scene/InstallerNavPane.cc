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

#include "InstallerNavPane.h"
#include "InstallerDialogs.h"
#include "InstallerStrings.h"

#include "XLUiButton.h"
#include "XLEventListener.h"
#include "XL2dIcons.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstallerNavRow::~InstallerNavRow() { }

bool InstallerNavRow::init(StringView label) {
	if (!ui::Panel::init()) {
		return false;
	}

	setName("nav-row-content");
	removeStyleClass("xl-ui-panel");
	registerStyleAppliers("nav-row-content");

	// The widget places both children itself, so a stylesheet must not add a second writer.
	setComponent<ui::SystemManagedLayout>();

	// Behind the label: a plain Layer, created opaque because a transparent one would zero the
	// subtree's opacity. Its colour comes from `.tree-row-progress` in the sheet.
	_fill = addChild(Rc<basic2d::Layer>::create(Color::Black), ZOrder(-1));
	_fill->addStyleClass("tree-row-progress");
	_fill->setAnchorPoint(Anchor::BottomLeft);
	_fill->setVisible(false);

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("tree-label");
	_label->setString(label);
	_label->setAnchorPoint(Anchor::MiddleLeft);

	return true;
}

void InstallerNavRow::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();

	if (_label) {
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setPosition(Vec2(0.0f, _contentSize.height / 2.0f));
	}
	if (_fill) {
		_fill->setAnchorPoint(Anchor::BottomLeft);
		_fill->setPosition(Vec2::ZERO);
		_fill->setContentSize(
				Size2(_contentSize.width * (sprt::isnan(_progress) ? 0.0f : _progress),
						_contentSize.height));
	}
}

void InstallerNavRow::setProgress(float value) {
	_progress = value;
	if (_fill) {
		_fill->setVisible(!sprt::isnan(value));
	}
	markContentSizeDirty();
}

InstallerNavPane::~InstallerNavPane() { }

bool InstallerNavPane::init() {
	if (!ui::Panel::init()) {
		return false;
	}

	setName("nav-pane");
	removeStyleClass("xl-ui-panel");
	registerStyleAppliers("nav-pane");

	_tree = addChild(Rc<ui::TreeView>::create());
	_tree->setName("nav-tree");
	// The root IS a row: "Xenolith" opens the welcome page (design.md).
	_tree->setRootVisible(true);
	_tree->setRowCallback([this](ui::TreeView::RowBuilder &builder) {
		buildRow(builder); //
	});
	_tree->setActivateCallback([this](size_t index, const ui::TreeView::Row &row) {
		handleRowActivated(index, row); //
	});
	_tree->setSelectCallback([this](size_t index, const ui::TreeView::Row &row) {
		handleRowActivated(index, row); //
	});

	return true;
}

void InstallerNavPane::handleEnter(Scene *scene) {
	ui::Panel::handleEnter(scene);

	auto controller = AppController::getInstance();
	if (!controller) {
		return;
	}

	_tree->setSource(controller->getNavSource());
	expandAll();

	auto listener = addSystem(Rc<EventListener>::create());
	// Row DATA arrives through the Source; these are the two things a Source cannot express.
	listener->listenForEvent(AppController::onInstalledStateChanged, [this](const Event &) {
		_tree->invalidateSource();
		expandAll();
	});
	listener->listenForEvent(AppController::onEngineRefsChanged, [this](const Event &) {
		_tree->invalidateSource();
		expandAll();
	});
	listener->listenForEvent(AppController::onCatalogueChanged, [this](const Event &) {
		_tree->invalidateSource();
		expandAll();
	});
	listener->listenForEvent(AppController::onJobProgress, [this](const Event &event) {
		applyJobProgress(static_cast<JobId>(event.getDataValue().getInteger()));
	});
	listener->listenForEvent(AppController::onJobFinished, [this](const Event &event) {
		applyJobProgress(static_cast<JobId>(event.getDataValue().getInteger()));
	});
}

void InstallerNavPane::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();
	if (_tree) {
		_tree->setAnchorPoint(Anchor::BottomLeft);
		_tree->setPosition(Vec2::ZERO);
		_tree->setContentSize(_contentSize);
	}
}

void InstallerNavPane::expandAll() {
	if (!_tree) {
		return;
	}
	// Open every branch: this tree is a summary of what is installed, and a collapsed one hides the
	// only thing it has to say. Expanding a row inserts its children as new rows, so the sweep is
	// repeated until nothing more opens - bounded, because the tree is three levels deep.
	for (uint32_t pass = 0; pass < 3; ++pass) {
		bool changed = false;
		const auto rows = _tree->getRows();
		for (size_t i = 0; i < rows.size(); ++i) {
			if (rows[i].isCategory() && !_tree->isRowExpanded(i)) {
				changed = _tree->expandRow(i) || changed;
			}
		}
		if (!changed) {
			break;
		}
	}
}

PageId InstallerNavPane::pageForNode(StringView node) {
	if (node == "engine") {
		return PageId::Engines;
	}
	if (node == "host") {
		return PageId::Hosts;
	}
	if (node == "target") {
		return PageId::Targets;
	}
	return PageId::Welcome;
}

void InstallerNavPane::buildRow(ui::TreeView::RowBuilder &builder) {
	const auto &data = builder.getData();
	const auto node = data.getString("node");

	if (!data.getBool("enabled") && node == "projects") {
		// Present but inert: project management is a separate piece of work, and leaving the branch
		// out entirely would make its later return a structural change rather than one Value.
		builder.addStyleClass("disabled");
		return;
	}

	if (data.getBool("add")) {
		builder.addStyleClass("add-row");
		builder.setIcon(basic2d::IconName::Content_add_solid);
		return;
	}

	if (node == "root") {
		builder.setIcon(basic2d::IconName::Action_home_solid);
		return;
	}
	if (node == "group") {
		builder.addStyleClass("group-row");
		return;
	}

	// A leaf that stands for something installed: it can be removed, so it carries the delete
	// button design.md asks for. The button is present always and revealed by
	// `tree-row:hover .tree-delete` — the CSS subset has :hover and a descendant combinator, so no
	// hover bookkeeping is needed in C++. It stays clickable while transparent, which is why every
	// removal goes through a confirmation.
	if (data.getBool("removable")) {
		auto id = toString(data.getString("id"));
		auto nodeStr = toString(node);
		auto button = Rc<ui::Button>::create([this, nodeStr, id] { confirmRemove(nodeStr, id); });
		button->setIcon(basic2d::IconName::Action_delete_solid);
		button->addStyleClass("tree-delete");
		builder.addTrailing(button.get());
	}

	// Take over the label slot with a node that can also show progress, and remember it by key so
	// onJobProgress can reach it without going anywhere near the Source.
	const auto key = toString(data.getString("key"));
	auto content = Rc<InstallerNavRow>::create(data.getString("name"));
	if (!key.empty()) {
		if (auto controller = AppController::getInstance()) {
			// Built with the CURRENT progress: a row scrolling back into view must not appear idle
			// while its download is still running.
			if (const auto *job = controller->findJob(JobKind::ComponentInstall, key)) {
				content->setProgress(job->total > 0
								? static_cast<float>(double(job->bytes) / double(job->total))
								: nan());
			}
		}
		_rowNodes.insert_or_assign(key, content);
	}
	builder.setContent(content.get());
}

void InstallerNavPane::handleRowActivated(size_t index, const ui::TreeView::Row &row) {
	const auto node = row.data.getString("node");
	if (node == "projects") {
		return; // inert
	}

	/* design.md: a nested entry has no page of its own, it opens the page of its SET. So a leaf and
	its branch resolve to the same page, and clicking a toolchain simply takes you to the table that
	lists it.

	A branch row does NOT name its set in `node` - that says "group" for all three of them, and the
	set is in `branch`. Reading `node` alone sent Engines, Hosts and Targets to the welcome page. */
	StringView which = node;
	if (node == "group") {
		which = row.data.getString("branch");
	} else if (node.empty() && row.isCategory()) {
		which = StringView("root");
	}
	const auto page = pageForNode(which);
	_current = page;
	if (_selectCallback) {
		_selectCallback(page, row.data.getString("id"));
	}
	(void)index;
}

void InstallerNavPane::confirmRemove(StringView node, StringView id) {
	auto *scene = getScene();
	auto *director = scene ? scene->getDirector() : nullptr;
	auto *window = director ? static_cast<AppWindow *>(director->getRenderServer()) : nullptr;
	if (!window) {
		return;
	}

	auto controller = AppController::getInstance();
	if (!controller || node == "engine") {
		// Removing an engine clone is not wired yet: the core has no removeEngine(), and deleting
		// the directory behind the running build root would be worse than refusing.
		return;
	}

	const auto kind = (node == "host") ? Kind::Host : Kind::Target;
	const auto idStr = toString(id);
	showConfirmDialog(window, strings::confirmDeleteTitle(), strings::confirmDeleteMessage(idStr),
			strings::actionDelete(), ConfirmTone::Danger,
			[controller, kind, idStr] { controller->uninstallComponent(kind, idStr); });
}

void InstallerNavPane::applyJobProgress(JobId id) {
	auto controller = AppController::getInstance();
	if (!controller || !_tree) {
		return;
	}
	const auto *job = controller->getJob(id);
	if (!job || job->kind != JobKind::ComponentInstall) {
		return;
	}

	// Write the fraction into the node that is already on screen. Deliberately NOT a Source dirty:
	// that would rebuild the row nodes on every progress tick. A row outside the scroll window has
	// no entry here and needs nothing — it is rebuilt with the current progress when it returns.
	auto it = _rowNodes.find(job->target);
	if (it == _rowNodes.end()) {
		return;
	}
	if (!job->isActive()) {
		it->second->setProgress(nan());
		return;
	}
	it->second->setProgress(
			job->total > 0 ? static_cast<float>(double(job->bytes) / double(job->total)) : nan());
}

void InstallerNavPane::selectPage(PageId page) {
	_current = page;
	if (!_tree) {
		return;
	}
	const auto rows = _tree->getRows();
	for (size_t i = 0; i < rows.size(); ++i) {
		const auto node = rows[i].data.getString("node");
		if (rows[i].isCategory() && pageForNode(node) == page) {
			_tree->setSelectedRow(i);
			return;
		}
	}
}

} // namespace stappler::xenolith::installer
