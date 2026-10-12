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


// A run whose arena reaches its budget. The machine allocates as it goes - a frame per activation,
// an entry per node put on the front - and an allocation refused by the store used to come back as
// something else: a loop or a call that could not open its next turn just stopped, a block that
// could not open a branch said `activation-limit` with the run's activation ceiling as the limit,
// and a node whose push onto the front failed held its token and was swept up as a deadlock. Each of
// those is now the one outcome it is, `out-of-memory`, with the arena's budget as its number.

#include "interp_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::interpfx;

// Two chunks: the root and its table in the first, a little room in the second.
constexpr uint32_t SmallBudget = 2 * vstore::ChunkSize;
constexpr uint32_t Turns = 5'000;

// prepare() without its last line: the arena is initialised with a budget of its own.
bool prepareSmall(Fixture &fx, StringView json) {
	if (!fx.registerOps()) {
		return false;
	}
	fx.asset.init();
	if (fx.asset.load(data::read<mem_std::Interface>(json)) != Status::Ok) {
		return false;
	}
	fx.graph.init();
	if (fx.graph.build(fx.asset, fx.ops) != Status::Ok) {
		return false;
	}
	vstore::Config config;
	config.budgetBytes = SmallBudget;
	return fx.arena.init(config);
}

bool onlyOutOfMemory(const RunReport &report) {
	if (!report.diagnostics.isArray() || report.diagnostics.size() != 1) {
		return false;
	}
	return test::getDiagCodeName(report.diagnostics.getValue(0)) == "out-of-memory";
}

mem_std::String loopGraph() {
	mem_std::String items;
	for (uint32_t i = 0; i < Turns; ++i) {
		items += mem_std::toString(i == 0 ? "" : ", ", i);
	}
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "flow.forEach", "params": {"items": [)", items, R"(]}},
		{"id": 3, "op": "flow.sequence"}
	], "edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3}
	]})");
}

mem_std::String blockGraph() {
	mem_std::String items;
	for (uint32_t i = 0; i < Turns; ++i) {
		items += mem_std::toString(i == 0 ? "" : ", ", "{\"index\": ", i + 1, ", \"generation\": 1}");
	}
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEach", "params": {"entities": [)", items, R"(]}},
		{"id": 3, "op": "debug.trace"},
		{"id": 4, "op": "par.barrier"}
	], "edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 4}
	]})");
}

} // namespace

void performInterpMemoryTests() {
	sprt::cout << "\n== interp: a run that reaches its arena's budget ==\n";

	{
		Fixture fx;
		check(prepareSmall(fx, loopGraph()), "interp-memory: a long loop over a small arena builds");
		RunConfig config;
		config.maxActivations = Turns * 4;
		config.maxSteps = Turns * 16;
		fx.run(config);
		check(fx.report.outcome == RunOutcome::OutOfMemory,
				mem_std::toString("interp-memory: the loop that cannot open its next turn ends out of memory (",
						getRunOutcomeName(fx.report.outcome), ")"));
		check(onlyOutOfMemory(fx.report), "interp-memory: and says so, once, and nothing else");
		check(fx.interp.getLocal().getActivationCount() < Turns,
				"interp-memory: having opened only what the arena held");
	}

	{
		Fixture fx;
		check(prepareSmall(fx, blockGraph()), "interp-memory: a wide block over a small arena builds");
		RunConfig config;
		config.maxActivations = Turns * 4;
		config.maxSteps = Turns * 16;
		fx.run(config);
		check(fx.report.outcome == RunOutcome::OutOfMemory,
				mem_std::toString("interp-memory: the block that cannot open its branches ends out of "
								  "memory, not at the activation limit (",
						getRunOutcomeName(fx.report.outcome), ")"));
		check(onlyOutOfMemory(fx.report),
				"interp-memory: and no branch it could not queue is called a deadlock");
	}

	{
		// The same block with the room it needs runs as it always did.
		Fixture fx;
		check(fx.prepare(blockGraph()), "interp-memory: the block over a default arena builds");
		RunConfig config;
		config.maxActivations = Turns * 4;
		config.maxSteps = Turns * 16;
		check(fx.run(config) == Status::Ok && fx.report.outcome == RunOutcome::Completed,
				"interp-memory: and completes where there is room");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
