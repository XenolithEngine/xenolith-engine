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

#ifndef XENOLITH_RENDERER_UI_FORMS_XLUIFORMINPUTLISTENER_H_
#define XENOLITH_RENDERER_UI_FORMS_XLUIFORMINPUTLISTENER_H_

#include "XLUiFormTypes.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class FormSystem;

// One form field, attached to the widget node it represents.
//
// It is an InputListener rather than a plain System because a field IS an input target: it has to
// be in the focus group to be reachable by Tab, and it has to bind keys for the widgets that bind
// none of their own (a Checkbox has no key handling, a Button has none either).
//
// WHAT THE NODE OWNS AND WHAT THIS OWNS. The listener knows the field's identity - name, role,
// required-ness, validator - and nothing about how the widget works. Everything it needs the
// widget to DO goes through FormFieldSlots, which the widget fills in. See XLUiFormAdapters.h for
// the ready-made fillers.
//
// DISPATCH ORDER. This listener sits at a lower system priority than the widget's own, which means
// it is visited first and therefore dispatched LAST on its node (the dispatcher walks the scene
// bucket in reverse). It is the fallback: it only ever sees the keys the widget declined. That is
// what makes ui::TextInput's Enter callback win over the form's submit without either of them
// knowing about the other.
class SP_PUBLIC FormInputListener : public InputListener {
public:
	// Below System::DefaultPriority so this listener is visited before the widget's own and
	// dispatched after it - see the class comment
	static constexpr uint32_t SystemPriority = System::DefaultPriority - 16;

	// Return false to reject, writing an explanation into `message`
	using Validator = Function<bool(const Value &, String &message)>;

	virtual ~FormInputListener() = default;

	virtual bool init(StringView name = StringView(), FormFieldRole = FormFieldRole::Field);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Empty means "take the owner's name", resolved when the listener enters the scene. A node's
	// name is also its CSS id, so a form field is named exactly once
	virtual void setFieldName(StringView);
	virtual StringView getFieldName() const;

	virtual void setRole(FormFieldRole);
	virtual FormFieldRole getRole() const { return _role; }

	virtual void setFieldFlags(FormFieldFlags);
	virtual FormFieldFlags getFieldFlags() const { return _fieldFlags; }

	// Runs after the Required check, and only on a value that passed it
	virtual void setValidator(Validator &&);

	virtual void setSlots(FormFieldSlots &&);
	virtual const FormFieldSlots &getSlots() const { return _slots; }

	FormSystem *getForm() const { return _form; }

	// In the tab ring: has a form, is enabled, is not a bare non-focusable widget
	virtual bool isFocusable() const;

	// Commands from the form
	virtual Value collect() const;
	virtual void assign(const Value &);
	virtual void clear();
	virtual bool validate(String &message) const;
	virtual bool activate();

	// Marks the owner node with the form's invalid style class, since the engine's CSS subset has
	// neither `:invalid` nor attribute selectors
	virtual void setInvalid(bool);
	virtual bool isInvalid() const { return _invalid; }

	// Requests to the form. False when there is no form, or nowhere to go
	virtual bool requestNavigate(bool backwards);
	virtual bool requestSubmit();
	virtual bool requestReset();

	// Entry point for FormSystem, which is this field's focus group. InputListener declares
	// FocusGroup a friend so it can reach handleFocusIn/handleFocusOut, and friendship does not
	// extend to a subclass - but a derived listener may always call its own protected hooks, so
	// the group asks the field to do it
	void applyFocus(bool value, FocusGroup *group);

protected:
	using InputListener::init;

	virtual bool handleFormHotkey(HotkeyId, const InputEvent &);

	virtual void handleFocusIn(FocusGroup *) override;
	virtual void handleFocusOut(FocusGroup *) override;

	// Writes InteractiveComponent's focus counter for a widget that does not write it itself
	void updateFocusStyle(bool);

	String _name;
	FormFieldRole _role = FormFieldRole::Field;
	FormFieldFlags _fieldFlags = FormFieldFlags::None;
	Validator _validator;
	FormFieldSlots _slots;
	FormSystem *_form = nullptr;
	bool _invalid = false;
	bool _focusStyleApplied = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMINPUTLISTENER_H_
