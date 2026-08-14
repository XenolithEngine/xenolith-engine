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

#include "XLUiWindowFrame.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr StringView s_osButtonClass = StringView("os-button");
static constexpr StringView s_frameItemClass = StringView("frame-item");

WindowFrame::~WindowFrame() { }

bool WindowFrame::init() { return init(Config()); }

bool WindowFrame::init(Config &&config) {
	if (!Panel::init()) {
		return false;
	}

	setType("window-frame");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-window-frame");
	// the same fill / outline / border-radius appliers Panel registers for itself, under
	// "window-frame"
	registerStyleAppliers("window-frame");

	/* Creation order below is NOT display order. Every child is placed by the sheet's `order`, so
	that the macOS media block can move the traffic lights to the other end of the bar without a
	line of C++ changing - see the class documentation. */

	if (config.minimize) {
		_osMinimize = makeOsButton(ButtonType::OsMinimize, "os-minimize");
	}
	if (config.maximize) {
		_osMaximize = makeOsButton(ButtonType::OsMaximize, "os-maximize");
	}
	if (config.close) {
		_osClose = makeOsButton(ButtonType::OsClose, "os-close");
	}
	if (config.menuButton) {
		_osMenu = makeOsButton(ButtonType::OsMenu, "os-menu");
	}

	if (!config.iconImage.empty()) {
		_icon = addChild(Rc<basic2d::Sprite>::create(config.iconImage));
	} else if (config.icon != IconName::None) {
		_icon = addChild(Rc<basic2d::IconSprite>::create(config.icon));
	}

	if (_icon) {
		_icon->setName("frame-icon");
		// Either click opens the window menu, which is what a title bar icon is for. This is also
		// why Config::menuButton defaults off: the affordance already exists.
		auto listener = _icon->addSystem(Rc<InputListener>::create());
		listener->setLayerFlags(
				WindowLayerFlags::WindowMenuRight | WindowLayerFlags::WindowMenuLeft);
	}

	_leading = addChild(Rc<Node>::create());
	_leading->setName("frame-leading");

	_trailing = addChild(Rc<Node>::create());
	_trailing->setName("frame-trailing");

	{
		_titleLine = addChild(Rc<Panel>::create());
		_titleLine->setName("frame-title-line");
		_titleLine->addStyleClass("frame-title-line");

		_titleLabel = _titleLine->addChild(Rc<basic2d::Label>::create());
		_titleLabel->setType("label");
		_titleLabel->setName("frame-title");
		_titleLabel->setString(config.title);

		// MoveGrip is what makes a drag anywhere on the strip move the window; WindowMenuRight is
		// the conventional right-click-the-title-bar menu.
		auto listener = _titleLine->addSystem(Rc<InputListener>::create());
		listener->setLayerFlags(WindowLayerFlags::MoveGrip | WindowLayerFlags::WindowMenuRight);
	}

	return true;
}

Button *WindowFrame::makeOsButton(ButtonType type, StringView name) {
	auto button = addChild(Rc<Button>::create(type));
	button->setName(name);
	button->addStyleClass(s_osButtonClass);
	return button;
}

void WindowFrame::setTitle(StringView value) {
	if (_titleLabel) {
		_titleLabel->setString(value);
	}
}

StringView WindowFrame::getTitle() const {
	return _titleLabel ? _titleLabel->getString8() : StringView();
}

Node *WindowFrame::addLeading(Rc<Node> &&node) {
	auto ret = _leading->addChild(sp::move(node));
	ret->addStyleClass(s_frameItemClass);
	return ret;
}

Node *WindowFrame::addTrailing(Rc<Node> &&node) {
	auto ret = _trailing->addChild(sp::move(node));
	ret->addStyleClass(s_frameItemClass);
	return ret;
}

void WindowFrame::clearLeading() { _leading->removeAllChildren(); }

void WindowFrame::clearTrailing() { _trailing->removeAllChildren(); }

Button *WindowFrame::getOsButton(ButtonType type) const {
	switch (type) {
	case ButtonType::OsMinimize: return _osMinimize; break;
	case ButtonType::OsMaximize: return _osMaximize; break;
	case ButtonType::OsClose: return _osClose; break;
	case ButtonType::OsMenu: return _osMenu; break;
	default: break;
	}
	return nullptr;
}

float WindowFrame::getFrameHeight() const {
	// Zero until the first layout: the sheet has not been applied yet, so there is nothing to read
	// back and the default is the only honest answer.
	return _contentSize.height > 0.0f ? _contentSize.height : kDefaultFrameHeight;
}

} // namespace stappler::xenolith::ui
