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
#include "XLUiLayoutSystem.h"
#include "XL2dIconSprite.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// inset used by the no-stylesheet fallback placement in handleContentSizeDirty
static constexpr float s_labelPadding = 8.0f;

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

Button::~Button() { }

bool Button::init(ButtonType type, Function<void()> &&cb) {
	if (!Panel::init()) {
		return false;
	}

	_type = type;

	_leftCallback = sp::move(cb);

	setType("button");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-button");
	// same fill / outline / border-radius appliers Panel registers for itself, under "button"
	registerStyleAppliers("button");

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-button-label");
	_label->setVisible(false);

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(2));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-button-icon");
	_icon->setVisible(false);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began:
			setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
				return state->handleHover(1); //
			});
			break;
		case GestureEvent::Activated: break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled:
			setOrUpdateComponent<InteractiveComponent>([](NotNull<InteractiveComponent> state) {
				return state->handleHover(-1); //
			});
			break;
		}
		return true;
	}, false);

	if (_type != ButtonType::General) {
		_listener->setWindowStateCallback([this](WindowState state, WindowState changes) {
			_windowState = state;
			updateState();
			return true;
		});
	}

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		switch (tap.event) {
		case GestureEvent::Began: break;
		case GestureEvent::Activated: return handleLeftTap(); break;
		case GestureEvent::Ended: break;
		case GestureEvent::Cancelled: break;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 1});

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		switch (tap.event) {
		case GestureEvent::Began: break;
		case GestureEvent::Activated: return handleRightTap(); break;
		case GestureEvent::Ended: break;
		case GestureEvent::Cancelled: break;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::MouseRight}), 1});

#if SPRT_APPLE
	_theme = ButtonIconTheme::Apple;
#else
	_theme = ButtonIconTheme::Default;
#endif

	return true;
}

bool Button::init(Function<void()> &&cb) {
	return Button::init(ButtonType::General, sp::move(cb)); //
}

bool Button::init(StringView str, Function<void()> &&cb) {
	if (!Button::init(ButtonType::General, sp::move(cb))) {
		return false;
	}

	setString(str);
	return true;
}

void Button::handleEnter(Scene *scene) {
	Panel::handleEnter(scene);

	_windowState = _director->getRenderServer()->getWindowState();

	updateState();
}

void Button::handleComponentsDirty(const ComponentMask &mask) {
	Panel::handleComponentsDirty(mask);

	// the icon is derived from the interactive state (hover glyphs on the Apple theme), so it is
	// rebuilt here - once per visit - rather than from every mutation that touches the component
	if (mask.contains(InteractiveComponent::Id.value)) {
		updateState();
	}
}

void Button::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// a LayoutSystem - created by the style resolver for `display:flex`, or added by hand - owns
	// the children's geometry; the fallback below would be a second writer of the same positions
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const bool hasIcon = _icon && _icon->isVisible();
	if (hasIcon) {
		_icon->setAnchorPoint(Anchor::MiddleLeft);
		_icon->setPosition(Vec2(s_labelPadding, _contentSize.height / 2.0f));
	}

	if (_label) {
		// the label takes what the icon left of the content box, and is centered in it
		const float offset = hasIcon ? _icon->getContentSize().width + s_labelPadding : 0.0f;
		_label->setAnchorPoint(Anchor::Middle);
		_label->setAlignment(font::TextAlign::Center);
		_label->setPosition(
				Vec2((_contentSize.width + offset) / 2.0f, _contentSize.height / 2.0f));
		_label->setWidth(sprt::max(_contentSize.width - offset - s_labelPadding * 2.0f, 0.0f));
	}
}

void Button::setString(StringView str) {
	if (_label) {
		_label->setString(str);
		_label->setVisible(!str.empty());
	}
}

void Button::setCallback(Function<void()> &&cb) { _leftCallback = sp::move(cb); }

