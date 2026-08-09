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

// The form: a system on whatever node the form is rooted at, and the focus group for its fields.
//
// IT IS A FocusGroup ON PURPOSE. A form cannot work without one - the group is what arbitrates
// who gets the keyboard and what order Tab walks - and a focus group with form semantics is not
// useful to anything else. Keeping them apart meant the form had to create, own and forward to a
// second system, and left room for one to exist without the other.
//
// It collects from, clears, validates and navigates the FormInputListeners in its subtree. It
// never touches a widget directly - only through the listeners' slots - so a form works the same
// whether its fields are ui::TextInputs or something an application wrote itself.
//
// Nesting is by construction: a field joins the nearest form above it, and a nested form's fields
// join the nested group, so neither form sees the other's.
//
// EVERYTHING PUBLIC HERE HAPPENS OUTSIDE A VISIT - a key press, collect(), submit(). That is why
// fields find their form by walking the parent chain rather than through the frame stack, which is
// only alive while a node is being visited.
//
// TWO THINGS THE BASE FocusGroup CANNOT DO, and why this overrides them:
//
// First, focus has to be per WIDGET, not per listener. FocusGroup::canHandleEventWithListener
// filters by listener id, and every InputListener in the subtree joins the nearest group - so with
// plain SingleFocus, the moment a field's FormInputListener takes focus, ui::TextInput's own
// listener stops receiving keys and the arrows, Home/End, Shift-selection and Ctrl+A all die
// inside the focused field. Here a listener passes when its owner IS the focused field's node or
// sits below it.
//
// Second, when the base class loses its focused listener it falls back to listeners.front(), and
// after InputListenerStorage::sort() that is whichever listener has the highest priority - for a
// form full of text inputs, one of their priority-1 blur-on-outside-tap listeners, which is not a
// field at all. Here the fallback can only ever land on a real field.
//
// THE TAB RING. The vector the dispatcher hands updateWithListeners is already sorted by priority
// DESC, then visit order DESC. Every FormInputListener keeps InputListener priority 0, so among
// themselves reversing that vector yields document order - one entry per field NODE, because only
// FormInputListeners are taken and a widget carries exactly one. The things that should not be in
// the ring are already gone, for free: a display:none, visibility:hidden or invisible subtree is
// never visited, so its listeners never registered; a disabled listener does not register either;
// and a nested form's fields joined that form's own group instead of this one.
//
// This group is deliberately NOT Exclusive. An exclusive group makes the dispatcher re-collect
// listeners scoped to it, which would cut the form off from everything outside - and an outer
// exclusive group (basic2d::OverlayLayout installs one) already needs Flags::Propagate for a form
// nested inside it to receive input at all.
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

	// Added to a rejected field's node by submit(). The engine's CSS subset has no `:invalid`, so
	// a style class is the only way to paint one
	virtual void setInvalidStyleClass(StringView);
	virtual StringView getInvalidStyleClass() const { return _invalidClass; }

	// Escape on a focused field resets the form. Off by default: losing what was typed to a
	// stray Escape is worse than having to reach for the button
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

	// The field that currently HOLDS focus - committed, i.e. as of the last frame
	FormInputListener *getFocusedField() const { return _focusedField.get(); }

	// The field a focus change has been REQUESTED for and not yet committed, if any. This is the
	// difference between what the form has been told to do and what has actually happened
	FormInputListener *getPendingField() const;

	// Indices into getTabRing(), or maxOf<size_t>()
	size_t getFocusedIndex() const;
	size_t getPendingIndex() const;

	// Step the tab ring, wrapping around.
	//
	// `from` is the field the request came from - normally the one that received the Tab. It is
	// only the anchor when nothing is pending; see the implementation for why a pending request
	// has to win, and what goes wrong when it does not.
	virtual bool focusNext(bool backwards, FormInputListener *from = nullptr);

	virtual bool focusField(NotNull<FormInputListener>);

protected:
	virtual void updateWithListeners(SpanView<InputListener *>) override;

	// `listener`'s owner is the focused field's node, or a descendant of it
	bool isWithinFocusedField(NotNull<InputListener>) const;

	size_t indexOfField(const FormInputListener *) const;

	// Writes `value` under `name`, splitting the name on '.' in Nested mode
	static void writeValue(Value &target, StringView name, Value &&, FormValueMode);

	// Reads `name` back out of a value written that way; Value::Null when it is not there
	static const Value &readValue(const Value &source, StringView name, FormValueMode);

	FormValueMode _valueMode = FormValueMode::Flat;
	String _invalidClass = String("invalid");
	SubmitCallback _submitCallback;
	ResetCallback _resetCallback;
	InvalidCallback _invalidCallback;

	// Lookup and registration order. The tab order is NOT read from here - it comes from the ring
	// below, which is in document order and already excludes what is hidden or disabled
	Vector<FormInputListener *> _fields;

	Vector<Rc<FormInputListener>> _tabRing;

	// Held by Rc, not raw: the focused field can be destroyed between two commits (its node was
	// removed), and the focus-out that follows has to reach a live object
	Rc<FormInputListener> _focusedField;

	bool _resetOnEscape = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMSYSTEM_H_
