/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCommon.h" // IWYU pragma: keep

#include "form/FormFields.h"
#include "XLUiLayoutSystem.h"
#include "XLUiCodeEditor.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// What both popups open in-scene rather than as a window of their own. It is the ONE line that
// makes each of them an overlay, and it is spelled out at both call sites for that reason.
static constexpr bool s_preferNative = false;

struct GroupInfo {
	FieldGroup group;
	StringView name;
	StringView title;

	/* What the group's rows come to, in points - see getFieldGroupHeight in the header for why an
	accordion cannot work this out for itself.

	Each is the rows' own heights plus the 4-point gap between them plus the 16 points of padding
	`accordion-body` declares, rounded up to leave room for a font that measures a little taller
	than the one this was written against. The row heights are the stylesheet's: 36 for a text-like
	field (30 plus the row's 3+3 padding), 24 for a checkbox or a slider, 40 for the chip row and
	for the button row, 92 for one of the editors. */
	float height;
};

static constexpr GroupInfo s_groups[] = {
	// 4 x 36 + 3 x 4 + 16
	{FieldGroup::Text, StringView("text"), StringView("Text"), 176.0f},
	// 36 + 36 + 24 + 36 + 3 x 4 + 16
	{FieldGroup::Numbers, StringView("numbers"), StringView("Numbers"), 164.0f},
	// 24 + 36 + 36 + 40 + 3 x 4 + 16
	{FieldGroup::Choice, StringView("choice"), StringView("Choice"), 168.0f},
	// 2 x 36 + 4 + 16
	{FieldGroup::Color, StringView("color"), StringView("Colour"), 96.0f},
	// 2 x 92 + 4 + 16
	{FieldGroup::Editors, StringView("editors"), StringView("Editors (not fields)"), 208.0f},
	// 40 + 16
	{FieldGroup::Actions, StringView("actions"), StringView("Actions"), 60.0f},
};

static constexpr FieldGroup s_order[] = {
	FieldGroup::Text,
	FieldGroup::Numbers,
	FieldGroup::Choice,
	FieldGroup::Color,
	FieldGroup::Editors,
	FieldGroup::Actions,
};

// The names each group contributes, in build order. Declared beside the builder rather than
// derived from it, so the self-check compares a collect() against something a person WROTE - a
// check that reads its expectations out of the thing it is checking cannot fail.
static constexpr StringView s_textFields[] = {StringView("name"), StringView("email"),
	StringView("password"), StringView("notes")};
static constexpr StringView s_numberFields[] = {StringView("count"), StringView("ratio"),
	StringView("volume"), StringView("offset")};
static constexpr StringView s_choiceFields[] = {StringView("subscribe"), StringView("role"),
	StringView("country"), StringView("tags")};
static constexpr StringView s_colorFields[] = {StringView("accent"), StringView("overlay")};
static constexpr StringView s_actionFields[] = {StringView("submit"), StringView("reset")};

/* Everything a form here COLLECTS.

Not the concatenation of the lists above: `notes` is Transient and the two buttons are not fields,
so three of the names a form knows never appear in a collect(). Spelling the difference out is the
point - it is the one thing about ui::FormFieldFlags that a demo can actually show. */
static constexpr StringView s_collected[] = {StringView("name"), StringView("email"),
	StringView("password"), StringView("count"), StringView("ratio"), StringView("volume"),
	StringView("offset"), StringView("subscribe"), StringView("role"), StringView("country"),
	StringView("tags"), StringView("accent"), StringView("overlay")};

// ---- rows ---------------------------------------------------------------------------------

/* One labelled row: the caption, then the widget.

The row is a flex ROW inside the caller's flex COLUMN, and everything about its geometry is in the
stylesheet - this only builds the two nodes and gives them the classes the sheet addresses. The
caption is a plain basic2d::Label because a Label is the one node in the kit that measures itself,
which is what lets `.field-caption { flex: 0 0 var(--caption-w) }` line every row up without any
of them knowing how wide the others turned out. */
static Node *makeRow(NotNull<Node> parent, StringView caption, ZOrder z) {
	auto row = parent->addChild(Rc<Node>::create(), z);
	row->addStyleClass("field-row");

	auto label = row->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	label->addStyleClass("field-caption");
	label->setString(caption);

	return row;
}

