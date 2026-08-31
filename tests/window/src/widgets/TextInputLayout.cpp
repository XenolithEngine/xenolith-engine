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

#include "widgets/TextInputLayout.h"
#include "XLUiStyleResolver.h"
#include "XLInheritedStyle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// `text-input` (type selector) -> the widget's own vector fill + stroke + radius + padding, applied
// by the registered "text-input" type appliers. `--caret-color` / `--selection-color` are custom
// properties, because the document engine's CSS subset has neither caret-color nor ::selection.
// `:focus` is the point of the accent outline rule: nothing in the engine produced that
// pseudo-class before this widget.
static constexpr auto s_textInputCss = StringView(R"css(
text-input {
	width: 350px;
	height: 39px;
	background-color: #292929;
	outline-color: #3d3d3d;
	outline-width: 1px;
	border-radius: 7px;
	padding: 8px 12px;
}
/* On a CLASS, not on the type, so that one field is left without them: a caret and a selection
   with no colour of their own must come out in the text's ink, and a rule that reached every field
   would leave that half of the rule untested. */
text-input.tinted {
	--caret-color: #fcb400;
	--selection-color: #7a5600;
	--marked-color: #2b5f7a;
}
text-input:hover {
	outline-color: #5a5a5a;
}
text-input:focus {
	outline-color: #fcb400;
}
text-input.wide {
	width: 520px;
}
/* A box far taller than its line, so that "the text is centred" and "the text is on the floor"
   cannot be confused: at the stock 39px they differ by a single pixel. */
text-input.tall {
	height: 64px;
}
label {
	color: #e8e8e8;
	font-size: 18px;
}
label.xl-ui-text-input-placeholder {
	color: #6a6a6a;
}
)css");

constexpr auto s_longText = StringView(
		"The quick brown fox jumps over the lazy dog, and then keeps running well past the edge.");

Value encodeColor(const Color4B &c) {
	Value ret;
	ret.setInteger(c.r, "r");
	ret.setInteger(c.g, "g");
	ret.setInteger(c.b, "b");
	ret.setInteger(c.a, "a");
	return ret;
}

} // namespace

bool TextInputLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_textInputCss);

	// One recursive resolver covers the fields and the labels inside them.
	addSystem(Rc<ui::StyleResolver>::create(true));

	_plain = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_plain->setName("plain");
	_plain->addStyleClass("tinted");
	_plain->setPlaceholder("Type here");
	_plain->setCallback([this](StringView str) {
		++_changeCallbacks;
		_lastChange = str.str<Interface>();
	});
	_plain->setEnterCallback([this] { ++_enterCallbacks; });

	_password = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_password->setName("password");
	_password->addStyleClass("tinted");
	_password->setPlaceholder("Password");
	_password->setPasswordMode(ui::TextInputPasswordMode::ShowNone);

	// Deliberately NOT `tinted`: this is the field whose caret and selection have to be derived
	// from the text colour, which is the behaviour every unconfigured field in an application gets
	_readOnly = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_readOnly->setName("readonly");
	_readOnly->addStyleClass("tall");
	_readOnly->setText("Read-only value");
	_readOnly->setReadOnly(true);

	_long = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_long->setName("long");
	_long->addStyleClass("wide");
	_long->addStyleClass("tinted");
	_long->setText(s_longText);

	// Deterministic screenshots: a blinking caret is a coin flip in a still image.
	for (auto it : {_plain, _password, _readOnly, _long}) { it->setCaretBlink(false); }

	return true;
}

void TextInputLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;
	ui::TextInput *fields[] = {_plain, _password, _readOnly, _long};
	for (size_t i = 0; i < 4; ++i) {
		if (!fields[i]) {
			continue;
		}
		fields[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		fields[i]->setPosition(Vec2(48.0f, top - float(i) * 72.0f));
	}
}

ui::TextInput *TextInputLayout::getWidget(const Value &args) const {
	auto name = args.getString("widget");
	if (name.empty() || name == "plain") {
		return _plain;
	} else if (name == "password") {
		return _password;
	} else if (name == "readonly") {
		return _readOnly;
	} else if (name == "long") {
		return _long;
	}
	return nullptr;
}

