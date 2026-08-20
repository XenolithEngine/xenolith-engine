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

#ifndef STAPPLER_CORE_UTILS_SPCOMMANDHISTORY_H_
#define STAPPLER_CORE_UTILS_SPCOMMANDHISTORY_H_

#include "SPCommon.h"
#include "SPMemory.h"

/* ONE UNDO HISTORY, AND NOTHING ABOUT WHAT IS BEING EDITED.

A header-only template that costs nothing to anyone who does not use it. Named consumers, both real
and both present in the tree: Xenolith Studio's two editors - the graph editor's EdCommandBus and
the component editor's CeCommandBus, where the shape first appeared three times - and the engine's
own ui::TextHistory, which is what gives ui::TextView and ui::CodeEditor a Ctrl+Z.

It lives HERE, below the renderer, for a reason that is a build invariant rather than a preference:
the studio's test binary links both editors' histories and NOT ONE xenolith_* module. A common home
above stappler_core would drag a window system into a program that renders nothing.

What is here is exactly what the consumers agreed on and nothing they disagreed about:

  - ONE LOG PLUS A CURSOR, not two stacks. Entries at or past the cursor are the redo tail, and they
    are dropped when a new edit is committed.
  - REDO IS A SECOND apply(). There is no redo method, which puts one rule on every command:
    anything it ALLOCATES - an id, a name in a trash directory - is allocated on the FIRST apply and
    remembered, because an id handed out again on redo would make the redone document a different
    document though an isomorphic one.
  - TRANSACTIONS, where a nested one joins the outer and only the outermost commits, and a body that
    refuses rolls the whole thing back in reverse and leaves the history untouched.
  - GROUPS, so that a drag of a hundred steps is one entry, closed explicitly or by an idle window.
    A run of keystrokes coalescing into one undo entry is the same mechanism, from the other end.
  - TIME ARRIVES AS AN ARGUMENT. Nothing here reads a clock: a test advances a counter and never
    sleeps, and a UI calls tickIdle() once a frame.

What is NOT here, deliberately: a WITNESS of the outside world and the rule "a refused undo drops
the log". Those exist in the studio shell's own file history because the filesystem is a shared
world that another program may be writing to, and a document a program owns outright is not.
Generalizing over that difference would mean giving every document editor a policy it has no use
for; the shell keeps its own history until it needs this one.

The containers are mem_std deliberately, not an Interface parameter. This is a log of pointers to
objects the caller new'd, whose lifetime is the history's own and never a memory pool's; every
consumer instantiates it the same way, and a parameter nobody varies is a parameter that only makes
the type name longer.

The two parameters are the whole seam. `Context` is what a command edits - for the graph editor a
document plus a selection, because undo takes back the click as well as the edit; for the text
editor a document plus a caret, for the same reason - and `Event` is what it reports having done.
Both are the caller's types, and this file knows nothing about either.
*/

namespace STAPPLER_VERSIONIZED stappler::hist {

// Which way a command was run. Redo is a second apply, so it is Forward too.
enum class Direction : uint8_t {
	Forward,
	Backward
};

// The one thing in this module that is not a template: the name of a direction.
//
// A table rather than a switch for the same reason every other name table here is one - it is read
// by a log, by a golden dump and by whatever drives the editor from a socket, and those three must
// spell it identically.
inline StringView getDirectionName(Direction d) {
	switch (d) {
	case Direction::Forward: return StringView("forward");
	case Direction::Backward: return StringView("backward");
	}
	return StringView("?");
}

// One undoable edit.
//
// The inverse data lives in the command's own fields rather than in a payload the bus hands back:
// the history owns the command object anyway, so the record can stay statically typed instead of
// being erased into a Value and read back by name.
template <typename Context, typename Event>
class Command {
public:
	using EventSink = Callback<void(const Event &)>;

	virtual ~Command() = default;

	// The history entry's name: what an event log reports and what a dump of the history reads as.
	// Points at a literal, so it outlives the command.
	virtual StringView getName() const = 0;

	virtual Status apply(Context &) = 0;
	virtual Status undo(Context &) = 0;

