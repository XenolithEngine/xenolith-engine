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

// Codegen-1: the oracle, proved on the interpreter against itself.
//
// The translation plan (docs/planning/graph_codegen.md) says a generated program is right when it
// is indistinguishable from the interpreter, and gives that word two levels (docs/codegen.md, §1):
// the observable run, and the store byte for byte. Before any generated code exists, the instrument
// that will measure it has to be shown to measure - on the one pair of engines already known to be
// the same run, the batch interpreter and the stepped one, and across the three quanta that
// interp-step-rollback says are three prices for one behaviour.
//
// So every block here is a claim another section already makes, restated through the oracle over
// the WHOLE corpus rather than over the handful of graphs that section chose. If the oracle passed
// a run it should have refused, the last block is what would say so: it hands the comparison two
// runs that differ in one channel each, and requires the refusal.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;
using namespace stappler::test::interpfx::corpus;
namespace oracle = stappler::test::oracle;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

void snapshot(Fixture &fx, RunSnapshot &out) {
	oracle::takeSnapshot(fx.graph, fx.interp, fx.arena, fx.report, out);
}

// A case as the batch interpreter takes it: one run.
Status runFrame(Fixture &fx, const Case &, uint32_t, const RunConfig &config) {
	return fx.run(config);
}

// The same run, up to the first unit: what a stepped engine starts from.
Status beginFrame(Fixture &fx, const Case &, uint32_t, const RunConfig &config) {
	return fx.interp.begin(fx.graph, fx.ops, fx.arena, config, fx.report);
}

// Runs a case to its end on one fixture, batch, and answers whether it ran.
bool runAll(Fixture &fx, const Case &c, const RunConfig &config) {
	return runFrame(fx, c, 0, config) == Status::Ok || c.outcome != RunOutcome::Completed;
}

const char *quantumName(RollbackQuantum q) {
	switch (q) {
	case RollbackQuantum::Step: return "Step";
	case RollbackQuantum::Failable: return "Failable";
	case RollbackQuantum::Run: return "Run";
	}
	return "?";
}

} // namespace

