// The flow GPU executor on a WebGPU device: the WGSL lowering of the flow kernel against the CPU,
// through the engine's webgpu backend and the flow::GpuDevice over it.
//
// Three answers, in the order that narrows a failure fastest:
//   conformance  every operation body that has a WGSL spelling, on a device, against the C++ body
//                of the same operation - one dispatch per operation, one invocation per case, the
//                edge cases a 32-bit value can hold;
//   corpus       the GPU corpus of the interpreter's own tests (executors_corpus.h), whole graphs
//                through flow::GpuExecutor and the device, compared with the serial run under the
//                level-C tolerance - the load, the records taken back, the folds of the collectors;
//   reducers     the reducer library's tree folds against the CPU's statement of the same order
//                (treeFoldReference), several inputs in one call, absent branches included.
//
// Native is wgpu-native over Vulkan; in a browser the same binary runs through runtime/wasm-js.
// Exit code is the number of failed checks; 0 with a SKIP line when there is no adapter.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "XLFlowWgpuDevice.h"

#include "SPFlowExecutorGpu.h"

#include "../../flow/tests.h"
#include "../../flow/interp/executors_fixture.h"
#include "../../flow/interp/executors_corpus.h"
#include "../../flow/check/run_oracle.h"
#include "../../flow/interp/gpu_eval.h"

// the machine's template definitions: the fixture names the types, and this binary is the one
// that instantiates them
#include "SPFlowMachine.hpp"
#include "SPFlowLocal.hpp"

namespace STAPPLER_VERSIONIZED stappler::flowwgpu {

namespace fw = stappler::xenolith::flowwebgpu;
using stappler::test::check;
using stappler::test::execfx::corpus::Case;
using stappler::test::execfx::Fixture;
namespace oracle = stappler::test::oracle;
using flow::Var;
using flow::VarType;

static double now() { return double(sprt::platform::nanoclock(ClockType::Monotonic)) * 1e-9; }

// WGSL text travels as words: anything without the SPIR-V magic word is source, and the device's
// shader setup reads it that way.
static mem_std::Vector<uint32_t> packWords(StringView wgsl) {
	mem_std::Vector<uint32_t> words((wgsl.size() + 3) / 4, 0);
	if (!wgsl.empty()) {
		sprt::memcpy(words.data(), wgsl.data(), wgsl.size());
	}
	return words;
}

// ----------------------------------------------------------------------------------------------
// the rig: a looper, a headless WebGPU loop over it, the device

struct Rig {
	static Rc<sprt::dispatch::Looper> s_looper;

	Rc<fw::WgpuGpuDevice> device;

	enum class Setup {
		Ok,
		Skip,
		Fail,
	};

	Setup init(StringView &reason) {
		if (!s_looper) {
			s_looper = sprt::dispatch::Looper::acquire();
		}
		if (!s_looper) {
			reason = "no looper";
			return Setup::Fail;
		}

		mem_std::String why;
		auto loop = fw::makeWgpuLoop(s_looper, &why);
		if (!loop) {
			reason = why.empty() ? StringView("no WebGPU adapter") : StringView(why);
			return Setup::Skip;
		}

		device = Rc<fw::WgpuGpuDevice>::create(sp::move(loop),
				fw::WgpuGpuDevice::Options{.pump = s_looper.get()});
		if (!device || !device->getLoop()->getDevice()) {
			reason = "the device has no loop";
			return Setup::Fail;
		}
		return Setup::Ok;
	}

