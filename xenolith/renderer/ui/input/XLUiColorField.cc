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

#include "XLUiColorField.h"

#include "XLInheritedStyle.h" // placeInline*: the row follows the inline direction
#include "XLUiMenuPopup.h" // placementForNode
#include "XLUiLayoutSystem.h"
#include "XLInteractiveComponent.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr IconName s_colorFieldIcon = IconName::Image_colorize_outline;

// Fallback placement metrics, used only without a LayoutSystem (no `display:flex`).
static constexpr float s_colorPadding = 8.0f;
static constexpr float s_colorGap = 8.0f;
static constexpr float s_colorSwatchWidth = 32.0f;
static constexpr float s_colorSwatchRadius = 3.0f;

// ---- the hex line --------------------------------------------------------------------------

/* A ui::TextInput that reports the focus edge to the owner. TextInput::focus() only requests
focus; `_focused` follows what the platform granted, and updateInteractiveState runs on the flip. */
class ColorField::Input : public TextInput {
public:
	virtual ~Input() = default;

	virtual bool init(NotNull<ColorField> owner) {
		if (!TextInput::init()) {
			return false;
		}
		_owner = owner;
		return true;
	}

	/* blur() does not echo (it cancels the handler), so an explicit blur is hooked here and a
	platform-side end of input through the echo below. */
	virtual void blur() override {
		TextInput::blur();
		if (_owner) {
			_owner->handleInputEcho(isFocused());
		}
	}

protected:
	using TextInput::init;

	virtual void handleTextInput(const TextInputState &state) override {
		TextInput::handleTextInput(state);
		if (_owner) {
			// after the base call, which stores the echoed state
			_owner->handleInputEcho(isFocused());
		}
	}

	virtual void updateInteractiveState() override {
		TextInput::updateInteractiveState();
		if (_reportedFocus != isFocused()) {
			_reportedFocus = isFocused();
			if (_owner) {
				_owner->handleInputFocus(_reportedFocus);
			}
		}
	}

	ColorField *_owner = nullptr;
	bool _reportedFocus = false;
};

// ---- ColorField ----------------------------------------------------------------------------

ColorField::~ColorField() { }

bool ColorField::init() {
	if (!Panel::init()) {
		return false;
	}

	/* The InteractiveComponent must exist before anything reads isEnabled(): a node without one
	reads as state 0, which matches `:disabled`. */
	applyControlEnabled(this, true);

	setType("color-field");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-color-field");
	registerStyleAppliers("color-field");

	for (auto &it : getDefaultColorPalette()) { _palette.emplace_back(it); }

	_swatch = addChild(Rc<basic2d::LayerRounded>::create(Color4F(_value), s_colorSwatchRadius),
			ZOrder(1));
	_swatch->setName("swatch");
	_swatch->setType("swatch");

	_input = addChild(Rc<Input>::create(this), ZOrder(1));
	_input->setName("hex");
	_input->addStyleClass("xl-ui-color-hex");
	_input->setEnterCallback([this] { commitText(true); });

	_icon = addChild(Rc<basic2d::IconSprite>::create(s_colorFieldIcon), ZOrder(1));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-color-icon");

	/* Priority 1, above the hex line's listener, filtered to points outside the text: a tap on the
	swatch or icon opens the picker, a tap on the text goes to the hex line. */
	_listener = addSystem(Rc<InputListener>::create());
	_listener->setPriority(1);
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			return handleTap();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});
	_listener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (_input && _input->isTouched(event.currentLocation, 0.0f)) {
			return false;
		}
		return cb(event);
	});

	updateContent();
	updateInteractiveState();


	return true;
}

void ColorField::handleExit() {
	// close the surface and cancel a system dialog, which would otherwise outlive the widget
	close();
	Panel::handleExit();
}

void ColorField::handleContentSizeDirty() { Panel::handleContentSizeDirty(); }

void ColorField::handleLayoutChildren() {
	Panel::handleLayoutChildren();
	placeInlineParts();
}

void ColorField::placeInlineParts() {
	// a LayoutSystem owns the children's geometry when present
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	/* Runs from handleLayoutChildren: the resolved direction is not yet current in
	handleContentSizeDirty. The swatch leads, the icon trails, the text line fills the rest. */
	const bool rtl = isInlineRtl(this);

	float startInset = s_colorPadding;
	if (_swatch) {
		placeInlineStart(_swatch, startInset, height / 2.0f, width, rtl);
		_swatch->setContentSize(Size2(s_colorSwatchWidth, sprt::max(height - 12.0f, 0.0f)));
		startInset += s_colorSwatchWidth + s_colorGap;
	}

	float endInset = s_colorPadding;
	if (_icon) {
		placeInlineEnd(_icon, endInset, height / 2.0f, width, rtl);
		endInset += _icon->getContentSize().width + s_colorGap;
	}

	if (_input) {
		placeInlineStart(_input, startInset, height / 2.0f, width, rtl);
		_input->setContentSize(
				Size2(sprt::max(width - startInset - endInset, 0.0f), height));
	}
}

