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

#include "InstallerProjects.h"
#include "InstallerDialogs.h"
#include "InstallerStrings.h"
#include "InstallerLayout.h"

#include "XLUiStyleResolver.h"
#include "XL2dScrollController.h"
#include "XL2dIcons.h"
#include "XLAppWindow.h"
#include "XLAction.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLDynamicStateSystem.h"
#include "SPIProjects.h"
#include "SPITriple.h"
#include "SPIEngineSource.h"

#include <algorithm>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

namespace {

constexpr StringView kDefaultName = "my-app";

constexpr float kProjRowHeight = 56.0f;
// ~22 lines fit ~280px at 11px; keep under that so text cannot paint over the list
// even if scissor fails for a frame.
constexpr size_t kMaxLogLines = 40;

} // namespace

InstallerProjectsView::~InstallerProjectsView() { }

bool InstallerProjectsView::init() {
	if (!Node::init()) {
		return false;
	}
	setName("projects-area");
	setType("node");
	addStyleClass("projects-area");

	_listPane = addChild(Rc<Node>::create());
	_listPane->setName("proj-list-pane");
	_listPane->addStyleClass("proj-pane");

	_formPane = addChild(Rc<Node>::create());
	_formPane->setName("proj-form-pane");
	_formPane->addStyleClass("proj-pane");
	_formPane->setVisible(false);

	{
		_scroll =
				_listPane->addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
		_scroll->setName("proj-scroll");
		_scroll->addStyleClass("proj-scroll");
		_scrollController = Rc<basic2d::ScrollController>::create();
		_scrollController->setKeepNodes(true);
		_scroll->setController(_scrollController);
		_scroll->setIndicatorColor(Color4B(255, 255, 255, 77));
	}

	{
		// Solid panel (not a ScrollView): opaque fill + scissor so Label cannot bleed upward.
		_consolePanel =
				_listPane->addChild(Rc<basic2d::Layer>::create(Color4F(0.07f, 0.07f, 0.07f, 1.0f)));
		_consolePanel->setName("proj-console-panel");
		_consolePanel->addStyleClass("proj-console-panel");
		_consolePanel->setVisible(false);
		auto *scissor = _consolePanel->addSystem(
				Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
		if (scissor) {
			scissor->enableScissor();
		}
		_console = _consolePanel->addChild(Rc<basic2d::Label>::create());
		_console->setType("label");
		_console->addStyleClass("proj-console");
		_console->setString("");
		_console->setRenderingLevel(RenderingLevel::Transparent);
	}

	{
		_footer = _listPane->addChild(Rc<basic2d::Layer>::create(Color::Black));
		_footer->setName("proj-footer");
		_footer->addStyleClass("proj-footer");

		_btnOpenFolder = _footer->addChild(Rc<ui::Button>::create([this] {
			if (auto *window = appWindow()) {
				if (_controller) {
					_controller->openFolder(window, _location);
				}
			}
		}));
		_btnOpenFolder->addStyleClass("ghost");
		_btnOpenFolder->setIcon(basic2d::IconName::File_folder_open_solid);
		_btnOpenFolder->setString(strings::projectOpenFolder());

		_btnNew = _footer->addChild(Rc<ui::Button>::create([this] { showNewForm(); }));
		_btnNew->addStyleClass("primary");
		_btnNew->setIcon(basic2d::IconName::Content_add_solid);
		_btnNew->setString(strings::projectNew());
	}

	{
		auto back = _formPane->addChild(Rc<ui::Button>::create([this] { showList(); }));
		back->addStyleClass("ghost");
		back->addStyleClass("proj-back");
		back->setString(strings::projectBack());

		auto title = _formPane->addChild(Rc<basic2d::Label>::create());
		title->setType("label");
		title->addStyleClass("proj-form-title");
		title->setString(strings::projectNew());

		auto nameCap = _formPane->addChild(Rc<basic2d::Label>::create());
		nameCap->setType("label");
		nameCap->addStyleClass("proj-field-label");
		nameCap->setString(strings::projectName());

		_nameLabel = _formPane->addChild(Rc<basic2d::Label>::create());
		_nameLabel->setType("label");
		_nameLabel->addStyleClass("proj-location");
		_name = kDefaultName.str<memory::StandardInterface>();
		_nameLabel->setString(_name);

		auto editName = _formPane->addChild(Rc<ui::Button>::create([this] { onEditName(); }));
		editName->addStyleClass("ghost");
		editName->addStyleClass("proj-browse");
		editName->setString(strings::projectChoose());

		auto locCap = _formPane->addChild(Rc<basic2d::Label>::create());
		locCap->setType("label");
		locCap->addStyleClass("proj-field-label");
		locCap->setString(strings::projectLocation());

		_locationLabel = _formPane->addChild(Rc<basic2d::Label>::create());
		_locationLabel->setType("label");
		_locationLabel->addStyleClass("proj-location");
		_locationLabel->setString("");

		auto browse = _formPane->addChild(Rc<ui::Button>::create([this] { onBrowse(); }));
		browse->addStyleClass("ghost");
		browse->addStyleClass("proj-browse");
		browse->setString(strings::projectChoose());

		_statusLabel = _formPane->addChild(Rc<basic2d::Label>::create());
		_statusLabel->setType("label");
		_statusLabel->addStyleClass("proj-status");
		_statusLabel->setString("");

		auto create = _formPane->addChild(Rc<ui::Button>::create([this] { onCreate(); }));
		create->addStyleClass("primary");
		create->addStyleClass("proj-create");
		create->setString(strings::projectCreate());
	}

	_location = defaultProjectsLocation();
	if (_locationLabel) {
		_locationLabel->setString(_location);
	}
	return true;
}

AppWindow *InstallerProjectsView::appWindow() const {
	auto *scene = getScene();
	auto *director = scene ? scene->getDirector() : nullptr;
	auto *server = director ? director->getRenderServer() : nullptr;
	return static_cast<AppWindow *>(server);
}

void InstallerProjectsView::setController(InstallerController *controller) {
	_controller = controller;
	if (controller) {
		auto host = resolveHost(getNativeArch(), getNativeOs());
		_defaultTarget = host.native;
		auto tgts = controller->targets();
		if (!_defaultTarget.empty()
				&& std::find(tgts.begin(), tgts.end(), _defaultTarget) == tgts.end()) {
			_defaultTarget = tgts.empty() ? String() : tgts.front();
		}
	}
	reload();
}

void InstallerProjectsView::setBusy(bool busy) {
	_busy = busy;
	if (busy) {
		runAction(Rc<RenderContinuously>::create(), "ProjBusy"_tag);
	} else {
		stopActionByTag("ProjBusy"_tag);
	}
}

void InstallerProjectsView::reload() {
	if (!_controller) {
		return;
	}
	_controller->loadProjects(
			[this, self = Rc<InstallerProjectsView>(this)](Vector<ProjectEntry> list) {
		_projects = sp::move(list);
		rebuildList();
	});
}

void InstallerProjectsView::showNewForm() {
	_formVisible = true;
	if (_listPane) {
		_listPane->setVisible(false);
	}
	if (_formPane) {
		_formPane->setVisible(true);
	}
	if (_statusLabel) {
		_statusLabel->setString("");
	}
	if (_name.empty()) {
		_name = kDefaultName.str<memory::StandardInterface>();
	}
	if (_nameLabel) {
		_nameLabel->setString(_name);
	}
}

void InstallerProjectsView::showList() {
	_formVisible = false;
	if (_formPane) {
		_formPane->setVisible(false);
	}
	if (_listPane) {
		_listPane->setVisible(true);
	}
	reload();
}

void InstallerProjectsView::appendLog(StringView line) {
	_logLines.emplace_back(toString(line));
	if (_logLines.size() > kMaxLogLines) {
		_logLines.erase(_logLines.begin(),
				_logLines.begin() + static_cast<long>(_logLines.size() - kMaxLogLines));
	}

	String joined;
	for (const auto &it : _logLines) {
		joined.append(it);
		// `joined` is empty when the very first line is — back() would be UB there.
		if (joined.empty() || joined.back() != '\n') {
			joined.push_back('\n');
		}
	}
	if (_console) {
		_console->setString(joined);
	}
	if (_consolePanel) {
		_consolePanel->setVisible(!_logLines.empty());
	}
}

void InstallerProjectsView::rebuildList() {
	if (!_scrollController) {
		return;
	}
	_scrollController->clear();
	_scrollController->addPlaceholder(8.0f);

	if (_projects.empty()) {
		_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
			auto row = Rc<Node>::create();
			row->addStyleClass("proj-empty");
			auto lab = row->addChild(Rc<basic2d::Label>::create());
			lab->setType("label");
			lab->addStyleClass("proj-empty-label");
			lab->setString(strings::projectsEmpty());
			return row;
		}, 80.0f);
	} else {
		for (const auto &p : _projects) {
			_scrollController->addItem(
					[this, p](const basic2d::ScrollController::Item &) -> Rc<Node> {
				auto row = Rc<Node>::create();
				row->addStyleClass("proj-row");

				auto name = row->addChild(Rc<basic2d::Label>::create());
				name->setType("label");
				name->addStyleClass("proj-row-name");
				name->setString(p.name);

				auto path = row->addChild(Rc<basic2d::Label>::create());
				path->setType("label");
				path->addStyleClass("proj-row-path");
				path->setString(p.path);

				auto run = row->addChild(Rc<ui::Button>::create([this, p] { onBuild(p, true); }));
				run->addStyleClass("ghost");
				run->addStyleClass("proj-row-btn");
				run->setIcon(basic2d::IconName::Av_play_arrow_solid);
				run->setString(strings::projectRun());

				auto build =
						row->addChild(Rc<ui::Button>::create([this, p] { onBuild(p, false); }));
				build->addStyleClass("primary");
				build->addStyleClass("proj-row-btn");
				build->setIcon(basic2d::IconName::Action_build_solid);
				build->setString(strings::projectBuild());

				auto rm = row->addChild(Rc<ui::Button>::create([this, p] { onRemove(p); }));
				rm->addStyleClass("ghost");
				rm->addStyleClass("proj-row-icon");
				rm->setIcon(basic2d::IconName::Action_delete_solid);

				return row;
			}, kProjRowHeight);
		}
	}

	_scrollController->addPlaceholder(16.0f);
	_scrollController->commitChanges();
}

