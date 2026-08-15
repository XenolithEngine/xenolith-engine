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

#include "XLUiConsole.h"

#include "SPString.h"
#include "XLInputListener.h"
#include "XLScene.h"
#include "XLSceneContent.h"
#include "XLSceneInspector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// ===========================================================================
// ConsoleInput
// ===========================================================================

bool ConsoleInput::init() {
	if (!TextInput::init()) {
		return false;
	}

	setName("console-input");
	addStyleClass("console-input");
	return true;
}

bool ConsoleInput::handleKey(const GestureData &data) {
	if (_focused && _historyCallback && data.input) {
		const auto &ev = data.input->data;
		if (ev.event == InputEventName::KeyPressed || ev.event == InputEventName::KeyRepeated) {
			if (ev.key.keycode == InputKeyCode::UP) {
				return _historyCallback(-1);
			}
			if (ev.key.keycode == InputKeyCode::DOWN) {
				return _historyCallback(1);
			}
		}
	}

	// Left/Right/Home/End are still the base's business, and so are Up/Down when no history is
	// installed - falling through keeps the field behaving like an ordinary one.
	return TextInput::handleKey(data);
}

// ===========================================================================
// Console
// ===========================================================================

bool Console::init() {
	if (!Node::init()) {
		return false;
	}

	setName("console");
	addStyleClass("console");

	_output = addChild(Rc<TextView>::create());
	_output->setName("console-output");
	_output->addStyleClass("console-output");
	// Read-only, so it never acquires the IME and never shows a caret - but it still takes taps,
	// drag-selection and the copy chord. Wrapped, because a log line has no meaningful column and
	// horizontal scrolling to read one is hostile. No gutter: log lines are not numbered.
	_output->setReadOnly(true);
	_output->setWordWrap(true);
	_output->setGutterVisible(false);
	_output->setCurrentLineHighlight(false);

	_row = addChild(Rc<Node>::create());
	_row->setName("console-row");
	_row->addStyleClass("console-row");

	_promptLabel = _row->addChild(Rc<basic2d::Label>::create());
	_promptLabel->setType("label");
	_promptLabel->addStyleClass("console-prompt");

	_input = _row->addChild(Rc<ConsoleInput>::create());
	_input->setEnterCallback([this] { submit(); });
	_input->setHistoryCallback([this](int32_t d) { return moveHistory(d); });

	setPrompt("> ");

	return true;
}

void Console::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	_inspectorScene = scene;
	addInspectorCommands(scene);
}

void Console::handleExit() {
	// Before the base call: Node::handleExit() clears _scene at its very end, and a command whose
	// lambda captured a destroyed widget is a dangling call from the inspector socket.
	if (_inspectorScene) {
		if (auto content = _inspectorScene->getContent()) {
			if (auto i = inspector::get(content)) {
				for (auto &it : _inspectorCommands) { i->removeCommand(it); }
			}
		}
		_inspectorScene = nullptr;
	}
	_inspectorCommands.clear();

	Node::handleExit();
}

void Console::setPrompt(StringView str) {
	_prompt = str.str<Interface>();
	_promptLabel->setString(_prompt);
}

void Console::appendLine(StringView str) { appendOutput(mem_std::toString(str, "\n")); }

void Console::appendOutput(StringView str) {
	// The pane's scroll follows the tail only while it is already at the tail: a user who scrolled
	// up to read something must not be yanked back down by the next line of output.
	const auto view = _output->getView();
	const auto stick = view->getScrollRange().height - view->getScrollOffset().y < 1.0f;

	_outputText.append(string::toUtf16<Interface>(str));

	// Drop whole lines off the front until the line limit holds. Only the line count is bounded:
	// each visible line is its own Label, so there is no character ceiling to keep, and the ring
	// exists so an immortal console does not grow without bound.
	size_t lines = 0;
	for (auto c : _outputText) {
		if (c == u'\n') {
			++lines;
		}
	}

	size_t cut = 0;
	while (lines > kMaxOutputLines && cut < _outputText.size()) {
		auto next = _outputText.find(u'\n', cut);
		if (next == WideString::npos) {
			cut = _outputText.size();
			break;
		}
		cut = next + 1;
		--lines;
	}

	if (cut > 0) {
		_outputText.erase(0, cut);
	}

	// No handler is active on a read-only field, so this takes setText's local-write path.
	_output->setText(WideStringView(_outputText));

	if (stick) {
		_output->scrollToEnd();
	}
}

void Console::clearOutput() {
	_outputText.clear();
	_output->setText(StringView());
}

