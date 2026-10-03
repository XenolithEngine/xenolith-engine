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

// Subtask F1: the local store of one run.
//
// The store is an ordinary ECS - group D's, opened with init() rather than initScene(), because a
// run has no frame and no clock. What is new is what lives in it, and the answer is EVERYTHING the
// run knows: the directory of (node, activation) records, the state of every node, and the ready
// front itself.
//
// The front is the part worth defending. A mem_std::Vector on the host would be simpler today and
// wrong tomorrow: stage F-II snapshots after every node, and a snapshot that does not contain the
// front can only be replayed from the beginning, never resumed. So it costs a few blob operations
// per step and buys resumability - and "two runs produce byte-identical images" below is what says
// the cost was paid correctly.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowLocal.h"

#include "../graph/graph_fixture.h"
#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::graphfx;

struct Occupancy {
	uint64_t count = 0;
	uint64_t bytes = 0;

	bool operator==(const Occupancy &) const = default;
};

Occupancy measure(const Arena &arena) {
	mem_std::Value dump;
	arena.describe(dump);
	auto &live = dump.getValue("live");
	return Occupancy{uint64_t(live.getInteger("count")), uint64_t(live.getInteger("bytes"))};
}

mem_std::Vector<uint8_t> takeImage(const Arena &arena) {
	mem_std::Vector<uint8_t> image;
	image.reserve(arena.saveSize());
	arena.save([&](const uint8_t *data, size_t size, bool zero) {
		if (zero) {
			image.resize(image.size() + size, 0);
		} else {
			image.insert(image.end(), data, data + size);
		}
	});
	return image;
}

// start -> print, with an addf hanging off the value. Three nodes is enough to have one with a
// derived schema, one without, and one of each in between.
StringView fixtureGraph() {
	return StringView(R"json({
		"formatVersion": 1,
		"name": "local",
		"nodes": [
			{"id": 1, "op": "flow.start"},
			{"id": 2, "op": "math.addf"},
			{"id": 3, "op": "sink.print"}
		],
		"edges": [
			{"kind": "exec", "from": 1, "fromPin": "then", "to": 3},
			{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "lhs"},
			{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "value"}
		]
	})json");
}

bool buildGraph(OpRegistry &ops, GraphAsset &asset, RuntimeGraph &graph) {
	asset.init();
	if (asset.load(data::read<mem_std::Interface>(fixtureGraph())) != Status::Ok) {
		return false;
	}
	graph.init();
	mem_std::Value report;
	if (graph.build(asset, ops, &report) != Status::Ok) {
		sprt::cout << "       build: " << data::toString<mem_std::Interface>(report, true) << "\n";
		return false;
	}
	return true;
}

} // namespace

