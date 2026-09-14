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

#include "XLUiColorPicker.h"
#include "XLUiLayoutSystem.h"
#include "XLUiControlLock.h" // applyControlInvalid
#include "XLInputListener.h"
#include "XLInheritedStyle.h" // inherited text colour
#include "XLClipboard.h"
#include "XLDirector.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Surface metrics in points, set in code so the picker is usable without a stylesheet.
static constexpr float s_pickerPadding = 12.0f;
static constexpr float s_pickerGap = 8.0f;
static constexpr float s_pickerRowHeight = 28.0f;
static constexpr float s_pickerPreviewWidth = 44.0f;
static constexpr float s_pickerSwatch = 24.0f;
static constexpr float s_pickerSwatchGap = 6.0f;
static constexpr uint32_t s_pickerColumns = 8;
static constexpr float s_pickerTabHeight = 26.0f;
static constexpr float s_pickerTabGap = 4.0f;
static constexpr float s_pickerBarHeight = 20.0f;
static constexpr float s_pickerThumb = 14.0f;
static constexpr float s_pickerLabelWidth = 14.0f;
static constexpr float s_pickerNumberWidth = 68.0f;
static constexpr float s_pickerButtonWidth = 56.0f;
static constexpr float s_pickerRadius = 3.0f;

// Fits the widest row: the preview, a hex line showing `#rrggbbaa`, and two clipboard buttons.
static constexpr float s_pickerMinWidth = 340.0f;

/* Quads per gradient strip. A ramp is a run of two-colour basic2d::Layer quads (SimpleGradient),
not a multi-stop LinearGradient (Sprite::setLinearGradient is unused elsewhere). The hue sweeps the
whole wheel and needs more segments to hide the joins. */
static constexpr uint32_t s_pickerSegments = 8;
static constexpr uint32_t s_pickerHueSegments = 12;

// The checkerboard under the alpha bar: two rows of cells dividing the bar height exactly.
static constexpr float s_pickerCheckerCell = s_pickerBarHeight / 2.0f;
static constexpr Color4B s_pickerCheckerLight = Color4B(0x9E, 0x9E, 0x9E, 0xFF);
static constexpr Color4B s_pickerCheckerDark = Color4B(0x61, 0x61, 0x61, 0xFF);

// The surface's own colours, for the parts that are not the edited colour.
static constexpr Color4B s_pickerTextColor = Color4B(0xE8, 0xE8, 0xE8, 0xFF);
static constexpr Color4B s_pickerControlColor = Color4B(0x2E, 0x2E, 0x36, 0xFF);
static constexpr Color4B s_pickerActiveColor = Color4B(0x3D, 0x7E, 0xCF, 0xFF);
static constexpr Color4B s_pickerOutlineColor = Color4B(0x4A, 0x4A, 0x55, 0xFF);
static constexpr Color4B s_pickerThumbColor = Color4B(0xF2, 0xF2, 0xF2, 0xFF);

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

SpanView<Color4B> getDefaultColorPalette() { return SpanView<Color4B>(s_defaultPalette); }

