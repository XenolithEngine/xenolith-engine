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

#include "widgets/ChipRowLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

/* Colours and text only. NOT `display:flex`, and NOT a size for a chip: the point of this stand is
the row's own wrap arithmetic and the natural width a chip reports, and a flex container would
replace both with the layout engine's. The styled path is what every other widget's stand exercises;
this one deliberately runs the fallback. */
static constexpr auto s_chipCss = StringView(R"css(
chip-row {
	background-color: #232323;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 6px;
}
chip-row:focus {
	outline-color: #fcb400;
}
chip-row.invalid {
	outline-color: #e53935;
}
chip {
	background-color: #333333;
	border-radius: 12px;
}
chip.selected {
	outline-color: #fcb400;
	outline-width: 1px;
}
chip > label {
	color: #e8e8e8;
	font-size: 13px;
}
text-input {
	width: 220px;
	height: 36px;
	background-color: #292929;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 6px;
	padding: 0 10px;
	color: #e8e8e8;
	font-size: 14px;
	--caret-color: #fcb400;
}
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

Value encodeRect(Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}
	auto size = node->getContentSize();
	auto center = node->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f));
	ret.setDouble(double(center.x), "cx");
	ret.setDouble(double(center.y), "cy");
	ret.setDouble(double(size.width), "width");
	ret.setDouble(double(size.height), "height");
	return ret;
}

Value encodeClasses(Node *node) {
	Value ret;
	if (auto set = node->getStyleClasses()) {
		for (auto &it : *set) { ret.addString(it); }
	}
	return ret;
}

// The options every row offers. Five, so that a limit of three is reachable and a duplicate is
// possible.
static constexpr StringView s_optionIds[] = {
	StringView("int"),
	StringView("float"),
	StringView("string"),
	StringView("array"),
	StringView("map"),
};

} // namespace

bool ChipRowLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_chipCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	Vector<ui::ChipOption> options;
	for (auto &it : s_optionIds) {
		options.emplace_back(ui::ChipOption{it.str<Interface>(), it.str<Interface>()});
	}

	auto makeRow = [&](StringView name, float width, ZOrder z, Node *parent) {
		auto row = parent->addChild(Rc<ui::ChipRow>::create(), z);
		row->setName(name);
		row->setOptions(options);
		// The height is the row's to decide; only the width is imposed, which is exactly the input
		// the wrap is a function of.
		row->setContentSize(Size2(width, 32.0f));

		row->setChangeCallback([this, name = name.str<Interface>()](SpanView<ui::ChipItem> items) {
			// emplace/find rather than operator[]: the runtime's map returns an access token from
			// it, which traps on a key that is not there.
			auto it = _callbacks.find(name);
			if (it == _callbacks.end()) {
				_callbacks.emplace(name, 1);
			} else {
				++it->second;
			}

			StringStream ids;
			for (auto &item : items) {
				if (!ids.empty()) {
					ids << ",";
				}
				ids << item.id;
			}
			auto last = _lastValue.find(name);
			if (last == _lastValue.end()) {
				_lastValue.emplace(name, ids.str());
			} else {
				last->second = ids.str();
			}
		});

		row->setIntrinsicHeightCallback([this, name = name.str<Interface>()](float) {
			auto it = _heightReports.find(name);
			if (it == _heightReports.end()) {
				_heightReports.emplace(name, 1);
			} else {
				++it->second;
			}
		});
		return row;
	};

	ui::ChipItem starting[] = {
		ui::ChipItem{String("int"), String("int")},
		ui::ChipItem{String("float"), String("float")},
		ui::ChipItem{String("string"), String("string")},
	};

	_free = makeRow("free", 420.0f, ZOrder(1), this);
	_free->setItems(makeSpanView(starting, 3), true);

	_limited = makeRow("limited", 420.0f, ZOrder(2), this);
	_limited->setUniqueIds(true);
	_limited->setMaxCount(3);
	_limited->setItems(makeSpanView(starting, 2), true);

	// Narrow on purpose: five chips do not fit on one line of 150 points, so this row is the one
	// that says whether the reported height and the drawn lines agree.
	_narrow = makeRow("narrow", 150.0f, ZOrder(3), this);
	ui::ChipItem many[] = {
		ui::ChipItem{String("int"), String("int")},
		ui::ChipItem{String("float"), String("float")},
		ui::ChipItem{String("string"), String("string")},
		ui::ChipItem{String("array"), String("array")},
		ui::ChipItem{String("map"), String("map")},
	};
	_narrow->setItems(makeSpanView(many, 5), true);

	/* The form is on a node of its own - see the class comment. */
	_formPanel = addChild(Rc<Node>::create(), ZOrder(4));
	_form = _formPanel->addSystem(Rc<ui::FormSystem>::create());
	_form->setSubmitCallback([this](Value &&value) {
		++_submitCount;
		_lastSubmit = sp::move(value);
	});
	_form->setInvalidCallback([this](SpanView<ui::FormValidationError>) { ++_invalidCount; });

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// an unstable sort.
	_formRow = makeRow("form-chips", 420.0f, ZOrder(1), _formPanel);
	_formRow->setItems(makeSpanView(starting, 1), true);
	// Required, so that an empty row can be shown to be refused ONCE rather than per chip.
	ui::addFormField(_formRow, StringView(), ui::FormFieldFlags::Required);

	_neighbour = _formPanel->addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setCaretBlink(false);
	_neighbour->setText("abc");
	ui::addFormField(_neighbour);

	return true;
}

void ChipRowLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_formPanel) {
		_formPanel->setAnchorPoint(Vec2(0.0f, 0.0f));
		_formPanel->setPosition(Vec2(0.0f, 0.0f));
		_formPanel->setContentSize(_contentSize);
	}

	// Position only: the WIDTH was assigned once, and the height is the row's own answer. Writing a
	// size here every frame would take the auto height back off it.
	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_free, _limited, _narrow, _formRow, _neighbour};
	for (size_t i = 0; i < 5; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 96.0f));
	}
}

ui::ChipRow *ChipRowLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "limited") {
		return _limited;
	} else if (name == "narrow") {
		return _narrow;
	} else if (name == "form-chips") {
		return _formRow;
	}
	return _free;
}

Value ChipRowLayout::encodeChip(ui::Chip *chip, bool selected) const {
	Value ret;
	if (!chip) {
		return ret;
	}
	ret.setString(chip->getName(), "name");
	ret.setString(chip->getText(), "text");
	ret.setBool(chip->isSelected(), "selected");
	// What the ROW thinks, beside what the chip wears: the two disagreeing is the bug this catches.
	ret.setBool(selected, "rowSelected");
	ret.setBool(chip->isRemovable(), "removable");
	ret.setValue(encodeRect(chip), "rect");
	if (auto button = chip->getRemoveButton()) {
		ret.setValue(encodeRect(button), "removeRect");
	}
	ret.setValue(encodeClasses(chip), "classes");
	return ret;
}

Value ChipRowLayout::encodeRow(ui::ChipRow *row) const {
	Value ret;
	if (!row) {
		return ret;
	}

	Value ids;
	for (auto &it : row->getItems()) { ids.addString(it.id); }
	ret.setValue(sp::move(ids), "ids");

	ret.setInteger(int64_t(row->getSelected()), "selected");
	ret.setBool(row->isFocused(), "focused");
	ret.setBool(row->isEnabled(), "enabled");
	ret.setBool(row->isFull(), "full");
	ret.setBool(row->isUniqueIds(), "unique");
	ret.setInteger(int64_t(row->getMaxCount()), "maxCount");
	ret.setBool(row->isOpen(), "open");
	if (auto popup = row->getPopup()) {
		ret.setString(popup->getId(), "popupWindow");
	}

	// The two numbers that have to agree: what the row REPORTS and what it is.
	ret.setInteger(int64_t(row->getLineCount()), "lines");
	ret.setDouble(double(row->getIntrinsicHeight()), "intrinsicHeight");
	ret.setDouble(double(row->getContentSize().height), "height");
	ret.setDouble(double(row->getContentSize().width), "width");

	ret.setValue(encodeClasses(row), "classes");
	ret.setValue(encodeRect(row), "rect");

	if (auto add = row->getAddButton()) {
		ret.setBool(add->isVisible(), "addVisible");
		ret.setBool(add->isEnabled(), "addEnabled");
		ret.setValue(encodeClasses(add), "addClasses");
		ret.setValue(encodeRect(add), "addRect");
	}

	Value chips;
	for (uint32_t i = 0; i < uint32_t(row->getItemCount()); ++i) {
		chips.addValue(encodeChip(row->getChipAt(i), int32_t(i) == row->getSelected()));
	}
	ret.setValue(sp::move(chips), "chips");

	auto name = row->getName().str<Interface>();
	auto cb = _callbacks.find(name);
	ret.setInteger(cb == _callbacks.end() ? 0 : int64_t(cb->second), "callbacks");
	auto hr = _heightReports.find(name);
	ret.setInteger(hr == _heightReports.end() ? 0 : int64_t(hr->second), "heightReports");
	auto last = _lastValue.find(name);
	if (last != _lastValue.end()) {
		ret.setString(last->second, "lastValue");
	}
	return ret;
}

