/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "widgets/FormLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiInteractiveComponent.h"
#include "XLUiTooltipSystem.h"
#include "XLUiEditLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// `.invalid` is the only way to paint a rejected field: the engine's CSS subset has no `:invalid`
// pseudo-class and no attribute selectors, so FormSystem marks the node with a style class.
static constexpr auto s_formCss = StringView(R"css(
text-input {
	width: 320px;
	height: 39px;
	background-color: #292929;
	outline-color: #3d3d3d;
	outline-width: 1px;
	border-radius: 7px;
	padding: 8px 12px;
	--caret-color: #fcb400;
	--selection-color: #7a5600;
}
text-input:focus {
	outline-color: #fcb400;
}
text-input.invalid {
	outline-color: #e53935;
}
checkbox {
	width: 20px;
	height: 20px;
	border-radius: 4px;
	background-color: #292929;
	outline-color: #3d3d3d;
	outline-width: 1px;
}
checkbox:focus {
	outline-color: #fcb400;
}
button {
	width: 120px;
	height: 36px;
	background-color: #3a3a3a;
	outline-color: #5a5a5a;
	outline-width: 1px;
	border-radius: 6px;
}
button:focus {
	outline-color: #fcb400;
}
label {
	color: #e8e8e8;
	font-size: 16px;
}
)css");

Value encodeInteractive(const Node *node) {
	Value ret;
	if (auto ic = node->getComponent<ui::InteractiveComponent>()) {
		ret.setBool(hasFlag(ic->state, ui::InteractiveState::Focus), "focus");
		ret.setBool(hasFlag(ic->state, ui::InteractiveState::Hover), "hover");
		ret.setBool(hasFlag(ic->state, ui::InteractiveState::Active), "active");

		// The raw counter, not just the flag: a listener that writes the counter twice for one
		// focus-in leaves the flag looking right and the counter stuck at 2
		ret.setInteger(int64_t(ic->focusCounter), "focusCounter");

		// What `:enabled` / `:disabled` and `:checked` actually select. Read off the component
		// rather than off the widget, because the whole point is whether the STYLE side can see it:
		// a node carrying no component reads as state 0, which is `:disabled` on something that is
		// not disabled.
		ret.setBool(hasFlag(ic->state, ui::InteractiveState::Enabled), "enabled");
		ret.setBool(hasFlag(ic->state, ui::InteractiveState::Checked), "checked");
		ret.setBool(true, "hasComponent");
	} else {
		ret.setBool(false, "hasComponent");
	}
	return ret;
}

