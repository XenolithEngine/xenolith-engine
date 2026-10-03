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

// The arena mode, held to the interpreter at level B under every kind of arena.
//
// A unit run over the arena store is the interpreter's machine over the unit's rows
// (CompiledRunT<A>, SPFlowCompiled.h), and this section is where "the same machine" becomes a
// measurement: for every graph of the corpus and every kind of arena, the interpreter and the
// compiled engine are stepped side by side and both arenas are compared byte for byte after every
// unit of work; under every quantum, with the journal's ladder of versions folded in; across a cut
// in either direction, with one engine picking up the bytes the other left at unit seventeen; and
// through an operation's refusal, where both stores have to come back to the same version.
//
// Both engines are driven through the executor interface (RunEngineT<A>, SPFlowEngine.h) and
// snapshotted through its view of the store, so the interface is proved to carry a whole run on
// the way - which is what a host will hold either engine by. The comparison is the oracle that
// proved the interpreter against itself (`interp-oracle`), unchanged.

#include "codegen_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../check/run_oracle.h"

#include <type_traits>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::codegenfx;
using oracle::CompareOptions;
using oracle::Level;
using oracle::RunSnapshot;

const char *quantumName(flow::RollbackQuantum q) {
	switch (q) {
	case flow::RollbackQuantum::Step: return "Step";
	case flow::RollbackQuantum::Failable: return "Failable";
	case flow::RollbackQuantum::Run: return "Run";
	}
	return "?";
}

// Where a run is cut for the continuation block: every unit of a short run, a spread of a long
// one, and always unit seventeen when there is one. The reduced run keeps three; the full run
// keeps up to forty. Sorted and unique, so a cut is tried once.
mem_std::Vector<uint32_t> cutsFor(uint32_t units) {
	mem_std::Vector<uint32_t> out;
	auto add = [&](uint32_t k) {
		if (k >= units) {
			return;
		}
		for (auto it : out) {
			if (it == k) {
				return;
			}
		}
		out.emplace_back(k);
	};
	if (units == 0) {
		out.emplace_back(0);
		return out;
	}
	if (test::full()) {
		if (units <= 40) {
			for (uint32_t k = 0; k < units; ++k) { add(k); }
		} else {
			for (uint32_t i = 0; i < 40; ++i) { add(uint32_t(uint64_t(i) * units / 40)); }
			add(17);
			add(units - 1);
		}
	} else {
		add(0);
		add(17 < units ? 17 : units / 2);
		add(units - 1);
	}
	sprt::sort(out.begin(), out.end());
	return out;
}

// ---- the blocks, over one arena kind ------------------------------------------------------------

