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

// Undo for text, recorded at the owner's mutation choke point. Undo restores the caret as well as
// the characters, and the owner re-pushes the IME window afterwards.
//
// The target is an interface because TextView owns its TextDocument and edits locally, while a
// plain TextInput's string is owned by the IME and every edit is a request. The history never
// touches text itself.
//
// Time is passed as an argument; nothing here reads a clock.

class TextHistory;

/* Whoever owns the text. All indices are UTF-16 code units, as in TextDocument, TextCursor and
   the IME. */
class SP_PUBLIC TextHistoryTarget {
public:
	virtual ~TextHistoryTarget() = default;

	// The text about to be replaced, read before it goes. The view may not outlive the next
	// mutation, so the history copies it immediately.
	virtual WideStringView sliceForHistory(uint32_t pos, uint32_t len) const = 0;

	// Replace text. Implementations route this through their own single insertion path, so an
	// undo also pushes the window and fires the change callback.
	virtual void applyHistoryEdit(uint32_t pos, uint32_t removed, WideStringView inserted) = 0;

	// The caret an undo restores. Called after applyHistoryEdit, which has already left the
	// caret at the end of what it inserted; this is what overrides that with what was recorded.
	virtual void setHistoryCursor(TextCursor) = 0;

	/* Around every undo and redo, since one entry can hold many edits. A target whose edits are
	requests against a string not yet echoed back folds the batch into a single request here;
	a target owning its text can ignore both. */
	virtual void beginHistoryBatch() { }
	virtual void endHistoryBatch() { }
};

// What a text command edits: the target, not the document.
struct TextEditContext {
	TextHistoryTarget *target = nullptr;
};

// What a command reports having done, in the direction it was run. Emitted by the bus per
// command, not per committed entry; the widget itself does not subscribe.
struct TextEditEvent {
	uint32_t pos = 0;
	uint32_t removed = 0;
	uint32_t inserted = 0;
	TextCursor cursor = TextCursor(0u);
};

using TextCommandBus = hist::CommandBus<TextEditContext, TextEditEvent>;

class SP_PUBLIC TextHistory final {
public:
	// Idle time, in microseconds, after which a run of keystrokes stops coalescing into one entry.
	static constexpr uint64_t DefaultCoalesceIdle = 700'000;

	// Entry names, as reported by getUndoName(). Literals, so they outlive the command.
	static constexpr StringView NameTyping = StringView("typing");
	static constexpr StringView NameDelete = StringView("delete");
	static constexpr StringView NamePaste = StringView("paste");
	static constexpr StringView NameCut = StringView("cut");
	static constexpr StringView NameDrop = StringView("drop");
	static constexpr StringView NameReplace = StringView("replace");

	bool init(TextHistoryTarget *);

	/* Off by default (TextView enables it): in a plain field Ctrl+Z must reach the owning
	document's undo. */
	void setEnabled(bool);
	bool isEnabled() const { return _enabled; }

	void setCoalesceIdle(uint64_t idleMicros);
	uint64_t getCoalesceIdle() const { return _idle; }

	/* Stop recording without forgetting anything, around a change that is not an edit, such as
	replacing the whole document. */
	void setRecording(bool);
	bool isRecording() const { return _recording; }

	/* Record one replacement, called from the owner's mutation choke point before the text
	changes. `cursorBefore` (selection included) is what an undo restores. Returns false when
	nothing was recorded: disabled, or re-entered while applying undo/redo. */
	bool recordEdit(uint32_t pos, WideStringView removed, WideStringView inserted,
			TextCursor cursorBefore, StringView name, uint64_t now);

	/* Close a run whose idle window has passed. recordEdit() calls this itself, so no per-frame
	tick is required; public for checks and for owners that tick anyway. */
	void tickIdle(uint64_t now);

	// Close the current run explicitly (caret moved, focus lost, paste, and so on).
	void breakRun();

	bool undo();
	bool redo();

	// An uncommitted run in progress counts; undo() commits it before undoing.
	bool canUndo() const { return _bus.canUndo() || _runKind != RunKind::None; }
	bool canRedo() const { return _bus.canRedo(); }

	StringView getUndoName() const {
		return _runKind != RunKind::None ? _runName : _bus.getUndoName();
	}
	StringView getRedoName() const { return _bus.getRedoName(); }

	// Forgets the history without changing the text. Called on load.
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
	// the same anchor.
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