String formatColorHex(const Color4B &color, bool alpha) {
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

StringView getColorPickerModeName(ColorPickerMode mode) {
	switch (mode) {
	case ColorPickerMode::RGB: return StringView("rgb");
	case ColorPickerMode::HSL: return StringView("hsl");
	case ColorPickerMode::HSV: return StringView("hsv");
	}
	return StringView("rgb");
}

bool readColorPickerMode(StringView str, ColorPickerMode &out) {
	if (str == "rgb") {
		out = ColorPickerMode::RGB;
	} else if (str == "hsl") {
		out = ColorPickerMode::HSL;
	} else if (str == "hsv") {
		out = ColorPickerMode::HSV;
	} else {
		return false;
	}
	return true;
}

namespace {

// The letter beside each bar, per mode. Index is the channel.
static StringView channelLabel(ColorPickerMode mode, uint32_t index) {
	static constexpr StringView labels[3][3] = {
		{StringView("R"), StringView("G"), StringView("B")},
		{StringView("H"), StringView("S"), StringView("L")},
		{StringView("H"), StringView("S"), StringView("V")},
	};
	return labels[toInt(mode)][index];
}

// The top of a channel's scale, in display units. The bottom is always zero.
static float channelMax(ColorPickerMode mode, uint32_t index) {
	if (mode == ColorPickerMode::RGB) {
		return 255.0f;
	}
	// the hue wraps: 360 is 0
	return index == 0 ? 359.0f : 100.0f;
}

// How many quads that channel's ramp is worth - see s_pickerSegments.
static uint32_t channelSegments(ColorPickerMode mode, uint32_t index) {
	return (mode != ColorPickerMode::RGB && index == 0) ? s_pickerHueSegments : s_pickerSegments;
}

} // namespace

/* Child controls are styled in code: without a stylesheet a ui::TextInput or ui::Button is white
with black text, which does not fit the dark panel. */
static void paintPickerField(NotNull<TextInput> input) {
	input->setOrUpdateComponent<TextInputStyleComponent>([](NotNull<TextInputStyleComponent> s) {
		s->backgroundColor = s_pickerControlColor;
		s->outlineColor = s_pickerOutlineColor;
		s->outlineWidth = 1.0f;
		s->borderRadiusTopLeft = s_pickerRadius;
		s->borderRadiusTopRight = s_pickerRadius;
		s->borderRadiusBottomRight = s_pickerRadius;
		s->borderRadiusBottomLeft = s_pickerRadius;
		s->padding = Padding(0.0f, 8.0f);
		s->caretColor = s_pickerTextColor;
		s->hasCaretColor = true;
		s->selectionColor = s_pickerActiveColor;
		s->hasSelectionColor = true;
		return true;
	});
}

static void paintPickerButton(NotNull<Button> button, const Color4B &fill) {
	button->setPathColor(fill, false);
	button->setBorderRadius(s_pickerRadius);
	button->setLabelColor(Color4F(s_pickerTextColor));
}

// ---- the bar -----------------------------------------------------------------------------------

/* One channel's ui::Slider, with the colours that channel produces drawn under its track.

The strip is inset by half a handle at each end: the handle centre travels
[thumb/2, width - thumb/2], so the colour under the handle matches the value. */
class ColorPickerContent::Bar : public Slider {
public:
	virtual ~Bar() = default;

	virtual bool init() override {
		if (!Slider::init()) {
			return false;
		}

		setType("color-picker-bar");
		removeStyleClass("xl-ui-slider");
		addStyleClass("xl-ui-color-picker-bar");
		registerStyleAppliers("color-picker-bar");

		// transparent track: an unstyled ui::Panel is opaque white and would cover the strip
		setPathColor(Color4B(0, 0, 0, 0), false);

		// the fill would cover the passed half of the ramp
		if (auto fill = getFill()) {
			fill->setVisible(false);
		}

		// below the fill (1) and the handle (2)
		_strip = addChild(Rc<Node>::create(), ZOrder(0));
		_strip->setAnchorPoint(Anchor::BottomLeft);

		if (auto thumb = getThumb()) {
			thumb->setContentSize(Size2(s_pickerThumb, s_pickerThumb));
			thumb->setPathColor(s_pickerThumbColor, false);
			thumb->setBorderRadius(s_pickerThumb / 2.0f);
			// outline keeps the pale handle visible over pale ramps
			thumb->setOutline(Color4B(0x1A, 0x1A, 0x1A, 0xFF), 1.0f);
		}

		return true;
	}

	/* The ramp as N+1 sample colours with N quads between them. Stops are compared and nodes
	reused, since this runs for every bar on every edit. */
	void setStops(SpanView<Color4B> stops) {
		if (stops.size() < 2) {
			return;
		}
		if (_stops.size() == stops.size()) {
			bool same = true;
			for (size_t i = 0; i < stops.size(); ++i) {
				if (_stops[i] != stops[i]) {
					same = false;
					break;
				}
			}
			if (same) {
				return;
			}
		}

		_stops.clear();
		for (auto &it : stops) { _stops.emplace_back(it); }

		const size_t count = _stops.size() - 1;
		while (_segments.size() > count) {
			_segments.back()->removeFromParent(true);
			_segments.pop_back();
		}
		while (_segments.size() < count) {
			auto layer = _strip->addChild(Rc<basic2d::Layer>::create(), ZOrder(1));
			layer->setAnchorPoint(Anchor::BottomLeft);
			_segments.emplace_back(layer);
		}

		for (size_t i = 0; i < count; ++i) {
			// four-corner form: left colour on both left corners, right colour on both right
			_segments[i]->setGradient(
					basic2d::SimpleGradient(_stops[i], _stops[i + 1], _stops[i], _stops[i + 1]));
		}

		layoutStrip();
	}

	// A checkerboard under the ramp, so transparency is visible.
	void setCheckerVisible(bool value) {
		if (value == (_checker != nullptr)) {
			return;
		}
		if (!value) {
			_checker->removeFromParent(true);
			_checker = nullptr;
			return;
		}
		// under the ramp, showing through its alpha
		_checker = _strip->addChild(Rc<Node>::create(), ZOrder(0));
		_checker->setAnchorPoint(Anchor::BottomLeft);
		layoutStrip();
	}

	virtual void handleContentSizeDirty() override {
		Slider::handleContentSizeDirty();
		layoutStrip();
	}

protected:
	using Slider::init;

	void layoutStrip() {
		if (!_strip) {
			return;
		}

		const float inset = s_pickerThumb / 2.0f;
		const float width = sprt::max(_contentSize.width - s_pickerThumb, 0.0f);
		const float height = _contentSize.height;

		_strip->setPosition(Vec2(inset, 0.0f));
		_strip->setContentSize(Size2(width, height));

		if (_segments.empty() || width <= 0.0f || height <= 0.0f) {
			return;
		}

		const float step = width / float(_segments.size());
		for (size_t i = 0; i < _segments.size(); ++i) {
			// each quad ends where the next starts (same expression), so rounding leaves no gaps
			const float from = float(i) * step;
			const float to = float(i + 1) * step;
			_segments[i]->setPosition(Vec2(from, 0.0f));
			_segments[i]->setContentSize(Size2(to - from, height));
		}

		if (_checker) {
			layoutChecker(width, height);
		}
	}

	void layoutChecker(float width, float height) {
		const uint32_t columns = uint32_t(sprt::ceil(width / s_pickerCheckerCell));
		const uint32_t rows = uint32_t(sprt::ceil(height / s_pickerCheckerCell));
		const size_t count = size_t(columns) * size_t(rows);

		while (_checkerCells.size() > count) {
			_checkerCells.back()->removeFromParent(true);
			_checkerCells.pop_back();
		}
		while (_checkerCells.size() < count) {
			auto cell = _checker->addChild(Rc<basic2d::Layer>::create(), ZOrder(1));
			cell->setAnchorPoint(Anchor::BottomLeft);
			_checkerCells.emplace_back(cell);
		}

		_checker->setPosition(Vec2::ZERO);
		_checker->setContentSize(Size2(width, height));

		for (uint32_t row = 0; row < rows; ++row) {
			for (uint32_t column = 0; column < columns; ++column) {
				auto cell = _checkerCells[size_t(row) * columns + column];
				const float x = float(column) * s_pickerCheckerCell;
				const float y = float(row) * s_pickerCheckerCell;
				cell->setPosition(Vec2(x, y));
				// the last cell is clipped arithmetically so the board stays within the bar
				cell->setContentSize(Size2(sprt::min(s_pickerCheckerCell, width - x),
						sprt::min(s_pickerCheckerCell, height - y)));
				cell->setColor(
						Color4F(((row + column) % 2) ? s_pickerCheckerDark : s_pickerCheckerLight));
			}
		}
	}

	Node *_strip = nullptr;
	Node *_checker = nullptr;
	Vector<basic2d::Layer *> _segments;
	Vector<basic2d::Layer *> _checkerCells;
	Vector<Color4B> _stops;
};

// ---- the surface -------------------------------------------------------------------------------

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

	// the hex row, the tabs and one row per channel; the rest is conditional
	float height = s_pickerPadding * 2.0f + s_pickerRowHeight + s_pickerGap + s_pickerTabHeight
			+ float(ChannelCount) * (s_pickerGap + s_pickerRowHeight);

	if (params.alpha) {
		height += s_pickerGap + s_pickerRowHeight;
	}

	if (rows) {
		height += s_pickerGap + float(rows) * s_pickerSwatch + float(rows - 1) * s_pickerSwatchGap;
	}

	return Extent2(uint32_t(std::lround(width)), uint32_t(std::lround(height)));
}

