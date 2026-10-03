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

#ifndef TESTS_FLOW_INTERP_CORPUS_H_
#define TESTS_FLOW_INTERP_CORPUS_H_

#include "interp_fixture.h"

// The corpus: every graph the interpreter sections run, in one place, and a table that says what a
// run of each one needs beyond the core library. A generated program has to execute a graph exactly
// as the interpreter does, and "exactly" is proved by running the same graphs through both and
// comparing - so the graphs have to be a LIST somebody can iterate, not literals scattered over the
// sections. The sections include this header and name their graphs from it rather than keeping a
// copy. None of them needs a scene: the runs are in the kernel's own environment.
namespace STAPPLER_VERSIONIZED stappler::test::interpfx::corpus {

// ---- pure data flow -----------------------------------------------------------------------

// 3.0 * 2.0 + 1.0, as a chain of eager nodes.
constexpr StringView Arithmetic(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.float", "params": {"value": 3.0}},
		{"id": 2, "op": "math.mulFloat", "params": {"rhs": 2.0}},
		{"id": 3, "op": "math.addFloat", "params": {"rhs": 1.0}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "lhs"}
	]})json");

// A Bool crossing a widening edge into an Int input.
constexpr StringView Widen(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.bool", "params": {"value": true}},
		{"id": 2, "op": "math.addInt", "params": {"rhs": 41}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"}
	]})json");

// A container literal, borrowed across an edge and copied into a new one.
constexpr StringView Strings(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.string", "params": {"value": "hi"}},
		{"id": 2, "op": "string.concat", "params": {"rhs": "!"}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"}
	]})json");

// The same, with an unrelated scalar beside it.
constexpr StringView StringsAndFloat(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.string", "params": {"value": "hi"}},
		{"id": 2, "op": "string.concat", "params": {"rhs": "!"}},
		{"id": 3, "op": "value.float", "params": {"value": 2.5}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"}
	]})json");

// A pure node whose operation refuses.
constexpr StringView OpFail(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.float", "params": {"value": 1.0}},
		{"id": 2, "op": "test.fail"}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"}
	]})json");

// ---- the flow of execution ---------------------------------------------------------------

constexpr StringView Chain(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "debug.trace"},
		{"id": 3, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 3}
	]})json");

// A sequence whose first branch is two nodes deep.
constexpr StringView Sequence(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 5}
	]})json");

constexpr StringView BranchFalse(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.branch", "params": {"condition": false}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "true", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "false", "to": 4}
	]})json");

constexpr StringView BranchTrue(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.branch", "params": {"condition": true}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "true", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "false", "to": 4}
	]})json");

// test.split fires AND produces, so an exec target and a data consumer are ready at once.
constexpr StringView ComputableFirst(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "test.split", "params": {"value": 2.0}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "math.mulFloat", "params": {"rhs": 3.0}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 3},
		{"kind": "data", "from": 2, "fromPin": "value", "to": 4, "toPin": "lhs"}
	]})json");

constexpr StringView TwoEntries(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.event"},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 4}
	]})json");

// The same graph written in two orders: the file's order must not reach the execution order.
constexpr StringView SequenceShortOrdered(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 4}
	]})json");

constexpr StringView SequenceShortShuffled(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 4, "op": "debug.trace"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "debug.trace"},
		{"id": 1, "op": "flow.event"}
	],
	"edges": [
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 4},
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 3}
	]})json");

// ---- waiting and the fixed point --------------------------------------------------------

// The consumer is on the FIRST branch and its value comes from the second.
constexpr StringView LateValue(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "test.split", "params": {"value": 5.0}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 4},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 3, "toPin": "value"}
	]})json");

// Three waits in a row, each unblocked by the arrival before it.
constexpr StringView ChainedWait(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "math.addFloat"},
		{"id": 4, "op": "flow.sequence"},
		{"id": 5, "op": "math.addFloat"},
		{"id": 6, "op": "test.split", "params": {"value": 1.0}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 6},
		{"kind": "exec", "from": 4, "fromPin": "first", "to": 6},
		{"kind": "data", "from": 6, "fromPin": "value", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 3, "toPin": "lhs"}
	]})json");