void ColorField::setValue(const Color4B &value, bool silent) {
	if (_value == value) {
		// still refresh the text, which may show a refused edit
		updateContent();
		return;
	}

	_value = value;
	setInvalid(false, StringView());
	updateContent();

	if (!silent && _valueCallback) {
		_valueCallback(_value);
	}
}

bool ColorField::setValueFromString(StringView str, bool silent) {
	Color4B color;
	if (!sprt::geom::readColor(str, color)) {
		return false;
	}
	if (!_alpha) {
		// alpha from the string is ignored when the field has no alpha
		color.a = 255;
	}
	setValue(color, silent);
	return true;
}

String ColorField::formatValue() const { return formatColor(_value, _alpha); }

String ColorField::formatColor(const Color4B &color, bool alpha) {
	// lower case, always the long form: one spelling per colour, comparable as text
	auto digits = StringView("0123456789abcdef");
	String ret;
	ret.reserve(alpha ? 9 : 7);
	ret.push_back('#');

	auto write = [&](uint8_t v) {
		ret.push_back(digits[(v >> 4) & 0xF]);
		ret.push_back(digits[v & 0xF]);
	};

	write(color.r);
	write(color.g);
	write(color.b);
	if (alpha) {
		write(color.a);
	}
	return ret;
}

void ColorField::setAlphaEnabled(bool value) {
	if (_alpha == value) {
		return;
	}
	_alpha = value;
	if (!_alpha) {
		_value.a = 255;
	}
	updateContent();
}

void ColorField::setEnabled(bool value) {
	// the edit lock has the last word and remembers the requested value for unlocking
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	if (_input) {
		_input->setEnabled(value);
	}
	if (!value) {
		close();
	}
	updateInteractiveState();
}

void ColorField::setPickerMode(PickerMode mode) { _mode = mode; }

bool ColorField::isSystemPickerAvailable() const {
	auto window = getAppWindow();
	return window ? window->isDialogSupported(sprt::window::DialogType::Color) : false;
}

void ColorField::setPalette(SpanView<Color4B> palette) {
	_palette.clear();
	_palette.reserve(palette.size());
	for (auto &it : palette) { _palette.emplace_back(it); }

	if (isOpen()) {
		// the surface was built from the previous palette; close rather than rebuild it
		close();
	}
}

bool ColorField::open() {
	if (!isEnabled() || isOpen() || _dialog) {
		return false;
	}

	// clear the previous attempt's unavailable state
	setUnavailable(false, StringView());

	switch (_mode) {
	case PickerMode::System: return openSystemPicker();
	case PickerMode::Fallback: return openFallbackPicker();
	case PickerMode::Auto: break;
	}

	// checked on every open: the widget may have moved to another window
	return isSystemPickerAvailable() ? openSystemPicker() : openFallbackPicker();
}

void ColorField::close() {
	if (auto picker = sp::move(_picker)) {
		_picker = nullptr;
		removeStyleClass("open");
		picker->dismiss();
	}

	if (auto dialog = sp::move(_dialog)) {
		_dialog = nullptr;
		removeStyleClass("open");
		if (auto window = getAppWindow()) {
			// the completion still runs with ErrorCancelled and releases what the callback owns
			window->cancelDialog(dialog);
		}
	}
}

void ColorField::setPickerConfig(PopupSurfaceConfig &&config) { _pickerConfig = sp::move(config); }

void ColorField::setPickerColorMode(ColorPickerMode mode) {
	_pickerMode = mode;

	// an open surface follows the mode change
	auto panel = _picker ? _picker->getPanel() : nullptr;
	if (auto content = dynamic_cast<ColorPickerContent *>(panel)) {
		content->setMode(mode);
	}
}

void ColorField::setValueCallback(ColorCallback &&cb) { _valueCallback = sp::move(cb); }

void ColorField::setFocusCallback(FocusCallback &&cb) { _focusCallback = sp::move(cb); }

void ColorField::setNavigateCallback(NavigateCallback &&cb) {
	_navigateCallback = sp::move(cb);

	// Tab out of the hex line is Tab out of the field
	if (_input) {
		_input->setNavigateCallback([this](bool backwards) {
			return _navigateCallback ? _navigateCallback(backwards) : false;
		});
	}
}

TextInput *ColorField::getInput() const { return _input; }

void ColorField::focus() {
	if (_input) {
		_input->focus();
	}
}

void ColorField::blur() {
	if (_input) {
		_input->blur();
	}
}

bool ColorField::isFocused() const { return _input ? _input->isFocused() : false; }

bool ColorField::commitText(bool fromEnter) {
	if (!_input) {
		return false;
	}

	Color4B color;
	if (!sprt::geom::readColor(_input->getText(), color)) {
		if (fromEnter) {
			// on Enter the refused text stays, marked invalid
			setInvalid(true, StringView("not a colour"));
			return false;
		}

		// on blur the value's text is restored and the mark cleared
		setInvalid(false, StringView());
		updateContent();
		return false;
	}

	if (!_alpha) {
		color.a = 255;
	}

	setInvalid(false, StringView());
	setValue(color);
	return true;
}

