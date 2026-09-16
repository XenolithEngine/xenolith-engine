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

#include "widgets/TextViewLayout.h"
#include "XLUiStyleResolver.h"
#include "XLDirector.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

static constexpr auto s_textViewCss = StringView(R"css(
label {
	color: #e8e8e8;
	font-size: 14px;
}
text-input {
	width: 420px;
	height: 30px;
	background-color: #292929;
	outline-width: 1px;
	outline-color: rgba(255,255,255,.15);
	border-radius: 4px;
	padding: 0 8px;
	color: #e8e8e8;
	font-size: 14px;
}
.text-view {
	width: 420px;
	height: 220px;
	background-color: #202026;
	outline-width: 1px;
	outline-color: #3d3d3d;
	color: #e8e8e8;
	font-size: 14px;
}
)css");

// A mutating command answers whether it was taken, not what the state became: the caller reads
// `state` afterwards, and by then it reflects what the platform actually did rather than what was
// asked for. Copied from the text-input stand for exactly that reason.
Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool TextViewLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_textViewCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_caption = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_caption->setType("label");
	_caption->setString("ui::TextHistory: the view undoes, the field below does not");

	_view = addChild(Rc<ui::TextView>::create(), ZOrder(1));
	_view->setName("view");

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_field->setName("field");

	// Deterministic screenshots: a blinking caret is a coin flip in a still image.
	_view->setCaretBlink(false);
	_field->setCaretBlink(false);

	// The catcher below the widgets. Not FocusedOnly and not on either of them: it must see
	// exactly the chords that the focused widget declined.
	auto listener = addSystem(Rc<InputListener>::create());
	auto &hk = EngineHotkeys::get();
	listener->addHotkey(hk.undo, [this](HotkeyId, const InputEvent &) {
		++_undoFellThrough;
		return true;
	});
	auto redo = [this](HotkeyId, const InputEvent &) {
		++_redoFellThrough;
		return true;
	};
	listener->addHotkey(hk.redo, redo);
	listener->addHotkey(hk.redoAlt, redo);

	return true;
}

void TextViewLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const auto top = getWorkTop();
	const auto size = getWorkSize();
	const auto x = size.width / 2.0f;

	_caption->setAnchorPoint(Anchor::MiddleTop);
	_caption->setPosition(Vec2(x, top - 8.0f));

	_view->setAnchorPoint(Anchor::MiddleTop);
	_view->setPosition(Vec2(x, top - 40.0f));

	_field->setAnchorPoint(Anchor::MiddleTop);
	_field->setPosition(Vec2(x, top - 280.0f));
}

ui::TextInput *TextViewLayout::widget(const Value &args) const {
	return args.getString("widget") == "field" ? static_cast<ui::TextInput *>(_field) : _view;
}

Value TextViewLayout::encodeField() const {
	auto &history = _field->getHistory();

	Value ret;
	ret.setString(_field->getText(), "text");
	ret.setInteger(int64_t(_field->getCursor().start), "cursorStart");
	ret.setInteger(int64_t(_field->getCursor().length), "cursorLength");
	ret.setBool(_field->isFocused(), "focused");
	ret.setBool(history.isEnabled(), "undoEnabled");
	ret.setBool(_field->canUndo(), "canUndo");
	ret.setBool(_field->canRedo(), "canRedo");
	ret.setString(_field->getUndoName(), "undoName");
	ret.setInteger(int64_t(history.getDepth()), "historyDepth");
	ret.setInteger(int64_t(history.getPosition()), "historyPosition");
	return ret;
}

Value TextViewLayout::encodeState() const {
	Value ret;
	// The view reports itself: everything undo needs is already in ui::TextView's own state, which
	// is what makes this stand thin and what lets a code editor be driven the same way.
	ret.setValue(_view->encodeState(), "view");
	ret.setValue(encodeField(), "field");
	ret.setInteger(_undoFellThrough, "undoFellThrough");
	ret.setInteger(_redoFellThrough, "redoFellThrough");
	return ret;
}

