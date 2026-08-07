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

#include "InstallerLayout.h"
#include "InstallerTitleBar.h"
#include "InstallerDialogs.h"
#include "InstallerStrings.h"
#include "InstallerGearMenu.h"
#include "InstallerProjects.h"

#include "XLUiBadge.h"
#include "XLUiButton.h"
#include "XLUiCheckbox.h"
#include "XLUiStyleResolver.h"
#include "XL2dIcons.h"
#include "XL2dScrollController.h"
#include "XLAppWindow.h"
#include "XLAction.h"
#include "XLScene.h"
#include "XLDirector.h"

#include "SPICatalogue.h"
#include "SPITriple.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr auto kLogTag = StringView("installer-ui");

static constexpr float kRowHeight = 44.0f;
static constexpr float kHeadRowHeight = 36.0f;
static constexpr float kGroupRowHeight = 40.0f;

namespace {

StringView statusLabel(RowStatus s) {
	switch (s) {
	case RowStatus::Installed: return strings::statusInstalled();
	case RowStatus::UpdateAvailable: return strings::statusUpdateAvailable();
	case RowStatus::NotInstalled: return strings::statusNotInstalled();
	}
	return strings::statusNotInstalled();
}

StringView statusVariant(RowStatus s) {
	switch (s) {
	case RowStatus::Installed: return "installed";
	case RowStatus::UpdateAvailable: return "update";
	case RowStatus::NotInstalled: return "not-installed";
	}
	return "not-installed";
}

// Badge flex fit-content often settles at width 0 inside the status slot before label metrics
// land; size the pill from the label so "Installed" / progress text stays visible.
void syncBadgeSize(ui::Badge *badge) {
	if (!badge) {
		return;
	}
	for (auto &child : badge->getChildren()) {
		if (auto *lab = dynamic_cast<basic2d::Label *>(child.get())) {
			const Size2 ls = lab->getContentSize();
			if (ls.width > 1.0f) {
				badge->setContentSize(Size2(ls.width + 24.0f, 22.0f));
			}
			return;
		}
	}
}

} // namespace

InstallerLayout::~InstallerLayout() { }

bool InstallerLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	setName("installer-layout");
	addSystem(Rc<ui::StyleResolver>::create(true));

	_titleBar = addChild(Rc<TitleBar>::create());

	// Shared shell header (Tauri-style): title | Packages/Projects tabs | meta | gear.
	_header = addChild(Rc<basic2d::Layer>::create(Color::Black));
	_header->setName("pkg-header");
	_header->addStyleClass("pkg-header");

	{
		auto title = _header->addChild(Rc<basic2d::Label>::create());
		title->setType("label");
		title->addStyleClass("pkg-title");
		title->setString(strings::appTitle());

		_tabPackages = _header->addChild(Rc<ui::Button>::create([this] { showPackagesTab(); }));
		_tabPackages->addStyleClass("nav-tab");
		_tabPackages->setString(strings::tabPackages());

		_tabProjects = _header->addChild(Rc<ui::Button>::create([this] { showProjectsTab(); }));
		_tabProjects->addStyleClass("nav-tab");
		_tabProjects->setString(strings::tabProjects());

		setNavTabSelected(_tabPackages, _tabProjects);

		auto spacer = _header->addChild(Rc<Node>::create());
		spacer->addStyleClass("pkg-spacer");

		auto host = resolveHost(getNativeArch(), getNativeOs());

		_releaseLabel = _header->addChild(Rc<basic2d::Label>::create());
		_releaseLabel->setType("label");
		_releaseLabel->addStyleClass("pkg-release");
		_releaseLabel->setString(toString(getDefaultRelease()));

		_nativeLabel = _header->addChild(Rc<basic2d::Label>::create());
		_nativeLabel->setType("label");
		_nativeLabel->addStyleClass("pkg-native");
		_nativeLabel->setString(host.native);

		_gearButton = _header->addChild(Rc<ui::Button>::create([this] {
			if (auto *win = appWindow()) {
				showGearMenu(win, _controller);
			}
		}));
		_gearButton->addStyleClass("gear");
		_gearButton->setIcon(basic2d::IconName::Action_settings_solid);
	}

	_packagesArea = addChild(Rc<basic2d::Layer>::create(Color::Black));
	_packagesArea->setName("packages-area");
	_packagesArea->setType("node");

	_projectsView = addChild(Rc<InstallerProjectsView>::create());
	_projectsView->setVisible(false);

	{
		_scroll = _packagesArea->addChild(
				Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
		_scroll->setName("pkg-scroll");
		_scroll->addStyleClass("pkg-scroll");

		_scrollController = Rc<basic2d::ScrollController>::create();
		_scrollController->setKeepNodes(true);
		_scroll->setController(_scrollController);
		_scroll->setIndicatorColor(Color4B(255, 255, 255, 77));
	}

	{
		_footer = _packagesArea->addChild(Rc<basic2d::Layer>::create(Color::Black));
		_footer->setName("pkg-footer");
		_footer->addStyleClass("pkg-footer");

		_btnRefresh = _footer->addChild(Rc<ui::Button>::create([this] { onRefreshAll(); }));
		_btnRefresh->addStyleClass("ghost");
		_btnRefresh->addStyleClass("footer-refresh");
		_btnRefresh->setIcon(basic2d::IconName::Navigation_refresh_solid);
		_btnRefresh->setString(strings::actionRefreshAll());

		_btnInstallSelected =
				_footer->addChild(Rc<ui::Button>::create([this] { confirmInstallSelected(); }));
		_btnInstallSelected->addStyleClass("ghost");
		_btnInstallSelected->addStyleClass("install-selected");
		_btnInstallSelected->setString(strings::actionInstallSelected(0));
		updateFooterButtons();

		_btnInstallEverything =
				_footer->addChild(Rc<ui::Button>::create([this] { confirmInstallEverything(); }));
		_btnInstallEverything->addStyleClass("primary");
		_btnInstallEverything->addStyleClass("footer-install-all");
		_btnInstallEverything->setIcon(basic2d::IconName::Content_bolt_solid);
		_btnInstallEverything->setString(strings::actionInstallEverything());
	}

	return true;
}

