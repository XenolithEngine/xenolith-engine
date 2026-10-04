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

// Functions at run time: a call node opens an activation of its function's scope, the body's entry
// reads the call's inputs, a return writes the call's outputs, and when the function's work runs
// out the call node gets its second turn and leaves through the return's exit. A call site in inline
// mode runs a copy of the body instead, and the two modes compute the same values.

#include "interp_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::interpfx;

// A library: documents by name.
struct LibraryHost : FunctionHost {
	mem_std::Vector<sprt::pair<mem_std::String, GraphAsset *>> docs;

	~LibraryHost() {
		for (auto &it : docs) { delete it.second; }
	}

	bool add(StringView json) {
		auto asset = new GraphAsset();
		asset->init();
		mem_std::Value diag;
		if (asset->load(data::read<mem_std::Interface>(json), &diag) != Status::Ok) {
			sprt::cout << "       library: " << data::toString<mem_std::Interface>(diag, true)
					   << "\n";
			delete asset;
			return false;
		}
		docs.emplace_back(asset->getName().str<memory::StandardInterface>(), asset);
		return true;
	}

	const GraphAsset *findFunction(StringView name) const override {
		for (auto &it : docs) {
			if (StringView(it.first) == name) {
				return it.second;
			}
		}
		return nullptr;
	}
};

// One exec function defined in the document: y = x + 10, called with x = 5, and the call's output
// read by a node after it (y + 1). MODE is the call node's mode setting.
#define ADD_TEN(MODE) R"json({"__meta": {"kind": "graph", "version": 2}, "name": "main",
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "fn.addTen", "params": {"x": 5}, "settings": {"mode": ")json" MODE R"json("}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "math.addInt", "params": {"rhs": 1}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "done", "to": 3},
		{"kind": "data", "from": 2, "fromPin": "y", "to": 4, "toPin": "lhs"}
	],
	"functions": [{"name": "addTen",
		"interface": {"inputs": [{"name": "x", "type": "int"}],
			"outputs": [{"name": "y", "type": "int"}], "execIn": true, "execOut": ["done"]},
		"nodes": [
			{"id": 10, "op": "fn.entry"},
			{"id": 11, "op": "math.addInt", "params": {"rhs": 10}},
			{"id": 12, "op": "fn.return"}
		],
		"edges": [
			{"kind": "exec", "from": 10, "fromPin": "start", "to": 12},
			{"kind": "data", "from": 10, "fromPin": "x", "to": 11, "toPin": "lhs"},
			{"kind": "data", "from": 11, "fromPin": "result", "to": 12, "toPin": "y"}
		]}]
})json"

// A pure function: no exec pins, a call node computed when its inputs are.
#define TWICE(MODE) R"json({"__meta": {"kind": "graph", "version": 2}, "name": "pure",
	"nodes": [
		{"id": 1, "op": "fn.twice", "params": {"v": 21}, "settings": {"mode": ")json" MODE R"json("}},
		{"id": 2, "op": "math.addInt", "params": {"rhs": 0}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "w", "to": 2, "toPin": "lhs"}
	],
	"functions": [{"name": "twice",
		"interface": {"inputs": [{"name": "v", "type": "int"}], "outputs": [{"name": "w", "type": "int"}]},
		"nodes": [
			{"id": 10, "op": "fn.entry"},
			{"id": 11, "op": "math.addInt"},
			{"id": 12, "op": "fn.return"}
		],
		"edges": [
			{"kind": "data", "from": 10, "fromPin": "v", "to": 11, "toPin": "lhs"},
			{"kind": "data", "from": 10, "fromPin": "v", "to": 11, "toPin": "rhs"},
			{"kind": "data", "from": 11, "fromPin": "result", "to": 12, "toPin": "w"}
		]}]
})json"

