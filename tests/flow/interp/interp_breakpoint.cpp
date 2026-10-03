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

// Stage F-II, step S3: stopping a run on a condition.
//
// One shape covers both families a debugger needs. Leave the watch empty and a breakpoint is a
// position - "stop before node N in iteration A", which is subtask I2's requirement stated in the
// interpreter's own terms. Fill the watch in and it is a watchpoint - "stop when this field becomes
// that" - and the field may be in the node's own record, in the interpreter's bookkeeping, or in
// the scene.
//
// Breakpoints live on the host. That is not a hole in IN-1: no operation can read them, they change
// no result, and a rollback must NOT remove them, because a breakpoint that vanishes when you step
// backwards is useless. Same category as the execution log.
//
// The failure mode worth designing against is a breakpoint that silently never fires: the search
// for the bug then goes into the graph, where the bug is not. So a description that cannot work is
// refused at once, and the last block here is entirely about refusals.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;


} // namespace

void performInterpBreakpointTests() {
	sprt::cout << "\n== flow interp: stopping on a condition ==\n";

	// ---- a position ---------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the value graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the run begins");

		Breakpoint bp;
		bp.node = 3;
		auto id = fx.interp.addBreakpoint(bp);
		check(id != 0, "interp-breakpoint: a positional breakpoint is accepted");

		auto stored = fx.interp.getBreakpoint(id);
		check(stored && stored->when == BreakWhen::BeforeStep,
				"interp-breakpoint: and defaults to stopping BEFORE the node, where you are looking");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id, "interp-breakpoint: finish stops on it");
		check(fx.interp.getState() == RunState::Paused,
				"interp-breakpoint: leaving the run paused, not finished");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2"),
				"interp-breakpoint: having run everything before the node and not the node");
		check(fx.interp.peekNext() == makeRecordKey(fx.graph.findNode(3), RootActivation),
				"interp-breakpoint: and the node it stopped before is the one it would do next");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == 0,
				"interp-breakpoint: continuing gets past it instead of stopping on it forever");
		check(fx.report.outcome == RunOutcome::Completed,
				"interp-breakpoint: and the run finishes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4"),
				"interp-breakpoint: through exactly the order a run without breakpoints takes");
		check(fx.interp.getBreakpoint(id)->hits == 1,
				"interp-breakpoint: one stop, one hit");
	}

	// ---- a value in a node's own record --------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the record-watch graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the record-watch run begins");

		Breakpoint bp;
		bp.watch.source = Watch::Source::NodeRecord;
		bp.watch.node = 3;
		bp.watch.field = StringView("result");
		bp.compare = BreakCompare::Equal;
		bp.value = flow::value::makeFloat(6.0);
		auto id = fx.interp.addBreakpoint(bp);
		check(id != 0, "interp-breakpoint: a watch on a node's output is accepted");
		check(fx.interp.getBreakpoint(id)->when == BreakWhen::AfterStep,
				"interp-breakpoint: and defaults to AFTER the step, which is when a value exists");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id, "interp-breakpoint: it stops the run");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3"),
				"interp-breakpoint: on the step that produced the value, not before and not after");

		flow::value::Var seen;
		check(fx.output(3, 0, seen) && seen.f == 6.0,
				"interp-breakpoint: and the value really is what was watched for");
	}

	{
		// The same watch with a value that never occurs must not stop the run - and must not stop it
		// SILENTLY WRONG either: the run has to complete normally.
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the never-matching graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the never-matching run begins");

		Breakpoint bp;
		bp.watch.source = Watch::Source::NodeRecord;
		bp.watch.node = 3;
		bp.watch.field = StringView("result");
		bp.compare = BreakCompare::Equal;
		bp.value = flow::value::makeFloat(999.0);
		auto id = fx.interp.addBreakpoint(bp);

		check(fx.interp.finish(fx.report) == Status::Ok, "interp-breakpoint: the run completes");
		check(fx.interp.getLastBreakpoint() == 0 && fx.interp.getBreakpoint(id)->hits == 0,
				"interp-breakpoint: a value that never occurs stops nothing and counts nothing");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4"),
				"interp-breakpoint: and the order is untouched");
	}

	// ---- greater-than, and the interpreter's own bookkeeping ------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the state-watch graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the state-watch run begins");

		// Node 4 gets its exec token when node 2 fires, which is the first moment its flags are
		// anything but zero.
		Breakpoint bp;
		bp.watch.source = Watch::Source::NodeState;
		bp.watch.node = 4;
		bp.watch.field = StringView("flags");
		bp.compare = BreakCompare::Greater;
		bp.value = flow::value::makeInt(0);
		auto id = fx.interp.addBreakpoint(bp);
		check(id != 0, "interp-breakpoint: a watch on interp.NodeState is accepted");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id, "interp-breakpoint: it stops the run");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2"),
				"interp-breakpoint: on the step that delivered the token, not on the one that used it");
	}

	// ---- changed ---------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the changed-watch graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the changed-watch run begins");

		Breakpoint bp;
		bp.watch.source = Watch::Source::NodeRecord;
		bp.watch.node = 3;
		bp.watch.field = StringView("result");
		bp.compare = BreakCompare::Changed;
		auto id = fx.interp.addBreakpoint(bp);
		check(id != 0, "interp-breakpoint: Changed needs no value and is accepted without one");

		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id,
				"interp-breakpoint: Changed stops on the step that wrote the field");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3"),
				"interp-breakpoint: which is node 3's own step");

		// It does not fire again: the field is written once and stays.
		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == 0 && fx.interp.getBreakpoint(id)->hits == 1,
				"interp-breakpoint: and a field that stops changing stops firing");
	}

	// ---- disabled, removed, and stepped over -------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the toggle graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the toggle run begins");

		Breakpoint bp;
		bp.node = 3;
		auto id = fx.interp.addBreakpoint(bp);
		check(fx.interp.setBreakpointEnabled(id, false) == Status::Ok,
				"interp-breakpoint: a breakpoint can be switched off");

		check(fx.interp.finish(fx.report) == Status::Ok && fx.interp.getLastBreakpoint() == 0,
				"interp-breakpoint: a disabled breakpoint stops nothing");
		check(fx.interp.getBreakpoint(id)->hits == 0,
				"interp-breakpoint: and counts nothing");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4"),
				"interp-breakpoint: the run is exactly the run without it");
	}

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the remove graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the remove run begins");

		Breakpoint bp;
		bp.node = 3;
		auto id = fx.interp.addBreakpoint(bp);
		check(fx.interp.getBreakpoints().size() == 1, "interp-breakpoint: one breakpoint is set");
		check(fx.interp.removeBreakpoint(id) && fx.interp.getBreakpoints().empty(),
				"interp-breakpoint: and can be taken away");
		check(!fx.interp.removeBreakpoint(id),
				"interp-breakpoint: taking away an id twice is a no, not a second removal");

		check(fx.interp.finish(fx.report) == Status::Ok && fx.interp.getLastBreakpoint() == 0,
				"interp-breakpoint: a removed breakpoint stops nothing");
	}

	{
		// Stepping is stepping OVER: stepOnce takes the unit a breakpoint would have stopped before.
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the step-over graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the step-over run begins");

		Breakpoint bp;
		bp.node = 2;
		auto id = fx.interp.addBreakpoint(bp);

		uint32_t units = 0;
		while (fx.interp.stepOnce(fx.report)) { ++units; }
		check(units == 4 && fx.report.outcome == RunOutcome::Completed,
				"interp-breakpoint: stepping walks straight through the breakpoint");
		check(fx.interp.getBreakpoint(id)->hits == 0,
				"interp-breakpoint: without counting a hit, because it never stopped anything");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4"),
				"interp-breakpoint: and the order is the order");
	}

	{
		// A breakpoint survives the run it was set on. It belongs to whoever is debugging.
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the survival graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the survival run begins");

		Breakpoint bp;
		bp.node = 3;
		auto id = fx.interp.addBreakpoint(bp);
		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id, "interp-breakpoint: it stops the first run");

		fx.interp.reset();
		check(fx.interp.getBreakpoints().size() == 1,
				"interp-breakpoint: and is still there after the run is thrown away");
		check(fx.interp.getLastBreakpoint() == 0,
				"interp-breakpoint: while what stopped the old run is not");

		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: a second run begins");
		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id && fx.interp.getBreakpoint(id)->hits == 2,
				"interp-breakpoint: it stops the second run too, and has now been hit twice");
	}

	// ---- what is refused, and refused loudly ---------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::SplitWatch), "interp-breakpoint: the refusal graph builds");

		{
			Breakpoint bp;
			bp.node = 3;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a breakpoint on a node needs a run to name the node in");
		}

		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-breakpoint: the refusal run begins");

		{
			Breakpoint bp;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a breakpoint on no node and no value is refused - that is what "
					"stepOnce already is");
		}
		{
			Breakpoint bp;
			bp.node = 99;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a node the graph does not have is refused");
		}
		{
			Breakpoint bp;
			bp.watch.source = Watch::Source::NodeRecord;
			bp.watch.node = 3;
			bp.watch.field = StringView("nonesuch");
			bp.compare = BreakCompare::Changed;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a field the node's record does not have is refused");
		}
		{
			Breakpoint bp;
			bp.watch.source = Watch::Source::NodeState;
			bp.watch.node = 3;
			bp.watch.field = StringView("nonesuch");
			bp.compare = BreakCompare::Changed;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: and so is a field interp.NodeState does not have");
		}
		{
			Breakpoint bp;
			bp.watch.source = Watch::Source::Scene;
			bp.watch.field = StringView("value");
			bp.compare = BreakCompare::Changed;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a scene watch without a component type is refused");
		}
		{
			Breakpoint bp;
			bp.watch.source = Watch::Source::NodeRecord;
			bp.watch.node = 3;
			bp.watch.field = StringView("result");
			bp.compare = BreakCompare::Changed;
			bp.value = flow::value::makeFloat(1.0);
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: Changed compares against the past, so a value is refused");
		}
		{
			Breakpoint bp;
			bp.watch.source = Watch::Source::NodeRecord;
			bp.watch.node = 3;
			bp.watch.field = StringView("result");
			bp.compare = BreakCompare::Equal;
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: and Equal without one is refused the other way");
		}
		{
			Breakpoint bp;
			bp.node = 3;
			bp.compare = BreakCompare::Equal;
			bp.value = flow::value::makeFloat(1.0);
			check(fx.interp.addBreakpoint(bp) == 0,
					"interp-breakpoint: a comparison with nothing to compare is refused");
		}

		check(fx.interp.getBreakpoints().empty(),
				"interp-breakpoint: not one of them was quietly accepted");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler