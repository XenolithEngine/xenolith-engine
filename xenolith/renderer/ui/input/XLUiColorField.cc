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
#include "XLUiMenuPopup.h" // placementForNode: the arithmetic every popup needs and only this has
#include "XLUiLayoutSystem.h"
#include "XLInteractiveComponent.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr IconName s_colorFieldIcon = IconName::Image_colorize_outline;

// The closed control's fallback placement. Only the fallback: a styled field gets its LayoutSystem
// from `display:flex` and none of it runs.
static constexpr float s_colorPadding = 8.0f;
static constexpr float s_colorGap = 8.0f;
static constexpr float s_colorSwatchWidth = 32.0f;
static constexpr float s_colorSwatchRadius = 3.0f;

/* The built-in picker's metrics, in points, and DELIBERATELY not stylesheet-driven - the same
choice ui::TooltipSystem's stock hint makes. This surface exists for the platforms where the system
colour dialog does not, and it may well come up in an application that never named a stylesheet for
it; a picker that needs a sheet to be usable is not a fallback. */
static constexpr float s_pickerPadding = 12.0f;
static constexpr float s_pickerRowHeight = 28.0f;
static constexpr float s_pickerGap = 8.0f;
static constexpr float s_pickerPreviewWidth = 44.0f;
static constexpr float s_pickerSwatch = 24.0f;
static constexpr float s_pickerSwatchGap = 6.0f;
static constexpr uint32_t s_pickerColumns = 8;
static constexpr float s_pickerMinWidth = 240.0f;
static constexpr Color4B s_pickerSurfaceColor = Color4B(0x20, 0x20, 0x26, 0xFF);

/* What a picker with nothing declared offers. Sixteen colours, because the grid is eight wide and
two rows of it are enough to be useful without pretending to be a palette an application designed.
An application that has a theme passes its own through setPalette. */
static constexpr Color4B s_defaultPalette[] = {
	Color4B(0x00, 0x00, 0x00, 0xFF),
	Color4B(0x44, 0x44, 0x44, 0xFF),
	Color4B(0x88, 0x88, 0x88, 0xFF),
	Color4B(0xCC, 0xCC, 0xCC, 0xFF),
	Color4B(0xFF, 0xFF, 0xFF, 0xFF),
	Color4B(0xE5, 0x39, 0x35, 0xFF),
	Color4B(0xFB, 0x8C, 0x00, 0xFF),
	Color4B(0xFD, 0xD8, 0x35, 0xFF),
	Color4B(0x43, 0xA0, 0x47, 0xFF),
	Color4B(0x00, 0x89, 0x7B, 0xFF),
	Color4B(0x1E, 0x88, 0xE5, 0xFF),
	Color4B(0x39, 0x49, 0xAB, 0xFF),
	Color4B(0x8E, 0x24, 0xAA, 0xFF),
	Color4B(0xD8, 0x1B, 0x60, 0xFF),
	Color4B(0x6D, 0x4C, 0x41, 0xFF),
	Color4B(0x54, 0x6E, 0x7A, 0xFF),
};

// ---- the built-in picker's surface -------------------------------------------------------------

namespace {

struct ColorPickerParams {
	Color4B value = Color4B::WHITE;
	bool alpha = false;
	Vector<Color4B> palette;

	// A colour was chosen. The receiver closes the surface FIRST - see the comment at the call.
	Function<void(const Color4B &)> onPick;

	// The surface asked to go away. Also what Escape calls, and what "there is somewhere to close
	// to" is judged by.
	Function<void()> onClose;
};

// The surface: a hex line beside a preview, over a grid of swatches.
class ColorPickerContent : public Panel {
public:
	static Extent2 measure(const ColorPickerParams &);

	virtual ~ColorPickerContent() = default;

