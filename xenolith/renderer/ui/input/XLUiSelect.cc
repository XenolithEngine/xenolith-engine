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

#include "XLUiSelect.h"
#include "XLUiLayoutSystem.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The chevron, and the horizontal breathing room the fallback placement leaves around the parts.
// Only the fallback: a styled select gets its LayoutSystem from `display:flex` and none of this
// runs.
static constexpr IconName s_selectArrowIcon = IconName::Navigation_arrow_drop_down_outline;
static constexpr float s_selectPadding = 10.0f;
static constexpr float s_selectGap = 8.0f;

// The two spellings differ only in what they read from; see the note in the header on why one
// cannot serve both.
template <typename Source>
static Vector<SelectOption> Select_makeOptions(Source names) {
	Vector<SelectOption> ret;
	ret.reserve(names.size());
	for (auto &it : names) {
		auto id = StringView(it).str<Interface>();
		// The title is a COPY of the id, not a view of it: the two are separate fields and an
		// option whose title aliased its id would change both when either was rewritten.
		ret.emplace_back(SelectOption{id, id});
	}
	return ret;
}

Vector<SelectOption> makeSelectOptions(SpanView<StringView> names) {
	return Select_makeOptions(names);
}

Vector<SelectOption> makeSelectOptions(SpanView<String> names) { return Select_makeOptions(names); }

Select::~Select() { }

bool Select::init() {
	if (!Panel::init()) {
		return false;
	}

	/* The InteractiveComponent has to EXIST from the first line, not from the first call that
	changes something: a node without one reads as state 0, so `:disabled` would match an untouched
	widget - and anything this init() builds from isEnabled() would be built disabled. */
	applyControlEnabled(this, true);

	setType("select");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-select");
	registerStyleAppliers("select");

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-select-icon");
	_icon->setVisible(false);

	_label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_label->setType("label");
	_label->addStyleClass("xl-ui-select-label");
	_label->setAlignment(font::TextAlign::Left);

	// Its own type rather than a second `icon`: a rule addressing `select > icon` cannot tell two
	// children of the same type apart, and the two mean different things.
	_arrow = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	_arrow->setType("select-arrow");
	_arrow->addStyleClass("xl-ui-select-arrow");
	_arrow->setIconName(s_selectArrowIcon);

	_listener = addSystem(Rc<InputListener>::create());

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			return handleTap();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::UP));
	keys.set(toInt(InputKeyCode::DOWN));
	keys.set(toInt(InputKeyCode::HOME));
	keys.set(toInt(InputKeyCode::END));
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	keys.set(toInt(InputKeyCode::SPACE));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	// A key event carries the pointer location, so the default filter would answer the arrows only
	// while the mouse hovers the control. A focused widget owns the keyboard wherever the pointer
	// is - the same seam, and the same reason, as ui::TextInput's.
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return _focused;
		}
		return cb(event);
	});

	// A tap outside gives focus up. Priority 1 puts it above the scene graph and its filter accepts
	// only points outside the widget, so it never competes with the tap above.
	_focusListener = addSystem(Rc<InputListener>::create());
	_focusListener->setPriority(1);
	_focusListener->addTapRecognizer([this](const GestureTap &) {
		// Not while the list is up: the tap that picks a row lands in another window, and blurring
		// on it would take the control out of the form ring mid-choice.
		if (!isOpen()) {
			blur();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_focusListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &) {
		return !isTouched(event.currentLocation, 0.0f);
	});
	// Off until there is focus to lose. A listener that registers every frame to watch for a blur
	// that cannot happen is work nobody asked for - ui::TextInput's does the same.
	_focusListener->setEnabled(false);

	updateContent();


	return true;
}

void Select::handleExit() {
	// The surface hangs off a window this node is leaving; a list left standing over a control that
	// is no longer on screen is one the user has to dismiss by hand.
	close();
	Panel::handleExit();
}

