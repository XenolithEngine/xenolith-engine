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

// The kernel on its own: a graph is read, built, run by the interpreter over both of
// its stores, written out by the generator and run again from the unit, in the kernel's own
// environment (flow::NoEnv), and every refusal is checked as the numbers it is reported in.

#include "SPCommon.h"
#include "SPData.h"
#include "SPFilesystem.h"

#include "SPFlowInterp.h"
#include "SPFlowCompiled.h"
#include "SPFlowOps.h"
#include "SPFlowCodegen.h"

#include "../tests.h"

// Absent only before the first `--emit`, and then `flow-codegen` says so.
#if __has_include("../gen/flow_arith.gen.h")
#include "../gen/flow_arith.gen.h"
#define SP_FLOW_TEST_UNIT 1
#endif

#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::test::flowfx {

using stappler::test::check;

using flow::DiagCode;
using flow::DiagDetail;
using flow::DiagLocus;
using flow::NodeId;
using flow::RunOutcome;
using flow::value::Diag;
using flow::value::Var;
using flow::value::VarType;

// `3 * 2 + 1`, a string carried across an edge and copied into a new one, and a branch on the
// result: the dataflow, a container and the exec side, and nothing that reaches a scene.
constexpr StringView ArithJson(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.float", "params": {"value": 3.0}},
		{"id": 2, "op": "math.mulFloat", "params": {"rhs": 2.0}},
		{"id": 3, "op": "math.addFloat", "params": {"rhs": 1.0}},
		{"id": 4, "op": "value.string", "params": {"value": "hi"}},
		{"id": 5, "op": "string.concat", "params": {"rhs": "!"}},
		{"id": 6, "op": "flow.event"},
		{"id": 7, "op": "compare.lessFloat", "params": {"rhs": 10.0}},
		{"id": 8, "op": "flow.branch"},
		{"id": 9, "op": "debug.trace", "params": {"value": 1.0}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "lhs"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 7, "toPin": "lhs"},
		{"kind": "data", "from": 7, "fromPin": "result", "to": 8, "toPin": "condition"},
		{"kind": "exec", "from": 6, "fromPin": "then", "to": 8},
		{"kind": "exec", "from": 8, "fromPin": "true", "to": 9}
	]})json");

// A node naming an operation no registry has, and an edge into a pin that does not exist.
constexpr StringView BadJson(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.float", "params": {"value": 3.0}},
		{"id": 2, "op": "nope.op"},
		{"id": 3, "op": "math.addFloat", "params": {"rhs": 1.0}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 3, "toPin": "nope"}
	]})json");

// An operation of the scene family, in a run that has none.
constexpr StringView SceneJson(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "scene.global"}
	],
	"edges": []})json");

// What a kernel caller holds: the entries as numbers. The names an entry borrows are not kept.
struct Collect final : flow::value::DiagSink {
	mem_std::Vector<Diag> entries;
	mem_std::Vector<mem_std::String> firstNames;

	void add(const Diag &d) override {
		entries.emplace_back(d);
		firstNames.emplace_back(d.argCount > 0 && d.args[0].kind == flow::value::DiagArgKind::Name
						? d.args[0].name.str<mem_std::Interface>()
						: mem_std::String());
		entries.back().inner = nullptr;
		for (auto &a : entries.back().args) { a.name = StringView(); }
		entries.back().locusName[0] = entries.back().locusName[1] = StringView();
	}

	const Diag *find(DiagCode code, size_t *index = nullptr) const {
		for (size_t i = 0; i < entries.size(); ++i) {
			if (entries[i].domain == flow::value::DiagDomain::Graph
					&& entries[i].code == uint16_t(code)) {
				if (index) {
					*index = i;
				}
				return &entries[i];
			}
		}
		return nullptr;
	}
};

struct Kernel {
	flow::OpRegistry ops;
	flow::GraphAsset asset;
	flow::RuntimeGraph graph;

	bool prepare(StringView json, flow::value::DiagSink *sink = nullptr) {
		if (!ops.init() || flow::ops::registerCoreOps(ops) != Status::Ok
				|| flow::InterpreterT<flow::value::PlainArena>::registerCoreTypes(ops.getLocalTypes())
						!= Status::Ok) {
			return false;
		}
		asset.init();
		graph.init();
		return asset.load(data::read<mem_std::Interface>(json), sink) == Status::Ok
				&& graph.build(asset, ops, sink) == Status::Ok;
	}
};