Value ChipRowLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeRow(_free), "free");
	ret.setValue(encodeRow(_limited), "limited");
	ret.setValue(encodeRow(_narrow), "narrow");
	ret.setValue(encodeRow(_formRow), "formRow");

	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}

	if (_form) {
		ret.setValue(_form->collect(), "collected");
		if (auto focused = _form->getFocusedField()) {
			ret.setString(focused->getFieldName(), "formFocus");
		}
	}

	ret.setInteger(int64_t(_submitCount), "submits");
	ret.setInteger(int64_t(_invalidCount), "invalids");
	ret.setValue(_lastSubmit, "lastSubmit");
	return ret;
}

void ChipRowLayout::registerCommands() {
	addCommand("state",
			"Report every row: its ids, selection, limits, geometry and what the form " "collects",
			[this](Value &&) { return encodeState(); });

	addCommand("set-items", "Replace the row's members: {target, ids}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		Vector<ui::ChipItem> items;
		for (auto &it : static_cast<const Value &>(args).getValue("ids").asArray()) {
			auto id = it.getString();
			items.emplace_back(ui::ChipItem{String(id), String(id)});
		}
		row->setItems(items, static_cast<const Value &>(args).getBool("silent"));
		return ackValue(true);
	});

	addCommand("add", "Add the declared option: {target, id}", [this](Value &&args) {
		auto row = getTarget(args);
		return ackValue(
				row ? row->addById(static_cast<const Value &>(args).getString("id")) : false);
	});

	addCommand("remove", "Take a member off by index: {target, index}", [this](Value &&args) {
		auto row = getTarget(args);
		return ackValue(row ? row->removeItem(uint32_t(
									  static_cast<const Value &>(args).getInteger("index")))
							: false);
	});

	addCommand("select", "Select a chip, or -1 for none: {target, index}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		row->select(int32_t(static_cast<const Value &>(args).getInteger("index")));
		return ackValue(true);
	});

	addCommand("focus", "Give the row the keyboard, or take it away: {target, value}",
			[this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		const Value &a = args;
		if (!a.isBool("value") || a.getBool("value")) {
			row->focus();
		} else {
			row->blur();
		}
		return ackValue(true);
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the rows", [this](Value &&) {
		if (!_neighbour) {
			return ackValue(false);
		}
		_neighbour->focus();
		return ackValue(true);
	});

	addCommand("open", "Open the \"+\" menu: {target}", [this](Value &&args) {
		auto row = getTarget(args);
		return ackValue(row ? row->open() : false);
	});

	addCommand("close", "Dismiss the \"+\" menu: {target}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		row->close();
		return ackValue(true);
	});

	addCommand("set-max", "Declare the limit, 0 for none: {target, value}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		row->setMaxCount(uint32_t(static_cast<const Value &>(args).getInteger("value")));
		return ackValue(true);
	});

	addCommand("set-unique", "Whether the same id may appear twice: {target, value}",
			[this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		row->setUniqueIds(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("set-width", "Re-wrap at another width: {target, value}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		const auto width = float(static_cast<const Value &>(args).getDouble("value"));
		row->setContentSize(Size2(width, row->getContentSize().height));
		return ackValue(true);
	});

	addCommand("set-enabled", "Turn the row off and on: {target, value}", [this](Value &&args) {
		auto row = getTarget(args);
		if (!row) {
			return ackValue(false);
		}
		row->setEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("assign", "Assign the form's value: {value}", [this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}
		_form->assign(static_cast<const Value &>(args).getValue("value"));
		return ackValue(true);
	});

	addCommand("submit", "Submit the form",
			[this](Value &&) { return ackValue(_form ? _form->submit() : false); });

	addCommand("reset-counters", "Zero every row's counters", [this](Value &&) {
		_callbacks.clear();
		_heightReports.clear();
		_lastValue.clear();
		_submitCount = 0;
		_invalidCount = 0;
		_lastSubmit = Value();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