// A function that calls itself: fact(n) = n < 2 ? 1 : n * fact(n - 1). The library's, so its body
// takes fresh ids in the caller's graph.
constexpr StringView FactLibrary(R"json({"__meta": {"kind": "graph", "version": 2}, "name": "fact",
	"interface": {"inputs": [{"name": "n", "type": "int"}], "outputs": [{"name": "r", "type": "int"}],
		"execIn": true, "execOut": ["done"]},
	"nodes": [
		{"id": 1, "op": "fn.entry"},
		{"id": 2, "op": "compare.lessInt", "params": {"rhs": 2}},
		{"id": 3, "op": "flow.branch"},
		{"id": 4, "op": "fn.return", "params": {"r": 1}},
		{"id": 5, "op": "math.subInt", "params": {"rhs": 1}},
		{"id": 6, "op": "fn.fact"},
		{"id": 7, "op": "math.mulInt"},
		{"id": 8, "op": "fn.return"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "start", "to": 3},
		{"kind": "data", "from": 1, "fromPin": "n", "to": 2, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "condition"},
		{"kind": "exec", "from": 3, "fromPin": "true", "to": 4},
		{"kind": "exec", "from": 3, "fromPin": "false", "to": 6},
		{"kind": "data", "from": 1, "fromPin": "n", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "n"},
		{"kind": "exec", "from": 6, "fromPin": "done", "to": 8},
		{"kind": "data", "from": 1, "fromPin": "n", "to": 7, "toPin": "lhs"},
		{"kind": "data", "from": 6, "fromPin": "r", "to": 7, "toPin": "rhs"},
		{"kind": "data", "from": 7, "fromPin": "result", "to": 8, "toPin": "r"}
	]
})json");

constexpr StringView FactMain(R"json({"__meta": {"kind": "graph", "version": 1}, "name": "main",
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "fn.fact", "params": {"n": 5}},
		{"id": 3, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "done", "to": 3}
	]
})json");

// An exec-only function with two exits and a return for each: sign(v) leaves by `pos` or `neg`.
#define SIGN(MODE, VALUE) R"json({"__meta": {"kind": "graph", "version": 2}, "name": "exits",
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "fn.sign", "params": {"v": )json" VALUE R"json(}, "settings": {"mode": ")json" MODE R"json("}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "pos", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "neg", "to": 4}
	],
	"functions": [{"name": "sign",
		"interface": {"inputs": [{"name": "v", "type": "int"}], "execIn": true,
			"execOut": ["pos", "neg"]},
		"nodes": [
			{"id": 10, "op": "fn.entry"},
			{"id": 11, "op": "compare.lessInt", "params": {"rhs": 0}},
			{"id": 12, "op": "flow.branch"},
			{"id": 13, "op": "fn.return", "settings": {"exit": "neg"}},
			{"id": 14, "op": "fn.return", "settings": {"exit": "pos"}}
		],
		"edges": [
			{"kind": "exec", "from": 10, "fromPin": "start", "to": 12},
			{"kind": "data", "from": 10, "fromPin": "v", "to": 11, "toPin": "lhs"},
			{"kind": "data", "from": 11, "fromPin": "result", "to": 12, "toPin": "condition"},
			{"kind": "exec", "from": 12, "fromPin": "true", "to": 13},
			{"kind": "exec", "from": 12, "fromPin": "false", "to": 14}
		]}]
})json"

// A loop whose body calls a function once per turn: every turn is a call of its own.
constexpr StringView LoopCall(R"json({"__meta": {"kind": "graph", "version": 2}, "name": "loop",
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [1, 2, 3]}},
		{"id": 3, "op": "fn.square"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 4},
		{"kind": "data", "from": 2, "fromPin": "item", "to": 3, "toPin": "x"}
	],
	"functions": [{"name": "square",
		"interface": {"inputs": [{"name": "x", "type": "int"}],
			"outputs": [{"name": "y", "type": "int"}], "execIn": true, "execOut": ["done"]},
		"nodes": [
			{"id": 10, "op": "fn.entry"},
			{"id": 11, "op": "math.mulInt"},
			{"id": 12, "op": "fn.return"}
		],
		"edges": [
			{"kind": "exec", "from": 10, "fromPin": "start", "to": 12},
			{"kind": "data", "from": 10, "fromPin": "x", "to": 11, "toPin": "lhs"},
			{"kind": "data", "from": 10, "fromPin": "x", "to": 11, "toPin": "rhs"},
			{"kind": "data", "from": 11, "fromPin": "result", "to": 12, "toPin": "y"}
		]}]
})json");

