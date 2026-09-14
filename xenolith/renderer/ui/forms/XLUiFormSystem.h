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

#ifndef XENOLITH_RENDERER_UI_FORMS_XLUIFORMSYSTEM_H_
#define XENOLITH_RENDERER_UI_FORMS_XLUIFORMSYSTEM_H_

#include "XLUiFormInputListener.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// A form: a system on the form's root node and the focus group for the FormInputListeners in its
// subtree. Collects, clears, validates and navigates fields only through their slots. A field
// joins the nearest form above it, so nested forms are disjoint. Public calls happen outside a
// visit, so fields find their form by walking parents, not the frame stack.
//
// Overrides two FocusGroup behaviours: focus is per widget (any listener at or below the focused
// field's node receives keys, not only the focused listener), and a lost focus falls back to a
// real field rather than listeners.front().
//
// The tab ring is the dispatcher's listener vector (priority desc, visit order desc) reversed:
// with all fields at priority 0 that is document order. Hidden subtrees and disabled listeners
// never register, so they are excluded. Not Exclusive: that would cut the form off from outside
// listeners; inside an exclusive group (basic2d::OverlayLayout) it needs Flags::Propagate.
class SP_PUBLIC FormSystem : public FocusGroup {
public:
	using SubmitCallback = Function<void(Value &&)>;
	using ResetCallback = Function<void()>;
	using InvalidCallback = Function<void(SpanView<FormValidationError>)>;

	virtual ~FormSystem() = default;

	virtual bool init() override;

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;

	// The nearest FormSystem at or above `node`
	static FormSystem *findForNode(Node *);

	virtual void setValueMode(FormValueMode);
	virtual FormValueMode getValueMode() const { return _valueMode; }

	// Escape on a focused field resets the form. Off by default
	virtual void setResetOnEscape(bool value) { _resetOnEscape = value; }
	virtual bool isResetOnEscape() const { return _resetOnEscape; }

	virtual void setSubmitCallback(SubmitCallback &&);
	virtual void setResetCallback(ResetCallback &&);
	virtual void setInvalidCallback(InvalidCallback &&);

	// Called by FormInputListener as it enters and leaves the scene
	virtual void addField(NotNull<FormInputListener>);
	virtual void removeField(NotNull<FormInputListener>);

	virtual FormInputListener *getField(StringView name) const;
	SpanView<FormInputListener *> getFields() const { return _fields; }

	// Every field with a `collect` slot that is neither Transient nor a button
	virtual Value collect() const;
	virtual void assign(const Value &);
	virtual void reset();

	// Validates; on success clears every invalid mark and fires the submit callback with
	// collect(). On failure marks each offender, focuses the first one, fires the invalid callback
	// and returns false without submitting anything
	virtual bool submit();

	virtual bool validate(Vector<FormValidationError> &) const;

	// --- focus ---------------------------------------------------------------------------------

	virtual bool canHandleEventWithListener(const InputEvent &, NotNull<InputListener>) override;

	// Focusable fields in document order, as of the last committed frame
	SpanView<Rc<FormInputListener>> getTabRing() const { return _tabRing; }

	// The field holding focus as of the last commit
	FormInputListener *getFocusedField() const { return _focusedField.get(); }

	// The field a focus change was requested for but not yet committed, if any
	FormInputListener *getPendingField() const;

	// Indices into getTabRing(), or maxOf<size_t>()
	size_t getFocusedIndex() const;
	size_t getPendingIndex() const;

	// Step the tab ring, wrapping around. The anchor is the pending field, else `from`, else the
	// focused field
	virtual bool focusNext(bool backwards, FormInputListener *from = nullptr);

	virtual bool focusField(NotNull<FormInputListener>);

	// --- the default button ---------------------------------------------------------------------

	/* The first FormFieldRole::Submit field in the tab ring, marked `:default`; fired by Enter from
	a field that does not consume it. Null when no submit button is reachable. */
	FormInputListener *getDefaultButton() const { return _defaultButton.get(); }

	// Activate the default button, or submit when there is none. Called by a field that received
	// Enter and had nothing of its own to do with it.
	virtual bool activateDefault();

	/* Whether the pending focus change should show `:focus-visible`: true for focusNext() and for
	the first rejected field in submit(), false for focusField(). Applies only inside a form. */
	bool isFocusVisible() const { return _focusVisible; }

protected:
	virtual void updateWithListeners(SpanView<InputListener *>) override;

	// `listener`'s owner is the focused field's node, or a descendant of it
	bool isWithinFocusedField(NotNull<InputListener>) const;

	size_t indexOfField(const FormInputListener *) const;

	// Pick the default button out of the freshly built ring and move the `:default` bit onto it
	void updateDefaultButton();

	// Writes `value` under `name`, splitting the name on '.' in Nested mode
	static void writeValue(Value &target, StringView name, Value &&, FormValueMode);

	// Reads `name` back out of a value written that way; Value::Null when it is not there
	static const Value &readValue(const Value &source, StringView name, FormValueMode);

	FormValueMode _valueMode = FormValueMode::Flat;
	SubmitCallback _submitCallback;
	ResetCallback _resetCallback;
	InvalidCallback _invalidCallback;

	// Registration order, for lookup; tab order comes from _tabRing
	Vector<FormInputListener *> _fields;

	Vector<Rc<FormInputListener>> _tabRing;

	// Rc: the field may be destroyed between commits, and its focus-out must reach a live object
	Rc<FormInputListener> _focusedField;

	// Recomputed with the ring; carries the `:default` bit
	Rc<FormInputListener> _defaultButton;

	// Direction of the pending navigation, kept until the commit applies it. Cleared by
	// focusField() and by that commit
	bool _navigateBackwards = false;

	// Focus-visible state of the pending change, read at commit like _navigateBackwards
	bool _focusVisible = false;

	bool _resetOnEscape = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMSYSTEM_H_
