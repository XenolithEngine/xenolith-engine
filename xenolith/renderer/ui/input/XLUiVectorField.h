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
#include "XLUiEditLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A row of ui::NumberFields that is ONE value.

A Vec2, a Vec3, an RGB triple, a range: several numbers that mean one thing. What makes this a
widget rather than a habit of putting three fields beside each other is that everything outside it
treats it as a single field.

ONE FORM FIELD, NOT N. Inside a ui::FormSystem this collects one array under one name, is validated
once, and occupies one stop in the tab ring. Three separate fields would collect three keys, refuse
a Required vector three times, and make Tab walk components where the author meant it to walk
properties. The components are still real widgets underneath - they keep their own keys, their own
drag, their own `invalid` - and the form never sees them, because ui::FormSystem admits a listener
whose owner IS the focused field's node or sits BELOW it (FormSystem::isWithinFocusedField). That
one rule is what makes composition possible with no new machinery.

THE KEYBOARD IS SPLIT BY PLACE, the same way ui::Select's is. Inside the row this widget navigates:
Tab steps to the next component, Shift+Tab to the previous. At either end it hands the key OUT -
to the form through FormInputListener::requestNavigate, or, with no form above it, by giving up
focus the way a lone ui::TextInput does. Coming back the other way, a Shift+Tab that ENTERS the row
lands on the LAST component: that is what FormFieldSlots::setFocused's `backwards` argument is for,
and without it backwards navigation would walk forwards inside every composite widget.

WHAT IS SHARED AND WHAT IS NOT. setInteger, setRange, setStep and the drag settings are written
into every component AT THE MOMENT OF THE CALL - they are conveniences over the same setter, not a
state this widget keeps in parallel. A component that needs its own range (a normalized w, an angle
in degrees beside two lengths) is configured through getComponent(i) afterwards, and keeps it until
the next shared write.

THE VALUE IS AN ARRAY, and only an array. Not a dictionary keyed by the component labels: the
labels are presentation - setLabels replaces them, and an empty list removes them - and a value
whose keys came from a label would change meaning when the labels did.

A REFUSAL MARKS THE ROW. ui::NumberField refuses a number typed past its range and marks itself;
this widget marks ITSELF as well, with the same `invalid` class the form uses, and names the
component in getValidationMessage(). One error is one mark, in the place the form and the
stylesheet already look for it.

CSS: type `vector-field`, class `xl-ui-vector-field`, plus `arity-N` for the current width, so a
sheet can lay a pair out differently from a quadruple. Children are `vector-field > number-field`
and `vector-field > component-label`. Hover lives on the components, which are what a pointer is
actually over; the row itself paints `:focus` (any component focused) and `:disabled`. */
class SP_PUBLIC VectorField : public Panel, public EditLockTarget {
public:
	// The whole vector, on every accepted change to any component. A consumer of this widget holds
	// a vector, so that is what it hears; per-component notification is on the component itself.
	using ValueCallback = Function<void(SpanView<double>)>;

	// Which component holds the keyboard, or -1. The FORM ADAPTER listens here: a tap that puts
	// the caret in a component has to make the form focus this FIELD, or the form goes on
	// filtering keys to whatever it focused last and the arrows die inside the row. The widget
	// cannot do that itself - forms/ knows about input/ and never the other way round.
	using FocusCallback = Function<void(int32_t component)>;

	// Tab at either end of the row. Exactly ui::TextInput's seam, for exactly its reason: inside a
	// form the adapter hands this to the form; standalone, the row just gives up focus.
	using NavigateCallback = Function<bool(bool backwards)>;

	static constexpr uint32_t DefaultArity = 3;

	virtual ~VectorField();

	virtual bool init() override;
	virtual bool init(uint32_t arity);

	virtual void handleContentSizeDirty() override;

	/* How many components. Refuses 0 - a vector of nothing is not a value.

	Values SURVIVE the change where the indices overlap: widening a Vec3 to a Vec4 must not empty
	the three numbers already typed, and narrowing and widening back is a common way to fix a
	mistake. */
	virtual bool setArity(uint32_t);
	uint32_t getArity() const { return uint32_t(_components.size()); }

