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

	// A ui::NumberField holds a number, not text; an overload would be ambiguous via NotNull<>
	if (auto number = dynamic_cast<NumberField *>(input.get())) {
		// Integer field collects an integer, so 7 is not submitted as 7.0
		slots.collect = [number] {
			return number->isInteger() ? Value(int64_t(number->getValue()))
									   : Value(number->getValue());
		};
		// silent: assigning a form's value must not fire the change callback
		slots.assign = [number](const Value &v) { number->setValue(v.getDouble(), true); };
		slots.clear = [number] { number->setValue(0.0, true); };
	} else {
		slots.collect = [input = input.get()] { return Value(input->getText()); };
		slots.assign = [input = input.get()](const Value &v) { input->setText(v.getString()); };
		slots.clear = [input = input.get()] { input->setText(StringView()); };
	}
	// Direction is ignored: the caret stays where it was left
	slots.setFocused = [input = input.get()](bool value, bool) {
		if (value) {
			input->focus();
		} else {
			input->blur();
		}
	};

	// No `activate`: Enter in a single-line field submits the form
	slots.copy = [input = input.get()] { return input->copy(); };
	slots.cut = [input = input.get()] { return input->cut(); };
	slots.paste = [input = input.get()] { return input->paste(); };
	slots.selectAll = [input = input.get()] {
		input->selectAll();
		return true;
	};

	// TextInput drives the focus counter from the IME echo; the listener must not write it too
	slots.ownsFocusStyle = true;
	slots.focusable = input->isEnabled() && !input->isReadOnly();

	auto listener = FormAdapters_attach(input, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// The widget receives Tab before this listener and hands it over instead of blur()
	input->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	return listener;
}

FormInputListener *addFormField(NotNull<Checkbox> checkbox, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	slots.collect = [checkbox = checkbox.get()] { return Value(checkbox->isChecked()); };

	// silent: assigning a form's value must not fire the change callback
	slots.assign = [checkbox = checkbox.get()](
						   const Value &v) { checkbox->setChecked(v.getBool(), true); };
	slots.clear = [checkbox = checkbox.get()] { checkbox->setChecked(false, true); };

	slots.activate = [checkbox = checkbox.get()] {
		if (!checkbox->isEnabled()) {
			return false;
		}
		// Not silent: this is a user toggle from the keyboard
		checkbox->setChecked(!checkbox->isChecked());
		return true;
	};

	// A checkbox has no notion of focus of its own, so the listener paints `:focus` for it
	slots.ownsFocusStyle = false;
	slots.focusable = checkbox->isEnabled();

	return FormAdapters_attach(checkbox, sp::move(slots), name, FormFieldRole::Field, flags);
}

FormInputListener *addFormField(NotNull<Select> select, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// The id, not the (possibly localized) title
	slots.collect = [select = select.get()] { return Value(select->getValue()); };

	// silent: assigning a form's value must not fire the change callback
	slots.assign = [select = select.get()](
						   const Value &v) { select->setValue(v.getString(), true); };
	slots.clear = [select = select.get()] { select->setValue(StringView(), true); };

	// Enter or Space on the focused control opens the list
	slots.activate = [select = select.get()] { return select->open(); };

	// The widget writes the focus counter itself: the same flag gates its arrow handling
	slots.ownsFocusStyle = true;
	slots.focusable = select->isEnabled();

	slots.setFocused = [select = select.get()](bool value, bool) {
		if (value) {
			select->focus();
		} else {
			select->blur();
		}
	};

	return FormAdapters_attach(select, sp::move(slots), name, FormFieldRole::Field, flags);
}

FormInputListener *addFormField(NotNull<SearchPicker> picker, StringView name,
		FormFieldFlags flags) {
	FormFieldSlots slots;

	// The id, not the (possibly localized) title
	slots.collect = [picker = picker.get()] { return Value(picker->getValue()); };

	/* A value with no known title is shown as the id itself (e.g. an undeclared subtype hash). */
	slots.assign = [picker = picker.get()](const Value &v) {
		auto id = v.getString();
		picker->setValue(id, id, true);
	};
	slots.clear = [picker = picker.get()] { picker->setValue(StringView(), StringView(), true); };

	slots.activate = [picker = picker.get()] { return picker->open(); };

	slots.ownsFocusStyle = true;
	slots.focusable = picker->isEnabled();

	slots.setFocused = [picker = picker.get()](bool value, bool) {
		if (value) {
			picker->focus();
		} else {
			picker->blur();
		}
	};

	return FormAdapters_attach(picker, sp::move(slots), name, FormFieldRole::Field, flags);
}