void TextViewLayout::registerCommands() {
	// Built here rather than in init(): the director the session is a seam over only exists once
	// the layout has entered a scene.
	_session = Rc<ClipboardSession>::create(getDirector()->getApplication());

	addCommand("state", "The view, the field and both histories",
			[this](Value &&) { return encodeState(); });

	addCommand("reset-counters", "Zero the fall-through counters", [this](Value &&) {
		_undoFellThrough = 0;
		_redoFellThrough = 0;
		return ackValue(true);
	});

	addCommand("clipboard-write", "Put {text} on the clipboard so a paste has something to take",
			[this](Value &&args) {
		ClipboardOffer offer;
		offer.addText(args.getString("text"), "text/plain");
		return ackValue(sprt::status::isSuccessful(_session->write(sp::move(offer))));
	});

	addCommand("paste", "Start an asynchronous paste at the caret: {widget}",
			[this](Value &&args) { return ackValue(widget(args)->paste()); });

	// Everything below forwards into the widget's own inspector surface, which matches by suffix -
	// so the stand adds no logic of its own and a code editor answers the same commands under its
	// own prefix. The field takes the same route through its base-class half.
	auto forward = [this](StringView action) {
		return [this, action](Value &&args) {
			auto target = widget(args);
			Value result;
			if (auto view = dynamic_cast<ui::TextView *>(target)) {
				result.setBool(view->handleInspectorCommand(action, args, result), "ok");
				return result;
			}

			// The plain field has no inspector surface of its own; the three things this stand
			// asks of it are spelled out here.
			if (action == "copy") {
				return ackValue(_field->copy());
			} else if (action == "cut") {
				return ackValue(_field->cut());
			} else if (action == "undo") {
				return ackValue(_field->undo());
			} else if (action == "redo") {
				return ackValue(_field->redo());
			} else if (action == "undo-enabled") {
				_field->setUndoEnabled(args.getBool("value"));
				return ackValue(true);
			} else if (action == "set-text") {
				_field->setText(args.getString("text"));
				return ackValue(true);
			} else if (action == "set-cursor") {
				_field->setCursor(TextCursor(uint32_t(args.getInteger("start")),
						uint32_t(args.getInteger("length"))));
				return ackValue(true);
			} else if (action == "focus") {
				_field->focus();
				return ackValue(true);
			} else if (action == "history-break") {
				_field->getHistory().breakRun();
				return ackValue(true);
			} else if (action == "history-idle") {
				_field->getHistory().setCoalesceIdle(
						uint64_t(sprt::max(args.getInteger("value"), int64_t(0))));
				return ackValue(true);
			} else if (action == "history-clear") {
				_field->getHistory().clear();
				return ackValue(true);
			}
			return ackValue(false);
		};
	};

	addCommand("set-text", "Replace the whole text: {text, widget}", forward("set-text"));
	addCommand("insert", "Insert {text} at character {at}", forward("insert"));
	addCommand("set-cursor", "Set the cursor to {start, length, widget}", forward("set-cursor"));
	addCommand("focus", "Acquire text input: {widget}", forward("focus"));
	addCommand("copy", "Copy the selection: {widget}", forward("copy"));
	addCommand("cut", "Cut the selection: {widget}", forward("cut"));
	addCommand("undo", "Undo one entry: {widget}", forward("undo"));
	addCommand("redo", "Redo one entry: {widget}", forward("redo"));
	addCommand("undo-enabled", "Turn the history {value} on or off: {widget}",
			forward("undo-enabled"));
	addCommand("history-break", "End the run in progress now: {widget}", forward("history-break"));
	addCommand("history-idle", "Set the coalescing window to {value} microseconds: {widget}",
			forward("history-idle"));
	addCommand("history-clear", "Forget the history without changing the text: {widget}",
			forward("history-clear"));
}

} // namespace stappler::xenolith::app