Value TextInputLayout::encodeState(ui::TextInput *input) const {
	Value ret;
	if (!input) {
		ret.setString("unknown widget", "error");
		return ret;
	}

	const auto &state = input->getInputState();
	auto container = input->getContainer();
	auto caret = container->getCaret();

	ret.setString(input->getText(), "text");
	ret.setString(string::toUtf8<Interface>(input->getDisplayText()), "displayText");
	ret.setString(input->getPlaceholder(), "placeholder");
	ret.setBool(input->isFocused(), "focused");
	ret.setBool(input->isReadOnly(), "readOnly");
	ret.setBool(input->isEnabled(), "enabled");
	ret.setInteger(int64_t(state.cursor.start), "cursorStart");
	ret.setInteger(int64_t(state.cursor.length), "cursorLength");
	ret.setInteger(int64_t(state.marked.start), "markedStart");
	ret.setInteger(int64_t(state.marked.length), "markedLength");
	ret.setDouble(double(caret->getPosition().x), "caretX");
	ret.setDouble(double(caret->getPosition().y), "caretY");
	ret.setBool(caret->isVisible(), "caretVisible");
	ret.setDouble(double(container->getLabelOffset()), "labelOffsetX");
	ret.setBool(container->hasHorizontalOverflow(), "overflow");
	ret.setBool(container->getPlaceholder()->isVisible(), "placeholderVisible");

	// what the Label was actually told to highlight - the widget's cursor and the drawn selection
	// are two different things, and only this tells them apart
	auto sel = container->getLabel()->getSelectionCursor();
	ret.setInteger(int64_t(sel.start), "labelSelectionStart");
	ret.setInteger(int64_t(sel.length), "labelSelectionLength");
	// The colour the selection was actually DRAWN with, and the ink it should match when the field
	// declares none of its own
	ret.setValue(encodeColor(Color4B(container->getLabel()->getSelectionColor())),
			"labelSelectionColor");
	ret.setValue(encodeColor(Color4B(container->getCaret()->getColor())), "appliedCaretColor");
	ret.setValue(encodeColor(Color4B(container->getLabel()->getMarkedColor())), "labelMarkedColor");

	// Where the highlight is DRAWN, not what it was told to cover: a selection built against a
	// size the label did not have yet lands outside the box and is scissored away, while every
	// cursor field above still reports it as present.
	auto selRect = container->getLabel()->getSelectionRect();
	Value rect;
	rect.setDouble(double(selRect.origin.x), "x");
	rect.setDouble(double(selRect.origin.y), "y");
	rect.setDouble(double(selRect.size.width), "width");
	rect.setDouble(double(selRect.size.height), "height");
	ret.setValue(sp::move(rect), "labelSelectionRect");

	// The single line and the box it has to sit in the middle of.
	ret.setDouble(double(container->getLabel()->getPosition().y), "labelY");
	ret.setDouble(double(container->getLabel()->getContentSize().height), "labelHeight");
	ret.setDouble(double(container->getPlaceholder()->getPosition().y), "placeholderY");
	ret.setDouble(double(container->getContentSize().height), "viewportHeight");

	// The text's own ink, accumulated the way the Label paints it: an inherited `color` beats the
	// node's tint, and it is the tint that used to be handed to the caret
	auto label = container->getLabel();
	auto textColor = Color4B(label->getColor());
	const auto inherited = accumulateInheritedStyle<InheritedColorStyle>(label);
	if (inherited.defined & InheritedColorStyle::DefinedColor) {
		textColor = Color4B(inherited.color);
	}
	ret.setValue(encodeColor(textColor), "textColor");

	Value interactive;
	if (auto ic = input->getComponent<InteractiveComponent>()) {
		interactive.setBool(hasFlag(ic->state, InteractiveState::Hover), "hover");
		interactive.setBool(hasFlag(ic->state, InteractiveState::Focus), "focus");
		interactive.setBool(hasFlag(ic->state, InteractiveState::Active), "active");
		interactive.setBool(hasFlag(ic->state, InteractiveState::Enabled), "enabled");
	}
	ret.setValue(sp::move(interactive), "interactive");

	if (auto style = input->getComponent<ui::TextInputStyleComponent>()) {
		ret.setValue(encodeColor(style->outlineColor), "outlineColor");
		ret.setValue(encodeColor(style->backgroundColor), "backgroundColor");
		ret.setValue(encodeColor(style->caretColor), "caretColor");
	}

	ret.setInteger(int64_t(_changeCallbacks), "changeCallbacks");
	ret.setInteger(int64_t(_enterCallbacks), "enterCallbacks");
	ret.setString(_lastChange, "lastChange");
	return ret;
}

// Mutating commands answer with a bare ack, never with a state snapshot. A command handler runs
// synchronously, but every edit is a REQUEST to the platform whose result arrives an event loop
// later - a snapshot taken here would report the state before the echo. Read it back with a
// separate `state` call, which by then reflects what the platform actually did.
static Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

void TextInputLayout::registerCommands() {
	addCommand("state", "Report the full state of a field: {widget}", [this](Value &&args) {
		return encodeState(getWidget(args));
	});

	addCommand("focus", "Acquire text input for a field: {widget}. Read back with `state`",
			[this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			input->focus();
		}
		return ackValue(input != nullptr);
	});

	addCommand("blur", "Release text input: {widget}", [this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			input->blur();
		}
		return ackValue(input != nullptr);
	});

	addCommand("set-text", "Replace a field's text: {widget, text}", [this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			input->setText(static_cast<const Value &>(args).getString("text"));
		}
		return ackValue(input != nullptr);
	});

	addCommand("select-all", "Select the whole text: {widget}", [this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			input->selectAll();
		}
		return ackValue(input != nullptr);
	});

	addCommand("set-cursor", "Move the cursor: {widget, start, length}", [this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			const Value &req = args;
			input->setCursor(TextCursor(uint32_t(req.getInteger("start", 0)),
					uint32_t(req.getInteger("length", 0))));
		}
		return ackValue(input != nullptr);
	});

	addCommand("set-max-chars", "Limit the field's length: {widget, value}", [this](Value &&args) {
		auto input = getWidget(args);
		if (input) {
			input->setMaxChars(size_t(static_cast<const Value &>(args).getInteger("value", 0)));
		}
		return ackValue(input != nullptr);
	});

	addCommand("reset-counters", "Zero the change/enter callback counters", [this](Value &&) {
		_changeCallbacks = 0;
		_enterCallbacks = 0;
		_lastChange.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
