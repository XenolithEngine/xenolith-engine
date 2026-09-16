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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICONSOLE_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICONSOLE_H_

#include "XLUiTextView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The prompt line: a single-line ui::TextInput where Up and Down walk the history instead of
// moving the caret to the ends of the string.
class SP_PUBLIC ConsoleInput : public TextInput {
public:
	// `direction` is -1 for older, +1 for newer. Return true to consume the key.
	using HistoryCallback = Function<bool(int32_t direction)>;

	virtual ~ConsoleInput() = default;

	virtual bool init() override;

	void setHistoryCallback(HistoryCallback &&cb) { _historyCallback = sp::move(cb); }

protected:
	virtual bool handleKey(const GestureData &) override;

	HistoryCallback _historyCallback;
};

/* Console I/O: an append-only output pane over a prompt row.

The output pane is a read-only ui::TextView, which provides selection and Ctrl+A/Ctrl+C
(ui::TextView::handleTextHotkey).

The prompt is a separate label, not part of the input string, so it cannot be deleted or copied and
does not affect cursor positions.

Output is a ring of at most kMaxOutputLines lines; whole lines are dropped from the front.

CSS: the widget is a plain Node carrying the class `console`, over `.console-output` (a
ui::TextView, so everything that widget publishes applies), `.console-row`, `.console-prompt` (the
prompt label) and `.console-input` (a ui::TextInput). */
class SP_PUBLIC Console : public Node {
public:
	using CommandCallback = Function<void(StringView)>;

	static constexpr size_t kMaxOutputLines = 2'000;

	virtual ~Console() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	void appendOutput(StringView);
	void appendLine(StringView);
	void clearOutput();

	void setPrompt(StringView);
	StringView getPrompt() const { return _prompt; }

	void setCommandCallback(CommandCallback &&cb) { _commandCallback = sp::move(cb); }

	// Echoes the input line to the output, pushes it onto the history and hands it to the callback.
	void submit();

	TextView *getOutput() const { return _output; }
	ConsoleInput *getInput() const { return _input; }

protected:
	bool moveHistory(int32_t direction);

	void addInspectorCommands(Scene *);

	// Registers the command so handleExit() can remove it; the lambdas capture this widget.
	void addInspectorCommand(Scene *, StringView name, StringView desc,
			Function<void(Value &&, Function<void(Value &&)> &&)> &&);

	TextView *_output = nullptr;
	Node *_row = nullptr;
	basic2d::Label *_promptLabel = nullptr;
	ConsoleInput *_input = nullptr;

	CommandCallback _commandCallback;

	WideString _outputText;
	Vector<String> _history;
	String _prompt;

	// What was being typed before the history was walked into, so walking back out restores it.
	String _draft;

	// _history.size() means "not in the history, showing the draft".
	size_t _historyPos = 0;

	Vector<String> _inspectorCommands;
	Scene *_inspectorScene = nullptr;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICONSOLE_H_