// A function whose return waits on a value nobody produces: the branch that would have is not taken.
constexpr StringView StuckCall(R"json({"__meta": {"kind": "graph", "version": 2}, "name": "stuck",
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "fn.stuck"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2}
	],
	"functions": [{"name": "stuck",
		"interface": {"outputs": [{"name": "y", "type": "float"}], "execIn": true,
			"execOut": ["done"]},
		"nodes": [
			{"id": 10, "op": "fn.entry"},
			{"id": 11, "op": "flow.branch", "params": {"condition": false}},
			{"id": 12, "op": "test.split", "params": {"value": 1.0}},
			{"id": 13, "op": "fn.return"}
		],
		"edges": [
			{"kind": "exec", "from": 10, "fromPin": "start", "to": 11},
			{"kind": "exec", "from": 11, "fromPin": "true", "to": 12},
			{"kind": "exec", "from": 11, "fromPin": "false", "to": 13},
			{"kind": "data", "from": 12, "fromPin": "value", "to": 13, "toPin": "y"}
		]}]
})json");

// What a document's own nodes ended with, as text: whether each ran and what it left in its
// outputs. A call node exists only when the site is called, so it is left out.
mem_std::String documentOutcome(Fixture &fx) {
	mem_std::String out;
	for (auto &n : fx.asset.getNodes()) {
		if (n.op.starts_with(GraphAsset::FunctionOpPrefix)) {
			continue;
		}
		auto index = fx.graph.findNode(n.id);
		if (index == InvalidIndex) {
			out.append(mem_std::toString(n.id, ":missing "));
			continue;
		}
		out.append(mem_std::toString(n.id, fx.ran(n.id) ? ":ran" : ":idle"));
		auto &rt = fx.graph.getNodeAt(index);
		for (uint32_t pin = 0; rt.op && pin < rt.op->getDataOut().size(); ++pin) {
			flow::value::Var v;
			if (fx.output(n.id, pin, v)) {
				out.append(mem_std::toString("=", v.i, "/", v.v[0]));
			}
		}
		out.append(" ");
	}
	return out;
}

bool sameInBothModes(StringView callJson, StringView inlineJson, StringView label) {
	Fixture called;
	Fixture inlined;
	if (!called.prepare(callJson) || !inlined.prepare(inlineJson)) {
		return false;
	}
	called.run();
	inlined.run();
	auto a = documentOutcome(called);
	auto b = documentOutcome(inlined);
	if (called.report.outcome != inlined.report.outcome || a != b) {
		sprt::cout << "       " << label << ": call  " << a << "\n       " << label
				   << ": inline " << b << "\n";
		return false;
	}
	return true;
}

int64_t outputInt(Fixture &fx, NodeId id, uint32_t pin, uint32_t activation = RootActivation) {
	flow::value::Var v;
	if (!fx.outputAt(id, pin, activation, v)) {
		return -1;
	}
	return v.i;
}

} // namespace