void Select::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// A LayoutSystem - from `display:flex` or added by hand - owns the children's geometry, and the
	// placement below would be a second writer of the same positions. Same rule as ui::Button's.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	float left = s_selectPadding;
	if (_icon && _icon->isVisible()) {
		_icon->setAnchorPoint(Anchor::MiddleLeft);
		_icon->setPosition(Vec2(left, height / 2.0f));
		left += _icon->getContentSize().width + s_selectGap;
	}

	float right = width - s_selectPadding;
	if (_arrow) {
		_arrow->setAnchorPoint(Anchor::MiddleRight);
		_arrow->setPosition(Vec2(right, height / 2.0f));
		right -= _arrow->getContentSize().width + s_selectGap;
	}

	if (_label) {
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setPosition(Vec2(left, height / 2.0f));
		_label->setWidth(sprt::max(right - left, 0.0f));
	}
}

void Select::setOptions(SpanView<SelectOption> options) {
	_options.clear();
	_options.reserve(options.size());
	for (auto &it : options) { _options.emplace_back(it); }

	// A list that no longer carries the current value leaves the control with nothing chosen: a
	// title held over from a set of options that is gone describes nothing.
	if (!_value.empty() && indexOf(_value) < 0) {
		_value.clear();
	}

	if (isOpen()) {
		// The surface was built from the previous list. Rebuilding it under the user is worse than
		// closing it: the row they were about to click would move.
		close();
	}

	updateContent();
}

int32_t Select::indexOf(StringView id) const {
	for (uint32_t i = 0; i < uint32_t(_options.size()); ++i) {
		if (StringView(_options[i].id) == id) {
			return int32_t(i);
		}
	}
	return -1;
}

const SelectOption *Select::getSelectedOption() const {
	auto index = indexOf(_value);
	return index < 0 ? nullptr : &_options[uint32_t(index)];
}

bool Select::setValue(StringView id, bool silent) {
	if (id.empty()) {
		if (_value.empty()) {
			return true;
		}
		_value.clear();
		updateContent();
		if (!silent && _changeCallback) {
			_changeCallback(StringView());
		}
		return true;
	}

	if (indexOf(id) < 0) {
		return false;
	}
	if (StringView(_value) == id) {
		return true;
	}

	_value = id.str<Interface>();
	updateContent();
	if (!silent && _changeCallback) {
		_changeCallback(_value);
	}
	return true;
}

void Select::setPlaceholder(StringView text) {
	if (StringView(_placeholder) == text) {
		return;
	}
	_placeholder = text.str<Interface>();
	updateContent();
}

void Select::setChangeCallback(ChangeCallback &&cb) { _changeCallback = sp::move(cb); }

void Select::setEnabled(bool value) {
	// The lock has the last word, and remembers what was asked for so unlocking can give it
	// back. A no-op, and one pointer test, on a control nobody locked.
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	if (!value) {
		close();
		blur();
	}
	updateInteractiveState();
}

bool Select::step(int32_t delta) {
	if (!isEnabled() || _options.empty() || delta == 0) {
		return false;
	}

	const int32_t count = int32_t(_options.size());
	const int32_t dir = delta > 0 ? 1 : -1;
	int32_t index = indexOf(_value);

	// Nothing chosen yet: a step in either direction lands on the first enabled option there is,
	// walking from the end the step came from.
	if (index < 0) {
		index = dir > 0 ? -1 : count;
	}

	for (int32_t i = index + dir; i >= 0 && i < count; i += dir) {
		if (_options[uint32_t(i)].enabled) {
			return setValue(_options[uint32_t(i)].id);
		}
	}
	return false;
}

Rc<MenuSource> Select::makeSource() {
	auto source = Rc<MenuSource>::create();
	for (auto &option : _options) {
		auto button = source->addButton(option.id, option.title, option.icon,
				[this, id = option.id](NotNull<MenuSourceButton>) { setValue(id); });
		button->setEnabled(option.enabled);
		// The check rides the menu's own leading column, so a list of options with icons and one
		// without them still lines up.
		button->setChecked(StringView(option.id) == StringView(_value));
	}
	return source;
}

