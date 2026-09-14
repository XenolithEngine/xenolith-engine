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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICOLORPICKER_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICOLORPICKER_H_

#include "XLUiPanel.h"
#include "XLUiTextInput.h"
#include "XLUiButton.h"
#include "XLUiSlider.h"
#include "XLUiNumberField.h"
#include "XL2dLayerRounded.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The colour model the bars and boxes show; switching it keeps the same colour.
enum class ColorPickerMode : uint8_t {
	RGB,
	HSL,
	HSV,
};

// "rgb" / "hsl" / "hsv": the mode names used by commands, saved sessions and tests.
SP_PUBLIC StringView getColorPickerModeName(ColorPickerMode);
SP_PUBLIC bool readColorPickerMode(StringView, ColorPickerMode &);

// Sixteen default swatches (two rows of the eight-wide grid); override with
// ui::ColorField::setPalette.
SP_PUBLIC SpanView<Color4B> getDefaultColorPalette();

/* A colour as "#rrggbb", or "#rrggbbaa" when `alpha` is on. The inverse of sprt::geom::readColor;
shared by the clipboard, ui::ColorField (formatColor) and forms. */
SP_PUBLIC String formatColorHex(const Color4B &, bool alpha);

struct SP_PUBLIC ColorPickerParams {
	Color4B value = Color4B::WHITE;

	// Whether the alpha bar is shown and the hex carries a fourth byte.
	bool alpha = false;

	// The swatch grid; empty hides it. ui::ColorField passes getDefaultColorPalette() by default.
	Vector<Color4B> palette;

	ColorPickerMode mode = ColorPickerMode::RGB;

	/* The colour changed and the surface stays open (bar drag, box edit, hex commit). Fires
	throughout a drag, as ui::Slider does; an owner groups a gesture into one history entry. */
	Function<void(const Color4B &)> onChange;

	/* The choice is made: a swatch click or Enter in the hex line (not bars or boxes). The receiver
	closes the surface before applying the value, whose callback may open another surface. */
	Function<void(const Color4B &)> onPick;

	// The tab changed; lets ui::ColorField remember it without reading a surface being torn down.
	Function<void(ColorPickerMode)> onMode;

	// The surface asks to close; also called by Escape, which is handled only when this is set.
	Function<void()> onClose;
};

/** The colour picker's surface: a preview and a hex line, tabs over three bars, an alpha bar of its
own, and a grid of swatches.

Separate from the control that opens it (like ui::SearchPickerContent): it works inside a
ui::SubWindow or parented directly into a node, which allows testing without a window.

Bars are ui::Sliders with a gradient strip under the track; boxes are ui::NumberFields, the hex line
a ui::TextInput, tabs and clipboard buttons ui::Buttons.

The default look is set in code so the surface is usable without a stylesheet; all parts are typed
and named, so a sheet can override it (ui::openPopupSurface shares the opener's sheet).

The channels are state, not a projection of the value, so the hue survives a grey. Only an outside
assignment (swatch, committed hex, owner) rebuilds them, carrying the hue over when the new colour
has no chroma.

CSS: type `color-picker`, class `xl-ui-color-picker`. Children are `color-picker > preview`,
`> text-input#hex`, `> button`, `> color-picker-tab` (class `active` on the chosen one),
`> color-picker-bar` (a slider), `> number-field` and `> color-picker-swatch`. */
class SP_PUBLIC ColorPickerContent : public Panel {
public:
	// Channels per mode, excluding alpha.
	static constexpr uint32_t ChannelCount = 3;

	// Background without a stylesheet; ui::ColorField also passes it as
	// PopupSurfaceConfig::fallbackColor.
	static constexpr Color4B SurfaceColor = Color4B(0x20, 0x20, 0x26, 0xFF);

	/* The extent the surface needs, computed before any node exists (for the window request).
	Depends only on the palette row count and `alpha`. */
	static Extent2 measure(const ColorPickerParams &);

	virtual ~ColorPickerContent();

