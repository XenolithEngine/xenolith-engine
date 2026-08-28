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


// hist::CommandBus: one log plus a cursor, transactions, groups, and who owns what.
//
// The assertions are a port of the ones that guarded this code at its previous address (Xenolith
// Studio's studio_hist, inside the graph editor's edit-commands section), and they are repeated
// here rather than pointed at because the code lives here now: a module that cannot fail on its own
// tests is a module nobody can change safely. The studio's section stays exactly as it was, which
// is the proof that the move changed the address and not the behaviour.
//
// The commands touch NO document, deliberately. History, cursor, transactions and groups are
// properties of the bus, and mixing a subject into them would only make a failure harder to read -
// which is also why the two template parameters here are the smallest things that satisfy them.
//
// Time is an argument, never a clock: the group's idle window is driven by a counter, so this runs
// identically on every target and never sleeps.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPCommandHistory.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

// The smallest possible pair of parameters. `Context` is what a command edits and `Event` is what
// it reports; neither is looked at by the bus, and this section is about the bus.
struct TestContext {
	mem_std::String applied;
	mem_std::String undone;
};

struct TestEvent {
	char name = 0;
};

using TestBus = hist::CommandBus<TestContext, TestEvent>;
using TestCommand = hist::Command<TestContext, TestEvent>;

// A command that does nothing but say it ran. `fails` makes apply() refuse, which is how a
// transaction is made to roll back - nothing here throws, so a refusal is the only way a command
// can say "no".
class MarkCommand final : public TestCommand {
public:
	MarkCommand(char name, bool fails = false) : _name(name), _fails(fails) { }

	virtual StringView getName() const override { return _label; }

	virtual Status apply(TestContext &ctx) override {
		if (_fails) {
			return Status::ErrorInvalidArguemnt;
		}
		ctx.applied.push_back(_name);
		return Status::Ok;
	}

	virtual Status undo(TestContext &ctx) override {
		ctx.undone.push_back(_name);
		return Status::Ok;
	}

	virtual void describeEvents(const TestContext &, hist::Direction,
			const EventSink &sink) const override {
		sink(TestEvent{_name});
	}

	// Names point at literals, as the contract requires: the bus hands them out from getUndoName()
	// long after the call that made the command has returned.
	void setLabel(StringView label) { _label = label; }

private:
	char _name;
	bool _fails;
	StringView _label = StringView("Mark");
};

TestCommand *mark(char name) { return new MarkCommand(name); }
TestCommand *failing() { return new MarkCommand('!', true); }

TestCommand *named(char name, StringView label) {
	auto cmd = new MarkCommand(name);
	cmd->setLabel(label);
	return cmd;
}

// ---- ownership ---------------------------------------------------------------------------------
//
// The bus owns commands through bare pointers, and there are six paths on which it has to free one:
// a refused apply, a truncated redo tail, an entry evicted by the depth bound, clearHistory, a
// rolled-back transaction, and the destructor. A leak on any of them would be invisible to every
// other check here - the trace would still read correctly.
//
// Counting is the check rather than a sanitizer, because it is deterministic and needs no tooling.
uint32_t s_liveCommands = 0;

class CountedCommand final : public TestCommand {
public:
	explicit CountedCommand(bool fails = false) : _fails(fails) { ++s_liveCommands; }
	virtual ~CountedCommand() { --s_liveCommands; }

	virtual StringView getName() const override { return StringView("Counted"); }
	virtual Status apply(TestContext &) override {
		return _fails ? Status::ErrorInvalidArguemnt : Status::Ok;
	}
	virtual Status undo(TestContext &) override { return Status::Ok; }
	virtual void describeEvents(const TestContext &, hist::Direction,
			const EventSink &) const override { }

private:
	bool _fails;
};