	virtual bool init(ColorPickerParams &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

protected:
	using Panel::init;

	bool commitText();
	bool handleTap(const Vec2 &location);

	ColorPickerParams _params;

	basic2d::LayerRounded *_preview = nullptr;
	TextInput *_hex = nullptr;
	Vector<basic2d::LayerRounded *> _swatches;
	InputListener *_listener = nullptr;
};

Extent2 ColorPickerContent::measure(const ColorPickerParams &params) {
	const auto count = uint32_t(params.palette.size());
	const uint32_t rows = count ? (count + s_pickerColumns - 1) / s_pickerColumns : 0;

	float width = s_pickerMinWidth;
	if (count) {
		const uint32_t columns = sprt::min(count, s_pickerColumns);
		width = sprt::max(width,
				s_pickerPadding * 2.0f + float(columns) * s_pickerSwatch
						+ float(columns - 1) * s_pickerSwatchGap);
	}

	float height = s_pickerPadding * 2.0f + s_pickerRowHeight;
	if (rows) {
		height += s_pickerGap + float(rows) * s_pickerSwatch + float(rows - 1) * s_pickerSwatchGap;
	}

	return Extent2(uint32_t(std::lround(width)), uint32_t(std::lround(height)));
}

bool ColorPickerContent::init(ColorPickerParams &&params) {
	if (!Panel::init()) {
		return false;
	}

	_params = sp::move(params);

	setType("color-picker");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-color-picker");
	registerStyleAppliers("color-picker");

	_preview =
			addChild(Rc<basic2d::LayerRounded>::create(Color4F(_params.value), s_colorSwatchRadius),
					ZOrder(1));
	_preview->setName("preview");

	_hex = addChild(Rc<TextInput>::create(), ZOrder(1));
	_hex->setName("hex");
	_hex->addStyleClass("xl-ui-color-hex");
	_hex->setCaretBlink(false);
	_hex->setText(ColorField::formatColor(_params.value, _params.alpha));
	_hex->setEnterCallback([this] { commitText(); });

	uint32_t index = 0;
	for (auto &it : _params.palette) {
		auto swatch = addChild(Rc<basic2d::LayerRounded>::create(Color4F(it), s_colorSwatchRadius),
				ZOrder(1));
		swatch->setName(mem_std::toString("swatch-", index));
		_swatches.emplace_back(swatch);
		++index;
	}

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			return handleTap(convertToNodeSpace(tap.location()));
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	/* Escape is a HOTKEY, not a key: the engine registers it as `back` and the hotkey pass consumes
	it before any key recognizer runs, so bound as a raw keycode it would simply never arrive. The
	same trap ui::SearchPickerContent names.

	Gated on having somewhere to close TO rather than on focus: a surface in a popup may not have
	been given the keyboard yet when the user hits Escape. */
	_listener->addHotkey(EngineHotkeys::get().back, [this](HotkeyId, const InputEvent &) {
		if (_params.onClose) {
			_params.onClose();
			return true;
		}
		return false;
	}, HotkeyFlags::None);

	_listener->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		// A key event carries a pointer location, so the default filter would answer only while the
		// mouse happens to be over the surface.
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});

	return true;
}

void ColorPickerContent::handleEnter(Scene *scene) {
	Panel::handleEnter(scene);

	// The caret starts in the hex line: it is the only part of this surface a keyboard can reach.
	if (_hex) {
		_hex->focus();
	}
}

void ColorPickerContent::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	const float width = _contentSize.width;
	const float height = _contentSize.height;
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	const float top = height - s_pickerPadding;

	if (_preview) {
		_preview->setAnchorPoint(Anchor::TopLeft);
		_preview->setPosition(Vec2(s_pickerPadding, top));
		_preview->setContentSize(Size2(s_pickerPreviewWidth, s_pickerRowHeight));
	}

	if (_hex) {
		const float left = s_pickerPadding + s_pickerPreviewWidth + s_pickerGap;
		_hex->setAnchorPoint(Anchor::TopLeft);
		_hex->setPosition(Vec2(left, top));
		_hex->setContentSize(
				Size2(sprt::max(width - left - s_pickerPadding, 0.0f), s_pickerRowHeight));
	}

	float rowTop = top - s_pickerRowHeight - s_pickerGap;
	for (uint32_t i = 0; i < uint32_t(_swatches.size()); ++i) {
		const uint32_t column = i % s_pickerColumns;
		const uint32_t row = i / s_pickerColumns;
		_swatches[i]->setAnchorPoint(Anchor::TopLeft);
		_swatches[i]->setPosition(
				Vec2(s_pickerPadding + float(column) * (s_pickerSwatch + s_pickerSwatchGap),
						rowTop - float(row) * (s_pickerSwatch + s_pickerSwatchGap)));
		_swatches[i]->setContentSize(Size2(s_pickerSwatch, s_pickerSwatch));
	}
}