	// What that did, said precisely, in whichever direction it was run.
	//
	// Called by the bus AFTER the mutation landed, so a command reports what is now true - which is
	// how undoing a move reports the position it restored. It also means a command must not read
	// something it has just removed: a composite removal describes its parts BEFORE removing them
	// going forward, and after restoring them coming back, for exactly that reason.
	virtual void describeEvents(const Context &, Direction, const EventSink &) const = 0;
};

template <typename Context, typename Event>
class CommandBus final {
public:
	using CommandType = Command<Context, Event>;

	struct Config {
		// 0 = unbounded, which is what an editor wants until someone measures otherwise. When set,
		// the oldest entry is dropped and the cursor moves with it.
		uint32_t maxDepth = 0;

		// Microseconds of inactivity after which an open group closes itself; 0 = only endGroup()
		// closes it. Nothing here reads a clock - see apply()/tickIdle().
		uint64_t groupIdle = 0;
	};

	// The bus notifies once per COMMITTED ENTRY, not once per command: a listener re-validates the
	// whole document, and a group of a hundred moves must not cost a hundred validation passes.
	// Per-command events are handleEvent's, and land beside this rather than replacing it.
	struct Observer {
		virtual ~Observer() = default;

		// The document changed and the change is now visible. Fires on commit, undo and redo.
		virtual void handleDocumentChanged() { }

		// canUndo()/canRedo() may have moved. Fires whenever the log or the cursor does.
		virtual void handleHistoryChanged() { }

		// One per COMMAND, including inside a transaction or a group, and including undo and redo.
		// This is the fine-grained half: handleDocumentChanged above stays per entry, because its
		// listener revalidates the whole document.
		virtual void handleEvent(const Event &) { }
	};

	~CommandBus() {
		for (auto &e : _log) { clearEntry(e); }
		_log.clear();

		// A group's buffer is this object's; an open one holds commands that were applied and never
		// committed, and they are still owned here. The document keeps whatever they did - a
		// destructor is not an undo.
		if (_group) {
			for (auto *c : *_group) { delete c; }
			delete _group;
			_group = nullptr;
		}

		// _transaction is deliberately NOT touched: it points at a vector on transaction()'s stack
		// frame, so it is non-null only while that frame is running - and a bus cannot be destroyed
		// from inside its own transaction. Deleting it would be freeing a stack address.
	}

	CommandBus() = default;
	CommandBus(const CommandBus &) = delete;
	CommandBus &operator=(const CommandBus &) = delete;

	bool init(Context *ctx, const Config &cfg) {
		if (!ctx) {
			return false;
		}
		_context = ctx;
		_config = cfg;
		return true;
	}
	bool init(Context *c) { return init(c, Config()); }

	// Takes ownership. A command whose apply() refuses is deleted at once and never enters the
	// history: a failed edit is not an edit.
	//
	// `now` is the caller's clock in microseconds, and the ONLY way time enters this class. It
	// resets an open group's idle window, and closes that group first if the window has already
	// passed. Callers that use no group pass nothing.
	Status apply(CommandType *command, uint64_t now = 0) {
		if (!command) {
			return Status::ErrorInvalidArguemnt;
		}
		if (!_context) {
			delete command;
			return Status::ErrorInvalidArguemnt;
		}

		// An idle group closes BEFORE this command is routed, so the command that arrives after the
		// window has passed starts an entry of its own rather than joining a group that is over.
		tickIdle(now);

		if (auto st = command->apply(*_context); st != Status::Ok) {
			// A refused edit is not an edit: nothing enters the history, it says nothing, and the
			// command dies here.
			delete command;
			return st;
		}

		dispatch(*command, Direction::Forward);

		if (_transaction) {
			_transaction->emplace_back(command);
			return Status::Ok;
		}

		if (_group) {
			_group->emplace_back(command);
			_groupTouched = now;
			return Status::Ok;
		}

		mem_std::Vector<CommandType *> one;
		one.emplace_back(command);
		pushEntry(sprt::move(one));

		notifyDocument();
		notifyHistory();
		return Status::Ok;
	}

	// Close an open group if its idle window has passed. This is the deterministic stand-in for a
	// timer: time arrives from outside, so a test advances a counter instead of sleeping, and the
	// UI calls this per frame.
	void tickIdle(uint64_t now) {
		if (!_group || _groupIdle == 0 || _transaction) {
			return;
		}
		if (now >= _groupTouched && now - _groupTouched >= _groupIdle) {
			endGroup();
		}
	}

