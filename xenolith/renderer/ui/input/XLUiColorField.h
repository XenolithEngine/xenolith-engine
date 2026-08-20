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
#include "XLUiEditLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A colour: a swatch of it, its hex, and a way to pick another one.

A swatch with a picker, not four spin boxes - which is the whole reason a colour is a type of its
own rather than a vector of four numbers.

TWO PICKERS, AND WHICH ONE IS A DECLARED CHOICE. The platform's colour dialog is the right one
where it exists: it is the one the user already knows, and it can reach the whole screen. It does
not exist everywhere - `AppWindow::isDialogSupported(DialogType::Color)` is exactly the question -
and a widget with no answer for that case is a widget that does not work on the machine in front of
you. So this carries a picker of its own, and PickerMode says which one a tap opens: `Auto` asks the
window AT THE MOMENT OF OPENING (the window can change under a widget), `System` and `Fallback` are
the direct answers. A mode is not a debug switch: an application that wants its own picker
everywhere, for one look across platforms, says so.

THE HEX FIELD IS REAL. Typing `#3a7` is how a colour is entered without any picker at all, and it
is the only path a keyboard user has where no dialog exists. Reading it is NOT this widget's work:
`sprt::geom::readColor` already reads `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb()`, `hsl()` and the
named colours, and it is the same function a stylesheet is parsed with - so what this field accepts
and what CSS accepts cannot drift apart. Printing has no such function, and formatColor is its
inverse: `readColor(formatColor(c)) == c` for every colour this field can hold.

IT COMMITS ON ENTER AND ON BLUR, and that is a deliberate difference from ui::NumberField, which
commits on every keystroke. A number has valid prefixes - "-", "1", "1." are all on the way to a
number - while a colour has almost none: `#ff0` is a colour and `#ff00` is nothing, so committing
per keystroke would flash a refusal through the middle of every value typed. The two keys therefore
mean different things: ENTER is "take this", so a refusal stays on screen with its reason, while
BLUR is "I am done", so the text goes back to what the value actually is and the mark clears.

INSIDE A FORM it is ONE field - the swatch, the hex line and the button are one value - and it
joins by the same three seams ui::VectorField uses: the form focuses it and the hex line takes the
caret, a tap in the hex line tells the form to catch up, and Tab out of it goes to the form rather
than to nowhere.

CSS: type `color-field`, class `xl-ui-color-field`, states `.open`, `.invalid`, `.disabled`.
Children are `color-field > swatch`, `color-field > text-input` and `color-field > icon`. The
swatch's colour is the VALUE, so it is written in code rather than by a sheet - it is data, not
decoration. The built-in picker's surface is type `color-picker`. */
class SP_PUBLIC ColorField : public Panel, public EditLockTarget {
public:
	// The accepted colour. Not fired for a refusal, and not for a value a program assigned
	// silently.
	using ColorCallback = Function<void(const Color4B &)>;

	// The hex line took or lost the caret. The FORM ADAPTER listens here: a tap that puts the caret
	// in it has to make the form focus this FIELD, or the form goes on filtering keys to whatever
	// it focused last. The widget cannot do that itself - forms/ knows about input/, never the
	// other way round.
	using FocusCallback = Function<void(bool focused)>;

	// Tab out of the hex line. Exactly ui::TextInput's seam, for its reason.
	using NavigateCallback = Function<bool(bool backwards)>;

	enum class PickerMode {
		Auto, // the system dialog where there is one, the built-in surface where there is not
		System, // only the system dialog: where it is unsupported, a tap opens nothing
		Fallback, // only the built-in surface
	};

	virtual ~ColorField();

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
	virtual void handleExit() override;

	virtual void setValue(const Color4B &, bool silent = false);
	const Color4B &getValue() const { return _value; }

	// Everything sprt::geom::readColor reads. False leaves the value exactly as it was.
	virtual bool setValueFromString(StringView, bool silent = false);

	// "#rrggbb", or "#rrggbbaa" when alpha is enabled.
	String formatValue() const;
	static String formatColor(const Color4B &, bool alpha);