// The value of node `id`'s output `pin` in a run's records, through the read half of its store.
template <typename Graph, typename View>
static bool outputOf(const Graph &g, const View &view, NodeId id, uint32_t pin, Var &out) {
	auto index = g.findNode(id);
	if (index == flow::InvalidIndex) {
		return false;
	}
	auto &rt = g.getNodeAt(index);
	auto record = view.getRecord(index, flow::RootActivation);
	if (!rt.localSchema || record == flow::value::NullAddr || pin >= rt.localSchema->getFields().size()) {
		return false;
	}
	auto arena = view.getArenaRef();
	return rt.localSchema->getField(arena, record, rt.localSchema->getFields()[pin], out)
			== Status::Ok;
}

template <typename Graph, typename View>
static mem_std::String stringOf(const Graph &g, const View &view, NodeId id) {
	auto index = g.findNode(id);
	auto &rt = g.getNodeAt(index);
	auto record = view.getRecord(index, flow::RootActivation);
	if (!rt.localSchema || record == flow::value::NullAddr) {
		return mem_std::String();
	}
	return flow::value::blob::stringGet(view.getArenaRef(),
			record + rt.localSchema->getFields()[0].offset);
}

static void performBuild() {
	Kernel ok;
	check(ok.prepare(ArithJson),
			"flow-build: a graph of the standard library builds with no scene");

	Kernel bad;
	Collect said;
	check(!bad.prepare(BadJson, &said),
			"flow-build: a graph naming what does not exist is refused");

	size_t at = 0;
	auto unknown = said.find(DiagCode::UnknownOp, &at);
	check(unknown && unknown->detail == uint16_t(DiagDetail::UnknownOp)
					&& unknown->locus == uint16_t(DiagLocus::Node) && unknown->locusValue[0] == 2
					&& unknown->severity == flow::value::DiagSeverity::Error,
			"flow-build: ... the operation, as a code, a sentence and a node, all numbers");
	check(unknown && said.firstNames[at] == "nope.op",
			"flow-build: ... and the name it could not find, borrowed from the file");
	auto pin = said.find(DiagCode::UnknownPin);
	check(pin && pin->locus == uint16_t(DiagLocus::Edge) && pin->locusValue[0] == 1
					&& pin->locusValue[1] == 3,
			"flow-build: ... the edge into a pin that is not there, by its two nodes");
}

template <typename A>
static void runInterpreter(StringView kind) {
	Kernel k;
	if (!k.prepare(ArithJson)) {
		check(false, "flow-run: prepares");
		return;
	}

	A arena;
	arena.init();
	flow::InterpreterT<A> interp;
	flow::RunReport report;
	auto st = interp.run(k.graph, k.ops, arena, flow::RunConfigT<A>(), report);
	check(st == Status::Ok && report.outcome == RunOutcome::Completed,
			mem_std::toString("flow-run: the interpreter completes over ", kind));

	Var v;
	check(outputOf(k.graph, interp.getLocal(), 3, 0, v) && v.type == VarType::Float && v.f == 7.0,
			mem_std::toString("flow-run: ... and 3 * 2 + 1 is 7 (", kind, ")"));
	check(stringOf(k.graph, interp.getLocal(), 5) == "hi!",
			mem_std::toString("flow-run: ... the string crossed an edge and was copied (", kind,
					")"));
	check(report.getTrace().find("9") != mem_std::String::npos,
			mem_std::toString("flow-run: ... and the branch was taken (", kind, ")"));

	// The run is the arena's bytes: an image of it, adopted by another arena, opens as the same run.
	mem_std::Vector<uint8_t> image;
	image.reserve(arena.saveSize());
	arena.save([&](const uint8_t *data, size_t size, bool zero) {
		if (zero) {
			image.resize(image.size() + size, 0);
		} else {
			image.insert(image.end(), data, data + size);
		}
	});
	A elsewhere;
	flow::LocalStoreT<A> again;
	check(elsewhere.adopt(BytesView(image.data(), image.size())) == Status::Ok
					&& again.open(elsewhere, k.graph, k.ops.getLocalTypes()) == Status::Ok
					&& again.getStep() == interp.getLocal().getStep()
					&& again.verify() == Status::Ok,
			mem_std::toString("flow-run: ... and its image opens elsewhere as the same run (", kind,
					")"));
}

static void performRun() {
	runInterpreter<flow::value::PlainArena>("a plain arena");
	runInterpreter<flow::value::TrackedArena>("a tracked arena");
}