void testOwnership() {
	TestContext ctx;
	const auto liveAtStart = s_liveCommands;

	{
		// The destructor: everything the log holds, plus an open group that never committed.
		TestBus bus;
		bus.init(&ctx);
		bus.apply(new CountedCommand());
		bus.apply(new CountedCommand());
		bus.beginGroup();
		bus.apply(new CountedCommand());
		check(s_liveCommands == liveAtStart + 3,
				"command-history: the bus is holding three commands");
	}
	check(s_liveCommands == liveAtStart,
			"command-history: the destructor frees the log and any open group");

	{
		// A refused apply: the command dies at once and never enters the history.
		TestBus bus;
		bus.init(&ctx);
		check(bus.apply(new CountedCommand(true)) != Status::Ok,
				"command-history: a refused command reports failure");
		check(s_liveCommands == liveAtStart && bus.getDepth() == 0,
				"command-history: a refused command is freed and leaves no entry");
	}

	{
		// The truncated redo tail.
		TestBus bus;
		bus.init(&ctx);
		bus.apply(new CountedCommand());
		bus.apply(new CountedCommand());
		bus.undo();
		bus.undo();
		bus.apply(new CountedCommand()); // drops the two undone entries
		check(s_liveCommands == liveAtStart + 1,
				"command-history: a truncated redo tail frees its commands");
	}
	check(s_liveCommands == liveAtStart, "command-history: ... and the rest go with the bus");

	{
		// Eviction by the depth bound.
		TestBus bus;
		TestBus::Config cfg;
		cfg.maxDepth = 2;
		bus.init(&ctx, cfg);
		bus.apply(new CountedCommand());
		bus.apply(new CountedCommand());
		bus.apply(new CountedCommand());
		check(s_liveCommands == liveAtStart + 2,
				"command-history: the entry dropped by the depth bound is freed");
	}

	{
		// A rolled-back transaction.
		TestBus bus;
		bus.init(&ctx);
		bus.transaction([&]() -> Status {
			bus.apply(new CountedCommand());
			bus.apply(new CountedCommand());
			return bus.apply(new CountedCommand(true));
		});
		check(s_liveCommands == liveAtStart,
				"command-history: a rolled-back transaction frees everything it applied");
	}

	{
		TestBus bus;
		bus.init(&ctx);
		bus.apply(new CountedCommand());
		bus.apply(new CountedCommand());
		bus.clearHistory();
		check(s_liveCommands == liveAtStart, "command-history: clearHistory frees the commands");
	}

	check(s_liveCommands == liveAtStart, "command-history: no command outlives its bus");
}