void ColorField::updateContent() {
	if (_swatch) {
		// with alpha: the swatch shows the value as it is
		_swatch->setPathColor(_value, true);
	}

	if (_input) {
		// guarded so the echo of this write is not read back as an edit
		_inUpdate = true;
		_input->setText(formatValue());
		_inUpdate = false;
	}
}

void ColorField::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit is written by applyControlEnabled, from setEnabled.
		bool dirty = false;
		// The counter is cumulative, so it moves only on an edge. This is the field's `:focus`;
		// the hex line has its own.
		const bool focus = isFocused() && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

void ColorField::setInvalid(bool value, StringView message) {
	_valid = !value;
	_message = message.str<Interface>();

	{
		// the same state ui::FormSystem marks a rejected field with
		applyControlInvalid(this, value);
	}
}

void ColorField::setUnavailable(bool value, StringView message) {
	_unavailableMessage = message.str<Interface>();
	if (value == _unavailable) {
		return;
	}
	_unavailable = value;
	// the `unavailable` class, not `:invalid`: the value is fine, the picker route is missing
	if (value) {
		addStyleClass("unavailable");
	} else {
		removeStyleClass("unavailable");
	}
}

bool ColorField::openSystemPicker() {
	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	if (!window->isDialogSupported(sprt::window::DialogType::Color)) {
		// reported through setUnavailable, not setInvalid: the value itself is fine
		setUnavailable(true, StringView("no system colour picker on this platform"));
		return false;
	}

	auto request = Rc<sprt::window::DialogRequest>::create();
	request->type = sprt::window::DialogType::Color;
	request->title = sprt::window::String("Colour");
	request->color = Color4F(_value);
	if (_alpha) {
		request->flags |= sprt::window::DialogFlags::AlphaChannel;
	}

	// `target` keeps this node alive until the callback has run, so the capture below can be raw.
	request->target = this;
	request->callback = [this, req = request.get()](const sprt::window::DialogResult &res) {
		if (_dialog == req) {
			_dialog = nullptr;
			removeStyleClass("open");
		}

		if (res.status == Status::Declined) {
			// the user cancelled; not a failure
			return;
		}

		if (!sprt::status::isSuccessful(res.status)) {
			// the dialog failed; the value is unaffected
			setUnavailable(true, sprt::status::getStatusName(res.status));
			return;
		}

		auto color = Color4B(res.color);
		if (!_alpha) {
			color.a = 255;
		}
		setValue(color);
	};

	_dialog = request;
	addStyleClass("open");

	if (auto st = window->openDialog(request); !sprt::status::isSuccessful(st)) {
		// on failure the completion is already scheduled with that status; do not call it here
		log::source().debug("ui::ColorField",
				"openDialog refused: ", sprt::status::getStatusName(st));
		return false;
	}
	return true;
}

bool ColorField::openFallbackPicker() {
	auto window = getAppWindow();
	if (!window) {
		return false;
	}

	ColorPickerParams params;
	params.value = _value;
	params.alpha = _alpha;
	params.palette = _palette;

	params.mode = _pickerMode;

	// the field remembers the tab across opens
	params.onMode = [this](ColorPickerMode mode) { _pickerMode = mode; };

	// live update while a bar is dragged; the surface stays open
	params.onChange = [this](const Color4B &color) { setValue(color); };

	params.onPick = [this](const Color4B &color) {
		// close first: the value callback may open another surface in its place
		close();
		setValue(color);
	};
	params.onClose = [this] { close(); };

	auto config = _pickerConfig;
	// the field's stylesheet is used when the config names none
	config.styleSource = this;
	config.size = ColorPickerContent::measure(params);
	config.title = config.title.empty() ? String("Colour") : config.title;
	config.idPrefix = config.idPrefix.empty() ? String("color-picker") : config.idPrefix;
	config.layoutName = String("color-picker-layout");
	config.panelName = String("color-picker");
	config.fallbackColor = ColorPickerContent::SurfaceColor;

	// the content sets its own type and classes in init
	config.makePanel = [params = sp::move(params)](NotNull<SubWindow>,
							   Extent2) mutable -> Rc<Panel> {
		return Rc<ColorPickerContent>::create(ColorPickerParams(params));
	};

	config.onClose = [this] {
		_picker = nullptr;
		removeStyleClass("open");
	};

	_picker = openPopupSurface(window, placementForNode(this, MenuSide::Below), sp::move(config));
	if (!_picker) {
		return false;
	}

	addStyleClass("open");
	return true;
}

bool ColorField::handleTap() {
	if (!isEnabled()) {
		return false;
	}
	if (isOpen() || _dialog) {
		close();
		return true;
	}
	return open();
}

void ColorField::handleInputFocus(bool focused) {
	updateInteractiveState();

	if (_focusCallback) {
		_focusCallback(focused);
	}
}

void ColorField::handleInputEcho(bool focused) {
	// the platform can end input without blur() (Escape), so the blur commit happens here
	if (_inUpdate || focused) {
		return;
	}
	commitText(false);
}

AppWindow *ColorField::getAppWindow() const {
	auto scene = getScene();
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

} // namespace stappler::xenolith::ui