bool ColorPickerContent::commitText() {
	if (!_hex) {
		return false;
	}

	Color4B color;
	if (!sprt::geom::readColor(_hex->getText(), color)) {
		// The refusal is marked HERE and the surface stays: the text is what the user is still
		// working on, and taking it away would be answering a mistake by hiding it.
		applyControlInvalid(_hex, true);
		return false;
	}

	applyControlInvalid(_hex, false);
	if (_params.onPick) {
		_params.onPick(color);
	}
	return true;
}

bool ColorPickerContent::handleTap(const Vec2 &location) {
	for (uint32_t i = 0; i < uint32_t(_swatches.size()); ++i) {
		auto &node = _swatches[i];
		const auto pos = node->getPosition();
		const auto size = node->getContentSize();
		// Anchored top-left, so the rect runs down from the position.
		const Rect rect(pos.x, pos.y - size.height, size.width, size.height);
		if (rect.containsPoint(location)) {
			if (_params.onPick) {
				_params.onPick(_params.palette[i]);
			}
			return true;
		}
	}
	return false;
}

} // namespace

// ---- the hex line --------------------------------------------------------------------------

/* A ui::TextInput that reports the focus EDGE.

TextInput::focus() only ASKS the platform; `_focused` follows what the platform granted, and
updateInteractiveState is the one method called on that flip. The owner needs the edge twice over:
a blur is where the text has to agree with the value again, and a focus is what tells a form that
this field now holds the keyboard. */
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

	/* blur() does not always echo: TextInput::blur cancels the handler, and a cancel is not an
	edit. So the two ways an edit can end are hooked separately - this one, and the echo below for
	when the platform takes input away with nobody calling blur() at all. ui::NumberField splits it
	the same way. */
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
			// AFTER the base call: the echoed state is stored in there, and anything written back
			// before it lands is overwritten by it.
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

	/* The InteractiveComponent has to EXIST from the first line, not from the first call that
	changes something: a node without one reads as state 0, so `:disabled` would match an untouched
	widget - and anything this init() builds from isEnabled() would be built disabled. */
	applyControlEnabled(this, true);

	setType("color-field");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-color-field");
	registerStyleAppliers("color-field");

	for (auto &it : s_defaultPalette) { _palette.emplace_back(it); }

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

	/* Priority 1, above the hex line's own listener, with a filter that takes only what is NOT over
	the text: a tap on the swatch or the icon opens the picker, a tap on the text puts the caret in
	it. Two targets in one control, and the split is by where the tap landed rather than by which
	listener happened to be asked first. */
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
	// The surface hangs off a window this node is leaving, and a dialog outlives the widget that
	// asked for it unless it is cancelled - a picker over a control that is gone is one the user
	// has to dismiss by hand.
	close();
	Panel::handleExit();
}

void ColorField::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// A LayoutSystem - from `display:flex` or added by hand - owns the children's geometry, and the
	// placement below would be a second writer of the same positions. Same rule as ui::Select's.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	float left = s_colorPadding;
	if (_swatch) {
		_swatch->setAnchorPoint(Anchor::MiddleLeft);
		_swatch->setPosition(Vec2(left, height / 2.0f));
		_swatch->setContentSize(Size2(s_colorSwatchWidth, sprt::max(height - 12.0f, 0.0f)));
		left += s_colorSwatchWidth + s_colorGap;
	}

	float right = width - s_colorPadding;
	if (_icon) {
		_icon->setAnchorPoint(Anchor::MiddleRight);
		_icon->setPosition(Vec2(right, height / 2.0f));
		right -= _icon->getContentSize().width + s_colorGap;
	}

	if (_input) {
		_input->setAnchorPoint(Anchor::MiddleLeft);
		_input->setPosition(Vec2(left, height / 2.0f));
		_input->setContentSize(Size2(sprt::max(right - left, 0.0f), height));
	}
}

void ColorField::setValue(const Color4B &value, bool silent) {
	if (_value == value) {
		// Still refresh the text: a refused edit left the line showing something else, and this is
		// how "assign what it already holds" puts it back.
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
		// The field does not carry alpha, so it does not silently acquire one from a string that
		// happened to have it.
		color.a = 255;
	}
	setValue(color, silent);
	return true;
}

String ColorField::formatValue() const { return formatColor(_value, _alpha); }

String ColorField::formatColor(const Color4B &color, bool alpha) {
	// Lower case and always the long form: one spelling per colour, so that what a form collects
	// and what a file already holds can be compared as text.
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
	// The lock has the last word, and remembers what was asked for so unlocking can give it
	// back. A no-op, and one pointer test, on a control nobody locked.
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
		// The surface was built from the previous palette; rebuilding it under the pointer would
		// move the swatch the user was about to click.
		close();
	}
}

