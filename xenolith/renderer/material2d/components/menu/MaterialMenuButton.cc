/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "MaterialMenuButton.h"
#include "MaterialFloatingMenu.h"
#include "MaterialLabel.h"
#include "MaterialScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

float MenuButton::getMaxWidthForButton(MenuSourceButton *btn, font::FontController *c, float density) {
	float widthFront = 12.0f;
	float widthBack = 12.0f;

	if (btn->getNameIcon() != IconName::None) {
		widthFront += 18.0f + 12.0f;
	}

	if (!btn->getName().empty()) {
		auto style = TypescaleLabel::getTypescaleRoleStyle(TypescaleRole::LabelLarge, density);
		style.text.textTransform = font::TextTransform::Uppercase;
		widthFront += Label::getStringWidth(c, style, btn->getName(), true);
	}

	if (btn->getValueIcon() != IconName::None) {
		widthBack += 18.0f + 12.0f;
	}

	if (!btn->getValue().empty()) {
		auto style = TypescaleLabel::getTypescaleRoleStyle(TypescaleRole::LabelLarge, density);
		widthFront += Label::getStringWidth(c, style, btn->getValue(), true);
	}

	return widthFront + 4.0f + widthBack;
}

bool MenuButton::init(Menu *menu, MenuSourceButton *button) {
	if (!Button::init(SurfaceStyle({
		Elevation::Level1,
		ColorRole::Primary,
		NodeStyle::Text,
		ShapeStyle::None
	}))) {
		return false;
	}

	setMenu(menu);
	setMenuSourceButton(button);
	setFollowContentSize(false);
	setTapCallback(std::bind(&MenuButton::handleButton, this));

	return true;
}

void MenuButton::setWrapName(bool val) {
	if (_wrapName != val) {
		_wrapName = val;
		_contentSizeDirty = true;
	}
}

bool MenuButton::isWrapName() const {
	return _wrapName;
}

void MenuButton::handleButton() {
	if (auto s = _menuButtonListener->getSubscription()) {
		Rc<Menu> menu = getMenu();
		if (auto cb = s->getCallback()) {
			cb(this, s);
		}
		if (auto nextMenu = s->getNextMenu()) {
			if (auto content = dynamic_cast<SceneContent2d *>(_scene->getContent())) {
				auto posRight = content->convertToNodeSpace(convertToWorldSpace(Vec2(_contentSize.width, _contentSize.height)));

				FloatingMenu::push(content, nextMenu, posRight,
					FloatingMenu::Binding::OriginRight, _menu);
			}
		}
		if (menu) {
			menu->onMenuButtonPressed(this);
		}
	}
}

void MenuButton::layoutContent() {
	float offsetFront = 12.0f;
	float offsetBack = _contentSize.width - 12.0f;

	if (getTrailingIconName() != IconName::None) {
		_trailingIcon->setAnchorPoint(Anchor::MiddleRight);
		_trailingIcon->setPosition(Vec2(offsetBack, _contentSize.height / 2));
		offsetBack -= _trailingIcon->getContentSize().width + 12.0f;
	}

	if (!_labelValue->empty()) {
		_labelValue->setAnchorPoint(Anchor::MiddleRight);
		_labelValue->setPosition(Vec2(offsetBack, _contentSize.height / 2));
		offsetBack -= _labelValue->getContentSize().width + 12.0f;
	}

	if (getLeadingIconName() != IconName::None) {
		_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);
		_leadingIcon->setPosition(Vec2(offsetFront, _contentSize.height / 2));
		offsetFront += _leadingIcon->getContentSize().width + 12.0f;
	}

	_labelText->setAnchorPoint(Anchor::MiddleLeft);
	_labelText->setPosition(Vec2(offsetFront, _contentSize.height / 2));
	_labelText->setTextTransform(font::TextTransform::Uppercase);

	if (_wrapName) {
		_labelText->setWidth(offsetBack - offsetFront - 4.0f);
	} else {
		_labelText->setMaxWidth(offsetBack - offsetFront - 4.0f);
	}
}

}
