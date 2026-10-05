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

#ifndef TESTS_FLOW_INTERP_INTERP_FIXTURE_H_
#define TESTS_FLOW_INTERP_INTERP_FIXTURE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"
#include "SPVStoreJournal.h"

// For the suite's arena kind - see tests.h. A fixture has to name one like everything else.
#include "../tests.h"
#include "../check/flow_check.h"

// One place that builds a graph out of JSON, runs it, and reads what the run left behind. Shared by
// every interpreter section so that they agree on what a run IS - and so that a change to the shape
// of a run breaks in one place rather than four.
//
// A template over the arena kind, because the compiled engine is held to the interpreter under EVERY
// kind (`codegen-exact-arena`), and `Fixture` is the suite's kind. Runs are in the kernel's own
// environment: there is no scene, and a graph that needs one is a host's test, not this one.
//
// Two probe operations live here rather than in the library. The library is what a graph is written
// with; a probe is what a test needs to see the interpreter's own behaviour, and it has no business
// being shipped:
//
//   test.fail   - fails, so that the failure path can be looked at;
//   test.split  - fires an exec output AND produces a value, which no library operation does, so the
//                 order between "continue the chain" and "compute what became computable" becomes
//                 observable.
namespace STAPPLER_VERSIONIZED stappler::test::interpfx {

using namespace stappler::flow;

struct Occupancy {
	uint64_t count = 0;
	uint64_t bytes = 0;

	bool operator==(const Occupancy &) const = default;
};

template <typename A>
inline Occupancy measure(const A &arena) {
	mem_std::Value dump;
	arena.describe(dump);
	auto &live = dump.getValue("live");
	return Occupancy{uint64_t(live.getInteger("count")), uint64_t(live.getInteger("bytes"))};
}

template <typename A>
inline mem_std::Vector<uint8_t> takeImage(const A &arena) {
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

inline Status opFail(OpContext &) { return Status::ErrorInvalidArguemnt; }

inline Status opSplit(OpContext &ctx) {
	flow::value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.setOutput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename A>
struct FixtureT {
	using ArenaType = A;
	using InterpType = InterpreterT<A, NoEnv>;
	using ConfigType = RunConfigT<A, NoEnv>;

	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	A arena;

	// Built only by prepareJournal(). Declared before the interpreter so that it outlives the run
	// whose arenas it observes. Absent without versioning, and so is every section that asks for one.
	vstore::Journal journal;

	InterpType interp;
	RunReport report;

	// A section that needs operations of its own installs them here; called from registerOps() so
	// that they are in place before the asset is resolved against the registry.
	bool (*extraOps)(OpRegistry &) = nullptr;

	// The library a graph's calls are resolved against, for a section about functions.
	const FunctionHost *functions = nullptr;

	bool registerOps() {
		if (!ops.init() || flow::ops::registerCoreOps(ops) != Status::Ok) {
			return false;
		}
		if (InterpType::registerCoreTypes(ops.getLocalTypes()) != Status::Ok) {
			return false;
		}

		PinDesc value[] = {PinDesc{.name = StringView("value"), .type = flow::value::VarType::Float}};
		{
			OpDef def;
			def.name = StringView("test.fail");
			def.dataIn = SpanView<PinDesc>(value, 1);
			def.invoke = &opFail;
			if (!ops.createNative(def)) {
				return false;
			}
		}
		{
			const StringView then[] = {StringView("then")};
			OpDef def;
			def.name = StringView("test.split");
			def.dataIn = SpanView<PinDesc>(value, 1);
			def.dataOut = SpanView<PinDesc>(value, 1);
			def.hasExecIn = true;
			def.execOut = SpanView<StringView>(then, 1);
			def.invoke = &opSplit;
			if (!ops.createNative(def)) {
				return false;
			}
		}
		return extraOps ? extraOps(ops) : true;
	}

	// A journal over the run's store. Call after prepare(), because attaching wants an initialised
	// arena - and because Journal::attach has no counterpart, so it is done once and for the arena's
	// life.
	//
	// A PlainArena announces nothing and cannot be watched, so under that kind this answers false
	// rather than failing to compile: a section that sweeps the kinds asks, and takes the answer.
	bool prepareJournal() {
		if constexpr (A::IsTracked) {
			if (!arena.isInitialized() || !journal.init()) {
				return false;
			}
			journal.attach(&arena);
			return true;
		} else {
			return false;
		}
	}

	ConfigType journalConfig() {
		ConfigType config;
		config.journal = &journal;
		return config;
	}

	bool prepare(StringView json) {
		if (!registerOps()) {
			return false;
		}

		asset.init();
		mem_std::Value diag;
		if (asset.load(data::read<mem_std::Interface>(json), &diag) != Status::Ok) {
			sprt::cout << "       asset: " << data::toString<mem_std::Interface>(diag, true)
					   << "\n";
			return false;
		}

		graph.init();
		graph.setFunctionHost(functions);
		mem_std::Value buildReport;
		auto st = graph.build(asset, ops, &buildReport);
		if (st != Status::Ok) {
			sprt::cout << "       build: " << data::toString<mem_std::Interface>(buildReport, true)
					   << "\n";
			return false;
		}
		return arena.init();
	}

	Status run(const ConfigType &config = ConfigType()) {
		return interp.run(graph, ops, arena, config, report);
	}

	// The value a node left in one of its outputs - exactly what the next node's getInput reads.
	bool output(NodeId id, uint32_t pin, flow::value::Var &out) {
		return outputAt(id, pin, RootActivation, out);
	}

	// The same, in a given turn of a loop. Two turns of one node are two records, and that is the
	// whole of what "the iterations are isolated" means.
	bool outputAt(NodeId id, uint32_t pin, uint32_t activation, flow::value::Var &out) {
		auto index = graph.findNode(id);
		if (index == InvalidIndex) {
			return false;
		}
		auto &rt = graph.getNodeAt(index);
		auto record = interp.getLocal().getRecord(index, activation);
		if (!rt.localSchema || record == NullAddr) {
			return false;
		}
		auto fields = rt.localSchema->getFields();
		if (pin >= fields.size()) {
			return false;
		}
		return rt.localSchema->getField(*interp.getLocal().getArena(), record, fields[pin], out)
				== Status::Ok;
	}

	mem_std::String outputString(NodeId id, uint32_t pin) {
		auto index = graph.findNode(id);
		auto &rt = graph.getNodeAt(index);
		auto record = interp.getLocal().getRecord(index, RootActivation);
		if (!rt.localSchema || record == NullAddr) {
			return mem_std::String();
		}
		auto &field = rt.localSchema->getFields()[pin];
		return flow::value::blob::stringGet(*interp.getLocal().getArena(), record + field.offset);
	}

	// Did this node run at all? The question every exec test asks about the branch not taken.
	bool ran(NodeId id) {
		auto index = graph.findNode(id);
		if (index == InvalidIndex) {
			return false;
		}
		auto state = interp.getLocal().readState(interp.getLocal().getState(index, RootActivation));
		return (state.flags & NodeFlags::Ran) != 0;
	}
};

// The suite's kind, and the name every section reads.
using Fixture = FixtureT<Arena>;

} // namespace stappler::test::interpfx

#endif /* TESTS_FLOW_INTERP_INTERP_FIXTURE_H_ */