void testMechanics() {
	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		check(!bus.canUndo() && !bus.canRedo(), "command-history: a fresh bus has nothing to undo");

		bus.apply(mark('A'));
		bus.apply(mark('B'));
		checkEq(StringView(ctx.applied), StringView("AB"),
				"command-history: apply runs each command");
		check(bus.getDepth() == 2 && bus.getCursor() == 2,
				"command-history: each command is its own history entry");
		check(bus.canUndo() && !bus.canRedo(),
				"command-history: two edits can be undone, none redone");

		check(bus.undo(), "command-history: undo reports that it did something");
		checkEq(StringView(ctx.undone), StringView("B"),
				"command-history: undo takes the last edit");
		check(bus.canUndo() && bus.canRedo(), "command-history: the cursor sits between the two");

		check(bus.redo(), "command-history: redo reports that it did something");
		checkEq(StringView(ctx.applied), StringView("ABB"),
				"command-history: redo is a second apply, not a third method");

		check(bus.undo() && bus.undo(), "command-history: both edits undo");
		check(!bus.undo(), "command-history: undoing past the start reports false");
		checkEq(StringView(ctx.undone), StringView("BBA"),
				"command-history: undo runs in reverse order");
	}

	{
		// A new edit drops the redo tail: the future the author walked away from is gone.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		bus.apply(mark('A'));
		bus.apply(mark('B'));
		bus.undo();
		check(bus.canRedo(), "command-history: an undone edit can be redone");

		bus.apply(mark('C'));
		check(!bus.canRedo() && bus.getDepth() == 2,
				"command-history: a new edit truncates the redo tail");
	}

	{
		// The depth bound: the oldest entry goes and the cursor moves with it, so what remains
		// still lines up with where the author is.
		TestContext ctx;
		TestBus bus;
		TestBus::Config cfg;
		cfg.maxDepth = 2;
		bus.init(&ctx, cfg);

		bus.apply(mark('A'));
		bus.apply(mark('B'));
		bus.apply(mark('C'));
		check(bus.getDepth() == 2 && bus.getCursor() == 2,
				"command-history: the history stops at maxDepth");

		bus.undo();
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("CB"),
				"command-history: what survives the bound is the newest");
		check(!bus.undo(), "command-history: the dropped entry cannot be undone");
	}

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.apply(mark('A'));
		bus.clearHistory();
		check(!bus.canUndo() && !bus.canRedo() && bus.getDepth() == 0,
				"command-history: clearHistory forgets how the document got here");
		check(!bus.undo(), "command-history: nothing to undo after clearHistory");
	}

	// ---- names -------------------------------------------------------------------------------
	//
	// WHAT would be taken back, not whether it could: a menu that can only say "Undo" is how a
	// person eventually undoes the wrong thing in the wrong history.

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		checkEq(bus.getUndoName(), StringView(), "command-history: an empty history names nothing");

		bus.apply(named('A', "Rename field"));
		checkEq(bus.getUndoName(), StringView("Rename field"),
				"command-history: the top entry names itself");
		checkEq(bus.getRedoName(), StringView(), "command-history: and there is nothing to redo");

		bus.undo();
		checkEq(bus.getRedoName(), StringView("Rename field"),
				"command-history: after undo it is what would be redone");
		checkEq(bus.getUndoName(), StringView(),
				"command-history: and there is nothing left to undo");

		bus.redo();
		bus.transaction([&]() -> Status {
			bus.apply(named('B', "Remove node"));
			bus.apply(named('C', "Remove edge"));
			return Status::Ok;
		});
		checkEq(bus.getUndoName(), StringView("Remove node"),
				"command-history: a transaction is named for the operation that opened it");
	}

	// ---- transactions --------------------------------------------------------------------------

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		auto st = bus.transaction([&]() -> Status {
			bus.apply(mark('A'));
			bus.apply(mark('B'));
			bus.apply(mark('C'));
			return Status::Ok;
		});
		check(st == Status::Ok && bus.getDepth() == 1,
				"command-history: a transaction of three is one history entry");

		bus.undo();
		checkEq(StringView(ctx.undone), StringView("CBA"),
				"command-history: a transaction undoes in reverse, all at once");
	}

	{
		// Nested joins outer: a high-level operation assembled out of smaller ones is still one
		// entry, which is what lets a composite open a transaction of its own without splitting
		// the author's single gesture into several undos.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		bus.transaction([&]() -> Status {
			bus.apply(mark('A'));
			return bus.transaction([&]() -> Status {
				bus.apply(mark('B'));
				bus.apply(mark('C'));
				return Status::Ok;
			});
		});
		check(bus.getDepth() == 1, "command-history: a nested transaction joins the outer one");
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("CBA"),
				"command-history: the whole nest undoes as one");
	}

	{
		// A refusal rolls the whole transaction back and leaves the history as it was.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.apply(mark('Z'));
		const auto depthBefore = bus.getDepth();

		auto st = bus.transaction([&]() -> Status {
			bus.apply(mark('A'));
			bus.apply(mark('B'));
			return bus.apply(failing());
		});

		check(st != Status::Ok, "command-history: a refusal inside a transaction is reported");
		checkEq(StringView(ctx.undone), StringView("BA"),
				"command-history: a failed transaction rolls back in reverse");
		check(bus.getDepth() == depthBefore,
				"command-history: a failed transaction leaves no history entry");
		check(bus.canUndo(), "command-history: the edit made before it is still undoable");
	}

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.transaction([&]() -> Status { return Status::Ok; });
		check(bus.getDepth() == 0, "command-history: an empty transaction leaves no entry");
	}

	// ---- groups --------------------------------------------------------------------------------

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		bus.beginGroup();
		bus.apply(mark('A'));
		bus.apply(mark('B'));
		check(bus.getDepth() == 0, "command-history: an open group has committed nothing yet");
		bus.endGroup();
		check(bus.getDepth() == 1, "command-history: a closed group is one entry");
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("BA"),
				"command-history: a group undoes in reverse");
	}

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.beginGroup();
		bus.endGroup();
		check(bus.getDepth() == 0, "command-history: an empty group leaves no entry");
	}

	{
		// Re-entrant: a second begin must not throw away what the first one collected.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.beginGroup();
		bus.apply(mark('A'));
		bus.beginGroup();
		bus.apply(mark('B'));
		bus.endGroup();
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("BA"),
				"command-history: a redundant beginGroup does not reset the buffer");
		check(!bus.canUndo(), "command-history: ... and it was still only one entry");
	}

	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.beginGroup();
		bus.apply(mark('A'));
		bus.endGroup();
		bus.endGroup(); // idempotent
		check(bus.getDepth() == 1, "command-history: endGroup twice is not two entries");
	}

	{
		// A transaction opened inside a group folds into it: one nesting policy, not two.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);
		bus.beginGroup();
		bus.apply(mark('A'));
		bus.transaction([&]() -> Status {
			bus.apply(mark('B'));
			bus.apply(mark('C'));
			return Status::Ok;
		});
		bus.endGroup();
		check(bus.getDepth() == 1, "command-history: a transaction inside a group folds into it");
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("CBA"),
				"command-history: and the whole group undoes together");
	}
}

