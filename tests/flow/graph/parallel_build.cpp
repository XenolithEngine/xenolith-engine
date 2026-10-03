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

// A parallel block at build time: the pair of fan-out and barrier, the edges out of a
// body, what a body may do, the region that may run while a block is in flight, the conflicts that
// region makes, the GPU segment and the enum range - and the firing contract the region relies on.
//
// Every refusal is asserted with its locus. The region is asserted through describeParallel on the
// graphs that build.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "graph_fixture.h"
#include "../interp/interp_fixture.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace flow;
using stappler::test::graphfx::diagLine;
namespace vstore = flow::value;
using flow::value::VarType;

// ---- graphs, spelled short ------------------------------------------------------------------------

struct G {
	mem_std::String nodes;
	mem_std::String edges;

	G &n(uint32_t id, StringView op, StringView extra = StringView()) {
		if (!nodes.empty()) {
			nodes.append(",\n");
		}
		nodes.append(mem_std::toString(R"({"id": )", id, R"(, "op": ")", op, "\""));
		if (!extra.empty()) {
			nodes.append(", ");
			nodes.append(extra.data(), extra.size());
		}
		nodes.append("}");
		return *this;
	}

	G &x(uint32_t from, StringView pin, uint32_t to) {
		sep();
		edges.append(mem_std::toString(R"({"kind": "exec", "from": )", from, R"(, "fromPin": ")", pin,
				R"(", "to": )", to, "}"));
		return *this;
	}

	G &d(uint32_t from, StringView pin, uint32_t to, StringView toPin) {
		sep();
		edges.append(mem_std::toString(R"({"kind": "data", "from": )", from, R"(, "fromPin": ")", pin,
				R"(", "to": )", to, R"(, "toPin": ")", toPin, "\"}"));
		return *this;
	}

	// A scene access: a `scene.*` node on Unit.<field>, its target the block's entity or the global
	// entity node.
	G &own(uint32_t id, StringView op, StringView field, uint32_t fanOut) {
		n(id, op, unit(field));
		return d(fanOut, "entity", id, "target");
	}
	G &foreign(uint32_t id, StringView op, StringView field, uint32_t global) {
		n(id, op, unit(field));
		return d(global, "entity", id, "target");
	}

	static mem_std::String unit(StringView field) {
		return mem_std::toString(R"("params": {"component": "Unit", "field": ")", field, "\"}");
	}

	mem_std::String json() const {
		return mem_std::toString(R"({"formatVersion": 1, "nodes": [)", nodes, R"(], "edges": [)", edges,
				"]}");
	}

private:
	void sep() {
		if (!edges.empty()) {
			edges.append(",\n");
		}
	}
};

constexpr StringView Query(R"("settings": {"with": ["Unit"]})");
constexpr StringView QueryGpu(R"("settings": {"with": ["Unit"], "executors": ["gpu"]})");

// ---- the fixture ---------------------------------------------------------------------------------------

Status peekInvoke(OpContext &) { return Status::Ok; }

Status fireBothInvoke(OpContext &ctx) {
	auto st = ctx.fire(uint32_t(0));
	return st != Status::Ok ? st : ctx.fire(uint32_t(1));
}

bool registerProbes(OpRegistry &ops) {
	// An eager reader of Unit.hp with nothing to wait for: an entry point, runnable at any moment.
	{
		PinDesc in[] = {test::graphfx::pin("component", VarType::String),
			test::graphfx::pin("field", VarType::String)};
		in[0].role = PinRole::ComponentName;
		in[1].role = PinRole::FieldName;
		in[0].def = mem_std::Value("Unit");
		in[1].def = mem_std::Value("hp");
		PinDesc out[] = {test::graphfx::pin("value", VarType::Int32)};
		out[0].role = PinRole::FieldValue;
		OpDef def;
		def.name = StringView("probe.peek");
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::ReadsScene;
		def.parallel = OpParallel::SceneRead;
		def.invoke = &peekInvoke;
		if (!ops.createNative(def)) {
			return false;
		}
	}
	// An enum with no family, from an Int32.
	{
		PinDesc in[] = {test::graphfx::pin("value", VarType::Int32)};
		PinDesc out[] = {test::graphfx::pin("result", VarType::Enum)};
		OpDef def;
		def.name = StringView("probe.anyEnum");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		def.invoke = &peekInvoke;
		if (!ops.createNative(def)) {
			return false;
		}
	}
	// An exclusive operation that breaks its word.
	{
		static constexpr StringView outs[] = {StringView("a"), StringView("b")};
		OpDef def;
		def.name = StringView("probe.fireBoth");
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(outs, 2);
		def.execExclusive = true;
		def.parallel = OpParallel::Flow;
		def.invoke = &fireBothInvoke;
		if (!ops.createNative(def)) {
			return false;
		}
	}
	return true;
}