AppWindow *InstallerLayout::appWindow() const {
	auto *scene = getScene();
	auto *director = scene ? scene->getDirector() : nullptr;
	auto *server = director ? director->getRenderServer() : nullptr;
	return static_cast<AppWindow *>(server);
}

void InstallerLayout::setSelection(Kind kind, StringView id, bool on) {
	const auto key = rowKey(kind, id);
	if (on) {
		_selected.emplace(key);
	} else {
		_selected.erase(key);
	}
	updateFooterButtons();
}

bool InstallerLayout::isSelected(Kind kind, StringView id) const {
	return _selected.find(rowKey(kind, id)) != _selected.end();
}

void InstallerLayout::updateFooterButtons() {
	const auto n = _selected.size();
	if (_btnInstallSelected) {
		_btnInstallSelected->setString(strings::actionInstallSelected(n));
		_btnInstallSelected->setVisible(true);
		// Always occupy the same footer slot — hide-when-empty made Refresh/Install jump.
		if (n == 0) {
			_btnInstallSelected->addStyleClass("disabled");
			_btnInstallSelected->setOpacity(0.4f);
		} else {
			_btnInstallSelected->removeStyleClass("disabled");
			_btnInstallSelected->setOpacity(1.0f);
		}
	}
}

void InstallerLayout::setBusy(bool busy) {
	_busy = busy;
	if (_controller) {
		_controller->setBusy(busy);
	}
	// Downloads run on a worker; the main loop only paints on demand. Without a continuous
	// render lock the progress strings update in memory while the window stays frozen until
	// the mouse moves — and schedule/actions starve the same way.
	if (busy) {
		runAction(Rc<RenderContinuously>::create(), "BusyRender"_tag);
	} else {
		stopActionByTag("BusyRender"_tag);
	}
	updateFooterButtons();
}

void InstallerLayout::setEngineStatus(const EngineStatusInfo &info) {
	_engineInfo = info;
	requestRebuildPackages(true);
}

void InstallerLayout::reloadCatalogue() {
	if (!_controller || _busy) {
		return;
	}
	setBusy(true);
	// `self`: an install/refresh runs for minutes on the worker pool. The controller's Ref keeps
	// only itself alive, so the layout has to hold its own across every completion below. These
	// lambdas live in the controller, not in the layout, so the strong ref is not a cycle.
	_controller->loadCatalog([this, self = Rc<InstallerLayout>(this)](bool ok, String) {
		setBusy(false);
		if (ok) {
			onCatalogReady(_controller);
		}
	});
}