	virtual bool init(ColorPickerParams &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// Assigns a colour and rebuilds the channels from it. `silent` suppresses `onChange`.
	virtual void setValue(const Color4B &, bool silent = false);
	const Color4B &getValue() const { return _value; }

	// Accepts what sprt::geom::readColor reads. False leaves the value unchanged.
	virtual bool setValueFromString(StringView, bool silent = false);

	// "#rrggbb", or "#rrggbbaa" when the surface carries an alpha channel.
	String formatValue() const;

	virtual void setMode(ColorPickerMode);
	ColorPickerMode getMode() const { return _mode; }

	bool isAlphaEnabled() const { return _params.alpha; }

	/* The active mode's channels in display units, as shown on screen: 0-255 for R, G, B; 0-359
	for hue; 0-100 (percent) for saturation, lightness and value. */
	SpanView<float> getChannels() const { return SpanView<float>(_channels, ChannelCount); }
	virtual bool setChannel(uint32_t index, float value, bool silent = false);

	// 0-255. The alpha bar exists only when `alpha` is on.
	float getAlpha() const { return float(_value.a); }
	virtual void setAlpha(float, bool silent = false);

	/* Copy writes formatValue(); paste reads with sprt::geom::readColor. Paste is asynchronous: it
	returns whether the request started. An empty clipboard or non-colour text marks the hex line
	invalid. */
	virtual bool copyToClipboard();
	virtual bool pasteFromClipboard();

	// The hex line, named `hex`.
	TextInput *getHexInput() const { return _hex; }

	// One channel's bar, or null past the end.
	Slider *getChannelBar(uint32_t index) const;
	NumberField *getChannelInput(uint32_t index) const;

	// Null without alpha. Defined in the .cc, where `Bar` is complete.
	Slider *getAlphaBar() const;
	NumberField *getAlphaInput() const { return _alphaInput; }

	Button *getTab(ColorPickerMode) const;

	SpanView<Color4B> getPalette() const { return _params.palette; }

	// Whether the last hex commit was accepted; drives `:invalid` on the hex line.
	bool isValid() const { return _valid; }

protected:
	using Panel::init;

	// A ui::Slider with a gradient strip under its track. Defined in the .cc.
	class Bar;

	// Parses and accepts or refuses the hex text. On refusal, Enter keeps the text and the mark,
	// blur restores the value's text (as in ui::ColorField).
	virtual bool commitText(bool fromEnter);

	// The colour the current channels and alpha spell.
	Color4B colorFromChannels() const;

	// The channels `_value` spells in the current mode, with the hue carried across when the colour
	// has no chroma to name one of its own.
	void channelsFromValue();

	// A channel or the alpha moved: recompute the value, update the views, report.
	void applyChannels(bool silent);

	// Push `_channels` / `_value` to the bars, boxes, preview and hex line; guarded so the echo is
	// not read back as an edit.
	virtual void updateContent();

	// Repaint every bar's gradient. Separate from updateContent because it is expensive and only
	// stale when another channel moved.
	virtual void updateGradients();

	virtual void updateTabs();

	// Marks a refused hex text; only the hex line can be invalid.
	virtual void setInvalid(bool);

	bool handleTap(const Vec2 &location);

	ClipboardSession *acquireClipboard();

	ColorPickerParams _params;

	// The colour opened on, then the colour now: `preview-old` and `preview`.
	basic2d::LayerRounded *_previewOld = nullptr;
	basic2d::LayerRounded *_preview = nullptr;
	TextInput *_hex = nullptr;
	Button *_copy = nullptr;
	Button *_paste = nullptr;

	Button *_tabs[3] = {nullptr, nullptr, nullptr};

	// The channel letter beside each bar; changes with the tab.
	basic2d::Label *_labels[ChannelCount] = {nullptr, nullptr, nullptr};
	Bar *_bars[ChannelCount] = {nullptr, nullptr, nullptr};
	NumberField *_inputs[ChannelCount] = {nullptr, nullptr, nullptr};

	// Null when the surface carries no alpha channel (the row is omitted, not disabled).
	basic2d::Label *_alphaLabel = nullptr;
	Bar *_alphaBar = nullptr;
	NumberField *_alphaInput = nullptr;

	Vector<basic2d::LayerRounded *> _swatches;

	InputListener *_listener = nullptr;
	Rc<ClipboardSession> _clipboard;

	Color4B _value = Color4B::WHITE;
	ColorPickerMode _mode = ColorPickerMode::RGB;

	// The active mode's channels: editing state, not a projection of the value (keeps hue on grey).
	float _channels[ChannelCount] = {0.0f, 0.0f, 0.0f};

	// Carried across a mode switch and across a colour that has none of its own.
	float _hue = 0.0f;

	bool _valid = true;

	// Guards updateContent() against the writes it makes being read back as edits.
	bool _inUpdate = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICOLORPICKER_H_