static void performScene() {
	Kernel k;
	if (!k.prepare(SceneJson)) {
		check(false, "flow-scene: prepares");
		return;
	}
	flow::value::PlainArena arena;
	arena.init();
	flow::InterpreterT<flow::value::PlainArena> interp;
	flow::RunReport report;
	interp.run(k.graph, k.ops, arena, flow::RunConfigT<flow::value::PlainArena>(), report);
	check(report.outcome == RunOutcome::OpError,
			"flow-scene: an operation that needs a scene refuses in a run that has none");

	auto &entries = report.diagnostics;
	auto found = false;
	if (entries.isArray()) {
		for (auto &e : entries.asArray()) {
			if (e.getInteger("domain") == int64_t(flow::value::DiagDomain::Graph)
					&& e.getInteger("code") == int64_t(DiagCode::OpError)
					&& e.getInteger("locus") == int64_t(DiagLocus::NodeOp)) {
				found = true;
			}
		}
	}
	check(found, "flow-scene: ... and the run's report says so in numbers");
}

static void performCodegen() {
	Kernel k;
	if (!k.prepare(ArithJson)) {
		check(false, "flow-codegen: prepares");
		return;
	}

	flow::codegen::EmitOptions options;
	options.name = StringView("flow_arith");
	static constexpr StringView bodies[] = {StringView("SPFlowOpsInline.h")};
	options.bodyIncludes = SpanView<StringView>(bodies, 1);

	flow::codegen::Emitted now;
	Collect said;
	auto st = flow::codegen::emit(k.graph, k.asset, k.ops, options, now, &said);
	check(st == Status::Ok,
			"flow-codegen: the generator writes the graph for the kernel's environment");

	if (!test::s_emitDir.empty()) {
		filesystem::write(FileInfo(mem_std::toString(test::s_emitDir, "/flow_arith.gen.h")),
				BytesView(reinterpret_cast<const uint8_t *>(now.header.data()), now.header.size()));
		for (size_t i = 0; i < now.sources.size(); ++i) {
			auto name = i == 0 ? mem_std::String("flow_arith.gen.cpp")
							   : mem_std::toString("flow_arith.gen.", i, ".cpp");
			filesystem::write(FileInfo(mem_std::toString(test::s_emitDir, "/", name)),
					BytesView(reinterpret_cast<const uint8_t *>(now.sources[i].data()),
							now.sources[i].size()));
		}
		sprt::cout << "       wrote the unit into " << test::s_emitDir
				   << " - rebuild before checking it\n";
	}

#if SP_FLOW_TEST_UNIT
	check(now.textHash == flow::gen::flow_arith::Tables::identity.textHash,
			"flow-codegen: tests/flow/gen holds the unit the generator writes today");

	flow::gen::flow_arith::Graph unit;
	check(unit.load(k.ops) == Status::Ok && unit.getSteps<flow::NoEnv>() != nullptr,
			"flow-codegen: the unit loads, with steps compiled for the kernel's environment");

	// The arenas outlive the engines: an engine gives its run back to the arena it ran in when it
	// is destroyed, so an arena that went first would be read after it was freed.
	flow::value::PlainArena arenaForArena;
	flow::value::PlainArena arenaForFast;
	auto runOne = [&](auto &engine, flow::value::PlainArena &arena, StringView name) {
		arena.init();
		flow::RunReport report;
		auto st = engine.run(k.ops, arena, flow::RunConfigT<flow::value::PlainArena>(), report);
		Var v;
		check(st == Status::Ok && report.outcome == RunOutcome::Completed
						&& outputOf(unit, engine.getLocal(), 3, 0, v) && v.f == 7.0
						&& stringOf(unit, engine.getLocal(), 5) == "hi!",
				mem_std::toString("flow-codegen: the ", name, " engine computes what the interpreter did"));
	};
	flow::gen::flow_arith::Engine<flow::value::PlainArena> arenaEngine(unit);
	runOne(arenaEngine, arenaForArena, "arena");
	flow::gen::flow_arith::FastEngine<flow::value::PlainArena> fastEngine(unit);
	runOne(fastEngine, arenaForFast, "fast");
#else
	check(false, "flow-codegen: tests/flow/gen holds the unit the generator writes today");
#endif
}

} // namespace stappler::test::flowfx

namespace STAPPLER_VERSIONIZED stappler {

void performFlowBuildTests() { test::flowfx::performBuild(); }
void performFlowRunTests() { test::flowfx::performRun(); }
void performFlowSceneTests() { test::flowfx::performScene(); }
void performFlowCodegenTests() { test::flowfx::performCodegen(); }

} // namespace stappler
