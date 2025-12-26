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

#include "MaterialFloatingMenu.h"
#include "MaterialOverlayLayout.h"
#include "MaterialMenuButton.h"
#include "XL2dSceneLayout.h"
#include "XLAppThread.h"
#include "XLFontController.h"
#include "XL2dLayer.h"
#include "XLInputListener.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class FloatingMenuLayout : public OverlayLayout {
public:
	virtual ~FloatingMenuLayout() { }

	virtual bool init(MenuSource *source, const Vec2 &globalOrigin, FloatingMenu::Binding b,
			Menu *root);
	;

	virtual void handlePushTransitionEnded(SceneContent2d *l, bool replace) override;

protected:
	FloatingMenu *_menu = nullptr;
};

bool FloatingMenuLayout::init(MenuSource *source, const Vec2 &globalOrigin, FloatingMenu::Binding b,
		Menu *root) {
	auto menu = Rc<FloatingMenu>::create(source, root);

	if (!OverlayLayout::init(globalOrigin, b, menu, Size2())) {
		return false;
	}

	_menu = menu;

	_readyCallback = [this](bool ready) { _menu->setReady(ready); };

	_closeCallback = [this]() {
		if (auto &cb = _menu->getCloseCallback()) {
			cb();
		}
	};

	return true;
}

void FloatingMenuLayout::handlePushTransitionEnded(SceneContent2d *l, bool replace) {
	float w = _menu->getMenuWidth(this);
	float h = _menu->getMenuHeight(this, w);

	_fullSize = Size2(w, h);

	OverlayLayout::handlePushTransitionEnded(l, replace);
}

void FloatingMenu::push(SceneContent2d *content, MenuSource *source, const Vec2 &globalOrigin,
		Binding b, Menu *root) {
	auto l = Rc<FloatingMenuLayout>::create(source, globalOrigin, b, root);

	content->pushOverlay(l);
}

bool FloatingMenu::init(MenuSource *source, Menu *root) {
	if (!Menu::init(source)) {
		return false;
	}

	setMenuButtonCallback(std::bind(&FloatingMenu::onMenuButton, this, std::placeholders::_1));

	_root = root;

	if (_root) {
		setElevation(Elevation(toInt(_root->getStyleOrigin().elevation) + 1));
	} else {
		setElevation(Elevation::Level3);
	}

	_scroll->setIndicatorVisible(_ready);

	return true;
}

void FloatingMenu::setCloseCallback(const CloseCallback &cb) { _closeCallback = cb; }
const FloatingMenu::CloseCallback &FloatingMenu::getCloseCallback() const { return _closeCallback; }

void FloatingMenu::close() {
	if (!_running) {
		return;
	}

	stopAllActions();
	if (auto l = dynamic_cast<FloatingMenuLayout *>(_parent)) {
		if (auto c = l->getSceneContent()) {
			c->popOverlay(l);
		}
	}
}

void FloatingMenu::closeRecursive() {
	if (_root) {
		if (auto r = dynamic_cast<FloatingMenu *>(_root)) {
			r->close();
		}
	}
	close();
}

void FloatingMenu::onCapturedTap() { close(); }

float FloatingMenu::getMenuWidth(Node *root) {
	float minWidth = 0;
	auto &items = _menuListener->getSubscription()->getItems();
	for (auto &item : items) {
		if (item->getType() == MenuSourceItem::Type::Custom) {
			float w = static_cast<MenuSourceCustom *>(item.get())->getMinWidth();
			if (w > minWidth) {
				minWidth = w;
			}
		} else if (item->getType() == MenuSourceItem::Type::Button) {
			auto btn = static_cast<MenuSourceButton *>(item.get());
			auto c = root->getDirector()->getApplication()->getExtension<font::FontController>();

			float w = MenuButton::getMaxWidthForButton(btn, c, root->getInputDensity());
			if (w > minWidth) {
				minWidth = w;
			}
		}
	}

	const float incr = MenuHorizontalIncrement;
	const auto &size = root->getContentSize();

	minWidth = incr * ceilf(minWidth / incr);
	if (minWidth > size.width - incr / 2) {
		minWidth = size.width - incr / 2;
	}

	return minWidth;
}

float FloatingMenu::getMenuHeight(Node *root, float width) {
	float height = MenuLeadingHeight + MenuTrailingHeight;
	auto &items = _menuListener->getSubscription()->getItems();
	for (auto &item : items) {
		if (item->getType() == MenuSourceItem::Type::Custom) {
			height += static_cast<MenuSourceCustom *>(item.get())->getHeight(this, width);
		} else if (item->getType() == MenuSourceItem::Type::Button) {
			height += MenuItemHeight;
		} else {
			height += MenuVerticalPadding;
		}
	}

	const float incr = MenuHorizontalIncrement;
	const auto &size = root->getContentSize();

	if (height > size.height - incr / 2) {
		height = size.height - incr / 2;
	}

	return height;
}

void FloatingMenu::setReady(bool value) {
	if (value != _ready) {
		_ready = value;
		_scroll->setIndicatorVisible(_ready);
	}
}

bool FloatingMenu::isReady() const { return _ready; }

void FloatingMenu::onMenuButton(MenuButton *btn) {
	if (!btn->getMenuSourceButton()->getNextMenu()) {
		setEnabled(false);
		runAction(Rc<Sequence>::create(0.15f, std::bind(&FloatingMenu::closeRecursive, this)));
	}
}

} // namespace stappler::xenolith::material2d
