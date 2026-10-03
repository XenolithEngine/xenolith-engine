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

// The fast mode, held to the interpreter at level A.
//
// The fast store (SPFlowFast.h) keeps a run's frames in a scratch arena of its own and its
// bookkeeping - the front, the activation tree, the open and stalled lists, the two counters - in
// host memory. What it gives up is what a shipped program does not use: an image, `attach`, undoing
// a unit of work. What it must not give up is anything a run computes, and that is level A of the
// contract: the same report unit for unit, the same records and the same activation tree logically.
//
// Level B is not asked for and could not be: there is no run image to compare. That is
// `codegen-exact-arena`'s, and between the two every graph of the corpus is held to the interpreter
// under both stores.
//
// Beside the equivalence, the three refusals that are the mode - `attach`, `stepBack`, and a journal
// at a quantum finer than `Run` - and the trace policy: a run with no execution log at all leaves
// the same records as one that writes it.

#include "codegen_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

#include <type_traits>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::codegenfx;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

// Level A, and the one place that says what level A is in this section's terms: the observable run,
// with the ladder of versions folded out because the fast store never takes one.
constexpr CompareOptions Observable{Level::Observable, false, true};

// ---- the corpus, case for case ------------------------------------------------------------------

template <typename A>
void exactFast(StringView kind) {
	sprt::cout << "   -- " << kind << " --\n";

	bool prepared = true;
	bool named = true;
	bool compiledStep = true;
	bool same = true;
	bool stepped = true;
	bool quiet = true;

	for (auto &c : corpus::Cases) {
		auto u = findUnit(c.name);
		Side<A> i;
		Side<A> f;
		Side<A> q;
		if (!u || !i.prepare(c, nullptr, false) || !f.prepareFast(c, u, false)
				|| !q.prepareFast(c, u, false, true)) {
			sprt::cout << "       " << kind << " " << c.name << ": does not prepare\n";
			prepared = false;
			continue;
		}
		if (f.engine->getEngineName() != StringView("fast")
				|| q.engine->getEngineName() != StringView("fast")) {
			named = false;
		}
		// And the fast side really is running the unit's own step over the fast store, not the
		// machine's own door: the two are one run either way (`codegen-ops` says so), but a section
		// that meant to measure the compiled path and quietly measured the other would be measuring
		// the wrong thing.
		auto steps = f.unit.template getSteps<NoEnv>();
		if (!steps || !steps->template get<CompiledFastLocalT<A, NoEnv>>()) {
			compiledStep = false;
		}

		i.runFrame(corpus::configFor(i.fx, c, false));
		f.runFrame(corpus::configFor(f.fx, c, false));
		q.runFrame(corpus::configFor(q.fx, c, false));

		RunSnapshot a;
		RunSnapshot b;
		i.snapshot(a);
		f.snapshot(b);
		if (!oracle::compareRuns(a, b, Observable,
					mem_std::toString(kind, " ", c.name, " interp vs fast"))) {
			same = false;
		}

		// The quiet run writes no log, so the report cannot be compared - and that is what has to be
		// checked instead: the outcome and the records it leaves are the ones the logged run left.
		RunSnapshot silent;
		q.snapshot(silent);
		if (silent.outcome != b.outcome
				|| !oracle::compareRuns(silent, b, CompareOptions{Level::Observable, false, false},
						mem_std::toString(kind, " ", c.name, " quiet records"))) {
			quiet = false;
		}
		if (!q.fx.report.log.empty()) {
			sprt::cout << "       " << kind << " " << c.name << ": the quiet run wrote "
					   << q.fx.report.log.size() << " log entries\n";
			quiet = false;
		}

		// A fourth fixture, stepped rather than run whole: a fast run has no image to compare after
		// every unit, but it still has to be one run - the batch and the stepped form must end in
		// the same place, which is what `interp-stepping` says about the interpreter.
		Side<A> s;
		if (s.prepareFast(c, u, false)) {
			s.beginFrame(corpus::configFor(s.fx, c, false));
			while (s.step()) { }
			RunSnapshot bit;
			s.snapshot(bit);
			if (!oracle::compareRuns(b, bit, Observable,
						mem_std::toString(kind, " ", c.name, " fast batch vs stepped"))) {
				stepped = false;
			}
		} else {
			stepped = false;
		}
	}

	check(prepared, mem_std::toString("codegen-exact-fast[", kind, "]: all ", corpus::CaseCount,
								" cases prepare on every side"));
	check(named, mem_std::toString("codegen-exact-fast[", kind,
						   "]: a run over the fast store names itself `fast`"));
	check(compiledStep, mem_std::toString("codegen-exact-fast[", kind,
								  "]: and performs its nodes through the unit's own step, compiled "
								  "for this store"));
	check(same, mem_std::toString("codegen-exact-fast[", kind,
						  "]: the fast run is the interpreter's run - the report, the records, "
						  "the activation tree"));
	check(stepped, mem_std::toString("codegen-exact-fast[", kind,
							 "]: and a fast run stepped a unit at a time ends where the batch run "
							 "ends"));
	check(quiet, mem_std::toString("codegen-exact-fast[", kind,
						   "]: with no execution log at all, the same outcome and records"));
}