ColorPickerContent::~ColorPickerContent() = default;

bool ColorPickerContent::init(ColorPickerParams &&params) {
	if (!Panel::init()) {
		return false;
	}

	_params = sp::move(params);
	_mode = _params.mode;
	_value = _params.value;
	if (!_params.alpha) {
		// no alpha bar, so the value is opaque
		_value.a = 255;
	}

	setType("color-picker");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-color-picker");
	registerStyleAppliers("color-picker");
	setPathColor(SurfaceColor, false);

	/* Inherited text colour for all labels (an uninherited basic2d::Label is black); a stylesheet
	that reaches the surface overwrites it. */
	setOrUpdateComponent<InheritedColorStyle>([](NotNull<InheritedColorStyle> style) {
		style->color = Color3B(s_pickerTextColor.r, s_pickerTextColor.g, s_pickerTextColor.b);
		style->defined |= InheritedColorStyle::DefinedColor;
		return true;
	});

	channelsFromValue();

	// --- the preview and the hex line ---------------------------------------------------------

	/* Two previews: the colour the picker opened on and the current one. Both are created white
	and coloured via setPathColor: the LayerRounded node colour multiplies the path colour, so a
	recoloured swatch must have a white node colour. Palette swatches never change colour and are
	built with it directly. */
	_previewOld =
			addChild(Rc<basic2d::LayerRounded>::create(Color4F::WHITE, s_pickerRadius), ZOrder(1));
	_previewOld->setName("preview-old");
	_previewOld->setPathColor(_value, true);

	_preview =
			addChild(Rc<basic2d::LayerRounded>::create(Color4F::WHITE, s_pickerRadius), ZOrder(1));
	// the current colour; the reference beside it is `preview-old`
	_preview->setName("preview");

	_hex = addChild(Rc<TextInput>::create(), ZOrder(1));
	_hex->setName("hex");
	_hex->addStyleClass("xl-ui-color-hex");
	_hex->setCaretBlink(false);
	_hex->setText(formatValue());
	_hex->setEnterCallback([this] { commitText(true); });
	paintPickerField(_hex);

	_copy = addChild(Rc<Button>::create(StringView("Copy"), [this] { copyToClipboard(); }),
			ZOrder(1));
	_copy->setName("copy");

	_paste = addChild(Rc<Button>::create(StringView("Paste"), [this] { pasteFromClipboard(); }),
			ZOrder(1));
	_paste->setName("paste");

	paintPickerButton(_copy, s_pickerControlColor);
	paintPickerButton(_paste, s_pickerControlColor);

	// --- the tabs -----------------------------------------------------------------------------

	// display titles, independent of the mode ids
	static constexpr StringView tabTitles[3] = {StringView("RGB"), StringView("HSL"),
		StringView("HSV")};

	for (uint32_t i = 0; i < 3; ++i) {
		const auto mode = ColorPickerMode(i);
		auto button = addChild(Rc<Button>::create(tabTitles[i], [this, mode] { setMode(mode); }),
				ZOrder(2));
		button->setType("color-picker-tab");
		button->setName(mem_std::toString("tab-", getColorPickerModeName(mode)));
		// the fill is set by updateTabs to mark the active tab
		paintPickerButton(button, s_pickerControlColor);
		_tabs[i] = button;
	}

	// --- the channel rows ------------------------------------------------------------------------

	for (uint32_t i = 0; i < ChannelCount; ++i) {
		auto label = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
		label->setName(mem_std::toString("label-", i));
		label->setColor(Color4F(s_pickerTextColor));
		_labels[i] = label;

		auto bar = addChild(Rc<Bar>::create(), ZOrder(2));
		bar->setName(mem_std::toString("bar-", i));
		bar->setCallback([this, i](int64_t index) { setChannel(i, float(index)); });
		_bars[i] = bar;

		auto input = addChild(Rc<NumberField>::create(), ZOrder(2));
		input->setName(mem_std::toString("channel-", i));
		input->setCaretBlink(false);
		input->setInteger(true);
		input->setStep(1.0);
		input->setValueCallback([this, i](double value) { setChannel(i, float(value)); });
		paintPickerField(input);
		_inputs[i] = input;
	}

	// --- the alpha row ---------------------------------------------------------------------------

	if (_params.alpha) {
		auto label = addChild(Rc<basic2d::Label>::create(), ZOrder(3));
		label->setName("label-alpha");
		label->setColor(Color4F(s_pickerTextColor));
		label->setString("A");
		_alphaLabel = label;

		_alphaBar = addChild(Rc<Bar>::create(), ZOrder(3));
		_alphaBar->setName("alpha-bar");
		_alphaBar->setCheckerVisible(true);
		_alphaBar->setRange(0.0, 255.0, 1.0);
		_alphaBar->setInteger(true);
		_alphaBar->setCallback([this](int64_t index) { setAlpha(float(index)); });

		_alphaInput = addChild(Rc<NumberField>::create(), ZOrder(3));
		_alphaInput->setName("alpha-value");
		_alphaInput->setCaretBlink(false);
		_alphaInput->setInteger(true);
		_alphaInput->setStep(1.0);
		_alphaInput->setRange(0.0, 255.0);
		_alphaInput->setValueCallback([this](double value) { setAlpha(float(value)); });
		paintPickerField(_alphaInput);
	}

	// --- the palette -----------------------------------------------------------------------------

	uint32_t index = 0;
	for (auto &it : _params.palette) {
		auto swatch =
				addChild(Rc<basic2d::LayerRounded>::create(Color4F(it), s_pickerRadius), ZOrder(4));
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

	/* Escape must be bound as the `back` hotkey: the hotkey pass consumes it before key
	recognizers. Gated on onClose rather than focus, since a popup may not have the keyboard yet. */
	_listener->addHotkey(EngineHotkeys::get().back, [this](HotkeyId, const InputEvent &) {
		if (_params.onClose) {
			_params.onClose();
			return true;
		}
		return false;
	}, HotkeyFlags::None);

	_listener->setTouchFilter(
			[](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		// key events carry the pointer location; accept them regardless of the pointer
		if (event.data.isKeyEvent()) {
			return true;
		}
		return cb(event);
	});

	updateTabs();
	updateContent();
	updateGradients();

	return true;
}

void ColorPickerContent::handleEnter(Scene *scene) {
	Panel::handleEnter(scene);

	// the caret starts in the hex line
	if (_hex) {
		_hex->focus();
	}
}

void ColorPickerContent::handleExit() {
	// cancel a pending read: its callback captures `this`
	if (_clipboard) {
		_clipboard->cancel();
	}

	Panel::handleExit();
}

// ---- the model ---------------------------------------------------------------------------------

Color4B ColorPickerContent::colorFromChannels() const {
	const uint8_t a = _value.a;

	switch (_mode) {
	case ColorPickerMode::RGB:
		return Color4B(uint8_t(sprt::clamp(_channels[0], 0.0f, 255.0f)),
				uint8_t(sprt::clamp(_channels[1], 0.0f, 255.0f)),
				uint8_t(sprt::clamp(_channels[2], 0.0f, 255.0f)), a);
	case ColorPickerMode::HSL: {
		auto rgb = sprt::geom::hslToRgb(_channels[0], _channels[1] / 100.0f, _channels[2] / 100.0f);
		return Color4B(rgb.r, rgb.g, rgb.b, a);
	}
	case ColorPickerMode::HSV: {
		auto rgb = sprt::geom::hsvToRgb(_channels[0], _channels[1] / 100.0f, _channels[2] / 100.0f);
		return Color4B(rgb.r, rgb.g, rgb.b, a);
	}
	}
	return _value;
}

void ColorPickerContent::channelsFromValue() {
	const Color3B rgb(_value.r, _value.g, _value.b);

	switch (_mode) {
	case ColorPickerMode::RGB:
		_channels[0] = float(_value.r);
		_channels[1] = float(_value.g);
		_channels[2] = float(_value.b);
		return;
	case ColorPickerMode::HSL: {
		float h, s, l;
		sprt::geom::rgbToHsl(rgb, h, s, l);
		// a grey reports hue 0; keep the last hue instead
		_channels[0] = (s > 0.0f) ? h : _hue;
		_channels[1] = s * 100.0f;
		_channels[2] = l * 100.0f;
		break;
	}
	case ColorPickerMode::HSV: {
		float h, s, v;
		sprt::geom::rgbToHsv(rgb, h, s, v);
		_channels[0] = (s > 0.0f) ? h : _hue;
		_channels[1] = s * 100.0f;
		_channels[2] = v * 100.0f;
		break;
	}
	}

	_hue = _channels[0];
}

void ColorPickerContent::applyChannels(bool silent) {
	if (_mode != ColorPickerMode::RGB) {
		// the hue bar value is kept even when the colour has no hue
		_hue = _channels[0];
	}

	const auto color = colorFromChannels();
	if (color == _value) {
		// adjacent channel steps can round to the same colour: update the controls, report nothing
		updateContent();
		return;
	}

	_value = color;
	setInvalid(false);
	updateContent();
	updateGradients();

	if (!silent && _params.onChange) {
		_params.onChange(_value);
	}
}

void ColorPickerContent::setValue(const Color4B &value, bool silent) {
	auto color = value;
	if (!_params.alpha) {
		color.a = 255;
	}

	if (color == _value) {
		// still refresh the hex line, which may show a refused edit
		updateContent();
		return;
	}

	_value = color;
	setInvalid(false);

	// an assignment rebuilds the channels; an edit does not
	channelsFromValue();

	updateContent();
	updateGradients();

	if (!silent && _params.onChange) {
		_params.onChange(_value);
	}
}

bool ColorPickerContent::setValueFromString(StringView str, bool silent) {
	Color4B color;
	if (!sprt::geom::readColor(str, color)) {
		return false;
	}
	if (!_params.alpha) {
		color.a = 255;
	}
	setValue(color, silent);
	return true;
}

String ColorPickerContent::formatValue() const { return formatColorHex(_value, _params.alpha); }

void ColorPickerContent::setMode(ColorPickerMode mode) {
	if (mode == _mode) {
		return;
	}

	_mode = mode;

	// the colour is unchanged, so nothing is reported
	channelsFromValue();

	updateTabs();
	updateContent();
	updateGradients();

	if (_params.onMode) {
		_params.onMode(_mode);
	}
}

bool ColorPickerContent::setChannel(uint32_t index, float value, bool silent) {
	if (index >= ChannelCount || _inUpdate) {
		return false;
	}

	const float max = channelMax(_mode, index);
	value = sprt::clamp(value, 0.0f, max);
	if (value == _channels[index]) {
		return false;
	}

	_channels[index] = value;
	applyChannels(silent);
	return true;
}

void ColorPickerContent::setAlpha(float value, bool silent) {
	if (_inUpdate) {
		return;
	}

	const uint8_t a = uint8_t(sprt::clamp(value, 0.0f, 255.0f));
	if (a == _value.a) {
		return;
	}

	_value.a = a;
	updateContent();

	// the alpha changes the other bars' gradients but not the channels
	updateGradients();

	if (!silent && _params.onChange) {
		_params.onChange(_value);
	}
}

bool ColorPickerContent::commitText(bool fromEnter) {
	if (!_hex) {
		return false;
	}

	Color4B color;
	if (!sprt::geom::readColor(_hex->getText(), color)) {
		// mark the refusal and keep the surface open with the text
		setInvalid(true);
		return false;
	}

	if (!_params.alpha) {
		color.a = 255;
	}

	setInvalid(false);
	setValue(color);

	// Enter picks and closes the surface; a paste does not.
	if (fromEnter && _params.onPick) {
		_params.onPick(_value);
	}
	return true;
}

void ColorPickerContent::setInvalid(bool value) {
	_valid = !value;
	if (_hex) {
		// the same state ui::FormSystem marks a rejected field with
		applyControlInvalid(_hex, value);
	}
}

// ---- what is on screen -------------------------------------------------------------------------

void ColorPickerContent::updateTabs() {
	for (uint32_t i = 0; i < 3; ++i) {
		if (!_tabs[i]) {
			continue;
		}
		const bool active = (ColorPickerMode(i) == _mode);
		if (active) {
			_tabs[i]->addStyleClass("active");
		} else {
			_tabs[i]->removeStyleClass("active");
		}
		_tabs[i]->setPathColor(active ? s_pickerActiveColor : s_pickerControlColor, false);
	}

	for (uint32_t i = 0; i < ChannelCount; ++i) {
		if (_labels[i]) {
			_labels[i]->setString(channelLabel(_mode, i));
		}
	}
}

void ColorPickerContent::updateContent() {
	// guard: the widgets written below report their changes back
	_inUpdate = true;

	if (_preview) {
		// with alpha: the preview shows the value as it is
		_preview->setPathColor(_value, true);
	}

	for (uint32_t i = 0; i < ChannelCount; ++i) {
		const float max = channelMax(_mode, i);
		if (_bars[i]) {
			_bars[i]->setRange(0.0, double(max), 1.0);
			_bars[i]->setInteger(true);
			_bars[i]->setValue(double(_channels[i]), true);
		}
		if (_inputs[i]) {
			_inputs[i]->setRange(0.0, double(max));
			_inputs[i]->setValue(double(_channels[i]), true);
		}
	}

	if (_alphaBar) {
		_alphaBar->setValue(double(_value.a), true);
	}
	if (_alphaInput) {
		_alphaInput->setValue(double(_value.a), true);
	}

	if (_hex) {
		_hex->setText(formatValue());
	}

	_inUpdate = false;
}

void ColorPickerContent::updateGradients() {
	Vector<Color4B> stops;

	// Sampled by evaluating the colour model, not interpolating endpoints: saturation and
	// lightness are not linear in RGB.
	auto sample = [&](uint32_t channel, uint32_t segments) {
		stops.clear();
		const float max = channelMax(_mode, channel);
		const float saved = _channels[channel];
		for (uint32_t i = 0; i <= segments; ++i) {
			_channels[channel] = max * float(i) / float(segments);
			auto color = colorFromChannels();
			// channel ramps are opaque; only the alpha bar shows transparency
			color.a = 255;
			stops.emplace_back(color);
		}
		_channels[channel] = saved;
	};

	for (uint32_t i = 0; i < ChannelCount; ++i) {
		if (!_bars[i]) {
			continue;
		}
		sample(i, channelSegments(_mode, i));
		_bars[i]->setStops(stops);
	}

	if (_alphaBar) {
		// two stops suffice: alpha is linear
		Color4B clear = _value;
		clear.a = 0;
		Color4B solid = _value;
		solid.a = 255;
		Color4B alphaStops[2] = {clear, solid};
		_alphaBar->setStops(SpanView<Color4B>(alphaStops, 2));
	}
}

// ---- the clipboard -----------------------------------------------------------------------------

ClipboardSession *ColorPickerContent::acquireClipboard() {
	if (!_clipboard && _director) {
		_clipboard = Rc<ClipboardSession>::create(_director->getApplication());
	}
	return _clipboard;
}

bool ColorPickerContent::copyToClipboard() {
	auto clipboard = acquireClipboard();
	if (!clipboard) {
		return false;
	}
	return clipboard->writeText(formatValue()) == Status::Ok;
}

bool ColorPickerContent::pasteFromClipboard() {
	auto clipboard = acquireClipboard();
	if (!clipboard) {
		return false;
	}

	// `this` is safe: ClipboardSession retains its target, and handleExit cancels the read
	return clipboard->readText([this](const ClipboardSession::Result &result) {
		if (!result.ok()) {
			// nothing to paste; the hex line is the only place to show it
			setInvalid(true);
			return;
		}

		if (!setValueFromString(result.text())) {
			setInvalid(true);
		}
	}, this) != 0;
}

// ---- geometry ----------------------------------------------------------------------------------

bool ColorPickerContent::handleTap(const Vec2 &location) {
	for (uint32_t i = 0; i < uint32_t(_swatches.size()); ++i) {
		auto &node = _swatches[i];
		const auto pos = node->getPosition();
		const auto size = node->getContentSize();
		// Anchored top-left, so the rect runs down from the position.
		const Rect rect(pos.x, pos.y - size.height, size.width, size.height);
		if (rect.containsPoint(location)) {
			auto color = _params.palette[i];
			if (!_params.alpha) {
				color.a = 255;
			} else {
				// a swatch keeps the current alpha
				color.a = _value.a;
			}

			// a swatch is a pick: assign silently, report only through onPick
			setValue(color, true);
			if (_params.onPick) {
				_params.onPick(_value);
			}
			return true;
		}
	}
	return false;
}

void ColorPickerContent::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	const float width = _contentSize.width;
	const float height = _contentSize.height;
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	// a LayoutSystem owns the children's geometry when present
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float left = s_pickerPadding;
	const float right = width - s_pickerPadding;
	float top = height - s_pickerPadding;

	auto place = [](Node *node, const Vec2 &pos, const Size2 &size) {
		if (!node) {
			return;
		}
		node->setAnchorPoint(Anchor::TopLeft);
		node->setPosition(pos);
		node->setContentSize(size);
	};

	// --- the preview, the hex line and the two clipboard buttons --------------------------------

	{
		// the two previews share one slot: the original colour, then the current one
		const float half = s_pickerPreviewWidth / 2.0f;
		place(_previewOld, Vec2(left, top), Size2(half, s_pickerRowHeight));
		place(_preview, Vec2(left + half, top), Size2(half, s_pickerRowHeight));

		float x = right;
		if (_paste) {
			x -= s_pickerButtonWidth;
			place(_paste, Vec2(x, top), Size2(s_pickerButtonWidth, s_pickerRowHeight));
			x -= s_pickerTabGap;
		}
		if (_copy) {
			x -= s_pickerButtonWidth;
			place(_copy, Vec2(x, top), Size2(s_pickerButtonWidth, s_pickerRowHeight));
			x -= s_pickerGap;
		}

		const float hexLeft = left + s_pickerPreviewWidth + s_pickerGap;
		place(_hex, Vec2(hexLeft, top), Size2(sprt::max(x - hexLeft, 0.0f), s_pickerRowHeight));
	}

	top -= s_pickerRowHeight + s_pickerGap;

	// --- the tabs -------------------------------------------------------------------------------

	{
		const float total = right - left;
		const float tab = (total - s_pickerTabGap * 2.0f) / 3.0f;
		for (uint32_t i = 0; i < 3; ++i) {
			place(_tabs[i], Vec2(left + float(i) * (tab + s_pickerTabGap), top),
					Size2(tab, s_pickerTabHeight));
		}
	}

	top -= s_pickerTabHeight + s_pickerGap;

	// --- one row per channel, then the alpha ----------------------------------------------------

	// the bar takes what is left after the letter and the box, so rows line up
	const float barLeft = left + s_pickerLabelWidth + s_pickerGap;
	const float barWidth = sprt::max(right - s_pickerNumberWidth - s_pickerGap - barLeft, 0.0f);
	const float numberLeft = right - s_pickerNumberWidth;

	auto placeRow = [&](Node *label, Node *bar, Node *input) {
		// the bar is thinner than the row and centred vertically
		const float barTop = top - (s_pickerRowHeight - s_pickerBarHeight) / 2.0f;
		place(label, Vec2(left, top), Size2(s_pickerLabelWidth, s_pickerRowHeight));
		place(bar, Vec2(barLeft, barTop), Size2(barWidth, s_pickerBarHeight));
		place(input, Vec2(numberLeft, top), Size2(s_pickerNumberWidth, s_pickerRowHeight));
		top -= s_pickerRowHeight + s_pickerGap;
	};

	for (uint32_t i = 0; i < ChannelCount; ++i) { placeRow(_labels[i], _bars[i], _inputs[i]); }

	if (_alphaBar) {
		placeRow(_alphaLabel, _alphaBar, _alphaInput);
	}

	// --- the swatch grid -------------------------------------------------------------------------

	for (uint32_t i = 0; i < uint32_t(_swatches.size()); ++i) {
		const uint32_t column = i % s_pickerColumns;
		const uint32_t row = i / s_pickerColumns;
		place(_swatches[i],
				Vec2(left + float(column) * (s_pickerSwatch + s_pickerSwatchGap),
						top - float(row) * (s_pickerSwatch + s_pickerSwatchGap)),
				Size2(s_pickerSwatch, s_pickerSwatch));
	}
}

// ---- accessors ---------------------------------------------------------------------------------

Slider *ColorPickerContent::getChannelBar(uint32_t index) const {
	return index < ChannelCount ? _bars[index] : nullptr;
}

NumberField *ColorPickerContent::getChannelInput(uint32_t index) const {
	return index < ChannelCount ? _inputs[index] : nullptr;
}

Slider *ColorPickerContent::getAlphaBar() const { return _alphaBar; }

Button *ColorPickerContent::getTab(ColorPickerMode mode) const { return _tabs[toInt(mode)]; }

} // namespace stappler::xenolith::ui