// The widget goes at z 2, after the caption at z 1 - inside a flex row the child order IS the
// placement order, so this is what puts the control to the right of its name.
static constexpr ZOrder s_widgetZOrder = ZOrder(2);

// ---- the two popups' data -----------------------------------------------------------------

// The dropdown's list. Long enough that scrolling it is not the way to find anything - which is
// the whole argument for a ui::SearchPicker over a ui::Select, and it has to be visible here or
// the demo shows two widgets that look the same.
static Vector<ui::SearchItem> makeCountryItems() {
	struct Row {
		StringView id;
		StringView title;
		StringView region;
	};

	static constexpr Row rows[] = {
		{StringView("ar"), StringView("Argentina"), StringView("Americas")},
		{StringView("at"), StringView("Austria"), StringView("Europe")},
		{StringView("au"), StringView("Australia"), StringView("Oceania")},
		{StringView("be"), StringView("Belgium"), StringView("Europe")},
		{StringView("br"), StringView("Brazil"), StringView("Americas")},
		{StringView("ca"), StringView("Canada"), StringView("Americas")},
		{StringView("ch"), StringView("Switzerland"), StringView("Europe")},
		{StringView("cl"), StringView("Chile"), StringView("Americas")},
		{StringView("cn"), StringView("China"), StringView("Asia")},
		{StringView("cz"), StringView("Czechia"), StringView("Europe")},
		{StringView("de"), StringView("Germany"), StringView("Europe")},
		{StringView("dk"), StringView("Denmark"), StringView("Europe")},
		{StringView("eg"), StringView("Egypt"), StringView("Africa")},
		{StringView("es"), StringView("Spain"), StringView("Europe")},
		{StringView("fi"), StringView("Finland"), StringView("Europe")},
		{StringView("fr"), StringView("France"), StringView("Europe")},
		{StringView("gb"), StringView("United Kingdom"), StringView("Europe")},
		{StringView("gr"), StringView("Greece"), StringView("Europe")},
		{StringView("id"), StringView("Indonesia"), StringView("Asia")},
		{StringView("ie"), StringView("Ireland"), StringView("Europe")},
		{StringView("in"), StringView("India"), StringView("Asia")},
		{StringView("it"), StringView("Italy"), StringView("Europe")},
		{StringView("jp"), StringView("Japan"), StringView("Asia")},
		{StringView("ke"), StringView("Kenya"), StringView("Africa")},
		{StringView("kr"), StringView("Korea"), StringView("Asia")},
		{StringView("ma"), StringView("Morocco"), StringView("Africa")},
		{StringView("mx"), StringView("Mexico"), StringView("Americas")},
		{StringView("ng"), StringView("Nigeria"), StringView("Africa")},
		{StringView("nl"), StringView("Netherlands"), StringView("Europe")},
		{StringView("no"), StringView("Norway"), StringView("Europe")},
		{StringView("nz"), StringView("New Zealand"), StringView("Oceania")},
		{StringView("pe"), StringView("Peru"), StringView("Americas")},
		{StringView("ph"), StringView("Philippines"), StringView("Asia")},
		{StringView("pl"), StringView("Poland"), StringView("Europe")},
		{StringView("pt"), StringView("Portugal"), StringView("Europe")},
		{StringView("ru"), StringView("Russia"), StringView("Europe")},
		{StringView("se"), StringView("Sweden"), StringView("Europe")},
		{StringView("sg"), StringView("Singapore"), StringView("Asia")},
		{StringView("th"), StringView("Thailand"), StringView("Asia")},
		{StringView("tr"), StringView("Turkey"), StringView("Asia")},
		{StringView("ua"), StringView("Ukraine"), StringView("Europe")},
		{StringView("us"), StringView("United States"), StringView("Americas")},
		{StringView("vn"), StringView("Vietnam"), StringView("Asia")},
		{StringView("za"), StringView("South Africa"), StringView("Africa")},
	};

	Vector<ui::SearchItem> ret;
	ret.reserve(sizeof(rows) / sizeof(rows[0]));

	int64_t id = 1;
	for (auto &it : rows) {
		ui::SearchItem item;
		item.id = id++;
		item.title = it.title.str<Interface>();
		item.subtitle = it.region.str<Interface>();

		/* `data["id"]` is what the picker collects - SearchPicker::open reads exactly this key when
		a hit is activated - and `data["category"]` is what its grouped mode files a hit under with
		no callback of its own. Both are documented keys, not conventions of this demo. */
		item.data.setString(it.id, "id");
		item.data.setString(it.region, "category");
		ret.emplace_back(sp::move(item));
	}
	return ret;
}