	bool undo() {
		if (!canUndo() || !_context) {
			return false;
		}

		// The cursor moves first, so an observer reading canUndo()/canRedo() sees the state that
		// the undo produced rather than the one it started from.
		--_cursor;
		auto &entry = _log[_cursor];
		for (size_t i = entry.commands.size(); i > 0; --i) {
			// A refusal here is not recoverable and not reported: the entry has already been half
			// undone, and there is nowhere consistent to stop. Commands are written so it cannot
			// happen - the document they undo is the one they applied to.
			entry.commands[i - 1]->undo(*_context);
			dispatch(*entry.commands[i - 1], Direction::Backward);
		}

		notifyDocument();
		notifyHistory();
		return true;
	}

	bool redo() {
		if (!canRedo() || !_context) {
			return false;
		}

		auto &entry = _log[_cursor];
		++_cursor;
		// Forward order, and re-applying is all redo is.
		for (auto *c : entry.commands) {
			c->apply(*_context);
			dispatch(*c, Direction::Forward);
		}

		notifyDocument();
		notifyHistory();
		return true;
	}

	bool canUndo() const { return _cursor > 0; }
	bool canRedo() const { return _cursor < uint32_t(_log.size()); }

	/* WHAT undo would take back, not whether it could. A menu item that says "Undo" when it could
	say "Undo rename field" is how a person eventually undoes the wrong thing in the wrong history,
	which is the whole reason a shell arbitrating two of them needs to name them.

	An entry's name is its FIRST command's: a transaction is named for the operation that opened it,
	and a group of a hundred identical keystrokes is named the same whichever one is asked. Empty
	when there is nothing on that side. The result points at whatever Command::getName() returns,
	which the contract there requires to outlive the command. */
	StringView getUndoName() const {
		if (!canUndo()) {
			return StringView();
		}
		auto &e = _log[_cursor - 1];
		return e.commands.empty() ? StringView() : e.commands.front()->getName();
	}

	StringView getRedoName() const {
		if (!canRedo()) {
			return StringView();
		}
		auto &e = _log[_cursor];
		return e.commands.empty() ? StringView() : e.commands.front()->getName();
	}

	// A nested call joins the outer one: only the outermost commits, so a multi-step operation built
	// out of smaller ones is still a SINGLE undo entry. A body that returns anything but success
	// rolls the whole transaction back, in reverse, and leaves the history untouched.
	//
	// With a group open the transaction folds into the group for the same reason.
	Status transaction(const Callback<Status()> &fn) {
		if (!_context) {
			return Status::ErrorInvalidArguemnt;
		}

		// A nested transaction joins the outer one, and so does one opened inside a group: only the
		// outermost scope commits, so a high-level operation assembled out of smaller ones is still
		// one undo entry. Transactions win over groups - a single nesting policy.
		if (_transaction || _group) {
			return fn();
		}

		mem_std::Vector<CommandType *> buffer;
		_transaction = &buffer;

		auto st = fn();

		_transaction = nullptr;

		if (st != Status::Ok) {
			// Roll back in reverse. A failing undo does not stop the rest: the alternative is to
			// leave the earlier commands applied, which is worse than an imperfect rollback.
			for (size_t i = buffer.size(); i > 0; --i) {
				buffer[i - 1]->undo(*_context);
				dispatch(*buffer[i - 1], Direction::Backward);
			}
			for (auto *c : buffer) { delete c; }

			// The history never saw this transaction, so nothing about it changed. The document did
			// move and come back, though, so listeners are told to look again.
			if (!buffer.empty()) {
				notifyDocument();
			}
			return st;
		}

		if (buffer.empty()) {
			return st;
		}

		pushEntry(sprt::move(buffer));
		notifyDocument();
		notifyHistory();
		return st;
	}

