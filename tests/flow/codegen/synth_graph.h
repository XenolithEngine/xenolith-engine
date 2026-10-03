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

#ifndef TESTS_FLOW_CODEGEN_SYNTH_GRAPH_H_
#define TESTS_FLOW_CODEGEN_SYNTH_GRAPH_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include "../check/flow_check.h"

// A graph with a shape nobody drew, for the generated half of the differential corpus.
//
// The hand-written corpus answers "a graph somebody thought of". This answers "a graph nobody would
// think of": an analysis in the generator that claims some nodes keep their order whatever the data
// does is wrong first on a shape its author never pictured. So the shapes come from a seed, and the
// seed is the only input.
//
// Deterministic to the bit on every platform: one LCG (test::Lcg), no float in any decision, and
// nothing that depends on the iteration order of a container.
//
// The vocabulary is small and every operation in it is one the hand-written corpus already runs,
// with the pin names taken from there. The point is not to reach every operation - `codegen-ops`
// does that - but every combination of the control-flow constructs.
namespace STAPPLER_VERSIONIZED stappler::test::synth {

struct RunConfig {
	uint64_t seed = 1;

	// Exec-side nodes to spend. Data nodes are added on top, so the graph comes out larger.
	uint32_t budget = 12;

	// How deep a loop body may nest. Two is enough for "a loop inside a loop"; three costs turns
	// multiplicatively and buys no shape the analysis has not already met.
	uint32_t maxDepth = 2;

	// An exec cycle with no scope (a repeat). It ends the run at the
	// step ceiling by construction, so a seed that takes it reaches nothing after the cycle.
	bool allowRepeat = true;

	// String constants and `string.concat`. A loop's item list is an array either way - a container
	// is what `flow.forEach` iterates - so this gates the other container path, not both.
	bool allowStrings = true;
};

// What the seed actually built. The section asserts over the whole set that every construct turned
// up somewhere: a fuzzer whose shapes nobody counts is a fuzzer that can quietly stop producing
// loops, and stay green about it.
struct Shape {
	uint32_t nodes = 0;
	uint32_t execEdges = 0;
	uint32_t dataEdges = 0;
	uint32_t loops = 0;
	uint32_t depth = 0;

	bool branch = false;
	bool sequence = false;
	bool loop = false;
	bool nestedLoop = false;
	bool repeat = false;
	bool crossScope = false; // a value read from an enclosing scope
	bool container = false; // an array or string constant
	bool dataCondition = false; // a branch whose condition is computed, not a literal
	bool deadEnd = false; // an exec pin that goes nowhere
};

namespace detail {

// One producer a data edge may come from: which node, which pin, and the scope depth it sits at -
// a consumer at depth d may read a producer at depth <= d, and reading one at a smaller depth is
// a cross-scope read.
struct SynthProducer {
	uint32_t id = 0;
	StringView pin;
	uint32_t depth = 0;
	char type = 'i'; // 'i' int, 's' string, 'b' bool
};

struct SynthBuilder {
	test::Lcg lcg;
	RunConfig cfg;
	Shape shape;
	mem_std::Value nodes = mem_std::Value(mem_std::Value::Type::ARRAY);
	mem_std::Value edges = mem_std::Value(mem_std::Value::Type::ARRAY);
	mem_std::Vector<SynthProducer> producers;
	uint32_t nextId = 1;
	uint32_t spent = 0;

	explicit SynthBuilder(const RunConfig &c) : lcg(c.seed), cfg(c) { }

	uint32_t node(StringView op, mem_std::Value params = mem_std::Value()) {
		mem_std::Value n(mem_std::Value::Type::DICTIONARY);
		n.setInteger(int64_t(nextId), "id");
		n.setString(op, "op");
		if (params) {
			n.setValue(sprt::move(params), "params");
		}
		nodes.addValue(sprt::move(n));
		++shape.nodes;
		return nextId++;
	}

	void execEdge(uint32_t from, StringView pin, uint32_t to) {
		mem_std::Value e(mem_std::Value::Type::DICTIONARY);
		e.setString("exec", "kind");
		e.setInteger(int64_t(from), "from");
		e.setString(pin, "fromPin");
		e.setInteger(int64_t(to), "to");
		edges.addValue(sprt::move(e));
		++shape.execEdges;
	}

	void dataEdge(uint32_t from, StringView fromPin, uint32_t to, StringView toPin) {
		mem_std::Value e(mem_std::Value::Type::DICTIONARY);
		e.setString("data", "kind");
		e.setInteger(int64_t(from), "from");
		e.setString(fromPin, "fromPin");
		e.setInteger(int64_t(to), "to");
		e.setString(toPin, "toPin");
		edges.addValue(sprt::move(e));
		++shape.dataEdges;
	}

