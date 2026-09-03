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

/* Which three numbers the bars and the boxes are showing.

Three models of one colour, not three widgets: the surface keeps ONE value and the tabs only change
what it is spelled as. That is the whole reason they are tabs rather than three sections - a person
reaching for HSL has not stopped meaning the colour that is already chosen. */
enum class ColorPickerMode : uint8_t {
	RGB,
	HSL,
	HSV,
};

// "rgb" / "hsl" / "hsv" - what a command, a saved session and a test name a mode by.
SP_PUBLIC StringView getColorPickerModeName(ColorPickerMode);
SP_PUBLIC bool readColorPickerMode(StringView, ColorPickerMode &);

/* What a picker with nothing declared offers.

Sixteen colours, because the grid is eight wide and two rows of it are enough to be useful without
pretending to be a palette an application designed. An application that has a theme passes its own
through ui::ColorField::setPalette. */
SP_PUBLIC SpanView<Color4B> getDefaultColorPalette();

/* A colour as an html tag: "#rrggbb", or "#rrggbbaa" when `alpha` is on.

The INVERSE of sprt::geom::readColor - readColor(formatColorHex(c, a)) == c for every colour - and
the only printer in the kit, so what this surface copies to the clipboard, what ui::ColorField shows
in its hex line and what a form collects are one spelling. `ui::ColorField::formatColor` is this
function under the name it had before there was a picker to share it with. */
SP_PUBLIC String formatColorHex(const Color4B &, bool alpha);

struct SP_PUBLIC ColorPickerParams {
	Color4B value = Color4B::WHITE;

	// Whether the alpha bar is there at all, and whether the hex the surface prints and collects
	// carries a fourth byte.
	bool alpha = false;

	// The swatch grid. Empty hides it; getDefaultColorPalette() is what ui::ColorField passes when
	// nothing was declared.
	Vector<Color4B> palette;

	ColorPickerMode mode = ColorPickerMode::RGB;

	/* THE COLOUR CHANGED AND THE SURFACE STAYS. A bar being dragged, a spin box being typed in, a
	hex line taken - every one of them is a person still choosing, and a picker that only reported
	on the way out would make the preview the only feedback there is.

	Fires throughout a drag, exactly as ui::Slider's callback does and for the same reason. An owner
	that wants one history entry per gesture groups them itself. */
	Function<void(const Color4B &)> onChange;

	/* THE CHOICE IS MADE: a swatch was clicked, or Enter was pressed in the hex line. The receiver
	closes the surface FIRST - the value's own callback is free to put something else in its place,
	and a picker still standing behind it is one the user has to dismiss by hand.

	Deliberately not fired for a bar or a box: dragging a slider is not a decision to stop. */
	Function<void(const Color4B &)> onPick;

	/* THE TAB CHANGED. Reported rather than read back, because the one caller that wants it -
	ui::ColorField, keeping the tab across an open and a close - would otherwise have to reach into
	the surface while it is being torn down, and by then the field has already let go of it. */
	Function<void(ColorPickerMode)> onMode;

	// The surface asked to go away. Also what Escape calls, and what "there is somewhere to close
	// to" is judged by.
	Function<void()> onClose;
};