// A pure chain hanging off an exec-gated producer: not an entry, runs when the value exists.
constexpr StringView DeferredPure(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "test.split", "params": {"value": 4.0}},
		{"id": 3, "op": "math.mulFloat", "params": {"rhs": 2.0}},
		{"id": 4, "op": "math.addFloat", "params": {"rhs": 1.0}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "data", "from": 2, "fromPin": "value", "to": 3, "toPin": "lhs"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "lhs"}
	]})json");

// ---- deadlock and the paths that simply end ----------------------------------------------

// Node 4 holds a token and waits on node 3, which sits on the branch not taken.
constexpr StringView Deadlock(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.branch", "params": {"condition": false}},
		{"id": 3, "op": "test.split", "params": {"value": 1.0}},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "true", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "false", "to": 4},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "value"}
	]})json");

// The same deadlock, mirrored: the token goes down `true`, the producer sits on `false`.
constexpr StringView DeadlockTrue(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.branch", "params": {"condition": true}},
		{"id": 3, "op": "test.split", "params": {"value": 1.0}},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "true", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "false", "to": 3},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "value"}
	]})json");

// A pure node whose only input comes from the branch not taken: work nobody required.
constexpr StringView UnneededPure(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.branch", "params": {"condition": false}},
		{"id": 3, "op": "math.mulFloat", "params": {"rhs": 2.0}},
		{"id": 4, "op": "test.split", "params": {"value": 1.0}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "true", "to": 4},
		{"kind": "data", "from": 4, "fromPin": "value", "to": 3, "toPin": "lhs"}
	]})json");

// Four in a row: the graph the step ceiling is lowered to meet.
constexpr StringView ChainOfFour(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "debug.trace"},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 4}
	]})json");

// One node with an exec input and nothing to fire it.
constexpr StringView NoEntry(R"json({"formatVersion": 1,
	"nodes": [{"id": 1, "op": "debug.trace"}], "edges": []})json");

// ---- one unit at a time -----------------------------------------------------------------

// test.fail as an entry point of its own beside an exec chain that has nothing to do with it.
constexpr StringView FailEntry(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "test.fail", "params": {"value": 1.0}},
		{"id": 3, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 3}
	]})json");

// ---- stopping on a condition ------------------------------------------------------------

// 1 event -> 2 split (fires, and produces 2.0) -> 4 trace, with 3 computing 2.0 * 3.0 = 6.0.
constexpr StringView SplitWatch(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "test.split", "params": {"value": 2.0}},
		{"id": 3, "op": "math.mulFloat", "params": {"rhs": 3.0}},
		{"id": 4, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 4},
		{"kind": "data", "from": 2, "fromPin": "value", "to": 3, "toPin": "lhs"}
	]})json");

// ---- a version per unit, and undoing one ------------------------------------------------

// Five units: 1 event, 2 sequence, 3 mul, 4 trace, 5 trace. Enough depth to step back through.
constexpr StringView Walk(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "math.mulFloat", "params": {"lhs": 2.0, "rhs": 3.0}},
		{"id": 4, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 5}
	]})json");

// ---- the 32-bit scalars, vectors and a node's own enum family ----------------------

// Pure nodes only: 32-bit wrap, a conversion, a vector length, and an enum through its family.
constexpr StringView Numeric32(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.int32", "params": {"value": 2147483647}},
		{"id": 2, "op": "math.addInt32", "params": {"rhs": 2}},
		{"id": 3, "op": "convert.int32ToFloat32"},
		{"id": 4, "op": "math.mulFloat32", "params": {"rhs": 0.5}},
		{"id": 5, "op": "value.vec3", "params": {"value": [3, 4, 12]}},
		{"id": 6, "op": "math.lengthVec3"},
		{"id": 7, "op": "math.addFloat32"},
		{"id": 8, "op": "convert.intToUInt32", "params": {"value": 4294967295}},
		{"id": 9, "op": "math.addUInt32", "params": {"rhs": 1}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "value"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "value", "to": 6, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 7, "toPin": "lhs"},
		{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "rhs"},
		{"kind": "data", "from": 8, "fromPin": "result", "to": 9, "toPin": "lhs"}
	]})json");

// A conversion out of range refuses.
constexpr StringView Numeric32Refused(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "convert.intToInt32", "params": {"value": 2147483648}},
		{"id": 2, "op": "math.absInt32"}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "result", "to": 2, "toPin": "value"}
	]})json");