// ---- the group's idle window, on an injected clock ----------------------------------------------

void testGroupIdle() {
	{
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		// 1000 units of idle. Nothing here reads a clock: the numbers ARE the clock, which is why
		// this runs the same on every machine and never sleeps.
		bus.beginGroup(1'000, 0);
		bus.apply(mark('A'), 10);
		bus.apply(mark('B'), 20);
		check(bus.getDepth() == 0 && bus.isGroupOpen(),
				"command-history: the group is still open inside its idle window");

		bus.tickIdle(2'000);
		check(bus.getDepth() == 1 && !bus.isGroupOpen(),
				"command-history: the group closes itself once the window passes");

		// The next edit must start its OWN entry rather than joining a group that is over.
		bus.apply(mark('C'), 2'100);
		check(bus.getDepth() == 2,
				"command-history: an edit after the flush is an entry of its own");

		bus.undo();
		checkEq(StringView(ctx.undone), StringView("C"), "command-history: ... and undoes alone");
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("CBA"),
				"command-history: the flushed group is still one entry behind it");
	}

	{
		// Every apply resets the window, so a rapid drag stays one entry however long it lasts.
		TestContext ctx;
		TestBus bus;
		bus.init(&ctx);

		bus.beginGroup(1'000, 0);
		uint64_t now = 0;
		for (uint32_t i = 0; i < 5; ++i) {
			now += 500; // shorter than the window
			bus.apply(mark(char('0' + i)), now);
		}
		check(bus.isGroupOpen() && bus.getDepth() == 0,
				"command-history: each apply resets the idle window");
		bus.endGroup();
		check(bus.getDepth() == 1, "command-history: the whole drag is one entry");
		bus.undo();
		checkEq(StringView(ctx.undone), StringView("43210"),
				"command-history: and it undoes in reverse, all five");
	}
}

// ---- observers ----------------------------------------------------------------------------------

class TestObserver final : public TestBus::Observer {
public:
	virtual void handleDocumentChanged() override { ++documents; }
	virtual void handleHistoryChanged() override { ++histories; }
	virtual void handleEvent(const TestEvent &ev) override { events.push_back(ev.name); }

	uint32_t documents = 0;
	uint32_t histories = 0;
	mem_std::String events;
};

void testObservers() {
	TestContext ctx;
	TestBus bus;
	bus.init(&ctx);

	TestObserver obs;
	bus.addObserver(&obs);
	bus.addObserver(&obs); // deduplicated by pointer

	bus.beginGroup();
	bus.apply(mark('A'));
	bus.apply(mark('B'));
	bus.apply(mark('C'));
	check(obs.documents == 0, "command-history: an open group notifies nothing yet");
	checkEq(StringView(obs.events), StringView("ABC"),
			"command-history: but every command reports itself as it runs");

	bus.endGroup();
	check(obs.documents == 1,
			"command-history: a hundred moves cost ONE document notification, not a hundred");
	check(obs.histories == 1, "command-history: and one history notification");

	obs.events.clear();
	bus.undo();
	checkEq(StringView(obs.events), StringView("CBA"),
			"command-history: undo reports each command as it comes back");
	check(obs.documents == 2, "command-history: undo is a document change too");

	bus.removeObserver(&obs);
	bus.redo();
	check(obs.documents == 2, "command-history: a removed observer hears nothing");
}

} // namespace

void performCommandHistoryTests() {
	testOwnership();
	testMechanics();
	testGroupIdle();
	testObservers();
}

} // namespace STAPPLER_VERSIONIZED stappler
