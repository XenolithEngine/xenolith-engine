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

#ifndef TESTS_FLOW_CHECK_RUN_ORACLE_H_
#define TESTS_FLOW_CHECK_RUN_ORACLE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowInterp.h"

#include "flow_check.h"

// The oracle of a run: everything one execution of a graph leaves behind, taken as a value, and
// the comparison that says whether two of them are the same run.
//
// A generated program has to execute a graph EXACTLY as the interpreter does, and the contract of
// "exactly" is stated in levels. Level A - the observable run: the report, the records of every node
// in every activation, and the activation tree.
// Level B - the store: level A plus the run's own arena byte for byte and the ladder of versions
// the journal took. Both are what this header measures, and the comparison prints every channel
// that differs rather than the first, because a divergence is found by reading which channels moved
// together.
//
// It was written against the interpreter alone and proved on it (`interp-oracle`): the batch run
// against the stepped run, and one quantum against another. The compiled engine is held to the same
// snapshot and the same comparison (`codegen-exact-arena`) - which is why the two
// engines of a lockstep are callables rather than a type, the dump is over the graph policy and
// anything that answers the store's read half (a LocalStoreT, or a RunEngineT's view), and the
// snapshot takes whatever has a getLocal().
namespace STAPPLER_VERSIONIZED stappler::test::oracle {

namespace graph = stappler::flow;

enum class Level : uint8_t {
	// A: the report, the records, the activations.
	Observable,
	// B: A plus the run's own arena byte for byte.
	Store,
	// C: A, with the records under the GPU tolerance and the images not compared
	// at all. What a run on a device is held to, because a real sum is not a byte sum.
	Tolerant,
};

struct RunSnapshot {
	// The report, field by field - a RunStep has no comparison of its own, and the log is compared
	// entry by entry so that the first differing unit is named.
	graph::RunOutcome outcome = graph::RunOutcome::Invalid;
	uint32_t stepCount = 0;
	uint32_t sweepCount = 0;
	mem_std::Vector<graph::RunStep> log;
	mem_std::Value diagnostics;

	// The run's store, logically: every record with its NodeState and its fields, the activation
	// tree, the stalled list, the counters. What a debugger would show, in the form compareValues
	// can name a path into.
	mem_std::Value records;

	// The same store keyed by where a record is rather than by the number its activation got
	// records by `id@path`, activations by path. Two runs that opened the same turns in
	// another order agree on this.
	mem_std::Value pathRecords;

	// The log with every activation given as its path.
	struct PathStep {
		graph::RunStepKind kind = graph::RunStepKind::Node;
		graph::NodeId id = 0;
		uint32_t fired = 0;
		mem_std::String path;
	};
	mem_std::Vector<PathStep> pathLog;

	// And physically: the run arena's save() image.
	mem_std::Vector<uint8_t> localImage;

};

// What is compared. `versions` folds the journal's ladder (RunStep::baseVersion / version) into the
// log comparison; off, two runs under different quanta compare as the same run, which is the claim
// interp-step-rollback makes about them. `report` off compares the stores alone - what a rollback
// is judged by, since the report of the run that was undone is deliberately not restored.
struct CompareOptions {
	Level level = Level::Store;
	bool versions = true;
	bool report = true;