// Both pickers spawn a platform helper the user may sit on for minutes. The controller runs it on
// the app looper, so `onDone` lands back on this thread with nothing having blocked; `self` keeps
// the view alive until it does.
void InstallerProjectsView::onBrowse() {
	if (!_controller) {
		return;
	}
	auto *window = appWindow();
	if (!window) {
		return;
	}
	_controller->pickFolder(window, strings::projectChoose(),
			[this, self = Rc<InstallerProjectsView>(this)](String picked) {
		if (picked.empty()) {
			return;
		}
		_location = sp::move(picked);
		if (_locationLabel) {
			_locationLabel->setString(_location);
		}
	});
}

void InstallerProjectsView::onEditName() {
	if (!_controller) {
		return;
	}
	_controller->promptText(strings::projectName(),
			_name.empty() ? StringView(kDefaultName) : _name,
			[this, self = Rc<InstallerProjectsView>(this)](String typed) {
		if (typed.empty()) {
			return;
		}
		_name = sp::move(typed);
		if (_nameLabel) {
			_nameLabel->setString(_name);
		}
		if (_statusLabel) {
			_statusLabel->setString(
					isValidProjectName(_name) ? StringView() : strings::projectNameRule());
		}
	});
}

void InstallerProjectsView::onCreate() {
	if (!_controller || _busy) {
		return;
	}
	if (!isValidProjectName(_name)) {
		if (_statusLabel) {
			_statusLabel->setString(strings::projectNameRule());
		}
		return;
	}
	if (_location.empty()) {
		_location = defaultProjectsLocation();
	}
	if (_location.find(' ') != String::npos) {
		if (_statusLabel) {
			_statusLabel->setString(strings::projectPathNoSpace());
		}
		return;
	}

	auto engines = _controller->engines();
	String engine = engines.empty() ? String() : engines.front();
	bool engOk = false;
	resolveEngineRoot(_controller->layout(), StringView(), &engOk);
	if (!engOk && engine.empty()) {
		if (_statusLabel) {
			_statusLabel->setString(strings::projectNeedSdk());
		}
		return;
	}
	if (_defaultTarget.empty()) {
		auto tgts = _controller->targets();
		if (tgts.empty()) {
			if (_statusLabel) {
				_statusLabel->setString(strings::projectNeedSdk());
			}
			return;
		}
		_defaultTarget = tgts.front();
	}

	setBusy(true);
	if (_statusLabel) {
		_statusLabel->setString(strings::projectCreating());
	}
	// `self` here and below: a scaffold/build runs for minutes on the worker pool, far longer than
	// a tab switch. The controller's Ref only keeps ITSELF alive, so the view has to hold its own.
	_controller->createProject(_name, _location, engine, _defaultTarget,
			[this, self = Rc<InstallerProjectsView>(this)](bool ok, String err, ProjectEntry) {
		setBusy(false);
		if (!ok) {
			if (_statusLabel) {
				_statusLabel->setString(err.empty() ? String("failed") : err);
			}
			return;
		}
		showList();
	});
}

