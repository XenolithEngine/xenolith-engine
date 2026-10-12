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

// The corpus of the executor sections: the parallel blocks and the GPU bodies, over the scene
// fixture (executors_fixture.h). Kept beside the sections rather than in the interpreter's corpus,
// which runs with no scene: a fan-out's branches are entities, so every case here is a host's case.
//
// The synthetic block package at the top is the same shape the benchmark measures - one block over
// N units, a body of chained adds, a loop when a shape needs nested activations - so a check that
// counts jobs or phases runs the graph whose numbers the cost model carries.

#ifndef TESTS_FLOW_INTERP_EXECUTORS_CORPUS_H_
#define TESTS_FLOW_INTERP_EXECUTORS_CORPUS_H_

#include "SPFlowGpu.h"

#include "executors_fixture.h"

namespace STAPPLER_VERSIONIZED stappler::test::execfx {

using namespace stappler::flow;

// ---- the synthetic block package ---------------------------------------------------------------------
namespace synth {

// The unit of the block package: {hp, mp: Int32}. A constant name rather than a generated one, so
// a package built here is the same package wherever it is built from.
constexpr StringView BlockUnitTypeName("synth.Unit");

// The value unit `i` starts with. A rule rather than a constant, so a body's arithmetic has
// something to do.
inline int32_t blockUnitSeed(uint32_t i) { return int32_t(i % 97); }

struct BlockConfig {
	uint32_t bodyNodes = 1; // how many math.addInt32 the body chains after the read
	bool write = true; // write the result back to the branch's own entity
	bool gather = false; // a par.sumInt32 collector after the barrier
	StringView executors; // "" (the heuristic decides), "threads", "gpu", or both
	uint32_t timeoutMs = 0;

