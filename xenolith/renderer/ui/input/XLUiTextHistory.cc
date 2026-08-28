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

#include "XLUiTextHistory.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// One replacement, and its own inverse.
//
// Both directions go through the target's applyHistoryEdit, which is the widget's ordinary
// insertion path - so an undo pushes a window, fires a change callback and repaints exactly as a
// typed character does, and nothing downstream has to know the difference.
//
// Nothing here is ALLOCATED on apply, so the bus's one rule for commands ("whatever you allocate,
// allocate on the first apply and remember it") is satisfied trivially: both strings are captured
// when the command is built and redo re-inserts the same characters it inserted the first time.
//
// THE FIRST apply() IS A NO-OP, and it has to be. Everywhere else a command is what MAKES the
// edit, so the bus applies it on the way in. Here the edit has already happened - a person typed
// a character, or the platform echoed one - and this command exists to record it. Doing it again
// would insert the text twice and, because the widget's insertion path is the same one this calls,
// recurse until the stack runs out. Redo is the second apply, exactly as the bus's charter says,
// and that one does the work.
class TextReplaceCommand final : public hist::Command<TextEditContext, TextEditEvent> {
public:
	TextReplaceCommand(uint32_t pos, WideStringView removed, WideStringView inserted,
			TextCursor cursorBefore, StringView name)
	: _pos(pos)
	, _removed(removed.str<Interface>())
	, _inserted(inserted.str<Interface>())
	, _cursorBefore(cursorBefore)
	, _name(name) { }

	virtual StringView getName() const override { return _name; }

	virtual Status apply(TextEditContext &ctx) override {
		if (_recorded) {
			// The edit this records is already in the text; see the note above.
			_recorded = false;
			return Status::Ok;
		}
		if (!ctx.target) {
			return Status::ErrorInvalidArguemnt;
		}
		ctx.target->applyHistoryEdit(_pos, uint32_t(_removed.size()), WideStringView(_inserted));
		ctx.target->setHistoryCursor(TextCursor(_pos + uint32_t(_inserted.size())));
		return Status::Ok;
	}

	virtual Status undo(TextEditContext &ctx) override {
		if (!ctx.target) {
			return Status::ErrorInvalidArguemnt;
		}
		ctx.target->applyHistoryEdit(_pos, uint32_t(_inserted.size()), WideStringView(_removed));
		// The caret as it stood before the edit, selection and all: undoing a "type over the
		// selection" has to give the selection back, or the next keystroke would not be able to
		// repeat what was just undone.
		ctx.target->setHistoryCursor(_cursorBefore);
		return Status::Ok;
	}

	virtual void describeEvents(const TextEditContext &, hist::Direction dir,
			const EventSink &sink) const override {
		TextEditEvent ev;
		ev.pos = _pos;
		if (dir == hist::Direction::Forward) {
			ev.removed = uint32_t(_removed.size());
			ev.inserted = uint32_t(_inserted.size());
			ev.cursor = TextCursor(_pos + uint32_t(_inserted.size()));
		} else {
			ev.removed = uint32_t(_inserted.size());
			ev.inserted = uint32_t(_removed.size());
			ev.cursor = _cursorBefore;
		}
		sink(ev);
	}

	uint32_t getPos() const { return _pos; }

protected:
	// Set at construction and cleared by the first apply(): "the edit I describe has happened".
	bool _recorded = true;

	uint32_t _pos = 0;
	WideString _removed;
	WideString _inserted;
	TextCursor _cursorBefore;
	StringView _name;
};

// A newline ends the thought, so a run never spans one. Written out rather than searched for
// because StringViewBase has no find-a-character: the string here is one keystroke long in the
// case that matters.
static bool TextHistory_hasNewline(WideStringView str) {
	for (auto &c : str) {
		if (c == u'\n') {
			return true;
		}
	}
	return false;
}

bool TextHistory::init(TextHistoryTarget *target) {
	if (!target) {
		return false;
	}
	_context.target = target;

	/* groupIdle stays 0 on purpose: the bus would then close a group from inside apply(), and
	this class would learn about it only afterwards - leaving the keystroke that outran the window
	as an entry of one, followed by a new run starting at the NEXT character. One clock, one
	decider; the window is checked here, before anything is applied. */
	return _bus.init(&_context, TextCommandBus::Config{.maxDepth = 0, .groupIdle = 0});
}

void TextHistory::setEnabled(bool value) {
	if (_enabled == value) {
		return;
	}
	_enabled = value;
	if (!_enabled) {
		// Whatever was half-collected stops being collectable. The text keeps whatever it has -
		// turning a history off is not an undo.
		breakRun();
		_bus.clearHistory();
	}
}

