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

// Stage F-II, step S2: running a graph one unit of work at a time.
//
// Nothing was added to make this possible - it was paid for in F-I. IN-1 says that after any step
// the complete state of a run is the bytes of its local store, and the ready front was put in the
// arena for exactly this reason, against the temptation of a host-side vector. What S2 does is cut
// the loop that was already there into begin() / stepOnce() / finish() so that a caller can stop
// between two units.
//
// The one thing that must not happen is two loops. A batch run and a stepped run have to execute
// the same thing, and the only way to be certain is for finish() to BE `while (stepOnce())`. So the
// first and largest check here is not about stepping at all: it takes the whole F-I corpus and
// demands that the stepped run and the batch run produce the same execution log and the same store,
// byte for byte. If anyone ever "optimises the batch path", this is what catches it.
//
// The last check takes the invariant literally. A run is paused, its arena is saved, the image is
// adopted into a DIFFERENT arena, and the run carries on there to the same end. If any part of a
// run lived on the host, that would be impossible.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;

// The corpus. Deliberately the shapes F-I settled: a chain, a sequence whose first branch has to
// finish first, a branch not taken, work that becomes computable mid-chain, two entry points, and a
// wait that resolves later. Each is a different path through the loop being cut.
struct Case {
	StringView name;
	StringView json;
};

const Case s_cases[] = {
	{StringView("chain"), StringView(corpus::Chain)},

	{StringView("sequence"), StringView(corpus::Sequence)},

	{StringView("branch"), StringView(corpus::BranchFalse)},

	{StringView("computable-first"), StringView(corpus::ComputableFirst)},

	{StringView("two-entries"), StringView(corpus::TwoEntries)},

	{StringView("late-value"), StringView(corpus::LateValue)},

	{StringView("deadlock"), StringView(corpus::DeadlockTrue)},
};

} // namespace

