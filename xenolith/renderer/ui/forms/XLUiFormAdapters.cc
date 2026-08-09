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

#include "XLUiFormAdapters.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static FormInputListener *FormAdapters_attach(Node *node, FormFieldSlots &&slots, StringView name,
		FormFieldRole role, FormFieldFlags flags) {
	auto listener = node->addSystem(Rc<FormInputListener>::create(name, role));
	if (!listener) {
		return nullptr;
	}
	listener->setFieldFlags(flags);
	listener->setSlots(sp::move(slots));
	return listener;
}

FormInputListener *addFormField(NotNull<TextInput> input, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	slots.collect = [input = input.get()] { return Value(input->getText()); };
	slots.assign = [input = input.get()](const Value &v) { input->setText(v.getString()); };
	slots.clear = [input = input.get()] { input->setText(StringView()); };
	slots.setFocused = [input = input.get()](bool value) {
		if (value) {
			input->focus();
		} else {
			input->blur();
		}
	};

	// No `activate`: a single-line field has nothing to do with Enter, so declining is what lets
	// the form submit instead
	slots.copy = [input = input.get()] { return input->copy(); };
	slots.cut = [input = input.get()] { return input->cut(); };
	slots.paste = [input = input.get()] { return input->paste(); };
	slots.selectAll = [input = input.get()] {
		input->selectAll();
		return true;
	};

	// TextInput drives InteractiveComponent's focus counter from the IME echo, which is the only
	// moment it is actually true. The listener must not write it too
	slots.ownsFocusStyle = true;
	slots.focusable = input->isEnabled() && !input->isReadOnly();

	auto listener = FormAdapters_attach(input, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// Tab arrives at the widget first (it is dispatched before this listener), so the widget has
	// to hand it over rather than fall back to its standalone blur()
	input->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	return listener;
}

FormInputListener *addFormField(NotNull<Checkbox> checkbox, StringView name,
		FormFieldFlags flags) {
	FormFieldSlots slots;

	slots.collect = [checkbox = checkbox.get()] { return Value(checkbox->isChecked()); };

	// silent: assigning a form's value is not the user toggling the box, and a change callback
	// fired here would look like one
	slots.assign = [checkbox = checkbox.get()](const Value &v) {
		checkbox->setChecked(v.getBool(), true);
	};
	slots.clear = [checkbox = checkbox.get()] { checkbox->setChecked(false, true); };

	slots.activate = [checkbox = checkbox.get()] {
		if (!checkbox->isEnabled()) {
			return false;
		}
		// Not silent: this IS the user toggling it, just with the keyboard
		checkbox->setChecked(!checkbox->isChecked());
		return true;
	};

	// A checkbox has no notion of focus of its own, so the listener paints `:focus` for it
	slots.ownsFocusStyle = false;
	slots.focusable = checkbox->isEnabled();

	return FormAdapters_attach(checkbox, sp::move(slots), name, FormFieldRole::Field, flags);
}

FormInputListener *addFormButton(NotNull<Button> button, FormFieldRole role) {
	FormFieldSlots slots;

	// No collect/assign/clear: a button carries no value. Enter on it is routed by role in
	// FormInputListener::handleKey, so it needs no activate slot either
	slots.ownsFocusStyle = false;
	slots.focusable = true;

	return FormAdapters_attach(button, sp::move(slots), StringView(), role, FormFieldFlags::None);
}

FormInputListener *addFormField(NotNull<Node> node, FormFieldSlots &&slots, StringView name,
		FormFieldFlags flags) {
	return FormAdapters_attach(node, sp::move(slots), name, FormFieldRole::Field, flags);
}

} // namespace stappler::xenolith::ui