void InstallerLayout::bindStatusBadge(StringView key, ui::Badge *badge) {
	const auto k = toString(key);
	if (badge) {
		_statusBadges.insert_or_assign(k, badge);
		syncBadgeSize(badge);
	} else {
		_statusBadges.erase(k);
	}
}

void InstallerLayout::setRowProgress(StringView key, StringView text) {
	const auto k = toString(key);
	_progressText.insert_or_assign(k, toString(text));

	if (auto it = _statusBadges.find(k); it != _statusBadges.end() && it->second) {
		it->second->setText(text);
		it->second->setVariant("update");
		syncBadgeSize(it->second);
		return;
	}

	// Badge not built yet — one rebuild only, never on every byte tick.
	requestRebuildPackages(true);
}

void InstallerLayout::requestRebuildPackages(bool immediate) {
	_packagesDirty = true;
	if (immediate) {
		stopActionByTag("RebuildPackages"_tag);
		_rebuildPending = false;
		_packagesDirty = false;
		rebuildPackages();
		return;
	}
	if (_rebuildPending) {
		return;
	}
	_rebuildPending = true;
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.12f),
					  [this] {
		_rebuildPending = false;
		if (_packagesDirty) {
			_packagesDirty = false;
			rebuildPackages();
		}
	}),
			"RebuildPackages"_tag);
}

void InstallerLayout::confirmInstallEverything() {
	auto *win = appWindow();
	if (!win || !_controller || _busy) {
		return;
	}
	showConfirmDialog(win, strings::confirmInstallTitle(), strings::confirmInstallMessage(),
			strings::actionInstall(), ConfirmTone::Primary, [this] {
		setBusy(true);
		_progressText.insert_or_assign(String("engine"), toString(strings::phaseDownloading()));
		requestRebuildPackages(true);
		_controller->installForSystem(
				[this, self = Rc<InstallerLayout>(this)](StringView step,
						const InstallProgress &p) {
			const auto key = (step == "engine") ? String("engine") : rowKey(p.kind, p.id);
			setRowProgress(key,
					toString(strings::phaseDownloading(), " ", p.bytes / 1'000'000, " MB"));
		},
				[this, self = Rc<InstallerLayout>(this)](bool, String) {
			_progressText.clear();
			setBusy(false);
			reloadCatalogue();
		});
	});
}

void InstallerLayout::confirmInstallSelected() {
	auto *win = appWindow();
	if (!win || !_controller || _busy || _selected.empty()) {
		return;
	}
	Vector<Pair<Kind, String>> items;
	if (auto *cat = _controller->catalog()) {
		for (const auto &row : cat->rows) {
			if (isSelected(row.kind, row.id)
					&& (row.status == RowStatus::NotInstalled
							|| row.status == RowStatus::UpdateAvailable)) {
				items.emplace_back(row.kind, row.id);
			}
		}
	}
	if (items.empty()) {
		return;
	}
	const auto n = items.size();
	showConfirmDialog(win, strings::confirmInstallSelectedTitle(),
			strings::confirmInstallSelectedMessage(n), strings::actionInstall(),
			ConfirmTone::Primary, [this, items = sp::move(items)]() mutable {
		setBusy(true);
		for (const auto &it : items) {
			_progressText.insert_or_assign(rowKey(it.first, it.second),
					toString(strings::phaseDownloading()));
		}
		requestRebuildPackages(true);
		_controller->installSelected(sp::move(items),
				[this, self = Rc<InstallerLayout>(this)](const InstallProgress &p) {
			setRowProgress(rowKey(p.kind, p.id),
					toString(strings::phaseDownloading(), " ", p.bytes / 1'000'000, " MB"));
		}, [this, self = Rc<InstallerLayout>(this)](bool, String) {
			_progressText.clear();
			_selected.clear();
			setBusy(false);
			reloadCatalogue();
		});
	});
}

