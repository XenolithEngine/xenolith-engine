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

// An operation's inline form is its registry form.
//
// There is one body per operation and two spellings of it: `&ops::inl::name<OpContext>` in the
// registry, which the interpreter calls through a vtable, and `ops::inl::name(ctx)` written into a
// generated unit, which the compiler inlines into the step. This section turns that into three
// measurements.
//
// The pairing exists. Every operation of the standard library declares an inline form; the two test
// probes deliberately declare none, because they are what proves a unit still runs an operation it
// cannot call by name.
//
// The generator makes the right choice, per node. For every node of every graph the corpus has, the
// emitted source either calls the operation's inline form by the name the registry gave, or fetches
// `getInvoke()` from the descriptor. The check is on the exact text, because the text is the
// decision.
//
// The two paths are the same run. The same unit is loaded twice against the same fixture: once with
// its compiled steps and once with them suppressed, which leaves the machine to perform every node
// itself through the registry. The two are stepped side by side and compared at level B - the same
// status, the same outputs, the same `fired`, the same bytes - after every unit of work.
//
// What this section does not do is compare the compiled run against the interpreter: that is
// `codegen-exact-arena`, over three kinds of arena and three quanta. Here both sides are the unit,
// so what is isolated is the door and the call, and nothing else.

#include "codegen_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::codegenfx;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

// The two operations the tests bring with them (FixtureT::registerOps), and the only ones in this
// binary that have no inline form. Named here rather than counted, so that a probe acquiring one -
// or a library operation losing one - is a failure with a name in it.
constexpr StringView Probes[] = {
	StringView("test.fail"),
	StringView("test.split"),
};

bool isProbe(StringView name) {
	for (auto &p : Probes) {
		if (p == name) {
			return true;
		}
	}
	return false;
}

// What a unit's source must say about one node: the whole function, from the comment that names the
// operation to the call. Spelled out rather than searched for piecemeal, because a shape that
// drifted - the door built over another node's index, the site bound after the call - would still
// contain every fragment.
mem_std::String expectedStep(uint32_t node, const flow::OpDesc &op) {
	auto head = mem_std::toString("// node ", node, ": ", op.getName());
	auto inlineName = op.getInlineName();
	if (inlineName.empty()) {
		head.append(" (through the registry: no inline form)");
	}
	auto out = mem_std::toString(head, "\ntemplate <typename Local>\nstatic Status step", node,
			"(flow::CompiledStepSiteT<Local> &site) {\n\tflow::StaticContext<Tables, ", node,
			", Local> ctx;\n\tctx.bind(site);\n");
	if (inlineName.empty()) {
		out.append("\tauto invoke = site.op->getInvoke();\n"
				   "\tauto st = invoke ? invoke(ctx) : Status::ErrorNotImplemented;\n");
	} else {
		out.append(mem_std::toString("\tauto st = ", inlineName, "(ctx);\n"));
	}
	out.append("\tctx.finish(site);\n\treturn st;\n}\n");
	return out;
}

// Every node of `g`, against the source `emitted` was written from. Returns how many took each path,
// so that a graph which happens to use no probe still says so.
bool checkSteps(const flow::RuntimeGraph &g, const codegen::Emitted &unit, StringView what,
		uint32_t &direct, uint32_t &registry) {
	bool ok = true;
	for (uint32_t n = 0; n < g.getNodeCount(); ++n) {
		auto op = g.getNodeAt(n).op;
		if (!op) {
			continue;
		}
		if (op->getInlineName().empty()) {
			++registry;
		} else {
			++direct;
		}
		auto expected = expectedStep(n, *op);
		if (!sourcesHave(unit, expected)) {
			sprt::cout << "       " << what << ": node " << n << " (" << op->getName()
					   << ") is not performed as it should be; expected\n-----\n" << expected
					   << "-----\n";
			ok = false;
		}
	}
	return ok;
}

// ---- the pairing ---------------------------------------------------------------------------------