template <typename A>
void exactArena(StringView kind) {
	sprt::cout << "   -- " << kind << " --\n";
	auto label = [&](const corpus::Case &c, StringView what) {
		return mem_std::toString(kind, " ", c.name, " ", what);
	};

	// ---- lockstep, level B, every unit ---------------------------------------------------------------
	//
	// interp-oracle's lockstep with the compiled engine on the other side: after every unit of work
	// the report, the records, both arenas byte for byte. Beside it, a batch run of the compiled
	// engine on a third fixture, which has to end where the stepped one ends - two runs of the
	// compiled code are the same bytes - and, before every unit, both engines have to name the same
	// next unit.

	{
		bool prepared = true;
		bool identity = true;
		bool units = true;
		bool counted = true;
		bool batch = true;
		bool next = true;
		bool named = true;
		for (auto &c : corpus::Cases) {
			auto u = findUnit(c.name);
			Side<A> i;
			Side<A> k;
			Side<A> b;
			if (!u || !i.prepare(c, nullptr, false) || !k.prepare(c, u, false)
					|| !b.prepare(c, u, false)) {
				sprt::cout << "       " << kind << " " << c.name << ": does not prepare\n";
				prepared = false;
				continue;
			}
			if (k.unit.getIdentity().assetHash != k.fx.asset.getContentHash()) {
				sprt::cout << "       " << kind << " " << c.name
						   << ": the unit's asset hash is not the fixture's asset\n";
				identity = false;
			}
			if (i.engine->getEngineName() != StringView("interp")
					|| k.engine->getEngineName() != StringView("arena")) {
				named = false;
			}

			auto ci = corpus::configFor(i.fx, c, false);
			auto ck = corpus::configFor(k.fx, c, false);
			auto cb = corpus::configFor(b.fx, c, false);

			i.beginFrame(ci);
			k.beginFrame(ck);
			b.runFrame(cb);

			uint32_t diverged = 0;
			auto walked = oracle::lockstep(
					[&] {
						if (i.engine->peekNext() != k.engine->peekNext()) {
							sprt::cout << "       " << kind << " " << c.name
									   << ": the engines name different next units\n";
							next = false;
						}
						return i.step();
					},
					[&] { return k.step(); }, [&](RunSnapshot &s) { i.snapshot(s); },
					[&](RunSnapshot &s) { k.snapshot(s); }, CompareOptions{Level::Store},
					label(c, "interp vs arena"), diverged);
			if (diverged != flow::InvalidIndex) {
				units = false;
			}
			if (walked != b.fx.report.stepCount) {
				sprt::cout << "       " << kind << " " << c.name << ": lockstep walked " << walked
						   << " units, the compiled batch run took " << b.fx.report.stepCount
						   << "\n";
				counted = false;
			}

			RunSnapshot stepped;
			RunSnapshot whole;
			k.snapshot(stepped);
			b.snapshot(whole);
			if (!oracle::compareRuns(whole, stepped, CompareOptions{Level::Store},
						label(c, "arena batch vs stepped"))) {
				batch = false;
			}
		}
		check(prepared, mem_std::toString("codegen-exact-arena[", kind, "]: all ",
									corpus::CaseCount,
									" cases prepare on both sides, the unit loaded"));
		check(identity, mem_std::toString("codegen-exact-arena[", kind,
								  "]: every unit carries the hash of the asset the fixture built"));
		check(named, mem_std::toString("codegen-exact-arena[", kind,
							   "]: the engines name themselves interp and arena"));
		check(units, mem_std::toString("codegen-exact-arena[", kind,
							   "]: interpreter and compiled engine agree after every unit, level B"));
		check(next, mem_std::toString("codegen-exact-arena[", kind,
							  "]: and name the same next unit before each"));
		check(counted, mem_std::toString("codegen-exact-arena[", kind,
								 "]: the lockstep counts the units the compiled batch run took"));
		check(batch, mem_std::toString("codegen-exact-arena[", kind,
							   "]: two runs of the compiled code are the same bytes"));
	}

	// ---- three quanta, the ladder included ---------------------------------------------------------------
	//
	// With a journal over both stores, under Step, Failable and Run: the same units in the same
	// order, the same bytes, and the same versions - RunStep::baseVersion and version unit by unit,
	// and the journal's count of boundaries at the end. A case that ends in a refusal is compared
	// too: its last unit is the one the rollback lands in, and it has to land where the
	// interpreter's does.

	if constexpr (A::IsTracked) {
		bool prepared = true;
		bool agreed = true;
		bool ladders = true;
		// The reduced run keeps Failable alone: it is the quantum whose boundaries depend on what
		// the operations declare, so it is the arm where the ladder is least likely to agree by
		// accident. Step is walked by the refusal block below in any case.
		const flow::RollbackQuantum quanta[] = {flow::RollbackQuantum::Failable,
			flow::RollbackQuantum::Step, flow::RollbackQuantum::Run};
		const uint32_t quantumCount = test::full() ? 3 : 1;

		for (auto &c : corpus::Cases) {
			auto u = findUnit(c.name);
			for (uint32_t qi = 0; qi < quantumCount; ++qi) {
				auto q = quanta[qi];
				Side<A> i;
				Side<A> k;
				if (!u || !i.prepare(c, nullptr, true) || !k.prepare(c, u, true)) {
					prepared = false;
					continue;
				}
				auto ci = corpus::configFor(i.fx, c, true, q);
				auto ck = corpus::configFor(k.fx, c, true, q);
				auto beforeI = i.fx.journal.getMetrics().versions;
				auto beforeK = k.fx.journal.getMetrics().versions;

				i.beginFrame(ci);
				k.beginFrame(ck);
				uint32_t diverged = 0;
				oracle::lockstep([&] { return i.step(); }, [&] { return k.step(); },
						[&](RunSnapshot &s) { i.snapshot(s); },
						[&](RunSnapshot &s) { k.snapshot(s); },
						CompareOptions{Level::Store, true, true},
						label(c, mem_std::toString("under ", quantumName(q))), diverged);
				if (diverged != flow::InvalidIndex) {
					agreed = false;
				}
				auto tookI = i.fx.journal.getMetrics().versions - beforeI;
				auto tookK = k.fx.journal.getMetrics().versions - beforeK;
				if (tookI != tookK) {
					sprt::cout << "       " << kind << " " << c.name << " under " << quantumName(q)
							   << ": " << tookI << " boundaries against " << tookK << "\n";
					ladders = false;
				}
			}
		}
		check(prepared, mem_std::toString("codegen-exact-arena[", kind,
								  "]: every case prepares with a journal on both sides, under every "
								  "quantum"));
		check(agreed, mem_std::toString("codegen-exact-arena[", kind,
								"]: under every quantum the two engines leave the same run, the "
								"ladder of versions included"));
		check(ladders, mem_std::toString("codegen-exact-arena[", kind,
								 "]: and take the same number of boundaries"));
	}

	// ---- a cut in either direction -------------------------------------------------------------------
	//
	// A run taken across engines: one engine runs up to a unit, its arena is saved, the image is
	// adopted into a fresh arena, and the other engine attaches to it and finishes. The end has to be
	// the end the interpreter reaches running the case whole - the report carried across the cut
	// included. Both directions, at every cut of a short run and a spread of a long one, unit
	// seventeen among them.

	{
		bool prepared = true;
		bool attached = true;
		bool same = true;
		uint32_t cuts = 0;
		for (auto &c : corpus::Cases) {
			auto u = findUnit(c.name);
			Side<A> ref;
			if (!u || !ref.prepare(c, nullptr, false)) {
				prepared = false;
				continue;
			}
			auto refConfig = corpus::configFor(ref.fx, c, false);
			ref.runFrame(refConfig);
			RunSnapshot end;
			ref.snapshot(end);

			for (auto cut : cutsFor(ref.fx.report.stepCount)) {
				for (uint32_t dir = 0; dir < 2; ++dir) {
					// The arena the continuation runs in, declared before the engine that will attach
					// to it: the machine destroys its local store in the arena it was attached to, so
					// the arena has to outlive it.
					A localElsewhere;

					// dir 0: the interpreter runs, the unit continues. dir 1: the reverse.
					Side<A> src;
					Side<A> dst;
					if (!src.prepare(c, dir == 0 ? nullptr : u, false)
							|| !dst.prepare(c, dir == 0 ? u : nullptr, false)) {
						prepared = false;
						continue;
					}
					auto srcConfig = corpus::configFor(src.fx, c, false);
					src.beginFrame(srcConfig);
					for (uint32_t j = 0; j < cut; ++j) { src.step(); }

					// The cut: the image, and the report carried by hand.
					auto localImage = takeImage(src.fx.arena);
					auto carried = src.fx.report;

					if (localElsewhere.adopt(BytesView(localImage.data(), localImage.size()))
							!= Status::Ok) {
						sprt::cout << "       " << kind << " " << c.name << " cut " << cut
								   << ": the image does not adopt\n";
						attached = false;
						continue;
					}

					auto dstConfig = corpus::configFor(dst.fx, c, false);
					if (dst.engine->attach(dst.fx.ops, localElsewhere, dstConfig, carried)
							!= Status::Ok) {
						sprt::cout << "       " << kind << " " << c.name << " cut " << cut << ": "
								   << (dir == 0 ? "the unit" : "the interpreter")
								   << " does not attach\n";
						attached = false;
						continue;
					}
					dst.engine->finish(carried);
					++cuts;

					RunSnapshot got;
					if (dst.isCompiled()) {
						oracle::takeSnapshot(dst.unit, *dst.engine, localElsewhere, carried, got);
					} else {
						oracle::takeSnapshot(dst.fx.graph, *dst.engine, localElsewhere, carried, got);
					}
					if (!oracle::compareRuns(got, end, CompareOptions{Level::Store},
								label(c, mem_std::toString("cut at unit ", cut,
											 dir == 0 ? " interp -> arena" : " arena -> interp")))) {
						same = false;
					}
				}
			}
		}
		check(prepared, mem_std::toString("codegen-exact-arena[", kind,
								  "]: every case prepares for the continuation block"));
		check(attached, mem_std::toString("codegen-exact-arena[", kind, "]: at every cut (", cuts,
								  " of them) the other engine attaches to the adopted image"));
		check(same, mem_std::toString("codegen-exact-arena[", kind,
							  "]: and finishes where the interpreter finishes running the case "
							  "whole, the arena byte for byte, the carried report included"));
	}

	// ---- a refusal, under a journal -------------------------------------------------------------------
	//
	// Every case that ends in an operation's refusal, with a journal over both stores under Step:
	// after the failing unit, the compiled engine's store is what it was before it - report
	// aside, which is deliberately not restored - and they are what the interpreter's are, the
	// version the rollback landed on included.

	if constexpr (A::IsTracked) {
		bool prepared = true;
		bool undone = true;
		bool agreed = true;
		uint32_t refusals = 0;
		for (auto &c : corpus::Cases) {
			if (c.outcome != flow::RunOutcome::OpError) {
				continue;
			}
			auto u = findUnit(c.name);
			Side<A> i;
			Side<A> k;
			if (!u || !i.prepare(c, nullptr, true) || !k.prepare(c, u, true)) {
				prepared = false;
				continue;
			}
			auto ci = corpus::configFor(i.fx, c, true, flow::RollbackQuantum::Step);
			auto ck = corpus::configFor(k.fx, c, true, flow::RollbackQuantum::Step);

			i.beginFrame(ci);
			k.beginFrame(ck);

			RunSnapshot before;
			while (true) {
				k.snapshot(before);
				bool moreI = i.step();
				bool moreK = k.step();
				if (!moreK || !moreI) {
					if (moreI != moreK) {
						agreed = false;
					}
					break;
				}
			}
			if (k.fx.report.outcome != flow::RunOutcome::OpError) {
				continue;
			}
			++refusals;

			RunSnapshot afterK;
			RunSnapshot afterI;
			k.snapshot(afterK);
			i.snapshot(afterI);
			if (!oracle::compareRuns(before, afterK, CompareOptions{Level::Store, false, false},
						label(c, "arena before vs after"))) {
				undone = false;
			}
			if (!oracle::compareRuns(afterK, afterI, CompareOptions{Level::Store, true, true},
						label(c, "after the refusal"))) {
				agreed = false;
			}
			if (i.fx.journal.getMetrics().versions != k.fx.journal.getMetrics().versions
					|| i.fx.journal.getMetrics().rollbacks != k.fx.journal.getMetrics().rollbacks) {
				sprt::cout << "       " << kind << " " << c.name
						   << ": the journals took different versions or rollbacks\n";
				agreed = false;
			}
		}
		check(prepared, mem_std::toString("codegen-exact-arena[", kind,
								  "]: every refusing case prepares with a journal"));
		check(refusals > 0 && undone,
				mem_std::toString("codegen-exact-arena[", kind, "]: after each of the ", refusals,
						" refusals the compiled engine's store is what it was before the "
						"node, byte for byte"));
		check(agreed, mem_std::toString("codegen-exact-arena[", kind,
								"]: and are the interpreter's, the version and the report "
								"included"));
	}
}

} // namespace

