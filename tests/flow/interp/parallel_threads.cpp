// The `threads` executor over the corpus: a block's branches, cut into batches, run on the engine's
// task pool - and the run is the serial one, per activation, whatever the batch and whatever the
// pool's width. Batches run inline (an executor that runs them where they were asked for) prove the
// import and the paths without a single thread in the way; the pool adds the threads themselves.
//
// Wide blocks prove the two ends of a job: the build of its batches and the copying of its frames
// back are handed to the pool too, and a count says so rather than a timing.
//
// The knight-shaped sections of the studio's suite - a block over a whole board, a host that
// refuses an input while branches read its scene, the generated units - stay there: they ride on
// the studio's run host and its committed units, and neither travels with the executors.

#include "executors_fixture.h"
#include "executors_corpus.h"

#include "SPFlowExecutorThreads.h"

#include "SPPlatform.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::execfx;
using namespace stappler::test::execfx::corpus;
namespace oracle = stappler::test::oracle;
namespace synth = stappler::test::execfx::synth;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

// The suite-wide `RunConfig` names the kernel's own environment (tests.h); a section with a scene
// runs in TestEnv's, so it names its own.
using RunConfig = Fixture::ConfigType;

void snapshot(Fixture &fx, RunSnapshot &out) {
	oracle::takeSnapshot(fx.graph, fx.interp, fx.arena, fx.report, out, &fx.scene, &fx.sceneArena,
			true);
}

CompareOptions perActivation() {
	CompareOptions opts{Level::Observable, false, true};
	opts.order = CompareOptions::Order::PerActivation;
	return opts;
}

// Runs every batch on the machine's own thread, when the machine asks for a delivery: the import and
// the paths without a single thread in the way.
struct InlineBranchExecutor : BlockExecutor {
	uint32_t batch = 1;
	mem_std::Vector<BranchJob *> jobs;
	mem_std::Vector<BlockTicket> ready;
	uint32_t launched = 0;

	void submit(const BlockTicket &ticket) override { ready.emplace_back(ticket); }

	bool poll(BlockTicket &out) override {
		if (!ready.empty()) {
			out = ready.front();
			ready.erase(ready.begin());
			return true;
		}
		if (jobs.empty()) {
			return false;
		}
		auto job = jobs.front();
		for (uint32_t i = 0; i < job->getBatchCount(); ++i) {
			job->runBatch(i);
		}
		out = job->getTicket();
		return true;
	}

	uint32_t takesBranches(const BlockTicket &, uint32_t n, uint32_t) override {
		return batch == 0 ? n : batch;
	}

	void launch(BranchJob &job) override {
		jobs.emplace_back(&job);
		++launched;
	}

	void forget(BranchJob &job) override {
		for (auto it = jobs.begin(); it != jobs.end(); ++it) {
			if (*it == &job) {
				jobs.erase(it);
				break;
			}
		}
	}

	StringView getName() const override { return StringView("inline"); }
};

// The rest of a stepped run: a unit at a time, and when the run waits for its branches, the wait.
// False when the deadline passes first.
bool finishThreaded(Fixture &fx, flow::ThreadExecutor &executor) {
	uint32_t waits = 0;
	while (true) {
		if (fx.interp.stepOnce(fx.report)) {
			continue;
		}
		if (fx.interp.getState() != RunState::Suspended) {
			return true;
		}
		if (!executor.waitDelivery(TimeInterval::milliseconds(100)) && ++waits > 300) {
			return false;
		}
	}
}

// One frame under a pool of threads.
bool runThreaded(Fixture &fx, const Case &c, const RunConfig &cfg, flow::ThreadExecutor &executor,
		uint32_t frame = 0) {
	return fx.frameBegin(c.dt, frame, cfg) == Status::Ok && finishThreaded(fx, executor);
}