	// Turns of a `flow.forEach` INSIDE the body, or 0 for a straight one. A branch with a loop in it
	// is the one shape the cost model cannot forecast from the graph - how many turns a loop takes
	// is data, so the model substitutes the build's own budget and says so. It is also the shape
	// that opens nested activations, which is the half of an import that cannot be reserved from
	// the branch count alone.
	uint32_t loopTurns = 0;
};

// One `par.forEachWith` over every `synth.Unit`: the body reads `hp`, adds the branch's index to it
// `bodyNodes` times, and writes it back. Every operation of it is in the GPU set, so the same
// package measures all three executors.
inline mem_std::String makeBlockGraph(const BlockConfig &config) {
	mem_std::String nodes;
	mem_std::String edges;
	auto add = [&](mem_std::String &out, StringView text) {
		if (!out.empty()) {
			out.append(",\n\t\t");
		}
		out.append(text.data(), text.size());
	};

	mem_std::String settings("\"with\": [\"synth.Unit\"]");
	if (!config.executors.empty()) {
		settings.append(", \"executors\": [");
		bool first = true;
		StringView rest = config.executors;
		while (!rest.empty()) {
			auto name = rest.readUntil<StringView::Chars<','>>();
			rest.skipChars<StringView::Chars<','>>();
			if (name.empty()) {
				continue;
			}
			if (!first) {
				settings.append(", ");
			}
			settings.append("\"");
			settings.append(name.data(), name.size());
			settings.append("\"");
			first = false;
		}
		settings.append("]");
	}
	if (config.timeoutMs > 0) {
		settings.append(mem_std::toString(", \"timeout\": ", config.timeoutMs));
	}

	add(nodes, "{\"id\": 1, \"op\": \"flow.event\"}");
	add(nodes, mem_std::toString("{\"id\": 2, \"op\": \"par.forEachWith\", \"settings\": {", settings, "}}"));
	add(nodes, "{\"id\": 3, \"op\": \"scene.getInt32\", \"params\": {\"component\": \"synth.Unit\", \"field\": \"hp\"}}");
	add(edges, "{\"kind\": \"exec\", \"from\": 1, \"fromPin\": \"then\", \"to\": 2}");
	add(edges, "{\"kind\": \"exec\", \"from\": 2, \"fromPin\": \"body\", \"to\": 3}");
	add(edges, "{\"kind\": \"data\", \"from\": 2, \"fromPin\": \"entity\", \"to\": 3, \"toPin\": \"target\"}");

	// The body: a chain of adds, each taking the last one's result and the branch's index.
	uint32_t last = 3;
	StringView lastPin("value");
	auto width = config.bodyNodes == 0 ? 1u : config.bodyNodes;
	for (uint32_t k = 0; k < width; ++k) {
		auto id = 10 + k;
		add(nodes, mem_std::toString("{\"id\": ", id, ", \"op\": \"math.addInt32\"}"));
		add(edges, mem_std::toString("{\"kind\": \"data\", \"from\": ", last, ", \"fromPin\": \"", lastPin,
					  "\", \"to\": ", id, ", \"toPin\": \"lhs\"}"));
		add(edges, mem_std::toString("{\"kind\": \"data\", \"from\": 2, \"fromPin\": \"index\", \"to\": ", id,
					  ", \"toPin\": \"rhs\"}"));
		last = id;
		lastPin = StringView("result");
	}

	// A loop inside the body, between the read and the write: `loopTurns` turns, each one writing
	// what the chain above produced into the branch's second field. The write lives in the LOOP's
	// scope, so a branch of this block opens an activation per turn.
	uint32_t bodyTail = 3;
	StringView bodyTailPin("then");
	if (config.loopTurns > 0) {
		mem_std::String items;
		for (uint32_t k = 0; k < config.loopTurns; ++k) {
			if (!items.empty()) {
				items.append(", ");
			}
			items.append(mem_std::toString(int32_t(k + 1)));
		}
		// The turn's own work is a write to the branch's second field. It has to be a node with an
		// exec pin - a loop body is executed, and `math.addInt32` is pure data - and a write is the
		// cheapest honest one: it puts a turn's result somewhere a reader can see it.
		add(nodes, mem_std::toString("{\"id\": 7, \"op\": \"flow.forEach\", \"params\": {\"items\": [", items, "]}}"));
		add(nodes, "{\"id\": 8, \"op\": \"scene.setInt32\", \"params\": {\"component\": \"synth.Unit\", \"field\": \"mp\"}}");
		add(edges, "{\"kind\": \"exec\", \"from\": 3, \"fromPin\": \"then\", \"to\": 7}");
		add(edges, "{\"kind\": \"exec\", \"from\": 7, \"fromPin\": \"body\", \"to\": 8}");
		add(edges, "{\"kind\": \"data\", \"from\": 2, \"fromPin\": \"entity\", \"to\": 8, \"toPin\": \"target\"}");
		add(edges, mem_std::toString("{\"kind\": \"data\", \"from\": ", last, ", \"fromPin\": \"", lastPin,
					  "\", \"to\": 8, \"toPin\": \"value\"}"));
		bodyTail = 7;
		bodyTailPin = StringView("completed"); // a loop fires `completed`, not `then`
	}

	uint32_t tail = bodyTail; // whose `then` the barrier or the write hangs on
	if (config.write) {
		add(nodes, "{\"id\": 4, \"op\": \"scene.setInt32\", \"params\": {\"component\": \"synth.Unit\", \"field\": \"hp\"}}");
		add(edges, mem_std::toString("{\"kind\": \"exec\", \"from\": ", bodyTail, ", \"fromPin\": \"",
					  bodyTailPin, "\", \"to\": 4}"));
		add(edges, "{\"kind\": \"data\", \"from\": 2, \"fromPin\": \"entity\", \"to\": 4, \"toPin\": \"target\"}");
		add(edges, mem_std::toString("{\"kind\": \"data\", \"from\": ", last, ", \"fromPin\": \"", lastPin,
					  "\", \"to\": 4, \"toPin\": \"value\"}"));
		tail = 4;
	}

	add(nodes, "{\"id\": 5, \"op\": \"par.barrier\"}");
	add(edges, mem_std::toString("{\"kind\": \"exec\", \"from\": ", tail, ", \"fromPin\": \"",
				  tail == bodyTail ? bodyTailPin : StringView("then"), "\", \"to\": 5}"));
	if (config.gather) {
		add(nodes, "{\"id\": 6, \"op\": \"par.sumInt32\"}");
		add(edges, mem_std::toString("{\"kind\": \"data\", \"from\": ", last, ", \"fromPin\": \"", lastPin,
					  "\", \"to\": 6, \"toPin\": \"value\"}"));
	}

	// The JSON text, not a Value: the loader takes text, and a header that built a Value would have
	// to know which loader.
	return mem_std::toString("{\"formatVersion\": 1, \"name\": \"synth-block\", \"nodes\": [\n\t\t", nodes,
			"],\n\t\"edges\": [\n\t\t", edges, "]}");
}

} // namespace synth

// ---- the table -----------------------------------------------------------------------------------------------

namespace corpus {

using SceneEnv = execfx::SceneEnv;

// One graph of the corpus and what a run of it needs. `json` is the static graph, or empty when
// `build` supplies it; `sceneTypes` is what the scene registers beyond the store's core types,
// `extraOps` what the registry gets beyond the library.
//
// `outcome` is what the interpreter answers, recorded so that a case that silently stops reaching
// its shape - a builder that fails, a probe that no longer refuses - fails a line rather than
// comparing two wrong runs and finding them equal.
struct Case {
	StringView name;
	StringView json;
	mem_std::String (*build)(SceneEnv &) = nullptr;