void Button::setEnabled(bool value) {
	if (_enabled == value) {
		return;
	}

	_enabled = value;

	if (value) {
		removeStyleClass("disabled");
	} else {
		addStyleClass("disabled");
	}

	setOrUpdateComponent<InteractiveComponent>([&](NotNull<InteractiveComponent> state) {
		return state->updateState(value ? (state->state | InteractiveState::Enabled)
										: (state->state & ~InteractiveState::Enabled));
	});
}

StringView Button::getString() const {
	if (_label) {
		return _label->getString8();
	}
	return StringView();
}

void Button::setLabelColor(const Color4F &color) {
	if (_label) {
		_label->setColor(color);
	}
}

void Button::setLabelFontWeight(font::FontWeight weight) {
	if (_label) {
		_label->setFontWeight(weight);
	}
}

basic2d::IconSprite *Button::getIconSprite() const { return _icon; }

basic2d::Label *Button::getLabel() const { return _label; }

void Button::setIcon(IconName name) {
	if (_icon) {
		_icon->setIconName(name);
		_icon->setVisible(name != IconName::None);
	}
}

IconName Button::getIcon() const {
	if (_icon) {
		return _icon->getIconName();
	}
	return IconName::None;
}

void Button::updateState() {
	switch (_theme) {
	case ButtonIconTheme::Default:
		switch (_type) {
		case ButtonType::OsClose:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderClose));
			_icon->setVisible(true);
			break;
		case ButtonType::OsMinimize:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMinimize));
			_icon->setVisible(true);
			break;
		case ButtonType::OsMaximize:
			if (hasFlagAll(_windowState, WindowState::Maximized)) {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximizeExit));
			} else {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMaximize));
			}
			_icon->setVisible(true);
			break;
		case ButtonType::OsFullscreen:
			if (hasFlagAll(_windowState, WindowState::Fullscreen)) {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreenExit));
			} else {
				_icon->setImage(Rc<VectorImage>::create(s_windowHeaderFullscreen));
			}
			_icon->setVisible(true);
			break;
		case ButtonType::OsMenu:
			_icon->setImage(Rc<VectorImage>::create(s_windowHeaderMenu));
			_icon->setVisible(true);
			break;
		default: break;
		}
		break;
	case ButtonIconTheme::Apple: {
		// Traffic-light chrome is ONLY for OS window buttons. General buttons keep whatever
		// setIcon()/IconName drew — unconditionally replacing _icon with a circle here is what
		// turned every toolbar/row icon into a grey disk on macOS.
		if (_type == ButtonType::General) {
			break;
		}

		auto image = Rc<VectorImage>::create(Size2(24.0f, 24.0f));
		image->addPath()
				->setStyle(vg::DrawFlags::FillAndStroke)
				.setFillColor(Color::White)
				.setStrokeColor(Color::Grey_200)
				.setStrokeWidth(0.25f)
				.openForWriting([&](PathWriter &writer) { writer.addCircle(12.0f, 12.0f, 10.0f); });

		// macOS traffic-light hover: a dark glyph over the colored circle.
		bool hovered = false;
		if (auto ic = getComponent<InteractiveComponent>()) {
			hovered = ic->hoverCounter > 0;
		}
		if (hovered) {
			// macOS hover glyphs. All FILLED: a path with only a stroke does not render on the
			// icon sprite, so the ✕/− lines were invisible while the filled zoom triangles showed.
			// close = ✕ (filled outline), minimize = − (filled bar), zoom = two filled triangles.
			image->addPath()
					->setStyle(vg::DrawFlags::FillAndStroke)
					.setFillColor(Color4B(0x33, 0x33, 0x33, 0xFF))
					.setStrokeColor(Color4B(0x33, 0x33, 0x33, 0xFF))
					.setStrokeWidth(1.0f)
					.openForWriting([&](PathWriter &writer) {
				switch (_type) {
				case ButtonType::OsClose:
					// filled ✕, two thin diagonal bars (bigger span, thin)
					writer.moveTo(7.61f, 8.39f)
							.lineTo(15.61f, 16.39f)
							.lineTo(16.39f, 15.61f)
							.lineTo(8.39f, 7.61f);
					writer.closePath();
					writer.moveTo(16.39f, 8.39f)
							.lineTo(8.39f, 16.39f)
							.lineTo(7.61f, 15.61f)
							.lineTo(15.61f, 7.61f);
					writer.closePath();
					break;
				case ButtonType::OsMinimize: writer.addRect(7.0f, 11.45f, 10.0f, 1.1f); break;
				case ButtonType::OsMaximize:
					writer.moveTo(16.0f, 8.0f).lineTo(16.0f, 12.0f).lineTo(12.0f, 8.0f);
					writer.closePath();
					writer.moveTo(8.0f, 16.0f).lineTo(8.0f, 12.0f).lineTo(12.0f, 16.0f);
					writer.closePath();
					break;
				default: break;
				}
			});
		}
		_icon->setImage(sp::move(image));

		switch (_type) {
		case ButtonType::OsClose:
			if (hasFlag(_windowState, WindowState::Focused)) {
				_icon->setColor(Color4F(0.992f, 0.373f, 0.361f, 1.0f));
			} else {
				_icon->setColor(Color4F(0.5f, 0.5f, 0.5f, 1.0f));
			}
			_icon->setVisible(true);
			break;
		case ButtonType::OsMinimize:
			if (hasFlag(_windowState, WindowState::Focused)) {
				_icon->setColor(Color4F(0.996f, 0.741f, 0.263f, 1.0f));
			} else {
				_icon->setColor(Color4F(0.5f, 0.5f, 0.5f, 1.0f));
			}
			_icon->setVisible(true);
			break;
		case ButtonType::OsMaximize:
			if (hasFlag(_windowState, WindowState::Focused)) {
				_icon->setColor(Color4F(0.188f, 0.792f, 0.294f, 1.0f));
			} else {
				_icon->setColor(Color4F(0.5f, 0.5f, 0.5f, 1.0f));
			}
			_icon->setVisible(true);
			break;
		case ButtonType::OsFullscreen:
		case ButtonType::OsMenu: _icon->setVisible(false); break;
		default: break;
		}
		break;
	}
	}
}