	// No-op when a group or a transaction is already open, and specifically does NOT reset the
	// buffer - so two nested begins do not lose the first one's commands. An empty group leaves no
	// entry; endGroup() is idempotent.
	void beginGroup(uint64_t idleTimeout = 0, uint64_t now = 0) {
		// Re-entrant safe by doing nothing: a second begin must not drop the commands the first one
		// has already collected.
		if (_group || _transaction) {
			return;
		}
		_group = new mem_std::Vector<CommandType *>();
		_groupIdle = idleTimeout ? idleTimeout : _config.groupIdle;
		_groupTouched = now;
	}

	void endGroup() {
		if (!_group) {
			return; // idempotent
		}

		auto *buffer = _group;
		_group = nullptr;
		_groupIdle = 0;
		_groupTouched = 0;

		const bool any = !buffer->empty();
		pushEntry(sprt::move(*buffer));
		delete buffer;

		if (any) {
			notifyDocument();
			notifyHistory();
		}
	}

	bool isGroupOpen() const { return _group; }
	bool isTransactionOpen() const { return _transaction; }

	// Drops the whole log. The document is left exactly as it is - this forgets how it got there,
	// it does not undo anything. A load calls it, because a history of edits to a document that is
	// no longer open would undo into a document nobody ever had.
	void clearHistory() {
		for (auto &e : _log) { clearEntry(e); }
		_log.clear();
		_cursor = 0;
		notifyHistory();
	}

	uint32_t getDepth() const { return uint32_t(_log.size()); }
	uint32_t getCursor() const { return _cursor; }

	Context *getContext() const { return _context; }

	void addObserver(Observer *observer) {
		for (auto *o : _observers) {
			if (o == observer) {
				return;
			}
		}
		_observers.emplace_back(observer);
	}

	void removeObserver(Observer *observer) {
		for (auto it = _observers.begin(); it != _observers.end(); ++it) {
			if (*it == observer) {
				_observers.erase(it);
				return;
			}
		}
	}

private:
	// A group and a transaction produce the SAME entry shape, and a lone command is an entry of one.
	struct Entry {
		mem_std::Vector<CommandType *> commands;
	};

	void clearEntry(Entry &e) {
		for (auto *c : e.commands) { delete c; }
		e.commands.clear();
	}

	void truncateRedo() {
		// The redo tail is dropped when a new edit is committed - not when a command lands in an
		// open buffer, because a transaction that rolls back must leave the tail where it was.
		while (uint32_t(_log.size()) > _cursor) {
			clearEntry(_log.back());
			_log.pop_back();
		}
	}

	void enforceDepth() {
		if (_config.maxDepth == 0) {
			return;
		}
		while (uint32_t(_log.size()) > _config.maxDepth) {
			clearEntry(_log.front());
			_log.erase(_log.begin());
			if (_cursor > 0) {
				--_cursor;
			}
		}
	}

	void pushEntry(mem_std::Vector<CommandType *> &&commands) {
		if (commands.empty()) {
			return; // an empty transaction or group leaves no trace
		}
		truncateRedo();
		Entry e;
		e.commands = sprt::move(commands);
		_log.emplace_back(sprt::move(e));
		++_cursor;
		enforceDepth();
	}

	void notifyDocument() {
		for (auto *o : _observers) { o->handleDocumentChanged(); }
	}

	void notifyHistory() {
		for (auto *o : _observers) { o->handleHistoryChanged(); }
	}

	// A copy of the list, so a listener may unsubscribe - or subscribe - from inside its own handler
	// without the walk losing its place.
	void dispatch(const CommandType &command, Direction direction) {
		mem_std::Vector<Observer *> snapshot = _observers;
		command.describeEvents(*_context, direction, [&](const Event &event) {
			for (auto *o : snapshot) { o->handleEvent(event); }
		});
	}

	Context *_context = nullptr;
	Config _config;

	mem_std::Vector<Entry> _log;
	uint32_t _cursor = 0;

	// Open buffers. A transaction wins over a group when both are open.
	mem_std::Vector<CommandType *> *_transaction = nullptr;
	mem_std::Vector<CommandType *> *_group = nullptr;
	uint64_t _groupIdle = 0;
	uint64_t _groupTouched = 0;

	mem_std::Vector<Observer *> _observers;
};

} // namespace stappler::hist

#endif /* STAPPLER_CORE_UTILS_SPCOMMANDHISTORY_H_ */
