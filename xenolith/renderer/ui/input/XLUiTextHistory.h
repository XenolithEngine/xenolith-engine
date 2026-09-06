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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUITEXTHISTORY_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUITEXTHISTORY_H_

#include "XLTextInputManager.h" // IWYU pragma: keep - TextCursor
#include "SPCommandHistory.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// UNDO FOR TEXT, OVER THE ONE POINT WHERE TEXT ACTUALLY CHANGES.
//
// The history sits beside TextDocument rather than inside it, at the choke point that mutates it.
// A document is flat data and index arithmetic - it has no caret, no selection and no way to
// tell the platform anything. Undo has to put back the caret as well as the characters, and then
// re-push the IME window, or the next echo would be diffed against a base that no longer exists.
//
// WHY THE TARGET IS AN INTERFACE. There are two text authorities in this stack and they are not
// alike. TextView owns a TextDocument outright and edits it locally. A plain TextInput owns
// nothing: the OS-side IME owns the string, every edit is a REQUEST, and the widget renders what
// comes back. One history serves both because it never touches text itself - it asks whoever owns
// the text to do it, and that owner is also the one who can talk to the platform afterwards.
//
// TIME ARRIVES AS AN ARGUMENT, inherited from the bus below and kept for the same reason: a check
// script advances a counter and never sleeps. Nothing here reads a clock.

class TextHistory;

/* Whoever owns the text. Three questions, all in UTF-16 code units, which is the domain of every
   index in this stack - TextDocument's, TextCursor's and the IME's alike. */
class SP_PUBLIC TextHistoryTarget {
public:
	virtual ~TextHistoryTarget() = default;

	// The text about to be replaced, read BEFORE it goes. The view may not outlive the next
	// mutation, so the history copies it immediately.
	virtual WideStringView sliceForHistory(uint32_t pos, uint32_t len) const = 0;

	// Put this text where that text was. Implementations route this through their OWN single
	// insertion path, so an undo is indistinguishable from an edit to everything downstream -
	// including the window push and the change callback.
	virtual void applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted) = 0;

	// The caret an undo restores. Called after applyHistoryEdit, which has already left the
	// caret at the end of what it inserted; this is what overrides that with what was recorded.
	virtual void setHistoryCursor(TextCursor) = 0;

	/* Around every undo and every redo, because ONE entry can hold many edits - a run of
	keystrokes is exactly that.

	A target that owns its text applies them one by one and needs neither hook. A target whose text
	belongs to someone else does: each of its edits is a REQUEST computed against a string that has
	not come back yet, so N requests in a row all describe the same starting point and only the
	last one survives. Such a target folds the batch into a single request here. */
	virtual void beginHistoryBatch() { }
	virtual void endHistoryBatch() { }
};

// What a text command edits. One member, and it is the seam rather than the document: a command
// never reads text it did not keep, and the caret it restores belongs to the widget.
struct TextEditContext {
	TextHistoryTarget *target = nullptr;
};

// What a command reports having done, in the direction it was run. Carried by the bus per
// command (as opposed to per committed entry) and offered to whoever asks; the widget itself
// does not subscribe, because what it needs after an edit it already did while making it.
struct TextEditEvent {
	uint32_t pos = 0;
	uint32_t removed = 0;
	uint32_t inserted = 0;
	TextCursor cursor = TextCursor(0u);
};

using TextCommandBus = hist::CommandBus<TextEditContext, TextEditEvent>;

class SP_PUBLIC TextHistory final {
public:
	/* How long a run of keystrokes stays one undo entry. 700 ms is the pause that separates
	"still typing the same word" from "stopped and thought", and it is the one number here a
	person could reasonably want to change, so it is settable. */
	static constexpr uint64_t DefaultCoalesceIdle = 700'000;

	// The names an entry can carry. They are what getUndoName() answers and what a menu shows,
	// so they are words rather than codes, and they are literals so they outlive the command.
	static constexpr StringView NameTyping = StringView("typing");
	static constexpr StringView NameDelete = StringView("delete");
	static constexpr StringView NamePaste = StringView("paste");
	static constexpr StringView NameCut = StringView("cut");
	static constexpr StringView NameDrop = StringView("drop");
	static constexpr StringView NameReplace = StringView("replace");

