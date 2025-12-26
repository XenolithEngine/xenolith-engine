/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "MaterialMenu.h"
#include "MaterialMenuButton.h"
#include "MaterialMenuSeparator.h"
#include "XLDynamicStateSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

Menu::~Menu() { }

bool Menu::init() {
	if (!Surface::init(SurfaceStyle(ShapeFamily::RoundedCorners, ShapeStyle::ExtraSmall,
				Elevation::Level2, NodeStyle::SurfaceTonalElevated))) {
		return false;
	}

	_menuListener = addSystem(Rc<DataListener<MenuSource>>::create(
			[this](SubscriptionFlags flags) { handleSourceDirty(); }));

	_scroll = addChild(Rc<ScrollView>::create(ScrollView::Vertical), ZOrder(1));
	_scroll->setAnchorPoint(Vec2(0.0f, 1.0f));

	auto comp =
			_scroll->addSystem(Rc<DynamicStateSystem>::create(DynamicStateApplyMode::ApplyForAll));
	comp->enableScissor(Padding(-2.0f));

	_controller = _scroll->setController(Rc<ScrollController>::create());

	return true;
}

bool Menu::init(MenuSource *source) {
	if (!init()) {
		return false;
	}

	setMenuSource(source);

	return true;
}

void Menu::setMenuSource(MenuSource *source) { _menuListener->setSubscription(source); }

MenuSource *Menu::getMenuSource() const { return _menuListener->getSubscription(); }

void Menu::setEnabled(bool value) {
	_scroll->setEnabled(value);
	auto nodes = _controller->getNodes();
	if (!nodes.empty()) {
		for (auto it : nodes) {
			if (auto button = dynamic_cast<MenuButton *>(it.get())) {
				button->setEnabled(value);
			}
		}
	}
}

bool Menu::isEnabled() const { return _scroll->isEnabled(); }

void Menu::rebuildMenu() {
	_controller->clear();
	if (!_menuListener->getSubscription()) {
		return;
	}

	_controller->addPlaceholder(MenuLeadingHeight);

	auto &menuItems = _menuListener->getSubscription()->getItems();
	for (auto &item : menuItems) {
		if (item->getType() == MenuSourceItem::Type::Separator) {
			_controller->addItem([this, item](const ScrollController::Item &) -> Rc<Node> {
				return createSeparator(this, item);
			}, MenuVerticalPadding);
		} else if (item->getType() == MenuSourceItem::Type::Button) {
			_controller->addItem([this, item](const ScrollController::Item &) -> Rc<Node> {
				return createButton(this, static_cast<MenuSourceButton *>(item.get()));
			}, MenuItemHeight);
		} else if (item->getType() == MenuSourceItem::Type::Custom) {
			auto customItem = static_cast<MenuSourceCustom *>(item.get());
			const auto &func = customItem->getFactoryFunction();
			if (func) {
				float height = customItem->getHeight(this, _contentSize.width);
				_controller->addItem(
						[this, func, item, customItem](const ScrollController::Item &) -> Rc<Node> {
					return func(this, customItem);
				}, height);
			}
		}
	}
	_controller->addPlaceholder(MenuTrailingHeight);
	_scroll->setScrollDirty(true);
}

Rc<MenuButton> Menu::createButton(Menu *m, MenuSourceButton *btn) {
	return Rc<MenuButton>::create(m, btn);
}

Rc<MenuSeparator> Menu::createSeparator(Menu *m, MenuSourceItem *item) {
	return Rc<MenuSeparator>::create(m, item);
}

void Menu::handleContentSizeDirty() {
	Surface::handleContentSizeDirty();
	layoutSubviews();
}

void Menu::layoutSubviews() {
	auto size = getContentSize();
	_scroll->setPosition(Vec2(0, size.height));
	_scroll->setContentSize(Size2(size.width, size.height));
}

void Menu::setMenuButtonCallback(const ButtonCallback &cb) { _callback = cb; }

const Menu::ButtonCallback &Menu::getMenuButtonCallback() { return _callback; }

void Menu::onMenuButtonPressed(MenuButton *button) {
	if (_callback) {
		_callback(button);
	}
}

ScrollView *Menu::getScroll() const { return _scroll; }

void Menu::handleSourceDirty() {
	_controller->clear();
	rebuildMenu();
}

} // namespace stappler::xenolith::material2d
