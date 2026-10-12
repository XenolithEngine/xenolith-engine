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


// A block that names no executor is a GPU candidate: the cost model may give it the device, and
// only a body that lowers can be given one. Lowering alone used to be the judge, and it is not the
// whole of one - it carries what it cannot hold as tagged cells and writes a shader for a body the
// build's GPU checks refuse (`parallel-gpu-op`, `parallel-gpu-type`, ...). The build now runs those
// checks over a candidate too, quietly, and a candidate they refuse has no GPU arm and no shader.

#include "interp_fixture.h"

#include "SPFlowGpu.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

// One block over three references; the body is `body`, the collector sums `index + 1`.
mem_std::String blockGraph(StringView executors, StringView bodyOp, StringView bodyEdgeIn) {
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEach", )", executors, R"("params": {"entities": [
			{"index": 1, "generation": 1}, {"index": 2, "generation": 1}, {"index": 3, "generation": 1}]}},
		{"id": 3, "op": ")", bodyOp, R"("},
		{"id": 4, "op": "par.barrier"},
		{"id": 5, "op": "math.addInt32", "params": {"rhs": 1}},
		{"id": 6, "op": "par.sumInt32"}
	], "edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": ")", bodyEdgeIn, R"(", "to": 4},
		{"kind": "data", "from": 2, "fromPin": "index", "to": 5, "toPin": "lhs"},
		{"kind": "data", "from": 5, "fromPin": "result", "to": 6, "toPin": "value"}
	]})");
}

} // namespace

void performGpuCandidateTests() {
	sprt::cout << "\n== flow: GPU candidates are checked before they are lowered ==\n";

	{
		// Numbers of the GPU set and a structural fork: a candidate that lowers.
		Fixture fx;
		check(fx.prepare(blockGraph(StringView(), StringView("flow.sequence"), StringView("first"))),
				"gpu-candidate: a block of GPU values that names no executor builds");
		check(fx.graph.getBlockCount() == 1 && !fx.graph.getBlockAt(0).gpuRefused
						&& isGpuCandidate(fx.graph, 0) && hashGpuBlock(fx.graph, 0) != 0,
				"gpu-candidate: and is a candidate with a shader");
	}

	{
		// `debug.trace` takes a Float and has no shader form: the checks refuse it.
		Fixture fx;
		check(fx.prepare(blockGraph(StringView(), StringView("debug.trace"), StringView("then"))),
				"gpu-candidate: a block whose body the GPU cannot hold, naming no executor, builds");
		check(fx.graph.getBlockCount() == 1 && fx.graph.getBlockAt(0).gpuRefused,
				"gpu-candidate: the build marks it refused");
		check(!isGpuCandidate(fx.graph, 0) && hashGpuBlock(fx.graph, 0) == 0,
				"gpu-candidate: so it is no candidate and gets no shader");
		check(fx.run() == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"gpu-candidate: and it runs on the CPU as it always did");
	}

	{
		// The same body, demanded: an error, as before.
		Fixture fx;
		check(!fx.prepare(blockGraph(StringView(R"("settings": {"executors": ["gpu"]}, )"),
							  StringView("debug.trace"), StringView("then"))),
				"gpu-candidate: demanding the GPU for that body is still refused by the build");
	}

	{
		// Naming the threads alone is no candidate at all, whatever the body.
		Fixture fx;
		check(fx.prepare(blockGraph(StringView(R"("settings": {"executors": ["threads"]}, )"),
							  StringView("flow.sequence"), StringView("first"))),
				"gpu-candidate: a block that names the threads builds");
		check(!isGpuCandidate(fx.graph, 0) && !fx.graph.getBlockAt(0).gpuRefused,
				"gpu-candidate: and is offered no device without being called refused");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
