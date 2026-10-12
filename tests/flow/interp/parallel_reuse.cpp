// What a block's job keeps between launches.
//
// A branch job holds a whole machine PER BATCH - an arena, the memory pool that arena cut from, the
// frame layout, the seeded store - and without a pool every one of them is built when the block
// opens and destroyed when it is delivered, every frame, forever.
//
// So the machine keeps a small pool of jobs, and this section is what says the keeping is honest:
//
//   1. A block that runs every frame builds its job ONCE. The counters say so; nothing is timed.
//   2. A run that reuses a job is the run that built a new one - the same frames, the same scene.
//   3. A job taken back from a CANCELLED launch comes back usable. `BranchJob::_cancel` is set from a
//      worker's thread and was never cleared by anything before the pool, so a recycled job that
//      kept it would run no branches at all and quietly deliver an empty block.
//   4. The pool outlives reset(), which is the whole point: a host resets the engine at the end of
//      every tick, so a pool released there would be a pool that never saw a second frame.

#include "executors_fixture.h"
#include "executors_corpus.h"

#include "SPFlowExecutorThreads.h"

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

using flow::ThreadExecutor;

// The suite-wide `RunConfig` names the kernel's own environment (tests.h); a section with a scene
// runs in TestEnv's, so it names its own.
using RunConfig = Fixture::ConfigType;

// Wide enough that the job's phases go to the pool as well (ThreadExecutor::PhaseMinBranches), which
// is the arrangement the reuse has to hold under.
constexpr uint32_t Branches = 1024;
constexpr uint32_t Frames = 5;

// The synthetic package, as parallel-threads builds it. Repeated rather than shared for the reason
// the corpus constants are: what has to be identical is the GRAPH, and a helper that two sections
// reached through would tie them to each other instead.
bool prepareSynthBlock(Fixture &fx, uint32_t branches) {
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
	synth::BlockConfig config;
	config.bodyNodes = 2;
	return fx.prepare(synth::makeBlockGraph(config));
}

RunConfig wideConfig(Fixture &fx) {
	auto cfg = fx.sceneConfig();
	cfg.maxActivations = 20'064;
	cfg.maxSteps = 200'000;
	return cfg;
}

bool finishThreaded(Fixture &fx, ThreadExecutor &executor) {
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

ThreadExecutor::Options fourThreads() {
	return ThreadExecutor::Options{
		.threads = 4,
		.batch = 0,
		.policy = ThreadExecutor::Policy::Always,
	};
}

} // namespace

void performParallelReuseTests() {
	sprt::cout << "\n== flow run: what a block's job keeps ==\n";

	// Deltas, not absolutes: these counters are one per process, and the sections that come before
	// raise them on purpose - the ownership check in `parallel-threads` catches a foreign write on
	// purpose, the view in the same section allocates on purpose.
	const auto foreignBefore = vstore::getForeignWriteCount();
	const auto frozenBefore = vstore::getFrozenAllocCount();
	const auto viewBefore = vstore::getViewAllocCount();

	// ---- the same block, five frames, one job ----------------------------------------------------

	mem_std::Vector<uint8_t> serialScene;
	{
		Fixture fx;
		check(prepareSynthBlock(fx, Branches), "parallel-reuse: the synthetic package builds");
		auto cfg = wideConfig(fx);
		bool ok = true;
		for (uint32_t i = 0; i < Frames; ++i) {
			ok = ok && fx.frame(0.03, i, cfg) == Status::Ok
					&& fx.report.outcome == RunOutcome::Completed;
		}
		check(ok, "parallel-reuse: five frames on the machine's own front");
		serialScene = oracle::imageOf(fx.sceneArena);
		check(fx.interp.getJobsCreated() == 0,
				"parallel-reuse: a run with no executor builds no job at all");
		fx.interp.reset();
	}

	{
		ThreadExecutor executor(fourThreads());
		Fixture fx;
		check(prepareSynthBlock(fx, Branches), "parallel-reuse: ... and again for the pool");
		auto cfg = wideConfig(fx);
		cfg.executor = &executor;
		bool ok = true;
		for (uint32_t i = 0; i < Frames; ++i) {
			ok = ok && fx.frameBegin(0.03, i, cfg) == Status::Ok && finishThreaded(fx, executor)
					&& fx.report.outcome == RunOutcome::Completed;
		}
		check(ok, "parallel-reuse: five frames on threads");
		check(oracle::imageOf(fx.sceneArena) == serialScene,
				"parallel-reuse: and the scene is the one the front left, byte for byte");

		check(fx.interp.getJobsCreated() == 1 && fx.interp.getJobsReused() == Frames - 1,
				mem_std::toString("parallel-reuse: one job built and ", Frames - 1,
						" taken back out of the pool (built ", fx.interp.getJobsCreated(),
						", reused ", fx.interp.getJobsReused(), ")"));
		fx.interp.reset();
	}

	// ---- a job taken back from a launch that was cancelled ---------------------------------------
	//
	// reset() abandons the flight: the job is cancelled, waited for, and given back. A job reused
	// after one that kept its cancel would run no branch of its next block and deliver it empty -
	// which the scene below would show as the first frame's numbers.
	{
		ThreadExecutor executor(fourThreads());
		Fixture fx;
		check(prepareSynthBlock(fx, Branches), "parallel-reuse: a package to break off mid-flight");
		auto cfg = wideConfig(fx);
		cfg.executor = &executor;

		check(fx.frameBegin(0.03, 0, cfg) == Status::Ok, "parallel-reuse: the frame begins");
		while (fx.interp.getState() != RunState::Suspended && fx.interp.stepOnce(fx.report)) { }
		check(fx.interp.getState() == RunState::Suspended && fx.interp.hasBlocksInFlight(),
				"parallel-reuse: its block is out");
		fx.interp.reset(); // abandons the flight, and the job comes back to the pool
		check(fx.interp.getJobsCreated() == 1,
				"parallel-reuse: the abandoned job was kept, not destroyed");

		// The scene is where the broken-off frame left it, so this is a fresh comparison rather than
		// the five-frame one: what matters is that the block RUNS.
		bool ok = fx.frameBegin(0.03, 1, cfg) == Status::Ok && finishThreaded(fx, executor)
				&& fx.report.outcome == RunOutcome::Completed;
		check(ok, "parallel-reuse: the next frame runs on the job it got back");
		check(fx.interp.getJobsReused() == 1,
				"parallel-reuse: ... and it is the same job, out of the pool");

		auto reused = oracle::imageOf(fx.sceneArena);
		Fixture fresh;
		check(prepareSynthBlock(fresh, Branches), "parallel-reuse: a package that was never broken");
		auto freshCfg = wideConfig(fresh);
		check(fresh.frame(0.03, 1, freshCfg) == Status::Ok
						&& fresh.report.outcome == RunOutcome::Completed,
				"parallel-reuse: which runs once on the front");
		check(reused == oracle::imageOf(fresh.sceneArena),
				"parallel-reuse: and the reused job left the scene the front leaves - every branch ran");
		fx.interp.reset();
		fresh.interp.reset();
	}

	check(vstore::getForeignWriteCount() == foreignBefore
					&& vstore::getFrozenAllocCount() == frozenBefore
					&& vstore::getViewAllocCount() == viewBefore,
			"parallel-reuse: nothing wrote off its own thread, nothing allocated through a view");
}

} // namespace STAPPLER_VERSIONIZED stappler