void performInterpSteppingTests() {
	sprt::cout << "\n== flow interp: one unit at a time ==\n";

	// ---- the batch run and the stepped run are the same run -----------------------------------

	for (auto &it : s_cases) {
		Fixture batch;
		Fixture stepped;
		auto built = batch.prepare(it.json) && stepped.prepare(it.json);
		check(built, mem_std::toString("interp-stepping: ", it.name, " builds"));
		if (!built) {
			continue;
		}

		batch.run();

		check(stepped.interp.begin(stepped.graph, stepped.ops, stepped.arena, RunConfig(),
					  stepped.report)
						== Status::Ok,
				mem_std::toString("interp-stepping: ", it.name, " begins"));
		uint32_t units = 0;
		while (stepped.interp.stepOnce(stepped.report)) { ++units; }

		check(units == batch.report.stepCount,
				mem_std::toString("interp-stepping: ", it.name, " takes the same number of units"));
		check(stepped.report.outcome == batch.report.outcome,
				mem_std::toString("interp-stepping: ", it.name, " reaches the same outcome"));
		checkEq(StringView(stepped.report.getTrace()), StringView(batch.report.getTrace()),
				mem_std::toString("interp-stepping: ", it.name, " runs in the same order"));
		check(takeImage(stepped.arena) == takeImage(batch.arena),
				mem_std::toString("interp-stepping: ", it.name, " leaves a byte-identical store"));
	}

	// ---- what peekNext says, stepOnce does ----------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(s_cases[1].json), "interp-stepping: the peek graph builds");
		check(fx.interp.peekNext() == NullRecordKey,
				"interp-stepping: before begin() there is nothing to do next");

		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-stepping: the peek run begins");

		bool agreed = true;
		mem_std::String peeked;
		while (true) {
			auto next = fx.interp.peekNext();
			auto before = uint32_t(fx.report.log.size());
			if (!fx.interp.stepOnce(fx.report)) {
				// The last call did no work, so it must have had nothing to point at.
				agreed = agreed && (next == NullRecordKey);
				break;
			}
			if (fx.report.log.size() != before + 1) {
				agreed = false;
				break;
			}
			auto &done = fx.report.log.back();
			agreed = agreed && next == makeRecordKey(done.node, done.activation);
			if (!peeked.empty()) {
				peeked.append(" ");
			}
			peeked.append(mem_std::toString(done.id));
		}
		check(agreed, "interp-stepping: peekNext names exactly the unit stepOnce then performs");
		checkEq(StringView(peeked), StringView("1 2 3 4 5"),
				"interp-stepping: and the units are the execution order");
		check(fx.interp.peekNext() == NullRecordKey,
				"interp-stepping: a finished run has nothing to do next");
	}

	// ---- the state machine ---------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(s_cases[0].json), "interp-stepping: the state graph builds");
		check(fx.interp.getState() == RunState::Idle,
				"interp-stepping: a fresh interpreter is idle");

		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-stepping: begin succeeds");
		check(fx.interp.getState() == RunState::Paused,
				"interp-stepping: after begin the run is paused, not started");
		check(fx.report.stepCount == 0 && fx.report.log.empty(),
				"interp-stepping: and begin executed nothing at all");

		check(fx.interp.stepOnce(fx.report) && fx.interp.getState() == RunState::Paused,
				"interp-stepping: one unit leaves it paused again");

		check(fx.interp.finish(fx.report) == Status::Ok,
				"interp-stepping: finish takes it the rest of the way");
		check(fx.interp.getState() == RunState::Finished
						&& fx.report.outcome == RunOutcome::Completed,
				"interp-stepping: and the run is over");
		check(!fx.interp.stepOnce(fx.report) && fx.report.stepCount == 3,
				"interp-stepping: stepping a finished run does nothing and changes nothing");

		fx.interp.reset();
		check(fx.interp.getState() == RunState::Idle,
				"interp-stepping: reset puts it back to idle");
	}

	{
		// A failing operation ends the run, and says so through the state as well as the outcome.
		Fixture fx;
		check(fx.prepare(StringView(corpus::FailEntry)),
				"interp-stepping: the failing graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-stepping: the failing run begins");
		while (fx.interp.stepOnce(fx.report)) { }
		check(fx.interp.getState() == RunState::Finished
						&& fx.report.outcome == RunOutcome::OpError,
				"interp-stepping: an operation's failure ends the stepped run too");
		check(fx.interp.peekNext() == NullRecordKey,
				"interp-stepping: and there is nothing to step into afterwards");
	}

	// ---- the ceiling leaves something that can be looked at ------------------------------------

	{
		// F-I popped the node and cleared its Queued flag before reporting the limit, which left a
		// store nobody could carry on from. The limit is now tested before the node leaves the front.
		Fixture fx;
		check(fx.prepare(s_cases[1].json), "interp-stepping: the ceiling graph builds");

		RunConfig config;
		config.maxSteps = 2;
		check(fx.run(config) == Status::ErrorInvalidArguemnt
						&& fx.report.outcome == RunOutcome::StepLimit,
				"interp-stepping: a low ceiling stops the run");
		check(fx.report.stepCount == 2,
				"interp-stepping: after exactly the allowed number of units");
		check(fx.interp.getLocal().getReadyCount() > 0,
				"interp-stepping: and the node it did not run is still on the front");
		check(fx.interp.getLocal().getArena()->verify() == Status::Ok
						&& fx.interp.getLocal().verify() == Status::Ok,
				"interp-stepping: the store it left behind is sound");
	}

	// ---- a pause is nothing but bytes ----------------------------------------------------------

	{
		// The whole invariant in one check: pause, save, adopt somewhere else, carry on, same end.
		Fixture fx;
		check(fx.prepare(s_cases[1].json), "interp-stepping: the snapshot graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-stepping: the snapshot run begins");
		fx.interp.stepOnce(fx.report);
		fx.interp.stepOnce(fx.report);

		auto image = takeImage(fx.arena);
		auto carried = fx.report; // the log is host diagnostics; the arena has no copy

		check(fx.interp.finish(fx.report) == Status::Ok, "interp-stepping: the original finishes");

		Arena elsewhere;
		check(elsewhere.adopt(BytesView(image.data(), image.size())) == Status::Ok,
				"interp-stepping: the paused image adopts into a different arena");

		Interpreter resumed;
		check(resumed.attach(fx.graph, fx.ops, elsewhere, RunConfig(), carried) == Status::Ok,
				"interp-stepping: and a run attaches to it");
		check(resumed.getState() == RunState::Paused,
				"interp-stepping: paused exactly where the other one was");
		check(resumed.peekNext() != NullRecordKey,
				"interp-stepping: with the front the pause left behind, and work still to do");

		check(resumed.finish(carried) == Status::Ok,
				"interp-stepping: the adopted run runs to the end");
		checkEq(StringView(carried.getTrace()), StringView(fx.report.getTrace()),
				"interp-stepping: through the same nodes in the same order");
		check(carried.stepCount == fx.report.stepCount,
				"interp-stepping: in the same number of units");
		check(takeImage(elsewhere) == takeImage(fx.arena),
				"interp-stepping: and ends byte-identical to the run it was cut out of");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
