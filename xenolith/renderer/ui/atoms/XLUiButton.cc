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

#include "XLUiButton.h"
#include "XL2dIconSprite.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

Button::~Button() { }

bool Button::init(Function<void()> &&cb) {
	if (!VectorSprite::init()) {
		return false;
	}

	_callback = cb;

	setType("button");
	addStyleClass("xl-ui-button");

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-button-label");
	_label->setVisible(false);

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(2));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-button-icon");
	_icon->setVisible(false);

	addSystem(Rc<StyleResolver>::create(true));

	return true;
}

void Button::handleContentSizeDirty() {
	VectorSprite::handleContentSizeDirty();

	auto image = Rc<VectorImage>::create(_contentSize);

	image->addPath()
			->openForWriting([&](PathWriter &writer) {
		writer.addRect(Rect(0, 0, _contentSize.width, _contentSize.height));
	}).setFillColor(Color4B::WHITE);

	setImage(sp::move(image));
}

void Button::setString(StringView str) {
	if (_label) {
		_label->setString(str);
		_label->setVisible(!str.empty());
	}
}

StringView Button::getString() const {
	if (_label) {
		return _label->getString8();
	}
	return StringView();
}

void Button::setIcon(IconName name) {
	if (_icon) {
		_icon->setIconName(name);
	}
}

IconName Button::getIcon() const {
	if (_icon) {
		return _icon->getIconName();
	}
	return IconName::None;
}


} // namespace stappler::xenolith::ui
