// A block run on a device. The device here is the CPU evaluator (gpu_fake_device.h), so what this
// section holds is the EXECUTOR and the machine's half of it - the load, the records taken back, the
// log and the diagnostics a dispatch leaves, the folds of the collectors, and every way a block can
// fail to come back: a timeout, a silence, a lost device, a refusal, a rollback.
//
// The run is compared with serial under level C: the same units per activation, the same
// diagnostics, the records and the scene under the tolerance a real value carries.
//
// The shaders the host attaches are keyed by the text the lowering writes today, with a payload the
// evaluator never reads: the studio proves the same contract against the SPIR-V its build compiled
// from the committed text, and a device harness proves what the payload computes.

#include "executors_fixture.h"
#include "executors_corpus.h"
#include "gpu_fake_device.h"

#include "SPFlowExecutorChoice.h"
#include "SPFlowExecutorGpu.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::execfx;
using namespace stappler::test::execfx::corpus;
namespace oracle = stappler::test::oracle;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;
using Device = stappler::test::gpufake::CpuGpuDevice<flow::RuntimeGraph>;
using stappler::test::gpufake::Mode;

// The suite-wide `RunConfig` names the kernel's own environment (tests.h); a section with a scene
// runs in TestEnv's, so it names its own.
using RunConfig = Fixture::ConfigType;

void snapshot(Fixture &fx, RunSnapshot &out) {
	oracle::takeSnapshot(fx.graph, fx.interp, fx.arena, fx.report, out, &fx.scene, &fx.sceneArena,
			true);
	oracle::dropUnpublishedBodyRecords(fx.graph, out.pathRecords);
	oracle::dropUnpublishedBodyTrace(fx.graph, out);
}

// Level C, per activation: what a device may differ in, and nothing more.
CompareOptions levelC() {
	CompareOptions opts{Level::Tolerant, false, true};
	opts.order = CompareOptions::Order::PerActivation;
	return opts;
}

// The shaders the host would hand over: a block's body by the hash of the text it lowers to today,
// and every reducer of the library by name and text. The payload is one word the evaluator does not
// read - what a real host compiles into it is its own business, and the fake device keys its
// pipelines by the hash alone. The names outlive the vector: a reducer's shader is matched by name,
// and a StringView into a dead String would read whatever came to live there.
constexpr uint32_t s_placeholder[] = {1};

mem_std::Vector<flow::GpuShaderBinary> manifest(const flow::RuntimeGraph &g) {
	static mem_std::Vector<mem_std::String> names;
	names.reserve(64); // a growth would move the short names out from under the views
	mem_std::Vector<flow::GpuShaderBinary> out;
	for (uint32_t b = 0; b < g.getBlockCount(); ++b) {
		auto hash = flow::hashGpuBlock(g, b);
		if (hash != 0) {
			out.emplace_back(flow::GpuShaderBinary{StringView(), hash,
				SpanView<uint32_t>(s_placeholder, 1)});
		}
	}
	for (auto &r : flow::getGpuReducers()) {
		names.emplace_back(flow::getGpuReduceName(r.first, r.second));
		auto text = flow::writeGpuReduceGlsl(r.first, r.second);
		out.emplace_back(flow::GpuShaderBinary{StringView(names.back()),
			sprt::hash64(text.data(), text.size()),
			SpanView<uint32_t>(s_placeholder, 1)});
	}
	return out;
}

RunConfig config(Fixture &fx, const Case &c, BlockExecutor *executor, BlockClock *clock = nullptr) {
	auto cfg = configFor(fx, c, false);
	cfg.executor = executor;
	cfg.clock = clock;
	return cfg;
}

// A frame with a device in it: the machine suspends while a block is out, and the device answers when
// it is pumped. `guard` keeps a silent device from hanging the test itself.
bool runFrame(Fixture &fx, const Case &c, const RunConfig &cfg, Device &device,
		uint32_t *unitsOut = nullptr) {
	if (fx.frameBegin(c.dt, 0, cfg) != Status::Ok) {
		return false;
	}
	uint32_t units = 0;
	for (uint32_t guard = 0; guard < 100000; ++guard) {
		if (fx.interp.stepOnce(fx.report)) {
			++units;
			continue;
		}
		if (fx.interp.getState() == RunState::Suspended) {
			auto before = device.getPendingCount();
			device.update();
			if (device.getPendingCount() != before) {
				continue;
			}
		}
		break;
	}
	if (unitsOut) {
		*unitsOut = units;
	}
	return true;
}