// A theme's worth of swatches for the colour picker, so the grid shows something an application
// chose rather than the kit's default sixteen.
static constexpr Color4B s_palette[] = {
	Color4B(0x1E, 0x88, 0xE5, 0xFF),
	Color4B(0x43, 0xA0, 0x47, 0xFF),
	Color4B(0xFD, 0xD8, 0x35, 0xFF),
	Color4B(0xFB, 0x8C, 0x00, 0xFF),
	Color4B(0xE5, 0x39, 0x35, 0xFF),
	Color4B(0x8E, 0x24, 0xAA, 0xFF),
	Color4B(0x00, 0x89, 0x7B, 0xFF),
	Color4B(0x54, 0x6E, 0x7A, 0xFF),
	Color4B(0xFF, 0xFF, 0xFF, 0xFF),
	Color4B(0xBD, 0xBD, 0xBD, 0xFF),
	Color4B(0x75, 0x75, 0x75, 0xFF),
	Color4B(0x21, 0x21, 0x21, 0xFF),
};

} // namespace

StringView getFieldGroupName(FieldGroup group) {
	for (auto &it : s_groups) {
		if (it.group == group) {
			return it.name;
		}
	}
	return StringView();
}

StringView getFieldGroupTitle(FieldGroup group) {
	for (auto &it : s_groups) {
		if (it.group == group) {
			return it.title;
		}
	}
	return StringView();
}

bool readFieldGroup(StringView str, FieldGroup &out) {
	for (auto &it : s_groups) {
		if (it.name == str) {
			out = it.group;
			return true;
		}
	}
	return false;
}

SpanView<FieldGroup> getFieldGroups() { return SpanView<FieldGroup>(s_order); }

float getFieldGroupHeight(FieldGroup group) {
	for (auto &it : s_groups) {
		if (it.group == group) {
			return it.height;
		}
	}
	return 0.0f;
}

SpanView<StringView> getFieldNames(FieldGroup group) {
	switch (group) {
	case FieldGroup::Text: return SpanView<StringView>(s_textFields);
	case FieldGroup::Numbers: return SpanView<StringView>(s_numberFields);
	case FieldGroup::Choice: return SpanView<StringView>(s_choiceFields);
	case FieldGroup::Color: return SpanView<StringView>(s_colorFields);
	case FieldGroup::Editors: return SpanView<StringView>(); // see the header
	case FieldGroup::Actions: return SpanView<StringView>(s_actionFields);
	}
	return SpanView<StringView>();
}

SpanView<StringView> getAllFieldNames() { return SpanView<StringView>(s_collected); }

// ---- the groups ---------------------------------------------------------------------------