	// x, y, z, w and then the index, unless told otherwise. An EMPTY list removes the labels
	// entirely, for a row that is captioned from outside.
	virtual void setLabels(SpanView<StringView>);

	/* The unit of the WHOLE ROW, drawn once after the last component - metres, degrees, px.

	One, not one per component, and that is a statement about what a vector is: a Vec3 in metres is
	metres in all three, and three "px" in a row read as noise rather than as information. It is
	also the shape the value has - a control hint carries one unit per FIELD, and this widget is one
	field.

	Unlike setRange/setStep/setInteger this does NOT fan out into the components: those are
	conveniences over each component's own setter, while this one belongs to the row itself. A
	component that must name its own unit is reached through getComponentAt(). */
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

	virtual void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

	/* The component itself, for what only it can be told. Null for an index past the arity.

	`At` because a Node ALREADY has getComponent<T>() - the node-component system's - and a member
	of that name here would hide it inside this class and in every caller holding a VectorField. */
	NumberField *getComponentAt(uint32_t) const;

	// ---- the value -----------------------------------------------------------------------------

	/* False when the length does not match the arity, and then NOTHING moves: half of an assigned
	vector describes something other than what was asked for. */
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

	// The focus of ONE component; -1 when the row does not hold the keyboard.
	int32_t getFocusedComponent() const { return _focused; }

	virtual void focus(uint32_t component = 0);
	virtual void blur();

	/* Enter the row from a navigation: the last component when it went backwards, the first
	otherwise - and nothing at all when a component already holds focus, because then a tap has
	already decided where the caret goes and the form is only catching up. */
	virtual void focusFromNavigation(bool backwards);

protected:
	using Panel::init;

	// A ui::NumberField that reports what only its owner needs to know: that it took focus, and
	// that its verdict on its own text changed. Defined in the .cc - nothing outside this widget
	// has a use for the type.
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

	/* Tab that no component answered.

	The components answer their own while they hold the keyboard, and normally that is all of them.
	But focus moves through the platform's text input: the component a Tab moved AWAY from stops
	being focused at once, while the one it moved TO only becomes focused when the echo comes back,
	so a second Tab arriving in the same key batch - two in one frame, or an autorepeat - finds
	nobody focused and would be dropped. This listener is the row answering for its parts in that
	window, which is why it is guarded by the row's own idea of where the caret is going. */
	bool handleRowNavigate(bool backwards);

	// x/y/z/w, then the index. Also what names the component in a validation message, so it is a
	// name even for a row whose labels are hidden.
	String getDefaultLabel(uint32_t) const;

	InputListener *_keyListener = nullptr;

	Vector<NumberField *> _components;
	Vector<basic2d::Label *> _labels;

	// Deliberately NOT in _labels: rebuildComponents() tears that vector down on every setArity,
	// and the row's unit has to survive a widening. Typed `field-unit` rather than
	// `component-label` for the same reason - anything walking the children by that type is asking
	// for the per-component labels and must not be handed this one.
	basic2d::Label *_unitLabel = nullptr;
	String _unit;

	// What setLabels was given, empty until it is called. `_labelsExplicit` is the difference
	// between "not told" (x/y/z/w) and "told none" (no labels at all).
	Vector<String> _labelStrings;
	bool _labelsExplicit = false;

	// The accepted values, kept in step with the components rather than derived on read: getValue
	// hands out a view, and a view of a temporary is a bug waiting for a caller.
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

	bool _enabled = true;
	bool _invalidApplied = false;

	int32_t _focused = -1;

	/* The component a Tab has ASKED for, before it holds the keyboard.

	Focus is not immediate, and it is not symmetric either: the component a Tab moves AWAY from
	stops being focused at once, because TextInputManager displaces its handler synchronously,
	while the one it moves TO becomes focused only when the platform's echo arrives. In that window
	`_focused` is -1 and nobody would answer the next Tab - so this is what the step is measured
	from, and it is why handleRowNavigate exists at all. FormSystem::focusNext steps from its
	pending field for the same reason, one level up. */
	int32_t _pending = -1;

	ValueCallback _valueCallback;
	FocusCallback _focusCallback;
	NavigateCallback _navigateCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUIVECTORFIELD_H_