// A field's WIDGET node, by the field name - which is also the node's name, and its CSS id.
// Resolved through the form rather than off a member, so it reaches every field the form knows.
static Node *FormLayout_fieldNode(ui::FormSystem *form, StringView name) {
	if (!form) {
		return nullptr;
	}
	for (auto &it : form->getFields()) {
		if (it->getFieldName() == name) {
			return it->getOwner();
		}
	}
	return nullptr;
}

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool FormLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_formCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_form = addSystem(Rc<ui::FormSystem>::create());
	_form->setSubmitCallback([this](Value &&value) {
		++_submitCallbacks;
		_lastSubmit = sp::move(value);
	});
	_form->setResetCallback([this] { ++_resetCallbacks; });
	_form->setInvalidCallback([this](SpanView<ui::FormValidationError> errors) {
		++_invalidCallbacks;
		_lastInvalid.clear();
		for (auto &it : errors) { _lastInvalid.emplace_back(it.name); }
	});

	// Explicit, distinct z-orders. Node::sortAllChildren sorts on z alone with an UNSTABLE sort, so
	// siblings sharing a z-order may be permuted on any reorder pass - and the tab ring is document
	// order, which is z-order. A test about traversal must not be a test about tie-breaking.
	_name = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_name->setName("name");
	_name->setPlaceholder("Name");
	_name->setCaretBlink(false);
	ui::addFormField(_name, StringView(), ui::FormFieldFlags::Required);

	_email = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_email->setName("email");
	_email->setPlaceholder("Email");
	_email->setCaretBlink(false);
	auto emailField = ui::addFormField(_email, StringView(), ui::FormFieldFlags::Required);
	emailField->setValidator([](const Value &value, String &message) {
		if (value.getString().find('@') == maxOf<size_t>()) {
			message = String("must contain @");
			return false;
		}
		return true;
	});

	_subscribe = addChild(Rc<ui::Checkbox>::create(), ZOrder(3));
	_subscribe->setName("subscribe");
	ui::addFormField(_subscribe);

	// Transient: focusable, but never collected and never validated
	_notes = addChild(Rc<ui::TextInput>::create(), ZOrder(4));
	_notes->setName("notes");
	_notes->setPlaceholder("Notes");
	_notes->setCaretBlink(false);
	ui::addFormField(_notes, StringView(), ui::FormFieldFlags::Transient);

	// A field inside a collapsible container. Collapsing the container must take the field out of
	// the tab ring without the form being told anything
	_hiddenBox = addChild(Rc<Node>::create(), ZOrder(5));
	_hiddenBox->setName("hidden-box");
	_hidden = _hiddenBox->addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_hidden->setName("hidden");
	_hidden->setCaretBlink(false);
	ui::addFormField(_hidden);

	_submit = addChild(Rc<ui::Button>::create(), ZOrder(6));
	_submit->setName("submit");
	_submit->setString("Submit");
	ui::addFormButton(_submit, ui::FormFieldRole::Submit);

	_reset = addChild(Rc<ui::Button>::create(), ZOrder(7));
	_reset->setName("reset");
	_reset->setString("Reset");
	ui::addFormButton(_reset, ui::FormFieldRole::Reset);

	return true;
}

void FormLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_name, _email, _subscribe, _notes, _hiddenBox, _submit, _reset};
	for (size_t i = 0; i < 7; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 56.0f));
	}

	if (_hiddenBox) {
		_hiddenBox->setContentSize(Size2(320.0f, 39.0f));
	}
	if (_hidden) {
		_hidden->setAnchorPoint(Vec2(0.0f, 1.0f));
		_hidden->setPosition(Vec2(0.0f, 39.0f));
	}
}

ui::FormInputListener *FormLayout::getField(const Value &args) const {
	return _form ? _form->getField(args.getString("field")) : nullptr;
}

ui::TextInput *FormLayout::getTextInput(StringView name) const {
	if (name == "name") {
		return _name;
	} else if (name == "email") {
		return _email;
	} else if (name == "notes") {
		return _notes;
	} else if (name == "hidden") {
		return _hidden;
	}
	return nullptr;
}

Value FormLayout::encodeTextInput(ui::TextInput *input) const {
	Value ret;
	if (!input) {
		ret.setString("unknown field", "error");
		return ret;
	}

	const auto &st = input->getInputState();
	ret.setString(input->getText(), "text");
	ret.setBool(input->isFocused(), "focused");
	ret.setInteger(int64_t(st.cursor.start), "cursorStart");
	ret.setInteger(int64_t(st.cursor.length), "cursorLength");
	if (auto style = input->getComponent<ui::TextInputStyleComponent>()) {
		Value color;
		color.setInteger(style->outlineColor.r, "r");
		color.setInteger(style->outlineColor.g, "g");
		color.setInteger(style->outlineColor.b, "b");
		color.setInteger(style->outlineColor.a, "a");
		ret.setValue(sp::move(color), "outlineColor");
	}
	return ret;
}