void TextHistory::setCoalesceIdle(uint64_t idleMicros) { _idle = idleMicros; }

void TextHistory::setRecording(bool value) {
	if (_recording == value) {
		return;
	}
	// A run cannot span the gap: whatever the owner is doing while recording is off is precisely
	// the thing the next keystroke must not be glued to.
	breakRun();
	_recording = value;
}

bool TextHistory::continuesRun(RunKind kind, uint32_t pos, uint32_t removed,
		uint32_t inserted) const {
	if (_runKind != kind) {
		return false;
	}
	switch (kind) {
	case RunKind::Insert:
		// Typing continues where the last character landed.
		return pos == _runAnchor;
	case RunKind::Erase:
		// Backspace eats the character before the anchor; Delete eats the one after it. Either
		// way the caret stays put, which is why both stay one entry.
		return pos + removed == _runAnchor || pos == _runAnchor;
	case RunKind::None: break;
	}
	return false;
}

bool TextHistory::recordEdit(uint32_t pos, WideStringView removed, WideStringView inserted,
		TextCursor cursorBefore, StringView name, uint64_t now) {
	if (!_enabled || _applying || !_recording) {
		return false;
	}
	if (removed.empty() && inserted.empty()) {
		return false; // an edit that changed nothing is not an edit
	}

	// The window is checked before the decision, not after: a character that arrives after the
	// pause has to START the new run rather than land alone between two of them.
	tickIdle(now);

	RunKind kind = RunKind::None;
	if (name == NameTyping) {
		if (removed.empty() && !inserted.empty() && !TextHistory_hasNewline(inserted)) {
			// A newline ends the thought: it is where an undo most usefully stops.
			kind = RunKind::Insert;
		} else if (inserted.empty() && !removed.empty()) {
			kind = RunKind::Erase;
		}
	} else if (name == NameDelete) {
		if (inserted.empty() && !removed.empty()) {
			kind = RunKind::Erase;
		}
	}

	const bool joins = kind != RunKind::None
			&& continuesRun(kind, pos, uint32_t(removed.size()), uint32_t(inserted.size()));

	if (!joins) {
		// A paste, a cut, a replacement, a jump elsewhere - each is its own entry, and each ends
		// whatever run was in progress.
		breakRun();
	}

	if (kind != RunKind::None && !_bus.isGroupOpen()) {
		_bus.beginGroup(0, now);
	}

	auto cmd = new TextReplaceCommand(pos, removed, inserted, cursorBefore, name);
	if (_bus.apply(cmd, now) != Status::Ok) {
		breakRun();
		return false;
	}

	if (kind == RunKind::None) {
		// Committed on its own; nothing may join it.
		breakRun();
	} else {
		_runKind = kind;
		_runAnchor = (kind == RunKind::Insert) ? pos + uint32_t(inserted.size()) : pos;
		_runTouched = now;
		_runName = name;
	}
	return true;
}

void TextHistory::tickIdle(uint64_t now) {
	if (_runKind == RunKind::None) {
		return;
	}
	// The `now >= _runTouched` guard is the bus's, for the same reason: a clock that went
	// backwards must not read as an eternity of silence.
	if (now >= _runTouched && now - _runTouched >= _idle) {
		breakRun();
	}
}

void TextHistory::breakRun() {
	_runKind = RunKind::None;
	_runAnchor = 0;
	_runTouched = 0;
	_runName = StringView();
	_bus.endGroup();
}

bool TextHistory::undo() {
	if (!_enabled) {
		return false;
	}
	// The run in progress is committed FIRST, and the question is asked afterwards: Ctrl+Z in the
	// middle of a word takes back the word, not the entry before it - and while that word is the
	// only thing in the history, asking the log first would answer "nothing to undo".
	breakRun();
	if (!_bus.canUndo()) {
		return false;
	}

	_applying = true;
	_context.target->beginHistoryBatch();
	const auto ret = _bus.undo();
	_context.target->endHistoryBatch();
	_applying = false;
	return ret;
}

bool TextHistory::redo() {
	if (!_enabled) {
		return false;
	}
	breakRun();
	if (!_bus.canRedo()) {
		return false;
	}

	_applying = true;
	_context.target->beginHistoryBatch();
	const auto ret = _bus.redo();
	_context.target->endHistoryBatch();
	_applying = false;
	return ret;
}

void TextHistory::clear() {
	breakRun();
	_bus.clearHistory();
}

} // namespace stappler::xenolith::ui