const Case &gpuCase(StringView name) {
	for (auto &c : GpuCases) {
		if (c.name == name) {
			return c;
		}
	}
	return GpuCases[0];
}

// The report carries numbers; the check names the code the way a check reads best, by the table in
// flow_names.cpp.
uint32_t countDiag(const RunReport &report, StringView code) {
	uint32_t n = 0;
	if (report.diagnostics.isArray()) {
		for (auto &it : report.diagnostics.asArray()) {
			n += test::getDiagCodeName(it) == code ? 1 : 0;
		}
	}
	return n;
}

} // namespace

void performParallelGpuJobTests() {
	sprt::cout << "\n== flow run: a block on a device ==\n";

	// The counters are the process's and other sections add to them deliberately; what this section
	// owes is that IT adds nothing.
	auto foreignWritesBefore = vstore::getForeignWriteCount();

	// ---- what the host attaches ------------------------------------------------------------------

	{
		Fixture fx;
		auto &c = gpuCase(StringView("gpu-collectors"));
		check(prepareCase(fx, c, false), "parallel-gpu-job: the collectors case prepares");
		auto attached = fx.graph.attachGpuShaders(manifest(fx.graph));
		check(attached == 1, "parallel-gpu-job: the block of the case takes the host's shader");
		auto block = fx.graph.getGpuBlock(0);
		check(block != nullptr && !block->spirv.empty(),
				"parallel-gpu-job: ... with the payload the host attached");
		check(block && block->reducers.size() == 8,
				mem_std::toString("parallel-gpu-job: ... and the eight reducers of its collectors (",
						block ? block->reducers.size() : 0, ")"));
		check(fx.graph.getGpuShaders() && fx.graph.getGpuShaders()->getIdentity() != 0,
				"parallel-gpu-job: the table has an identity the device can key its pipelines by");
	}

	{
		// A shader of another text is not this block's, whatever it is called.
		Fixture fx;
		auto &c = gpuCase(StringView("gpu-basic"));
		prepareCase(fx, c, false);
		auto shaders = manifest(fx.graph);
		for (auto &it : shaders) {
			it.textHash ^= 1;
		}
		check(fx.graph.attachGpuShaders(shaders) == 0,
				"parallel-gpu-job: a shader whose text hash moved is refused, and the block stays on the CPU");
		check(fx.graph.getGpuBlock(0) == nullptr, "parallel-gpu-job: ... so the graph offers none");
	}

	// ---- the corpus against serial ---------------------------------------------------------------

	uint32_t ran = 0;
	for (auto &c : GpuCases) {
		auto label = [&](StringView what) {
			return mem_std::toString("parallel-gpu-job: ", c.name, ": ", what);
		};

		Fixture serial;
		check(prepareCase(serial, c, false), label("prepares"));
		serial.frame(c.dt, 0, configFor(serial, c, false));
		RunSnapshot base;
		snapshot(serial, base);

		Fixture fx;
		prepareCase(fx, c, false);
		if (fx.graph.attachGpuShaders(manifest(fx.graph)) == 0) {
			check(false, label("the host's shader is attached"));
			continue;
		}
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());
		runFrame(fx, c, config(fx, c, &executor), *device);
		RunSnapshot snap;
		snapshot(fx, snap);

		check(fx.report.outcome == c.outcome,
				label(mem_std::toString("ends ", getRunOutcomeName(c.outcome))));
		check(executor.getDispatchCount() == 1, label("one dispatch for the block"));
		if (c.outcome == RunOutcome::Completed) {
			check(oracle::compareRuns(base, snap, levelC(), label("device vs serial")),
					label("the serial run, per activation, under the level-C tolerance"));
		} else {
			check(snap.outcome == base.outcome, label("the outcome serial reached"));
		}
		fx.interp.reset();
		++ran;
	}
	check(ran == GpuCaseCount, "parallel-gpu-job: every case of the GPU corpus ran on the device");

	// ---- the folds -------------------------------------------------------------------------------

	{
		auto &c = gpuCase(StringView("gpu-collectors"));
		Fixture serial;
		prepareCase(serial, c, false);
		serial.frame(c.dt, 0, configFor(serial, c, false));
		RunSnapshot base;
		snapshot(serial, base);

		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());
		runFrame(fx, c, config(fx, c, &executor), *device);
		RunSnapshot snap;
		snapshot(fx, snap);
		check(executor.getReduceCount() == 1,
				"parallel-gpu-job: the collectors of a block fold in one call");
		check(oracle::compareRuns(base, snap, levelC(), "parallel-gpu-job: folded collectors"),
				"parallel-gpu-job: the folds are serial's, under the tolerance");
		fx.interp.reset();

		// And with the folds turned off, the collectors fold on the CPU and the run is serial's exactly.
		Fixture plain;
		prepareCase(plain, c, false);
		plain.graph.attachGpuShaders(manifest(plain.graph));
		Rc<Device> plainDevice = Rc<Device>::alloc(plain.graph);
		flow::GpuExecutor noFold(plainDevice,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always, .reducers = false});
		noFold.prepare(*plain.graph.getGpuShaders());
		runFrame(plain, c, config(plain, c, &noFold), *plainDevice);
		RunSnapshot plainSnap;
		snapshot(plain, plainSnap);
		check(noFold.getReduceCount() == 0, "parallel-gpu-job: ... none when the folds are off");
		check(oracle::compareRuns(base, plainSnap, levelC(), "parallel-gpu-job: CPU-folded collectors"),
				"parallel-gpu-job: ... and the collectors then fold in branch order, as they always did");
		plain.interp.reset();
	}

	// ---- a device that does not answer -----------------------------------------------------------

	{
		auto &c = gpuCase(StringView("gpu-basic"));
		for (auto mode : {Mode::Silent, Mode::Stopped}) {
			Fixture fx;
			prepareCase(fx, c, false);
			fx.graph.attachGpuShaders(manifest(fx.graph));
			Rc<Device> device = Rc<Device>::alloc(fx.graph);
			ManualClock clock;
			flow::GpuExecutor executor(device,
					flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always,
						.silenceMs = 50, .wallClock = &clock});
			executor.prepare(*fx.graph.getGpuShaders());
			// The block is dispatched and then nothing comes back: the device goes quiet before the
			// answer, not before the call.
			device->setMode(Mode::Silent);
			fx.frameBegin(c.dt, 0, config(fx, c, &executor, &clock));
			for (uint32_t guard = 0; guard < 1000 && fx.interp.stepOnce(fx.report); ++guard) { }
			device->setMode(mode);
			clock.advance(1000);
			for (uint32_t guard = 0; guard < 1000 && fx.interp.stepOnce(fx.report); ++guard) { }
			auto what = mode == Mode::Silent ? StringView("silent") : StringView("stopped");
			check(countDiag(fx.report, StringView("parallel-gpu-lost")) == 1,
					mem_std::toString("parallel-gpu-job: a ", what,
							" device breaks the block off, once, without hanging"));
			check(!fx.interp.hasBlocksInFlight(),
					mem_std::toString("parallel-gpu-job: ... and the run holds nothing in flight after it"));
			fx.interp.reset();
		}
	}

	{
		// A lost device: the same break, and the next frame does not go to it.
		auto &c = gpuCase(StringView("gpu-basic"));
		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());
		device->setMode(Mode::LoseNext);
		runFrame(fx, c, config(fx, c, &executor), *device);
		check(countDiag(fx.report, StringView("parallel-gpu-lost")) == 1,
				"parallel-gpu-job: a lost device breaks the block off");
		check(device->isLost(), "parallel-gpu-job: ... and the device says so");
		fx.interp.reset();

		Fixture next;
		prepareCase(next, c, false);
		next.graph.attachGpuShaders(manifest(next.graph));
		BlockLaunchInfo info;
		info.branchCount = 3;
		info.allowed = uint32_t(BlockExecutorMask::Gpu);
		info.gpu = next.graph.getGpuBlock(0);
		info.gpuTable = next.graph.getGpuShaders();
		auto launch = executor.chooseLaunch(BlockTicket{0, 0, 1}, info);
		check(launch.kind == BlockLaunchKind::Machine,
				"parallel-gpu-job: ... and a lost device takes no block after that");
		next.interp.reset();
	}

	{
		// A device that refuses the call runs the same frame on the machine's front.
		auto &c = gpuCase(StringView("gpu-basic"));
		Fixture serial;
		prepareCase(serial, c, false);
		serial.frame(c.dt, 0, configFor(serial, c, false));
		RunSnapshot base;
		snapshot(serial, base);

		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());
		device->setMode(Mode::Refuse);
		runFrame(fx, c, config(fx, c, &executor), *device);
		RunSnapshot snap;
		snapshot(fx, snap);
		check(executor.getDispatchCount() == 0, "parallel-gpu-job: a refused dispatch is not one");
		check(oracle::compareRuns(base, snap, levelC(), "parallel-gpu-job: refused, on the front"),
				"parallel-gpu-job: a block the device refuses runs on the front and lands where serial did");
		fx.interp.reset();
	}

	// ---- broken off by its own timeout, under the three policies -----------------------------------

	{
		// The same body as gpu-basic, with a timeout of its own and a device that holds the answer.
		auto gpuTimeoutGraph = [](StringView policy) {
			return parGraph(R"(
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "par.forEachWith", "settings": {"with": ["par.Unit"], "executors": ["gpu"], "onFailure": ")"
							+ mem_std::String(policy.data(), policy.size())
							+ R"(", "timeout": 50}},
		{"id": 3, "op": "scene.getInt32", "params": {"component": "par.Unit", "field": "hp"}},
		{"id": 4, "op": "math.addInt32"},
		{"id": 5, "op": "scene.setInt32", "params": {"component": "par.Unit", "field": "hp"}},
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
		{"kind": "data", "from": 4, "fromPin": "result", "to": 5, "toPin": "value"},
		{"kind": "data", "from": 4, "fromPin": "result", "to": 7, "toPin": "value"})");
		};

		for (auto policy : {StringView("cancelFrame"), StringView("partial"), StringView("nothing")}) {
			Fixture fx;
			fx.extraOps = &registerParOps;
			check(fx.prepareScene(&registerParTypes) && makeParScene(fx)
							&& fx.prepare(gpuTimeoutGraph(policy)),
					mem_std::toString("parallel-gpu-job: ", policy, ": the timed block prepares"));
			fx.graph.attachGpuShaders(manifest(fx.graph));
			Rc<Device> device = Rc<Device>::alloc(fx.graph);
			ManualClock clock;
			flow::GpuExecutor executor(device,
					flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always,
						.silenceMs = 0, .wallClock = &clock});
			executor.prepare(*fx.graph.getGpuShaders());
			device->setMode(Mode::Hold);

			auto cfg = fx.sceneConfig();
			cfg.executor = &executor;
			cfg.clock = &clock;
			fx.frameBegin(0.03, 0, cfg);
			for (uint32_t guard = 0; guard < 1000 && fx.interp.stepOnce(fx.report); ++guard) { }
			check(fx.interp.hasBlocksInFlight(),
					mem_std::toString("parallel-gpu-job: ", policy, ": the block is out"));
			clock.advance(100); // past the block's own timeout
			for (uint32_t guard = 0; guard < 1000 && fx.interp.stepOnce(fx.report); ++guard) { }

			auto expected = policy == StringView("cancelFrame") ? RunOutcome::Cancelled
																: RunOutcome::Completed;
			check(fx.report.outcome == expected,
					mem_std::toString("parallel-gpu-job: ", policy, ": the run ends ",
							getRunOutcomeName(expected)));
			check(test::hasDiag(fx.report.diagnostics, StringView("parallel-block-timeout")),
					mem_std::toString("parallel-gpu-job: ", policy, ": ... with the block's timeout"));
			// Nothing was committed: the branches never closed, so no copy of theirs is anybody's.
			value::Var hp;
			fx.getField(s_parUnits[0], parUnitType(), StringView("hp"), hp);
			check(hp.i == 10,
					mem_std::toString("parallel-gpu-job: ", policy,
							": ... and a branch that was broken off commits nothing"));
			fx.interp.reset();
		}
	}

	// ---- buffers between frames ------------------------------------------------------------------

	{
		auto &c = gpuCase(StringView("gpu-basic"));
		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());

		runFrame(fx, c, config(fx, c, &executor), *device);
		fx.interp.reset();
		auto first = device->getBufferAllocations();
		check(first > 0, "parallel-gpu-job: the first frame allocates the block's buffers");
		for (uint32_t frame = 0; frame < 20; ++frame) {
			Fixture again;
			prepareCase(again, c, false);
			again.graph.attachGpuShaders(manifest(again.graph));
			// The same device, and the same shaders: the same buffers.
			runFrame(again, c, config(again, c, &executor), *device);
			again.interp.reset();
		}
		check(device->getBufferAllocations() == first,
				mem_std::toString("parallel-gpu-job: twenty frames of the same size allocate none (",
						device->getBufferAllocations(), " vs ", first, ")"));
		check(device->getLiveBufferSets() > 0, "parallel-gpu-job: ... and the set is still there");
		device->releaseBuffers();
		check(device->getLiveBufferSets() == 0,
				"parallel-gpu-job: ... until the device is told to give them up");
	}

	// ---- a rollback while the block is out -------------------------------------------------------

	{
		auto &c = gpuCase(StringView("gpu-basic"));
		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor executor(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		executor.prepare(*fx.graph.getGpuShaders());
		device->setMode(Mode::Hold);
		fx.frameBegin(c.dt, 0, config(fx, c, &executor));
		for (uint32_t guard = 0; guard < 1000 && fx.interp.stepOnce(fx.report); ++guard) { }
		check(fx.interp.hasBlocksInFlight(),
				"parallel-gpu-job: the block is out while the device holds it");
		fx.interp.reset(); // gives the flight up: the machine must not wait for an answer that never comes
		check(!fx.interp.hasBlocksInFlight(),
				"parallel-gpu-job: a reset while a block is out gives it up without hanging");
		device->release();
		device->update(); // the late answer finds the request abandoned
		check(true, "parallel-gpu-job: ... and a late answer is dropped");
	}

	// ---- who takes the block ---------------------------------------------------------------------

	{
		auto &c = gpuCase(StringView("gpu-basic"));
		Fixture fx;
		prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifest(fx.graph));
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor gpu(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Author});
		gpu.prepare(*fx.graph.getGpuShaders());
		flow::ThreadExecutor threads(flow::ThreadExecutor::Options{.threads = 2, .batch = 1,
			.policy = flow::ThreadExecutor::Policy::Author});
		flow::ChoiceExecutor choice(flow::ChoiceExecutor::Options{.gpu = &gpu, .threads = &threads});

		auto ask = [&](uint32_t allowed, bool withDevice) {
			BlockLaunchInfo info;
			info.branchCount = 3;
			info.allowed = allowed;
			info.gpu = withDevice ? fx.graph.getGpuBlock(0) : nullptr;
			info.gpuTable = withDevice ? fx.graph.getGpuShaders() : nullptr;
			return choice.chooseLaunch(BlockTicket{0, 0, 1}, info).kind;
		};
		using Kind = BlockLaunchKind;
		check(ask(0, true) == Kind::Machine,
				"parallel-gpu-job: a block whose author named nothing is the model's to place, and three "
				"branches stay on the front");
		check(ask(uint32_t(BlockExecutorMask::Threads), true) == Kind::Threads,
				"parallel-gpu-job: one that named threads goes to them, device or no device");
		check(ask(uint32_t(BlockExecutorMask::Gpu), true) == Kind::Gpu,
				"parallel-gpu-job: one that named the GPU goes to it when a device is there");
		check(ask(uint32_t(BlockExecutorMask::Gpu), false) == Kind::Machine,
				"parallel-gpu-job: ... and serially when there is none");
		// Two names are a question, not a demand, and three branches of two operations are not worth
		// waking a pool for - the model says so, and `parallel-heuristic` says where it stops saying so.
		check(ask(uint32_t(BlockExecutorMask::Gpu) | uint32_t(BlockExecutorMask::Threads), false)
						== Kind::Machine,
				"parallel-gpu-job: one that named both, with no device and three branches, stays on the front");
		fx.interp.reset();
	}

	check(vstore::getForeignWriteCount() == foreignWritesBefore,
			"parallel-gpu-job: not one write to a scene off the machine's thread");
}

} // namespace STAPPLER_VERSIONIZED stappler
