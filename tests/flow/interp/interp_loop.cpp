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

// Subtask F7: loops, and the activations that make their iterations independent.
//
// `flow.forEach` knows nothing about loops. It reads a cursor out of its own record, hands out an
// item, fires an exec output, and returns - exactly what `branch` does. Everything that makes that
// into a loop is the interpreter's, and it is two rules:
//
//   * an exec output its operation declared as opening a SCOPE opens an ACTIVATION: fresh records
//     for every node of the body, so what iteration three computes is not what iteration two did;
//   * when the ready front drains and an iteration is still open, the interpreter closes it and
//     hands control back to the node that opened it.
//
// The second rule is why an author does not wire the body's tail back to the loop. A body ending in
// a branch has two tails; a branch that did not fire has none. "The work of this iteration ran out"
// is the only signal that is always right, and the check for a body with an untaken branch below is
// the one that would fail under the other design.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;

uint32_t countKind(const RunReport &report, RunStepKind kind) {
	uint32_t n = 0;
	for (auto &it : report.log) {
		if (it.kind == kind) {
			++n;
		}
	}
	return n;
}

} // namespace

void performInterpLoopTests() {
	sprt::cout << "\n== flow interp: loops and activations ==\n";

	// ---- one turn per element -------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::Loop), "interp-loop: the loop graph builds");
		check(fx.graph.getScopeCount() == 2, "interp-loop: the body is a scope of its own");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: the loop run completes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4 2 3 4 2 3 4 2 5"),
				"interp-loop: three elements, three turns, and then the continuation");
		check(countKind(fx.report, RunStepKind::CloseActivation) == 3,
				"interp-loop: each turn was closed exactly once");
		check(fx.interp.getLocal().getActivationCount() == 4,
				"interp-loop: the root plus one activation per turn");
		check(fx.interp.getLocal().getOpenCount() == 0,
				"interp-loop: and none of them is left open");
	}

	// ---- what one turn computed is not what the next one did --------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::Loop), "interp-loop: the isolation graph builds");
		check(fx.run() == Status::Ok, "interp-loop: the isolation run completes");

		flow::value::Var a;
		flow::value::Var b;
		flow::value::Var c;
		check(fx.outputAt(4, 0, 1, a) && fx.outputAt(4, 0, 2, b) && fx.outputAt(4, 0, 3, c),
				"interp-loop: every turn left a record of its own");
		check(a.i == 110 && b.i == 120 && c.i == 130,
				"interp-loop: and each holds what THAT turn computed - i is not visible from j");

		// The counterpart: the loop node itself is outside the body, so it has ONE record, and the
		// body reads it by walking up the activation tree rather than by having a copy.
		check(fx.interp.getLocal().resolveScope(1, 0) == RootActivation
						&& fx.interp.getLocal().resolveScope(3, 0) == RootActivation,
				"interp-loop: a turn resolves the enclosing scope to the run's own activation");
		check(fx.interp.getLocal().resolveScope(RootActivation, 1) == NullActivation,
				"interp-loop: and the root cannot see into a body, which is the direction that has "
				"no " "answer");
	}

	// ---- an empty sequence is no turns at all ------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::LoopEmpty)),
				"interp-loop: the empty graph builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: an empty sequence completes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 5"),
				"interp-loop: straight to the continuation, with no turn at all");
		check(fx.interp.getLocal().getActivationCount() == 1,
				"interp-loop: and no activation was opened");
	}

	// ---- a body whose branch did not fire still ends its turn ---------------------------------------

	{
		// Under the other design - the author wiring the body's tail back to the loop - this graph
		// stops after one turn: the `false` branch has no tail to wire, so nothing hands control back.
		Fixture fx;
		check(fx.prepare(StringView(corpus::LoopUntakenBranch)),
				"interp-loop: the untaken-branch graph builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: it completes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 2 3 2 3 2 5"),
				"interp-loop: all three turns run, though no turn ever reached a tail");
	}

	// ---- nested ---------------------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::LoopNested)),
				"interp-loop: the nested graph builds");
		check(fx.graph.getScopeCount() == 3,
				"interp-loop: two loops are two scopes plus the graph");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: the nested run completes");
		checkEq(StringView(fx.report.getTrace()),
				StringView("1 2 3 4 3 4 3 4 3 2 3 4 3 4 3 4 3 2 5"),
				"interp-loop: the inner loop runs to its end inside every turn of the outer one");

		// The tree: the root, two turns of the outer loop, and three turns of the inner loop under
		// each of them.
		mem_std::Value dump;
		fx.interp.getLocal().describe(dump);
		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{"activation": 0, "scope": 0},
			{"activation": 1, "scope": 1, "parent": 0, "iteration": 0, "opener": 2},
			{"activation": 2, "scope": 2, "parent": 1, "iteration": 0, "opener": 3},
			{"activation": 3, "scope": 2, "parent": 1, "iteration": 1, "opener": 3},
			{"activation": 4, "scope": 2, "parent": 1, "iteration": 2, "opener": 3},
			{"activation": 5, "scope": 1, "parent": 0, "iteration": 1, "opener": 2},
			{"activation": 6, "scope": 2, "parent": 5, "iteration": 0, "opener": 3},
			{"activation": 7, "scope": 2, "parent": 5, "iteration": 1, "opener": 3},
			{"activation": 8, "scope": 2, "parent": 5, "iteration": 2, "opener": 3}
		])json"));
		check(test::compareValues(dump.getValue("activations"), expect, StringView("activations")),
				"interp-loop: and the activation tree says which turn of which loop each one was");

		// A record is found by offset - the activation owns a frame, the node's place inside it is a
		// build-time fact - so "no such record" cannot be discovered by failing to find a key. It has
		// to be refused, and this is the case that matters: a node of a loop body, asked for in the
		// root activation. A debugger asks exactly that, because a breakpoint's activation defaults to
		// the root, and the root's frame is laid out for a different scope entirely.
		auto &local = fx.interp.getLocal();
		auto bodyNode = fx.graph.findNode(3);
		check(bodyNode != InvalidIndex && fx.graph.getNodeAt(bodyNode).scope != 0,
				"interp-loop: node 3 belongs to a loop body, not to the graph");
		check(local.frameFor(bodyNode, RootActivation) == NullAddr,
				"interp-loop: and asking for it in the root activation is refused, not answered "
				"with " "an address inside somebody else's frame");
		check(!local.hasRecord(bodyNode, RootActivation)
						&& local.getState(bodyNode, RootActivation) == NullAddr
						&& local.getRecord(bodyNode, RootActivation) == NullAddr,
				"interp-loop: it has nothing there at all");
		// Activation 1 is a turn of scope 1, which is the scope node 3 belongs to - it OPENS scope 2
		// rather than living in it.
		check(local.activationScope(1) == fx.graph.getNodeAt(bodyNode).scope
						&& local.frameFor(bodyNode, 1) != NullAddr,
				"interp-loop: while in an activation of its own scope it resolves");

		check(fx.interp.getLocal().resolveScope(7, 1) == 5,
				"interp-loop: an inner turn resolves the outer loop to the turn it belongs to");
		check(fx.interp.getLocal().resolveScope(7, 0) == RootActivation,
				"interp-loop: and the graph itself to the root, two levels up");
	}

	// ---- a loop on the first branch of a sequence -------------------------------------------------------
	//
	// A turn closes when the ready front is empty ALTOGETHER - stepOnce() finds nothing to pop - and
	// not when the body's own work has run out. The sequence put its second branch on the front
	// before the loop opened its first turn, so that branch is still there when the first body drains,
	// and it runs BEFORE the second turn. "The first branch runs to completion before the second"
	// (interp-exec) is exact for a branch without a loop in it and an approximation otherwise.
	//
	// This is the order as the code defines it, pinned so that a second engine is held to the same
	// one (docs/planning/graph_codegen.md, §3 item 8). Whether it SHOULD be this order is a separate
	// question, and changing it means changing the interpreter and this line together.

	{
		Fixture fx;
		check(fx.prepare(corpus::LoopInSequence), "interp-loop: the loop-in-sequence graph builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: the loop-in-sequence run completes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4 5 3 4 3 6"),
				"interp-loop: the sequence's second branch runs after the FIRST turn of the loop, "
				"not after the loop");
		check(countKind(fx.report, RunStepKind::CloseActivation) == 2,
				"interp-loop: two turns, two closes, and the second branch between them");
	}

	// ---- a plain exec cycle: no scope, no activations, just a repeat -----------------------------------

	{
		// Part II item 9: a cycle over exec edges is a legal construction. Without a scope it is a
		// repeat rather than a loop - one record, re-run - and the only thing that ends it is the
		// ceiling, which is exactly why the ceiling was written in F-I.
		Fixture fx;
		check(fx.prepare(StringView(corpus::Repeat)),
				"interp-loop: the repeat graph builds");

		RunConfig config;
		config.maxSteps = 9;
		check(fx.run(config) != Status::Ok && fx.report.outcome == RunOutcome::StepLimit,
				"interp-loop: an exec cycle repeats until the ceiling stops it, rather than "
				"running once");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 2 3 2 3 2 3"),
				"interp-loop: and every turn of it really ran");
		check(fx.interp.getLocal().getActivationCount() == 1,
				"interp-loop: with no activations at all - a repeat is not a loop body");
	}

	// ---- the ceiling on turns -----------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::Loop), "interp-loop: the activation-ceiling graph builds");

		RunConfig config;
		config.maxActivations = 2; // the root plus one turn
		check(fx.run(config) != Status::Ok && fx.report.outcome == RunOutcome::ActivationLimit,
				"interp-loop: a loop that outruns its allowance is diagnosed, not left to exhaust "
				"the " "entity budget");
		check(fx.report.diagnostics.isArray() && fx.report.diagnostics.size() > 0
						&& test::getDiagCodeName(fx.report.diagnostics.getValue(0))
								== "activation-limit",
				"interp-loop: and the diagnostic says so by name");
		check(fx.interp.getLocal().getActivationCount() == 2,
				"interp-loop: with exactly what it was allowed");
	}

	// ---- stepping and stepping back across a turn boundary --------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(corpus::Loop) && fx.prepareJournal(),
				"interp-loop: the stepping graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-loop: the stepping run begins");

		// Up to the first close of a turn.
		while (fx.interp.stepOnce(fx.report)
				&& fx.report.log.back().kind != RunStepKind::CloseActivation) { }
		check(!fx.report.log.empty() && fx.report.log.back().kind == RunStepKind::CloseActivation,
				"interp-loop: a run can be stopped on the boundary between two turns");
		check(fx.interp.getLocal().getOpenCount() == 0,
				"interp-loop: the turn it closed is closed");

		auto trace = fx.report.getTrace();
		check(fx.interp.stepBack(fx.report) == Status::Ok,
				"interp-loop: and the close can be undone");
		check(fx.interp.getLocal().getOpenCount() == 1,
				"interp-loop: which puts the turn back to open");
		checkEq(StringView(fx.report.getTrace()), StringView(trace),
				"interp-loop: without changing the order the nodes ran in - a close runs no node");

		check(fx.interp.finish(fx.report) == Status::Ok
						&& fx.report.outcome == RunOutcome::Completed,
				"interp-loop: and the run carries on to the end");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 3 4 2 3 4 2 3 4 2 5"),
				"interp-loop: through the same order as a run that was never interrupted");
	}


	// ---- two runs of a loop are the same run ----------------------------------------------------------

	{
		Fixture a;
		Fixture b;
		check(a.prepare(corpus::Loop) && b.prepare(corpus::Loop),
				"interp-loop: two determinism fixtures build");
		check(a.run() == Status::Ok && b.run() == Status::Ok,
				"interp-loop: both loop runs complete");
		check(takeImage(a.arena) == takeImage(b.arena),
				"interp-loop: and leave byte-identical stores, activation tree and all");

		auto base = measure(a.arena);
		a.interp.reset();
		check(measure(a.arena).count <= base.count,
				"interp-loop: a run with loops still lets go of everything it took");
	}

	// ---- a literal list is read where it lies -----------------------------------------------------
	//
	// `items` on a forEach, and on array.getInt, is a CONSTANT in every graph anyone writes. It used
	// to be rebuilt in the arena on every call - an alloc, a decode of every element, and a free at
	// the end of the step - which made a loop cost the square of its length: 64 elements written per
	// turn, 64 turns. It is now read out of the graph's own constant table, and the arena never sees
	// it.
	//
	// That is an optimization, so what has to be pinned is that it is only that. The same list fed
	// from a constant and across a data edge has to produce the same turns, in the same order, with
	// the same values - and the edge-fed case is what still goes through the arena, so the two
	// together say the paths agree rather than that either one is self-consistent.
	{
		// 1 event -> 2 forEach over a literal, body 6; the same list is ALSO handed to array.getInt
		// as a literal (3), indexed by the loop's own `index` output, so a turn reads the list twice
		// by two different routes and 4 adds the two answers together.
		Fixture fx;
		check(fx.prepare(corpus::LoopLiteral), "interp-loop: the literal-list graph builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: a loop over a literal list completes");
		check(countKind(fx.report, RunStepKind::CloseActivation) == 4,
				"interp-loop: four elements, four turns");

		// item + items[index] is 2*items[index], which is what says the two routes to the same list
		// agree element by element rather than only in length.
		bool doubled = true;
		const int64_t expect[] = {14, 16, 18, 20};
		for (uint32_t turn = 0; turn < 4; ++turn) {
			flow::value::Var v;
			doubled = doubled && fx.outputAt(4, 0, turn + 1, v) && v.i == expect[turn];
		}
		check(doubled, "interp-loop: and every turn read the same element by both routes");

		auto base = measure(fx.arena);
		fx.interp.reset();
		check(measure(fx.arena).count <= base.count,
				"interp-loop: reading a literal takes nothing the run has to give back");
	}

	// The boundary the constant path has to get right on its own: a literal with no elements. The
	// arena path answered it by materialising an empty blob; this one has to answer it from the
	// constant table, and getting it wrong would either run a turn that has no item or hang.
	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::LoopEmpty)),
				"interp-loop: the empty-literal graph builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-loop: a loop over an empty literal completes");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 5"),
				"interp-loop: with no turns at all, straight to the continuation");
	}

	// array.getInt past the end of a literal is the operation's own refusal, and it stays one: the
	// count now comes from the constant table instead of from the arena, and an index the list does
	// not have has to be refused just the same.
	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::ArrayOutOfRange)),
				"interp-loop: the out-of-range literal graph builds");
		check(fx.run() != Status::Ok || fx.report.outcome != RunOutcome::Completed,
				"interp-loop: an index past the end of a literal is refused, not read");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