// ---- what the mode refuses ---------------------------------------------------------------------

void refusals() {
	auto c = findCase(StringView("chain"));
	auto u = c ? findUnit(c->name) : nullptr;
	auto found = c != nullptr && u != nullptr;
	check(found, StringView("codegen-exact-fast: the case the refusals are asked of"));
	if (!found) {
		return;
	}

	// `attach`: half of a fast run is host memory, and no image holds it.
	{
		Side<Arena> f;
		if (f.prepareFast(*c, u, false)) {
			auto config = corpus::configFor(f.fx, *c, false);
			auto st = f.engine->attach(f.fx.ops, f.fx.arena, config, f.fx.report);
			check(st != Status::Ok && f.fx.report.outcome == flow::RunOutcome::Invalid,
					StringView("codegen-exact-fast: attach refuses - there is no image to pick a "
							   "fast run up out of"));
		}
	}

	// `stepBack`: a version could restore the frames and nothing else of the run.
	{
		Side<Arena> f;
		if (f.prepareFast(*c, u, true)) {
			auto config = corpus::configFor(f.fx, *c, true, flow::RollbackQuantum::Run);
			auto begun = f.engine->begin(f.fx.ops, f.fx.arena, config, f.fx.report);
			f.step();
			auto st = f.engine->stepBack(f.fx.report);
			check(begun == Status::Ok && st == Status::ErrorNotSupported,
					StringView("codegen-exact-fast: a journal at the `Run` quantum is accepted, and "
							   "stepBack refuses"));
		}
	}

	// The quantum: a boundary per unit of work would promise an undo the store cannot perform, so
	// the run is refused before it takes the first one.
	{
		bool refused = true;
		bool located = true;
		for (auto q : {flow::RollbackQuantum::Step, flow::RollbackQuantum::Failable}) {
			Side<Arena> f;
			if (!f.prepareFast(*c, u, true)) {
				continue;
			}
			auto config = corpus::configFor(f.fx, *c, true, q);
			auto st = f.engine->begin(f.fx.ops, f.fx.arena, config, f.fx.report);
			if (st == Status::Ok || f.fx.report.outcome != flow::RunOutcome::Invalid) {
				refused = false;
			}
			if (!test::hasDiag(f.fx.report.diagnostics,
						test::getDiagCodeName(flow::DiagCode::CodegenQuantumUnsupported))) {
				located = false;
			}
		}
		check(refused,
				StringView("codegen-exact-fast: a quantum finer than `Run` is refused at begin()"));
		check(located,
				StringView("codegen-exact-fast: and the refusal is `codegen-quantum-unsupported`"));
	}

	// And with no journal at all, the same run is perfectly ordinary under any quantum: the quantum
	// only ever meant "how often to take a boundary", and without a journal there are none.
	{
		Side<Arena> f;
		if (f.prepareFast(*c, u, false)) {
			auto config = corpus::configFor(f.fx, *c, false, flow::RollbackQuantum::Step);
			auto st = f.engine->run(f.fx.ops, f.fx.arena, config, f.fx.report);
			check(st == Status::Ok && f.fx.report.outcome == flow::RunOutcome::Completed,
					StringView("codegen-exact-fast: without a journal the quantum is not a promise "
							   "and the run goes through"));
		}
	}
}

} // namespace

void performCodegenExactFastTests() {
	sprt::cout << "\n== flow codegen: the fast mode against the interpreter ==\n";

	refusals();

	constexpr bool shadowIsItsOwnKind = !std::is_same_v<flow::value::ShadowArena, flow::value::TrackedArena>;
	if (test::full()) {
		exactFast<flow::value::PlainArena>(StringView("plain"));
		exactFast<flow::value::TrackedArena>(StringView("tracked"));
		if constexpr (shadowIsItsOwnKind) {
			exactFast<flow::value::ShadowArena>(StringView("shadow"));
		}
	} else {
		// The suite's own kind. The fast store's own frames are a PlainArena whatever the kind, so
		// one kind reduced and three in full.
		exactFast<Arena>(shadowIsItsOwnKind ? StringView("shadow") : StringView("tracked"));
	}
}

} // namespace stappler
