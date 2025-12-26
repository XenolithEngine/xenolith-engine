/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "MaterialNavigationDrawer.h"
#include "MaterialMenuSource.h"
#include "MaterialMenu.h"
#include "MaterialMenuButton.h"
#include "MaterialStyleContainer.h"
#include "MaterialScene.h"
#include "XLEventListener.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

XL_DECLARE_EVENT_CLASS(NavigationDrawer, onNavigation);

NavigationDrawer::~NavigationDrawer() { }

bool NavigationDrawer::init() {
	if (!Sidebar::init(Sidebar::Left)) {
		return false;
	}

	_navigation = setNode(Rc<Menu>::create(), ZOrder(1));
	_navigation->setAnchorPoint(Vec2::ZERO);
	_navigation->setMenuButtonCallback([this](MenuButton *b) {
		if (!b->getMenuSourceButton()->getNextMenu()) {
			hide();
		}
	});
	_navigation->setEnabled(false);

	/*_statusBarLayer = _navigation->addChild(Rc<Layer>::create(), 100);
	_statusBarLayer->setColor(Color::Grey_500);
	_statusBarLayer->setAnchorPoint(Vec2(0.0f, 1.0f));*/

	setBackgroundPassiveOpacity(0.0f);
	setBackgroundActiveOpacity(0.5f);

	_navigation->setEnabled(false);
	_listener->setEnabled(false);

	setNodeWidthCallback([](const Size2 &size) { return std::min(size.width - 56, 64.0f * 5.0f); });

	return true;
}

void NavigationDrawer::handleContentSizeDirty() {
	Sidebar::handleContentSizeDirty();

	if (auto source = _navigation->getMenuSource()) {
		source->setDirty();
	}
}

Menu *NavigationDrawer::getNavigationMenu() const { return _navigation; }

MenuSource *NavigationDrawer::getMenuSource() const { return _navigation->getMenuSource(); }
void NavigationDrawer::setMenuSource(MenuSource *source) {
	_listener->setEnabled(source != nullptr);
	_navigation->setMenuSource(source);
}

void NavigationDrawer::setStyle(const SurfaceStyle &style) { _navigation->setStyle(style); }

void NavigationDrawer::setStatusBarColor(const Color &color) { _statusBarLayer->setColor(color); }

void NavigationDrawer::onNodeEnabled(bool value) {
	Sidebar::onNodeEnabled(value);
	_navigation->setEnabled(value);
}

void NavigationDrawer::onNodeVisible(bool value) {
	Sidebar::onNodeVisible(value);
	_navigation->getScroll()->setScrollDirty(true);
	onNavigation(this, value);
}

} // namespace stappler::xenolith::material2d