/** The colour picker's surface: a preview and a hex line, tabs over three bars, an alpha bar of its
own, and a grid of swatches.

SEPARATE FROM THE CONTROL THAT OPENS IT, for ui::SearchPickerContent's reason: it has to work in two
places - inside a ui::SubWindow, and parented straight into a node. The second is not a convenience,
it is what lets the widget be driven and asserted with no window at all, which for a surface that
exists PRECISELY for the platforms with no colour dialog is the only place its arithmetic can be
checked.

WHAT IT IS MADE OF IS ORDINARY. The bars are ui::Sliders with a strip of gradient under the track:
the base class already carries the drag, the arrows, Home/End, the focus and the disabled state, and
a bar that reimplemented them would be a second, worse slider. The boxes are ui::NumberFields, the
hex line is a ui::TextInput, the tabs and the two clipboard buttons are ui::Buttons. Nothing here is
a new interaction.

ITS LOOK IS WRITTEN IN CODE, and that is the same choice ui::TooltipSystem's stock hint makes rather
than an oversight. This surface exists for the platforms where the system dialog does not, and a
picker that needs a stylesheet to be usable is not a fallback. Everything is still typed and named,
so a sheet says otherwise wherever there is one - including on the native path, where
ui::openPopupSurface shares the opener's sheet with the popup's own scene.

THE HUE SURVIVES A GREY, and that is why the channels are state rather than a projection. Drag the
saturation to nothing and the colour is a grey, which has no hue to read back - so a picker that
recomputed its bars from the value each time would swing the hue home to red and strand the user
with no way back to the colour they were on. The three channels are therefore what the surface
KEEPS; the colour is what it derives. Only an assignment from outside (a swatch, a committed hex,
the owner) rebuilds them, because that genuinely is a different colour - and even then the hue is
carried across when the incoming colour has no chroma of its own to name one.

CSS: type `color-picker`, class `xl-ui-color-picker`. Children are `color-picker > preview`,
`> text-input#hex`, `> button`, `> color-picker-tab` (class `active` on the chosen one),
`> color-picker-bar` (a slider), `> number-field` and `> color-picker-swatch`. */
class SP_PUBLIC ColorPickerContent : public Panel {
public:
	// How many channels a mode has. Three in all of them; the alpha is not one of them, because it
	// is not part of the colour the tabs are respelling.
	static constexpr uint32_t ChannelCount = 3;

	// Painted when no sheet ever reaches the surface. Also what ui::ColorField hands
	// PopupSurfaceConfig::fallbackColor, so the panel and the window behind it agree.
	static constexpr Color4B SurfaceColor = Color4B(0x20, 0x20, 0x26, 0xFF);

	/* The extent this surface wants, before any node exists.

	Asked for rather than measured, because it is what a window REQUEST carries and the request is
	settled before there is anything to measure. Depends on the palette (how many rows of swatches)
	and on `alpha` (whether there is a fourth bar), and on nothing else. */
	static Extent2 measure(const ColorPickerParams &);

	virtual ~ColorPickerContent();

