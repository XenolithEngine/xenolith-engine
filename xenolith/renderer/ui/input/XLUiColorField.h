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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICOLORFIELD_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICOLORFIELD_H_

#include "XLUiPanel.h"
#include "XLUiTextInput.h"
#include "XLUiPopupSurface.h"
#include "XL2dLayerRounded.h"
#include "XL2dIconSprite.h"
#include "XLUiControlLock.h"
#include "XLUiColorPicker.h" // the built-in picker

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A colour field: a swatch, a hex text line and a picker.

Two pickers: the platform colour dialog and a built-in surface. PickerMode selects which one a tap
opens; `Auto` checks `AppWindow::isDialogSupported(DialogType::Color)` at the moment of opening.

The hex line is parsed with `sprt::geom::readColor` (the stylesheet parser: `#rgb`, `#rrggbb`,
`#rrggbbaa`, `rgb()`, `hsl()`, named colours); formatColor is its inverse.

Text is committed on Enter and on blur, not per keystroke (unlike ui::NumberField), since partial
hex strings are rarely valid. Enter keeps a refused text with its message; blur restores the value's
text and clears the mark.

In a form it is one field, joined like ui::VectorField: the form focuses the hex line, a tap in the
hex line reports focus to the form, and Tab leaves to the form.

CSS: type `color-field`, class `xl-ui-color-field`, states `.open`, `.unavailable`, `:invalid`,
`:disabled`.
Children are `color-field > swatch`, `color-field > text-input` and `color-field > icon`. The
swatch colour is the value and is set in code. The built-in picker's surface is type
`color-picker`. */
class SP_PUBLIC ColorField : public Panel, public EditLockTarget {
public:
	/* The accepted colour; not fired for a refusal or a silent assignment. Fires throughout a drag
	of the built-in picker's bars (as ui::Slider does); an owner recording history groups a gesture
	into one entry. */
	using ColorCallback = Function<void(const Color4B &)>;

	// The hex line took or lost the caret. The form adapter uses it to focus this field on a tap
	// (forms/ depends on input/, not the other way).
	using FocusCallback = Function<void(bool focused)>;

	// Tab out of the hex line, as in ui::TextInput.
	using NavigateCallback = Function<bool(bool backwards)>;

	enum class PickerMode {
		Auto, // the system dialog where there is one, the built-in surface where there is not
		System, // only the system dialog: where it is unsupported, a tap opens nothing
		Fallback, // only the built-in surface
	};

	virtual ~ColorField();

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	/* Places the parts by the resolved `direction`, which is only settled at this phase, not in
	   handleContentSizeDirty. */
	virtual void handleLayoutChildren() override;

	void placeInlineParts();
	virtual void handleExit() override;

	virtual void setValue(const Color4B &, bool silent = false);
	const Color4B &getValue() const { return _value; }

	// Accepts what sprt::geom::readColor reads. False leaves the value unchanged.
	virtual bool setValueFromString(StringView, bool silent = false);

	// "#rrggbb", or "#rrggbbaa" when alpha is enabled.
	String formatValue() const;
	static String formatColor(const Color4B &, bool alpha);

	// Whether the value carries alpha: selects the hex format and the system dialog's alpha slider.
	virtual void setAlphaEnabled(bool);
	bool isAlphaEnabled() const { return _alpha; }

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	virtual void setPickerMode(PickerMode);
	PickerMode getPickerMode() const { return _mode; }

	// Whether the system dialog is available for this window now; used by `Auto`.
	bool isSystemPickerAvailable() const;

	// The built-in surface's swatches (a default set is provided); an empty list hides the grid.
	virtual void setPalette(SpanView<Color4B>);
	SpanView<Color4B> getPalette() const { return _palette; }

	// False when disabled, already open, or the mode requires an unavailable system dialog.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _picker != nullptr; }
	SubWindow *getPicker() const { return _picker; }

	// The template for the built-in surface (stylesheet, title, flags); content, size and placement
	// are overwritten by the widget.
	virtual void setPickerConfig(PopupSurfaceConfig &&);
	const PopupSurfaceConfig &getPickerConfig() const { return _pickerConfig; }

	/* Which of RGB / HSL / HSV the built-in surface opens on (distinct from PickerMode). Stored
	here because the surface is recreated on every open. */
	virtual void setPickerColorMode(ColorPickerMode);
	ColorPickerMode getPickerColorMode() const { return _pickerMode; }

	virtual void setValueCallback(ColorCallback &&);
	virtual void setFocusCallback(FocusCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	// The hex line.
	TextInput *getInput() const;

	// Whether the last text commit was accepted; false sets `:invalid`, with the message.
	bool isValid() const { return _valid; }
	StringView getValidationMessage() const { return _message; }

	/* Why the picker did not open: no colour dialog on the platform, or the dialog failed. Separate
	from validation; the value is untouched. Cleared by the next successful open(). */
	bool isUnavailable() const { return _unavailable; }
	StringView getUnavailableMessage() const { return _unavailableMessage; }

	virtual void focus();
	virtual void blur();
	bool isFocused() const;

protected:
	using Panel::init;

	// The hex line: a ui::TextInput that reports the focus edge (the granted focus, not the
	// request). Defined in the .cc.
	class Input;

	// Parses and accepts or refuses the text. On refusal, Enter keeps the text and the mark, blur
	// restores the value's text.
	virtual bool commitText(bool fromEnter);

	virtual void updateContent();
	virtual void updateInteractiveState();
	virtual void setInvalid(bool, StringView message);

	// An action this control offers (opening a picker) could not be performed, and why.
	virtual void setUnavailable(bool, StringView message);

	virtual bool openSystemPicker();
	virtual bool openFallbackPicker();

	virtual bool handleTap();

	void handleInputFocus(bool);

	/* The hex line's echo has been applied; `focused` is what the platform granted. The blur commit
	happens here: the focus edge is reported before the echoed state is stored, so text written
	there would be overwritten. */
	void handleInputEcho(bool focused);

	AppWindow *getAppWindow() const;

	basic2d::LayerRounded *_swatch = nullptr;
	Input *_input = nullptr;
	basic2d::IconSprite *_icon = nullptr;

	InputListener *_listener = nullptr;

	Color4B _value = Color4B::WHITE;
	Vector<Color4B> _palette;
	String _message;

	PickerMode _mode = PickerMode::Auto;
	bool _alpha = false;
	bool _valid = true;

	String _unavailableMessage;
	bool _unavailable = false;

	// Guards updateContent() against being read back as an edit by the text it writes.
	bool _inUpdate = false;

	Rc<SubWindow> _picker;
	PopupSurfaceConfig _pickerConfig;
	ColorPickerMode _pickerMode = ColorPickerMode::RGB;

	// The open system dialog's cancellation token, if any: prevents a second dialog and is
	// cancelled when the field leaves the scene.
	Rc<sprt::window::DialogRequest> _dialog;

	ColorCallback _valueCallback;
	FocusCallback _focusCallback;
	NavigateCallback _navigateCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICOLORFIELD_H_