bool Button::handleLeftTap() {
	if (!_enabled) {
		return false;
	}
	if (_leftCallback) {
		_leftCallback();
		return true;
	} else {
		if (!_director) {
			return false;
		}

		auto w = _director->getRenderServer();
		if (!w) {
			return false;
		}

		switch (_type) {
		case ButtonType::OsClose:
			w->close(true);
			return true;
			break;
		case ButtonType::OsMinimize:
			w->enableState(WindowState::Minimized);
			return true;
			break;
		case ButtonType::OsMaximize:
			if (hasFlagAll(_windowState, WindowState::Maximized)) {
				w->disableState(WindowState::Maximized);
			} else {
				w->enableState(WindowState::Maximized);
			}
			return true;
			break;
		case ButtonType::OsFullscreen:
			if (hasFlagAll(_windowState, WindowState::Fullscreen)) {
				w->disableState(WindowState::Fullscreen);
			} else {
				w->enableState(WindowState::Fullscreen);
			}
			return true;
			break;
		default: break;
		}
	}
	return false;
}

bool Button::handleRightTap() {
	if (!_enabled) {
		return false;
	}
	if (_rightCallback) {
		_rightCallback();
		return true;
	} else {
		if (!_director) {
			return false;
		}

		auto w = _director->getRenderServer();
		if (!w) {
			return false;
		}

		switch (_type) {
		case ButtonType::OsMenu:
			w->openWindowMenu(Vec2::INVALID);
			return true;
			break;
		default: break;
		}
	}
	return false;
}

} // namespace stappler::xenolith::ui