	virtual bool init(ColorPickerParams &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	/* Take a colour. Rebuilds the channels from it - see the class comment on why that is an
	assignment and not what an edit does.

	`silent` suppresses `onChange`, which is what an owner echoing its own value back needs. */
	virtual void setValue(const Color4B &, bool silent = false);
	const Color4B &getValue() const { return _value; }

	// Everything sprt::geom::readColor reads. False leaves the value exactly as it was.
	virtual bool setValueFromString(StringView, bool silent = false);

	// "#rrggbb", or "#rrggbbaa" when the surface carries an alpha channel.
	String formatValue() const;

	virtual void setMode(ColorPickerMode);
	ColorPickerMode getMode() const { return _mode; }

	bool isAlphaEnabled() const { return _params.alpha; }

	/* The active mode's channels, in DISPLAY units - exactly the numbers the bars and the boxes are
	showing, so nothing has to convert to read the surface back: 0-255 for R, G and B; 0-359 for the
	hue; 0-100 for saturation, lightness and value.

	Percent rather than a fraction because that is what is on screen. A picker whose API answered
	0.42 for a box reading 42 would make every test and every command translate, and one of them
	would eventually translate the wrong way. */
	SpanView<float> getChannels() const { return SpanView<float>(_channels, ChannelCount); }
	virtual bool setChannel(uint32_t index, float value, bool silent = false);

	// 0-255. Always meaningful; the bar showing it is only there when `alpha` is on.
	float getAlpha() const { return float(_value.a); }
	virtual void setAlpha(float, bool silent = false);

	/* Put the hex on the clipboard, and take it back off.

	The html tag and nothing else: what goes out is what formatValue() prints, and what comes back
	in is read by sprt::geom::readColor - the same function a stylesheet is parsed with - so a
	colour copied out of this picker and pasted into a .css means there what it meant here.

	`paste` is asynchronous the way every clipboard read is: it returns whether the request was
	STARTED, and the value changes later. A refusal - nothing on the clipboard, or text that is not
	a colour - marks the hex line rather than going quiet. */
	virtual bool copyToClipboard();
	virtual bool pasteFromClipboard();

	// The hex line, for what only it can be told. Named `hex`.
	TextInput *getHexInput() const { return _hex; }

	// One channel's bar, or null past the end. A ui::Slider, so a test drives it the way it drives
	// any other.
	Slider *getChannelBar(uint32_t index) const;
	NumberField *getChannelInput(uint32_t index) const;

	// Null when the surface carries no alpha channel. Defined in the .cc: `Bar` is only declared
	// here, and a derived-to-base conversion needs the definition.
	Slider *getAlphaBar() const;
	NumberField *getAlphaInput() const { return _alphaInput; }

	Button *getTab(ColorPickerMode) const;

	SpanView<Color4B> getPalette() const { return _params.palette; }

	// Whether the last commit of the hex text was taken. What the `invalid` mark is painted from.
	bool isValid() const { return _valid; }

protected:
	using Panel::init;

	// The bar: a ui::Slider with a strip of gradient under its track. Defined in the .cc, because
	// nothing outside needs to name the type - a caller reaches it as the Slider it is.
	class Bar;

	// Reads the hex line and takes it, or refuses it. `fromEnter` decides what a refusal means, the
	// same split ui::ColorField documents: Enter keeps the text and the mark, a blur puts the
	// value's own text back.
	virtual bool commitText(bool fromEnter);

	// The colour the current channels and alpha spell.
	Color4B colorFromChannels() const;

	// The channels `_value` spells in the current mode, with the hue carried across when the colour
	// has no chroma to name one of its own.
	void channelsFromValue();

	// A channel or the alpha moved: recompute the value, repaint everything that shows it, report.
	void applyChannels(bool silent);

	// Push `_channels` / `_value` out to the bars, the boxes, the preview and the hex line. The
	// guard keeps the echo of what this writes from being read back as an edit.
	virtual void updateContent();

	// Repaint the gradient under every bar. Separate from updateContent because it is the expensive
	// half - every strip is re-evaluated - and it is only stale when a channel OTHER than the bar's
	// own has moved.
	virtual void updateGradients();

	virtual void updateTabs();

	// The surface's own view of a refusal, which is the hex line's alone: the bars cannot express
	// an invalid colour.
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

	// The letter beside each bar. It says which channel the row is, and it is the one part of a row
	// that changes when the tabs do.
	basic2d::Label *_labels[ChannelCount] = {nullptr, nullptr, nullptr};
	Bar *_bars[ChannelCount] = {nullptr, nullptr, nullptr};
	NumberField *_inputs[ChannelCount] = {nullptr, nullptr, nullptr};

	// Null when the surface carries no alpha channel: there is no row at all then, rather than a
	// disabled one - a control that cannot mean anything is not a control.
	basic2d::Label *_alphaLabel = nullptr;
	Bar *_alphaBar = nullptr;
	NumberField *_alphaInput = nullptr;

	Vector<basic2d::LayerRounded *> _swatches;

	InputListener *_listener = nullptr;
	Rc<ClipboardSession> _clipboard;

	Color4B _value = Color4B::WHITE;
	ColorPickerMode _mode = ColorPickerMode::RGB;

	/* The ACTIVE mode's channels - the editing state, not a projection of the value. See the class
	comment: this is what lets a saturation dragged to nothing keep the hue it was dragged from. */
	float _channels[ChannelCount] = {0.0f, 0.0f, 0.0f};

	// Carried across a mode switch and across a colour that has none of its own.
	float _hue = 0.0f;

	bool _valid = true;

	// Guards updateContent() against the writes it makes being read back as edits.
	bool _inUpdate = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICOLORPICKER_H_
