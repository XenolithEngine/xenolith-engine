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

// Subtask F2: pure data flow - a graph with no execution edges at all.
//
// Every node here is one the interpreter runs because its inputs became available, not because it
// was told to. That is the eager strategy of III.5, and the execution log is checked against a
// golden for every case: "the answer is right" is only half a test of an interpreter, because the
// same answer can be reached in an order that will not survive loops or snapshots.
//
// Two properties of the value plumbing are checked here rather than argued about:
//
//   * a value crosses an edge with the conversion the BUILD chose - nothing consults the matrix at
//     run time;
//   * a container crosses an edge by being read where its producer keeps it, and is copied only when
//     the consumer stores one of its own. The leak checks are what say the copy happened
//     exactly once.

#include "interp_fixture.h"
#include "corpus.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

// The probe of the scratch block: reads a String constant through the arena door and measures the
// arena before and after. What it learns is in the statics, because the measurement happens INSIDE
// the step, where no test can look - and the arena it measures is the fixture's, named here because
// an OpContext hands out a kind-erased reference that cannot describe() itself.
const Arena *s_scratchArena = nullptr;
bool s_scratchTouch = true;
Occupancy s_scratchBefore;
Occupancy s_scratchDuring;
Addr s_scratchHandle = NullAddr;
Addr s_scratchPayload = NullAddr;
uint32_t s_scratchSize = 0;

Status opScratch(OpContext &ctx) {
	if (!s_scratchTouch) {
		return Status::Ok;
	}
	s_scratchBefore = measure(*s_scratchArena);
	s_scratchHandle = ctx.getInputAddr(0);
	if (s_scratchHandle == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto handle = flow::value::blob::getHandle(ctx.getArena(), s_scratchHandle);
	s_scratchPayload = handle.data;
	s_scratchSize = handle.size;
	s_scratchDuring = measure(*s_scratchArena);
	return Status::Ok;
}

bool registerScratchOps(OpRegistry &reg) {
	PinDesc in[] = {PinDesc{.name = StringView("text"), .type = flow::value::VarType::String}};
	OpDef def;
	def.name = StringView("test.scratch");
	def.dataIn = SpanView<PinDesc>(in, 1);
	def.invoke = &opScratch;
	return reg.createNative(def) != nullptr;
}

constexpr StringView ScratchGraph(R"json({"formatVersion": 1,
	"nodes": [{"id": 1, "op": "test.scratch", "params": {"text": "scratch-payload"}}],
	"edges": []})json");

} // namespace