FormInputListener *addFormField(NotNull<VectorField> field, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// One array, not one key per component; integers in an integer row
	slots.collect = [field = field.get()] {
		Value ret;
		const bool integer = field->isInteger();
		for (auto &it : field->getValue()) {
			if (integer) {
				ret.addInteger(int64_t(it));
			} else {
				ret.addDouble(it);
			}
		}
		return ret;
	};

	// silent. The widget refuses an array whose length does not match the arity and keeps its value
	slots.assign = [field = field.get()](const Value &v) {
		Vector<double> values;
		values.reserve(v.size());
		for (auto &it : v.asArray()) { values.emplace_back(it.getDouble()); }
		field->setValue(values, true);
	};

	slots.clear = [field = field.get()] {
		Vector<double> values;
		values.resize(field->getArity(), 0.0);
		field->setValue(values, true);
	};

	// The row picks the component by direction: Shift+Tab enters at the last one
	slots.setFocused = [field = field.get()](bool value, bool backwards) {
		if (value) {
			field->focusFromNavigation(backwards);
		} else {
			field->blur();
		}
	};

	// The components are ui::TextInputs and handle their own editing keys; the row has no caret
	slots.ownsFocusStyle = true;
	slots.focusable = field->isEnabled();

	auto listener = FormAdapters_attach(field, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// Tab off either end of the row moves between fields instead of blur()
	field->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	// A tap in a component must move the form's focus here, or keys keep going to the previous
	// field. Wired here because input/ does not depend on forms/
	field->setFocusCallback([listener](int32_t component) {
		if (component >= 0) {
			listener->setFocused();
		}
	});

	return listener;
}

FormInputListener *addFormField(NotNull<ColorField> field, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// Hex text, not four numbers: the stored form, stable through JSON
	slots.collect = [field = field.get()] { return Value(field->formatValue()); };

	// silent. An unparsable string leaves the field unchanged
	slots.assign = [field = field.get()](
						   const Value &v) { field->setValueFromString(v.getString(), true); };
	slots.clear = [field = field.get()] { field->setValue(Color4B(0, 0, 0, 255), true); };

	// Enter or Space on the focused field opens the picker, like a tap on the swatch
	slots.activate = [field = field.get()] { return field->open(); };

	slots.setFocused = [field = field.get()](bool value, bool) {
		if (value) {
			field->focus();
		} else {
			field->blur();
		}
	};

	// The hex line is a ui::TextInput and writes the focus counter from the IME echo
	slots.ownsFocusStyle = true;
	slots.focusable = field->isEnabled();

	auto listener = FormAdapters_attach(field, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	field->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	// A tap in the hex line must move the form's focus here, as in ui::VectorField
	field->setFocusCallback([listener](bool focused) {
		if (focused) {
			listener->setFocused();
		}
	});

	return listener;
}

FormInputListener *addFormField(NotNull<ChipRow> row, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// The ids in display order; order is significant, so this is an array, not a set
	slots.collect = [row = row.get()] {
		Value ret;
		for (auto &it : row->getItems()) { ret.addString(it.id); }
		return ret;
	};

	/* An undeclared id still produces a chip titled by the id, so nothing assigned is lost. */
	slots.assign = [row = row.get()](const Value &v) {
		Vector<ChipItem> items;
		items.reserve(v.size());
		for (auto &it : v.asArray()) {
			auto id = it.getString();
			if (id.empty()) {
				continue;
			}
			if (auto option = row->getOption(id)) {
				items.emplace_back(ChipItem{option->id, option->title, option->icon, true});
			} else {
				items.emplace_back(ChipItem{String(id), String(), IconName::None, true});
			}
		}
		// silent: assigning a form's value must not fire the change callback
		row->setItems(items, true);
	};

	slots.clear = [row = row.get()] { row->clearItems(true); };

	// Enter or Space on the focused row opens the list
	slots.activate = [row = row.get()] { return row->open(); };

	// The widget writes the focus counter itself: the same flag gates its arrow handling
	slots.ownsFocusStyle = true;
	slots.focusable = row->isEnabled();

	// The row picks the chip by direction: Shift+Tab enters at the last one
	slots.setFocused = [row = row.get()](bool value, bool backwards) {
		if (value) {
			row->focusFromNavigation(backwards);
		} else {
			row->blur();
		}
	};

	auto listener = FormAdapters_attach(row, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// The row is one stop of the tab ring: Tab is handed to the form instead of blur()
	row->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	// A tap on a chip must move the form's focus here, or keys keep going to the previous field
	row->setFocusCallback([listener](bool focused) {
		if (focused) {
			listener->setFocused();
		}
	});

	return listener;
}

FormInputListener *addFormField(NotNull<Slider> slider, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// The value the notch stands for; integer or double as the widget declares
	slots.collect = [slider = slider.get()] {
		return slider->isInteger() ? Value(int64_t(slider->getValue())) : Value(slider->getValue());
	};

	// silent. A value between two notches snaps to the nearer one
	slots.assign = [slider = slider.get()](
						   const Value &v) { slider->setValue(v.getDouble(), true); };

	// Clears to the minimum, not 0.0, which may lie outside the scale
	slots.clear = [slider = slider.get()] { slider->setIndex(0, true); };

	// No `activate`: Enter on a focused slider submits the form.

	// The widget writes the focus counter itself: the same flag gates its arrow handling
	slots.ownsFocusStyle = true;
	slots.focusable = slider->isEnabled();

	// Direction is ignored: a slider has one point of entry
	slots.setFocused = [slider = slider.get()](bool value, bool) {
		if (value) {
			slider->focus();
		} else {
			slider->blur();
		}
	};

	auto listener = FormAdapters_attach(slider, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// A tap on the track must move the form's focus here, or keys keep going to the previous field
	slider->setFocusCallback([listener](bool focused) {
		if (focused) {
			listener->setFocused();
		}
	});

	// No setNavigateCallback: a slider does not consume Tab, the form handles it directly.

	return listener;
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