	bool init(TextHistoryTarget *);

	/* OFF by default, and TextView is the only thing in the engine that turns it on for itself.
	A plain field in a property panel commits its value into somebody's document, and Ctrl+Z there
	has to take back the DOCUMENT edit; a field that silently swallowed the chord would be the way
	a person eventually undoes the wrong thing. Turning it on is one line for whoever wants it. */
	void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

	void setCoalesceIdle(uint64_t idleMicros);
	uint64_t getCoalesceIdle() const { return _idle; }

	/* Stop recording without forgetting anything, for an owner that is about to do something which
	is not an edit. Replacing a whole document is the case that matters: it is a NEW document, and
	recording it would leave the old one's text sitting in the new one's history, one Ctrl+Z away
	from a file the person never opened. */
	void setRecording(bool);
	bool isRecording() const { return _recording; }

	/* Record one replacement, called from the owner's mutation choke point BEFORE the text moves.
	`cursorBefore` is the caret as it stands right now, which is what an undo restores - so undoing
	"type over a selection" brings the selection back too.

	Answers false when nothing was recorded: disabled, or the history is applying an edit of its
	own, which is the re-entry that would otherwise turn one undo into an infinite one. */
	bool recordEdit(uint32_t pos, WideStringView removed, WideStringView inserted,
			TextCursor cursorBefore, StringView name, uint64_t now);

	/* Close a run whose idle window has passed, so the next keystroke starts a new entry rather
	than joining one the person has already stopped making.

	recordEdit() calls this itself before deciding anything, so nothing here needs a frame
	scheduled to stay correct - which is why no text widget pays for one. It is public because a
	check script has to be able to advance the clock without sleeping, and because an owner that
	already ticks per frame may as well commit runs promptly. */
	void tickIdle(uint64_t now);

	/* Close the current run explicitly. The caret moved, the focus went, a newline was typed, a
	paste landed: all of them mean the next character is a new thought. */
	void breakRun();

	bool undo();
	bool redo();

	/* A run in progress counts. While a word is being typed its keystrokes sit in an open group
	that the log has not seen yet, and a menu asking "can I undo?" in the middle of that word would
	otherwise be told no - which is false, and visibly so. undo() commits the run before taking it
	back, so the answer and the action agree. */
	bool canUndo() const { return _bus.canUndo() || _runKind != RunKind::None; }
	bool canRedo() const { return _bus.canRedo(); }

	StringView getUndoName() const {
		return _runKind != RunKind::None ? _runName : _bus.getUndoName();
	}
	StringView getRedoName() const { return _bus.getRedoName(); }

	// Forgets how the text got here without changing it. A load calls this: a history of edits to
	// a file that is no longer open would undo into a document nobody ever had.
	void clear();

	uint32_t getDepth() const { return _bus.getDepth(); }
	uint32_t getPosition() const { return _bus.getCursor(); }

	// True while undo() or redo() is running. The owner's choke point sees its own edit come
	// back through it and must not record it a second time.
	bool isApplying() const { return _applying; }

protected:
	enum class RunKind {
		None,
		Insert,
		Erase
	};

	// Whether this edit continues the run in progress, by kind and by adjacency. A typed run is
	// contiguous forward; a Backspace run eats backwards and a Delete run forwards, and both keep
	// the same anchor, which is what makes them one entry either way.
	bool continuesRun(RunKind, uint32_t pos, uint32_t removed, uint32_t inserted) const;

	TextEditContext _context;
	TextCommandBus _bus;

	bool _enabled = false;
	bool _applying = false;
	bool _recording = true;
	uint64_t _idle = DefaultCoalesceIdle;

	RunKind _runKind = RunKind::None;
	uint32_t _runAnchor = 0;
	uint64_t _runTouched = 0;
	StringView _runName;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUITEXTHISTORY_H_