	// A producer of `type` visible at `depth`, or none. `outermost` asks for one from as far out as
	// possible instead of the nearest - which is how a body comes to read a value belonging to a
	// scope that encloses it, and that read has to be asked for: the
	// nearest producer is always the body's own, so a generator that only ever took the nearest
	// would never once cross a scope.
	const SynthProducer *pick(char type, uint32_t depth, bool outermost, bool *crossScope) {
		const SynthProducer *best = nullptr;
		for (auto &p : producers) {
			if (p.type != type || p.depth > depth) {
				continue;
			}
			if (!best || (outermost ? p.depth < best->depth : p.depth > best->depth)) {
				best = &p;
			}
		}
		if (best && crossScope) {
			*crossScope = best->depth < depth;
		}
		return best;
	}

	// An integer literal node, so that there is always something to read.
	uint32_t intValue(int64_t v, uint32_t depth) {
		mem_std::Value params(mem_std::Value::Type::DICTIONARY);
		params.setInteger(v, "value");
		auto id = node(StringView("value.int"), sprt::move(params));
		producers.emplace_back(SynthProducer{id, StringView("value"), depth, 'i'});
		return id;
	}

	// A computed Bool for a branch condition: `compare.equalInt` over an integer in scope. Makes the
	// branch data-dependent, which is the case an order analysis may not fold.
	uint32_t boolFromData(uint32_t depth) {
		bool cross = false;
		auto src = pick('i', depth, false, &cross);
		if (!src) {
			intValue(int64_t(lcg.next(4)), depth);
			src = pick('i', depth, false, &cross);
		}
		mem_std::Value params(mem_std::Value::Type::DICTIONARY);
		params.setInteger(int64_t(lcg.next(3)), "rhs");
		auto id = node(StringView("compare.equalInt"), sprt::move(params));
		dataEdge(src->id, src->pin, id, StringView("lhs"));
		shape.crossScope = shape.crossScope || cross;
		producers.emplace_back(SynthProducer{id, StringView("result"), depth, 'b'});
		return id;
	}

	// A consumer of a value in scope, so that a body has data work in it and so that cross-scope
	// reads happen where a body is fed from outside.
	void addDataWork(uint32_t depth) {
		bool cross = false;
		if (cfg.allowStrings && (lcg.next(4) == 0)) {
			mem_std::Value params(mem_std::Value::Type::DICTIONARY);
			params.setString("+", "rhs");
			auto strSrc = pick('s', depth, false, &cross);
			if (!strSrc) {
				mem_std::Value v(mem_std::Value::Type::DICTIONARY);
				v.setString("s", "value");
				auto id = node(StringView("value.string"), sprt::move(v));
				producers.emplace_back(SynthProducer{id, StringView("value"), depth, 's'});
				strSrc = pick('s', depth, false, &cross);
			}
			auto id = node(StringView("string.concat"), sprt::move(params));
			dataEdge(strSrc->id, strSrc->pin, id, StringView("lhs"));
			producers.emplace_back(SynthProducer{id, StringView("result"), depth, 's'});
			shape.container = true;
			shape.crossScope = shape.crossScope || cross;
			return;
		}
		auto src = pick('i', depth, false, &cross);
		if (!src) {
			intValue(int64_t(lcg.next(8)), depth);
			src = pick('i', depth, false, &cross);
		}

		// Both pins fed, when there is something further out to feed the second with: `lhs` from
		// this scope is what puts the node in this scope, and `rhs` from an enclosing one is then a
		// read across scopes, whose readiness is decided off the producer's own record rather than
		// pushed into this one.
		const SynthProducer *outer = nullptr;
		if (depth > 0 && lcg.next(2) == 0) {
			bool outerCross = false;
			auto candidate = pick('i', depth, true, &outerCross);
			if (candidate && candidate->depth < depth) {
				outer = candidate;
			}
		}

		mem_std::Value params(mem_std::Value::Type::DICTIONARY);
		if (!outer) {
			params.setInteger(int64_t(lcg.next(10)), "rhs");
		}
		auto id = node(StringView("math.addInt"), sprt::move(params));
		dataEdge(src->id, src->pin, id, StringView("lhs"));
		if (outer) {
			dataEdge(outer->id, outer->pin, id, StringView("rhs"));
			shape.crossScope = true;
		}
		producers.emplace_back(SynthProducer{id, StringView("result"), depth, 'i'});
		shape.crossScope = shape.crossScope || cross;
	}

