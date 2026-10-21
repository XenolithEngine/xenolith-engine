// Who takes a block, and how long it is given.
//
// The choice is a cost model, not an order (flow::ExecutorModel), and a model is a thing that can be
// asked on paper: every check below hands `chooseLaunch` a `BlockLaunchInfo` it built itself, with a
// table of costs it wrote itself, and reads the answer. Nothing here is timed - a section that
// decided whether a heuristic was right by running a stopwatch would be measuring this machine's
// load.
//
// What IS run is the last part: the same block under all three answers, held to the same run. A
// model that changed what a frame computes would not be a heuristic, it would be a bug.

#include "executors_fixture.h"
#include "executors_corpus.h"
#include "gpu_fake_device.h"

#include "SPFlowExecutorChoice.h"
#include "SPFlowExecutorGpu.h"
#include "SPFlowExecutorModel.h"
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

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;
using Device = stappler::test::gpufake::CpuGpuDevice<flow::RuntimeGraph>;
using Kind = flow::BlockLaunchKind;
using Mask = flow::BlockExecutorMask;

// The suite-wide `RunConfig` names the kernel's own environment (tests.h); a section with a scene
// runs in TestEnv's, so it names its own.
using RunConfig = Fixture::ConfigType;

// The numbers the reference host measured, as the model carries them. Written out here rather than
// taken from `getDefaultCosts()`: a section that read the same table it is asserting about would
// pass whatever the table said.
flow::ExecutorCosts reference() {
	flow::ExecutorCosts costs;
	costs.machineStepNs = 865;
	costs.threadStepNs = 196;
	costs.threadBranchNs = 841;
	costs.poolBlockNs = 11'400;
	costs.threadGrowthNs = 0.0118;
	costs.gpuBlockNs = 2'000'000;
	costs.gpuBranchNs = 1'285;
	return costs;
}

// The body the reference host measured: a read, an add, a write - four units of work with the
// branch's own close, which is what every number in `reference()` is per.
constexpr uint32_t MeasuredBody = 4;

BlockLaunchInfo makeInfo(uint32_t branches, uint32_t allowed, uint32_t bodyNodes,
		const flow::GpuBlockShaders *gpu = nullptr,
		const flow::GpuShaderTable *table = nullptr) {
	BlockLaunchInfo info;
	info.branchCount = branches;
	info.allowed = allowed;
	info.gpu = gpu;
	info.gpuTable = table;
	info.shape.block = 0;
	info.shape.bodyNodes = bodyNodes;
	info.shape.writeCount = 1;
	info.shape.rowBytes = 5;
	info.shape.maxSteps = bodyNodes * 4 + 16;
	return info;
}

constexpr uint32_t s_placeholder[] = {1};

mem_std::Vector<flow::GpuShaderBinary> manifest(const flow::RuntimeGraph &g) {
	// The names outlive the vector: a reducer's shader is matched by name, and a StringView into a
	// dead String would read whatever came to live there.
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

void snapshot(Fixture &fx, RunSnapshot &out) {
	oracle::takeSnapshot(fx.graph, fx.interp, fx.arena, fx.report, out, &fx.scene, &fx.sceneArena,
			true);
	oracle::dropUnpublishedBodyRecords(fx.graph, out.pathRecords);
	oracle::dropUnpublishedBodyTrace(fx.graph, out);
}

// `gpu-basic`, with its `executors` list taken out: the author now names nothing, so the model
// arbitrates - and the shader still fits, because the GLSL a body lowers to does not depend on who
// was asked to run it. That is what lets one case be run by all three.
mem_std::String openGpuBasic(Fixture &fx, uint32_t extraUnits = 0) {
	auto &c = *sprt::find_if(GpuCases.begin(), GpuCases.end(),
			[](const Case &it) { return it.name == StringView("gpu-basic"); });
	if (!fx.prepareScene(c.sceneTypes)) {
		return mem_std::String();
	}
	fx.extraOps = c.extraOps;
	auto json = c.build(fx);
	// The corpus scene is three units, which no model would hand to anybody; a deadline is only asked
	// for a block that went somewhere, so the width is a parameter here.
	for (uint32_t i = 0; i < extraUnits; ++i) {
		auto id = fx.createEntity();
		if (fx.addComponent(id, parUnitType()) == flow::NullAddr
				|| fx.setField(id, parUnitType(), StringView("hp"),
						   value::makeInt32(int32_t(10 + i % 97)))
						!= Status::Ok) {
			return mem_std::String();
		}
	}
	StringView named(", \"executors\": [\"gpu\"]");
	auto at = json.find(named.data());
	if (at != mem_std::String::npos) {
		json = json.replace(at, named.size(), "");
	}
	return json;
}

// The synthetic package: one block over N units of `synth.Unit`.
bool prepareSynth(Fixture &fx, uint32_t branches, StringView executors, uint32_t bodyNodes = 1) {
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
	config.bodyNodes = bodyNodes;
	config.executors = executors;
	return fx.prepare(synth::makeBlockGraph(config));
}

} // namespace

