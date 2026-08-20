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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUINUMBERFIELD_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUINUMBERFIELD_H_

#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A text field that holds a NUMBER.

IT HAS TO BE A WIDGET, not a flag. TextInputType::Number_* is a hint to the platform's IME and
nothing more - it reaches `_inputState.type` and never filters a keystroke - so a field that only
accepts numbers is code, here.

THE RANGE IS DECLARED, NOT DERIVED, and what happens at its edge depends on how the value got
there:

  * TYPED out of range - REFUSED. The value does not move, the callback does not fire, the node
    takes the `invalid` style class and getValidationMessage() says why. Silently correcting what
    someone typed shows them a number they did not write and does not say why;
  * DRAGGED out of range - CLAMPED. A drag is a continuous gesture with no "wrong" state to be in,
    and stopping at the end of the range is what a slider does.

The asymmetry is declared rather than accidental. Nothing here clamps the value a program assigns
through setValue() either: the range is guidance for the person editing, and a store that refused
to show what it holds would be worse than one that shows an out-of-range number.

BLUR RESTORES. A field left holding text that does not parse would say one thing on screen and
another through getValue(); the `invalid` mark lives only while the text is being edited.

THE DRAG runs on an UNFOCUSED field only. A focused field is dragged to select text, and TextInput
already does that; the rule "click to type, drag to scrub" costs the widget no gesture of its own.
The callback fires continuously through the drag - a value that only arrives at the end has no live
feedback - so an owner that records history is the one that groups a drag into a single entry.

CSS: type `number-field`, class `xl-ui-number-field`, and the same attributes ui::TextInput takes
(they are the same appliers, registered for this type as well). The refusal mark is the class
`invalid`, because the engine's CSS subset has no `:invalid`. */
class SP_PUBLIC NumberField : public TextInput {
public:
	// The value as accepted. Not fired for a refusal, and not for text that has not been committed.
	using ValueCallback = Function<void(double)>;

	// Points of horizontal travel per step of the value, at the default sensitivity.
	static constexpr float DefaultDragSensitivity = 4.0f;

	virtual ~NumberField();

	virtual bool init() override;

	// Whole numbers only: the fractional separator stops being an accepted character and the text
	// is printed without one.
	virtual void setInteger(bool);
	bool isInteger() const { return _integer; }

	virtual void setRange(double min, double max);
	virtual void clearRange();
	bool hasRange() const { return _hasRange; }
	double getMin() const { return _min; }
	double getMax() const { return _max; }

	// What one arrow press, or one step of the drag, is worth. Must be > 0.
	virtual void setStep(double);
	double getStep() const { return _step; }

	virtual void setValue(double, bool silent = false);
	double getValue() const { return _value; }

	// Whether the text as it stands right now parses and is in range. False is what the `invalid`
	// class is painted from.
	bool isValid() const { return _valid; }
	StringView getValidationMessage() const { return _message; }

	virtual void setValueCallback(ValueCallback &&);

	/* A word shown BESIDE the number - px, s, hp, deg. IT IS A LABEL AND NOTHING ELSE: nothing
	here converts, scales, or validates against a unit. A unit that meant conversion would be a
	TYPE, and arithmetic hidden inside presentation is the worst place to keep it - the same line
	the studio's control hints draw, carried here so both ends agree.

	It is a SIBLING of the text viewport, never part of the text. commit() requires the WHOLE text
	to be the number and parse(format(v)) == v is this widget's contract, so a suffix living in the
	string would break both, and getText() would start returning something no one typed.

	What it costs is width: the viewport is inset by what the unit measures, so the caret, the
	selection and the horizontal slide all go on working inside a narrower box. An empty unit takes
	the label away and gives the width back. */
	virtual void setUnit(StringView);
	StringView getUnit() const { return _unit; }
	basic2d::Label *getUnitLabel() const { return _unitLabel; }

	virtual void setDragEnabled(bool);
	bool isDragEnabled() const { return _dragEnabled; }

	virtual void setDragSensitivity(float);
	float getDragSensitivity() const { return _dragSensitivity; }

	// Restores the text when it does not parse: what is shown and what is held must agree the
	// moment the field stops being edited.
	virtual void blur() override;

	/* Text written from outside is read as an edit, exactly like text typed into the field.

	The two paths differ inside TextInput - a focused field defers to the platform and hears its
	own echo, an unfocused one writes locally and there is no echo at all - so hooking only the
	echo would leave setText() changing what is shown without changing what is held. The narrow
	overload forwards to this one, so this is the single seam. */
	virtual void setText(WideStringView) override;
	using TextInput::setText;

	// True while a drag is changing the value, i.e. between the press and the release.
	bool isDragging() const { return _dragging; }

	// The canonical text of a value, and the inverse of what commit() reads. parse(format(v)) == v
	// for every value this field can hold - a pair that is not reversible is a field that changes
	// the number by being looked at.
	String formatValue(double) const;

protected:
	using TextInput::init;

	/* Read the text and take it, or refuse it. Returns whether the value moved.

	This is the only place the text becomes a number, and the only writer of `_valid` and of the
	`invalid` class. */
	virtual bool commit();

	// Write the value back out as text. Silent as far as commit() is concerned: this is the field
	// agreeing with itself, not an edit.
	virtual void updateText();

	virtual void setInvalid(bool, StringView message);

	virtual void handleContentSizeDirty() override;

	// The unit's width plus its gap, on the right. Measured in handleContentSizeDirty BEFORE the
	// base sizes the viewport, which is the only moment it can be trusted.
	virtual Padding getViewportInset() const override;

	virtual void handleTextInput(const TextInputState &) override;

	virtual bool handleInputChar(char16_t) override;
	virtual bool handleKey(const GestureData &) override;

	virtual bool handleSwipeBegin(const Vec2 &) override;
	virtual bool handleSwipe(const Vec2 &location, const Vec2 &delta) override;
	virtual bool handleSwipeEnd() override;

	// value + delta steps, clamped into the range when there is one.
	double stepped(double base, double steps) const;

	double _value = 0.0;
	double _min = 0.0;
	double _max = 0.0;
	double _step = 1.0;

	// Accumulated horizontal travel of the running drag, so a movement smaller than one step is
	// remembered instead of being rounded away on every frame.
	float _dragTravel = 0.0f;
	double _dragOrigin = 0.0;
	float _dragSensitivity = DefaultDragSensitivity;

	String _message;

	// Built on the first non-empty unit and kept afterwards: a field that never names a unit costs
	// no node at all.
	basic2d::Label *_unitLabel = nullptr;
	String _unit;
	float _unitInset = 0.0f;

	bool _integer = false;
	bool _hasRange = false;
	bool _valid = true;
	bool _dragEnabled = true;
	bool _dragging = false;

	// Guards updateText() against being read back as an edit by the echo it causes.
	bool _inUpdate = false;

	ValueCallback _valueCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUINUMBERFIELD_H_
