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

#include "InstallerShell.h"
#include "InstallerPage.h"
#include "InstallerSettingsPage.h"
#include "InstallerStrings.h"

#include "XLUiStyleResolver.h"
#include "XL2dIcons.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static StringView getPageName(PageId page) {
	switch (page) {
	case PageId::Welcome: return StringView("welcome");
	case PageId::Engines: return StringView("engines");
	case PageId::Hosts: return StringView("hosts");
	case PageId::Targets: return StringView("targets");
	}
	return StringView();
}

InstallerShell::~InstallerShell() { }

bool InstallerShell::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	setName("installer-shell");
	addSystem(Rc<ui::StyleResolver>::create(true));

	// The frame is the engine's (ui::WindowFrame); the installer supplies the title, the icon and
	// the settings button design.md puts in the user-space decorations.
	_frame = addChild(Rc<ui::WindowFrame>::create(ui::WindowFrame::Config{
		.title = strings::appTitle(),
		.iconImage = StringView("app-icon.png"),
	}));

	_settingsButton = static_cast<ui::Button *>(
			_frame->addTrailing(Rc<ui::Button>::create([this] { openSettings(); })));
	_settingsButton->setName("frame-settings");
	_settingsButton->setIcon(basic2d::IconName::Action_settings_solid);

	_body = addChild(Rc<Node>::create());
	_body->setName("shell-body");

	_nav = _body->addChild(Rc<InstallerNavPane>::create());
	_nav->setSelectCallback([this](PageId page, StringView) { showPage(page); });

	_content = _body->addChild(Rc<Node>::create());
	_content->setName("content-pane");

	_status = addChild(Rc<InstallerStatusBar>::create());

	return true;
}

void InstallerShell::handleEnter(Scene *scene) {
	basic2d::SceneLayout2d::handleEnter(scene);

	showPage(PageId::Welcome);

	/* Pages stay LAZY, and pre-building them would not help.

	Building one is a few hundred microseconds (getPage logs it); what costs is the first LAYOUT of a
	page full of text - the column and the table measured, every label shaped, the glyphs they need
	rasterized and uploaded. That cannot be moved off the click by constructing the page earlier: a
	hidden node is not visited, so it is never laid out and never sized (its content size stays 0x0,
	which the inspector will show). Building the pages up front measurably changes nothing, so they
	are built where they are needed. */
}

AppWindow *InstallerShell::appWindow() const {
	auto *scene = getScene();
	auto *director = scene ? scene->getDirector() : nullptr;
	return director ? static_cast<AppWindow *>(director->getRenderServer()) : nullptr;
}

void InstallerShell::openSettings() {
	if (auto *window = appWindow()) {
		showSettingsPage(window);
	}
}

InstallerPage *InstallerShell::getPage(PageId page) {
	auto it = _pages.find(page);
	if (it != _pages.end()) {
		return it->second;
	}

	// Building a page is the one thing here that used to happen on a click, so it is measured: the
	// number is what says whether pre-building them was worth it and whether a page has since grown
	// expensive enough to need building off the app thread.
	const auto started = Time::now();

	Rc<InstallerPage> created;
	if (page == PageId::Welcome) {
		created = Rc<InstallerWelcomePage>::create();
	} else {
		created = Rc<InstallerToolsPage>::create(page);
	}

	auto *node = _content->addChild(created.get());
	node->setVisible(false);
	_pages.emplace(page, static_cast<InstallerPage *>(node));

	log::info("installer", "getPage: built ", getPageName(page), " in ",
			(Time::now() - started).toMicros(), "us");
	return static_cast<InstallerPage *>(node);
}

void InstallerShell::showPage(PageId page) {
	auto *next = getPage(page);
	if (!next) {
		return;
	}

	// Swapped by visibility, not rebuilt: a page keeps its scroll position, and there are only four
	// of them.
	for (auto &it : _pages) {
		it.second->setVisible(it.second == next);
	}
	// Only now, with the node visible again: see InstallerPage::handleShown.
	next->handleShown();
	_current = page;
	if (_nav) {
		_nav->selectPage(page);
	}
}

} // namespace stappler::xenolith::installer
