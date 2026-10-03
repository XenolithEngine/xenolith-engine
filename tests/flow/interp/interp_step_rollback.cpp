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

// Subtask F6: a version after every unit of work, and undoing one.
//
// F-I named its own limitation out loud: an operation that failed halfway left its half-written
// record where it was, because there was nothing to roll a step back with. This is that something,
// and it is not new machinery - Journal::commit() and Journal::rollback() have existed since group
// B. What F-II adds is a policy: where the version boundary goes.
//
// It goes BEFORE the node comes off the front. Part II item 8 says a failing node rolls back to
// "before entering the node", and taking the node off the front is part of entering it - so an undo
// has to put it back, or the run could not carry on from what the undo produced.
//
// Both stores are on one journal, which is III.2's single version counter taken literally: a
// version is a consistent slice of the run AND the scene, and there is no way to roll back one
// without the other. The probe operation below writes to both and then fails, which is the only
// honest way to test that.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

// This whole section is ABOUT versioning, so without it there is nothing here to test and
// nothing here that would compile. The section keeps its place in the list and says so.

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;


bool prepareWalk(Fixture &fx) { return fx.prepare(corpus::Walk) && fx.prepareJournal(); }

} // namespace

void performInterpStepRollbackTests() {
	sprt::cout << "\n== flow interp: a version per unit, and undoing one ==\n";

	// ---- a version per unit ---------------------------------------------------------------------

	{
		Fixture fx;
		check(prepareWalk(fx), "interp-step-rollback: the walk graph builds");
		check(fx.run(fx.journalConfig()) == Status::Ok,
				"interp-step-rollback: the walk run completes");
		check(fx.report.log.size() == 5, "interp-step-rollback: five units");

		bool monotone = true;
		for (uint32_t i = 0; i < fx.report.log.size(); ++i) {
			auto &s = fx.report.log[i];
			monotone = monotone && s.baseVersion < s.version;
			if (i > 0) {
				monotone = monotone && fx.report.log[i - 1].version <= s.baseVersion;
			}
		}
		check(monotone, "interp-step-rollback: every unit closed its own version, in order");

		auto metrics = fx.journal.getMetrics();
		// Not an assertion about a number - a measurement, so that the cost of a version per unit is
		// known before anything is built on top of it (plan risk 2).
		sprt::cout << "       journal: " << metrics.versions << " versions, " << metrics.records
				   << " records, " << (fx.journal.getJournalBytes() / 1024) << " KiB\n";
		check(metrics.versions >= fx.report.log.size(),
				"interp-step-rollback: at least one version per unit of work");
	}

	// ---- stepping back --------------------------------------------------------------------------

	{
		Fixture fx;
		check(prepareWalk(fx), "interp-step-rollback: the step-back graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-step-rollback: the step-back run begins");

		mem_std::Vector<mem_std::Vector<uint8_t>> images;
		mem_std::Vector<mem_std::String> traces;
		images.emplace_back(takeImage(fx.arena));
		traces.emplace_back(fx.report.getTrace());
		while (fx.interp.stepOnce(fx.report)) {
			images.emplace_back(takeImage(fx.arena));
			traces.emplace_back(fx.report.getTrace());
		}
		// Taken after the loop, not inside it: the last stepOnce() is the one that does the closing
		// sweep, and the sweep writes to the store too.
		auto finalImage = takeImage(fx.arena);

		check(fx.report.outcome == RunOutcome::Completed,
				"interp-step-rollback: the step-back run completes");
		checkEq(StringView(traces.back()), StringView("1 2 4 5 3"),
				"interp-step-rollback: in the order F-I settled");

		// Backwards, one unit at a time, checking the store against what it was at that point.
		bool matched = true;
		bool traced = true;
		for (uint32_t i = uint32_t(images.size()) - 1; i > 0; --i) {
			if (fx.interp.stepBack(fx.report) != Status::Ok) {
				matched = false;
				break;
			}
			matched = matched && takeImage(fx.arena) == images[i - 1];
			traced = traced && fx.report.getTrace() == traces[i - 1];
		}
		check(matched, "interp-step-rollback: every step back restores the store it came from");
		check(traced, "interp-step-rollback: and takes the execution log back with it");
		check(fx.report.log.empty() && fx.report.stepCount == 0,
				"interp-step-rollback: all the way back to a run that has done nothing");
		check(fx.interp.getState() == RunState::Paused,
				"interp-step-rollback: paused, not finished - it has somewhere to go");

		check(fx.interp.stepBack(fx.report) == Status::ErrorNotFound,
				"interp-step-rollback: and there is nothing before the beginning");

		// Forwards again: the same run, because the state it starts from is the same state.
		check(fx.interp.finish(fx.report) == Status::Ok
						&& fx.report.outcome == RunOutcome::Completed,
				"interp-step-rollback: running forward again completes");
		checkEq(StringView(fx.report.getTrace()), StringView(traces.back()),
				"interp-step-rollback: through exactly the same order");
		check(takeImage(fx.arena) == finalImage,
				"interp-step-rollback: and ends in exactly the same bytes");
	}

	{
		// One step back, then forward again: the unit that is redone is the same unit.
		Fixture fx;
		check(prepareWalk(fx), "interp-step-rollback: the redo graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-step-rollback: the redo run begins");
		for (uint32_t i = 0; i < 3; ++i) { fx.interp.stepOnce(fx.report); }

		auto undone = fx.report.log.back();
		check(fx.interp.stepBack(fx.report) == Status::Ok, "interp-step-rollback: one step back");
		check(fx.interp.peekNext() == makeRecordKey(undone.node, undone.activation),
				"interp-step-rollback: the undone unit is the one standing next in line");

		check(fx.interp.stepOnce(fx.report), "interp-step-rollback: one step forward");
		auto redone = fx.report.log.back();
		check(redone.node == undone.node && redone.id == undone.id && redone.fired == undone.fired,
				"interp-step-rollback: and it is the same unit, doing the same thing");
	}

	// ---- what a rollback does not move ------------------------------------------------------------

	{
		// Breakpoints are the debugger's, not the run's. A rollback must leave them alone - and must
		// leave the values they remember CORRECT, or the next step forward reports a change that
		// the rollback itself caused.
		Fixture fx;
		check(prepareWalk(fx), "interp-step-rollback: the breakpoint graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-step-rollback: the breakpoint run begins");

		Breakpoint bp;
		bp.watch.source = Watch::Source::NodeRecord;
		bp.watch.node = 3;
		bp.watch.field = StringView("result");
		bp.compare = BreakCompare::Changed;
		auto id = fx.interp.addBreakpoint(bp);
		check(id != 0, "interp-step-rollback: a watch is set on node 3's output");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id,
				"interp-step-rollback: it stops the run when node 3 writes");
		auto stops = fx.interp.getBreakpoint(id)->hits;

		check(fx.interp.stepBack(fx.report) == Status::Ok,
				"interp-step-rollback: stepping back over the write");
		check(fx.interp.getBreakpoints().size() == 1 && fx.interp.getBreakpoint(id)->hits == stops,
				"interp-step-rollback: the breakpoint is still there, and its count is untouched");

		check(fx.interp.stepOnce(fx.report), "interp-step-rollback: and the write happens again");
		check(fx.interp.getBreakpoint(id)->hits == stops,
				"interp-step-rollback: a step does not fire it - only finish() stops on "
				"breakpoints");

		check(fx.interp.stepBack(fx.report) == Status::Ok,
				"interp-step-rollback: back over the write once more");
		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id
						&& fx.interp.getBreakpoint(id)->hits == stops + 1,
				"interp-step-rollback: continuing sees the value change again, because the watch "
				"re-read what the rollback restored");
	}

	// ---- rolling back to a version by hand ---------------------------------------------------------

	{
		// What group G will do with RunStep::version: pick a point on the timeline and go there.
		Fixture fx;
		check(prepareWalk(fx), "interp-step-rollback: the timeline graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-step-rollback: the timeline run begins");

		mem_std::Vector<mem_std::Vector<uint8_t>> images;
		while (fx.interp.stepOnce(fx.report)) { images.emplace_back(takeImage(fx.arena)); }
		check(images.size() == 5, "interp-step-rollback: five snapshots taken");

		auto target = fx.report.log[1].version;
		fx.journal.commit();
		check(fx.journal.rollback(target) == Status::Ok,
				"interp-step-rollback: the journal rolls back to the version a unit closed");
		check(takeImage(fx.arena) == images[1],
				"interp-step-rollback: and the store is what it was when that unit finished");
		check(fx.arena.verify() == Status::Ok && fx.interp.getLocal().verify() == Status::Ok,
				"interp-step-rollback: structurally sound after a rollback nobody told it about");
	}

	// ---- the quantum: how OFTEN a boundary is taken, and nothing else ----------------------------
	//
	// The claim the whole deferred capture rests on is that the three policies differ in cost and in
	// undo granularity, and in NOTHING ELSE. So the same graph is run under each and the results are
	// compared byte for byte - which is the only form of that claim worth making.

	{
		auto runUnder = [](RollbackQuantum q, mem_std::String &trace,
								mem_std::Vector<uint8_t> &image, uint64_t &versions) {
			Fixture fx;
			if (!prepareWalk(fx)) {
				return false;
			}
			auto config = fx.journalConfig();
			config.quantum = q;
			auto before = fx.journal.getMetrics().versions;
			auto st = fx.run(config);
			trace = fx.report.getTrace();
			image = takeImage(fx.arena);
			versions = fx.journal.getMetrics().versions - before;
			return st == Status::Ok;
		};

		mem_std::String traceStep, traceFail, traceRun;
		mem_std::Vector<uint8_t> imageStep, imageFail, imageRun;
		uint64_t vStep = 0, vFail = 0, vRun = 0;

		check(runUnder(RollbackQuantum::Step, traceStep, imageStep, vStep)
						&& runUnder(RollbackQuantum::Failable, traceFail, imageFail, vFail)
						&& runUnder(RollbackQuantum::Run, traceRun, imageRun, vRun),
				"interp-step-rollback: the walk graph runs under all three quanta");

		checkEq(StringView(traceFail), StringView(traceStep),
				"interp-step-rollback: Failable executes the same units in the same order");
		checkEq(StringView(traceRun), StringView(traceStep),
				"interp-step-rollback: and so does Run");
		check(imageFail == imageStep && imageRun == imageStep,
				"interp-step-rollback: all three leave the store byte-identical");

		// The only number that is a promise: fewer boundaries as the quantum coarsens, and at least
		// one however coarse it gets - a run with no base has nothing to undo to. How many Failable
		// saves is a property of which operations this graph happens to use, so it is reported rather
		// than asserted.
		check(vStep > vFail && vFail > vRun && vRun >= 1,
				mem_std::toString("interp-step-rollback: boundaries fall as the quantum coarsens (",
						vStep, " / ", vFail, " / ", vRun, ")"));
	}

}


} // namespace STAPPLER_VERSIONIZED stappler