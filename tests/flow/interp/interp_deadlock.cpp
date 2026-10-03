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

// Subtask F5: a real deadlock, and the many things that look like one and are not.
//
// The rule from part II, point 10 of the design document, and the only rule here: an error is a
// DATA deadlock - a node that was told to run and can never get the values it needs. A branch that
// was not taken, a terminal that was never reached, a pure node whose value nobody asked for: all
// normal, all silent.
//
// Getting this wrong in the lenient direction makes a hung graph look fine; getting it wrong in the
// strict direction makes every conditional graph report an error. So each case below says which of
// the two it is and why.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

// The one reader of a report entry's code, for every section (check/studio_check.h).
using stappler::test::hasDiag;

} // namespace

void performInterpDeadlockTests() {
	sprt::cout << "\n== flow interp: deadlock and the paths that simply end ==\n";

	// ---- the real thing --------------------------------------------------------------------------------

	{
		// Node 4 is told to run - the false branch fires - and the value it needs comes from node 3,
		// which sits on the branch that was not taken and will never run.
		Fixture fx;
		check(fx.prepare(StringView(corpus::Deadlock)),
				"interp-deadlock: the deadlocked graph builds");

		check(fx.run() != Status::Ok && fx.report.outcome == RunOutcome::Deadlock,
				"interp-deadlock: a node with a token and no value is a deadlock");
		check(StringView(fx.report.getTrace()) == "1 2",
				"interp-deadlock: the run ends where it can go no further");
		check(hasDiag(fx.report.diagnostics, StringView("deadlock"))
						&& fx.report.diagnostics.size() == 1
						&& fx.report.diagnostics.getValue(0).getValue("at").getInteger(0) == 4
						&& StringView(fx.report.diagnostics.getValue(0).getValue("names").getString(0))
								== "value",
				"interp-deadlock: the diagnostic names the node and the input it waits on");
		check(fx.interp.getLocal().getStalledCount() == 1,
				"interp-deadlock: and the run remembers which node it was");
	}

	// ---- the same shape without the data edge ------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::BranchFalse)),
				"interp-deadlock: the branching graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-deadlock: an unreached terminal is a path that ended, not a failure");
		check(!fx.report.diagnostics.isArray() || fx.report.diagnostics.size() == 0,
				"interp-deadlock: and it produces no diagnostic at all");
	}

	// ---- a pure node nobody needed -------------------------------------------------------------------------

	{
		// Node 3 never becomes computable, because its input comes from the branch that was not taken.
		// Nobody with a token is waiting on IT, so this is not a deadlock - it is work that was not
		// required.
		Fixture fx;
		check(fx.prepare(StringView(corpus::UnneededPure)),
				"interp-deadlock: the unneeded-pure graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-deadlock: a pure node that never became computable is not a deadlock");
		check(!fx.ran(3), "interp-deadlock: it simply did not run");
	}

	// ---- the ceiling ------------------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::ChainOfFour)),
				"interp-deadlock: the chain builds");

		// Without loops this graph cannot exhaust a sensible ceiling, so the ceiling is lowered to meet
		// it. The point is that the mechanism exists and reports, before F-II makes loops able to reach
		// it for real.
		RunConfig config;
		config.maxSteps = 2;
		check(fx.run(config) != Status::Ok && fx.report.outcome == RunOutcome::StepLimit,
				"interp-deadlock: a run that will not end is stopped, not left to hang");
		check(fx.report.stepCount == 2, "interp-deadlock: it stopped exactly at the ceiling");
		check(hasDiag(fx.report.diagnostics, StringView("step-limit"))
						&& fx.report.diagnostics.getValue(0).getValue("at").getInteger(0) == 2,
				"interp-deadlock: and said what the ceiling was");

		// The same graph without the artificial ceiling finishes.
		Fixture full;
		full.prepare(StringView(corpus::ChainOfFour));
		check(full.run() == Status::Ok && StringView(full.report.getTrace()) == "1 2 3 4",
				"interp-deadlock: with the default ceiling the same graph runs to the end");
	}

	// ---- a graph with nothing to do ------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::NoEntry)),
				"interp-deadlock: the single unreachable node builds");

		// It has an exec input and nothing to fire it: the build warns about that, and the run has
		// simply nothing to do. Not an error either.
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed
						&& fx.report.stepCount == 0,
				"interp-deadlock: a graph with no entry point runs zero steps and completes");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