	bool scene = false;
	bool (*sceneTypes)(value::TypeRegistry &) = nullptr;
	bool (*extraOps)(OpRegistry &) = nullptr;

	double dt = 0.03;

	uint32_t maxSteps = 0;
	uint32_t maxActivations = 0;

	RunOutcome outcome = RunOutcome::Completed;
};

constexpr StringView ParUnitTypeName("par.Unit");
constexpr StringView ParTotalTypeName("par.Total");

inline TypeId parUnitType() { return value::makeTypeId(ParUnitTypeName); }
inline TypeId parTotalType() { return value::makeTypeId(ParTotalTypeName); }

// The three units every block case runs over, and the entity the totals are written to.
inline mem_std::Vector<value::EntityId> s_parUnits;

// par.Unit {hp, mp: Int32}, par.Total {sum: Int32} and the family enum.Dir {North 0, South 1}.
inline bool registerParTypes(value::TypeRegistry &reg) {
	value::FieldDef unit[] = {
		value::FieldDef{.name = StringView("hp"), .type = value::VarType::Int32},
		value::FieldDef{.name = StringView("mp"), .type = value::VarType::Int32},
	};
	value::FieldDef total[] = {
		value::FieldDef{.name = StringView("sum"), .type = value::VarType::Int32},
	};
	value::EnumMemberDef dir[] = {{StringView("North"), 0}, {StringView("South"), 1}};
	return (reg.get(ParUnitTypeName) != nullptr
				   || reg.createNative(ParUnitTypeName, SpanView<value::FieldDef>(unit, 2)) != nullptr)
			&& (reg.get(ParTotalTypeName) != nullptr
					|| reg.createNative(ParTotalTypeName, SpanView<value::FieldDef>(total, 1)) != nullptr)
			&& (reg.getEnum(StringView("enum.Dir")) != nullptr
					|| reg.createEnum(StringView("enum.Dir"), SpanView<value::EnumMemberDef>(dir, 2))
							!= nullptr);
}

// An enum of the node's family from an Int32 - and for 1, a value no member has and no Int32 holds,
// so the guard of enum.toInt32 refuses it and branch 1 fails.
inline Status opBigEnum(OpContext &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeEnum(value.i == 1 ? (int64_t(1) << 40) : value.i));
}

// Two of the same entity: a set a pin fan-out refuses.
inline Status opTwice(OpContext &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	auto addr = ctx.claimOutput(0);
	auto arena = ctx.getArena();
	auto element = ctx.getOp().getDataOut()[0].element;
	st = value::blob::arrayPush(arena, addr, element, value);
	return st != Status::Ok ? st
							: value::blob::arrayPush(arena, ctx.claimOutput(0), element, value);
}

inline bool registerParOps(OpRegistry &reg) {
	{
		PinDesc in[] = {PinDesc{.name = StringView("value"), .type = value::VarType::Int32},
			PinDesc{.name = StringView("family"), .type = value::VarType::String,
				.role = PinRole::EnumFamily}};
		PinDesc out[] = {PinDesc{.name = StringView("result"), .type = value::VarType::Enum,
			.role = PinRole::EnumFamily}};
		OpDef def;
		def.name = StringView("par.bigEnum");
		def.dataIn = SpanView<PinDesc>(in, 2);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		def.invoke = &opBigEnum;
		if (!reg.createNative(def)) {
			return false;
		}
	}
	{
		PinDesc in[] = {PinDesc{.name = StringView("value"), .type = value::VarType::EntityRef}};
		PinDesc out[] = {PinDesc{.name = StringView("result"), .type = value::VarType::Array,
			.element = value::makeChain(value::VarType::EntityRef)}};
		OpDef def;
		def.name = StringView("par.twice");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.dataOut = SpanView<PinDesc>(out, 1);
		def.flags = OpFlags::Pure;
		def.parallel = OpParallel::Pure;
		def.invoke = &opTwice;
		return reg.createNative(def) != nullptr;
	}
}

// Three units with hp 10, 20, 30 and mp 0, and par.Total {sum 5} on the global entity.
inline bool makeParScene(SceneEnv &env) {
	s_parUnits.clear();
	for (int32_t hp : {10, 20, 30}) {
		auto id = env.createEntity();
		if (env.addComponent(id, parUnitType()) == NullAddr
				|| env.setField(id, parUnitType(), StringView("hp"), value::makeInt32(hp))
						!= Status::Ok) {
			return false;
		}
		s_parUnits.emplace_back(id);
	}
	auto global = env.getGlobalEntity();
	return env.addComponent(global, parTotalType()) != NullAddr
			&& env.setField(global, parTotalType(), StringView("sum"), value::makeInt32(5))
					== Status::Ok;
}

inline mem_std::String parGraph(StringView nodes, StringView edges) {
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [)", nodes, R"(], "edges": [)", edges,
			"]}");
}