bool registerScene(flow::value::TypeRegistry &reg) {
	flow::value::FieldDef unit[] = {
		flow::value::FieldDef{.name = StringView("hp"), .type = VarType::Int32},
		flow::value::FieldDef{.name = StringView("mp"), .type = VarType::Int32},
		flow::value::FieldDef{.name = StringView("speed"), .type = VarType::Float32},
		flow::value::FieldDef{.name = StringView("count"), .type = VarType::Int},
	};
	flow::value::EnumMemberDef dir[] = {{StringView("North"), 0}, {StringView("Far"), int64_t(1) << 40}};
	return reg.createNative(StringView("Unit"), SpanView<flow::value::FieldDef>(unit, 4)) != nullptr
			&& reg.createEnum(StringView("enum.Dir"), SpanView<flow::value::EnumMemberDef>(dir, 2))
			!= nullptr;
}

struct Fx {
	OpRegistry ops;
	flow::value::TypeRegistry scene;
	bool ready = false;

	Fx() {
		ready = ops.init() && flow::ops::registerCoreOps(ops) == Status::Ok && registerProbes(ops)
				&& scene.init() && registerScene(scene);
	}

	struct Result {
		mem_std::String errors;
		mem_std::Value parallel;
		bool built = false;
	};

	Result build(const G &g, bool bound = false) {
		Result out;
		GraphAsset asset;
		RuntimeGraph graph;
		mem_std::Value report;
		asset.init();
		graph.init();
		if (asset.load(data::read<mem_std::Interface>(g.json()), &report) != Status::Ok) {
			out.errors = "<asset>";
			return out;
		}
		auto st = bound ? graph.build(asset, ops, scene, &report) : graph.build(asset, ops, &report);
		out.built = st == Status::Ok;
		if (report.isArray()) {
			for (auto &it : report.asArray()) {
				if (test::getDiagSeverityName(it) != StringView("error")) {
					continue;
				}
				if (!out.errors.empty()) {
					out.errors.append(" ");
				}
				out.errors.append(diagLine(it));
			}
		}
		if (out.built) {
			graph.describeParallel(out.parallel);
		}
		return out;
	}

	// validate with a shape: what the graph editor calls.
	GraphShape shape(const G &g, mem_std::String *errors = nullptr) {
		GraphShape out;
		GraphAsset asset;
		mem_std::Value report;
		asset.init();
		if (asset.load(data::read<mem_std::Interface>(g.json()), &report) != Status::Ok) {
			return out;
		}
		RuntimeGraph::validate(asset, ops, &report, &out);
		if (errors && report.isArray()) {
			for (auto &it : report.asArray()) {
				if (test::getDiagSeverityName(it) == StringView("error")) {
					errors->append(diagLine(it));
				}
			}
		}
		return out;
	}
};

mem_std::String idList(const mem_std::Value &parallel, uint32_t block, StringView key) {
	mem_std::String out;
	for (auto &it : parallel.getValue(block).getValue(key).asArray()) {
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(mem_std::toString(it.getInteger()));
	}
	return out;
}

// The smallest block: ev(1) -> F(2), body writes its own Unit.hp (3), closes into B(4).
G minimal() {
	G g;
	g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
	g.own(3, "scene.setInt32", "hp", 2);
	g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
	return g;
}

} // namespace

