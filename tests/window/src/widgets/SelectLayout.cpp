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

#include "widgets/SelectLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// The closed control and the surface its list opens on. The menu rules are repeated into the popup
// through MenuConfig::stylesheetSource, because a native popup is a scene of its own and this sheet
// does not reach it.
static constexpr auto s_selectCss = StringView(R"css(
select {
	width: 200px;
	height: 34px;
	background-color: #292929;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 6px;
}
select:hover {
	outline-color: rgba(255,255,255,.30);
}
select:focus {
	outline-color: #fcb400;
}
select.open {
	outline-color: #fcb400;
}
select:disabled {
	background-color: #202020;
}
select > label {
	color: #e8e8e8;
	font-size: 14px;
}
select > icon {
	width: 18px;
	height: 18px;
	color: #9a9aa4;
}
select > select-arrow {
	width: 18px;
	height: 18px;
	color: #9a9aa4;
}
text-input {
	width: 200px;
	height: 34px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 6px;
	padding: 0 10px;
	color: #e8e8e8;
	font-size: 14px;
}
label {
	color: #e8e8e8;
	font-size: 14px;
}
)css");

static constexpr auto s_selectPopupCss = StringView(R"css(
menu {
	background-color: #202026;
	border-radius: 6px;
	outline-color: #3d3d3d;
	outline-width: 1px;
}
menu-item {
	background-color: #00000000;
}
menu-item:hover {
	background-color: #2f2f38;
}
menu-item.highlighted {
	background-color: #3a3a5c;
}
menu-item:checked {
	background-color: #2a2a44;
}
menu-item > label {
	color: #e8e8e8;
	font-size: 14px;
}
)css");

// A plausible payload rather than "one two three": these are the VarTypes a schema editor has to
// offer, which is what this widget was built for.
struct OptionSpec {
	StringView id;
	StringView title;
	basic2d::IconName icon;
	bool enabled;
};

// clang-format off
static const OptionSpec s_options[] = {
	{StringView("bool"),   StringView("Bool"),   basic2d::IconName::Toggle_check_box_outline, true},
	{StringView("int"),    StringView("Int"),    basic2d::IconName::Editor_numbers_outline, true},
	{StringView("float"),  StringView("Float"),  basic2d::IconName::Editor_numbers_outline, true},
	{StringView("vec2"),   StringView("Vec2"),   basic2d::IconName::Editor_vertical_align_center_outline, true},
	{StringView("vec3"),   StringView("Vec3"),   basic2d::IconName::Editor_vertical_align_center_outline, true},
	{StringView("vec4"),   StringView("Vec4"),   basic2d::IconName::Editor_vertical_align_center_outline, true},
	{StringView("color"),  StringView("Color"),  basic2d::IconName::Editor_format_color_fill_outline, true},
	// No icon: the leading column has to stay lined up for the ones that do have one.
	{StringView("string"), StringView("String"), basic2d::IconName::None, true},
	{StringView("bytes"),  StringView("Bytes"),  basic2d::IconName::Hardware_memory_outline, true},
	// Disabled: a type no field may carry, so the list always has something for stepping and the
	// keyboard to skip over.
	{StringView("nil"),    StringView("Nil"),    basic2d::IconName::Action_info_outline, false},
	{StringView("array"),  StringView("Array"),  basic2d::IconName::Editor_data_array_outline, true},
	{StringView("map"),    StringView("Map"),    basic2d::IconName::Editor_data_object_outline, true},
	{StringView("enum"),   StringView("Enum"),   basic2d::IconName::Content_tag_outline, true},
};
// clang-format on

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool SelectLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_selectCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	Vector<ui::SelectOption> options;
	for (auto &it : s_options) {
		options.emplace_back(ui::SelectOption{it.id.str<Interface>(), it.title.str<Interface>(),
			it.icon, it.enabled});
	}

	auto makeSelect = [&](StringView name, ZOrder z) {
		auto select = addChild(Rc<ui::Select>::create(), z);
		select->setName(name);
		select->setPlaceholder("Type…");
		select->setOptions(options);

		ui::MenuConfig config;
		config.idPrefix = String("select-test");
		config.title = String("Select test");
		config.stylesheetSource = s_selectPopupCss.str<Interface>();
		select->setPopupConfig(sp::move(config));

		select->setChangeCallback([this, name = name.str<Interface>()](StringView id) {
			++_changes;
			_lastChange = id.str<Interface>();
			_changeLog.emplace_back(toString(name, ":", id));
		});
		return select;
	};

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// unstable.
	_select = makeSelect("select", ZOrder(1));
	_select->setValue("int");

	_neighbour = addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setText("abcdef");
	_neighbour->setCaretBlink(false);

	// The second one is a form field. Everything about "what a form sees" is checked here rather
	// than on the first, so the plain widget stays a widget with no form in sight.
	_form = addSystem(Rc<ui::FormSystem>::create());

	_formSelect = makeSelect("form-select", ZOrder(3));
	ui::addFormField(_formSelect);

	return true;
}

void SelectLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_select, _neighbour, _formSelect};
	for (size_t i = 0; i < 3; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 60.0f));
	}
}

ui::Select *SelectLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "form-select") {
		return _formSelect;
	}
	return _select;
}

Value SelectLayout::encodeSelect(ui::Select *select) const {
	Value ret;
	if (!select) {
		return ret;
	}

	ret.setString(select->getValue(), "value");
	ret.setBool(select->isOpen(), "open");
	ret.setBool(select->isEnabled(), "enabled");
	ret.setBool(select->isFocused(), "focused");
	ret.setInteger(int64_t(select->getOptions().size()), "optionCount");

	// What the closed face actually says - the assertion that the label follows the value.
	if (auto label = select->getLabel()) {
		ret.setString(string::toUtf8<Interface>(label->getString()), "label");
	}
	if (auto popup = select->getPopup()) {
		ret.setString(popup->getId(), "popupId");
		ret.setBool(popup->isNative(), "popupNative");
	}

	// The interactive flags CSS paints from, and the raw counter: a listener that writes the focus
	// counter twice leaves the flag looking right and the counter stuck at 2.
	if (auto ic = select->getComponent<InteractiveComponent>()) {
		ret.setBool(hasFlag(ic->state, InteractiveState::Focus), "focusFlag");
		ret.setInteger(int64_t(ic->focusCounter), "focusCounter");
	}
	return ret;
}

Value SelectLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeSelect(_select), "select");
	ret.setValue(encodeSelect(_formSelect), "formSelect");

	ret.setInteger(int64_t(_changes), "changes");
	ret.setString(_lastChange, "lastChange");

	Value log;
	for (auto &it : _changeLog) { log.addString(it); }
	ret.setValue(sp::move(log), "log");

	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}

	if (_form) {
		ret.setValue(_form->collect(), "collected");
	}

	Value options;
	if (_select) {
		for (auto &it : _select->getOptions()) {
			Value option;
			option.setString(it.id, "id");
			option.setString(it.title, "title");
			option.setBool(it.enabled, "enabled");
			options.addValue(sp::move(option));
		}
	}
	ret.setValue(sp::move(options), "options");
	return ret;
}

void SelectLayout::registerCommands() {
	addCommand("state", "Report both controls, the change log and what the form collects",
			[this](Value &&) { return encodeState(); });

	addCommand("open", "Open the list: {target}", [this](Value &&args) {
		auto select = getTarget(args);
		return ackValue(select && select->open());
	});

	addCommand("close", "Dismiss the list: {target}", [this](Value &&args) {
		if (auto select = getTarget(args)) {
			select->close();
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("set", "Choose an option by id: {target, value}", [this](Value &&args) {
		auto select = getTarget(args);
		return ackValue(
				select && select->setValue(static_cast<const Value &>(args).getString("value")));
	});

	addCommand("step", "Step the value: {target, delta}", [this](Value &&args) {
		auto select = getTarget(args);
		return ackValue(select
				&& select->step(int32_t(static_cast<const Value &>(args).getInteger("delta"))));
	});

	addCommand("focus", "Focus a control, or blur it: {target, value}", [this](Value &&args) {
		auto select = getTarget(args);
		if (!select) {
			return ackValue(false);
		}
		const Value &a = args;
		if (!a.isBool("value") || a.getBool("value")) {
			select->focus();
		} else {
			select->blur();
		}
		return ackValue(true);
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the controls",
			[this](Value &&) {
		if (_neighbour) {
			_neighbour->focus();
			return ackValue(true);
		}
		return ackValue(false);
	});

	addCommand("set-enabled", "Enable or disable a control: {target, value}", [this](Value &&args) {
		auto select = getTarget(args);
		if (!select) {
			return ackValue(false);
		}
		select->setEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("assign", "Assign the form's value: {value}", [this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}
		Value value;
		value.setString(static_cast<const Value &>(args).getString("value"), "form-select");
		_form->assign(value);
		return ackValue(true);
	});

	addCommand("set-string-options",
			"Repopulate a control from a plain list of names, id == title: {target, values}",
			[this](Value &&args) {
		auto select = getTarget(args);
		if (!select) {
			return ackValue(false);
		}
		Vector<String> names;
		for (auto &it : static_cast<const Value &>(args).getArray("values")) {
			names.emplace_back(it.getString());
		}
		// Through the Vector<String> overload on purpose: that is the spelling a schema's enum
		// family actually hands over, and the one a SpanView<StringView> could not have taken.
		select->setOptions(ui::makeSelectOptions(names));
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero the change log", [this](Value &&) {
		_changes = 0;
		_lastChange.clear();
		_changeLog.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