void performInterpOracleTests() {
	sprt::cout << "\n== flow interp: the oracle, on the interpreter against itself ==\n";

	// ---- the corpus is alive ------------------------------------------------------------------------
	//
	// Every case builds and reaches the outcome it was recorded with. A case that stopped reaching
	// its shape - a builder that fails, a probe that no longer refuses - would otherwise be two wrong
	// runs found equal.

	{
		bool built = true;
		bool reached = true;
		for (auto &c : Cases) {
			Fixture fx;
			if (!prepareCase(fx, c, false)) {
				sprt::cout << "       " << c.name << ": does not prepare\n";
				built = false;
				continue;
			}
			runAll(fx, c, configFor(fx, c, false));
			if (fx.report.outcome != c.outcome) {
				sprt::cout << "       " << c.name << ": reached "
						   << getRunOutcomeName(fx.report.outcome) << ", recorded "
						   << getRunOutcomeName(c.outcome) << "\n";
				reached = false;
			}
		}
		check(built, mem_std::toString("interp-oracle: all ", CaseCount, " cases of the corpus prepare"));
		check(reached, "interp-oracle: and each reaches the outcome it was recorded with");
	}

	// ---- the batch run and the stepped run are one run, over the whole corpus -----------------------
	//
	// interp-stepping's first claim, at level B: a run taken with run() and one taken
	// with begin() + stepOnce() leave the same report, the same records and the same bytes. And beside it, two STEPPED runs in lockstep, compared after every unit - which is the
	// shape the compiled engines will be held in, so it has to be shown to hold on the engine that
	// defines the answer.

	{
		bool frames = true;
		bool units = true;
		bool counted = true;
		for (auto &c : Cases) {
			Fixture batch;
			Fixture stepped;
			Fixture left;
			Fixture right;
			if (!prepareCase(batch, c, false) || !prepareCase(stepped, c, false)
					|| !prepareCase(left, c, false) || !prepareCase(right, c, false)) {
				frames = false;
				continue;
			}
			auto config = configFor(batch, c, false);
			auto steppedConfig = configFor(stepped, c, false);
			auto leftConfig = configFor(left, c, false);
			auto rightConfig = configFor(right, c, false);

			{
				const uint32_t i = 0; // a case is one run
				runFrame(batch, c, i, config);

				beginFrame(stepped, c, i, steppedConfig);
				while (stepped.interp.stepOnce(stepped.report)) { }

				RunSnapshot a;
				RunSnapshot b;
				snapshot(batch, a);
				snapshot(stepped, b);
				if (!oracle::compareRuns(a, b, CompareOptions{Level::Store},
							mem_std::toString(c.name, " run ", i, " batch vs stepped"))) {
					frames = false;
				}

				beginFrame(left, c, i, leftConfig);
				beginFrame(right, c, i, rightConfig);
				uint32_t diverged = 0;
				auto walked = oracle::lockstep([&] { return left.interp.stepOnce(left.report); },
						[&] { return right.interp.stepOnce(right.report); },
						[&](RunSnapshot &s) { snapshot(left, s); },
						[&](RunSnapshot &s) { snapshot(right, s); }, CompareOptions{Level::Store},
						mem_std::toString(c.name, " run ", i, " lockstep"), diverged);
				if (diverged != InvalidIndex) {
					units = false;
				}
				if (walked != batch.report.stepCount) {
					sprt::cout << "       " << c.name << " run " << i << ": lockstep walked "
							   << walked << " units, the batch run took "
							   << batch.report.stepCount << "\n";
					counted = false;
				}
			}
		}
		check(frames, "interp-oracle: batch and stepped leave the same run, level B, every case");
		check(units, "interp-oracle: two stepped runs agree after every unit of work");
		check(counted, "interp-oracle: and the lockstep counts the units the batch run took");
	}

	// ---- three quanta, one run ----------------------------------------------------------------------
	//
	// interp-step-rollback's claim about the quantum, over the corpus: with a journal over the
	// run's store, Step, Failable and Run execute the same units in the same order and leave the same
	// bytes, differing in the ladder of versions alone. The ladder is asked to fall as the quantum
	// coarsens and never to reach zero.
	//
	// A run that ends in an operation's refusal is the one exception, and it is the quantum's own
	// definition: the undo goes back to the last boundary, and where that is IS what differs. Such
	// a case is compared stepped-against-batch under each quantum, and not across them.

	{
		bool agreed = true;
		bool ladders = true;
		bool stepped = true;
		const RollbackQuantum quanta[] = {RollbackQuantum::Step, RollbackQuantum::Failable,
			RollbackQuantum::Run};

		for (auto &c : Cases) {
			RunSnapshot under[3];
			uint64_t versions[3] = {0, 0, 0};
			bool ready = true;

			for (uint32_t q = 0; q < 3; ++q) {
				Fixture batch;
				Fixture step;
				if (!prepareCase(batch, c, true) || !prepareCase(step, c, true)) {
					ready = false;
					break;
				}
				auto config = configFor(batch, c, true, quanta[q]);
				auto stepConfig = configFor(step, c, true, quanta[q]);
				auto before = batch.journal.getMetrics().versions;

				{
					const uint32_t i = 0; // a case is one run
					runFrame(batch, c, i, config);
					beginFrame(step, c, i, stepConfig);
					while (step.interp.stepOnce(step.report)) { }

					RunSnapshot a;
					RunSnapshot b;
					snapshot(batch, a);
					snapshot(step, b);
					if (!oracle::compareRuns(a, b, CompareOptions{Level::Store, true},
								mem_std::toString(c.name, " under ", quantumName(quanta[q]),
										" run ", i, " batch vs stepped"))) {
						stepped = false;
					}
				}
				snapshot(batch, under[q]);
				versions[q] = batch.journal.getMetrics().versions - before;
			}
			if (!ready) {
				agreed = false;
				continue;
			}

			if (c.outcome != RunOutcome::OpError) {
				// Without the ladder: that is the one thing that is allowed to differ.
				CompareOptions across{Level::Store, false};
				if (!oracle::compareRuns(under[0], under[1], across,
							mem_std::toString(c.name, " Step vs Failable"))
						|| !oracle::compareRuns(under[0], under[2], across,
								mem_std::toString(c.name, " Step vs Run"))) {
					agreed = false;
				}
			}

			if (!(versions[0] >= versions[1] && versions[1] >= versions[2] && versions[2] >= 1)) {
				sprt::cout << "       " << c.name << ": boundaries " << versions[0] << " / "
						   << versions[1] << " / " << versions[2] << "\n";
				ladders = false;
			}
		}
		check(stepped, "interp-oracle: under every quantum, batch and stepped are one run, ladder "
					   "included");
		check(agreed, "interp-oracle: the three quanta leave the same run, the ladder aside");
		check(ladders, "interp-oracle: and the ladder falls as the quantum coarsens, never to zero");
	}

	// ---- the oracle refuses what it should ----------------------------------------------------------
	//
	// Each pair below differs in exactly one channel, and the comparison has to say no. A comparison
	// that passed one of these would pass a compiled engine that was wrong in the same way.

	{
		auto snapshotOf = [](StringView json, RunSnapshot &out) {
			Fixture fx;
			if (!fx.prepare(json)) {
				return false;
			}
			fx.run();
			snapshot(fx, out);
			return true;
		};

		RunSnapshot chain;
		RunSnapshot sequence;
		check(snapshotOf(Chain, chain) && snapshotOf(Sequence, sequence),
				"interp-oracle: two different graphs run");
		check(!oracle::compareRuns(chain, sequence, CompareOptions{Level::Observable},
					  StringView("chain vs sequence (expected to differ)")),
				"interp-oracle: a different order is refused");

		// The same order, a different value: the trace agrees and the records do not.
		RunSnapshot three;
		RunSnapshot four;
		check(snapshotOf(Arithmetic, three)
						&& snapshotOf(StringView(R"json({"formatVersion": 1,
					"nodes": [
						{"id": 1, "op": "value.float", "params": {"value": 4.0}},
						{"id": 2, "op": "math.mulFloat", "params": {"rhs": 2.0}},
						{"id": 3, "op": "math.addFloat", "params": {"rhs": 1.0}}
					],
					"edges": [
						{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"},
						{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "lhs"}
					]})json"),
								four),
				"interp-oracle: the same graph runs with two literals");
		bool sameOrder = three.log.size() == four.log.size();
		for (size_t i = 0; sameOrder && i < three.log.size(); ++i) {
			sameOrder = three.log[i].id == four.log[i].id;
		}
		check(sameOrder, "interp-oracle: the two runs took the same order");
		check(!oracle::compareRuns(three, four, CompareOptions{Level::Observable},
					  StringView("3.0 vs 4.0 (expected to differ)")),
				"interp-oracle: and a different value in a record is refused at level A");

		// The ladder: two quanta of one run compare equal without it and unequal with it.
		{
			auto under = [](RollbackQuantum q, RunSnapshot &out) {
				Fixture fx;
				if (!fx.prepare(Walk) || !fx.prepareJournal()) {
					return false;
				}
				auto config = fx.journalConfig();
				config.quantum = q;
				fx.run(config);
				snapshot(fx, out);
				return true;
			};
			RunSnapshot step;
			RunSnapshot run;
			check(under(RollbackQuantum::Step, step) && under(RollbackQuantum::Run, run),
					"interp-oracle: the walk runs under Step and under Run");
			check(oracle::compareRuns(step, run, CompareOptions{Level::Store, false},
						  StringView("Step vs Run, ladder aside")),
					"interp-oracle: without the ladder they are one run");
			check(!oracle::compareRuns(step, run, CompareOptions{Level::Store, true},
						  StringView("Step vs Run, ladder (expected to differ)")),
					"interp-oracle: with it they are not");
		}
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