// Each branch adds its index to its own hp; after the barrier the sum of the new values is written to
// par.Total.sum, and node 8 gathers the indices.
inline mem_std::String buildParBasic(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"]}},
		{"id": 3, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 4, "op": "math.addInt32"},
		{"id": 5, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 6, "op": "par.barrier"},
		{"id": 7, "op": "par.sumInt32"},
		{"id": 8, "op": "par.gatherInt32"},
		{"id": 9, "op": "scene.global"},
		{"id": 10, "op": "scene.setInt32", "params": {"component": "par.Total", "field": "sum"}})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 6},
		{"kind": "exec", "from": 6, "fromPin": "completed", "to": 10},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 4, "toPin": "rhs"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 7, "toPin": "value"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 8, "toPin": "value"},
		{"kind": "data", "from": 9, "fromPin": "entity", "to": 10, "toPin": "target"},
		{"kind": "data", "from": 7, "fromPin": "result", "to": 10, "toPin": "value"})");
}

// The same over a query nothing answers: the block delivers at once and the sum is 0.
inline mem_std::String buildParEmpty(SceneEnv &env) {
	auto json = buildParBasic(env);
	const char *from = "{\"with\": [\"par.Unit\"]}";
	auto at = json.find(from);
	return at == mem_std::String::npos
			? mem_std::String()
			: json.replace(at, strlen(from), "{\"with\": [\"par.Unit\"], \"without\": [\"par.Unit\"]}");
}

// A first block gathers its entities; a second block runs over them by pin.
inline mem_std::String buildParPin(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"]}},
		{"id": 3, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp", "value": 1}},
		{"id": 4, "op": "par.barrier"},
		{"id": 5, "op": "par.gatherEntityRef"},
		{"id": 6, "op": "par.forEach"},
		{"id": 7, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp", "value": 7}},
		{"id": 8, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "completed", "to": 6},
		{"kind": "exec", "from": 6, "fromPin": "body", "to": 7},
		{"kind": "exec", "from": 7, "fromPin": "then", "to": 8},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "value"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "entities"},
		{"kind": "data", "from": 6, "fromPin": "entity", "to": 7, "toPin": "target"})");
}

// Two independent blocks in a sequence: the first writes hp, the second mp.
inline mem_std::String buildParSequence(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 10, "op": "par.forEachWith", "settings": {"with": ["par.Unit"]}},
		{"id": 11, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp", "value": 1}},
		{"id": 12, "op": "par.barrier"},
		{"id": 20, "op": "par.forEachWith", "settings": {"with": ["par.Unit"]}},
		{"id": 21, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp", "value": 2}},
		{"id": 22, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 10},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 20},
		{"kind": "exec", "from": 10, "fromPin": "body", "to": 11},
		{"kind": "exec", "from": 11, "fromPin": "then", "to": 12},
		{"kind": "exec", "from": 20, "fromPin": "body", "to": 21},
		{"kind": "exec", "from": 21, "fromPin": "then", "to": 22},
		{"kind": "data", "from": 10, "fromPin": "entity", "to": 11, "toPin": "target"},
		{"kind": "data", "from": 20, "fromPin": "entity", "to": 21, "toPin": "target"})");
}

// A block in the body of a two-turn loop: every unit gains 2.
inline mem_std::String buildParInLoop(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [1, 2]}},
		{"id": 3, "op": "par.forEachWith", "settings": {"with": ["par.Unit"]}},
		{"id": 4, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 5, "op": "math.addInt32", "params": {"rhs": 1}},
		{"id": 6, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 7, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "then", "to": 6},
		{"kind": "exec", "from": 6, "fromPin": "then", "to": 7},
		{"kind": "data", "from": 3, "fromPin": "entity", "to": 4, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "entity", "to": 6, "toPin": "target"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "value"})");
}

// A three-turn loop in the body reads and writes its own hp: each turn sees the copy the last one
// wrote, so every unit gains 3. With `items` long enough the branch runs out of its budget.
inline mem_std::String parLoopInBody(StringView items) {
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "onFailure": "partial"}},
		{"id": 3, "op": "flow.forEach", "params": {"items": )" + mem_std::String(items.data(), items.size()) + R"(}},
		{"id": 4, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 5, "op": "math.addInt32", "params": {"rhs": 1}},
		{"id": 6, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 7, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 3, "fromPin": "completed", "to": 7},
		{"kind": "exec", "from": 4, "fromPin": "then", "to": 6},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 4, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 6, "toPin": "target"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "value"})");
}

inline mem_std::String buildParLoopInBody(SceneEnv &env) {
	return makeParScene(env) ? parLoopInBody(StringView("[1, 2, 3]")) : mem_std::String();
}

inline mem_std::String buildParBudget(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	mem_std::String items("[");
	for (uint32_t i = 0; i < 120; ++i) {
		items.append(i ? ", 0" : "0");
	}
	items.append("]");
	return parLoopInBody(items);
}

// Branch 1 fails at the guard of enum.toInt32; the others write their index into hp. Node 6 is the
// barrier, 7 sums what was written into par.Total.sum.
inline mem_std::String parFailing(StringView policy) {
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "onFailure": ")" + mem_std::String(policy.data(), policy.size()) + R"("}},
		{"id": 3, "op": "par.bigEnum", "params": {"family": "enum.Dir"}},
		{"id": 4, "op": "enum.toInt32"},
		{"id": 5, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 6, "op": "par.barrier"},
		{"id": 7, "op": "par.sumInt32"},
		{"id": 9, "op": "scene.global"},
		{"id": 10, "op": "scene.setInt32", "params": {"component": "par.Total", "field": "sum"}})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 6},
		{"kind": "exec", "from": 6, "fromPin": "completed", "to": 10},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 3, "toPin": "value"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "value"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 7, "toPin": "value"},
		{"kind": "data", "from": 9, "fromPin": "entity", "to": 10, "toPin": "target"},
		{"kind": "data", "from": 7, "fromPin": "result", "to": 10, "toPin": "value"})");
}

inline mem_std::String buildParPartial(SceneEnv &env) {
	return makeParScene(env) ? parFailing(StringView("partial")) : mem_std::String();
}
inline mem_std::String buildParNothing(SceneEnv &env) {
	return makeParScene(env) ? parFailing(StringView("nothing")) : mem_std::String();
}
inline mem_std::String buildParCancel(SceneEnv &env) {
	return makeParScene(env) ? parFailing(StringView("cancelFrame")) : mem_std::String();
}

// A pin fan-out over the global entity named twice.
inline mem_std::String buildParDuplicate(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "scene.global"},
		{"id": 3, "op": "par.twice"},
		{"id": 4, "op": "par.forEach"},
		{"id": 5, "op": "debug.trace"},
		{"id": 6, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "body", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 6},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "value"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "entities"})");
}

// ---- GPU bodies ---------------------------------------------------------------------------------------
//
// Bodies under `executors: ["gpu"]`: what the lowering has to reproduce. Under serial they are
// ordinary blocks; the executor sections dispatch each through the CPU evaluator of gpu_eval.h.

constexpr StringView ParHeadingTypeName("par.Heading");
inline TypeId parHeadingType() { return value::makeTypeId(ParHeadingTypeName); }

// par.Heading {dir: Enum enum.Dir, speed: Float32} on units 0 and 2 only; unit 0 faces South, unit 2
// holds 1 << 40 - a value no member has and no Int32 holds.
inline bool registerGpuTypes(value::TypeRegistry &reg) {
	if (!registerParTypes(reg)) {
		return false;
	}
	value::FieldDef heading[] = {
		value::FieldDef{.name = StringView("dir"), .type = value::VarType::Enum,
			.subtypeId = value::makeTypeId("enum.Dir")},
		value::FieldDef{.name = StringView("speed"), .type = value::VarType::Float32},
	};
	return reg.get(ParHeadingTypeName) != nullptr
			|| reg.createNative(ParHeadingTypeName, SpanView<value::FieldDef>(heading, 2)) != nullptr;
}