// The family comes from the node; its record field reads back with it.
constexpr StringView EnumFamily(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "value.int32", "params": {"value": 4}},
		{"id": 2, "op": "enum.fromInt32", "params": {"family": "enum.Direction"}},
		{"id": 3, "op": "enum.toInt32"},
		{"id": 4, "op": "math.mulInt32", "params": {"rhs": 3}}
	],
	"edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "value"},
		{"kind": "data", "from": 3, "fromPin": "result", "to": 4, "toPin": "lhs"}
	]})json");

// ---- loops and activations --------------------------------------------------------------

// 1 event -> 2 forEach; body -> 3 trace and 4 addInt(item + 100); completed -> 5 trace.
constexpr StringView Loop(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [10, 20, 30]}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "math.addInt", "params": {"rhs": 100}},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 5},
		{"kind": "data", "from": 2, "fromPin": "item", "to": 4, "toPin": "lhs"}
	]})json");

constexpr StringView LoopEmpty(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": []}},
		{"id": 3, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 5}
	]})json");

// A body ending in a branch that never fires: no turn ever reaches a tail.
constexpr StringView LoopUntakenBranch(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [1, 2, 3]}},
		{"id": 3, "op": "flow.branch", "params": {"condition": false}},
		{"id": 4, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "true", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 5}
	]})json");

constexpr StringView LoopNested(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [1, 2]}},
		{"id": 3, "op": "flow.forEach", "params": {"items": [7, 8, 9]}},
		{"id": 4, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 5}
	]})json");

// An exec cycle with no scope: a repeat, bounded by nothing but the ceiling.
constexpr StringView Repeat(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "debug.trace"},
		{"id": 3, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "then", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 2}
	]})json");

// The same literal list read by two routes: forEach's item and array.getInt at the loop's index.
constexpr StringView LoopLiteral(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [7, 8, 9, 10]}},
		{"id": 3, "op": "array.getInt", "params": {"items": [7, 8, 9, 10]}},
		{"id": 4, "op": "math.addInt"},
		{"id": 5, "op": "debug.trace"},
		{"id": 6, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 6},
		{"kind": "exec", "from": 2, "fromPin": "completed", "to": 5},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 3, "toPin": "index"},
		{"kind": "data", "from": 2, "fromPin": "item", "to": 4, "toPin": "lhs"},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "rhs"}
	]})json");

// array.getInt past the end of a literal: the operation's own refusal.
constexpr StringView ArrayOutOfRange(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "debug.trace"},
		{"id": 3, "op": "array.getInt", "params": {"items": [1, 2], "index": 7}},
		{"id": 4, "op": "math.addInt"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "data", "from": 3, "fromPin": "value", "to": 4, "toPin": "lhs"}
	]})json");

// A loop on the FIRST branch of a sequence, and a trace on the second.
//
// What this pins is the rule for closing a turn: the interpreter closes an open activation only
// when the ready front is empty ALTOGETHER, not when the body's own work has run out. The second
// branch was put on the front by the sequence before the loop ever opened its first turn, so it is
// still there when the body drains - and it runs BEFORE the second turn. The order is therefore
// "1 2 3 4 5 3 4 3 6" and not "1 2 3 4 3 4 3 6 5", which is what the sentence "the first branch
// runs to completion before the second" would suggest. The code is the definition and the sentence
// is the approximation; the translation plan reproduces the code (docs/planning/graph_codegen.md,
// §3 item 8), and this graph is what says so in a form both machines are held to.
constexpr StringView LoopInSequence(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.sequence"},
		{"id": 3, "op": "flow.forEach", "params": {"items": [1, 2]}},
		{"id": 4, "op": "debug.trace"},
		{"id": 5, "op": "debug.trace"},
		{"id": 6, "op": "debug.trace"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 3},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 5},
		{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
		{"kind": "exec", "from": 3, "fromPin": "completed", "to": 6}
	]})json");

// ---- the table -------------------------------------------------------------------------------

// One graph of the corpus and what a run of it needs: `extraOps` is what the registry gets beyond
// the library, and `outcome` is what the interpreter answers - recorded so that a case that silently
// stops reaching its shape fails a line rather than comparing two wrong runs and finding them equal.
struct Case {
	StringView name;
	StringView json;
	bool (*extraOps)(OpRegistry &) = nullptr;

	uint32_t maxSteps = 0;
	uint32_t maxActivations = 0;