// The synthetic package: one block over N units of `synth.Unit`. Here it is the only fixture wide
// enough to exercise the phases.
bool prepareSynthBlock(Fixture &fx, uint32_t branches, uint32_t bodyNodes, uint32_t loopTurns = 0) {
	if (!fx.prepareScene([](value::TypeRegistry &reg) {
			value::FieldDef fields[] = {
				value::FieldDef{.name = StringView("hp"), .type = value::VarType::Int32},
				value::FieldDef{.name = StringView("mp"), .type = value::VarType::Int32},
			};
			return reg.get(synth::BlockUnitTypeName) != nullptr
					|| reg.createNative(synth::BlockUnitTypeName,
							   SpanView<value::FieldDef>(fields, 2))
							!= nullptr;
		})) {
		return false;
	}
	auto typeId = value::makeTypeId(synth::BlockUnitTypeName);
	for (uint32_t i = 0; i < branches; ++i) {
		auto id = fx.createEntity();
		if (fx.addComponent(id, typeId) == flow::NullAddr
				|| fx.setField(id, typeId, StringView("hp"),
						   value::makeInt32(synth::blockUnitSeed(i)))
						!= Status::Ok) {
			return false;
		}
	}
	synth::BlockConfig blockConfig;
	blockConfig.bodyNodes = bodyNodes;
	blockConfig.loopTurns = loopTurns;
	return fx.prepare(synth::makeBlockGraph(blockConfig));
}

// A thousand branches need room for a thousand activations, and the default budget is derived from
// the node count of a graph that has six nodes in it.
RunConfig wideConfig(Fixture &fx) {
	auto cfg = fx.sceneConfig();
	cfg.maxActivations = 20'064;
	cfg.maxSteps = 200'000;
	return cfg;
}

RunConfig config(Fixture &fx, const Case &c, BlockExecutor *executor = nullptr) {
	auto cfg = configFor(fx, c, false);
	cfg.executor = executor;
	return cfg;
}

// ---- a gate a body waits at, from outside ----------------------------------------------------------

sprt::atomic<bool> s_gateOpen = true;
sprt::atomic<int64_t> s_gateFrom = 0;

void pause(uint64_t ms) {
	auto until = platform::clock(ClockType::Monotonic) + ms * 1'000;
	while (platform::clock(ClockType::Monotonic) < until) {
		sprt::this_thread::yield();
	}
}

// Its input to its output; a branch whose index is at least `s_gateFrom` waits while the gate is shut.
Status opGate(OpContext &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	while (value.i >= s_gateFrom.load() && !s_gateOpen.load()) {
		sprt::this_thread::yield();
	}
	return ctx.setOutput(0, value);
}

bool registerThreadOps(OpRegistry &reg) {
	if (!registerParOps(reg)) {
		return false;
	}
	PinDesc in[] = {PinDesc{.name = StringView("value"), .type = value::VarType::Int32}};
	PinDesc out[] = {PinDesc{.name = StringView("result"), .type = value::VarType::Int32}};
	OpDef def;
	def.name = StringView("test.gate");
	def.dataIn = SpanView<PinDesc>(in, 1);
	def.dataOut = SpanView<PinDesc>(out, 1);
	def.flags = OpFlags::Pure;
	def.parallel = OpParallel::Pure;
	def.invoke = &opGate;
	return reg.createNative(def) != nullptr;
}

bool registerThreadTypes(value::TypeRegistry &reg) {
	value::FieldDef mark[] = {
		value::FieldDef{.name = StringView("v"), .type = value::VarType::Int32}};
	return registerParTypes(reg)
			&& (reg.get(StringView("par.Mark")) != nullptr
					|| reg.createNative(StringView("par.Mark"), SpanView<value::FieldDef>(mark, 1))
							!= nullptr);
}