void InstallerLayout::confirmUninstall(Kind kind, StringView id, StringView label) {
	auto *win = appWindow();
	if (!win || !_controller || _busy) {
		return;
	}
	const auto idStr = toString(id);
	const auto labelStr = toString(label);
	showConfirmDialog(win, strings::confirmDeleteTitle(), strings::confirmDeleteMessage(labelStr),
			strings::actionDelete(), ConfirmTone::Danger, [this, kind, idStr] {
		setBusy(true);
		_controller->uninstallComponent(kind, idStr,
				[this, self = Rc<InstallerLayout>(this), kind, idStr](bool ok, String) {
			if (ok) {
				_controller->setRowStatus(kind, idStr, RowStatus::NotInstalled);
				_selected.erase(rowKey(kind, idStr));
			}
			setBusy(false);
			requestRebuildPackages(true);
		});
	});
}

void InstallerLayout::confirmPrepareEngine() {
	auto *win = appWindow();
	if (!win || !_controller || _busy) {
		return;
	}
	showConfirmDialog(win, strings::confirmEngineTitle(), strings::confirmEngineMessage(),
			strings::actionPrepare(), ConfirmTone::Primary, [this] {
		setBusy(true);
		_progressText.insert_or_assign(String("engine"), toString(strings::phaseDownloading()));
		requestRebuildPackages(true);
		_controller->prepareEngine(
				[this, self = Rc<InstallerLayout>(this)](int64_t bytes, int64_t) {
			setRowProgress("engine",
					toString(strings::phaseDownloading(), " ", bytes / 1'000'000, " MB"));
		}, [this, self = Rc<InstallerLayout>(this)](bool, String) {
			_progressText.clear();
			_controller->queryEngine(
					[this, self = Rc<InstallerLayout>(this)](const EngineStatusInfo &info) {
				setBusy(false);
				setEngineStatus(info);
			});
		});
	});
}

void InstallerLayout::onRefreshAll() {
	if (!_controller || _busy) {
		return;
	}
	bool hasUpdate = false;
	if (auto *cat = _controller->catalog()) {
		for (const auto &row : cat->rows) {
			if (row.status == RowStatus::UpdateAvailable) {
				hasUpdate = true;
				break;
			}
		}
	}
	if (!hasUpdate) {
		reloadCatalogue();
		return;
	}
	auto *win = appWindow();
	if (!win) {
		return;
	}
	showConfirmDialog(win, strings::confirmRefreshTitle(), strings::confirmRefreshMessage(),
			strings::actionRefresh(), ConfirmTone::Primary, [this] {
		setBusy(true);
		_controller->refreshComponents(false,
				[this, self = Rc<InstallerLayout>(this)](const InstallProgress &p) {
			setRowProgress(rowKey(p.kind, p.id),
					toString(strings::phaseDownloading(), " ", p.bytes / 1'000'000, " MB"));
		}, [this, self = Rc<InstallerLayout>(this)](bool, String) {
			_progressText.clear();
			setBusy(false);
			reloadCatalogue();
		});
	});
}