inline bool makeGpuScene(SceneEnv &env) {
	if (!makeParScene(env)) {
		return false;
	}
	const int64_t dirs[] = {1, 0, int64_t(1) << 40};
	const float speeds[] = {1.5f, 0.0f, -2.25f};
	for (uint32_t i : {0u, 2u}) {
		auto id = s_parUnits[i];
		if (env.addComponent(id, parHeadingType()) == NullAddr
				|| env.setField(id, parHeadingType(), StringView("dir"), value::makeEnum(dirs[i]))
						!= Status::Ok
				|| env.setField(id, parHeadingType(), StringView("speed"),
						   value::makeFloat32(speeds[i]))
						!= Status::Ok) {
			return false;
		}
	}
	return true;
}

// par-basic under a GPU executor.
inline mem_std::String buildGpuBasic(SceneEnv &env) {
	auto json = buildParBasic(env);
	const char *from = "{\"with\": [\"par.Unit\"]}";
	auto at = json.find(from);
	return at == mem_std::String::npos
			? mem_std::String()
			: json.replace(at, strlen(from), "{\"with\": [\"par.Unit\"], \"executors\": [\"gpu\"]}");
}

// hp < 20 raises hp by twice the index; otherwise mp becomes 5. The product runs on one arm only.
inline mem_std::String buildGpuBranch(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"]}},
		{"id": 3, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 4, "op": "compare.lessInt32", "params": {"rhs": 20}},
		{"id": 5, "op": "flow.branch"},
		{"id": 6, "op": "math.mulInt32", "params": {"rhs": 2}},
		{"id": 7, "op": "math.addInt32"},
		{"id": 8, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 9, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp", "value": 5}},
		{"id": 10, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "true", "to": 8},
		{"kind": "exec", "from": 5, "fromPin": "false", "to": 9},
		{"kind": "exec", "from": 8, "fromPin": "then", "to": 10},
		{"kind": "exec", "from": 9, "fromPin": "then", "to": 10},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 8, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 9, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "condition"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 6, "toPin": "lhs"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 7, "toPin": "lhs"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "rhs"},
		{"kind": "data", "from": 7, "fromPin": "result", "to": 8, "toPin": "value"})");
}

// Both outputs of a sequence lead to the same read: it runs twice, and so does the write after it, while
// the pure addition between them runs once and the second write sees its first value.
inline mem_std::String buildGpuMergeTwice(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"]}},
		{"id": 3, "op": "flow.sequence"},
		{"id": 4, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "mp"}},
		{"id": 5, "op": "math.addInt32", "params": {"rhs": 3}},
		{"id": 6, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp"}},
		{"id": 7, "op": "par.barrier"},
		{"id": 8, "op": "par.sumInt32"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "first", "to": 4},
		{"kind": "exec", "from": 3, "fromPin": "second", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "then", "to": 6},
		{"kind": "exec", "from": 6, "fromPin": "then", "to": 7},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 4, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 6, "toPin": "target"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 8, "toPin": "value"})");
}

// The Enum field narrowed to an Int32 column: each unit with a heading turns by its index, written back
// through enum.fromInt32. Unit 1 has no heading (its read refuses), unit 2's heading is out of range
// (the guard refuses): both fail before dispatch.
inline mem_std::String gpuEnumField(StringView policy) {
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"], "onFailure": ")" + mem_std::String(policy.data(), policy.size()) + R"("}},
		{"id": 3, "op": "scene.getEnum", "params": {"component": "par.Heading", "field": "dir", "family": "enum.Dir"}},
		{"id": 4, "op": "enum.toInt32"},
		{"id": 5, "op": "math.addInt32"},
		{"id": 6, "op": "enum.fromInt32", "params": {"family": "enum.Dir"}},
		{"id": 7, "op": "scene.setEnum", "params": {"component": "par.Heading", "field": "dir", "family": "enum.Dir"}},
		{"id": 8, "op": "par.barrier"},
		{"id": 9, "op": "par.sumInt32"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 7},
		{"kind": "exec", "from": 7, "fromPin": "then", "to": 8},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 7, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 5, "toPin": "rhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "value"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 9, "toPin": "value"})");
}

inline mem_std::String buildGpuEnumPartial(SceneEnv &env) {
	return makeGpuScene(env) ? gpuEnumField(StringView("partial")) : mem_std::String();
}
inline mem_std::String buildGpuEnumNothing(SceneEnv &env) {
	return makeGpuScene(env) ? gpuEnumField(StringView("nothing")) : mem_std::String();
}
inline mem_std::String buildGpuEnumCancel(SceneEnv &env) {
	return makeGpuScene(env) ? gpuEnumField(StringView("cancelFrame")) : mem_std::String();
}