	bool pump(const Callback<bool()> &ready, uint32_t millis) {
		auto deadline = now() + double(millis) * 1e-3;
		while (now() < deadline) {
			if (ready()) {
				return true;
			}
			if (s_looper->poll() == 0) {
				sprt::platform::sleep(100'000ull);
			}
		}
		return ready();
	}

	void finalize() {
		if (device) {
			if (auto loop = device->getLoop(); loop && loop->isRunning()) {
				loop->stop();
				pump([&] { return !loop->isRunning(); }, 5'000);
			}
			device = nullptr;
		}
		s_looper = nullptr;
	}
};

Rc<sprt::dispatch::Looper> Rig::s_looper;

// ----------------------------------------------------------------------------------------------
// the watchdog: a section that hangs does not hang the suite

struct Watchdog {
	sprt::mutex mutex;
	mem_std::String section;
	sprt::atomic<uint64_t> deadline = 0;
	sprt::thread thread;
};

Watchdog *s_watchdog = nullptr;

void watchdogStart() {
	if (s_watchdog) {
		return;
	}
	s_watchdog = new Watchdog;
	s_watchdog->thread = sprt::thread([](Watchdog *w) {
		while (true) {
			sp::platform::sleep(50'000);
			auto deadline = w->deadline.load();
			if (deadline != 0 && now() > double(deadline) * 1e-6) {
				sprt::unique_lock<sprt::mutex> lock(w->mutex);
				sprt::cout << "[FAIL] watchdog: " << w->section << " hung\n";
				sprt::cout << "flowwgpu: watchdog, the device stopped answering\n";
				__sprt__Exit(3);
			}
		}
	}, s_watchdog);
	s_watchdog->thread.detach();
}

void watchdogArm(StringView section, uint32_t seconds) {
	sprt::unique_lock<sprt::mutex> lock(s_watchdog->mutex);
	s_watchdog->section = section.str<memory::StandartInterface>();
	s_watchdog->deadline = uint64_t(now() * 1e6) + uint64_t(seconds) * 1'000'000ull;
}

void watchdogDisarm() { s_watchdog->deadline = 0; }

// ----------------------------------------------------------------------------------------------
// what a flight waits on: pump until the request answers, or the section's watchdog says no

bool waitRequest(Rig &rig, const Rc<flow::GpuRequest> &request) {
	uint32_t guard = 0;
	while (request->isPending() && guard++ < 10'000'000) {
		rig.device->update();
		if (rig.s_looper->poll() == 0) {
			sprt::platform::sleep(100'000ull);
		}
	}
	return !request->isPending();
}

// ----------------------------------------------------------------------------------------------
// conformance: every operation body, a device against the C++ body of the same operation

// The cases a 32-bit value can hold. The interesting ones for arithmetic: the zeros, the ones, the
// extrema of the normal range - what a conversion may refuse, what a sum may saturate to.
static mem_std::Vector<Var> valuesOf(VarType type) {
	mem_std::Vector<Var> out;
	auto f32 = [&](float v) { out.emplace_back(flow::value::makeFloat32(v)); };
	auto vec = [&](std::initializer_list<float> vs) {
		uint32_t k = 0;
		Var v;
		v.type = type;
		for (auto f : vs) {
			v.v[k++] = f;
		}
		out.emplace_back(v);
	};
	switch (type) {
	case VarType::Bool:
		out.emplace_back(flow::value::makeBool(false));
		out.emplace_back(flow::value::makeBool(true));
		break;
	case VarType::Int32:
		for (auto v : {0, 1, -1, 7, -13, -2'147'483'647 - 1, 2'147'483'647}) {
			out.emplace_back(flow::value::makeInt32(v));
		}
		break;
	case VarType::UInt32:
		for (auto v : {0u, 1u, 2u, 7u, 2'147'483'647u, 4'294'967'295u}) {
			out.emplace_back(flow::value::makeUInt32(v));
		}
		break;
	case VarType::Float32:
		// No NaN, no infinity, no denormal among the inputs: a device's fast math answers for
		// none of them - a NaN comparison may go either way, a denormal may be flushed to zero
		// before it reaches the body, and what sin(inf) is nobody wrote down. The extrema of the
		// normal range and the numbers around them are the cases both sides must hold.
		f32(0.0f);
		f32(-0.0f);
		f32(1.0f);
		f32(-1.0f);
		f32(0.5f);
		f32(-2.75f);
		f32(1.17549435e-38f); // the smallest normal
		f32(3.402823466e38f); // the largest
		f32(-3.402823466e38f);
		f32(3.14159265f);
		break;
	case VarType::Vec2:
		vec({1.0f, 2.0f});
		vec({-1.0f, 0.5f});
		vec({0.0f, 0.0f});
		vec({1.17549435e-38f, -3.402823466e38f});
		vec({1.0f, 1.17549435e-38f});
		break;
	case VarType::Vec3:
		vec({1.0f, 2.0f, 3.0f});
		vec({-1.0f, 0.5f, -0.25f});
		vec({0.0f, 0.0f, 0.0f});
		vec({1.17549435e-38f, 1.0f, -3.402823466e38f});
		break;
	case VarType::Vec4:
	case VarType::Color:
		vec({1.0f, 2.0f, 3.0f, 4.0f});
		vec({-1.0f, 0.5f, -0.25f, 0.125f});
		vec({0.0f, 0.0f, 0.0f, 0.0f});
		vec({1.17549435e-38f, 1.0f, -1.0f, -3.402823466e38f});
		break;
	default: break;
	}
	return out;
}

static void performConformance(Rig &rig) {
	sprt::cout << "\n== flow on WebGPU: the conformance of every operation body ==\n";

	flow::OpRegistry ops;
	if (!ops.init() || flow::ops::registerCoreOps(ops) != Status::Ok) {
		check(false, "conformance: the operations would not register");
		return;
	}

	// one table for the conformance shaders: a pipeline per operation, keyed by its body's name
	const uint64_t table = 0x636f'6e66'6f72'6dull; // "conform"
	mem_std::Vector<flow::GpuPipelineSpec> specs;
	mem_std::Vector<mem_std::Vector<uint32_t>> words; // the specs' words, alive through the call
	mem_std::Vector<const flow::OpDesc *> bodies;
	for (uint32_t i = 0; i < ops.getCount(); ++i) {
		auto op = ops.getAt(i);
		if ((op->getFlags() & flow::OpFlags::ShaderForm) == flow::OpFlags::None) {
			continue;
		}
		auto name = op->getShaderName();
		if (!name.starts_with(StringView("xs_")) || op->getShaderWgslSource().empty()) {
			continue; // a structural form, or a body with no WGSL spelling
		}

		flow::GpuLayout in, out;
		auto wgsl = flow::writeGpuConformanceWgsl(*op, in, out);
		if (wgsl.empty()) {
			continue;
		}

		flow::GpuPipelineSpec spec;
		spec.kind = flow::GpuPipelineSpec::Kind::Block;
		spec.name = name;
		spec.hash = sprt::hash64(name.data(), name.size());
		words.emplace_back(packWords(wgsl));
		spec.spirv = SpanView<uint32_t>(words.back().data(), words.back().size());
		specs.emplace_back(spec);
		bodies.emplace_back(op);
	}

	check(specs.size() >= 55,
			mem_std::toString("conformance: every body operation has a WGSL spelling (",
					specs.size(), " of them)"));
	if (specs.empty()) {
		return;
	}

	rig.device->prepare(table, SpanView<flow::GpuPipelineSpec>(specs.data(), specs.size()));
	if (!rig.pump([&] { return rig.device->isPrepared(table); }, 60'000)) {
		check(false, "conformance: the pipelines did not compile");
		return;
	}

	uint32_t checked = 0;
	uint32_t failed = 0;
	for (auto op : bodies) {
		auto label = [&](StringView what) { return mem_std::toString("conformance: ", what); };

		flow::GpuLayout in, out;
		auto wgsl = flow::writeGpuConformanceWgsl(*op, in, out);
		auto name = op->getShaderName();
		auto pipeline = sprt::hash64(name.data(), name.size());

		// the cases: every combination of the edge values of the operation's inputs. An inexact
		// body (a transcendental, a square root - OpFlags::ShaderInexact) runs on tame inputs:
		// what sin(3e38) is depends on whose argument reduction, and no tolerance says.
		auto ins = op->getDataIn();
		const bool inexact = (op->getFlags() & flow::OpFlags::ShaderInexact) != flow::OpFlags::None;
		mem_std::Vector<mem_std::Vector<Var>> values;
		for (auto &pin : ins) {
			auto list = valuesOf(pin.type);
			if (inexact) {
				mem_std::Vector<Var> tame;
				for (auto &v : list) {
					bool ok = true;
					uint32_t n = pin.type == VarType::Vec2 ? 2
							: (pin.type == VarType::Vec3 ? 3
							: (pin.type == VarType::Vec4 || pin.type == VarType::Color ? 4 : 1));
					for (uint32_t k = 0; k < n; ++k) {
						ok = ok && sprt::abs(double(n == 1 ? (pin.type == VarType::Float32 ? v.v[0] : 0.0) : v.v[k])) <= 1000.0;
					}
					if (pin.type == VarType::Int32 || pin.type == VarType::UInt32
							|| pin.type == VarType::Bool) {
						ok = true; // exact for exact types; inexactness is a float question
					}
					if (ok) {
						tame.emplace_back(v);
					}
				}
				values.emplace_back(sp::move(tame));
			} else {
				values.emplace_back(sp::move(list));
			}
		}
		uint32_t cases = 1;
		for (auto &v : values) {
			cases *= uint32_t(v.size());
		}
		cases = sprt::min(cases, 4096u); // a guard, not a plan: the real counts are far below

		// Two poison shapes a device's fast math answers for, that no tolerance can carry away:
		// a division of the extrema - the device multiplies by a reciprocal it flushed to zero -
		// and pow itself - C answers a negative base with an integer exponent and a zero exponent
		// with one, where the device's pow answers NaN. Everything else, both sides must hold.
		const bool f32Div = name == StringView("xs_div_f32");
		const bool f32Pow = name == StringView("xs_pow_f32");
		auto poisoned = [&](const mem_std::Vector<Var> &vars) {
			if (f32Div && vars.size() == 2 && sprt::abs(vars[0].v[0]) >= 3.4e38
					&& sprt::abs(vars[1].v[0]) >= 3.4e38) {
				return true;
			}
			if (f32Pow && vars.size() == 2
					&& (vars[0].v[0] < 0.0 || vars[1].v[0] == 0.0
							|| sprt::abs(vars[1].v[0]) < 1.2e-38 // a backend may flush it to a zero exponent
							|| (vars[0].v[0] == 0.0 && vars[1].v[0] < 0.0))) {
				return true;
			}
			return false;
		};

		// the In records: one per case, written the way the loader writes them
		mem_std::Vector<uint8_t> inBytes;
		mem_std::Vector<mem_std::Vector<Var>> caseVars;
		for (uint32_t c = 0; c < cases; ++c) {
			mem_std::Vector<Var> vars(ins.size());
			uint32_t rest = c;
			for (uint32_t p = 0; p < ins.size(); ++p) {
				auto &list = values[p];
				vars[p] = list[rest % list.size()];
				rest /= uint32_t(list.size());
			}
			if (poisoned(vars)) {
				continue;
			}
			caseVars.emplace_back(sp::move(vars));
			auto at = inBytes.size();
			inBytes.resize(at + in.size, 0);
			for (auto &slot : in.slots) {
				if (slot.kind == flow::GpuSlot::Kind::In) {
					flow::writeGpuValue(inBytes.data() + at + slot.offset, slot.type,
							caseVars.back()[slot.index]);
				}
			}
		}
		cases = uint32_t(caseVars.size());
		if (cases == 0) {
			return;
		}

		// the block: one word, the case count
		uint32_t blockWord = cases;
		flow::GpuDispatchCall call;
		call.table = table;
		call.slot = 0;
		call.pipeline = pipeline;
		call.block = BytesView(reinterpret_cast<const uint8_t *>(&blockWord), 4);
		call.ins = BytesView(inBytes.data(), inBytes.size());
		call.count = cases;
		call.inStride = in.size;
		call.outStride = out.size;

		auto request = rig.device->dispatch(call);
		if (!request) {
			check(false, label(mem_std::toString(name, ": the device takes the dispatch")));
			++failed;
			continue;
		}
		if (!waitRequest(rig, request) || request->getState() != flow::GpuRequest::State::Done) {
			check(false, label(mem_std::toString(name, ": the dispatch answers")));
			++failed;
			continue;
		}

		// the CPU's answer for the same cases, and the comparison
		test::gpueval::ValueContext cpu;
		auto data = request->getData();
		const flow::GpuSlot *statusSlot = nullptr;
		for (auto &slot : out.slots) {
			if (slot.kind == flow::GpuSlot::Kind::Status) {
				statusSlot = &slot;
				break;
			}
		}
		uint32_t bad = 0;
		for (uint32_t c = 0; c < cases; ++c) {
			cpu.bind(*op);
			for (uint32_t p = 0; p < ins.size(); ++p) {
				cpu.inputs[p] = caseVars[c][p];
			}
			auto st = op->getInvoke()(cpu);

			auto record = data.data() + c * out.size;
			auto gpuStatus = statusSlot
					? flow::readGpuValue(record + statusSlot->offset, VarType::Int32) : Var();
			// A refusal leaves the CPU's output empty - the values are nobody's, only the status
			// is the contract. On success every output is compared, under the level-C tolerance.
			bool ok = gpuStatus.i == int64_t(toInt(st));
			if (ok && st == Status::Ok) {
				uint32_t r = 0;
				for (auto &slot : out.slots) {
					if (slot.kind != flow::GpuSlot::Kind::Cell) {
						continue;
					}
					auto got = flow::readGpuValue(record + slot.offset, slot.type);
					// An inexact body is compared wider: a device's transcendentals differ from the
					// CPU's in the fifth decimal (SwiftShader's do), and whose answer that is belongs
					// to the device, not to the lowering.
					bool one = cpu.outputs[r].type == got.type;
					if (one && inexact && !flow::getGpuTolerance(got.type).exact) {
						auto tol = flow::GpuTolerance{1e-5, 1e-4, false};
						uint32_t n = got.type == VarType::Float32 ? 1
							: (got.type == VarType::Vec2 ? 2
							: (got.type == VarType::Vec3 ? 3
							: (got.type == VarType::Vec4 || got.type == VarType::Color ? 4 : 0)));
						for (uint32_t k = 0; k < n && one; ++k) {
							auto a = double(got.v[k]);
							auto b = double(cpu.outputs[r].v[k]);
							if (a == b) {
								continue; // the infinities and the zeros agree outright
							} else if (a != a || b != b) {
								one = (a != a) && (b != b); // a NaN agrees with a NaN
							} else {
								one = sprt::abs(a - b) <= sprt::max(tol.abs, tol.rel * sprt::max(sprt::abs(a), sprt::abs(b)));
							}
						}
					} else if (one) {
						one = flow::nearlyEqual(cpu.outputs[r], got);
					}
					if (!one && bad < 3) {
						sprt::cout << "        dbg op " << name << " case " << c << ":";
						for (uint32_t p = 0; p < ins.size(); ++p) {
							sprt::cout << " in" << p << " " << caseVars[c][p].v[0];
						}
						sprt::cout << " cpu " << cpu.outputs[r].v[0] << " gpu " << got.v[0] << "\n";
					}
					ok = ok && one;
					++r;
				}
			}
			if (!ok) {
				++bad;
			}
		}

		check(bad == 0, label(mem_std::toString(name, ": ", cases - bad, " of ", cases,
						" cases agree with the C++ body")));
		++checked;
		if (bad != 0) {
			++failed;
		}
	}

	check(failed == 0, mem_std::toString("conformance: every operation agrees (", checked,
				" operations, ", failed, " refused)"));
}

// ----------------------------------------------------------------------------------------------
// the corpus: whole graphs through the GPU executor, against the serial run

// The WGSL the host hands over: a block's body by the hash of the text it lowers to (the GLSL text
// the shader table keys everything by - the WGSL is the payload, not the key), and every reducer of
// the library by name and text. The names and the words outlive the attach.
static mem_std::Vector<flow::GpuShaderBinary> manifestWgsl(const flow::RuntimeGraph &g) {
	static mem_std::Vector<mem_std::String> names;
	static mem_std::Vector<mem_std::Vector<uint32_t>> words;
	names.reserve(64); // a growth would move the short names out from under the views
	words.reserve(64);
	mem_std::Vector<flow::GpuShaderBinary> out;
	for (uint32_t b = 0; b < g.getBlockCount(); ++b) {
		flow::GpuProgram program;
		if (flow::lowerGpuBlock(g, b, program) != Status::Ok) {
			continue;
		}
		auto glsl = flow::writeGpuGlsl(g, program);
		auto hash = sprt::hash64(glsl.data(), glsl.size());
		auto wgsl = flow::writeGpuWgsl(g, program);
		if (wgsl.empty()) {
			continue; // a body operation with no WGSL spelling: the block stays on the CPU
		}
		words.emplace_back(packWords(wgsl));
		out.emplace_back(flow::GpuShaderBinary{StringView(), hash,
			SpanView<uint32_t>(words.back().data(), words.back().size())});
	}
	for (auto &r : flow::getGpuReducers()) {
		names.emplace_back(flow::getGpuReduceName(r.first, r.second));
		auto text = flow::writeGpuReduceGlsl(r.first, r.second);
		words.emplace_back(packWords(flow::writeGpuReduceWgsl(r.first, r.second)));
		out.emplace_back(flow::GpuShaderBinary{StringView(names.back()),
			sprt::hash64(text.data(), text.size()),
			SpanView<uint32_t>(words.back().data(), words.back().size())});
	}
	return out;
}

static void performCorpus(Rig &rig) {
	sprt::cout << "\n== flow on WebGPU: the GPU corpus of the interpreter, device against serial ==\n";

	using RunConfig = Fixture::ConfigType;

	auto snapshot = [&](Fixture &fx, oracle::RunSnapshot &out) {
		oracle::takeSnapshot(fx.graph, fx.interp, fx.arena, fx.report, out, &fx.scene, &fx.sceneArena,
				true);
		oracle::dropUnpublishedBodyRecords(fx.graph, out.pathRecords);
		oracle::dropUnpublishedBodyTrace(fx.graph, out);
	};
	// level C per activation - what a device may differ in, and nothing more
	auto levelC = [] {
		oracle::CompareOptions opts{oracle::Level::Tolerant, false, true};
		opts.order = oracle::CompareOptions::Order::PerActivation;
		return opts;
	};

	for (auto &c : stappler::test::execfx::corpus::GpuCases) {
		auto label = [&](StringView what) {
			return mem_std::toString("corpus: ", c.name, ": ", what);
		};

		Fixture serial;
		if (!stappler::test::execfx::corpus::prepareCase(serial, c, false)) {
			check(false, label("prepares"));
			continue;
		}
		serial.frame(c.dt, 0, stappler::test::execfx::corpus::configFor(serial, c, false));
		oracle::RunSnapshot base;
		snapshot(serial, base);

		Fixture fx;
		stappler::test::execfx::corpus::prepareCase(fx, c, false);
		if (fx.graph.attachGpuShaders(manifestWgsl(fx.graph)) == 0) {
			check(false, label("the host's WGSL is attached"));
			continue;
		}

		flow::GpuExecutor executor(rig.device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		auto table = fx.graph.getGpuShaders();
		executor.prepare(*table);
		if (!rig.pump([&] { return rig.device->isPrepared(table->getIdentity()); }, 60'000)) {
			check(false, label("the pipelines compiled"));
			continue;
		}

		auto cfg = stappler::test::execfx::corpus::configFor(fx, c, false);
		cfg.executor = &executor;

		// a frame with a device in it: the machine suspends while the block is out, and the pump
		// brings the answer back
		if (fx.frameBegin(c.dt, 0, cfg) != Status::Ok) {
			check(false, label("the frame begins"));
			continue;
		}
		uint32_t guard = 0;
		for (; guard < 100'000; ++guard) {
			if (fx.interp.stepOnce(fx.report)) {
				continue;
			}
			if (fx.interp.getState() == flow::RunState::Suspended) {
				rig.device->update();
				if (rig.s_looper->poll() == 0) {
					sprt::platform::sleep(100'000ull);
				}
				continue;
			}
			break;
		}

		oracle::RunSnapshot snap;
		snapshot(fx, snap);

		check(fx.report.outcome == c.outcome,
				label(mem_std::toString("ends ", flow::getRunOutcomeName(c.outcome))));
		check(executor.getDispatchCount() == 1, label("one dispatch for the block"));
		if (c.outcome == flow::RunOutcome::Completed) {
			check(oracle::compareRuns(base, snap, levelC(), label("device vs serial")),
					label("the serial run, per activation, under the level-C tolerance"));
		}
		fx.interp.reset();
	}

	// the buffers between frames: a series of one size allocates nothing after the first
	{
		auto &c = stappler::test::execfx::corpus::GpuCases[0];
		Fixture fx;
		stappler::test::execfx::corpus::prepareCase(fx, c, false);
		fx.graph.attachGpuShaders(manifestWgsl(fx.graph));
		flow::GpuExecutor executor(rig.device,
				flow::GpuExecutor::Options{.policy = flow::GpuExecutor::Policy::Always});
		auto table = fx.graph.getGpuShaders();
		executor.prepare(*table);
		rig.pump([&] { return rig.device->isPrepared(table->getIdentity()); }, 60'000);

		auto runOnce = [&](Fixture &target) {
			auto cfg = stappler::test::execfx::corpus::configFor(target, c, false);
			cfg.executor = &executor;
			target.frameBegin(c.dt, 0, cfg);
			for (uint32_t g = 0; g < 100'000; ++g) {
				if (target.interp.stepOnce(target.report)) {
					continue;
				}
				if (target.interp.getState() == flow::RunState::Suspended) {
					rig.device->update();
					if (rig.s_looper->poll() == 0) {
						sprt::platform::sleep(100'000ull);
					}
					continue;
				}
				break;
			}
			target.interp.reset();
		};

		runOnce(fx);
		auto first = rig.device->getBufferAllocations();
		check(first > 0, "corpus: the first frame allocates the block's buffers");
		for (uint32_t frame = 0; frame < 20; ++frame) {
			Fixture again;
			stappler::test::execfx::corpus::prepareCase(again, c, false);
			again.graph.attachGpuShaders(manifestWgsl(again.graph));
			runOnce(again);
		}
		check(rig.device->getBufferAllocations() == first,
				mem_std::toString("corpus: twenty frames of the same size allocate none (",
						rig.device->getBufferAllocations(), " vs ", first, ")"));
		rig.device->releaseBuffers();
		check(rig.device->getLiveBufferSets() == 0, "corpus: ... until the device is told to give them up");
	}
}

// ----------------------------------------------------------------------------------------------
// the reducers: the tree folds against the CPU's statement of the same order

static void performReducers(Rig &rig) {
	sprt::cout << "\n== flow on WebGPU: the reducer library's tree folds ==\n";

	const uint64_t table = 0x7265'6475'6365ull; // "reduce"
	mem_std::Vector<flow::GpuPipelineSpec> specs;
	mem_std::Vector<mem_std::String> names;
	mem_std::Vector<mem_std::Vector<uint32_t>> words;
	// The names and the words must outlive the prepare call with their addresses intact: a growth
	// would move the short strings out from under the views.
	names.reserve(16);
	words.reserve(16);
	for (auto &r : flow::getGpuReducers()) {
		names.emplace_back(flow::getGpuReduceName(r.first, r.second));
		words.emplace_back(packWords(flow::writeGpuReduceWgsl(r.first, r.second)));
		flow::GpuPipelineSpec spec;
		spec.kind = flow::GpuPipelineSpec::Kind::Reduce;
		spec.name = names.back();
		spec.hash = 0;
		spec.spirv = SpanView<uint32_t>(words.back().data(), words.back().size());
		specs.emplace_back(spec);
	}
	rig.device->prepare(table, SpanView<flow::GpuPipelineSpec>(specs.data(), specs.size()));
	if (!rig.pump([&] { return rig.device->isPrepared(table); }, 60'000)) {
		check(false, "reducers: the pipelines did not compile");
		return;
	}

	// deterministic values: the same numbers on every run, every device
	test::Lcg lcg{0x5eed'1234ull};
	auto element = [&](VarType type) -> Var {
		switch (type) {
		case VarType::Bool: return flow::value::makeBool((lcg.next() & 1) != 0);
		case VarType::Int32: return flow::value::makeInt32(int32_t(lcg.next() % 2001) - 1000);
		case VarType::UInt32: return flow::value::makeUInt32(uint32_t(lcg.next() >> 16));
		case VarType::Float32: return flow::value::makeFloat32(float(double(lcg.next() % 20001) / 1000.0 - 10.0));
		case VarType::Vec2:
		case VarType::Vec3:
		case VarType::Vec4: {
			Var v;
			v.type = type;
			uint32_t n = type == VarType::Vec2 ? 2 : (type == VarType::Vec3 ? 3 : 4);
			for (uint32_t k = 0; k < n; ++k) {
				v.v[k] = float(double(lcg.next() % 20001) / 1000.0 - 10.0);
			}
			return v;
		}
		default: return flow::value::makeInt32(0);
		}
	};

	// one fold: a call with several inputs of one kind, absent branches included
	auto foldCase = [&](StringView kind, VarType type, SpanView<uint32_t> counts,
			SpanView<uint64_t> holes) {
		auto label = [&](StringView what) {
			return mem_std::toString("reducers: ", kind, "/", flow::value::getVarTypeName(type), ": ",
					what);
		};

		uint32_t stride = flow::getGpuTypeSize(type);
		mem_std::Vector<flow::GpuReduceInput> inputs;
		mem_std::Vector<mem_std::Vector<Var>> expected;
		for (uint32_t i = 0; i < counts.size(); ++i) {
			auto count = counts[i];
			flow::GpuReduceInput input;
			input.kind = kind;
			input.type = type;
			input.count = count;
			input.stride = stride;
			input.values.resize(size_t(count) * stride);
			input.present.resize(size_t(count) * 4, 0);

			mem_std::Vector<Var> vars;
			for (uint32_t e = 0; e < count; ++e) {
				auto v = element(type);
				if (kind == StringView("count")) {
					v = flow::value::makeInt32(v.i != 0 ? 1 : 0); // count folds the ones
				}
				vars.emplace_back(v);
				flow::writeGpuValue(input.values.data() + size_t(e) * stride, type, v);
			}
			// the holes: branches whose value is absent. Branch 0 of the first input too - the
			// fold the shader has to move the other side for.
			for (uint32_t e = 0; e < count; ++e) {
				bool absent = false;
				for (auto hole : holes) {
					absent = absent || (hole == (uint64_t(i) << 32) | e);
				}
				if (absent) {
					continue;
				}
				uint32_t one = 1;
				sprt::memcpy(input.present.data() + size_t(e) * 4, &one, 4);
			}

			inputs.emplace_back(sp::move(input));
			expected.emplace_back(sp::move(vars));
		}

		auto request = rig.device->reduce(
				flow::GpuReduceCall{table, 0, SpanView<flow::GpuReduceInput>(inputs.data(), inputs.size())});
		if (!request) {
			sprt::cout << "        dbg reduce refused: kind " << kind << " prepared "
					<< (rig.device->isPrepared(table) ? 1 : 0) << " name "
					<< flow::getGpuReduceName(kind, type) << "\n";
			check(false, label("the device takes the fold"));
			return;
		}
		if (!waitRequest(rig, request) || request->getState() != flow::GpuRequest::State::Done) {
			check(false, label("the fold answers"));
			return;
		}

		auto data = request->getData();
		size_t at = 0;
		uint32_t bad = 0;
		uint32_t printed = 0;
		for (uint32_t i = 0; i < inputs.size(); ++i) {
			// the reference: the CPU's statement of the same tree, in branch order
			mem_std::Vector<uint8_t> present(inputs[i].count);
			for (uint32_t e = 0; e < inputs[i].count; ++e) {
				uint32_t p = 0;
				sprt::memcpy(&p, inputs[i].present.data() + size_t(e) * 4, 4);
				present[e] = uint8_t(p != 0);
			}
			Var want;
			auto any = false;
			for (auto p : present) {
				any = any || p != 0;
			}
			bool folded = flow::treeFoldReference(kind, type,
					SpanView<Var>(expected[i].data(), expected[i].size()),
					SpanView<uint8_t>(present.data(), present.size()), want);

			auto got = flow::readGpuValue(data.data() + at,
					kind == StringView("count") ? VarType::Int32 : type);
			at += inputs[i].stride;
			uint32_t flag = 0;
			sprt::memcpy(&flag, data.data() + at, 4);
			at += 4;

			// An absent fold (flag 0) answers a value nobody wrote: only the flag is the contract
			// then, exactly like a refusal in the conformance.
			const bool absent = !(folded && any);
			if (flag != (absent ? 0u : 1u) || (!absent && !flow::nearlyEqual(want, got))) {
				++bad;
				if (bad <= 2) {
					sprt::cout << "        dbg fold " << kind << "/" << flow::value::getVarTypeName(type)
							<< " input " << i << " count " << inputs[i].count << ": want i "
							<< want.i << " v0 " << want.v[0] << " got i " << got.i << " v0 "
							<< got.v[0] << " flag " << flag << " (folded " << int(folded)
							<< " any " << int(any) << ")\n";
				}
			}
		}
		check(bad == 0, label(mem_std::toString(inputs.size(), " inputs, ", bad, " differ")));
	};

	for (auto &r : flow::getGpuReducers()) {
		const uint32_t counts[] = {1, 13, 100};
		const uint64_t holes[] = {(0ull << 32) | 0, (1ull << 32) | 5, (2ull << 32) | 42,
				(2ull << 32) | 43};
		foldCase(r.first, r.second, counts, holes);
	}

	// several kinds in one call: the collectors of one block go together
	{
		mem_std::Vector<flow::GpuReduceInput> inputs;
		uint32_t counts[] = {37, 64};
		StringView kinds[] = {StringView("sum"), StringView("max")};
		VarType types[] = {VarType::Float32, VarType::Int32};
		mem_std::Vector<mem_std::Vector<Var>> expected;
		for (uint32_t i = 0; i < 2; ++i) {
			auto count = counts[i];
			flow::GpuReduceInput input;
			input.kind = kinds[i];
			input.type = types[i];
			input.count = count;
			input.stride = flow::getGpuTypeSize(types[i]);
			input.values.resize(size_t(count) * input.stride);
			input.present.resize(size_t(count) * 4);
			mem_std::Vector<Var> vars;
			for (uint32_t e = 0; e < count; ++e) {
				auto v = element(types[i]);
				vars.emplace_back(v);
				flow::writeGpuValue(input.values.data() + size_t(e) * input.stride, types[i], v);
				uint32_t one = (e % 3 == 1) ? 0 : 1; // a third of the branches absent
				sprt::memcpy(input.present.data() + size_t(e) * 4, &one, 4);
			}
			inputs.emplace_back(sp::move(input));
			expected.emplace_back(sp::move(vars));
		}

		auto request = rig.device->reduce(
				flow::GpuReduceCall{table, 0, SpanView<flow::GpuReduceInput>(inputs.data(), inputs.size())});
		bool ok = request && waitRequest(rig, request)
				&& request->getState() == flow::GpuRequest::State::Done;
		if (ok) {
			auto data = request->getData();
			size_t at = 0;
			for (uint32_t i = 0; i < inputs.size(); ++i) {
				mem_std::Vector<uint8_t> present(inputs[i].count);
				for (uint32_t e = 0; e < inputs[i].count; ++e) {
					uint32_t p = 0;
					sprt::memcpy(&p, inputs[i].present.data() + size_t(e) * 4, 4);
					present[e] = uint8_t(p != 0);
				}
				Var want;
				flow::treeFoldReference(inputs[i].kind, inputs[i].type,
						SpanView<Var>(expected[i].data(), expected[i].size()),
						SpanView<uint8_t>(present.data(), present.size()), want);
				auto got = flow::readGpuValue(data.data() + at, inputs[i].type);
				at += inputs[i].stride + 4;
				ok = ok && flow::nearlyEqual(want, got);
			}
		}
		check(ok, "reducers: two kinds in one call answer in order");
	}
}

// ----------------------------------------------------------------------------------------------

int performChecks() {
	Rig rig;
	StringView reason;
	switch (rig.init(reason)) {
	case Rig::Setup::Ok: break;
	case Rig::Setup::Skip:
		sprt::cout << "flowwgpu: SKIP no WebGPU adapter (" << reason << ")\n";
		sprt::cout << "flowwgpu: 0 checks, 0 failures\n";
		return 0;
	case Rig::Setup::Fail:
		sprt::cout << "[FAIL] setup: " << reason << "\n";
		return 2;
	}

	watchdogStart();

	sprt::cout << "flowwgpu: device \"" << rig.device->getDeviceName() << "\"\n";

	watchdogArm("conformance", 120);
	performConformance(rig);
	watchdogDisarm();

	watchdogArm("corpus", 120);
	performCorpus(rig);
	watchdogDisarm();

	watchdogArm("reducers", 120);
	performReducers(rig);
	watchdogDisarm();

	watchdogArm("teardown", 30);
	rig.finalize();
	watchdogDisarm();

	auto failures = test::failures();
	sprt::cout << "flowwgpu: " << (failures > 0 ? "FAILED " : "") << failures << " failures\n";
	return int(sprt::min(failures, 254));
}

int run(int argc, const char *argv[]) {
	StringView cmd = argc > 1 ? StringView(argv[1]) : StringView("check");
	if (cmd == "check") {
		return performChecks();
	}
	sprt::cout << "flowwgpu [check]\n";
	return 1;
}

} // namespace stappler::flowwgpu

int main(int argc, const char *argv[]) {
	return stappler::perform_main(argc, argv,
			[&] { return stappler::flowwgpu::run(argc, argv); });
}
