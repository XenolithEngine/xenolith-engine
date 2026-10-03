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

// Subtask F3: the flow of execution - tokens, sequence, branch.
//
// The interpreter has no idea what a branch is. `flow.branch` is an ordinary operation that fires one
// of its two exec outputs; the one it does not fire leaves the nodes behind it without a token, and
// that is a path ending normally, not a stall.
//
// What the interpreter DOES decide is the order, and the order is observable, so it is stated here
// rather than left to whatever the container happened to do:
//
//   * exec targets are queued in reverse declaration order, so `sequence` runs its first branch and
//     everything below it before the second;
//   * nodes that became computable are queued on top of them, so pure work is done before the chain
//     moves on;
//   * the front is a stack that lives in the arena, so all of this survives into F-II's snapshots.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

} // namespace

void performInterpExecTests() {
	sprt::cout << "\n== flow interp: the flow of execution ==\n";

	// ---- a chain ------------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::Chain)),
				"interp-exec: the chain builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-exec: the chain completes");
		check(StringView(fx.report.getTrace()) == "1 2 3",
				"interp-exec: a token walks the chain in order");
		check(fx.interp.getLocal().getReadyCount() == 0,
				"interp-exec: the front is empty when the run ends");
	}

	// ---- sequence: the first branch, all of it, then the second ----------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::Sequence)),
				"interp-exec: the sequence graph builds");

		check(fx.run() == Status::Ok, "interp-exec: the sequence completes");

		// Not "3 5 4": the first branch runs to its end before the second begins. That is what makes
		// `sequence` mean what an author reads it to mean.
		check(StringView(fx.report.getTrace()) == "1 2 3 4 5",
				"interp-exec: the first branch runs to completion before the second");
	}

	// ---- branch: the path not taken --------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::BranchFalse)),
				"interp-exec: the branch graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-exec: a branch that is not taken is not an error");
		check(StringView(fx.report.getTrace()) == "1 2 4",
				"interp-exec: only the chosen branch runs");
		check(!fx.ran(3) && fx.ran(4), "interp-exec: the node on the other branch never ran");

		// And the same graph the other way.
		Fixture yes;
		check(yes.prepare(StringView(corpus::BranchTrue)),
				"interp-exec: the other branch graph builds");
		check(yes.run() == Status::Ok && StringView(yes.report.getTrace()) == "1 2 3",
				"interp-exec: the condition decides which one, and nothing else does");
	}

	// ---- computable work first, then the chain -----------------------------------------------------------

	{
		// test.split both fires its exec output and produces a value, which no library operation does -
		// so the moment after it runs, an exec target and a data consumer are both ready. The data
		// consumer goes first.
		Fixture fx;
		check(fx.prepare(StringView(corpus::ComputableFirst)),
				"interp-exec: the split graph builds");

		check(fx.run() == Status::Ok, "interp-exec: the split graph completes");
		check(StringView(fx.report.getTrace()) == "1 2 4 3",
				"interp-exec: what became computable is computed before the chain moves on");

		flow::value::Var result;
		check(fx.output(4, 0, result) && result.f == 6.0,
				"interp-exec: and the value that travelled is the one that was produced");
	}

	// ---- two entry points ---------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::TwoEntries)),
				"interp-exec: the two-entry graph builds");

		check(fx.run() == Status::Ok, "interp-exec: both entry points run");
		// Depth first: the first entry and everything it reaches, then the second.
		check(StringView(fx.report.getTrace()) == "1 3 2 4",
				"interp-exec: entry points run in index order, each to its end");
	}

	// ---- the order comes from the graph, not from the file --------------------------------------------------

	{
		Fixture a;
		Fixture b;
		check(a.prepare(StringView(corpus::SequenceShortOrdered)),
				"interp-exec: the ordered file builds");
		check(b.prepare(StringView(corpus::SequenceShortShuffled)),
				"interp-exec: the shuffled file builds");

		check(a.run() == Status::Ok && b.run() == Status::Ok, "interp-exec: both run");
		check(StringView(a.report.getTrace()) == "1 2 3 4"
						&& StringView(b.report.getTrace()) == "1 2 3 4",
				"interp-exec: record order in the file does not reach the execution order");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