// hp divided by the index: branch 0 divides by zero on the GPU, after its row was read.
inline mem_std::String buildGpuDivZero(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"], "onFailure": "partial"}},
		{"id": 3, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 4, "op": "math.divInt32"},
		{"id": 5, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp"}},
		{"id": 6, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 6},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 4, "toPin": "rhs"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "value"})");
}

// The body reads par.Total.sum through the global entity - a row of its own for the whole block - and
// adds it to its own hp.
inline mem_std::String buildGpuForeignRead(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "scene.global"},
		{"id": 3, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"]}},
		{"id": 4, "op": "scene.getInt32", "params": {"component": "par.Total", "field": "sum"}},
		{"id": 5, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 6, "op": "math.addInt32"},
		{"id": 7, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 8, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "then", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 7},
		{"kind": "exec", "from": 7, "fromPin": "then", "to": 8},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 4, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "entity", "to": 7, "toPin": "target"},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 6, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "value", "to": 6, "toPin": "rhs"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "value"})");
}

// Every reducer over what the body computes from the speed of a heading and the index.
inline mem_std::String buildGpuCollectors(SceneEnv &env) {
	if (!makeGpuScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Heading"], "executors": ["gpu"]}},
		{"id": 3, "op": "scene.getFloat32", "params": {"component": "par.Heading", "field": "speed"}},
		{"id": 4, "op": "convert.int32ToFloat32"},
		{"id": 5, "op": "math.mulFloat32"},
		{"id": 6, "op": "compare.lessFloat32", "params": {"rhs": 0.0}},
		{"id": 7, "op": "value.vec2", "params": {"value": [0.5, -1.0]}},
		{"id": 8, "op": "math.scaleVec2"},
		{"id": 9, "op": "par.barrier"},
		{"id": 10, "op": "par.sumFloat32"},
		{"id": 11, "op": "par.minFloat32"},
		{"id": 12, "op": "par.maxFloat32"},
		{"id": 13, "op": "par.count"},
		{"id": 14, "op": "par.any"},
		{"id": 15, "op": "par.all"},
		{"id": 16, "op": "par.gatherFloat32"},
		{"id": 17, "op": "par.sumVec2"},
		{"id": 18, "op": "value.vec3", "params": {"value": [1.0, -2.0, 0.25]}},
		{"id": 19, "op": "math.scaleVec3"},
		{"id": 20, "op": "par.sumVec3"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 9},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 4, "toPin": "value"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "rhs"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 6, "toPin": "lhs"},
		{"kind": "data", "from": 7, "fromPin": "value", "to": 8, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 8, "toPin": "rhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 10, "toPin": "value"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 11, "toPin": "value"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 12, "toPin": "value"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 13, "toPin": "value"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 14, "toPin": "value"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 15, "toPin": "value"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 16, "toPin": "value"},
		{"kind": "data", "from": 8, "fromPin": "result", "to": 17, "toPin": "value"},
		{"kind": "data", "from": 18, "fromPin": "value", "to": 19, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 19, "toPin": "rhs"},
		{"kind": "data", "from": 19, "fromPin": "result", "to": 20, "toPin": "value"})");
}

// A write on the false arm takes its value from a read on the true arm: a branch that goes false holds a
// token it has no value for, and the run ends in a deadlock the branch itself does not fail over.
inline mem_std::String buildGpuDeadlock(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"]}},
		{"id": 3, "op": "compare.lessInt32", "params": {"rhs": 1}},
		{"id": 4, "op": "flow.branch"},
		{"id": 5, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 6, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp"}},
		{"id": 7, "op": "par.barrier"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 4, "fromPin": "true", "to": 5},
		{"kind": "exec", "from": 4, "fromPin": "false", "to": 6},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 7},
		{"kind": "exec", "from": 6, "fromPin": "then", "to": 7},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 3, "toPin": "lhs"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "condition"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 6, "toPin": "target"},
		{"kind": "data", "from": 5, "fromPin": "value", "to": 6, "toPin": "value"})");
}

