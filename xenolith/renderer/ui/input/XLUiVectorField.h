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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUIVECTORFIELD_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUIVECTORFIELD_H_

#include "XLUiPanel.h"
#include "XLUiNumberField.h"
#include "XL2dLabel.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A row of ui::NumberFields that is one value (a Vec2, Vec3, RGB triple, range).

Inside a ui::FormSystem it is one field: one array under one name, validated once, one stop in the
tab ring. The components are real widgets underneath; the form still delivers their keys because
it admits listeners at or below the focused field's node (FormSystem::isWithinFocusedField).

Tab and Shift+Tab step between components. At either end the key is handed out: to the form via
FormInputListener::requestNavigate, or, standalone, by giving up focus. Entering backwards lands on
the last component (FormFieldSlots::setFocused's `backwards`).

setInteger, setRange, setStep and the drag settings are written into every component at the time
of the call; nothing is kept in parallel. A per-component setting made through getComponentAt(i)
lasts until the next shared write.

The value is an array, not keyed by the labels, which are presentation only.

When a component refuses its text, the row also gets the `invalid` class the form uses, and
getValidationMessage() names the component.

CSS: type `vector-field`, class `xl-ui-vector-field`, plus `arity-N` for the current width, so a
sheet can lay a pair out differently from a quadruple. Children are `vector-field > number-field`
and `vector-field > component-label`. Hover lives on the components, which are what a pointer is
actually over; the row itself paints `:focus` (any component focused) and `:disabled`. */
class SP_PUBLIC VectorField : public Panel, public EditLockTarget {
public:
	// The whole vector, on every accepted change to any component. Per-component notification is
	// on the component itself.
	using ValueCallback = Function<void(SpanView<double>)>;

	// Which component holds the keyboard, or -1. The form adapter listens here so a tap into a
	// component makes the form focus this field (input/ cannot depend on forms/).
	using FocusCallback = Function<void(int32_t component)>;

	// Tab at either end of the row, as on ui::TextInput: inside a form the adapter hands this to
	// the form; standalone, the row gives up focus.
	using NavigateCallback = Function<bool(bool backwards)>;

	static constexpr uint32_t DefaultArity = 3;

	virtual ~VectorField();

	virtual bool init() override;
	virtual bool init(uint32_t arity);

	virtual void handleContentSizeDirty() override;

	// How many components. Refuses 0. Values at overlapping indices are kept.
	virtual bool setArity(uint32_t);
	uint32_t getArity() const { return uint32_t(_components.size()); }

	// x, y, z, w and then the index, unless told otherwise. An empty list removes the labels
	// entirely, for a row that is captioned from outside.
	virtual void setLabels(SpanView<StringView>);

	/* The unit of the whole row, drawn once after the last component. Not written into the
	components; set a per-component unit through getComponentAt(). */
	virtual void setUnit(StringView);
	StringView getUnit() const { return _unit; }
	basic2d::Label *getUnitLabel() const { return _unitLabel; }

	// ---- shared component settings; see the class comment on what "shared" means ---------------

	virtual void setInteger(bool);
	bool isInteger() const { return _integer; }

	virtual void setRange(double min, double max);
	virtual void clearRange();

	virtual void setStep(double);
	virtual void setDragEnabled(bool);
	virtual void setDragSensitivity(float);

	virtual void setEnabled(bool) override;
	bool isEnabled() const override { return isControlEnabled(this); }

	/* The component itself, or null for an index past the arity. Named `At` so it does not hide
	Node::getComponent<T>(). */
	NumberField *getComponentAt(uint32_t) const;

	// ---- the value -----------------------------------------------------------------------------

	// False, and nothing changes, when the length does not match the arity.
	virtual bool setValue(SpanView<double>, bool silent = false);
	SpanView<double> getValue() const { return _values; }

	virtual bool setComponentValue(uint32_t, double, bool silent = false);
	double getComponentValue(uint32_t) const;

	// True when no component is holding text it refused. The message names the component:
	// "y: 1000 is past the maximum 999".
	bool isValid() const { return _message.empty(); }
	StringView getValidationMessage() const { return _message; }

	virtual void setValueCallback(ValueCallback &&);
	virtual void setFocusCallback(FocusCallback &&);
	virtual void setNavigateCallback(NavigateCallback &&);

	// ---- focus ---------------------------------------------------------------------------------

	// The focused component; -1 when the row does not hold the keyboard.
	int32_t getFocusedComponent() const { return _focused; }

	virtual void focus(uint32_t component = 0);
	virtual void blur();

	/* Enter the row from a navigation: the last component when backwards, the first otherwise.
	No-op when a component already holds focus (a tap decided it). */
	virtual void focusFromNavigation(bool backwards);

protected:
	using Panel::init;

	// A ui::NumberField that reports focus changes and validity changes to the row. Defined in
	// the .cc.
	class Component;

	virtual void rebuildComponents(uint32_t arity);
	virtual void updateLabels();
	virtual void updateArityClass();
	virtual void updateInteractiveState();

	// Recomputes the row's verdict from the components and repaints the `invalid` class. Called
	// whenever a component's own verdict moves.
	virtual void updateValidity();

	void handleComponentValue(uint32_t index, double value);
	void handleComponentFocus(uint32_t index, bool focused);

	// Tab inside the row, or out of it. True when this widget consumed it.
	bool handleComponentNavigate(uint32_t index, bool backwards);

	/* Tab that no component answered: between a Tab and the platform's focus echo no component is
	focused, so a second Tab in the same batch is handled here, guarded by `_pending`. */
	bool handleRowNavigate(bool backwards);

	// x/y/z/w, then the index. Also names the component in a validation message when labels are
	// hidden.
	String getDefaultLabel(uint32_t) const;

	InputListener *_keyListener = nullptr;

	Vector<NumberField *> _components;
	Vector<basic2d::Label *> _labels;

	// Not in _labels, which rebuildComponents() recreates on setArity. Typed `field-unit`, not
	// `component-label`, so it is not taken for a per-component label.
	basic2d::Label *_unitLabel = nullptr;
	String _unit;

	// What setLabels was given, empty until it is called. `_labelsExplicit` is the difference
	// between "not told" (x/y/z/w) and "told none" (no labels at all).
	Vector<String> _labelStrings;
	bool _labelsExplicit = false;

	// The accepted values, kept in step with the components: getValue hands out a view.
	Vector<double> _values;

	String _message;
	String _arityClass;

	bool _integer = false;
	bool _hasRange = false;
	double _min = 0.0;
	double _max = 0.0;
	double _step = 1.0;

	bool _dragEnabled = true;
	float _dragSensitivity = NumberField::DefaultDragSensitivity;


	int32_t _focused = -1;

	/* The component a Tab has asked for, before it holds the keyboard. The old component loses
	focus synchronously but the new one gains it only on the platform echo; meanwhile `_focused`
	is -1 and steps are measured from this. */
	int32_t _pending = -1;

	ValueCallback _valueCallback;
	FocusCallback _focusCallback;
	NavigateCallback _navigateCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUIVECTORFIELD_H_
