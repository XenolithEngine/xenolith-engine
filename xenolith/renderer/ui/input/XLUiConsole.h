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

// The prompt line. A plain single-line ui::TextInput is the right base: Enter already reaches the
// enter callback, the clipboard chords and the gesture set are already there, and none of the
// multi-line machinery is wanted on one line.
//
// One thing has to change. The stock handleKey sends Up and Down to the ends of the string, which
// on a single line is a reasonable reading of "there is nowhere to go vertically" — but on a
// console prompt those two keys are the history, and that is the whole reason this class exists.
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

The output pane is a read-only ui::TextView, and that single fact is the whole answer to "selection
and copying over the output" — a read-only view still takes taps, drag-selection and Ctrl+A/Ctrl+C
(see ui::TextView::handleTextHotkey, which exists mostly for this). There is no second selection
mechanism anywhere in this class.

The prompt is a separate label, never part of the input string: it cannot then be deleted, cannot be
copied by accident, and does not offset any cursor arithmetic.

Output is a ring: whole lines are dropped off the front once the line count passes the limit. There
is no character ceiling — the pane renders a Label per visible line — so the ring's only job is
keeping an immortal console from growing without bound.

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

	// Registers through here so handleExit() can drop the lot: a command whose lambda captured a
	// destroyed widget is a dangling call from the inspector socket.
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