// A block on `from`: every branch writes its index, through the gate, into its own `field`.
mem_std::String gatedBlock(uint32_t fanOut, StringView field, StringView settings = StringView()) {
	return mem_std::toString(R"({"id": )", fanOut, R"(, "op": "par.forEachWith", "settings": {"with": ["par.Unit"])",
			settings.empty() ? "" : ", ", settings, R"(}},
		{"id": )", fanOut + 1, R"(, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": ")", field, R"("}},
		{"id": )", fanOut + 2, R"(, "op": "par.barrier"},
		{"id": )", fanOut + 3, R"(, "op": "test.gate"})");
}

mem_std::String gatedEdges(uint32_t fanOut) {
	return mem_std::toString(R"({"kind": "exec", "from": )", fanOut, R"(, "fromPin": "body", "to": )", fanOut + 1, R"(},
		{"kind": "exec", "from": )", fanOut + 1, R"(, "fromPin": "then", "to": )", fanOut + 2, R"(},
		{"kind": "data", "from": )", fanOut, R"(, "fromPin": "entity", "to": )", fanOut + 1, R"(, "toPin": "target"},
		{"kind": "data", "from": )", fanOut, R"(, "fromPin": "index", "to": )", fanOut + 3, R"(, "toPin": "value"},
		{"kind": "data", "from": )", fanOut + 3, R"(, "fromPin": "result", "to": )", fanOut + 1, R"(, "toPin": "value"})");
}

// Two blocks under a sequence.
mem_std::String twoBlocksGraph() {
	return parGraph(mem_std::toString(R"({"id": 1, "op": "flow.event"}, {"id": 2, "op": "flow.sequence"}, )",
							gatedBlock(10, StringView("hp")), ", ", gatedBlock(20, StringView("mp"))),
			mem_std::toString(R"({"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 10},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 20}, )",
					gatedEdges(10), ", ", gatedEdges(20)));
}

// A block, and a component added to the global entity on the sequence's other branch.
mem_std::String freezeGraph() {
	return parGraph(mem_std::toString(R"({"id": 1, "op": "flow.event"}, {"id": 2, "op": "flow.sequence"}, )",
							gatedBlock(10, StringView("hp")),
							R"(, {"id": 30, "op": "scene.global"},
		{"id": 31, "op": "scene.addComponent", "params": {"component": "par.Mark"}})"),
			mem_std::toString(R"({"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "first", "to": 10},
		{"kind": "exec", "from": 2, "fromPin": "second", "to": 31},
		{"kind": "data", "from": 30, "fromPin": "entity", "to": 31, "toPin": "target"}, )",
					gatedEdges(10)));
}

mem_std::String timeoutGraph(StringView policy) {
	return parGraph(mem_std::toString(R"({"id": 1, "op": "flow.event"}, )",
							gatedBlock(10, StringView("hp"),
									mem_std::toString(R"("onFailure": ")", policy, R"(", "timeout": 50)"))),
			mem_std::toString(R"({"kind": "exec", "from": 1, "fromPin": "then", "to": 10}, )",
					gatedEdges(10)));
}

bool prepareGraph(Fixture &fx, StringView json, bool journal = false) {
	fx.extraOps = &registerThreadOps;
	return fx.prepareScene(&registerThreadTypes) && makeParScene(fx) && fx.prepare(json)
			&& (!journal || fx.prepareJournal());
}

struct TestClock : BlockClock {
	uint64_t now = 0;
	uint64_t nowMs() const override { return now; }
};

int32_t unitHp(Fixture &fx, uint32_t unit) {
	value::Var v;
	fx.getField(s_parUnits[unit], parUnitType(), StringView("hp"), v);
	return int32_t(v.i);
}

// The report carries numbers; the check names the code the way a check reads best, by the table in
// flow_names.cpp.
bool hasCode(const RunReport &report, StringView code) {
	if (report.diagnostics.isArray()) {
		for (auto &it : report.diagnostics.asArray()) {
			if (test::getDiagCodeName(it) == code) {
				return true;
			}
		}
	}
	return false;
}

} // namespace