// One step makes a pure node ready by data and a write ready by its token: the machine runs the data one
// first, and the log says which came first.
inline mem_std::String buildGpuOrder(SceneEnv &env) {
	if (!makeParScene(env)) {
		return mem_std::String();
	}
	return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"]}},
		{"id": 3, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 4, "op": "math.addInt32"},
		{"id": 5, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "mp", "value": 1}},
		{"id": 6, "op": "par.barrier"},
		{"id": 7, "op": "par.sumInt32"})",
			R"(
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 5},
		{"kind": "exec", "from": 5, "fromPin": "then", "to": 6},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 3, "toPin": "target"},
		{"kind": "data", "from": 2, "fromPin": "entity", "to": 5, "toPin": "target"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 4, "toPin": "rhs"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 7, "toPin": "value"})");
}

inline const Case Cases[] = {
	// The blocks.
	{.name = StringView("par-basic"), .build = &buildParBasic, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-empty"), .build = &buildParEmpty, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-pin"), .build = &buildParPin, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-sequence"), .build = &buildParSequence, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-in-loop"), .build = &buildParInLoop, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-loop-in-body"), .build = &buildParLoopInBody, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-partial"), .build = &buildParPartial, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-nothing"), .build = &buildParNothing, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("par-cancel"), .build = &buildParCancel, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps,
		.outcome = RunOutcome::Cancelled},
	{.name = StringView("par-budget"), .build = &buildParBudget, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps, .maxSteps = 4000,
		.maxActivations = 4000},
	{.name = StringView("par-duplicate"), .build = &buildParDuplicate, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps,
		.outcome = RunOutcome::OpError},
	{.name = StringView("par-activation-limit"), .build = &buildParBasic, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps, .maxActivations = 2,
		.outcome = RunOutcome::ActivationLimit},

	// The GPU bodies, after the blocks above.
	{.name = StringView("gpu-basic"), .build = &buildGpuBasic, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-branch"), .build = &buildGpuBranch, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-merge-twice"), .build = &buildGpuMergeTwice, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-enum-partial"), .build = &buildGpuEnumPartial, .scene = true,
		.sceneTypes = &registerGpuTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-enum-nothing"), .build = &buildGpuEnumNothing, .scene = true,
		.sceneTypes = &registerGpuTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-enum-cancel"), .build = &buildGpuEnumCancel, .scene = true,
		.sceneTypes = &registerGpuTypes, .extraOps = &registerParOps,
		.outcome = RunOutcome::Cancelled},
	{.name = StringView("gpu-div-zero"), .build = &buildGpuDivZero, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-foreign-read"), .build = &buildGpuForeignRead, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-collectors"), .build = &buildGpuCollectors, .scene = true,
		.sceneTypes = &registerGpuTypes, .extraOps = &registerParOps},
	{.name = StringView("gpu-deadlock"), .build = &buildGpuDeadlock, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps,
		.outcome = RunOutcome::Deadlock},
	{.name = StringView("gpu-order"), .build = &buildGpuOrder, .scene = true,
		.sceneTypes = &registerParTypes, .extraOps = &registerParOps},
};

inline constexpr uint32_t CaseCount = sizeof(Cases) / sizeof(Cases[0]);

// The GPU bodies: the last of the cases.
inline constexpr uint32_t GpuCaseCount = 11;
inline const SpanView<Case> GpuCases(Cases + (CaseCount - GpuCaseCount), GpuCaseCount);
inline const SpanView<Case> ParallelCases(Cases, CaseCount);

// Prepares a fixture for a case: the scene it needs, the graph it names, and - when asked - a
// journal over both stores. The shape every consumer of the corpus starts from, written once.
template <typename A>
inline bool prepareCase(FixtureT<A> &fx, const Case &c, bool journal) {
	fx.extraOps = c.extraOps;
	if (c.scene && !fx.prepareScene(c.sceneTypes)) {
		return false;
	}
	auto json = c.build ? c.build(fx) : mem_std::String(c.json.data(), c.json.size());
	if (json.empty() || !fx.prepare(json)) {
		return false;
	}
	return !journal || fx.prepareJournal();
}

// The configuration a case runs under. The journal is `fx.journal` when asked for and nothing
// otherwise; the quantum only matters with one.
template <typename A>
inline RunConfigT<A, TestEnv> configFor(FixtureT<A> &fx, const Case &c, bool journal,
		RollbackQuantum quantum = RollbackQuantum::Step) {
	auto config = journal ? fx.journalConfig() : fx.sceneConfig();
	config.maxSteps = c.maxSteps;
	config.maxActivations = c.maxActivations;
	config.quantum = quantum;
	return config;
}

} // namespace corpus

} // namespace stappler::test::execfx

#endif /* TESTS_FLOW_INTERP_EXECUTORS_CORPUS_H_ */
