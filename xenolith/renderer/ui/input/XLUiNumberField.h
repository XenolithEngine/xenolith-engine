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

/** Scrub (drag-to-change) arithmetic, shared by NumberField and callers that scrub a value without
a field, so both land on the same number.

Travel is accumulated by the caller and passed whole, so movements shorter than one step are not
lost. The range clamps here (a typed value is refused instead). */
struct SP_PUBLIC ScrubRange {
	bool has = false;
	double min = 0.0;
	double max = 0.0;
};

// How many steps a run of horizontal travel is worth. Sensitivity is points per step and must
// be > 0; the result is truncated toward zero, so travel below one step is worth none.
SP_PUBLIC double scrubSteps(float travel, float sensitivity);

// base + step * steps, truncated when the value is whole-numbers-only and clamped into the range.
SP_PUBLIC double scrubValue(double base, double steps, double step, bool integer,
		const ScrubRange & = ScrubRange());

/** A text field that holds a number. Filtering is done here: TextInputType::Number_* is only an
IME hint.

Range handling:

  * typed out of range - refused: the value and callback do not change, the node matches `:invalid`
    and getValidationMessage() says why;
  * dragged or stepped out of range - clamped.

setValue() does not clamp a program-assigned value.

Blur restores the text of the held value when the text does not parse; `:invalid` applies only
while editing.

Dragging scrubs only an unfocused field; a focused field is dragged to select text. The callback
fires throughout the drag; an owner recording history groups it into one entry.

CSS: type `number-field`, class `xl-ui-number-field`, the same attributes as ui::TextInput, and
`:invalid` for a refused value. */
class SP_PUBLIC NumberField : public TextInput {
public:
	// The accepted value; not fired for a refusal or uncommitted text.
	using ValueCallback = Function<void(double)>;

	// Points of horizontal travel per step of the value, at the default sensitivity.
	static constexpr float DefaultDragSensitivity = 4.0f;

	virtual ~NumberField();

	virtual bool init() override;

	// Whole numbers only: the fractional separator is not accepted or printed.
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

	// Whether the current text parses and is in range; false sets `:invalid`.
	bool isValid() const { return _valid; }
	StringView getValidationMessage() const { return _message; }

	virtual void setValueCallback(ValueCallback &&);

	/* A unit label shown beside the number (px, s, deg); display only, no conversion. It is a
	sibling of the text viewport, not part of the text, and the viewport is inset by its width. An
	empty unit hides the label. */
	virtual void setUnit(StringView);
	StringView getUnit() const { return _unit; }
	basic2d::Label *getUnitLabel() const { return _unitLabel; }

	virtual void setDragEnabled(bool);
	bool isDragEnabled() const { return _dragEnabled; }

	virtual void setDragSensitivity(float);
	float getDragSensitivity() const { return _dragSensitivity; }

	// Restores the value's text when the current text does not parse.
	virtual void blur() override;

	/* Text set from outside is committed like typed text. Needed because an unfocused field writes
	locally with no echo; the narrow overload forwards here. */
	virtual void setText(WideStringView) override;
	using TextInput::setText;

	// True while a drag is changing the value, i.e. between the press and the release.
	bool isDragging() const { return _dragging; }

	// The canonical text of a value; parse(format(v)) == v for every value the field can hold.
	String formatValue(double) const;

protected:
	using TextInput::init;

	// Parses and accepts or refuses the text; returns whether the value changed. The only writer of
	// `_valid` and `:invalid`.
	virtual bool commit();

	// Writes the value as text without triggering commit().
	virtual void updateText();

	virtual void setInvalid(bool, StringView message);

	virtual void handleContentSizeDirty() override;

	/* Places the unit by the resolved `direction`, which is only settled at this phase, not in
	   handleContentSizeDirty. */
	virtual void handleLayoutChildren() override;

	void placeUnitLabel();

	// The unit's width plus its gap, measured in handleContentSizeDirty before the base sizes the
	// viewport.
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

	// Accumulated horizontal travel of the running drag, so sub-step movements are not lost.
	float _dragTravel = 0.0f;
	double _dragOrigin = 0.0;
	float _dragSensitivity = DefaultDragSensitivity;

	String _message;

	// Created on the first non-empty unit and kept afterwards.
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
