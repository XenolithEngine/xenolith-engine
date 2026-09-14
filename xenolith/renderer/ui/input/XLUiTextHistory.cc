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

// One replacement, and its own inverse. Both directions go through the target's
// applyHistoryEdit. Both strings are captured at construction; nothing is allocated on apply.
//
// The first apply() is a no-op: the edit has already happened when the command is recorded, and
// re-applying would insert twice through the same path. Redo is the second apply.
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
		// The caret as it stood before the edit, selection included.
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

// A run never spans a newline. A manual loop, since StringViewBase has no character search.
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

	/* groupIdle stays 0: the idle window is checked here before applying, not by the bus from
	inside apply(). */
	return _bus.init(&_context, TextCommandBus::Config{.maxDepth = 0, .groupIdle = 0});
}

void TextHistory::setEnabled(bool value) {
	if (_enabled == value) {
		return;
	}
	_enabled = value;
	if (!_enabled) {
		// The open run is closed; the text is left as is.
		breakRun();
		_bus.clearHistory();
	}
}

void TextHistory::setCoalesceIdle(uint64_t idleMicros) { _idle = idleMicros; }

void TextHistory::setRecording(bool value) {
	if (_recording == value) {
		return;
	}
	// A run cannot span a pause in recording.
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
		// Backspace eats the character before the anchor; Delete eats the one after it.
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

	// The window is checked first, so a character after the pause starts the new run.
	tickIdle(now);

	RunKind kind = RunKind::None;
	if (name == NameTyping) {
		if (removed.empty() && !inserted.empty() && !TextHistory_hasNewline(inserted)) {
			// A newline ends the run.
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
		// A paste, cut, replacement or non-adjacent edit is its own entry and ends the run.
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
	// `now >= _runTouched`: a clock that went backwards must not read as a long idle.
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
	// The run in progress is committed first, so Ctrl+Z mid-word undoes that word.
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