void InstallerProjectsView::onBuild(const ProjectEntry &p, bool run) {
	if (!_controller || _busy) {
		return;
	}
	setBusy(true);
	_logLines.clear();
	appendLog(toString(run ? "Run " : "Build ", p.name, " …"));
	const auto target = p.target.empty() ? _defaultTarget : p.target;
	_controller->buildProject(p.path, target, run, false,
			[this, self = Rc<InstallerProjectsView>(this)](StringView line) { appendLog(line); },
			[this, self = Rc<InstallerProjectsView>(this)](bool ok, String message) {
		setBusy(false);
		appendLog(ok ? toString("✓ ", message) : toString("✗ ", message));
	});
}

void InstallerProjectsView::onRemove(const ProjectEntry &p) {
	auto *win = appWindow();
	if (!win || !_controller || _busy) {
		return;
	}
	showConfirmDialog(win, strings::projectRemove(), strings::projectRemoveMessage(p.name),
			strings::projectRemove(), ConfirmTone::Danger,
			[this, self = Rc<InstallerProjectsView>(this), path = p.path] {
		_controller->removeProject(path,
				[this, self = Rc<InstallerProjectsView>(this)](bool) { reload(); });
	});
}

void showStorageDialog(NotNull<AppWindow> parent, InstallerController *controller) {
	String body = toString(strings::storageTitle(), "\n\n");
	if (controller) {
		const auto &layout = controller->layout();
		body += toString("engines: ", layout.getEnginesDir(), "\n");
		body += toString("hosts: ", layout.getHostsDir(), "\n");
		body += toString("targets: ", layout.getTargetsDir(), "\n");
		body += toString("data: ", layout.data, "\n");
	} else {
		body += "Controller unavailable.";
	}
	showConfirmDialog(parent, strings::storageTitle(), body, strings::actionClose(),
			ConfirmTone::Primary, [] { });
}

void showSettingsDialog(NotNull<AppWindow> parent) {
	using namespace strings;
	// Placeholder settings sheet: the confirm button is the only control the dialog offers, so
	// it cycles the language until there is a real settings surface.
	auto langName = [](Lang lang) -> StringView {
		switch (lang) {
		case Lang::En: return "en";
		case Lang::Ru: return "ru";
		case Lang::Zh: return "zh";
		}
		return "en";
	};
	auto nextLang = [](Lang lang) {
		switch (lang) {
		case Lang::En: return Lang::Ru;
		case Lang::Ru: return Lang::Zh;
		case Lang::Zh: return Lang::En;
		}
		return Lang::En;
	};

	String body = toString(settingsTitle(), "\n\nLanguage: ", langName(getLang()),
			"\n\nConfirm to switch to ", langName(nextLang(getLang())), ".");
	showConfirmDialog(parent, settingsTitle(), body, actionInstall(), ConfirmTone::Primary,
			[nextLang] { setLang(nextLang(getLang())); });
}

} // namespace stappler::xenolith::installer