bool ColorField::open() {
	if (!isEnabled() || isOpen() || _dialog) {
		return false;
	}

	// Each attempt answers for itself: whatever the last one could not do is not this one's verdict.
	setUnavailable(false, StringView());

	switch (_mode) {
	case PickerMode::System: return openSystemPicker();
	case PickerMode::Fallback: return openFallbackPicker();
	case PickerMode::Auto: break;
	}

	// Asked NOW, not remembered: a widget can be moved to another window, and the answer is the
	// window's.
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
			// The completion still runs, with ErrorCancelled: that is the contract, and it is what
			// clears whatever the callback owns.
			window->cancelDialog(dialog);
		}
	}
}

void ColorField::setPickerConfig(PopupSurfaceConfig &&config) { _pickerConfig = sp::move(config); }

void ColorField::setValueCallback(ColorCallback &&cb) { _valueCallback = sp::move(cb); }

void ColorField::setFocusCallback(FocusCallback &&cb) { _focusCallback = sp::move(cb); }

void ColorField::setNavigateCallback(NavigateCallback &&cb) {
	_navigateCallback = sp::move(cb);

	// Passed straight through: the row has one part, so Tab out of the hex line IS Tab out of the
	// field, and there is no inner ring to walk first.
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
			// ENTER is "take this": the refusal stays on screen, with the text that caused it.
			setInvalid(true, StringView("not a colour"));
			return false;
		}

		/* BLUR is "I am done", and a field that keeps unreadable text would say one thing on screen
		and another through getValue(). The value's own text goes back, and the mark goes with it. */
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
		// WITH the alpha: the swatch is the value, and a half-transparent colour shown opaque is a
		// swatch that lies about what it holds.
		_swatch->setPathColor(_value, true);
	}

	if (_input) {
		// The field agreeing with itself, not an edit: the guard keeps the echo of this write from
		// being read back as one.
		_inUpdate = true;
		_input->setText(formatValue());
		_inUpdate = false;
	}
}

void ColorField::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counter is cumulative, so it moves on an edge and never twice. The hex line paints
		// its own `:focus`; this one is the FIELD's.
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
		// The same state ui::FormSystem marks a rejected field with: one word for one meaning
		applyControlInvalid(this, value);
	}
}

void ColorField::setUnavailable(bool value, StringView message) {
	_unavailableMessage = message.str<Interface>();
	if (value == _unavailable) {
		return;
	}
	_unavailable = value;
	/* Its OWN class, deliberately not `invalid`. The two mean opposite remedies - `invalid` says
	"fix what you wrote", this says "there is nothing wrong with what you wrote, the way in is
	missing" - and a control that says the first when it means the second is worse than one that
	says nothing. */
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
		/* Named rather than swallowed. This is the whole reason isDialogSupported exists, and an
		application that offers a control which does nothing on this platform has to be able to find
		out why.

		Through setUnavailable, not setInvalid: nothing is wrong with the colour this field holds,
		and marking it `invalid` would say there was. */
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
			// The user pressed Cancel. An ordinary outcome, and reporting it as a failure is the
			// trap the dialog documentation names.
			return;
		}

		if (!sprt::status::isSuccessful(res.status)) {
			// The dialog failed, which says nothing about the value the field is holding.
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
		// Anything but Ok means the completion has ALREADY been scheduled with that status, so this
		// only names the refusal - it must not answer the callback itself.
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

	params.onPick = [this](const Color4B &color) {
		// Close FIRST: the value's callback is free to put something else in this surface's place,
		// and a picker still standing behind it is one the user has to dismiss by hand.
		close();
		setValue(color);
	};
	params.onClose = [this] { close(); };

	auto config = _pickerConfig;
	config.size = ColorPickerContent::measure(params);
	config.title = config.title.empty() ? String("Colour") : config.title;
	config.idPrefix = config.idPrefix.empty() ? String("color-picker") : config.idPrefix;
	config.layoutName = String("color-picker-layout");
	config.panelName = String("color-picker");
	config.fallbackColor = s_pickerSurfaceColor;

	// The surface IS the content, and it types and classes itself in init.
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
	// The platform can take input away with nobody calling blur() - Escape cancels it - so this is
	// where the text and the value have to agree again, whatever ended the edit.
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