	RunOutcome outcome = RunOutcome::Completed;
};

inline const Case Cases[] = {
	{.name = StringView("arithmetic"), .json = Arithmetic},
	{.name = StringView("widen"), .json = Widen},
	{.name = StringView("strings"), .json = Strings},
	{.name = StringView("strings-and-float"), .json = StringsAndFloat},
	{.name = StringView("op-fail"), .json = OpFail, .outcome = RunOutcome::OpError},
	{.name = StringView("chain"), .json = Chain},
	{.name = StringView("sequence"), .json = Sequence},
	{.name = StringView("branch-false"), .json = BranchFalse},
	{.name = StringView("branch-true"), .json = BranchTrue},
	{.name = StringView("computable-first"), .json = ComputableFirst},
	{.name = StringView("two-entries"), .json = TwoEntries},
	{.name = StringView("sequence-short"), .json = SequenceShortOrdered},
	{.name = StringView("sequence-short-shuffled"), .json = SequenceShortShuffled},
	{.name = StringView("late-value"), .json = LateValue},
	{.name = StringView("chained-wait"), .json = ChainedWait},
	{.name = StringView("deferred-pure"), .json = DeferredPure},
	{.name = StringView("deadlock"), .json = Deadlock, .outcome = RunOutcome::Deadlock},
	{.name = StringView("deadlock-true"), .json = DeadlockTrue, .outcome = RunOutcome::Deadlock},
	{.name = StringView("unneeded-pure"), .json = UnneededPure},
	{.name = StringView("chain-of-four"), .json = ChainOfFour},
	{.name = StringView("step-limit"), .json = ChainOfFour, .maxSteps = 2,
		.outcome = RunOutcome::StepLimit},
	{.name = StringView("no-entry"), .json = NoEntry},
	{.name = StringView("fail-entry"), .json = FailEntry, .outcome = RunOutcome::OpError},
	{.name = StringView("sequence-ceiling"), .json = Sequence, .maxSteps = 2,
		.outcome = RunOutcome::StepLimit},
	{.name = StringView("split-watch"), .json = SplitWatch},
	{.name = StringView("walk"), .json = Walk},
	{.name = StringView("loop"), .json = Loop},
	{.name = StringView("loop-empty"), .json = LoopEmpty},
	{.name = StringView("loop-untaken-branch"), .json = LoopUntakenBranch},
	{.name = StringView("loop-nested"), .json = LoopNested},
	{.name = StringView("repeat"), .json = Repeat, .maxSteps = 9,
		.outcome = RunOutcome::StepLimit},
	{.name = StringView("loop-activation-limit"), .json = Loop, .maxActivations = 2,
		.outcome = RunOutcome::ActivationLimit},
	{.name = StringView("loop-literal"), .json = LoopLiteral},
	{.name = StringView("array-out-of-range"), .json = ArrayOutOfRange,
		.outcome = RunOutcome::OpError},
	{.name = StringView("loop-in-sequence"), .json = LoopInSequence},
	{.name = StringView("numeric32"), .json = Numeric32},
	{.name = StringView("numeric32-refused"), .json = Numeric32Refused,
		.outcome = RunOutcome::OpError},
	{.name = StringView("enum-family"), .json = EnumFamily},
};

inline constexpr uint32_t CaseCount = sizeof(Cases) / sizeof(Cases[0]);

// Prepares a fixture for a case: the graph it names and, when asked, a journal over the run's store.
template <typename A>
inline bool prepareCase(FixtureT<A> &fx, const Case &c, bool journal) {
	fx.extraOps = c.extraOps;
	if (!fx.prepare(c.json)) {
		return false;
	}
	return !journal || fx.prepareJournal();
}

// The configuration a case runs under. The journal is `fx.journal` when asked for and nothing
// otherwise; the quantum only matters with one.
template <typename A>
inline RunConfigT<A, NoEnv> configFor(FixtureT<A> &fx, const Case &c, bool journal,
		RollbackQuantum quantum = RollbackQuantum::Step) {
	auto config = journal ? fx.journalConfig() : RunConfigT<A, NoEnv>();
	config.maxSteps = c.maxSteps;
	config.maxActivations = c.maxActivations;
	config.quantum = quantum;
	return config;
}

} // namespace stappler::test::interpfx::corpus

#endif /* TESTS_FLOW_INTERP_CORPUS_H_ */