namespace {

// ui::TextInput in its three shapes, plus the one field a form deliberately does NOT collect.
static void buildTextGroup(NotNull<Node> parent, int32_t &z) {
	auto plain = makeRow(parent, "Name", ZOrder(z++))
						 ->addChild(Rc<ui::TextInput>::create(), s_widgetZOrder);
	plain->setName("name");
	plain->setPlaceholder("Ada Lovelace");
	plain->setCaretBlink(false);
	ui::addFormField(plain, StringView(), ui::FormFieldFlags::Required);

	auto email = makeRow(parent, "Email", ZOrder(z++))
						 ->addChild(Rc<ui::TextInput>::create(), s_widgetZOrder);
	email->setName("email");
	email->setPlaceholder("ada@example.org");
	email->setCaretBlink(false);
	auto emailField = ui::addFormField(email, StringView(), ui::FormFieldFlags::Required);

	// A validator runs AFTER the Required check, so it never has to answer for an empty value.
	emailField->setValidator([](const Value &value, String &message) {
		if (value.getString().find('@') == maxOf<size_t>()) {
			message = String("must contain @");
			return false;
		}
		return true;
	});

	auto password = makeRow(parent, "Password", ZOrder(z++))
							->addChild(Rc<ui::TextInput>::create(), s_widgetZOrder);
	password->setName("password");
	password->setPasswordMode(ui::TextInputPasswordMode::ShowNone);
	password->setPlaceholder("hunter2");
	password->setCaretBlink(false);
	ui::addFormField(password, StringView(), ui::FormFieldFlags::Required);

	/* TRANSIENT: in the tab ring, validated by nothing, and absent from collect().

	The one field whose behaviour cannot be seen by looking at it - which is why the demo has it,
	and why the self-check asserts its absence rather than trusting the screen. */
	auto notes = makeRow(parent, "Notes (transient)", ZOrder(z++))
						 ->addChild(Rc<ui::TextInput>::create(), s_widgetZOrder);
	notes->setName("notes");
	notes->setPlaceholder("not collected");
	notes->setCaretBlink(false);
	ui::addFormField(notes, StringView(), ui::FormFieldFlags::Transient);
}

// ui::NumberField in both arities of number, ui::Slider, and the composite ui::VectorField.
static void buildNumbersGroup(NotNull<Node> parent, int32_t &z) {
	auto count = makeRow(parent, "Count (int)", ZOrder(z++))
						 ->addChild(Rc<ui::NumberField>::create(), s_widgetZOrder);
	count->setName("count");
	count->setInteger(true);
	count->setRange(0.0, 99.0);
	count->setStep(1.0);
	count->setValue(3.0, true);
	count->setCaretBlink(false);
	ui::addFormField(count);

	auto ratio = makeRow(parent, "Ratio (real)", ZOrder(z++))
						 ->addChild(Rc<ui::NumberField>::create(), s_widgetZOrder);
	ratio->setName("ratio");
	ratio->setRange(0.0, 100.0);
	ratio->setStep(0.5);
	ratio->setUnit("%");
	// Drag-to-scrub: the field is dragged while UNFOCUSED, so it costs the widget no gesture that
	// typing in it would want.
	ratio->setDragEnabled(true);
	ratio->setValue(42.5, true);
	ratio->setCaretBlink(false);
	ui::addFormField(ratio);

	auto volume = makeRow(parent, "Volume", ZOrder(z++))
						  ->addChild(Rc<ui::Slider>::create(), s_widgetZOrder);
	volume->setName("volume");
	volume->setRange(0.0, 100.0, 5.0);
	volume->setInteger(true);
	volume->setValue(60.0, true);
	// The form collects `min + step * index`, not the index - so this reports 60, not 12.
	ui::addFormField(volume);

	auto offset = makeRow(parent, "Offset (vec3)", ZOrder(z++))
						  ->addChild(Rc<ui::VectorField>::create(3), s_widgetZOrder);
	offset->setName("offset");
	offset->setStep(0.1);
	offset->setDragEnabled(true);
	double values[3] = {0.0, 1.5, -2.0};
	offset->setValue(SpanView<double>(values, 3), true);
	// ONE field under one name, whose value is an array. The parts keep their own keys because the
	// form admits a listener sitting below the focused field's node.
	ui::addFormField(offset);
}

// The four ways of choosing something, including both in-scene popups.
static void buildChoiceGroup(NotNull<Node> parent, int32_t &z) {
	auto subscribe = makeRow(parent, "Subscribe", ZOrder(z++))
							 ->addChild(Rc<ui::Checkbox>::create(), s_widgetZOrder);
	subscribe->setName("subscribe");
	subscribe->setChecked(true, true);
	ui::addFormField(subscribe);

	auto role = makeRow(parent, "Role", ZOrder(z++))
						->addChild(Rc<ui::Select>::create(), s_widgetZOrder);
	role->setName("role");
	role->setOptions(ui::makeSelectOptions(SpanView<StringView>({StringView("Reader"),
		StringView("Author"), StringView("Editor"), StringView("Administrator")})));
	role->setPlaceholder("choose one");
	role->setValue("Author", true);
	{
		// A menu of four is chosen by LOOKING, and it opens in the scene like everything else here.
		ui::MenuConfig config;
		config.preferNative = s_preferNative;
		role->setPopupConfig(sp::move(config));
	}
	ui::addFormField(role);

	/* POPUP ONE: a list too long to be a menu, so it is chosen by TYPING.

	Everything about it is ui::SearchPicker's; the demo only supplies the rows and asks for the
	overlay path. `grouped` is what makes the empty query show categories instead of a ranked list
	of forty-four names in some order nobody chose. */
	auto country = makeRow(parent, "Country (search)", ZOrder(z++))
						   ->addChild(Rc<ui::SearchPicker>::create(), s_widgetZOrder);
	country->setName("country");
	{
		ui::SearchPickerConfig config;
		config.items = makeCountryItems();
		config.placeholder = String("Type to filter");
		config.title = String("Country");
		config.grouped = true;
		config.preferNative = s_preferNative;
		config.style.maxRows = 9;
		config.style.minWidth = 260.0f;
		country->setConfig(sp::move(config));
	}
	country->setPlaceholder("choose one");
	country->setValue("de", "Germany", true);
	ui::addFormField(country);

	auto tags = makeRow(parent, "Tags", ZOrder(z++))
						->addChild(Rc<ui::ChipRow>::create(), s_widgetZOrder);
	tags->setName("tags");
	tags->setOptions(ui::makeSelectOptions(SpanView<StringView>({StringView("alpha"),
		StringView("beta"), StringView("stable"), StringView("legacy"), StringView("draft")})));
	tags->setItems(SpanView<ui::ChipItem>({
					   ui::ChipItem{String("alpha"), String("alpha")},
					   ui::ChipItem{String("stable"), String("stable")},
				   }),
			true);
	tags->setAutoHeight(true);
	// An ARRAY under one name, left to right: the order is part of the value.
	ui::addFormField(tags);
}

// ui::ColorField, once with each picker policy.
static void buildColorGroup(NotNull<Node> parent, int32_t &z) {
	/* POPUP TWO: the built-in colour picker, in the scene.

	`Fallback` rather than `Auto` on purpose - `Auto` asks the window for a system dialog and takes
	it where there is one, which would hide the very surface this demo is about. The field below it
	is left on `Auto` so both policies are on screen at once. */
	auto accent = makeRow(parent, "Accent (built-in)", ZOrder(z++))
						  ->addChild(Rc<ui::ColorField>::create(), s_widgetZOrder);
	accent->setName("accent");
	accent->setValue(Color4B(0x1E, 0x88, 0xE5, 0xFF), true);
	accent->setPickerMode(ui::ColorField::PickerMode::Fallback);
	accent->setPalette(SpanView<Color4B>(s_palette));
	{
		ui::PopupSurfaceConfig config;
		config.preferNative = s_preferNative;
		accent->setPickerConfig(sp::move(config));
	}
	ui::addFormField(accent);

	// With an alpha channel, so the hex it collects is `#rrggbbaa` and the picker grows a fourth
	// bar. Left on Auto: on a platform with a colour dialog this one opens that instead.
	auto overlay = makeRow(parent, "Overlay (auto + alpha)", ZOrder(z++))
						   ->addChild(Rc<ui::ColorField>::create(), s_widgetZOrder);
	overlay->setName("overlay");
	overlay->setAlphaEnabled(true);
	overlay->setValue(Color4B(0x8E, 0x24, 0xAA, 0x80), true);
	overlay->setPickerColorMode(ui::ColorPickerMode::HSL);
	overlay->setPalette(SpanView<Color4B>(s_palette));
	{
		ui::PopupSurfaceConfig config;
		config.preferNative = s_preferNative;
		overlay->setPickerConfig(sp::move(config));
	}
	ui::addFormField(overlay);
}

/* The text WIDGETS that are not form fields.

They are here because "every input the kit ships" is the promise, and leaving them out would make
the demo quietly incomplete. They are not ui::addFormField'ed because they cannot be: the TextInput
adapter drives a field through non-virtual, window-backed accessors that ui::TextView replaces
wholesale. A caller who needs one inside a form writes the FormFieldSlots for it - which is exactly
the seam ui::addFormField(NotNull<Node>, FormFieldSlots &&) exists for. */
static void buildEditorsGroup(NotNull<Node> parent, int32_t &z) {
	auto view = makeRow(parent, "TextView", ZOrder(z++))
						->addChild(Rc<ui::TextView>::create(), s_widgetZOrder);
	view->setName("prose");
	view->addStyleClass("demo-editor");
	view->setWordWrap(true);
	view->setCaretBlink(false);
	static constexpr auto kProse = WideStringView(u"A multi-line view: it wraps, it scrolls,\n"
												 u"and it is NOT a form field - see the "
												 u"comment beside this call.");
	view->setText(kProse);

	auto code = makeRow(parent, "CodeEditor", ZOrder(z++))
						->addChild(Rc<ui::CodeEditor>::create(), s_widgetZOrder);
	code->setName("source");
	code->addStyleClass("demo-editor");
	code->setCaretBlink(false);
	static constexpr auto kSource = WideStringView(
			u"// the same widget, configured as a source "
			u"editor\n" u"int main() {\n\treturn 0;\n}\n");
	code->setText(kSource);
}

// The two buttons. In the tab ring, fired by Enter from anywhere in the form, collected by nobody.
static void buildActionsGroup(NotNull<Node> parent, int32_t &z) {
	auto row = parent->addChild(Rc<Node>::create(), ZOrder(z++));
	row->addStyleClass("field-row");
	row->addStyleClass("actions-row");

	auto submit = row->addChild(Rc<ui::Button>::create(), ZOrder(1));
	submit->setName("submit");
	submit->setString("Submit");
	// The FIRST Submit-role field in the ring is also what `:default` paints, so the button that
	// lights up is the button Enter will actually fire.
	ui::addFormButton(submit, ui::FormFieldRole::Submit);

	auto reset = row->addChild(Rc<ui::Button>::create(), ZOrder(2));
	reset->setName("reset");
	reset->setString("Reset");
	ui::addFormButton(reset, ui::FormFieldRole::Reset);
}

} // namespace

void buildFieldGroup(NotNull<Node> parent, FieldGroup group, int32_t firstZOrder) {
	int32_t z = firstZOrder;
	switch (group) {
	case FieldGroup::Text: buildTextGroup(parent, z); break;
	case FieldGroup::Numbers: buildNumbersGroup(parent, z); break;
	case FieldGroup::Choice: buildChoiceGroup(parent, z); break;
	case FieldGroup::Color: buildColorGroup(parent, z); break;
	case FieldGroup::Editors: buildEditorsGroup(parent, z); break;
	case FieldGroup::Actions: buildActionsGroup(parent, z); break;
	}
}

} // namespace stappler::xenolith::examples