void performInterpLocalTests() {
	sprt::cout << "\n== flow interp: the local store ==\n";

	OpRegistry ops;
	ops.init();
	check(buildFixtureOps(ops), "interp-local: the fixture operations register");

	check(LocalStore::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
			"interp-local: the core types register");
	check(LocalStore::registerCoreTypes(ops.getLocalTypes()) == Status::Ok,
			"interp-local: registering them twice is not an error");
	check(ops.getLocalTypes().get(StringView("interp.Run")) != nullptr
					&& ops.getLocalTypes().get(StringView("interp.NodeState")) != nullptr,
			"interp-local: both core types are in the registry");

	GraphAsset asset;
	RuntimeGraph graph;
	check(buildGraph(ops, asset, graph), "interp-local: the fixture graph builds");

	// ---- what a run looks like the moment it starts ------------------------------------------------

	{
		Arena arena;
		check(arena.init(), "interp-local: the local arena initialises");
		auto base = measure(arena);

		{
			LocalStore local;
			check(local.init(arena, graph, ops.getLocalTypes()) == Status::Ok,
					"interp-local: the store initialises");

			check(local.getRecordCount() == graph.getNodeCount(),
					"interp-local: one record per node");

			// One activation, therefore one frame, and every node's state inside it. The addresses are
			// not merely non-null: they are distinct and they all lie in the one block, which is the
			// whole claim the layout makes.
			auto frame = local.activationFrame(RootActivation);
			auto frameBytes = local.getFrameBytes(0);
			check(frame != NullAddr && frameBytes > 0,
					"interp-local: the root activation owns one frame");

			bool inside = true;
			bool distinct = true;
			mem_std::Vector<Addr> seen;
			for (uint32_t n = 0; n < graph.getNodeCount(); ++n) {
				auto state = local.getState(n, RootActivation);
				if (state == NullAddr || state < frame || state >= frame + frameBytes) {
					inside = false;
				}
				for (auto it : seen) {
					if (it == state) {
						distinct = false;
					}
				}
				seen.emplace_back(state);

				// The record, where there is one, sits immediately after the state - that adjacency is
				// the reason the frame exists rather than two arrays of records.
				if (auto record = local.getRecord(n, RootActivation)) {
					if (record < frame || record >= frame + frameBytes || record <= state) {
						inside = false;
					}
				}
			}
			check(inside, "interp-local: every node's records lie inside that one frame");
			check(distinct, "interp-local: and no two nodes share a state record");

			check(local.frameFor(0, 1) == NullAddr,
					"interp-local: an activation that was never opened has no frame");
			check(local.frameFor(graph.getNodeCount(), RootActivation) == NullAddr,
					"interp-local: a node index past the end has none either");

			// flow.start and math.addf have outputs, so they have a derived schema; sink.print has
			// none, and E5 decided such an operation gets no schema at all.
			check(local.getRecord(0, RootActivation) != NullAddr
							&& local.getRecord(1, RootActivation) != NullAddr,
					"interp-local: a node with outputs has a record");
			check(graph.getNodeAt(2).localSchema == nullptr
							&& local.getRecord(2, RootActivation) == NullAddr,
					"interp-local: a node with no outputs has no record, and needs none");

			check(local.verify() == Status::Ok && arena.verify() == Status::Ok,
					"interp-local: the store and the arena verify");

			auto state = local.readState(local.getState(0, RootActivation));
			check(state.inputs == 0 && state.produced == 0 && state.flags == 0
							&& state.stallPin == 0,
					"interp-local: a fresh node state is all zeroes");
		}

		check(measure(arena) == base,
				"interp-local: destroying the run returns the arena to its base occupancy");
	}

	// ---- two runs are the same run -------------------------------------------------------------------

	{
		Arena first;
		Arena second;
		first.init();
		second.init();

		LocalStore a;
		LocalStore b;
		check(a.init(first, graph, ops.getLocalTypes()) == Status::Ok
						&& b.init(second, graph, ops.getLocalTypes()) == Status::Ok,
				"interp-local: two runs initialise");

		check(test::compareBytes(BytesView(takeImage(first).data(), first.saveSize()),
					  BytesView(takeImage(second).data(), second.saveSize()),
					  StringView("two runs")),
				"interp-local: two runs of one graph give byte-identical images");
	}

	// ---- the ready front lives in the arena --------------------------------------------------------

	{
		Arena arena;
		arena.init();
		LocalStore local;
		check(local.init(arena, graph, ops.getLocalTypes()) == Status::Ok,
				"interp-local: the store initialises for the front");

		check(local.getReadyCount() == 0, "interp-local: the front starts empty");
		uint32_t popped = 0;
		uint32_t poppedAct = 0;
		check(!local.popReady(popped, poppedAct), "interp-local: popping an empty front says so");

		check(local.pushReady(2, RootActivation) == Status::Ok
						&& local.pushReady(0, RootActivation) == Status::Ok
						&& local.pushReady(1, RootActivation) == Status::Ok,
				"interp-local: three nodes are pushed");
		check(local.getReadyCount() == 3, "interp-local: the front holds three");

		// A stack: the discipline the interpreter needs is last-in first-out, and it has to survive a
		// round-trip through the arena.
		bool order = local.popReady(popped, poppedAct) && popped == 1;
		order = order && local.popReady(popped, poppedAct) && popped == 0;
		order = order && local.popReady(popped, poppedAct) && popped == 2;
		check(order, "interp-local: the front is a stack");
		check(local.getReadyCount() == 0 && !local.popReady(popped, poppedAct),
				"interp-local: and it is empty again");

		// The front holds PAIRS: with loops the same node is on it more than once, in different
		// iterations, so a bare node index would be ambiguous.
		check(local.pushReady(3, 7) == Status::Ok && local.popReady(popped, poppedAct)
						&& popped == 3 && poppedAct == 7,
				"interp-local: and what comes back off it is (node, activation)");

		check(local.pushStalled(1, RootActivation) == Status::Ok
						&& local.pushStalled(2, RootActivation) == Status::Ok
						&& local.getStalledCount() == 2
						&& local.getStalledAt(0) == makeRecordKey(1, RootActivation)
						&& local.getStalledAt(1) == makeRecordKey(2, RootActivation),
				"interp-local: the stalled list keeps insertion order");
		check(local.clearStalled() == Status::Ok && local.getStalledCount() == 0,
				"interp-local: the stalled list clears");

		check(local.setStep(7) == Status::Ok && local.getStep() == 7
						&& local.setPass(2) == Status::Ok && local.getPass() == 2,
				"interp-local: the counters round-trip through the arena");

		// State bits, the way a step will write them.
		auto stateAddr = local.getState(1, RootActivation);
		check(local.markInput(stateAddr, 0) == Status::Ok
						&& local.markProduced(stateAddr, 0) == Status::Ok
						&& local.addFlags(stateAddr, NodeFlags::Token) == Status::Ok,
				"interp-local: the state bits are written");
		auto state = local.readState(stateAddr);
		check(state.inputs == 1 && state.produced == 1 && state.flags == NodeFlags::Token,
				"interp-local: and read back");

		check(local.setStall(stateAddr, NodeFlags::StallData, 3) == Status::Ok,
				"interp-local: a stall is recorded");
		state = local.readState(stateAddr);
		check((state.flags & NodeFlags::StallData) != 0 && state.stallPin == 3,
				"interp-local: with the pin it waits on");
		check(local.clearStall(stateAddr) == Status::Ok,
				"interp-local: and cleared when the value arrives");
		state = local.readState(stateAddr);
		check((state.flags & (NodeFlags::StallData | NodeFlags::StallExec)) == 0
						&& state.stallPin == 0 && (state.flags & NodeFlags::Token) != 0,
				"interp-local: clearing a stall leaves the other flags alone");

		check(local.verify() == Status::Ok && arena.verify() == Status::Ok,
				"interp-local: the store still verifies after all of that");
	}

	// ---- a second activation is a second frame ------------------------------------------------------
	//
	// A turn is one allocation. Opening an activation records that it exists; materialising its scope
	// gives it a frame, and every record of that turn is at a fixed offset inside it.
	//
	// Two schemes came before. A directory sorted by a (node, activation) key put a new activation's
	// records in the MIDDLE and shifted the tail element by element - the reason entering a loop body
	// cost more the more turns had already run. Then a slot array made the position arithmetic but
	// still spent an entity and one or two component rows per node, and two ECS lookups to read them
	// back. This spends one block per turn and an addition per record.

	{
		Arena arena;
		arena.init();
		LocalStore local;
		local.init(arena, graph, ops.getLocalTypes());

		auto rootRecords = local.getRecordCount();
		auto rootFrame = local.activationFrame(RootActivation);
		check(rootRecords == graph.getNodeCount() && rootFrame != NullAddr,
				"interp-local: the root activation has a frame holding one record per node");

		uint32_t second = 0;
		check(local.openActivation(0, RootActivation, 1, makeRecordKey(0, RootActivation), second)
								== Status::Ok
						&& second == 1,
				"interp-local: a second activation of the same scope opens");
		check(local.activationFrame(second) == NullAddr,
				"interp-local: an activation that has only been opened owns no frame yet");
		check(local.getRecordCount() == rootRecords,
				"interp-local: and holds no records - a frame is what a record lives in");

		check(local.materializeScope(0, second) == Status::Ok,
				"interp-local: materialising its scope gives it one");
		auto secondFrame = local.activationFrame(second);
		check(secondFrame != NullAddr && secondFrame != rootFrame,
				"interp-local: a frame of its own, not the root's");
		check(local.getRecordCount() == rootRecords + graph.getNodeCount(),
				"interp-local: and one record per node of the scope");

		// The same node in two turns is two records, at the same offset in two different frames.
		check(local.getState(1, second) - secondFrame
						== local.getState(1, RootActivation) - rootFrame,
				"interp-local: a node sits at the same offset in every frame of its scope");
		check(local.getState(1, second) != local.getState(1, RootActivation),
				"interp-local: which is still a different record, because it is a different frame");

		bool neighbours = true;
		for (uint32_t n = 0; n < graph.getNodeCount(); ++n) {
			if (local.getState(n, RootActivation) == NullAddr) {
				neighbours = false;
			}
		}
		check(neighbours, "interp-local: and the root activation's records are untouched");

		check(local.materializeScope(0, second) == Status::ErrorAlreadyPerformed,
				"interp-local: materialising the same activation twice is refused");
		check(local.frameFor(1, 2) == NullAddr
						&& local.materializeScope(0, 2) == Status::ErrorInvalidArguemnt,
				"interp-local: an activation that was never opened has no records and takes none");

		check(local.verify() == Status::Ok && arena.verify() == Status::Ok,
				"interp-local: the store verifies after the second frame");
	}

	// ---- the dump ---------------------------------------------------------------------------------------

	{
		Arena arena;
		arena.init();
		LocalStore local;
		local.init(arena, graph, ops.getLocalTypes());
		local.addFlags(local.getState(0, RootActivation), NodeFlags::Ran | NodeFlags::Token);
		local.markProduced(local.getState(0, RootActivation), 0);

		mem_std::Value dump;
		local.describe(dump);

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json({
			"records": 3, "step": 0, "pass": 0, "ready": 0,
			"nodes": [
				{"id": 1, "node": 0, "activation": 0, "inputs": 0, "produced": 1,
					"ran": true, "token": true},
				{"id": 2, "node": 1, "activation": 0, "inputs": 0, "produced": 0},
				{"id": 3, "node": 2, "activation": 0, "inputs": 0, "produced": 0}
			],
			"stalled": []
		})json"));

		check(test::compareValues(dump, expect, StringView("local store dump")),
				"interp-local: the dump matches the golden one");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