void performParallelHeuristicTests() {
	sprt::cout << "\n== flow run: who takes a block ==\n";

	Fixture shaderFx;
	prepareCase(shaderFx, GpuCases[0], false);
	shaderFx.graph.attachGpuShaders(manifest(shaderFx.graph));
	auto shaders = shaderFx.graph.getGpuBlock(0);
	auto table = shaderFx.graph.getGpuShaders();
	check(shaders != nullptr, "parallel-heuristic: a lowered block with a shader, to offer the model");

	// ---- what the model forecasts ----------------------------------------------------------------

	{
		flow::ExecutorModel model;
		model.setCosts(reference());
		auto forecast = [&](Kind kind, uint32_t n, uint32_t body = MeasuredBody) {
			return model.forecastNs(kind, makeInfo(n, 0, body), 1024);
		};
		// A branch of this body is four units of work, and the machine's cost is all of it: the row is
		// flat in N, which is what the benchmark prints.
		check(forecast(Kind::Machine, 1000) / 1000 == forecast(Kind::Machine, 1) * 1000 / 1000,
				"parallel-heuristic: the machine's forecast is linear in the branches");
		check(forecast(Kind::Machine, 1000, 8) > forecast(Kind::Machine, 1000, MeasuredBody),
				"parallel-heuristic: ... and grows with the body");
		// The pool is a fixed cost first and a per-branch cost after it.
		check(forecast(Kind::Threads, 1) > forecast(Kind::Machine, 1),
				"parallel-heuristic: one branch on a pool costs more than one branch on the front");
		check(forecast(Kind::Threads, 1000) < forecast(Kind::Machine, 1000),
				"parallel-heuristic: a thousand of them cost less");
		// The growth term is still there - a batch's store costs more the more of it there is - but
		// it does not overtake the front inside the measured range: the crossing is out past a
		// hundred and fifty thousand branches, where nothing has been measured.
		check(forecast(Kind::Threads, 100'000) < forecast(Kind::Machine, 100'000),
				"parallel-heuristic: a hundred thousand are still the pool's");
		check(forecast(Kind::Threads, 100'000) / 100'000 > forecast(Kind::Threads, 1000) / 1000,
				"parallel-heuristic: ... but a branch of them costs more than a branch of a thousand");
		// The device is a round trip first and almost nothing after it.
		check(forecast(Kind::Gpu, 100) > forecast(Kind::Machine, 100),
				"parallel-heuristic: a device is not worth a hundred branches");
		check(forecast(Kind::Gpu, 100'000) < forecast(Kind::Machine, 100'000) / 2,
				"parallel-heuristic: ... and is worth a hundred thousand twice over");
	}

	// ---- the choice, over fixed inputs -----------------------------------------------------------

	{
		Rc<Device> device = Rc<Device>::alloc(shaderFx.graph);
		flow::GpuExecutor gpu(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		gpu.prepare(*table);
		flow::ThreadExecutor threads(flow::ThreadExecutor::Options{.threads = 4, .batch = 0,
			.policy = flow::ThreadExecutor::Policy::Always});
		flow::ChoiceExecutor choice(flow::ChoiceExecutor::Options{&gpu, &threads, nullptr});
		choice.getModel().setCosts(reference());
		choice.getModel().setHysteresis(1); // the table is asked, not the cache

		auto ask = [&](uint32_t n, uint32_t allowed, bool withDevice) {
			choice.forgetChoices();
			auto info = makeInfo(n, allowed, MeasuredBody, withDevice ? shaders : nullptr,
					withDevice ? table : nullptr);
			return choice.chooseLaunch(BlockTicket{0, 0, 1}, info).kind;
		};

		// An author who named nothing: the model places it, and the two crossings are where the
		// measurements put them.
		check(ask(1, 0, true) == Kind::Machine, "parallel-heuristic: one branch stays on the front");
		check(ask(100, 0, false) == Kind::Threads,
				"parallel-heuristic: a hundred go to the pool when there is no device");
		check(ask(1000, 0, true) == Kind::Threads,
				"parallel-heuristic: a thousand are still the pool's, device or no device");
		check(ask(10'000, 0, true) == Kind::Gpu,
				"parallel-heuristic: ten thousand are the device's - the round trip is paid off by then");
		check(ask(10'000, 0, false) == Kind::Threads,
				"parallel-heuristic: ... and the pool's when there is no device");
		check(ask(100'000, 0, false) == Kind::Threads,
				"parallel-heuristic: a hundred thousand stay with the pool - it does not lose to the front");
		check(ask(100'000, 0, true) == Kind::Gpu,
				"parallel-heuristic: ... and to the device when there is one");

		// A single name is a demand: three branches go to the executor the author asked for, and the
		// model is not consulted.
		check(ask(3, uint32_t(Mask::Gpu), true) == Kind::Gpu,
				"parallel-heuristic: one named executor is a demand, not a hint");
		check(ask(3, uint32_t(Mask::Threads), false) == Kind::Threads,
				"parallel-heuristic: ... whichever one it is");
		check(ask(3, uint32_t(Mask::Gpu), false) == Kind::Machine,
				"parallel-heuristic: a demand that cannot be met falls to the front");

		// Two names are a question.
		auto both = uint32_t(Mask::Gpu) | uint32_t(Mask::Threads);
		check(ask(3, both, true) == Kind::Machine,
				"parallel-heuristic: two named, three branches: neither is worth it");
		check(ask(10'000, both, true) == Kind::Gpu,
				"parallel-heuristic: two named, ten thousand branches: the device");
		check(ask(10'000, both, false) == Kind::Threads,
				"parallel-heuristic: ... the pool, with no device");
	}

	// ---- the decision is held (hysteresis) -------------------------------------------------------

	{
		Rc<Device> device = Rc<Device>::alloc(shaderFx.graph);
		flow::GpuExecutor gpu(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		gpu.prepare(*table);
		flow::ThreadExecutor threads(flow::ThreadExecutor::Options{.threads = 4, .batch = 0,
			.policy = flow::ThreadExecutor::Policy::Always});
		flow::ChoiceExecutor choice(flow::ChoiceExecutor::Options{&gpu, &threads, nullptr});
		choice.getModel().setCosts(reference());
		choice.getModel().setHysteresis(4);

		auto ask = [&](uint32_t n, bool withDevice) {
			auto info = makeInfo(n, 0, MeasuredBody, withDevice ? shaders : nullptr,
					withDevice ? table : nullptr);
			return choice.chooseLaunch(BlockTicket{0, 0, 1}, info).kind;
		};
		auto decisions = [&] {
			auto entry = choice.getModel().getEntry(0);
			return entry ? entry->decisions : 0u;
		};

		auto first = ask(10'000, true);
		check(decisions() == 1 && first == Kind::Gpu, "parallel-heuristic: the first launch decides");
		auto kind = ask(20'000, true);
		check(decisions() == 1 && kind == Kind::Gpu,
				"parallel-heuristic: N that doubled is the same question, and the answer is held");
		ask(2500, true);
		check(decisions() == 1, "parallel-heuristic: ... and so is N that quartered");
		ask(1000, true);
		check(decisions() == 2, "parallel-heuristic: N ten times smaller is asked again");
		auto before = decisions();
		ask(1000, false); // the device is gone: another question at the same N
		check(decisions() == before + 1,
				"parallel-heuristic: a device that went reopens the question at the same N");
		choice.forgetChoices();
		check(choice.getModel().getEntry(0) == nullptr,
				"parallel-heuristic: a graph that was replaced forgets what was decided about its blocks");
	}

	// ---- the default deadline --------------------------------------------------------------------

	{
		flow::ExecutorModel model;
		model.setCosts(reference());
		model.setTimeoutRule(8.0, 250);
		auto info = makeInfo(100'000, 0, MeasuredBody);
		check(model.timeoutMs(Kind::Machine, info) == 0,
				"parallel-heuristic: a block on the machine's own front gets no wall-clock deadline");
		check(model.timeoutMs(Kind::Gpu, info) > model.getTimeoutFloorMs(),
				"parallel-heuristic: a big block's deadline is its forecast, not the floor");
		check(model.timeoutMs(Kind::Gpu, makeInfo(10, 0, MeasuredBody)) == model.getTimeoutFloorMs(),
				"parallel-heuristic: a small block's deadline is the floor, not its forecast");
		auto forecastMs = model.forecastNs(Kind::Gpu, info) / 1'000'000.0;
		auto deadline = double(model.timeoutMs(Kind::Gpu, info));
		check(deadline >= forecastMs * 8.0 - 1.0 && deadline <= forecastMs * 8.0 + 1.0,
				"parallel-heuristic: and it is eight times the forecast");
	}

	{
		// On a run: a device that holds the block, no `timeout` in the graph, and the deadline the
		// model gave is the one that bites - at that millisecond and not before.
		Fixture fx;
		auto json = openGpuBasic(fx, 20'000);
		check(!json.empty() && fx.prepare(json), "parallel-heuristic: the open block prepares");
		check(fx.graph.attachGpuShaders(manifest(fx.graph)) == 1,
				"parallel-heuristic: ... and its shader still fits, with no executor named");
		Rc<Device> device = Rc<Device>::alloc(fx.graph);
		flow::GpuExecutor gpu(device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always,
					.silenceMs = 0});
		flow::ThreadExecutor threads(flow::ThreadExecutor::Options{.threads = 2, .batch = 0,
			.policy = flow::ThreadExecutor::Policy::Always});
		flow::ChoiceExecutor choice(flow::ChoiceExecutor::Options{&gpu, &threads, nullptr});
		choice.getModel().setCosts(reference());
		if (auto attached = fx.graph.getGpuShaders()) {
			gpu.prepare(*attached);
		}
		device->setMode(stappler::test::gpufake::Mode::Hold);

		ManualClock clock;
		auto cfg = fx.sceneConfig();
		cfg.executor = &choice;
		cfg.clock = &clock;
		cfg.maxActivations = 20'064;
		cfg.maxSteps = 200'000;
		fx.frameBegin(0.03, 0, cfg);
		for (uint32_t guard = 0; guard < 10000 && fx.interp.stepOnce(fx.report); ++guard) { }
		check(fx.interp.hasBlocksInFlight(), "parallel-heuristic: the block is out on the device");

		auto deadline = choice.getModel().getTimeoutFloorMs();
		clock.advance(deadline);
		for (uint32_t guard = 0; guard < 10000 && fx.interp.stepOnce(fx.report); ++guard) { }
		check(!test::hasDiag(fx.report.diagnostics, StringView("parallel-block-timeout")),
				"parallel-heuristic: not overtaken at the deadline itself");
		clock.advance(2);
		for (uint32_t guard = 0; guard < 10000 && fx.interp.stepOnce(fx.report); ++guard) { }
		check(test::hasDiag(fx.report.diagnostics, StringView("parallel-block-timeout")),
				"parallel-heuristic: and broken off one millisecond past it, with no `timeout` in the graph");
		// The numbers form of the entry: the locus values of a timeout are the node, the effective
		// milliseconds and whether the executor's default stood in for the author's own.
		bool effective = false;
		if (fx.report.diagnostics.isArray()) {
			for (auto &it : fx.report.diagnostics.asArray()) {
				if (test::getDiagCodeName(it) != StringView("parallel-block-timeout")) {
					continue;
				}
				uint32_t index = 0;
				for (auto &v : it.getValue("at").asArray()) {
					if (index == 1) {
						effective = v.getInteger() == int64_t(deadline);
					} else if (index == 2) {
						effective = effective && v.getInteger() == 1;
					}
					++index;
				}
			}
		}
		check(effective,
				"parallel-heuristic: the diagnostic carries the effective number, and says it was a default");
		fx.interp.reset();
	}

	// ---- whoever was chosen, the run is the same one ---------------------------------------------

	{
		Fixture serial;
		auto json = openGpuBasic(serial);
		check(!json.empty() && serial.prepare(json), "parallel-heuristic: the open block prepares");
		serial.frame(0.03, 0, serial.sceneConfig());
		RunSnapshot base;
		snapshot(serial, base);

		// Three tables, each of which makes one executor the cheapest, over the same block.
		struct Arm {
			StringView name;
			Kind kind;
		};
		const Arm arms[] = {
			{StringView("the front"), Kind::Machine},
			{StringView("the pool"), Kind::Threads},
			{StringView("the device"), Kind::Gpu},
		};
		for (auto &arm : arms) {
			Fixture fx;
			auto armJson = openGpuBasic(fx);
			if (armJson.empty() || !fx.prepare(armJson)) {
				check(false, "parallel-heuristic: the arm's block prepares");
				continue;
			}
			fx.graph.attachGpuShaders(manifest(fx.graph));
			Rc<Device> device = Rc<Device>::alloc(fx.graph);
			flow::GpuExecutor gpu(device,
					flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
			flow::ThreadExecutor threads(flow::ThreadExecutor::Options{.threads = 2, .batch = 1,
				.policy = flow::ThreadExecutor::Policy::Always});
			flow::ChoiceExecutor choice(flow::ChoiceExecutor::Options{&gpu, &threads, nullptr});
			if (auto attached = fx.graph.getGpuShaders()) {
				gpu.prepare(*attached);
			}

			// The cheapest arm is the one being asserted: everything else is made expensive.
			flow::ExecutorCosts costs;
			costs.machineStepNs = arm.kind == Kind::Machine ? 1 : 1'000'000;
			costs.threadBranchNs = arm.kind == Kind::Threads ? 1 : 1'000'000;
			costs.gpuBranchNs = arm.kind == Kind::Gpu ? 1 : 1'000'000;
			choice.getModel().setCosts(costs);

			auto cfg = fx.sceneConfig();
			cfg.executor = &choice;
			fx.frameBegin(0.03, 0, cfg);
			for (uint32_t guard = 0; guard < 100000; ++guard) {
				if (fx.interp.stepOnce(fx.report)) {
					continue;
				}
				if (fx.interp.getState() != RunState::Suspended) {
					break;
				}
				device->update();
				threads.waitDelivery(TimeInterval::milliseconds(100));
			}
			RunSnapshot snap;
			snapshot(fx, snap);

			CompareOptions opts{Level::Tolerant, false, true};
			opts.order = CompareOptions::Order::PerActivation;
			check(choice.getChoiceCount(arm.kind) > 0,
					mem_std::toString("parallel-heuristic: the model sent the block to ", arm.name));
			check(oracle::compareRuns(base, snap, opts,
						  mem_std::toString("parallel-heuristic: ", arm.name)),
					mem_std::toString("parallel-heuristic: ... and the run is the one serial made"));
			fx.interp.reset();
		}
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