void performInterpDataflowTests() {
	sprt::cout << "\n== flow interp: pure data flow ==\n";

	// ---- arithmetic, and the order it happens in ----------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::Arithmetic)),
				"interp-dataflow: the arithmetic graph builds");

		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-dataflow: the run completes");
		check(fx.report.stepCount == 3, "interp-dataflow: three nodes, three steps");
		check(StringView(fx.report.getTrace()) == "1 2 3",
				"interp-dataflow: the chain runs in dependency order");

		flow::value::Var result;
		check(fx.output(3, 0, result) && result.f == 7.0,
				"interp-dataflow: 3 * 2 + 1 came out as seven");

		// The literal that fed the first node was converted by the BUILD, and its type is the pin's.
		flow::value::Var literal;
		check(fx.output(1, 0, literal) && literal.type == flow::value::VarType::Float
						&& literal.f == 3.0,
				"interp-dataflow: the literal reached the graph in the pin's type");
	}

	// ---- the conversion an edge carries --------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::Widen)),
				"interp-dataflow: the widening graph builds");

		// Bool -> Int is a Widen cell; the build put the rule in the edge and the interpreter applied
		// it without asking the matrix.
		check(fx.graph.getDataEdges()[0].cast == flow::value::CastRule::Widen,
				"interp-dataflow: the edge carries a widening conversion");
		check(fx.run() == Status::Ok, "interp-dataflow: the widening run completes");

		flow::value::Var result;
		check(fx.output(2, 0, result) && result.i == 42,
				"interp-dataflow: true reached an integer input as one");
	}

	// ---- containers: borrowed to read, copied to keep --------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::Strings)),
				"interp-dataflow: the string graph builds");

		auto base = measure(fx.arena);
		check(fx.run() == Status::Ok, "interp-dataflow: the string run completes");

		check(StringView(fx.outputString(1, 0)) == "hi",
				"interp-dataflow: a container literal reached its node");
		check(StringView(fx.outputString(2, 0)) == "hi!",
				"interp-dataflow: the value was read across the edge and a new one was built");

		// The scratch block a container CONSTANT needs lives exactly one step; the copy a node makes
		// of its own output lives as long as the run. Both are gone when the run is.
		fx.interp.reset();
		check(measure(fx.arena) == base,
				"interp-dataflow: the run leaves nothing behind - no scratch, no orphan blocks");
	}

	// ---- two runs are one run --------------------------------------------------------------------------

	{
		Fixture a;
		Fixture b;
		StringView json(corpus::StringsAndFloat);
		check(a.prepare(json) && b.prepare(json), "interp-dataflow: both fixtures build");
		check(a.run() == Status::Ok && b.run() == Status::Ok, "interp-dataflow: both run");

		check(StringView(a.report.getTrace()) == StringView(b.report.getTrace()),
				"interp-dataflow: two runs produce the same execution log");
		check(test::compareBytes(BytesView(takeImage(a.arena).data(), a.arena.saveSize()),
					  BytesView(takeImage(b.arena).data(), b.arena.saveSize()),
					  StringView("two runs")),
				"interp-dataflow: and byte-identical local stores");
	}

	// ---- an operation that fails ------------------------------------------------------------------------

	{
		Fixture fx;
		check(fx.prepare(StringView(corpus::OpFail)),
				"interp-dataflow: the failing graph builds");

		check(fx.run() != Status::Ok && fx.report.outcome == RunOutcome::OpError,
				"interp-dataflow: an operation's failure ends the run");
		check(StringView(fx.report.getTrace()) == "1",
				"interp-dataflow: the log holds what ran before it, and not the node that failed");
		check(fx.report.diagnostics.isArray() && fx.report.diagnostics.size() == 1
						&& fx.report.diagnostics.getValue(0).getValue("at").getInteger(0) == 2
						&& StringView(fx.report.diagnostics.getValue(0).getValue("names").getString(0))
								== "test.fail",
				"interp-dataflow: the diagnostic names the node and the operation");

		// F-I has nothing to roll a half-written step back with - snapshots are F-II - so the store is
		// left as it is. Said out loud here because it will read like a bug otherwise.
		check(fx.interp.getLocal().isValid(),
				"interp-dataflow: the local store survives the failure, for the inspector to look "
				"at");
	}

	// ---- a container constant read through the arena door is materialised, freed, and stays ---------
	//
	// A String or Array literal has no producer to borrow from, so getInput and getInputAddr decode
	// it into a SCRATCH block of the run's arena and free it when the step ends. Three facts
	// about that are pinned here because a second engine has to reproduce them to leave the same
	// bytes (docs/planning/graph_codegen.md, §3 item 16): the scratch is two blocks - the handle and
	// the payload - taken during the step; both are gone by the end of it; and a freed block is not
	// zeroed, so the image of the arena keeps a trace of the literal that a byte-for-byte comparison
	// sees. The last one is why "read the constant from the table instead" is not a free optimisation
	// in the mode that promises the same image.

	{
		Fixture touched;
		Fixture untouched;
		touched.extraOps = &registerScratchOps;
		untouched.extraOps = &registerScratchOps;
		check(touched.prepare(ScratchGraph) && untouched.prepare(ScratchGraph),
				"interp-dataflow: the scratch graph builds twice");

		s_scratchArena = &touched.arena;
		s_scratchTouch = true;
		check(touched.run() == Status::Ok && touched.report.outcome == RunOutcome::Completed
						&& StringView(touched.report.getTrace()) == "1",
				"interp-dataflow: the probe ran, and read its constant");

		check(s_scratchSize == 15 && s_scratchPayload != NullAddr,
				"interp-dataflow: the constant was materialised as a blob of its own length");
		check(s_scratchDuring.count == s_scratchBefore.count + 2,
				"interp-dataflow: the scratch is two blocks while the step runs - the handle and "
				"the payload");
		check(measure(touched.arena).count == s_scratchBefore.count,
				"interp-dataflow: and both are gone when the step is over");
		check(!touched.arena.isLivePayload(s_scratchHandle)
						&& !touched.arena.isLivePayload(s_scratchPayload),
				"interp-dataflow: neither address is a live block any more");

		// The same graph with the probe declining to read: the only difference between the two arenas
		// is the scratch that one of them took and gave back.
		s_scratchArena = &untouched.arena;
		s_scratchTouch = false;
		check(untouched.run() == Status::Ok, "interp-dataflow: the run that never reads completes");
		s_scratchTouch = true;

		auto image = takeImage(touched.arena);
		bool trace = false;
		for (uint32_t i = 0; i < s_scratchSize && s_scratchPayload + i < image.size(); ++i) {
			trace = trace || image[s_scratchPayload + i] != 0;
		}
		check(trace, "interp-dataflow: the freed payload is not zeroed - the image keeps a trace of "
					 "the literal");
		check(image != takeImage(untouched.arena),
				"interp-dataflow: so a run that materialised the constant and one that did not "
				"leave different images");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
