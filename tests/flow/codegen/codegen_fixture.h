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

#ifndef TESTS_FLOW_CODEGEN_CODEGEN_FIXTURE_H_
#define TESTS_FLOW_CODEGEN_CODEGEN_FIXTURE_H_

#include "../interp/interp_fixture.h"
#include "../interp/corpus.h"

#include "SPFlowCodegen.h"

#include "../check/run_oracle.h"

#include "gen/manifest.gen.h"

// What the codegen sections share: a case of the corpus turned into a unit, and the committed unit
// of that case found by name.
namespace STAPPLER_VERSIONIZED stappler::test::codegenfx {

using namespace stappler::test::interpfx;
namespace codegen = stappler::flow::codegen;
namespace corpus = stappler::test::interpfx::corpus;
namespace oracle = stappler::test::oracle;

// The unit's name for a case: "loop-in-sequence" -> "corpus_loop_in_sequence". The prefix keeps
// the namespace and the object file's basename distinct from anything else in the build.
inline mem_std::String unitName(StringView caseName) {
	mem_std::String out("corpus_");
	for (auto c : caseName) { out.push_back(c == '-' ? '_' : c); }
	return out;
}

// Prepares the case's fixture and writes its graph out, for the kernel's own environment and into
// the default namespace (`stappler::flow::gen`). The fixture is the caller's, because the checks
// that follow compare the unit against the graph it was written from.
//
// `stem` names the unit when the caller has its own rule for that: a seed of the generated half is
// `synth_4097` and not `corpus_synth_4097`, because the prefix is there to keep object basenames
// apart and a seed's name is already apart.
inline bool emitCase(Fixture &fx, const corpus::Case &c, codegen::Emitted &out,
		mem_std::Value *diag = nullptr, StringView stem = StringView()) {
	if (!corpus::prepareCase(fx, c, false)) {
		return false;
	}
	codegen::EmitOptions options;
	auto name = stem.empty() ? unitName(c.name) : mem_std::String(stem.data(), stem.size());
	options.name = name;

	// The corpus is built out of the standard library and the two test probes, and the probes have
	// no inline form on purpose: they are what proves a unit still runs an operation it cannot call
	// by name. So one header, and the probes go through the registry.
	static constexpr StringView bodies[] = {StringView("SPFlowOpsInline.h")};
	options.bodyIncludes = SpanView<StringView>(bodies, 1);

	// A graph of five nodes or more is written in two parts. No graph this small needs it, but the
	// split path has to run, and the corpus is what runs: about half the committed units carry a
	// dispatcher that calls across translation units, and the exact sections hold them to the
	// interpreter as they hold the rest. The threshold is this fixture's, not the generator's.
	options.split = fx.graph.getNodeCount() >= 5 ? 2 : 1;
	return codegen::emit(fx.graph, fx.asset, fx.ops, options, out, diag) == Status::Ok;
}

// The file a part goes in: `<name>.gen.cpp` for the first and `<name>.gen.<k>.cpp` for the rest
// (EmitOptions::split).
inline mem_std::String sourceName(StringView stem, uint32_t part) {
	return part == 0 ? mem_std::toString(stem, ".gen.cpp")
					 : mem_std::toString(stem, ".gen.", part, ".cpp");
}

// Whether the text appears in any of the unit's sources. A split unit holds its nodes across
// several, and a check that read only the first would pass by looking in the wrong file.
inline bool sourcesHave(const codegen::Emitted &unit, StringView what) {
	for (auto &src : unit.sources) {
		if (src.find(what.data(), 0, what.size()) != mem_std::String::npos) {
			return true;
		}
	}
	return false;
}

inline const corpus::Case *findCase(StringView name) {
	for (auto &c : corpus::Cases) {
		if (c.name == name) {
			return &c;
		}
	}
	return nullptr;
}

// One side of a comparison: a fixture prepared for the case, and an engine over it behind the
// interface. The interpreter's side runs the fixture's built graph; the compiled side runs the
// case's committed unit, loaded against the fixture's registry - the registry the built graph was
// built against, in a fixture prepared the same way, which is what makes the two runs comparable.
template <typename A>
struct Side {
	using ConfigType = RunConfigT<A, NoEnv>;

	FixtureT<A> fx;
	CompiledGraph unit;
	InterpreterEngineT<A, NoEnv> interp;
	CompiledEngineT<A, NoEnv> compiled;

	// The unit again over the fast store, with the execution log written and with it turned off.
	CompiledFastEngineT<A, NoEnv> fast;
	CompiledQuietEngineT<A, NoEnv> quiet;

	RunEngineT<A, NoEnv> *engine = nullptr;
	bool onUnit = false;

	// `u` null means the interpreter's side. `steps` false loads the unit's rows without its
	// compiled code, which is how a section holds the unit's own step against the one the machine
	// performs itself: the same rows, the same door, the operation through the registry
	// (`codegen-ops`).
	bool prepare(const corpus::Case &c, const Unit *u, bool journal, bool steps = true) {
		if (!corpus::prepareCase(fx, c, journal)) {
			return false;
		}
		if (!u) {
			interp.setGraph(&fx.graph);
			engine = &interp;
			return true;
		}
		auto tables = u->tables();
		if (!steps) {
			tables.steps = nullptr;
		}
		if (!load(c, tables)) {
			return false;
		}
		compiled.setGraph(&unit);
		engine = &compiled;
		onUnit = true;
		return true;
	}

	// The same unit over the fast store: the frames in a scratch arena the store owns, the run's
	// bookkeeping in host memory, and the run arena the fixture holds untouched. `quietTrace` is the
	// shipped shape - no execution log at all.
	bool prepareFast(const corpus::Case &c, const Unit *u, bool journal, bool quietTrace = false) {
		if (!u || !corpus::prepareCase(fx, c, journal)) {
			return false;
		}
		if (!load(c, u->tables())) {
			return false;
		}
		if (quietTrace) {
			quiet.setGraph(&unit);
			engine = &quiet;
		} else {
			fast.setGraph(&unit);
			engine = &fast;
		}
		onUnit = true;
		return true;
	}

	bool isCompiled() const { return onUnit; }

	// Everything the run left behind, read through the interface's view of the store.
	void snapshot(oracle::RunSnapshot &out) const {
		if (isCompiled()) {
			oracle::takeSnapshot(unit, *engine, fx.arena, fx.report, out);
		} else {
			oracle::takeSnapshot(fx.graph, *engine, fx.arena, fx.report, out);
		}
	}

	// The case's run, up to its first unit. A case is one run: there is no environment to advance
	// between runs.
	Status beginFrame(const ConfigType &config) {
		return engine->begin(fx.ops, fx.arena, config, fx.report);
	}

	// The same run, whole.
	Status runFrame(const ConfigType &config) {
		return engine->run(fx.ops, fx.arena, config, fx.report);
	}

	bool step() { return engine->stepOnce(fx.report); }

private:
	bool load(const corpus::Case &c, const CompiledTables &tables) {
		mem_std::Value diag;
		auto st = unit.load(tables, fx.ops, &diag);
		if (st != Status::Ok || !unit.isValid()) {
			sprt::cout << "       " << c.name << ": the unit does not load: "
					   << data::toString<mem_std::Interface>(diag, false) << "\n";
			return false;
		}
		return true;
	}
};

inline const Unit *findUnit(StringView name) {
	for (uint32_t i = 0; i < UnitCount; ++i) {
		if (Units[i].name == name) {
			return &Units[i];
		}
	}
	return nullptr;
}

} // namespace stappler::test::codegenfx

#endif /* TESTS_FLOW_CODEGEN_CODEGEN_FIXTURE_H_ */