	// How the log and the records are compared. `PerActivation` is: the units of each activation
	// in their order, the same set of units overall, the diagnostics as a multiset, the records by path;
	// the step numbers, the versions and the run arena's image are not compared.
	enum class Order : uint8_t {
		Strict,
		PerActivation,
	};
	Order order = Order::Strict;
};

template <typename A>
inline mem_std::Vector<uint8_t> imageOf(const A &arena) {
	mem_std::Vector<uint8_t> image;
	if (!arena.isInitialized()) {
		return image;
	}
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

// The logical content of a run's store. Nothing here is an address: a frame's Addr and a record's
// Addr are where the bytes happen to be, and a store that holds the same run somewhere else is the
// same run (IN-1 taken literally, which is what `attach` relies on).
template <typename Graph, typename Local>
inline void dumpRecords(const Graph &g, const Local &local, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	if (!local.isValid()) {
		out.setBool(false, "valid");
		return;
	}
	// Kind-erased, and it has to be: the two stores keep their frames in arenas of different kinds
	// and this dump is taken of both.
	auto arena = local.getArenaRef();
	if (!arena.isInitialized()) {
		out.setBool(false, "valid");
		return;
	}

	out.setInteger(local.getStep(), "step");
	out.setInteger(local.getPass(), "pass");
	out.setInteger(local.getReadyCount(), "ready");
	out.setInteger(local.getOpenCount(), "open");
	out.setInteger(local.getRecordCount(), "records");

	auto &nodes = out.newArray("nodes");
	local.forEachRecord([&](uint32_t node, uint32_t activation, graph::Addr frame) {
		if (node >= g.getNodeCount()) {
			return true;
		}
		auto &rt = g.getNodeAt(node);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(rt.id), "id");
		entry.setInteger(node, "node");
		entry.setInteger(activation, "activation");

		auto state = local.readState(local.getStateIn(frame, node));
		entry.setInteger(state.inputs, "inputs");
		entry.setInteger(state.produced, "produced");
		entry.setInteger(state.flags, "flags");
		entry.setInteger(state.stallPin, "stallPin");
		entry.setInteger(state.turns, "turns");

		if (rt.localSchema) {
			auto record = local.getRecordIn(frame, node);
			if (record != graph::NullAddr) {
				mem_std::Value fields;
				rt.localSchema->encodeInstance(arena, record, fields);
				entry.setValue(sprt::move(fields), "record");
			}
		}
		nodes.addValue(sprt::move(entry));
		return true;
	});

	auto &activations = out.newArray("activations");
	for (uint32_t i = 0; i < local.getActivationCount(); ++i) {
		auto data = local.readActivation(i);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(i, "activation");
		entry.setInteger(data.scope, "scope");
		entry.setInteger(int64_t(int32_t(data.parent)), "parent");
		entry.setInteger(data.iteration, "iteration");
		if (data.openerKey != graph::NullRecordKey) {
			entry.setInteger(graph::recordKeyNode(data.openerKey), "openerNode");
			entry.setInteger(graph::recordKeyActivation(data.openerKey), "openerActivation");
		}
		entry.setBool(data.frame != graph::NullAddr, "materialized");
		activations.addValue(sprt::move(entry));
	}

	auto &stalled = out.newArray("stalled");
	for (uint32_t i = 0; i < local.getStalledCount(); ++i) {
		auto key = local.getStalledAt(i);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(graph::recordKeyNode(key), "node");
		entry.setInteger(graph::recordKeyActivation(key), "activation");
		stalled.addValue(sprt::move(entry));
	}
}

// Every activation as its path from the root: `scope:iteration`, joined by `/`, with `#k` when the same
// parent opened the same scope and iteration more than once.
template <typename Local>
inline mem_std::Vector<mem_std::String> activationPaths(const Local &local) {
	mem_std::Vector<mem_std::String> out;
	auto count = local.getActivationCount();
	out.resize(count);
	for (uint32_t i = 0; i < count; ++i) {
		auto data = local.readActivation(i);
		if (data.parent == graph::NullActivation || data.parent >= i) {
			out[i] = mem_std::String("root");
			continue;
		}
		auto base = mem_std::toString(out[data.parent], "/", data.scope, ":", data.iteration);
		uint32_t seen = 0;
		auto prefix = mem_std::toString(base, "#");
		for (uint32_t k = 0; k < i; ++k) {
			if (out[k] == base || StringView(out[k]).starts_with(StringView(prefix))) {
				++seen;
			}
		}
		out[i] = seen == 0 ? base : mem_std::toString(base, "#", seen);
	}
	return out;
}

template <typename Graph, typename Local>
inline void dumpRecordsByPath(const Graph &g, const Local &local, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	if (!local.isValid()) {
		out.setBool(false, "valid");
		return;
	}
	auto arena = local.getArenaRef();
	auto paths = activationPaths(local);
	out.setInteger(local.getRecordCount(), "records");
	out.setInteger(local.getOpenCount(), "open");
	out.setInteger(local.getReadyCount(), "ready");

	auto &nodes = out.newDict("nodes");
	local.forEachRecord([&](uint32_t node, uint32_t activation, graph::Addr frame) {
		if (node >= g.getNodeCount()) {
			return true;
		}
		auto &rt = g.getNodeAt(node);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		auto state = local.readState(local.getStateIn(frame, node));
		entry.setInteger(state.inputs, "inputs");
		entry.setInteger(state.produced, "produced");
		entry.setInteger(state.flags, "flags");
		entry.setInteger(state.stallPin, "stallPin");
		entry.setInteger(state.turns, "turns");
		if (rt.localSchema) {
			auto record = local.getRecordIn(frame, node);
			if (record != graph::NullAddr) {
				mem_std::Value fields;
				rt.localSchema->encodeInstance(arena, record, fields);
				entry.setValue(sprt::move(fields), "record");
			}
		}
		nodes.setValue(sprt::move(entry), mem_std::toString(rt.id, "@", paths[activation]));
		return true;
	});

	// The branches that failed: what they left in their records is nobody's, and a device is not
	// held to it - the loader stops such a branch before dispatch, so the values a CPU run happened to
	// write are simply not there. Named here, filtered by the level-C comparison.
	auto &failed = out.newArray("failedBranches");
	for (uint32_t i = 0; i < local.getActivationCount(); ++i) {
		auto data = local.readActivation(i);
		if (data.scope >= g.getScopeCount()
				|| g.getScopeAt(data.scope).kind != graph::ScopeKind::Parallel) {
			continue;
		}
		auto frame = local.activationFrame(i);
		auto status = graph::readBranchInt(arena, frame, graph::BranchHeader::StatusOffset);
		if ((status & graph::BranchHeader::Failed) != 0) {
			failed.addString(paths[i]);
		}
	}

	auto &activations = out.newDict("activations");
	for (uint32_t i = 0; i < local.getActivationCount(); ++i) {
		auto data = local.readActivation(i);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(data.scope, "scope");
		entry.setInteger(data.iteration, "iteration");
		if (data.openerKey != graph::NullRecordKey) {
			auto opener = graph::recordKeyNode(data.openerKey);
			auto at = graph::recordKeyActivation(data.openerKey);
			entry.setInteger(opener < g.getNodeCount() ? int64_t(g.getNodeAt(opener).id) : -1, "opener");
			entry.setString(at < paths.size() ? paths[at] : mem_std::String("?"), "openerPath");
		}
		entry.setBool(data.frame != graph::NullAddr, "materialized");
		activations.setValue(sprt::move(entry), paths[i]);
	}

	mem_std::Vector<mem_std::String> stalled;
	for (uint32_t i = 0; i < local.getStalledCount(); ++i) {
		auto key = local.getStalledAt(i);
		auto node = graph::recordKeyNode(key);
		auto act = graph::recordKeyActivation(key);
		stalled.emplace_back(mem_std::toString(node < g.getNodeCount() ? int64_t(g.getNodeAt(node).id) : -1,
				"@", act < paths.size() ? paths[act] : mem_std::String("?")));
	}
	sprt::sort(stalled.begin(), stalled.end());
	auto &list = out.newArray("stalled");
	for (auto &it : stalled) {
		list.addString(it);
	}
}

// Everything a run left behind, from the pieces a host holds.
template <typename Graph, typename Run, typename A>
inline void takeSnapshot(const Graph &g, const Run &run, const A &arena,
		const graph::RunReport &report, RunSnapshot &out, bool paths = false) {
	out = RunSnapshot();
	out.outcome = report.outcome;
	out.stepCount = report.stepCount;
	out.sweepCount = report.sweepCount;
	out.log = report.log;
	out.diagnostics = report.diagnostics;

	dumpRecords(g, run.getLocal(), out.records);
	if (paths) {
		auto &local = run.getLocal();
		auto activation = local.isValid() ? activationPaths(local) : mem_std::Vector<mem_std::String>();
		for (auto &it : report.log) {
			out.pathLog.emplace_back(RunSnapshot::PathStep{it.kind, it.id, it.fired,
				it.activation < activation.size() ? activation[it.activation] : mem_std::String("?")});
		}
		dumpRecordsByPath(g, local, out.pathRecords);
	}
	out.localImage = imageOf(arena);
}

// Whether two snapshots are the same run, under `label`. Every channel that differs is printed,
// not only the first: which of them moved together is what says where a divergence came from.
inline bool compareRuns(const RunSnapshot &a, const RunSnapshot &b, const CompareOptions &opts,
		StringView label) {
	bool same = true;
	auto differ = [&](StringView what) {
		sprt::cout << "       " << label << ": " << what << " differs\n";
		same = false;
	};

	if (opts.report) {
		if (a.outcome != b.outcome) {
			sprt::cout << "       " << label << ": outcome " << graph::getRunOutcomeName(a.outcome)
					   << " != " << graph::getRunOutcomeName(b.outcome) << "\n";
			same = false;
		}
		if (a.stepCount != b.stepCount) {
			sprt::cout << "       " << label << ": stepCount " << a.stepCount
					   << " != " << b.stepCount << "\n";
			same = false;
		}
		if (a.sweepCount != b.sweepCount) {
			differ(StringView("sweepCount"));
		}

		if (a.log.size() != b.log.size()) {
			sprt::cout << "       " << label << ": log has " << a.log.size() << " units, expected "
					   << b.log.size() << "\n";
			same = false;
		}
		auto units = a.log.size() < b.log.size() ? a.log.size() : b.log.size();
		if (opts.order == CompareOptions::Order::PerActivation) {
			units = 0;
			// The units of each activation in their order, the whole log as a multiset. An activation
			// a block is delivered into is compared as a set: where the delivery lands among its units,
			// and so what ran before it, is the executor's.
			mem_std::Set<mem_std::String> delivered;
			for (auto snap : {&a, &b}) {
				for (auto &it : snap->pathLog) {
					if (it.kind == graph::RunStepKind::Deliver) {
						delivered.emplace(it.path);
					}
				}
			}
			auto perActivation = [&](const RunSnapshot &s) {
				mem_std::Map<mem_std::String, mem_std::Vector<mem_std::String>> out;
				for (auto &it : s.pathLog) {
					auto found = out.find(it.path);
					if (found == out.end()) {
						found = out.emplace(it.path, mem_std::Vector<mem_std::String>()).first;
					}
					found->second.emplace_back(mem_std::toString(uint32_t(it.kind), ":", it.id, ":", it.fired));
				}
				for (auto &it : out) {
					if (delivered.find(it.first) != delivered.end()) {
						sprt::sort(it.second.begin(), it.second.end());
					}
				}
				return out;
			};
			auto everything = [](const RunSnapshot &s) {
				mem_std::Vector<mem_std::String> out;
				for (auto &it : s.pathLog) {
					out.emplace_back(mem_std::toString(uint32_t(it.kind), ":", it.id, ":", it.fired, "@", it.path));
				}
				sprt::sort(out.begin(), out.end());
				return out;
			};
			auto pa = perActivation(a);
			auto pb = perActivation(b);
			if (pa != pb) {
				for (auto &it : pb) {
					auto found = pa.find(it.first);
					if (found == pa.end() || found->second != it.second) {
						sprt::cout << "       " << label << ": the units of activation " << it.first
								   << " differ\n";
						break;
					}
				}
				same = false;
			}
			if (everything(a) != everything(b)) {
				differ(StringView("the set of units"));
			}
			auto diagnostics = [](const mem_std::Value &v) {
				mem_std::Vector<mem_std::String> out;
				if (v.isArray()) {
					for (auto &it : v.asArray()) {
						out.emplace_back(data::toString<mem_std::Interface>(it, false));
					}
				}
				sprt::sort(out.begin(), out.end());
				return out;
			};
			if (diagnostics(a.diagnostics) != diagnostics(b.diagnostics)) {
				differ(StringView("the diagnostics"));
			}
		}
		for (size_t i = 0; i < units; ++i) {
			auto &x = a.log[i];
			auto &y = b.log[i];
			bool unit = x.step == y.step && x.kind == y.kind && x.node == y.node
					&& x.activation == y.activation && x.id == y.id && x.fired == y.fired;
			bool ladder = !opts.versions
					|| (x.baseVersion == y.baseVersion && x.version == y.version);
			if (!unit || !ladder) {
				sprt::cout << "       " << label << ": log unit " << i << " (node id " << x.id
						   << " / " << y.id << ", activation " << x.activation << " / "
						   << y.activation << (unit ? ", versions" : "") << ") differs\n";
				same = false;
				break;
			}
		}

		if (opts.order == CompareOptions::Order::Strict
				&& !compareValues(a.diagnostics, b.diagnostics, mem_std::toString(label, " diagnostics"))) {
			same = false;
		}
	}

	const bool tolerant = opts.level == Level::Tolerant;
	auto values = [&](const mem_std::Value &x, const mem_std::Value &y, StringView what) {
		return tolerant ? compareValuesNearly(x, y, what) : compareValues(x, y, what);
	};

	// Level C drops the records of a branch that failed, on both sides: a failed branch leaves them to nobody, and
	// what an executor writes there is its own business. The branch's header, log and diagnostics are
	// compared as for any other.
	auto withoutFailed = [](const RunSnapshot &x, const RunSnapshot &y) {
		mem_std::Vector<mem_std::String> paths;
		for (auto snap : {&x, &y}) {
			for (auto &it : snap->pathRecords.getArray("failedBranches")) {
				paths.emplace_back(it.getString());
			}
		}
		auto out = x.pathRecords;
		auto &nodes = out.getDict("nodes");
		mem_std::Vector<mem_std::String> drop;
		for (auto &it : nodes) {
			auto at = StringView(it.first).find('@');
			if (at == maxOf<size_t>()) {
				continue;
			}
			auto where = StringView(it.first).sub(at + 1);
			for (auto &path : paths) {
				if (where.starts_with(StringView(path))) {
					drop.emplace_back(it.first);
					break;
				}
			}
		}
		for (auto &it : drop) {
			nodes.erase(it);
		}
		return out;
	};

	if (opts.order == CompareOptions::Order::PerActivation) {
		auto ra = tolerant ? withoutFailed(a, b) : a.pathRecords;
		auto rb = tolerant ? withoutFailed(b, a) : b.pathRecords;
		if (!values(ra, rb, mem_std::toString(label, " records by path"))) {
			same = false;
		}
	} else if (!values(a.records, b.records, mem_std::toString(label, " records"))) {
		same = false;
	}


	if (opts.level == Level::Store && opts.order == CompareOptions::Order::Strict) {
		if (!compareBytes(BytesView(a.localImage.data(), a.localImage.size()),
					BytesView(b.localImage.data(), b.localImage.size()),
					mem_std::toString(label, " local image"), 4)) {
			same = false;
		}
	}
	return same;
}

// Two engines side by side: each performs one unit, both are snapshotted, the snapshots are
// compared, and so on until both say the run is over. `step` returns whether the run goes on
// (InterpreterT::stepOnce's contract); `snap` fills a snapshot.
//
// Returns the number of units both performed. `diverged` is the unit after which the first
// difference appeared - a snapshot that differs, or one engine finishing before the other - or
// InvalidIndex when there was none. The unit that closes the run is compared too: the closing
// sweep writes to the store, and it has to write the same thing.
template <typename StepA, typename StepB, typename SnapA, typename SnapB>
inline uint32_t lockstep(StepA &&stepA, StepB &&stepB, SnapA &&snapA, SnapB &&snapB,
		const CompareOptions &opts, StringView label, uint32_t &diverged) {
	diverged = graph::InvalidIndex;
	uint32_t units = 0;
	while (true) {
		bool moreA = stepA();
		bool moreB = stepB();

		RunSnapshot a;
		RunSnapshot b;
		snapA(a);
		snapB(b);
		if (!compareRuns(a, b, opts, mem_std::toString(label, " after unit ", units))) {
			diverged = units;
			return units;
		}
		if (moreA != moreB) {
			sprt::cout << "       " << label << ": after unit " << units << " one engine is "
					   << (moreA ? "still running" : "finished") << " and the other is "
					   << (moreB ? "still running" : "finished") << "\n";
			diverged = units;
			return units;
		}
		if (!moreA) {
			return units;
		}
		++units;
	}
}

} // namespace stappler::test::oracle

#endif /* TESTS_FLOW_CHECK_RUN_ORACLE_H_ */