void InstallerLayout::onCatalogReady(InstallerController *controller) {
	_controller = controller;
	if (_projectsView) {
		_projectsView->setController(controller);
	}
	if (!controller || !controller->catalog()) {
		return;
	}

	const auto *cat = controller->catalog();
	log::info(kLogTag, "onCatalogReady: rows=", cat->rows.size());

	_releaseLabel->setString(cat->release);
	if (!cat->nativeId.empty()) {
		_nativeLabel->setString(cat->nativeId);
	}

	requestRebuildPackages(true);
	updateFooterButtons();

	if (_scrollController) {
		_scrollController->setAnimationPadding(8'000.0f);
		_scrollController->commitChanges();
	}
}

void InstallerLayout::setNavTabSelected(ui::Button *on, ui::Button *off) {
	// Colour lives on the label's own class (nav-tab-on/off). Flipping only the button's
	// `.selected` does not re-resolve the child label, and InheritedColorStyle from CSS
	// outranks setLabelColor — so the gold/dim must be a class change on the label node.
	auto applyLabel = [](ui::Button *btn, bool selected) {
		if (!btn) {
			return;
		}
		if (selected) {
			btn->addStyleClass("selected");
		} else {
			btn->removeStyleClass("selected");
		}
		if (auto *lab = btn->getLabel()) {
			if (selected) {
				lab->removeStyleClass("nav-tab-off");
				lab->addStyleClass("nav-tab-on");
			} else {
				lab->removeStyleClass("nav-tab-on");
				lab->addStyleClass("nav-tab-off");
			}
		}
	};
	applyLabel(on, true);
	applyLabel(off, false);
}

void InstallerLayout::showPackagesTab() {
	if (_packagesArea) {
		_packagesArea->setVisible(true);
	}
	if (_projectsView) {
		_projectsView->setVisible(false);
	}
	setNavTabSelected(_tabPackages, _tabProjects);
	markContentSizeDirty();
}

void InstallerLayout::showProjectsTab() {
	if (_packagesArea) {
		_packagesArea->setVisible(false);
	}
	if (_projectsView) {
		_projectsView->setVisible(true);
		if (_controller) {
			_projectsView->setController(_controller);
		}
		_projectsView->showList();
		_projectsView->markContentSizeDirty();
	}
	setNavTabSelected(_tabProjects, _tabPackages);
	markContentSizeDirty();
}

void InstallerLayout::dropScrollWarmup() {
	if (_scrollController) {
		_scrollController->dropAnimationPadding();
		_scrollController->commitChanges();
	}
}

void InstallerLayout::rebuildPackages() {
	if (!_scrollController || !_controller || !_controller->catalog()) {
		return;
	}
	const auto *cat = _controller->catalog();

	_statusBadges.clear();
	_scrollController->clear();
	_scrollController->addPlaceholder(8.0f);

	_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
		auto row = Rc<Node>::create();
		row->setType("node");
		row->addStyleClass("pkg-row");
		row->addStyleClass("head");
		auto check = row->addChild(Rc<Node>::create());
		check->addStyleClass("c-check");
		auto name = row->addChild(Rc<basic2d::Label>::create());
		name->setType("label");
		name->addStyleClass("c-name");
		name->setString(strings::colName());
		auto size = row->addChild(Rc<basic2d::Label>::create());
		size->setType("label");
		size->addStyleClass("c-size");
		size->setAlignment(font::TextAlign::Right);
		size->setString(strings::colSize());
		auto status = row->addChild(Rc<Node>::create());
		status->addStyleClass("c-status");
		auto caption = status->addChild(Rc<basic2d::Label>::create());
		caption->setType("label");
		caption->setString(strings::colStatus());
		return row;
	}, kHeadRowHeight);

	// Engine group
	_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
		auto group = Rc<Node>::create();
		group->addStyleClass("pkg-group");
		auto label = group->addChild(Rc<basic2d::Label>::create());
		label->setType("label");
		label->setString(strings::groupEngine());
		return group;
	}, kGroupRowHeight);

	_scrollController->addItem([this](const basic2d::ScrollController::Item &) -> Rc<Node> {
		auto row = Rc<Node>::create();
		row->addStyleClass("pkg-row");
		auto check = row->addChild(Rc<Node>::create());
		check->addStyleClass("c-check");
		if (!_engineInfo.ready) {
			auto prep = check->addChild(Rc<ui::Button>::create([this] { confirmPrepareEngine(); }));
			prep->addStyleClass("c-trash");
			prep->setIcon(basic2d::IconName::Content_add_solid);
		}

		auto name = row->addChild(Rc<basic2d::Label>::create());
		name->setType("label");
		name->addStyleClass("c-name");
		if (_engineInfo.ready) {
			name->setString(toString("engine @ ",
					_engineInfo.reference.empty() ? StringView("local")
												  : StringView(_engineInfo.reference)));
		} else {
			name->setString(strings::engineNotReady());
		}

		auto size = row->addChild(Rc<basic2d::Label>::create());
		size->setType("label");
		size->addStyleClass("c-size");

		auto statusSlot = row->addChild(Rc<Node>::create());
		statusSlot->addStyleClass("c-status");
		auto badge = statusSlot->addChild(Rc<ui::Badge>::create());
		badge->setName("status-badge");
		if (auto it = _progressText.find("engine"); it != _progressText.end()) {
			badge->setText(it->second);
			badge->setVariant("update");
		} else {
			badge->setText(
					_engineInfo.ready ? strings::statusInstalled() : strings::statusNotInstalled());
			badge->setVariant(_engineInfo.ready ? "installed" : "not-installed");
		}
		bindStatusBadge("engine", badge);
		return row;
	}, kRowHeight);

	auto addGroup = [&](StringView title, Kind kind, bool collapsed, Function<void()> &&toggle) {
		_scrollController->addItem(
				[title = toString(title), toggle = sp::move(toggle), collapsed](
						const basic2d::ScrollController::Item &) -> Rc<Node> {
			auto group = Rc<Node>::create();
			group->addStyleClass("pkg-group");
			auto btn = group->addChild(Rc<ui::Button>::create([toggle] { toggle(); }));
			btn->addStyleClass("group-toggle");
			btn->setIcon(collapsed ? basic2d::IconName::Navigation_chevron_right_solid
								   : basic2d::IconName::Navigation_expand_more_solid);
			auto label = group->addChild(Rc<basic2d::Label>::create());
			label->setType("label");
			label->setString(title);
			return group;
		},
				kGroupRowHeight);

		if (collapsed) {
			return;
		}
		for (const auto &crow : cat->rows) {
			if (crow.kind != kind) {
				continue;
			}
			const auto key = rowKey(crow.kind, crow.id);
			const auto name =
					crow.variant.empty() ? crow.triple : toString(crow.triple, " +", crow.variant);
			const auto size = toString(crow.size / 1'000'000, " MB");
			const auto rowId = crow.id;
			const auto status = crow.status;
			const auto native = crow.isNative;
			const auto selected = isSelected(kind, rowId);
			String prog;
			if (auto it = _progressText.find(key); it != _progressText.end()) {
				prog = it->second;
			}

			// `this` is captured raw on purpose: the layout owns _scrollController, which owns
			// this factory — an Rc back to the layout would be a reference cycle.
			_scrollController->addItem(
					[this, name, size, kind, id = rowId, status, native, selected, key,
							prog = sp::move(prog)](
							const basic2d::ScrollController::Item &) -> Rc<Node> {
				auto row = Rc<Node>::create();
				row->addStyleClass("pkg-row");
				if (native) {
					row->addStyleClass("native");
				}

				auto checkSlot = row->addChild(Rc<Node>::create());
				checkSlot->addStyleClass("c-check");
				if (status == RowStatus::Installed || status == RowStatus::UpdateAvailable) {
					auto trash = checkSlot->addChild(Rc<ui::Button>::create(
							[this, kind, id, name] { confirmUninstall(kind, id, name); }));
					trash->addStyleClass("c-trash");
					trash->setIcon(basic2d::IconName::Action_delete_solid);
				} else {
					auto box = checkSlot->addChild(Rc<ui::Checkbox>::create());
					box->setChecked(selected, true);
					box->setCallback([this, kind, id](bool on) { setSelection(kind, id, on); });
				}

				auto nameLabel = row->addChild(Rc<basic2d::Label>::create());
				nameLabel->setType("label");
				nameLabel->addStyleClass("c-name");
				nameLabel->setString(name);

				auto sizeLabel = row->addChild(Rc<basic2d::Label>::create());
				sizeLabel->setType("label");
				sizeLabel->addStyleClass("c-size");
				sizeLabel->setAlignment(font::TextAlign::Right);
				sizeLabel->setString(size);

				auto statusSlot = row->addChild(Rc<Node>::create());
				statusSlot->addStyleClass("c-status");
				auto badge = statusSlot->addChild(Rc<ui::Badge>::create());
				badge->setName("status-badge");
				if (!prog.empty()) {
					badge->setText(prog);
					badge->setVariant("update");
				} else {
					badge->setText(statusLabel(status));
					badge->setVariant(statusVariant(status));
				}
				bindStatusBadge(key, badge);
				return row;
			},
					kRowHeight);
		}
	};

	addGroup(strings::groupHosts(), Kind::Host, _hostsCollapsed, [this] {
		_hostsCollapsed = !_hostsCollapsed;
		requestRebuildPackages(true);
	});
	addGroup(strings::groupTargets(), Kind::Target, _targetsCollapsed, [this] {
		_targetsCollapsed = !_targetsCollapsed;
		requestRebuildPackages(true);
	});

	_scrollController->addPlaceholder(16.0f);
	_scrollController->commitChanges();

	// Label metrics often arrive a frame late — re-sync pill sizes once.
	stopActionByTag("BadgeRelayout"_tag);
	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.0f),
					  [this] {
		for (auto &it : _statusBadges) { syncBadgeSize(it.second); }
	}, Rc<DelayTime>::create(0.05f),
					  [this] {
		for (auto &it : _statusBadges) { syncBadgeSize(it.second); }
	}),
			"BadgeRelayout"_tag);
}

} // namespace stappler::xenolith::installer