void performCodegenExactArenaTests() {
	sprt::cout << "\n== flow codegen: the arena mode against the interpreter, under every kind ==\n";

	// ---- the interface itself, before any run ---------------------------------------------------------
	//
	// An engine without a graph refuses to run rather than running nothing; given one, it runs it.
	// The three kinds follow.

	{
		CompiledEngineT<Arena, NoEnv> engine;
		check(!engine.hasGraph() && engine.getEngineName() == StringView("arena"),
				"codegen-exact-arena: a compiled engine names itself before it holds anything");
		Arena arena;
		OpRegistry ops;
		flow::RunReport report;
		check(arena.init() && ops.init(),
				"codegen-exact-arena: an arena and a registry for the refusal");
		check(engine.run(ops, arena, RunConfig(), report)
								!= Status::Ok
						&& report.outcome == flow::RunOutcome::Invalid
						&& engine.getState() == flow::RunState::Idle,
				"codegen-exact-arena: and refuses to run with no graph, leaving nothing behind");
		check(engine.peekNext() == flow::NullRecordKey && !engine.getLocal().isValid(),
				"codegen-exact-arena: with no next unit and no store to look at");
	}

	// The kinds are the sweep's arms. The full run walks all three - a PlainArena announces
	// nothing and is what a shipped game runs on, a TrackedArena is journalled, a ShadowArena
	// proves the barrier had no holes - and the reduced run keeps the suite's own kind, the one
	// every other section runs on, so that a reduced run is a run of every block on one arm rather
	// than of some blocks on three. ShadowArena is TrackedArena in a release build, and running the
	// same kind twice would prove nothing twice.
	constexpr bool shadowIsItsOwnKind = !std::is_same_v<flow::value::ShadowArena, flow::value::TrackedArena>;
	if (test::full()) {
		exactArena<flow::value::PlainArena>(StringView("plain"));
		exactArena<flow::value::TrackedArena>(StringView("tracked"));
		if constexpr (shadowIsItsOwnKind) {
			exactArena<flow::value::ShadowArena>(StringView("shadow"));
		}
	} else {
		exactArena<Arena>(shadowIsItsOwnKind ? StringView("shadow") : StringView("tracked"));
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
