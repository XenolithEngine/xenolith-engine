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

// The shapes nobody drew.
//
// Every other codegen section runs the hand-written corpus - graphs somebody sat down and thought
// of. This one runs graphs a seed made up: an analysis in the generator that says "these nodes keep
// this order whatever the data does" is wrong first on a shape its author never pictured.
//
// Two halves, and they check different things.
//
// The first half is over every seed: the graph builds, the interpreter runs it, and the generator
// writes it out - twice, to the same bytes. That half needs no compiled code and so can cover as
// many seeds as anybody cares to add.
//
// The second half is over the seeds whose units are committed: the same graph under the
// interpreter, under the unit in the arena store, and under the unit in the fast store, held
// together by the oracle. A shape only counts as covered when all three agree on it.
//
// And a tally, because a generator whose shapes nobody counts can quietly stop making loops and
// stay green about it.

#include "synth_cases.h"
#include "gen/synth_manifest.gen.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::codegenfx;

using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

constexpr CompareOptions Observable{Level::Observable, false, true};
constexpr CompareOptions Exact{Level::Store, false, true};

} // namespace

void performCodegenSynthTests() {
	sprt::cout << "\n== flow codegen: the shapes nobody drew ==\n";

	auto &cases = synthCases();

	// ---- every seed builds, runs, and writes out deterministically -----------------------------

	{
		bool built = true;
		bool deterministic = true;
		uint32_t outcomes[8] = {};

		for (auto &sc : cases) {
			auto c = sc.asCase();
			Fixture fx;
			if (!corpus::prepareCase(fx, c, false)) {
				sprt::cout << "       " << c.name << ": does not build\n";
				built = false;
				continue;
			}
			InterpreterEngineT<Fixture::ArenaType, NoEnv> interp;
			interp.setGraph(&fx.graph);
			auto config = corpus::configFor(fx, c, false);
			interp.run(fx.ops, fx.arena, config, fx.report);
			auto idx = uint32_t(toInt(fx.report.outcome));
			if (idx < 8) {
				++outcomes[idx];
			}

			codegen::Emitted first;
			codegen::Emitted second;
			Fixture ea;
			Fixture eb;
			mem_std::Value diag;
			auto stem = synthUnitName(c.name);
			if (!emitCase(ea, c, first, &diag, stem) || !emitCase(eb, c, second, nullptr, stem)) {
				sprt::cout << "       " << c.name << ": does not emit: "
						   << data::toString<mem_std::Interface>(diag, false) << "\n";
				built = false;
				continue;
			}
			if (first.header != second.header || first.sources != second.sources
					|| first.textHash != second.textHash) {
				sprt::cout << "       " << c.name << ": two emissions differ\n";
				deterministic = false;
			}
		}

		check(built, mem_std::toString("codegen-synth: all ", SynthSeedCount,
							  " seeds build, run and emit"));
		check(deterministic, "codegen-synth: and a seed emitted twice is the same bytes");

		sprt::cout << "   -- what the seeds made --\n";
		for (auto &sc : cases) {
			sprt::cout << "       " << sc.name << ": " << sc.shape.nodes << " nodes, "
					   << sc.shape.execEdges << " exec, " << sc.shape.dataEdges << " data, "
					   << sc.shape.loops << " loops, depth " << sc.shape.depth << " ";
			if (sc.shape.branch) { sprt::cout << "branch "; }
			if (sc.shape.sequence) { sprt::cout << "sequence "; }
			if (sc.shape.loop) { sprt::cout << "loop "; }
			if (sc.shape.nestedLoop) { sprt::cout << "nested "; }
			if (sc.shape.repeat) { sprt::cout << "repeat "; }
			if (sc.shape.crossScope) { sprt::cout << "cross-scope "; }
			if (sc.shape.container) { sprt::cout << "container "; }
			if (sc.shape.dataCondition) { sprt::cout << "data-condition "; }
			if (sc.shape.deadEnd) { sprt::cout << "dead-end "; }
			sprt::cout << "\n";
		}
		sprt::cout << "       outcomes:";
		for (uint32_t i = 0; i < 8; ++i) {
			if (outcomes[i]) {
				sprt::cout << " " << getRunOutcomeName(RunOutcome(i)) << "=" << outcomes[i];
			}
		}
		sprt::cout << "\n";
	}

	// ---- the tally: every construct the generator can make turned up somewhere ------------------

	{
		synth::Shape all;
		for (auto &sc : cases) {
			all.branch = all.branch || sc.shape.branch;
			all.sequence = all.sequence || sc.shape.sequence;
			all.loop = all.loop || sc.shape.loop;
			all.nestedLoop = all.nestedLoop || sc.shape.nestedLoop;
			all.repeat = all.repeat || sc.shape.repeat;
			all.crossScope = all.crossScope || sc.shape.crossScope;
			all.container = all.container || sc.shape.container;
			all.dataCondition = all.dataCondition || sc.shape.dataCondition;
			all.deadEnd = all.deadEnd || sc.shape.deadEnd;
		}
		check(all.branch, "codegen-synth: some seed branches");
		check(all.sequence, "codegen-synth: some seed sequences");
		check(all.loop, "codegen-synth: some seed loops");
		check(all.nestedLoop, "codegen-synth: some seed nests one loop in another");
		check(all.repeat, "codegen-synth: some seed closes an exec cycle with no scope");
		check(all.crossScope, "codegen-synth: some seed reads a value from an enclosing scope");
		check(all.container, "codegen-synth: some seed carries a container constant");
		check(all.dataCondition, "codegen-synth: some seed branches on a computed condition");
		check(all.deadEnd, "codegen-synth: some seed leaves an exec pin going nowhere");
	}

	// ---- the committed seeds, under all three engines --------------------------------------------
	//
	// The half that costs build time and earns it. A shape only counts as covered when the
	// interpreter, the unit over the arena store and the unit over the fast store all agree about
	// it - and the arena side is held at level B in lockstep: the run's own image compared after
	// every unit of work, which is the strongest statement the oracle can make and the one an order
	// analysis will have to go on making.
	//
	// A reduced run walks the first few seeds and the full run walks them all: it is the same
	// comparison over fewer shapes, never a cheaper comparison over the same ones.

	{
		bool prepared = true;
		bool sameArena = true;
		bool sameFast = true;
		bool next = true;
		uint32_t units = 0;

		auto walked = test::sized(SynthUnitCount, 4);
		for (uint32_t u = 0; u < walked; ++u) {
			auto &unit = SynthUnits[u];
			const SynthCase *sc = nullptr;
			for (auto &it : cases) {
				if (StringView(it.name) == unit.name) {
					sc = &it;
					break;
				}
			}
			if (!sc) {
				sprt::cout << "       unit " << unit.name << " names no seed\n";
				prepared = false;
				continue;
			}
			auto c = sc->asCase();

			// Two interpreter sides: the arena comparison walks one in lockstep, and a stepped
			// engine cannot also be the batch engine the fast side is compared against.
			Side<Fixture::ArenaType> i;
			Side<Fixture::ArenaType> k;
			Side<Fixture::ArenaType> bi;
			Side<Fixture::ArenaType> f;
			if (!i.prepare(c, nullptr, false) || !k.prepare(c, &unit, false)
					|| !bi.prepare(c, nullptr, false) || !f.prepareFast(c, &unit, false)) {
				sprt::cout << "       " << c.name << ": does not prepare\n";
				prepared = false;
				continue;
			}

			auto ci = corpus::configFor(i.fx, c, false);
			auto ck = corpus::configFor(k.fx, c, false);
			auto cbi = corpus::configFor(bi.fx, c, false);
			auto cf = corpus::configFor(f.fx, c, false);

			i.beginFrame(ci);
			k.beginFrame(ck);

			uint32_t diverged = flow::InvalidIndex;
			units += oracle::lockstep(
					[&] {
						if (i.engine->peekNext() != k.engine->peekNext()) {
							sprt::cout << "       " << c.name
									   << ": the engines name different next units\n";
							next = false;
						}
						return i.step();
					},
					[&] { return k.step(); }, [&](RunSnapshot &s) { i.snapshot(s); },
					[&](RunSnapshot &s) { k.snapshot(s); }, Exact,
					mem_std::toString(c.name, " interp vs arena"), diverged);
			if (diverged != flow::InvalidIndex) {
				sameArena = false;
			}

			bi.runFrame(cbi);
			f.runFrame(cf);
			RunSnapshot a;
			RunSnapshot b;
			bi.snapshot(a);
			f.snapshot(b);
			if (!oracle::compareRuns(a, b, Observable,
						mem_std::toString(c.name, " interp vs fast"))) {
				sameFast = false;
			}
		}

		check(prepared, mem_std::toString("codegen-synth: all ", walked,
								  " committed seeds prepare on every side"));
		check(next, "codegen-synth: and the two engines name the same next unit at every step");
		check(sameArena,
				mem_std::toString("codegen-synth: over ", units,
						" units the unit's arena run is the interpreter's run, image for image"));
		check(sameFast, "codegen-synth: and the fast run is the same run at level A");
	}
}

} // namespace stappler