	// Whether the value carries an alpha channel: it decides the hex the field prints and collects,
	// and it is what asks the system dialog for an alpha slider.
	virtual void setAlphaEnabled(bool);
	bool isAlphaEnabled() const { return _alpha; }

	virtual void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

	virtual void setPickerMode(PickerMode);
	PickerMode getPickerMode() const { return _mode; }

	// Whether the system dialog can be served for this window right now. What `Auto` asks.
	bool isSystemPickerAvailable() const;

	// The built-in surface's swatches. A modest set is built in; an empty list hides the grid and
	// leaves the hex line as the whole picker.
	virtual void setPalette(SpanView<Color4B>);
	SpanView<Color4B> getPalette() const { return _palette; }

	// False when the field is disabled, when the surface is already up, or when the mode asks for a
	// system dialog the platform will not serve.
	virtual bool open();
	virtual void close();
	bool isOpen() const { return _picker != nullptr; }
	SubWindow *getPicker() const { return _picker; }

	// The template the built-in surface is opened from: its stylesheet, its title, its flags. The
	// content, the size and the placement are the widget's and are overwritten.
	virtual void setPickerConfig(PopupSurfaceConfig &&);

	virtual void setValueCallback(ColorCallback &&);
	virtual void setFocusCallback(FocusCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	// The hex line, for what only it can be told.
	TextInput *getInput() const;

	// Whether the last commit of the text was taken. False is what the `invalid` class is painted
	// from, and the message is why.
	bool isValid() const { return _valid; }
	StringView getValidationMessage() const { return _message; }

	/* WHY THE PICKER DID NOT OPEN - which is a different question from whether the value is any
	good, and used to be answered through the same channel.

	Two things reach here: a platform with no colour dialog at all, and a dialog that ran and came
	back failed. Neither is a validation failure. The value is untouched, the hex line still takes
	the colour typed by hand, and only one ROUTE into the field is missing - so painting `invalid`
	sent the author hunting for a typo in a value that had none.

	Cleared by the next open() that gets somewhere, because "the picker is unavailable" is a
	statement about the attempt, not a property the field keeps for ever. */
	bool isUnavailable() const { return _unavailable; }
	StringView getUnavailableMessage() const { return _unavailableMessage; }

	virtual void focus();
	virtual void blur();
	bool isFocused() const;

protected:
	using Panel::init;

	// The hex line. A ui::TextInput that reports the focus EDGE - which is not the same thing as
	// the focus request, and is the only moment the row can act on. Defined in the .cc.
	class Input;

	// Reads the text and takes it, or refuses it. `fromEnter` decides what a refusal means: Enter
	// keeps the text and the mark, a blur puts the value's own text back.
	virtual bool commitText(bool fromEnter);

	virtual void updateContent();
	virtual void updateInteractiveState();
	virtual void setInvalid(bool, StringView message);

	// The other channel: an ACTION this control offers could not be performed, and why.
	virtual void setUnavailable(bool, StringView message);

	virtual bool openSystemPicker();
	virtual bool openFallbackPicker();

	virtual bool handleTap();

	void handleInputFocus(bool);

	/* The hex line's echo has just been applied, and `focused` is what the platform granted.

	The BLUR commit has to happen here rather than on the focus edge: the edge is reported from
	inside TextInput::handleTextInput, BEFORE the echoed state is stored, so text written back there
	is overwritten by the very echo that announced the blur. ui::NumberField restores in the same
	two places and for the same reason. */
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
	bool _enabled = true;
	bool _valid = true;
	bool _invalidApplied = false;

	String _unavailableMessage;
	bool _unavailable = false;

	// Guards updateContent() against being read back as an edit by the text it writes.
	bool _inUpdate = false;

	Rc<SubWindow> _picker;
	PopupSurfaceConfig _pickerConfig;

	// The cancellation token of the system dialog that is up, and what says one IS up. Kept because
	// a second tap must not open a second dialog, and because a field leaving the scene has to take
	// its dialog with it.
	Rc<sprt::window::DialogRequest> _dialog;

	ColorCallback _valueCallback;
	FocusCallback _focusCallback;
	NavigateCallback _navigateCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICOLORFIELD_H_
