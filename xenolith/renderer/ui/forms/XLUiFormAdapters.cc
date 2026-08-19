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

	/* What a field HOLDS, which for a ui::NumberField is a number and not the text of one. The
	branch is here rather than in a second overload because NotNull<> converts from either type and
	the two would be ambiguous at every call site - and this file is already the one place where
	the form machinery knows what a widget is. */
	if (auto number = dynamic_cast<NumberField *>(input.get())) {
		// An integer field collects an integer and a real one a double: a form that submits 7.0
		// where the schema says 7 has changed the value on its way out.
		slots.collect = [number] {
			return number->isInteger() ? Value(int64_t(number->getValue()))
									   : Value(number->getValue());
		};
		// silent: the form assigning its value is not somebody editing the field
		slots.assign = [number](const Value &v) { number->setValue(v.getDouble(), true); };
		slots.clear = [number] { number->setValue(0.0, true); };
	} else {
		slots.collect = [input = input.get()] { return Value(input->getText()); };
		slots.assign = [input = input.get()](const Value &v) { input->setText(v.getString()); };
		slots.clear = [input = input.get()] { input->setText(StringView()); };
	}
	// The direction is of no interest to a field with one caret: it enters at whichever end the
	// caret was left at, whichever way the Tab went
	slots.setFocused = [input = input.get()](bool value, bool) {
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

FormInputListener *addFormField(NotNull<Checkbox> checkbox, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	slots.collect = [checkbox = checkbox.get()] { return Value(checkbox->isChecked()); };

	// silent: assigning a form's value is not the user toggling the box, and a change callback
	// fired here would look like one
	slots.assign = [checkbox = checkbox.get()](
						   const Value &v) { checkbox->setChecked(v.getBool(), true); };
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

FormInputListener *addFormField(NotNull<Select> select, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// The id, not the title: the title is what a person reads and may be localized, the id is what
	// the value MEANS.
	slots.collect = [select = select.get()] { return Value(select->getValue()); };

	// silent: the form assigning its value is not the user picking an option, and a change callback
	// fired here would look like one
	slots.assign = [select = select.get()](
						   const Value &v) { select->setValue(v.getString(), true); };
	slots.clear = [select = select.get()] { select->setValue(StringView(), true); };

	// Enter or Space on the focused control shows the list - the same thing the widget does with
	// those keys on its own, routed here so the form does not have to know that
	slots.activate = [select = select.get()] { return select->open(); };

	// The widget writes the focus counter itself: its focus is also what decides whether it answers
	// the arrows at all, so the two must be the same flag rather than two that agree
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

	// The id, not the title - the same split as the Select adapter's, and for the same reason: the
	// title is what a person reads and may be localized, the id is what the value MEANS.
	slots.collect = [picker = picker.get()] { return Value(picker->getValue()); };

	/* Assigning a value the picker cannot resolve to a title shows the id itself.

	That is deliberate: a subtype stored as a hash by a file written before anyone declared a name
	for it has no title to show, and inventing one would be a lie about the file. */
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

	// One array, not one key per component - the whole reason this widget exists. Integers in an
	// integer row, for the same reason the ui::NumberField branch above collects one: a form that
	// submits 7.0 where the schema says 7 has changed the value on its way out.
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

	// silent: the form assigning its value is not somebody editing the row. A length that does not
	// match the arity is refused by the widget and nothing moves - assigning half a vector would
	// describe something other than what was asked for
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

	// The row decides WHICH component the focus lands on, and it needs the direction to do it:
	// Shift+Tab entering a row of numbers means its last field
	slots.setFocused = [field = field.get()](bool value, bool backwards) {
		if (value) {
			field->focusFromNavigation(backwards);
		} else {
			field->blur();
		}
	};

	// The components are ui::TextInputs: their own editing keys are theirs, and the row has no
	// caret of its own to copy from
	slots.ownsFocusStyle = true;
	slots.focusable = field->isEnabled();

	auto listener = FormAdapters_attach(field, sp::move(slots), name, FormFieldRole::Field, flags);
	if (!listener) {
		return nullptr;
	}

	// Tab off either end of the row is navigation between FIELDS, and the widget hands it here
	// rather than falling back to its standalone blur()
	field->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	// A tap that puts the caret in a component has to move the form's focus to this field, or the
	// form goes on filtering keys to the field it focused last and the arrows die in the component
	// the user just clicked. The widget cannot ask for this itself: forms/ knows about input/, and
	// never the other way round
	field->setFocusCallback([listener](int32_t component) {
		if (component >= 0) {
			listener->setFocused();
		}
	});

	return listener;
}

FormInputListener *addFormField(NotNull<ColorField> field, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// Hex text, not four numbers: it is what the value is written as everywhere it is stored, and
	// it survives a round trip through JSON unchanged.
	slots.collect = [field = field.get()] { return Value(field->formatValue()); };

	// silent: the form assigning its value is not somebody picking a colour. A string the colour
	// reader refuses leaves the field exactly as it was.
	slots.assign = [field = field.get()](
						   const Value &v) { field->setValueFromString(v.getString(), true); };
	slots.clear = [field = field.get()] { field->setValue(Color4B(0, 0, 0, 255), true); };

	// Enter or Space on the focused field shows the picker - the same thing a tap on its swatch
	// does, routed here so the form does not have to know that
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

	// A tap in the hex line has to move the form's focus to this field, or the form goes on
	// filtering keys to the field it focused last - the same seam ui::VectorField needs, and for
	// the same reason it cannot ask for it itself
	field->setFocusCallback([listener](bool focused) {
		if (focused) {
			listener->setFocused();
		}
	});

	return listener;
}

FormInputListener *addFormField(NotNull<ChipRow> row, StringView name, FormFieldFlags flags) {
	FormFieldSlots slots;

	// The ids, in the order they stand in. The titles are presentation and may be localized out
	// from under the value; the ORDER is not - an element chain read back reordered is a different
	// type - so this is an array and never a set.
	slots.collect = [row = row.get()] {
		Value ret;
		for (auto &it : row->getItems()) { ret.addString(it.id); }
		return ret;
	};

	/* Assigning an id nothing declares still produces a chip, titled by the id itself.

	That is deliberate, and it is the same decision the ui::SearchPicker adapter makes: a file
	written before anyone declared a name for that member has no title to show, and inventing one
	would be a lie about the file. Dropping it would be worse still - the form would silently
	collect back less than it was given. */
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
		// silent: the form assigning its value is not somebody building the set
		row->setItems(items, true);
	};

	slots.clear = [row = row.get()] { row->clearItems(true); };

	// Enter or Space on the focused row shows the list - the same thing the widget does with those
	// keys on its own, routed here so the form does not have to know that
	slots.activate = [row = row.get()] { return row->open(); };

	// The widget writes the focus counter itself: its focus is also what decides whether it answers
	// the arrows at all, so the two must be the same flag rather than two that agree
	slots.ownsFocusStyle = true;
	slots.focusable = row->isEnabled();

	// The row decides WHICH chip the focus lands on, and it needs the direction to do it: a
	// Shift+Tab entering a row of chips means its last one
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

	// Tab is not navigation INSIDE this widget - the row is one stop of the ring - so it is handed
	// straight over rather than falling back to the standalone blur()
	row->setNavigateCallback(
			[listener](bool backwards) { return listener->requestNavigate(backwards); });

	// A tap that selects a chip has to move the form's focus to this field, or the form goes on
	// filtering keys to the field it focused last and the arrows die in the row the user clicked
	row->setFocusCallback([listener](bool focused) {
		if (focused) {
			listener->setFocused();
		}
	});

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