	// Fills one exec slot and everything that follows it in a straight line. `from`/`fromPin` is the
	// slot; `depth` the scope it lives in. Returns the last node of the line, or 0 if it built none.
	uint32_t fill(uint32_t from, StringView fromPin, uint32_t depth) {
		uint32_t head = 0;
		uint32_t tail = 0;
		mem_std::Vector<uint32_t> line;

		// The repeat: an exec cycle with no scope, closed between two nodes of this line. Considered
		// on every exit from this function, because a sequence, a branch and a loop all return early.
		//
		// Top level only: inside a body a cycle multiplies with the turns and reaches the step
		// ceiling before the body has run once, which makes every seed that takes it the same seed.
		uint32_t lastTrace = 0;
		auto closeCycle = [&] {
			// The edge leaves by `then`, and only `debug.trace` has one - a branch's pins are
			// `true`/`false`, a loop's are `body`/`completed`. And an exec output emits at most one
			// edge, so it can only be a `then` nothing else used: the line's last node, and only when
			// the line ended on a trace rather than on a construct that returned early.
			if (cfg.allowRepeat && depth == 0 && !shape.repeat && lastTrace != 0
					&& lastTrace == tail && line.size() >= 2 && lastTrace != line.front()
					&& lcg.next(3) == 0) {
				execEdge(lastTrace, StringView("then"), line.front());
				shape.repeat = true;
			}
			return tail;
		};
		auto attach = [&](uint32_t id) {
			if (tail == 0) {
				execEdge(from, fromPin, id);
				head = id;
			} else {
				execEdge(tail, StringView("then"), id);
			}
			tail = id;
		};

		while (spent < cfg.budget) {
			// Weighted so that a plain step is common and the constructs are all reachable. The
			// deeper the scope the likelier the line just ends, which is what keeps a nested body
			// from spending the whole budget.
			auto roll = lcg.next(100);
			if (roll < 8 + depth * 20) {
				shape.deadEnd = true;
				break;
			}
			++spent;
			if (roll < 44) {
				auto id = node(StringView("debug.trace"));
				attach(id);
				line.emplace_back(id);
				lastTrace = id;
				if (lcg.next(3) == 0) {
					addDataWork(depth);
				}
				continue;
			}
			if (roll < 60) {
				auto id = node(StringView("flow.sequence"));
				attach(id);
				line.emplace_back(id);
				shape.sequence = true;
				fill(id, StringView("first"), depth);
				fill(id, StringView("second"), depth);
				return closeCycle();
			}
			if (roll < 76) {
				mem_std::Value params(mem_std::Value::Type::DICTIONARY);
				uint32_t cond = 0;
				if (lcg.next(2) == 0) {
					cond = boolFromData(depth);
					shape.dataCondition = true;
				} else {
					params.setBool(lcg.next(2) == 0, "condition");
				}
				auto id = node(StringView("flow.branch"), sprt::move(params));
				attach(id);
				line.emplace_back(id);
				if (cond) {
					dataEdge(cond, StringView("result"), id, StringView("condition"));
				}
				shape.branch = true;
				fill(id, StringView("true"), depth);
				fill(id, StringView("false"), depth);
				return closeCycle();
			}
			if (depth < cfg.maxDepth) {
				mem_std::Value items(mem_std::Value::Type::ARRAY);
				auto count = 1 + lcg.next(3);
				for (uint32_t i = 0; i < count; ++i) { items.addInteger(int64_t(10 + i)); }
				mem_std::Value params(mem_std::Value::Type::DICTIONARY);
				params.setValue(sprt::move(items), "items");
				auto id = node(StringView("flow.forEach"), sprt::move(params));
				attach(id);
				line.emplace_back(id);
				shape.loop = true;
				shape.nestedLoop = shape.nestedLoop || depth > 0;
				shape.container = true;
				++shape.loops;
				shape.depth = depth + 1 > shape.depth ? depth + 1 : shape.depth;

				// The body's own values, visible only inside it. Registered before the body is
				// filled and dropped after, so nothing outside can read them.
				auto mark = producers.size();
				producers.emplace_back(SynthProducer{id, StringView("item"), depth + 1, 'i'});
				producers.emplace_back(SynthProducer{id, StringView("index"), depth + 1, 'i'});
				fill(id, StringView("body"), depth + 1);
				producers.resize(mark);

				fill(id, StringView("completed"), depth);
				return closeCycle();
			}
			auto id = node(StringView("debug.trace"));
			attach(id);
			line.emplace_back(id);
			lastTrace = id;
		}

		(void)head;
		return closeCycle();
	}
};

} // namespace detail

// The graph a seed names. `out`, when given, receives what was actually built.
inline mem_std::Value makeRunGraph(const RunConfig &config, Shape *out = nullptr) {
	detail::SynthBuilder b(config);

	auto event = b.node(StringView("flow.event"));

	// One value at the root before anything else, so that a body always has an enclosing scope to
	// read from. Without it a cross-scope edge would depend on the seed having happened to make a
	// producer at depth 0 first, which is a coverage gap disguised as randomness.
	b.intValue(int64_t(1 + b.lcg.next(9)), 0);

	b.fill(event, StringView("then"), 0);

	mem_std::Value graph(mem_std::Value::Type::DICTIONARY);
	graph.setInteger(1, "formatVersion");
	graph.setString("synth", "name");
	graph.setValue(sprt::move(b.nodes), "nodes");
	graph.setValue(sprt::move(b.edges), "edges");
	if (out) {
		*out = b.shape;
	}
	return graph;
}

} // namespace stappler::test::synth

#endif /* TESTS_FLOW_CODEGEN_SYNTH_GRAPH_H_ */