bool Select::open() {
	if (!isEnabled() || _options.empty() || isOpen()) {
		return false;
	}

	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	auto source = makeSource();

	MenuConfig config;
	config.style = _menuStyle;
	config.stylesheet = _popupConfig.stylesheet;
	config.stylesheetCategory = _popupConfig.stylesheetCategory;
	config.stylesheetSource = _popupConfig.stylesheetSource;
	config.title = _popupConfig.title;
	config.idPrefix = _popupConfig.idPrefix.empty() ? String("select") : _popupConfig.idPrefix;
	config.flags = _popupConfig.flags;
	config.preferNative = _popupConfig.preferNative;
	config.keyboard = _popupConfig.keyboard;

	// The list is at least as wide as the control it drops out of, unless the owner pinned a width
	// of its own.
	if (config.style.minWidth < _contentSize.width) {
		config.style.minWidth = _contentSize.width;
	}
	if (config.style.maxWidth < config.style.minWidth) {
		config.style.maxWidth = config.style.minWidth;
	}

	// Where the keyboard starts. Without it a list opened with Space would begin at the top rather
	// than at what is currently chosen.
	config.highlight = _value;

	config.onClose = [this] {
		_popup = nullptr;
		removeStyleClass("open");
	};

	_popup = openMenuForNode(window, this, source, sp::move(config), MenuSide::Below);
	if (!_popup) {
		return false;
	}

	addStyleClass("open");
	return true;
}

void Select::close() {
	if (auto popup = sp::move(_popup)) {
		_popup = nullptr;
		removeStyleClass("open");
		popup->dismiss();
	}
}

void Select::setMenuStyle(const MenuStyle &style) { _menuStyle = style; }

void Select::setPopupConfig(MenuConfig &&config) { _popupConfig = sp::move(config); }

void Select::focus() {
	if (_focused || !isEnabled()) {
		return;
	}
	_focused = true;
	_focusApplied = true;
	if (_focusListener) {
		_focusListener->setEnabled(true);
	}
	updateInteractiveState();
}

void Select::blur() {
	if (!_focused) {
		return;
	}
	_focused = false;
	_focusApplied = false;
	if (_focusListener) {
		_focusListener->setEnabled(false);
	}
	updateInteractiveState();
}

bool Select::handleTap() {
	if (!isEnabled()) {
		return false;
	}
	focus();
	if (isOpen()) {
		close();
	} else {
		open();
	}
	return true;
}

bool Select::handleKey(const GestureData &data) {
	if (!_focused || !isEnabled() || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// While the list is up its own MenuSystem answers the keyboard, in its own window. Anything
	// this node did here would be a second reader of the same key.
	if (isOpen()) {
		return false;
	}

	const bool alt = hasFlag(ev.input.modifiers, InputModifier::Alt);

	switch (ev.key.keycode) {
	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER:
	case InputKeyCode::SPACE: return open();

	// Alt+Down is "show me the list" everywhere else, and it costs nothing to honour here.
	case InputKeyCode::DOWN: return alt ? open() : step(1);
	case InputKeyCode::UP: return step(-1);

	case InputKeyCode::HOME:
		for (auto &option : _options) {
			if (option.enabled) {
				return setValue(option.id);
			}
		}
		return false;

	case InputKeyCode::END:
		for (uint32_t i = uint32_t(_options.size()); i > 0; --i) {
			if (_options[i - 1].enabled) {
				return setValue(_options[i - 1].id);
			}
		}
		return false;

	default: break;
	}
	return false;
}

void Select::updateContent() {
	auto option = getSelectedOption();

	if (_label) {
		_label->setString(option ? StringView(option->title) : StringView(_placeholder));
	}

	if (_icon) {
		const auto icon = option ? option->icon : IconName::None;
		_icon->setIconName(icon);
		_icon->setVisible(icon != IconName::None);
	}

	_contentSizeDirty = true;
}

void Select::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counters are cumulative, so each flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		const bool focus = _focusApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

AppWindow *Select::getAppWindow() const {
	auto scene = getScene();
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

} // namespace stappler::xenolith::ui