Value FormLayout::encodeState() const {
	Value ret;
	if (!_form) {
		ret.setString("no form", "error");
		return ret;
	}

	auto focused = _form->getFocusedField();
	ret.setString(focused ? focused->getFieldName() : StringView(), "focused");

	Value ring;
	for (auto &it : _form->getTabRing()) { ring.addString(it->getFieldName()); }
	ret.setValue(sp::move(ring), "tabRing");

	// What has been ASKED for but not yet committed - the difference is exactly what a Tab
	// arriving before the previous one committed has to step from
	auto pending = _form->getPendingField();
	ret.setString(pending ? pending->getFieldName() : StringView(), "pending");

	Value fields;
	for (auto &it : _form->getFields()) {
		Value field;
		field.setValue(it->collect(), "value");
		field.setBool(it->isInvalid(), "invalid");
		field.setBool(it->isFocusable(), "focusable");
		field.setBool(it->isEnabled(), "enabled");
		if (auto owner = it->getOwner()) {
			field.setBool(owner->hasStyleClass(_form->getInvalidStyleClass()), "invalidClass");
			field.setValue(encodeInteractive(owner), "interactive");

			field.setBool(ui::isEditLocked(owner), "locked");
			field.setString(ui::getEditLockReason(owner), "lockReason");
			field.setBool(owner->hasStyleClass("locked"), "lockedClass");
			field.setBool(owner->hasStyleClass("disabled"), "disabledClass");

			// The hint the lock installed, read back through the target it installed it on.
			if (auto tooltip = owner->getSystemByType<ui::TooltipTarget>()) {
				field.setString(tooltip->getText(), "tooltip");
			} else {
				field.setString(StringView(), "tooltip");
			}
		}
		fields.setValue(sp::move(field), it->getFieldName());
	}
	ret.setValue(sp::move(fields), "fields");

	ret.setInteger(int64_t(_submitCallbacks), "submitCount");
	ret.setInteger(int64_t(_resetCallbacks), "resetCount");
	ret.setInteger(int64_t(_invalidCallbacks), "invalidCount");
	ret.setValue(_lastSubmit, "lastSubmit");

	Value invalid;
	for (auto &it : _lastInvalid) { invalid.addString(it); }
	ret.setValue(sp::move(invalid), "lastInvalid");

	ret.setString(_form->getValueMode() == ui::FormValueMode::Nested ? "nested" : "flat",
			"valueMode");
	return ret;
}