void performInterpCallTests() {
	sprt::cout << "\n== flow interp: function calls ==\n";

	{
		Fixture fx;
		check(fx.prepare(StringView(ADD_TEN("call"))), "interp-call: a local function builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: the call runs to completion");
		checkEq(StringView(fx.report.getTrace()), StringView("1 2 10 11 12 2 4 3"),
				"interp-call: the call, its body, the call's second turn, then what follows");
		check(outputInt(fx, 2, 0) == 15, "interp-call: the return wrote the call's output");
		check(outputInt(fx, 4, 0) == 16, "interp-call: and the node after the call read it");
		check(fx.ran(3), "interp-call: the call left through the return's exit");
	}

	{
		Fixture fx;
		check(fx.prepare(StringView(ADD_TEN("inline"))), "interp-call: an inline site builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: the substituted body runs to completion");
		checkEq(StringView(fx.report.getTrace()), StringView("1 13 14 15 4 3"),
				"interp-call: an inline site runs a copy of the body under fresh ids");
		check(outputInt(fx, 4, 0) == 16, "interp-call: inline computes what the call does");
		check(fx.ran(3), "interp-call: and leaves through the same exit");
	}

	{
		Fixture fx;
		check(fx.prepare(StringView(TWICE("call"))), "interp-call: a pure function builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: a pure call runs to completion");
		checkEq(StringView(fx.report.getTrace()), StringView("1 10 11 12 1 2"),
				"interp-call: a pure call is computed when its inputs are, in two turns");
		check(outputInt(fx, 2, 0) == 42, "interp-call: a pure call computes its value");
	}

	{
		Fixture fx;
		check(fx.prepare(StringView(TWICE("inline"))), "interp-call: a pure inline site builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: the substituted pure body runs");
		check(outputInt(fx, 2, 0) == 42, "interp-call: inline computes the same value");
	}

	{
		LibraryHost lib;
		check(lib.add(FactLibrary), "interp-call: the recursive library function loads");
		Fixture fx;
		fx.functions = &lib;
		check(fx.prepare(FactMain), "interp-call: a call of a recursive library function builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: the recursion runs to completion");
		check(outputInt(fx, 2, 0) == 120, "interp-call: fact(5) is 120");
		check(fx.ran(3), "interp-call: and the caller continues after it");

		// Five calls deep, each its own activation under its caller's, with the depth kept as the
		// activation's turn number.
		uint32_t calls = 0;
		uint32_t deepest = 0;
		auto &local = fx.interp.getLocal();
		for (uint32_t a = 0; a < local.getActivationCount(); ++a) {
			auto data = local.readActivation(a);
			if (fx.graph.getScopeAt(data.scope).kind == ScopeKind::Function) {
				++calls;
				deepest = sprt::max(deepest, data.iteration);
			}
		}
		check(calls == 5 && deepest == 5, "interp-call: five nested calls, the deepest at depth 5");
		check(fx.graph.getLink()->getOrigin(fx.report.log[2].id)
						&& StringView(fx.graph.getLink()->getOrigin(fx.report.log[2].id)->document)
								== "fact",
				"interp-call: a step of the library body is traced to the library document");
	}

	{
		LibraryHost lib;
		lib.add(FactLibrary);
		Fixture fx;
		fx.functions = &lib;
		check(fx.prepare(FactMain), "interp-call: the depth-limit graph builds");
		RunConfig config;
		config.maxCallDepth = 3;
		check(fx.run(config) != Status::Ok && fx.report.outcome == RunOutcome::ActivationLimit
						&& stappler::test::hasDiag(fx.report.diagnostics, StringView("call-depth")),
				"interp-call: calls nested past the limit end the run with call-depth");
	}

	{
		// The document of a function, run as a graph of its own: the entry hands out its own
		// parameters, and the returns have nobody to return to.
		LibraryHost lib;
		lib.add(FactLibrary);
		auto json = mem_std::toString(FactLibrary);
		StringView entry(R"json({"id": 1, "op": "fn.entry"})json");
		json.replace(json.find(entry.data(), 0, entry.size()), entry.size(),
				R"json({"id": 1, "op": "fn.entry", "params": {"n": 4}})json");
		Fixture fx;
		fx.functions = &lib;
		check(fx.prepare(StringView(json)), "interp-call: a function's own document builds as a root");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: and runs from its entry");
		check(outputInt(fx, 6, 0) == 6, "interp-call: its own call of itself computes fact(3)");
	}

	for (auto value : {StringView("5"), StringView("-5")}) {
		for (auto mode : {StringView("call"), StringView("inline")}) {
			auto json = mem_std::toString(SIGN("$MODE", "$VALUE"));
			json.replace(json.find("$MODE"), 5, mode.str<memory::StandardInterface>());
			json.replace(json.find("$VALUE"), 6, value.str<memory::StandardInterface>());
			Fixture fx;
			auto name = mem_std::toString("interp-call: sign(", value, ") in ", mode);
			check(fx.prepare(StringView(json)) && fx.run() == Status::Ok
							&& fx.report.outcome == RunOutcome::Completed,
					mem_std::toString(name, " runs"));
			auto positive = value == StringView("5");
			check(fx.ran(3) == positive && fx.ran(4) == !positive,
					mem_std::toString(name, " leaves through the exit its return named"));
		}
	}

	// ---- the two modes agree ------------------------------------------------------------------

	check(sameInBothModes(StringView(ADD_TEN("call")), StringView(ADD_TEN("inline")),
				  StringView("addTen")),
			"interp-call: an exec function computes the same in both modes");
	check(sameInBothModes(StringView(TWICE("call")), StringView(TWICE("inline")),
				  StringView("twice")),
			"interp-call: a pure function computes the same in both modes");
	check(sameInBothModes(StringView(SIGN("call", "5")), StringView(SIGN("inline", "5")),
				  StringView("sign+")),
			"interp-call: a function with two exits leaves the same way in both modes");
	check(sameInBothModes(StringView(SIGN("call", "-5")), StringView(SIGN("inline", "-5")),
				  StringView("sign-")),
			"interp-call: by the other exit as well");
	{
		auto inlined = mem_std::toString(LoopCall);
		StringView call(R"json({"id": 3, "op": "fn.square"})json");
		inlined.replace(inlined.find(call.data(), 0, call.size()), call.size(),
				R"json({"id": 3, "op": "fn.square", "settings": {"mode": "inline"}})json");
		check(sameInBothModes(LoopCall, StringView(inlined), StringView("loop")),
				"interp-call: a call in a loop body agrees with its substituted copy");
	}

	{
		Fixture fx;
		check(fx.prepare(LoopCall), "interp-call: a call in a loop body builds");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-call: the loop of calls completes");
		check(outputInt(fx, 3, 0, 1) == 1 && outputInt(fx, 3, 0, 3) == 4
						&& outputInt(fx, 3, 0, 5) == 9,
				"interp-call: every turn's call left its own value in that turn's record");
		check(fx.ran(4), "interp-call: and the loop completes after the last one");
	}

	{
		Fixture fx;
		check(fx.prepare(StuckCall), "interp-call: the stuck function builds");
		fx.run();
		check(fx.report.outcome == RunOutcome::Deadlock
						&& stappler::test::hasDiag(fx.report.diagnostics, StringView("deadlock")),
				"interp-call: a return waiting forever is a deadlock of the call");
	}

	{
		Fixture fx;
		check(fx.prepare(StringView(ADD_TEN("call"))), "interp-call: the breakpoint graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, RunConfig(), fx.report) == Status::Ok,
				"interp-call: the breakpoint run begins");
		Breakpoint bp;
		bp.node = 11;
		auto id = fx.interp.addBreakpoint(bp);
		fx.interp.finish(fx.report);
		check(fx.interp.getLastBreakpoint() == id && fx.interp.getState() == RunState::Paused,
				"interp-call: a breakpoint in a function's body stops the run");
		auto next = fx.interp.peekNext();
		check(recordKeyNode(next) == fx.graph.findNode(11)
						&& fx.graph.getScopeAt(fx.interp.getLocal()
														.readActivation(recordKeyActivation(next))
														.scope)
										.kind
								== ScopeKind::Function,
				"interp-call: inside the call's own activation");
		fx.interp.finish(fx.report);
		check(fx.report.outcome == RunOutcome::Completed && outputInt(fx, 4, 0) == 16,
				"interp-call: and continuing finishes the run with the same values");
	}

	{
		LibraryHost lib;
		lib.add(FactLibrary);
		Fixture fx;
		fx.functions = &lib;
		check(fx.prepare(FactMain) && fx.prepareJournal(),
				"interp-call: the step-back graph builds");
		check(fx.interp.begin(fx.graph, fx.ops, fx.arena, fx.journalConfig(), fx.report)
						== Status::Ok,
				"interp-call: the step-back run begins");
		mem_std::Vector<mem_std::Vector<uint8_t>> images;
		images.emplace_back(takeImage(fx.arena));
		while (fx.interp.stepOnce(fx.report)) { images.emplace_back(takeImage(fx.arena)); }
		check(fx.report.outcome == RunOutcome::Completed && outputInt(fx, 2, 0) == 120,
				"interp-call: the stepped run computes fact(5)");

		bool matched = true;
		auto units = fx.report.log.size();
		for (size_t i = units; i > 0; --i) {
			if (fx.interp.stepBack(fx.report) != Status::Ok) {
				matched = false;
				break;
			}
			auto image = takeImage(fx.arena);
			if (image != images[i - 1]) {
				matched = false;
				break;
			}
		}
		check(matched, "interp-call: every unit of a recursion steps back to the store it began with");
	}
}

} // namespace stappler