void Console::submit() {
	auto text = _input->getText().str<Interface>();

	appendLine(mem_std::toString(_prompt, text));

	if (!text.empty() && (_history.empty() || _history.back() != text)) {
		_history.emplace_back(text);
	}
	_historyPos = _history.size();
	_draft.clear();

	_input->setText(StringView());

	if (_commandCallback) {
		_commandCallback(text);
	}
}

bool Console::moveHistory(int32_t direction) {
	if (_history.empty()) {
		return false;
	}

	if (_historyPos == _history.size()) {
		if (direction > 0) {
			// already at the draft, there is nothing newer
			return false;
		}
		// Stepping into the history for the first time: remember what was being typed, so stepping
		// back out restores it rather than losing it.
		_draft = _input->getText().str<Interface>();
	}

	const auto pos = int64_t(_historyPos) + direction;
	if (pos < 0) {
		return true; // at the oldest entry; consume the key so the caret does not jump instead
	}

	_historyPos = size_t(sprt::min(pos, int64_t(_history.size())));
	_input->setText(
			_historyPos < _history.size() ? StringView(_history[_historyPos]) : StringView(_draft));
	return true;
}

void Console::addInspectorCommand(Scene *scene, StringView name, StringView desc,
		Function<void(Value &&, Function<void(Value &&)> &&)> &&cb) {
	if (inspector::addCommand(scene->getContent(), name, desc, sp::move(cb))) {
		_inspectorCommands.emplace_back(name.str<Interface>());
	}
}

void Console::addInspectorCommands(Scene *scene) {
	if (!scene->getContent()) {
		return;
	}

	addInspectorCommand(scene, "console.state", "Full state of the console",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		Value input;
		input.setString(_input->getText(), "text");
		input.setInteger(int64_t(_input->getCursor().start), "cursorStart");
		input.setInteger(int64_t(_input->getCursor().length), "cursorLength");
		input.setBool(_input->isFocused(), "focused");

		Value result;
		result.setBool(true, "ok");
		result.setValue(_output->encodeState(), "output");
		result.setValue(sp::move(input), "input");
		result.setString(_prompt, "prompt");
		result.setInteger(int64_t(_history.size()), "historyCount");
		result.setInteger(int64_t(_historyPos), "historyPos");
		done(sp::move(result));
	});

	addInspectorCommand(scene, "console.write",
			"Append {text} to the output, with a newline unless {newline: false}",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		if (args.getValue("newline").isNull() || args.getBool("newline")) {
			appendLine(args.getString("text"));
		} else {
			appendOutput(args.getString("text"));
		}

		Value result;
		result.setBool(true, "ok");
		done(sp::move(result));
	});

	addInspectorCommand(scene, "console.submit",
			"Set the input line to {text} (when given) and submit it",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		if (args.isString("text")) {
			_input->setText(args.getString("text"));
		}
		submit();

		Value result;
		result.setBool(true, "ok");
		done(sp::move(result));
	});

	addInspectorCommand(scene, "console.clear", "Clear the output pane",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		clearOutput();

		Value result;
		result.setBool(true, "ok");
		done(sp::move(result));
	});

	addInspectorCommand(scene, "console.set-prompt", "Set the prompt to {text}",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		setPrompt(args.getString("text"));

		Value result;
		result.setBool(true, "ok");
		done(sp::move(result));
	});

	addInspectorCommand(scene, "console.history", "List the command history: {offset, limit}",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		const auto offset = size_t(sprt::max(args.getInteger("offset"), int64_t(0)));
		const auto requested = args.getInteger("limit");
		const auto limit = size_t(requested > 0 ? requested : 40);

		Value entries(Value::Type::ARRAY);
		for (size_t i = offset; i < _history.size() && i < offset + limit; ++i) {
			entries.addString(_history[i]);
		}

		Value result;
		result.setBool(true, "ok");
		result.setInteger(int64_t(_history.size()), "count");
		result.setValue(sp::move(entries), "entries");
		done(sp::move(result));
	});

	// The output pane is a read-only ui::TextView, so its own command handler already knows how
	// to select, copy and scroll - these only route to it under a console-shaped name.
	auto route = [this, scene](StringView name, StringView desc, StringView action) {
		auto act = action.pdup();
		addInspectorCommand(scene, name, desc,
				[this, act](Value &&args, Function<void(Value &&)> &&done) {
			Value result;
			result.setBool(_output->handleInspectorCommand(act, args, result), "ok");
			done(sp::move(result));
		});
	};

	route("console.select", "Select in the output: {line, column, endLine, endColumn}", "select");
	route("console.select-all", "Select the whole output", "select-all");
	route("console.copy", "Copy the output selection to the clipboard", "copy");
	route("console.scroll-to-end", "Scroll the output to its end", "scroll-to-end");
	route("console.lines", "List output lines: {offset, limit}", "lines");
}

} // namespace stappler::xenolith::ui
