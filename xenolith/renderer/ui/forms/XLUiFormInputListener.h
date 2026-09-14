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

// For isControlInvalid(), used by isInvalid() below
#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class FormSystem;

// One form field, attached to its widget node. An InputListener so it joins the focus group and
// binds keys for widgets without their own. Holds the field identity (name, role, flags,
// validator); acts on the widget only through FormFieldSlots (see XLUiFormAdapters.h).
// Dispatched after the widget's own listener, so it only sees keys the widget declined.
class SP_PUBLIC FormInputListener : public InputListener {
public:
	// Lower priority: visited before the widget's listener, dispatched after it
	static constexpr uint32_t SystemPriority = System::DefaultPriority - 16;

	// Return false to reject, writing an explanation into `message`
	using Validator = Function<bool(const Value &, String &message)>;

	virtual ~FormInputListener() = default;

	virtual bool init(StringView name = StringView(), FormFieldRole = FormFieldRole::Field);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Empty means the owner's name (its CSS id), resolved when the listener enters the scene
	virtual void setFieldName(StringView);
	virtual StringView getFieldName() const;

	virtual void setRole(FormFieldRole);
	virtual FormFieldRole getRole() const { return _role; }

	virtual void setFieldFlags(FormFieldFlags);
	virtual FormFieldFlags getFieldFlags() const { return _fieldFlags; }

	// Publish `:required` on the owner; called when the flags change and when the owner appears
	void updateRequiredState();

	// Publish `:focus-visible` on the owner; see the .cc for why a text field always takes it
	void updateFocusVisibleStyle(bool);

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
	bool isInvalid() const { return _owner && isControlInvalid(_owner); }

	// Requests to the form. False when there is no form, or nowhere to go
	virtual bool requestNavigate(bool backwards);
	virtual bool requestSubmit();
	virtual bool requestReset();

	// Called by FormSystem (the focus group) to run the protected focus hooks, which FocusGroup's
	// friendship with InputListener does not reach. `backwards` is passed to the slot via a member
	void applyFocus(bool value, FocusGroup *group, bool backwards = false);

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
	bool _focusStyleApplied = false;

	// Set by applyFocus for the focus hooks
	bool _focusBackwards = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_FORMS_XLUIFORMINPUTLISTENER_H_