// Mutating commands answer with a bare ack, for the same reason TextInputLayout's do - and one
// more: a focus change is applied by the focus group on the NEXT commit, so a snapshot taken here
// would always report the focus that is about to be replaced.
void FormLayout::registerCommands() {
	addCommand("state", "Report the form: focus, tab ring, per-field values and marks",
			[this](Value &&) { return encodeState(); });

	addCommand("field-state", "Report a text field's own state: {field}", [this](Value &&args) {
		return encodeTextInput(getTextInput(static_cast<const Value &>(args).getString("field")));
	});

	addCommand("collect", "Return FormSystem::collect() verbatim", [this](Value &&) {
		return _form ? _form->collect() : Value();
	});

	addCommand("assign", "Push a value into the form: {value}", [this](Value &&args) {
		if (_form) {
			_form->assign(static_cast<const Value &>(args).getValue("value"));
		}
		return ackValue(_form != nullptr);
	});

	addCommand("submit", "Validate and submit", [this](Value &&) {
		return ackValue(_form ? _form->submit() : false);
	});

	addCommand("reset", "Clear every field", [this](Value &&) {
		if (_form) {
			_form->reset();
		}
		return ackValue(_form != nullptr);
	});

	addCommand("focus", "Focus a field by name: {field}", [this](Value &&args) {
		auto field = getField(args);
		if (field && _form) {
			_form->focusField(field);
		}
		return ackValue(field != nullptr);
	});

	// Answers with the focus state as it stands RIGHT AFTER the step, on the same app-thread hop.
	// That is the only way to observe the pre-commit state: a second command would arrive over the
	// socket a frame later, by which time the swap has already happened.
	addCommand("focus-next", "Step the tab ring: {backwards}", [this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}

		Value ret;
		ret.setBool(_form->focusNext(static_cast<const Value &>(args).getBool("backwards")), "ok");

		auto focused = _form->getFocusedField();
		auto pending = _form->getPendingField();
		ret.setString(focused ? focused->getFieldName() : StringView(), "focused");
		ret.setString(pending ? pending->getFieldName() : StringView(), "pending");
		return ret;
	});

	addCommand("set-value-mode", "Switch collect() shape: {mode: flat|nested}",
			[this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}
		auto mode = static_cast<const Value &>(args).getString("mode");
		_form->setValueMode(mode == "nested" ? ui::FormValueMode::Nested : ui::FormValueMode::Flat);
		return ackValue(true);
	});

	addCommand("set-field-name", "Rename a field: {field, value}", [this](Value &&args) {
		auto field = getField(args);
		if (field) {
			field->setFieldName(static_cast<const Value &>(args).getString("value"));
		}
		return ackValue(field != nullptr);
	});

	addCommand("set-visible", "Collapse or restore the hidden field's container: {visible}",
			[this](Value &&args) {
		if (!_hiddenBox) {
			return ackValue(false);
		}
		_hiddenBox->setVisible(static_cast<const Value &>(args).getBool("visible"));
		return ackValue(true);
	});

	addCommand("set-locked",
			"Lock or unlock a field's WIDGET, with a reason: {field, locked, reason}",
			[this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		auto node = FormLayout_fieldNode(_form, in.getString("field"));
		if (!node) {
			return ackValue(false);
		}
		if (in.getBool("locked")) {
			ui::setEditLock(node, in.getString("reason"));
		} else {
			ui::clearEditLock(node);
		}
		return ackValue(true);
	});

	addCommand("set-widget-enabled",
			"Enable or disable a field's WIDGET - not its listener: {field, enabled}",
			[this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		auto node = FormLayout_fieldNode(_form, in.getString("field"));
		if (!node) {
			return ackValue(false);
		}
		if (auto target = dynamic_cast<ui::EditLockTarget *>(node)) {
			target->setEnabled(in.getBool("enabled"));
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("set-checked", "Check or uncheck the checkbox field: {field, checked}",
			[this](Value &&args) {
		const auto &in = static_cast<const Value &>(args);
		auto node = FormLayout_fieldNode(_form, in.getString("field"));
		auto checkbox = dynamic_cast<ui::Checkbox *>(node);
		if (!checkbox) {
			return ackValue(false);
		}
		checkbox->setChecked(in.getBool("checked"));
		return ackValue(true);
	});

	addCommand("set-enabled", "Enable or disable a field's listener: {field, enabled}",
			[this](Value &&args) {
		auto field = getField(args);
		if (field) {
			field->setEnabled(static_cast<const Value &>(args).getBool("enabled"));
		}
		return ackValue(field != nullptr);
	});

	addCommand("set-text", "Set a text field's text directly: {field, text}", [this](Value &&args) {
		const Value &req = args;
		auto name = req.getString("field");
		ui::TextInput *input = nullptr;
		if (name == "name") {
			input = _name;
		} else if (name == "email") {
			input = _email;
		} else if (name == "notes") {
			input = _notes;
		} else if (name == "hidden") {
			input = _hidden;
		}
		if (input) {
			input->setText(req.getString("text"));
		}
		return ackValue(input != nullptr);
	});

	addCommand("set-password", "Mask a text field: {field, password}", [this](Value &&args) {
		const Value &req = args;
		auto input = getTextInput(req.getString("field"));
		if (input) {
			input->setPasswordMode(req.getBool("password") ? ui::TextInputPasswordMode::ShowNone
														  : ui::TextInputPasswordMode::NotPassword);
		}
		return ackValue(input != nullptr);
	});

	addCommand("reset-counters", "Zero the submit/reset/invalid counters", [this](Value &&) {
		_submitCallbacks = 0;
		_resetCallbacks = 0;
		_invalidCallbacks = 0;
		_lastSubmit = Value();
		_lastInvalid.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
