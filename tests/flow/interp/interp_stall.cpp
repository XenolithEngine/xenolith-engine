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

// Subtask F4: a node that is not ready yet, and the fixed point that picks it up.
//
// The document describes this as a loop: run the ready set, collect the ones that could not run, go
// round again, and repeat until a pass makes no progress. This implementation reaches the same fixed
// point by PUSH - a node is queued at the moment its last input arrives or its token does - so the
// second pass has nothing left to find. One sweep happens anyway at the end, and its job is to say
// why the run stopped rather than to make progress.
//
// The distinction matters and the numbers below are the evidence: graphs written specifically to
// need two and three passes of the naive loop complete in ONE sweep here, with the same result and
// the same execution order. What is being tested is that a node whose value arrives later in the
// chain of execution waits for it and then runs - not that the interpreter rescans.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

} // namespace

void performInterpStallTests() {
	sprt::cout << "\n== flow interp: waiting and the fixed point ==\n";

	// ---- the token arrives before the value ------------------------------------------------------------

	{
		// The consumer sits on the FIRST branch of the sequence and its value comes from the second:
		// it is told to run before the value it needs exists.
		Fixture fx;
		check(fx.prepare(StringView(corpus::LateValue)),
				"interp-stall: the late-value graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-stall: the run completes");
		check(StringView(fx.report.getTrace()) == "1 2 4 3",
				"interp-stall: the consumer waits for its value and runs after the producer");
		check(fx.ran(3), "interp-stall: it did run - waiting is not skipping");
		check(fx.report.sweepCount == 1,
				"interp-stall: one sweep, because a value arriving queues its consumer directly");
		check(fx.interp.getLocal().getStalledCount() == 0,
				"interp-stall: nothing is left waiting at the end");
	}

	// ---- three of them in a row --------------------------------------------------------------------------

	{
		// The naive loop would need a pass per link here. This one still needs none: every link is
		// queued by the arrival that unblocks it.
		Fixture fx;
		check(fx.prepare(StringView(corpus::ChainedWait)),
				"interp-stall: the chained-wait graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-stall: the chained-wait run completes");
		check(fx.ran(3) && fx.ran(5) && fx.ran(6),
				"interp-stall: every node in the chain of waits ran");
		check(fx.report.sweepCount == 1, "interp-stall: still one sweep");

		flow::value::Var result;
		check(fx.output(3, 0, result) && result.f == 1.0,
				"interp-stall: and the value made it all the way down the chain");
	}

	// ---- a pure node that is nobody's entry point ----------------------------------------------------------

	{
		// Its input comes from an exec-gated node, so it cannot be an entry: it runs when the value
		// shows up, and not before.
		Fixture fx;
		check(fx.prepare(StringView(corpus::DeferredPure)),
				"interp-stall: the deferred-pure graph builds");

		check(fx.graph.getEntryNodes().size() == 1,
				"interp-stall: a pure node with an incoming edge is not an entry point");
		check(fx.run() == Status::Ok, "interp-stall: the deferred-pure run completes");
		check(StringView(fx.report.getTrace()) == "1 2 3 4",
				"interp-stall: the pure chain runs the moment its value exists");

		flow::value::Var result;
		check(fx.output(4, 0, result) && result.f == 9.0,
				"interp-stall: 4 * 2 + 1 came out as nine");
	}

	// ---- what the run remembers about waiting ---------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::BranchTrue)),
				"interp-stall: the untaken-branch graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-stall: an untaken branch completes the run");

		// The node behind the branch is marked as waiting for a token it will never get. That is a
		// finished path, not a stall to report - so the stalled list stays empty.
		auto &local = fx.interp.getLocal();
		auto state = local.readState(local.getState(fx.graph.findNode(4), RootActivation));
		check((state.flags & NodeFlags::StallExec) != 0 && (state.flags & NodeFlags::Ran) == 0,
				"interp-stall: the node behind the branch is marked as never activated");
		check(local.getStalledCount() == 0,
				"interp-stall: and it is not in the stalled list, because nobody asked it to run");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