void checkDeclarations() {
	Fixture fx;
	auto registered = fx.registerOps();
	check(registered, StringView("codegen-ops: the library and the probes register"));
	if (!registered) {
		return;
	}

	uint32_t withForm = 0;
	uint32_t probes = 0;
	bool ok = true;
	for (uint32_t i = 0; i < fx.ops.getCount(); ++i) {
		auto op = fx.ops.getAt(i);
		auto has = !op->getInlineName().empty();
		if (isProbe(op->getName())) {
			++probes;
			if (has) {
				sprt::cout << "       " << op->getName()
						   << ": a probe declares an inline form; the adapter path then has "
							  "nothing left to prove\n";
				ok = false;
			}
			continue;
		}
		if (!has) {
			sprt::cout << "       " << op->getName() << ": no inline form\n";
			ok = false;
		} else {
			++withForm;
		}
	}

	check(ok && probes == uint32_t(sizeof(Probes) / sizeof(Probes[0])),
			mem_std::toString("codegen-ops: all ", withForm,
					" library operations carry an inline form, and the ", probes,
					" test probes carry none"));
}

// ---- the generator's choice ----------------------------------------------------------------------

void checkEmittedCalls() {
	uint32_t direct = 0;
	uint32_t registry = 0;
	bool ok = true;
	bool emitted = true;
	for (auto &c : corpus::Cases) {
		Fixture fx;
		codegen::Emitted unit;
		if (!emitCase(fx, c, unit)) {
			sprt::cout << "       " << c.name << ": does not emit\n";
			emitted = false;
			continue;
		}
		if (!checkSteps(fx.graph, unit, c.name, direct, registry)) {
			ok = false;
		}
	}
	check(emitted, mem_std::toString("codegen-ops: all ", corpus::CaseCount, " cases emit"));
	check(ok && direct > 0 && registry > 0,
			mem_std::toString("codegen-ops: of ", direct + registry,
					" nodes in the corpus the unit calls ", direct, " operations by name and ",
					registry, " through the registry, each as its descriptor says"));
}

// ---- the two paths -------------------------------------------------------------------------------
//
// The same rows twice: once with the unit's own step, once with it suppressed so that the machine
// fills its own door and calls through the registry. Stepped side by side, compared after every
// unit of work at level B - which folds "the same status, the same outputs, the same fired" into one
// statement, because a difference in any of them moves the bytes or the log.

void checkBothPaths() {
	bool prepared = true;
	bool carries = true;
	bool same = true;
	uint32_t units = 0;
	for (auto &c : corpus::Cases) {
		auto u = findUnit(c.name);
		Side<Arena> compiled;
		Side<Arena> plain;
		if (!u || !compiled.prepare(c, u, false, true) || !plain.prepare(c, u, false, false)) {
			sprt::cout << "       " << c.name << ": does not prepare\n";
			prepared = false;
			continue;
		}
		if (!compiled.unit.getSteps<flow::NoEnv>()
				|| plain.unit.getSteps<flow::NoEnv>()) {
			sprt::cout << "       " << c.name
					   << ": the unit does not carry a compiled step, or carries one when it was "
						  "loaded without\n";
			carries = false;
			continue;
		}

		compiled.beginFrame(corpus::configFor(compiled.fx, c, false));
		plain.beginFrame(corpus::configFor(plain.fx, c, false));

		uint32_t diverged = 0;
		units += oracle::lockstep([&] { return compiled.step(); }, [&] { return plain.step(); },
				[&](RunSnapshot &s) { compiled.snapshot(s); },
				[&](RunSnapshot &s) { plain.snapshot(s); }, CompareOptions{Level::Store},
				mem_std::toString(c.name, " by name vs through the registry"), diverged);
		if (diverged != flow::InvalidIndex) {
			same = false;
		}
	}
	check(prepared, mem_std::toString("codegen-ops: all ", corpus::CaseCount,
								" cases load twice, with the unit's step and without"));
	check(carries,
			StringView("codegen-ops: a unit carries a compiled step, and loading its rows alone "
					   "leaves the machine to perform the node itself"));
	check(same, mem_std::toString("codegen-ops: over ", units,
						  " units of work the two paths are one run, byte for byte"));
}

} // namespace

void performCodegenOpsTests() {
	sprt::cout << "\n== flow codegen: an operation's inline form is its registry form ==\n";

	checkDeclarations();
	checkEmittedCalls();
	checkBothPaths();
}

} // namespace stappler