void performParallelThreadsTests() {
	sprt::cout << "\n== flow interp: branches in stores of their own ==\n";

	// ---- the corpus, batches run inline ---------------------------------------------------------------

	for (auto &c : ParallelCases) {
		auto label = [&](StringView what) { return mem_std::toString("parallel-threads: ", c.name, ": ", what); };

		Fixture serial;
		check(prepareCase(serial, c, false), label("prepares"));
		serial.frame(c.dt, 0, config(serial, c));
		RunSnapshot base;
		snapshot(serial, base);

		for (uint32_t batch : {1u, 2u, 0u}) {
			InlineBranchExecutor executor;
			executor.batch = batch;
			Fixture fx;
			prepareCase(fx, c, false);
			fx.frame(c.dt, 0, config(fx, c, &executor));
			RunSnapshot snap;
			snapshot(fx, snap);
			auto what = mem_std::toString("inline, batch ", batch);
			if (c.outcome == RunOutcome::Completed) {
				check(fx.report.outcome == c.outcome
								&& (executor.launched > 0 || c.name == StringView("par-empty"))
								&& oracle::compareRuns(base, snap, perActivation(), label(what)),
						label(mem_std::toString(what, ": the serial run, per activation")));
			} else {
				check(fx.report.outcome == c.outcome,
						label(mem_std::toString(what, ": ends ", getRunOutcomeName(c.outcome))));
			}
		}

		// The same, on threads.
		const uint16_t many = sprt::max(uint16_t(4), uint16_t(sprt::thread::hardware_concurrency()));
		for (uint16_t threads : {uint16_t(1), uint16_t(2), many}) {
			for (uint32_t batch : {1u, 0u}) {
				flow::ThreadExecutor executor(flow::ThreadExecutor::Options{
					.threads = threads,
					.batch = batch,
					.policy = flow::ThreadExecutor::Policy::Always,
				});
				auto what = mem_std::toString(threads, " threads, batch ", batch);
				bool ran = false;
				RunSnapshot snap;
				{
					Fixture fx;
					prepareCase(fx, c, false);
					ran = runThreaded(fx, c, config(fx, c, &executor), executor);
					snapshot(fx, snap);
					check(ran && fx.report.outcome == c.outcome,
							label(mem_std::toString(what, ": ends ", getRunOutcomeName(c.outcome))));
					if (c.outcome == RunOutcome::Completed) {
						check(oracle::compareRuns(base, snap, perActivation(), label(what)),
								label(mem_std::toString(what, ": the serial run, per activation")));
					} else if (c.outcome != RunOutcome::Cancelled) {
						check(snap.sceneImage == base.sceneImage,
								label(mem_std::toString(what, ": the scene the serial run leaves")));
					}
					fx.interp.reset();
				}
			}
		}
	}

	using flow::ThreadExecutor;
	auto threads = [](uint16_t n, uint32_t batch = 1) {
		return ThreadExecutor::Options{.threads = n, .batch = batch,
			.policy = ThreadExecutor::Policy::Always};
	};
	check(vstore::getForeignWriteCount() == 0 && vstore::getFrozenAllocCount() == 0,
			"parallel-threads: not one write to a scene off the machine's thread, not one allocation in a frozen one");

	// ---- a block wide enough for its job to be built and taken back in parallel ---------------------
	//
	// The corpus is three branches, and three branches are never handed a phase: the machine waits for
	// a phase, so waking the pool for one costs more than doing it (ThreadExecutor::PhaseMinBranches).
	// Everything above proves the RUN on threads. This proves the two ends.
	{
		// The shapes, and why each one is here: a short body (the import is all of the cost), a long
		// one (the run is), and a body with a LOOP in it - the only shape that opens activations under
		// a branch, which is the half of an import whose destination cannot be worked out from the
		// branch count and has to be reserved one at a time.
		struct Shape {
			uint32_t bodyNodes;
			uint32_t loopTurns;
			StringView name;
		};
		const Shape shapes[] = {
			Shape{1, 0, StringView("body 1")},
			Shape{32, 0, StringView("body 32")},
			Shape{2, 6, StringView("body 2, loop 6")},
		};
		for (auto &shape : shapes) {
			const uint32_t branches = 1024;
			auto what = mem_std::toString("wide block, ", shape.name);
			auto label = [&](StringView tail) {
				return mem_std::toString("parallel-threads: ", what, ": ", tail);
			};

			RunSnapshot base;
			{
				Fixture fx;
				check(prepareSynthBlock(fx, branches, shape.bodyNodes, shape.loopTurns),
						label("the synthetic package builds"));
				auto cfg = wideConfig(fx);
				fx.frame(0.03, 0, cfg);
				snapshot(fx, base);
				check(fx.report.outcome == RunOutcome::Completed, label("the serial run completes"));
				fx.interp.reset();
			}

			ThreadExecutor executor(threads(4, 0));
			auto viewAllocs = vstore::getViewAllocCount();
			RunSnapshot snap;
			{
				Fixture fx;
				check(prepareSynthBlock(fx, branches, shape.bodyNodes, shape.loopTurns),
						label("... and again for the pool"));
				auto cfg = wideConfig(fx);
				cfg.executor = &executor;
				check(fx.frameBegin(0.03, 0, cfg) == Status::Ok && finishThreaded(fx, executor)
								&& fx.report.outcome == RunOutcome::Completed,
						label("the run on threads completes"));
				snapshot(fx, snap);
				fx.interp.reset();
			}
			check(oracle::compareRuns(base, snap, perActivation(), what),
					label("and it is the serial run, per activation"));

			// Two phases a block, and a block a frame: the number says the wide road was taken, and
			// says it without timing anything.
			check(executor.getPhaseHandoffs() == 2, label("its job was built and taken back on the pool"));
			check(vstore::getViewAllocCount() == viewAllocs,
					label("not one allocation was asked for through a write view"));
		}
		check(vstore::getForeignWriteCount() == 0 && vstore::getFrozenAllocCount() == 0,
				"parallel-threads: the wide block wrote nothing off its own thread either");
	}

	// ---- two blocks in flight at once ----------------------------------------------------------------

	{
		Fixture serial;
		check(prepareGraph(serial, twoBlocksGraph()), "parallel-threads: the two-block graph prepares");
		serial.frame(0.03, 0, serial.sceneConfig());
		RunSnapshot base;
		snapshot(serial, base);

		ThreadExecutor executor(threads(2));
		RunSnapshot snap;
		{
			Fixture fx;
			prepareGraph(fx, twoBlocksGraph());
			s_gateFrom = 0;
			s_gateOpen = false;
			auto cfg = fx.sceneConfig();
			cfg.executor = &executor;
			fx.frameBegin(0.03, 0, cfg);
			while (fx.interp.stepOnce(fx.report)) { }
			check(fx.interp.getState() == RunState::Suspended && fx.interp.hasBlocksInFlight()
							&& executor.getMaxInFlight() == 2,
					"parallel-threads: both blocks are out at once, and the machine waits for them");
			s_gateOpen = true;
			check(finishThreaded(fx, executor) && fx.report.outcome == RunOutcome::Completed,
					"parallel-threads: released, the run completes");
			snapshot(fx, snap);
			fx.interp.reset();
		}
		check(oracle::compareRuns(base, snap, perActivation(), "two blocks"),
				"parallel-threads: and it is the serial run, per activation");
	}

	// ---- a unit that would move the scene waits for the branches reading it -------------------------

	{
		Fixture serial;
		check(prepareGraph(serial, freezeGraph()), "parallel-threads: the freeze graph prepares");
		serial.frame(0.03, 0, serial.sceneConfig());
		RunSnapshot base;
		snapshot(serial, base);

		ThreadExecutor executor(threads(2));
		RunSnapshot snap;
		{
			Fixture fx;
			prepareGraph(fx, freezeGraph());
			s_gateOpen = false;
			auto cfg = fx.sceneConfig();
			cfg.executor = &executor;
			fx.frameBegin(0.03, 0, cfg);
			while (fx.interp.stepOnce(fx.report)) { }
			uint32_t node = 0;
			uint32_t act = 0;
			check(fx.interp.getState() == RunState::Suspended && fx.interp.getLocal().peekReady(node, act)
							&& fx.graph.getNodeAt(node).id == 31,
					"parallel-threads: addComponent waits on the front while the block is out");
			s_gateOpen = true;
			check(finishThreaded(fx, executor) && fx.report.outcome == RunOutcome::Completed,
					"parallel-threads: delivered, it runs");
			snapshot(fx, snap);
			fx.interp.reset();
		}
		check(oracle::compareRuns(base, snap, perActivation(), "freeze"),
				"parallel-threads: and the run is the serial one");
		check(vstore::getFrozenAllocCount() == 0, "parallel-threads: nothing allocated in the frozen scene");
	}

	// ---- a timeout breaks a block off -----------------------------------------------------------------

	for (auto policy : {StringView("partial"), StringView("cancelFrame")}) {
		ThreadExecutor executor(threads(1));
		TestClock clock;
		Fixture fx;
		prepareGraph(fx, timeoutGraph(policy));
		s_gateFrom = 1;
		s_gateOpen = false;
		auto cfg = fx.sceneConfig();
		cfg.executor = &executor;
		cfg.clock = &clock;
		fx.frameBegin(0.03, 0, cfg);
		while (fx.interp.stepOnce(fx.report)) { }
		pause(200); // branch 0 goes through the gate and closes
		clock.now += 100;
		sprt::thread opener([] {
			pause(50);
			s_gateOpen = true;
		});
		bool finished = finishThreaded(fx, executor);
		opener.join();
		auto label = mem_std::toString("parallel-threads: timeout under ", policy, ": ");
		check(finished && hasCode(fx.report, StringView("parallel-block-timeout")),
				mem_std::toString(label, "the block is broken off"));
		if (policy == StringView("partial")) {
			check(fx.report.outcome == RunOutcome::Completed && unitHp(fx, 0) == 0
							&& unitHp(fx, 1) == 20 && unitHp(fx, 2) == 30,
					mem_std::toString(label, "the branch that closed is committed, the others are not (",
							unitHp(fx, 0), " ", unitHp(fx, 1), " ", unitHp(fx, 2), ")"));
		} else {
			check(fx.report.outcome == RunOutcome::Cancelled, mem_std::toString(label, "the frame is cancelled"));
		}
		fx.interp.reset();
		s_gateFrom = 0;
		s_gateOpen = true;
	}

	// ---- back through a delivery, and the branches go out again ---------------------------------------

	{
		auto &c = ParallelCases[0];
		Fixture serial;
		prepareCase(serial, c, false);
		serial.frame(c.dt, 0, config(serial, c));
		RunSnapshot base;
		snapshot(serial, base);

		ThreadExecutor executor(threads(2));
		RunSnapshot snap;
		bool relaunched = false;
		{
			Fixture fx;
			prepareCase(fx, c, true);
			auto cfg = configFor(fx, c, true, RollbackQuantum::Step);
			cfg.executor = &executor;
			fx.frameBegin(c.dt, 0, cfg);
			finishThreaded(fx, executor);
			while (!fx.report.log.empty() && fx.report.log.back().kind != RunStepKind::Deliver) {
				fx.interp.stepBack(fx.report);
			}
			fx.interp.stepBack(fx.report);
			relaunched = fx.interp.hasBlocksInFlight();
			check(finishThreaded(fx, executor) && fx.report.outcome == RunOutcome::Completed,
					"parallel-threads: stepped back through the delivery, the run completes again");
			snapshot(fx, snap);
			fx.interp.reset();
		}
		check(relaunched, "parallel-threads: the block's branches went out again");
		check(oracle::compareRuns(base, snap, perActivation(), "rollback"),
				"parallel-threads: and the run is the serial one");
	}

	// ---- the owner of a store ------------------------------------------------------------------------

	check(vstore::getForeignWriteCount() == 0, "parallel-threads: every run above wrote its scene on its own thread");
	{
		vstore::setArenaViolationsFatal(false);
		vstore::ShadowArena arena;
		arena.init();
		auto addr = arena.alloc(16);
		arena.setOwnerThread();
		auto before = vstore::getForeignWriteCount();
		sprt::thread other([&] { arena.write(addr, 4); });
		other.join();
		check(vstore::getForeignWriteCount() == before + 1,
				"parallel-threads: a write from another thread is caught");

		vstore::ShadowArena view;
		view.initView(arena);
		view.write(addr, 4);
		check(vstore::getForeignWriteCount() == before + 2,
				"parallel-threads: so is a write through a view");

		auto frozen = vstore::getFrozenAllocCount();
		arena.setFrozen(true);
		arena.alloc(16);
		arena.setFrozen(false);
		check(vstore::getFrozenAllocCount() == frozen + 1,
				"parallel-threads: and an allocation in a frozen store");
		vstore::setArenaViolationsFatal(true);
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