void performParallelBuildTests() {
	sprt::cout << "\n== flow graph: parallel blocks at build time ==\n";

	Fx fx;
	check(fx.ready, "parallel-build: the library, the probes and the scene register");
	if (!fx.ready) {
		return;
	}

	// ---- the pair --------------------------------------------------------------------------------------

	{
		auto r = fx.build(minimal());
		check(r.built && r.errors.empty(), "parallel-build: the smallest block builds");
		checkEq(idList(r.parallel, 0, "body"), "3", "parallel-build: its body is the write");
		checkEq(idList(r.parallel, 0, "after"), "4", "parallel-build: and the barrier comes after");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query);
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3);
		checkEq(fx.build(g).errors, "parallel-unpaired@2", "parallel-build: a fan-out with no barrier");
	}
	{
		G g;
		g.n(1, "flow.event").n(4, "par.barrier");
		g.x(1, "then", 4);
		checkEq(fx.build(g).errors, "parallel-escape@1.then->4 parallel-unpaired@4",
				"parallel-build: a barrier entered from outside a body, closing nothing");
	}
	{
		G g;
		g.n(1, "flow.event").n(7, "flow.sequence").n(2, "par.forEachWith", Query).n(4, "par.barrier")
				.n(5, "par.forEachWith", Query);
		g.own(3, "scene.setInt32", "hp", 2);
		g.own(6, "scene.setInt32", "mp", 5);
		g.x(1, "then", 7).x(7, "first", 2).x(7, "second", 5);
		g.x(2, "body", 3).x(3, "then", 4).x(5, "body", 6).x(6, "then", 4);
		checkEq(fx.build(g).errors, "parallel-unpaired@4",
				"parallel-build: one barrier closing two blocks");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(3, "flow.sequence").n(4, "par.barrier")
				.n(5, "par.barrier");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "first", 4).x(3, "second", 5);
		checkEq(fx.build(g).errors, "parallel-unpaired@3.second->5",
				"parallel-build: one body closing into two barriers");
	}

	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith").n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		checkEq(fx.build(g).errors, "setting-invalid@2#with",
				"parallel-build: a query that names no component");
	}

	// ---- edges out of a body ------------------------------------------------------------------------------

	{
		G g = minimal();
		g.n(5, "scene.setInt32", G::unit("mp")).n(6, "scene.global");
		g.x(4, "completed", 5).d(6, "entity", 5, "target").d(2, "index", 5, "value");
		checkEq(fx.build(g).errors, "parallel-escape@2.index->5.value",
				"parallel-build: the fan-out's per-branch value read outside its body");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 4);
		g.n(5, "scene.getInt32", G::unit("mp")).n(6, "par.sumInt32").n(7, "scene.setInt32", G::unit("hp"))
				.n(8, "scene.global");
		g.d(2, "entity", 5, "target");
		g.d(6, "result", 7, "value").d(8, "entity", 7, "target").x(4, "completed", 7);
		g.d(5, "value", 6, "value");
		checkEq(fx.build(g).errors, "",
				"parallel-build: a collector is the one way a body's value reaches the parent");
	}
	{
		G g = minimal();
		g.n(5, "value.int32").n(6, "par.sumInt32");
		g.d(5, "value", 6, "value");
		checkEq(fx.build(g).errors, "parallel-unpaired@5.value->6.value",
				"parallel-build: a collector fed from outside any block");
	}
	{
		G g = minimal();
		g.n(6, "par.sumInt32");
		checkEq(fx.build(g).errors, "parallel-unpaired@6.value",
				"parallel-build: a collector fed by nothing - no literal stands for the branches");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 2);
		checkEq(fx.build(g).errors, "parallel-escape@3.then->2 parallel-unpaired@2 parallel-unpaired@4",
				"parallel-build: a body handing control back to its fan-out");
	}

	// ---- what a body may hold ------------------------------------------------------------------------------

	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5);
		g.n(5, "par.forEachWith", Query).n(7, "par.barrier");
		g.own(6, "scene.setInt32", "mp", 5);
		g.x(5, "body", 6).x(6, "then", 7).x(7, "completed", 4);
		auto r = fx.build(g);
		check(r.errors.find("parallel-nested@5") != mem_std::String::npos,
				mem_std::toString("parallel-build: a block inside a body (", r.errors, ")"));
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
		g.n(3, "scene.addComponent", R"("params": {"component": "Unit"})");
		g.d(2, "entity", 3, "target").x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		checkEq(fx.build(g).errors, "parallel-serial-op@3", "parallel-build: a serial operation in a body");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier").n(9, "scene.global");
		g.foreign(3, "scene.setInt32", "hp", 9);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		checkEq(fx.build(g).errors, "parallel-foreign-write@3",
				"parallel-build: a body writing an entity that is not its own");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier")
				.n(5, "string.concat", R"("params": {"lhs": "Un", "rhs": "it"})");
		g.n(3, "scene.setInt32", R"("params": {"field": "hp"})");
		g.d(2, "entity", 3, "target").d(5, "result", 3, "component");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		checkEq(fx.build(g).errors, "parallel-dynamic-scene@3",
				"parallel-build: a scene name on an edge in a body");
	}
	{
		G h;
		h.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier").n(9, "scene.global");
		h.own(3, "scene.setInt32", "hp", 2);
		h.foreign(5, "scene.getInt32", "hp", 9);
		h.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 4);
		checkEq(fx.build(h).errors, "parallel-conflict@5",
				"parallel-build: one branch reads on another entity the field every branch writes");

		G k;
		k.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier");
		k.own(3, "scene.setInt32", "hp", 2);
		k.n(5, "scene.findByField", R"("params": {"component": "Unit", "field": "hp", "value": 3})");
		k.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 4);
		checkEq(fx.build(k).errors, "parallel-conflict@5",
				"parallel-build: ... and so does a search by that field");
	}

	// ---- the region: graphs that build ---------------------------------------------------------------------

	{
		// P1: a straight line, the global entity read before and written after.
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier").n(9, "scene.global");
		g.foreign(5, "scene.getInt32", "hp", 9);
		g.own(3, "scene.setInt32", "hp", 2);
		g.foreign(6, "scene.setInt32", "hp", 9);
		g.x(1, "then", 5).x(5, "then", 2).x(2, "body", 3).x(3, "then", 4).x(4, "completed", 6);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(), "parallel-build: a line through a block builds");
		checkEq(idList(r.parallel, 0, "concurrent"), "", "parallel-build: nothing runs beside it");
		checkEq(idList(r.parallel, 0, "after"), "4 6", "parallel-build: and the write follows it");
	}
	{
		// P2: two independent blocks in a sequence.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(10, "par.forEachWith", Query).n(12, "par.barrier")
				.n(20, "par.forEachWith", Query).n(22, "par.barrier");
		g.own(11, "scene.setInt32", "hp", 10);
		g.own(21, "scene.setFloat32", "speed", 20);
		g.x(1, "then", 2).x(2, "first", 10).x(2, "second", 20);
		g.x(10, "body", 11).x(11, "then", 12).x(20, "body", 21).x(21, "then", 22);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(), "parallel-build: two independent blocks in a sequence build");
		checkEq(idList(r.parallel, 0, "concurrent"), "20 21 22",
				"parallel-build: the second block is in the first one's region");
		checkEq(idList(r.parallel, 1, "concurrent"), "10 11 12",
				"parallel-build: and the first, still in flight, in the second's");
	}
	{
		// P3: the block on a branch's `true`, a conflicting write on `false`.
		G g;
		g.n(1, "flow.event").n(2, "flow.branch", R"("params": {"condition": true})")
				.n(3, "par.forEachWith", Query).n(5, "par.barrier").n(9, "scene.global");
		g.own(4, "scene.setInt32", "hp", 3);
		g.foreign(6, "scene.setInt32", "hp", 9);
		g.x(1, "then", 2).x(2, "true", 3).x(2, "false", 6).x(3, "body", 4).x(4, "then", 5);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: the untaken arm of a branch runs beside nothing (", r.errors, ")"));
	}
	{
		// P4: both arms merge into the fan-out - one token either way.
		G g;
		g.n(1, "flow.event").n(2, "flow.branch", R"("params": {"condition": true})")
				.n(3, "debug.trace").n(4, "debug.trace").n(5, "par.forEachWith", Query).n(7, "par.barrier");
		g.own(6, "scene.setInt32", "hp", 5);
		g.x(1, "then", 2).x(2, "true", 3).x(2, "false", 4).x(3, "then", 5).x(4, "then", 5);
		g.x(5, "body", 6).x(6, "then", 7);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: a branch merging into a fan-out is one token (", r.errors, ")"));
	}
	{
		// P5: a block in a loop's body, the field read after the loop.
		G g;
		g.n(1, "flow.event").n(2, "flow.forEach", R"("params": {"items": [1, 2]})")
				.n(3, "par.forEachWith", Query).n(5, "par.barrier").n(9, "scene.global");
		g.own(4, "scene.setInt32", "hp", 3);
		g.foreign(6, "scene.getInt32", "hp", 9);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "body", 4).x(4, "then", 5).x(2, "completed", 6);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: a block in a loop, read after the loop (", r.errors, ")"));
	}
	{
		// P6: a loop inside the body.
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", Query)
				.n(3, "flow.forEach", R"("params": {"items": [1, 2]})").n(5, "par.barrier");
		g.own(4, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "body", 4).x(3, "completed", 5);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: a loop inside a body (", r.errors, ")"));
		checkEq(idList(r.parallel, 0, "body"), "3 4", "parallel-build: ... belongs to the body");
	}
	{
		// P7: reading whether an entity has a component beside a field write, and changing a pool the
		// block does not touch.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(3, "par.forEachWith", Query).n(5, "par.barrier")
				.n(9, "scene.global");
		g.own(4, "scene.setInt32", "hp", 3);
		g.n(6, "scene.has", R"("params": {"component": "Unit"})").d(9, "entity", 6, "target");
		g.n(7, "scene.addComponent", R"("params": {"component": "Other"})").d(9, "entity", 7, "target");
		g.x(1, "then", 2).x(2, "first", 3).x(2, "second", 6).x(6, "then", 7);
		g.x(3, "body", 4).x(4, "then", 5);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: membership reads and other pools do not conflict (", r.errors, ")"));
		checkEq(idList(r.parallel, 0, "concurrent"), "6 7 9",
				"parallel-build: though both are concurrent, and so is the global entity, an entry point");
	}
	{
		// P8: a fan-out over a pin.
		G g;
		g.n(1, "flow.event").n(2, "par.forEach", R"("params": {"entities": []})").n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(), "parallel-build: a fan-out over a pin builds");
	}

	// ---- the region: refusals ---------------------------------------------------------------------------------

	auto sequenced = [](StringView secondOp, StringView field) {
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(3, "par.forEachWith", Query).n(5, "par.barrier")
				.n(9, "scene.global");
		g.own(4, "scene.setInt32", "hp", 3);
		g.foreign(6, secondOp, field, 9);
		g.x(1, "then", 2).x(2, "first", 3).x(2, "second", 6).x(3, "body", 4).x(4, "then", 5);
		return g;
	};
	checkEq(fx.build(sequenced("scene.setInt32", "hp")).errors, "parallel-conflict@6",
			"parallel-build: the block on `first`, a write of its field on `second`");
	checkEq(fx.build(sequenced("scene.getInt32", "hp")).errors, "parallel-conflict@6",
			"parallel-build: ... and a read of it");
	checkEq(fx.build(sequenced("scene.getInt32", "mp")).errors, "",
			"parallel-build: ... and not a read of another field");
	{
		G g = sequenced("scene.setInt32", "hp");
		auto unbound = fx.build(g).errors;
		auto bound = fx.build(g, true).errors;
		checkEq(bound, unbound, "parallel-build: a bound build finds the same conflicts");
	}
	{
		// Two blocks that conflict: the first writes hp, the second reads it.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(10, "par.forEachWith", Query).n(12, "par.barrier")
				.n(20, "par.forEachWith", Query).n(22, "par.barrier");
		g.own(11, "scene.setInt32", "hp", 10);
		g.own(21, "scene.getInt32", "hp", 20);
		g.x(1, "then", 2).x(2, "first", 10).x(2, "second", 20);
		g.x(10, "body", 11).x(11, "then", 12).x(20, "body", 21).x(21, "then", 22);
		checkEq(fx.build(g).errors, "parallel-conflict@21",
				"parallel-build: two blocks in flight together, one refusal for the pair");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(3, "par.forEachWith", Query).n(5, "par.barrier");
		g.own(4, "scene.setInt32", "hp", 3);
		g.x(1, "then", 2).x(2, "first", 3).x(2, "second", 3).x(3, "body", 4).x(4, "then", 5);
		checkEq(fx.build(g).errors, "parallel-reentry@2.second->3",
				"parallel-build: both outputs of a sequence into the fan-out");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(6, "debug.trace").n(3, "par.forEachWith", Query)
				.n(5, "par.barrier");
		g.own(4, "scene.setInt32", "hp", 3);
		g.x(1, "then", 2).x(2, "first", 6).x(2, "second", 6).x(6, "then", 3).x(3, "body", 4)
				.x(4, "then", 5);
		checkEq(fx.build(g).errors, "parallel-reentry@6.then->3",
				"parallel-build: a node dominating the fan-out reached twice");
	}
	{
		// A loop above the block keeps turning while the block is in flight.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(7, "flow.forEach", R"("params": {"items": [1, 2]})")
				.n(3, "par.forEachWith", Query).n(5, "par.barrier").n(9, "scene.global");
		g.own(4, "scene.setInt32", "hp", 3);
		g.foreign(8, "scene.setInt32", "hp", 9);
		g.x(1, "then", 2).x(2, "first", 7).x(7, "body", 8).x(2, "second", 3);
		g.x(3, "body", 4).x(4, "then", 5);
		checkEq(fx.build(g).errors, "parallel-conflict@8",
				"parallel-build: a loop on `first` still turning when the block on `second` starts");
	}
	{
		// A cycle back to the fan-out's predecessor: after the barrier, a write on `first` and the next
		// lap on `second` - the write drains before the lap begins.
		auto cycle = [](bool writeFirst) {
			G g;
			g.n(1, "flow.event").n(2, "debug.trace").n(3, "par.forEachWith", Query).n(5, "par.barrier")
					.n(6, "flow.sequence").n(9, "scene.global");
			g.own(4, "scene.setInt32", "hp", 3);
			g.foreign(7, "scene.setInt32", "hp", 9);
			g.x(1, "then", 2).x(2, "then", 3).x(3, "body", 4).x(4, "then", 5).x(5, "completed", 6);
			g.x(6, writeFirst ? "first" : "second", 7).x(6, writeFirst ? "second" : "first", 2);
			return g;
		};
		auto quiet = fx.build(cycle(true));
		check(quiet.built && quiet.errors.empty(),
				mem_std::toString("parallel-build: a cycle whose write drains before the next lap (", quiet.errors, ")"));
		checkEq(fx.build(cycle(false)).errors, "parallel-conflict@7",
				"parallel-build: a cycle whose write waits under the next lap");
	}
	for (uint32_t id : {uint32_t(6), uint32_t(99)}) {
		G g = minimal();
		g.n(id, "probe.peek");
		checkEq(fx.build(g).errors, mem_std::toString("parallel-conflict@", id),
				mem_std::toString("parallel-build: an eager reader may run in flight, whatever its id (", id, ")"));
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "debug.trace").n(3, "par.forEachWith", Query).n(5, "par.barrier")
				.n(6, "probe.peek");
		g.own(4, "scene.setInt32", "hp", 3);
		g.d(6, "value", 2, "value").x(1, "then", 2).x(2, "then", 3).x(3, "body", 4).x(4, "then", 5);
		checkEq(fx.build(g).errors, "",
				"parallel-build: ... unless the fan-out waits for its value");
	}
	{
		// A body input produced again while the block reads it.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(7, "flow.sequence").n(3, "par.forEachWith", Query)
				.n(5, "par.barrier").n(9, "scene.global");
		g.foreign(6, "scene.getInt32", "speed", 9);
		g.own(4, "scene.setFloat32", "speed", 3);
		g.n(8, "debug.trace");
		g.x(1, "then", 2).x(2, "first", 6).x(2, "second", 7).x(7, "first", 3).x(7, "second", 6);
		g.x(3, "body", 8).d(6, "value", 8, "value").x(8, "then", 4).x(4, "then", 5);
		auto r = fx.build(g);
		check(r.errors.find("parallel-conflict@6.value->8.value") != mem_std::String::npos,
				mem_std::toString("parallel-build: a body input produced again in flight (", r.errors, ")"));
	}
	{
		G g = sequenced("scene.getInt32", "mp");
		g.n(7, "scene.addComponent", R"("params": {"component": "Unit"})").d(9, "entity", 7, "target");
		g.x(6, "then", 7);
		checkEq(fx.build(g).errors, "parallel-conflict@7",
				"parallel-build: adding a component the block reads while it is in flight");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(3, "par.forEachWith", Query).n(5, "par.barrier")
				.n(9, "scene.global").n(7, "string.concat", R"("params": {"lhs": "Un", "rhs": "it"})");
		g.own(4, "scene.setInt32", "hp", 3);
		g.n(6, "scene.getInt32", R"("params": {"field": "hp"})").d(9, "entity", 6, "target")
				.d(7, "result", 6, "component");
		g.x(1, "then", 2).x(2, "first", 3).x(2, "second", 6).x(3, "body", 4).x(4, "then", 5);
		checkEq(fx.build(g).errors, "parallel-dynamic-scene@6",
				"parallel-build: a name on an edge beside a block that writes");
	}
	{
		// What was pending while an earlier block was in flight is pending for the next one too.
		G g;
		g.n(1, "flow.event").n(2, "flow.sequence").n(10, "par.forEachWith", Query).n(12, "par.barrier")
				.n(20, "par.forEachWith", Query).n(22, "par.barrier").n(9, "scene.global");
		g.own(11, "scene.setInt32", "mp", 10);
		g.own(21, "scene.getInt32", "hp", 20);
		g.foreign(6, "scene.setInt32", "hp", 9);
		g.x(1, "then", 2).x(2, "first", 10).x(2, "second", 6).x(12, "completed", 20);
		g.x(10, "body", 11).x(11, "then", 12).x(20, "body", 21).x(21, "then", 22);
		checkEq(fx.build(g).errors, "parallel-conflict@6",
				"parallel-build: a write pending since the first block, beside the second");
	}

	// ---- the GPU segment -------------------------------------------------------------------------------------

	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(5, "par.barrier");
		g.own(3, "scene.getInt", "count", 2);
		g.own(4, "scene.setInt", "count", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4).x(4, "then", 5).d(3, "value", 4, "value");
		checkEq(fx.build(g).errors,
				"parallel-gpu-type@3.value parallel-gpu-op@3 parallel-gpu-type@4.value parallel-gpu-op@4",
				"parallel-build: an Int carried through a GPU body");
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(7, "par.barrier");
		g.own(3, "scene.getInt", "count", 2);
		g.n(4, "convert.intToInt32").n(5, "math.addInt32", R"("params": {"rhs": 1})");
		g.own(6, "scene.setInt", "count", 2);
		g.d(3, "value", 4, "value").d(4, "result", 5, "lhs").d(5, "result", 6, "value");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 6).x(6, "then", 7);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: narrowed on the way in, widened on the way out (", r.errors, ")"));
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(5, "par.barrier")
				.n(3, "flow.forEach", R"("params": {"items": [1]})");
		g.own(4, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "body", 4).x(3, "completed", 5);
		auto r = fx.build(g);
		check(r.errors.find("parallel-gpu-shape@3") != mem_std::String::npos
						&& r.errors.find("parallel-gpu-op@3") != mem_std::String::npos,
				mem_std::toString("parallel-build: a loop in a GPU body (", r.errors, ")"));
	}
	{
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(4, "par.barrier");
		g.own(3, "scene.setInt32", "hp", 2);
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(), "parallel-build: a GPU body of GPU values builds");
	}
	{
		// A read through the global entity is a row for the whole block: it lowers.
		G g;
		g.n(1, "flow.event").n(9, "scene.global").n(2, "par.forEachWith", QueryGpu).n(6, "par.barrier");
		g.foreign(3, "scene.getInt32", "mp", 9);
		g.n(4, "math.addInt32", R"("params": {"rhs": 1})");
		g.own(5, "scene.setInt32", "hp", 2);
		g.d(3, "value", 4, "lhs").d(4, "result", 5, "value");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 6);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: a foreign read in a GPU body lowers (", r.errors, ")"));
	}
	{
		// An Enum field narrowed on the prefix: the column the loader guards.
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(6, "par.barrier");
		g.n(3, "scene.getEnum", R"("params": {"component": "Unit", "field": "dir", "family": "enum.Dir"})")
				.d(2, "entity", 3, "target");
		g.n(4, "enum.toInt32");
		g.own(5, "scene.setInt32", "hp", 2);
		g.d(3, "value", 4, "value").d(4, "result", 5, "value");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 6);
		auto r = fx.build(g);
		check(r.built && r.errors.empty(),
				mem_std::toString("parallel-build: an Enum field narrowed at the start of a GPU body (", r.errors, ")"));
	}
	{
		// The same narrowing on one arm of a branch: its guard could not fail the branch before dispatch.
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(8, "par.barrier");
		g.n(3, "compare.lessInt32", R"("params": {"rhs": 1})").n(4, "flow.branch");
		g.own(5, "scene.getInt", "count", 2);
		g.n(6, "convert.intToInt32");
		g.own(7, "scene.setInt32", "hp", 2);
		g.d(2, "index", 3, "lhs").d(3, "result", 4, "condition").d(5, "value", 6, "value").d(6, "result", 7, "value");
		g.x(1, "then", 2).x(2, "body", 4).x(4, "true", 5).x(5, "then", 7).x(7, "then", 8).x(4, "false", 8);
		checkEq(fx.build(g).errors, "parallel-gpu-guard@6",
				"parallel-build: a CPU value narrowed on one arm of a GPU branch");
	}
	{
		// A field outside the GPU set read after the branch wrote it: the loader cannot know the value.
		G g;
		g.n(1, "flow.event").n(2, "par.forEachWith", QueryGpu).n(7, "par.barrier");
		g.n(3, "scene.setInt", R"("params": {"component": "Unit", "field": "count", "value": 5})").d(2, "entity", 3, "target");
		g.own(4, "scene.getInt", "count", 2);
		g.n(5, "convert.intToInt32");
		g.own(6, "scene.setInt32", "hp", 2);
		g.d(4, "value", 5, "value").d(5, "result", 6, "value");
		g.x(1, "then", 2).x(2, "body", 3).x(3, "then", 4).x(4, "then", 6).x(6, "then", 7);
		checkEq(fx.build(g).errors, "parallel-gpu-guard@4",
				"parallel-build: a CPU value read back after the GPU body wrote it");
	}

	// ---- the enum range --------------------------------------------------------------------------------------

	{
		G g = minimal();
		g.n(6, "enum.fromInt32", R"("params": {"family": "enum.Dir"})").n(7, "enum.toInt32");
		g.d(2, "index", 6, "value").d(6, "result", 7, "value");
		check(fx.build(g).errors.empty(),
				"parallel-build: an enum narrowed in a body, unbound: nothing to check against");
		checkEq(fx.build(g, true).errors, "parallel-enum-range@7",
				"parallel-build: bound, its family has a member outside Int32");

		G h = minimal();
		h.n(6, "probe.anyEnum").n(7, "enum.toInt32");
		h.d(2, "index", 6, "value").d(6, "result", 7, "value");
		checkEq(fx.build(h).errors, "parallel-enum-range@7",
				"parallel-build: an enum of no known family is refused");
	}

	// ---- the shape an editor draws ------------------------------------------------------------------------------

	{
		auto regionText = [](const GraphShape &shape) {
			mem_std::String out;
			for (auto &region : shape.regions) {
				out.append(mem_std::toString(out.empty() ? "" : "; ", getScopeKindName(region.kind), " ",
						region.opener, "->", region.barrier, ":"));
				for (auto id : region.nodes) {
					out.append(mem_std::toString(" ", id));
				}
			}
			return out;
		};

		auto plain = fx.shape(minimal());
		check(plain.produced, "parallel-build: validate with a shape produces one");
		checkEq(regionText(plain), "parallel 2->4: 3",
				"parallel-build: the block's region is its fan-out, its barrier and its body");

		G h;
		h.n(1, "flow.event").n(2, "par.forEachWith", Query).n(4, "par.barrier").n(9, "scene.global");
		h.own(3, "scene.setInt32", "hp", 2);
		h.foreign(5, "scene.getInt32", "hp", 9);
		h.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "then", 4);
		mem_std::String errors;
		auto conflicted = fx.shape(h, &errors);
		check(!errors.empty() && conflicted.produced,
				"parallel-build: a conflict in the block still leaves a shape");
		checkEq(regionText(conflicted), "parallel 2->4: 3 5", "parallel-build: ... with the whole body in it");

		G loop;
		loop.n(1, "flow.event").n(2, "flow.forEach", R"("params": {"items": [1, 2]})").n(3, "debug.trace")
				.n(5, "par.forEachWith", Query).n(7, "par.barrier");
		loop.own(6, "scene.setInt32", "hp", 5);
		loop.x(1, "then", 2).x(2, "body", 3).x(3, "then", 5).x(5, "body", 6).x(6, "then", 7);
		checkEq(regionText(fx.shape(loop)), "loop 2->0: 3 5 6 7; parallel 5->7: 6",
				"parallel-build: a loop is a region too, and holds the block inside it");

		G broken;
		broken.n(1, "flow.event").n(2, "no.suchOp");
		broken.x(1, "then", 2);
		check(!fx.shape(broken).produced, "parallel-build: a graph refused before its scopes has no shape");
	}

	// ---- the firing contract -----------------------------------------------------------------------------------

	{
		test::interpfx::Fixture run;
		run.extraOps = &registerProbes;
		G g;
		g.n(1, "flow.event").n(2, "probe.fireBoth").n(3, "debug.trace").n(4, "debug.trace");
		g.x(1, "then", 2).x(2, "a", 3).x(2, "b", 4);
		check(run.prepare(g.json()), "parallel-build: the exclusive probe prepares");
		run.run();
		check(run.report.outcome == RunOutcome::OpError,
				"parallel-build: an exclusive operation that fires two outputs fails its step");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
