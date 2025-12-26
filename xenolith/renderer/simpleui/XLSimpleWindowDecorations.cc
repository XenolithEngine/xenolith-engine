/**
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

#include "XLSimpleWindowDecorations.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

static constexpr StringView s_windowHeaderClose =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMinimize =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M6 19h12v2H6z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMaximize =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M19,4H5C3.9,4,3,4.9,3,6v12c0,1.1,0.9,2,2,2h14c1.1,0,2-0.9,2-2V6C21,4.9,20.1,4,19,4z M19,18H5V6h14V18z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMaximizeExit =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M3 5H1v16c0 1.1.9 2 2 2h16v-2H3V5zm18-4H7c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V3c0-1.1-.9-2-2-2zm0 16H7V3h14v14z"/>
</svg>
)";

static constexpr StringView s_windowHeaderFullscreen =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/>
</svg>
)";

static constexpr StringView s_windowHeaderFullscreenExit =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M5 16h3v3h2v-5H5v2zm3-8H5v2h5V5H8v3zm6 11h2v-3h3v-2h-5v5zm2-11V5h-2v5h5V8h-3z"/>
</svg>
)";

static constexpr StringView s_windowHeaderMenu =
		R"(<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
<path fill="white" d="M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z"/>
</svg>
)";

const ComponentId WindowDecorationsState::Id;
const ComponentId WindowDecorationsTheme::Id;

bool WindowDecorationsButton::init(WindowDecorationsButtonType type) {
	if (!Node::init()) {
		return false;
	}

	setEventFlags(NodeEventFlags::HandleComponents);

	_type = type;

	_icon = addChild(Rc<VectorSprite>::create(Size2(24.0f, 24.0f)), ZOrder(2));
	_icon->setColor(Color::Grey_500);

	_background = addChild(Rc<VectorSprite>::create(Size2(24.0f, 24.0f)), ZOrder(1));
	_background->getImage()
			->addPath()
			->openForWriting([](vg::PathWriter &writer) { writer.addCircle(12.0f, 12.0f, 12.0f); })
			.setStyle(vg::DrawStyle::Fill)
			.setFillColor(Color::White);
	_background->setColor(Color::White);

	auto l = addSystem(Rc<InputListener>::create());

	if (_type == WindowDecorationsButtonType::ContextMenu) {
		l->setLayerFlags(WindowLayerFlags::WindowMenuLeft);
	}

	l->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			if (!_selected) {
				_selected = true;
				_background->stopAllActions();
				_background->runAction(Rc<TintTo>::create(0.1f, Color::Grey_300));
			}
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			if (_selected) {
				_selected = false;
				_background->stopAllActions();
				_background->runAction(Rc<TintTo>::create(0.1f, Color::White));
			}
			break;
		default: break;
		}
		return true;
	}, InputMouseOverInfo(0, false));
	l->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			handleTap();
		}
		return true;
	}, InputTapInfo(1));

	return true;
}

void WindowDecorationsButton::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	if (_icon) {
		_icon->setAnchorPoint(Anchor::Middle);
		_icon->setContentSize(_contentSize - Size2(6.0f, 6.0f));
		_icon->setPosition(_contentSize / 2.0f);
	}

	if (_background) {
		_background->setAnchorPoint(Anchor::Middle);
		_background->setContentSize(_contentSize);
		_background->setPosition(_contentSize / 2.0f);
	}
}

void WindowDecorationsButton::handleComponentsDirty() {
	Node::handleComponentsDirty();

	bool dirty = false;

	findParentWithComponent<WindowDecorationsState>(
			[&](NotNull<Node>, NotNull<const WindowDecorationsState> state, uint32_t) {
		if (hasFlag(state->capabilities, WindowCapabilities::GripGuardsRequired)) {
			if (auto l = getSystemByType<InputListener>()) {
				l->setLayerFlags(l->getLayerFlags() | WindowLayerFlags::GripGuard);
			}
		}
		if (_state != state->state) {
			_state = state->state;
			dirty = true;
		}
		return false; // stop iteration
	});

	findParentWithComponent<WindowDecorationsTheme>(
			[&](NotNull<Node>, NotNull<const WindowDecorationsTheme> theme, uint32_t) {
		if (theme->icon != _iconTheme) {
			_iconTheme = theme->icon;
			dirty = true;
		}
		return false; // stop iteration
	});

	if (dirty) {
		updateState();
	}
}

void WindowDecorationsButton::handleTap() {
	if (!_director) {
		return;
	}

	auto w = _director->getWindow();
	if (!w) {
		return;
	}

	switch (_type) {
	case WindowDecorationsButtonType::Close: w->close(true); break;
	case WindowDecorationsButtonType::Minimize: w->enableState(WindowState::Minimized); break;
	case WindowDecorationsButtonType::Maximize:
		if (hasFlagAll(_state, WindowState::Maximized)) {
			w->disableState(WindowState::Maximized);
		} else {
			w->enableState(WindowState::Maximized);
		}
		break;
	case WindowDecorationsButtonType::Fullscreen:
		if (hasFlagAll(_state, WindowState::Fullscreen)) {
			w->disableState(WindowState::Fullscreen);
		} else {
			w->enableState(WindowState::Fullscreen);
		}
		break;
	case WindowDecorationsButtonType::ContextMenu: w->openWindowMenu(Vec2::INVALID); break;
	}
}

void WindowDecorationsButton::updateState() {
	switch (_iconTheme) {
	case WindowDecorationsTheme::IconTheme::Default:
		switch (_type) {
		case WindowDecorationsButtonType::Close:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderClose));
			break;
		case WindowDecorationsButtonType::Minimize:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMinimize));
			break;
		case WindowDecorationsButtonType::Maximize:
			if (hasFlagAll(_state, WindowState::Maximized)) {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximizeExit));
			} else {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximize));
			}
			break;
		case WindowDecorationsButtonType::Fullscreen:
			if (hasFlagAll(_state, WindowState::Fullscreen)) {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreenExit));
			} else {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreen));
			}
			break;
		case WindowDecorationsButtonType::ContextMenu:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMenu));
			break;
		}
		_icon->setColor(Color::Grey_500);
		break;
	case WindowDecorationsTheme::IconTheme::Macos: {
		auto image = Rc<VectorImage>::create(Size2(24.0f, 24.0f));
		image->addPath()
				->setStyle(vg::DrawFlags::FillAndStroke)
				.setFillColor(Color::White)
				.setStrokeColor(Color::Grey_200)
				.setStrokeWidth(0.25f)
				.openForWriting([&](PathWriter &writer) { writer.addCircle(12.0f, 12.0f, 10.0f); });
		_icon->setImage(sp::move(image));

		_background->setVisible(false);
		switch (_type) {
		case WindowDecorationsButtonType::Close:
			if (hasFlag(_state, WindowState::Focused)) {
				_icon->setColor(Color4F(0.992f, 0.373f, 0.361f, 1.0f));
			} else {
				_icon->setColor(Color::Grey_400);
			}
			break;
		case WindowDecorationsButtonType::Minimize:
			if (hasFlag(_state, WindowState::Focused)) {
				_icon->setColor(Color4F(0.188f, 0.792f, 0.294f, 1.0f));
			} else {
				_icon->setColor(Color::Grey_400);
			}
			break;
		case WindowDecorationsButtonType::Maximize:
			if (hasFlag(_state, WindowState::Focused)) {
				_icon->setColor(Color4F(0.996f, 0.741f, 0.263f, 1.0f));
			} else {
				_icon->setColor(Color::Grey_400);
			}
			break;
		case WindowDecorationsButtonType::Fullscreen:
		case WindowDecorationsButtonType::ContextMenu: _icon->setVisible(false); break;
		}
		break;
	}
	}
}

bool WindowDecorationsDefault::init() {
	if (!WindowDecorations::init()) {
		return false;
	}

	_header = addChild(Rc<Layer>::create(Color::Grey_500));
	_header->setAnchorPoint(Anchor::MiddleTop);
	_header->setVisible(true);

	auto l = _header->addSystem(Rc<InputListener>::create());
	l->setLayerFlags(WindowLayerFlags::MoveGrip | WindowLayerFlags::WindowMenuRight);

	_buttonClose =
			addChild(Rc<WindowDecorationsButton>::create(WindowDecorationsButtonType::Close));
	_buttonMaximize =
			addChild(Rc<WindowDecorationsButton>::create(WindowDecorationsButtonType::Maximize));
	_buttonMinimize =
			addChild(Rc<WindowDecorationsButton>::create(WindowDecorationsButtonType::Minimize));
	_buttonFullscreen =
			addChild(Rc<WindowDecorationsButton>::create(WindowDecorationsButtonType::Fullscreen));
	_buttonMenu =
			addChild(Rc<WindowDecorationsButton>::create(WindowDecorationsButtonType::ContextMenu));

	return true;
}

void WindowDecorationsDefault::handleContentSizeDirty() {
	WindowDecorations::handleContentSizeDirty();

	_header->setContentSize(Size2(_contentSize.width, HeaderHeight));
	_header->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height));

	_componentsDirty = true;
}

void WindowDecorationsDefault::handleComponentsDirty() {
	WindowDecorations::handleComponentsDirty();

	float buttonSize = HeaderHeight - 4.0f;
	float buttonPadding = 2.0f;

	auto pos = Vec2(_contentSize.width - (HeaderHeight - buttonSize),
			_contentSize.height - HeaderHeight / 2.0f);
	auto size = Size2(buttonSize, buttonSize);
	auto increment = -(size.width + (HeaderHeight - buttonSize) + buttonPadding);
	auto anchor = Anchor::MiddleRight;

	if (auto theme = getComponent<WindowDecorationsTheme>()) {
		switch (theme->icon) {
		case WindowDecorationsTheme::IconTheme::Default: break;
		case WindowDecorationsTheme::IconTheme::Macos:
			pos.x = (HeaderHeight - buttonSize);
			increment = -increment - 4.0f;
			anchor = Anchor::MiddleLeft;
			break;
		}
	}

	if (_buttonClose->isVisible()) {
		_buttonClose->setAnchorPoint(anchor);
		_buttonClose->setPosition(pos);
		_buttonClose->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMaximize->isVisible()) {
		_buttonMaximize->setAnchorPoint(anchor);
		_buttonMaximize->setPosition(pos);
		_buttonMaximize->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMinimize->isVisible()) {
		_buttonMinimize->setAnchorPoint(anchor);
		_buttonMinimize->setPosition(pos);
		_buttonMinimize->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonFullscreen->isVisible()) {
		_buttonFullscreen->setAnchorPoint(anchor);
		_buttonFullscreen->setPosition(pos);
		_buttonFullscreen->setContentSize(size);
		pos.x += increment;
	}

	if (_buttonMenu->isVisible()) {
		_buttonMenu->setAnchorPoint(anchor);
		_buttonMenu->setPosition(pos);
		_buttonMenu->setContentSize(size);
	}
}

Padding WindowDecorationsDefault::getPadding() const { return Padding(HeaderHeight, 0.0f, 0.0f); }

void WindowDecorationsDefault::updateWindowState(WindowState state) {
	WindowDecorations::updateWindowState(state);

	auto allowedMove = hasFlag(state, WindowState::AllowedMove)
			&& !hasFlag(state, WindowState::Fullscreen)
			&& (!hasFlagAll(state, WindowState::Maximized)
					|| hasFlag(_capabilities, WindowCapabilities::AllowMoveFromMaximized));

	_header->getSystemByType<InputListener>()->setEnabled(allowedMove);

	_buttonClose->setVisible(hasFlag(state, WindowState::AllowedClose));
	_buttonMaximize->setVisible(
			hasFlagAll(state, WindowState::AllowedMaximizeHorz | WindowState::AllowedMaximizeVert));
	_buttonMinimize->setVisible(hasFlag(state, WindowState::AllowedMinimize));
	_buttonFullscreen->setVisible(hasFlag(state, WindowState::AllowedFullscreen));
	_buttonMenu->setVisible(hasFlag(state, WindowState::AllowedWindowMenu));

	if (hasFlag(state, WindowState::Focused)) {
		_header->setColor(Color::Grey_300);
	} else {
		_header->setColor(Color::Grey_400);
	}

	setOrUpdateComponent<WindowDecorationsState>([&](WindowDecorationsState *value) {
		bool dirty = false;
		if (value->capabilities != _capabilities) {
			value->capabilities = _capabilities;
			dirty = true;
		}
		if (value->state != state) {
			// Data was updated - return true
			value->state = state;
			dirty = true;
		}
		return dirty;
	});

	_contentSizeDirty = true;
}

void WindowDecorationsDefault::updateWindowTheme(const ThemeInfo &theme) {
	WindowDecorationsTheme decTheme;

	if (theme.systemTheme == "Aqua") {
		decTheme.icon = WindowDecorationsTheme::IconTheme::Macos;
	}

	setOrUpdateComponent<WindowDecorationsTheme>([&](WindowDecorationsTheme *value) {
		if (value->color != decTheme.color || value->icon != decTheme.icon) {
			// Data was updated - return true
			*value = decTheme;
			return true;
		}
		// data was not modified - return false
		return false;
	});
}

} // namespace stappler::xenolith::simpleui
