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

#include "SPFlowExecutorModel.h"

#include "SPFlowGpu.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

// The reference host's numbers. They are a measurement and not a guess: the benchmark prints every
// one of them, and a host that measured its own machine passes its own table to the executor.
const ExecutorCosts &ExecutorModel::getDefaultCosts() {
	static const ExecutorCosts costs = [] {
		ExecutorCosts c;
		/* Release, the reference host, 2026-09-16. The unit costs come from a sweep over the
		body's length at one width and one thread count (`ns per branch by body length`), not from
		two rows of the table: bodies of 1, 8 and 32 nodes are 4, 11 and 35 units of work, the slope
		is the unit and the intercept is what a branch costs whatever its body. Reading a unit off
		the width table stops working once the thread count matters. */
		c.machineStepNs = 865; // slope of {3434, 9637, 30312} ns over {4, 11, 35} units
		c.machineBranchNs = 0; // the intercept is -34 ns: a branch's open and close are units

		/* The same sweep on the pool. Four threads is what these describe - the middle of the
		measured range and the column the published tables headline - because the thread count is
		folded in (`threads = 0`) and the scaling is sublinear: eight threads is 1.4 times four,
		not two. A host whose machine is very different measures its own, which is what
		`setCosts` is for. */
		c.threadStepNs = 196; // slope of {1648, 2884, 7729} over the same units
		c.threadBranchNs = 841; // the intercept, less this width's block and growth terms
		c.poolBatchNs = 0; // folded into the block's own cost: the batches are few and alike
		c.poolBlockNs = 11'400; // one branch on the pool (22.5 us) against one on the front (11.1)
		c.importBranchNs = 0; // no longer the machine's alone; what is left of it is in the growth
		c.threadGrowthNs = 0.0118; // was 0.0379 - the frames no longer come back through one thread
		c.threads = 0;

		// The GPU round trip, release, NVIDIA / RADV / llvmpipe alike: it is the CPU side of it.
		// The block's own cost is set where the crossing with `serial` was measured - a thousand
		// branches - rather than where the two widest rows extrapolate to.
		c.gpuBlockNs = 2'000'000;
		c.gpuBranchNs = 1'285;
		c.gpuByteNs = 0; // not separable from one record width; folded into the branch above
		return c;
	}();
	return costs;
}

void ExecutorModel::setTimeoutRule(double multiplier, uint64_t floorMs) {
	_timeoutMul = multiplier > 0.0 ? multiplier : 0.0;
	_timeoutFloorMs = floorMs;
}

uint32_t ExecutorModel::branchSteps(const BlockShape &shape) const {
	auto nodes = sprt::max(uint32_t(1), shape.bodyNodes);
	if (!shape.nested) {
		return nodes;
	}
	// A loop inside the body: how many turns it takes is data, and the only static number anybody
	// has is the budget the build derived from the body. Four turns of it, capped by that budget.
	return shape.maxSteps > 0 ? sprt::min(nodes * 4, shape.maxSteps) : nodes * 4;
}

double ExecutorModel::forecastNs(BlockLaunchKind kind, const BlockLaunchInfo &info,
		uint32_t batch) const {
	auto n = double(info.branchCount);
	auto steps = double(branchSteps(info.shape));
	switch (kind) {
	case BlockLaunchKind::Machine:
		return n * (_costs.machineBranchNs + steps * _costs.machineStepNs);
	case BlockLaunchKind::Threads: {
		// The thread count is not in it, and that is a measurement rather than an omission: two,
		// four and eight threads are within a fifth of each other at every size this was measured
		// at. What the constants below hold is the cost of a branch that went through a pool at
		// all.
		auto threads = double(sprt::max(uint32_t(1), _costs.threads));
		auto batches = batch > 0 ? sprt::max(1.0, sprt::ceil(n / double(batch))) : 1.0;
		return _costs.poolBlockNs + batches * _costs.poolBatchNs
				+ n
					* (_costs.threadBranchNs + steps * _costs.threadStepNs
							+ n * _costs.threadGrowthNs + _costs.importBranchNs)
					/ threads;
	}
	case BlockLaunchKind::Gpu: {
		// The real record stride when the body is lowered; the copies' bytes when it is not.
		double bytes = info.shape.rowBytes;
		if (info.gpu) {
			bytes = double(info.gpu->program.inLayout.size + info.gpu->program.outLayout.size);
		}
		return _costs.gpuBlockNs + n * (_costs.gpuBranchNs + bytes * _costs.gpuByteNs);
	}
	}
	return 0.0;
}

const ExecutorModel::Entry *ExecutorModel::getEntry(uint32_t block) const {
	for (auto &it : _cache) {
		if (it.block == block) {
			return &it;
		}
	}
	return nullptr;
}

void ExecutorModel::reset() { _cache.clear(); }

ExecutorModel::Decision ExecutorModel::choose(uint32_t candidates, uint32_t batch,
		const BlockLaunchInfo &info) {
	auto block = info.shape.block;
	Entry *entry = nullptr;
	for (auto &it : _cache) {
		if (it.block == block && block != InvalidIndex) {
			entry = &it;
		}
	}

	// Held while the question is the same one: the candidates have not changed, and N has not moved
	// by a factor of k either way. Everything else - a device that arrived or went, pipelines that
	// finished compiling - is a different question and reopens it.
	if (entry && entry->candidates == candidates && _hysteresis > 1) {
		auto decided = sprt::max(uint32_t(1), entry->decidedAt);
		auto n = sprt::max(uint32_t(1), info.branchCount);
		if (n <= decided * _hysteresis && decided <= n * _hysteresis) {
			return Decision{entry->kind, entry->batch, entry->forecastNs, true};
		}
	}

	Decision best;
	best.kind = BlockLaunchKind::Machine;
	best.forecastNs = forecastNs(BlockLaunchKind::Machine, info);
	auto consider = [&](BlockLaunchKind kind, uint32_t kindBatch) {
		auto forecast = forecastNs(kind, info, kindBatch);
		if (forecast < best.forecastNs) {
			best.kind = kind;
			best.batch = kindBatch;
			best.forecastNs = forecast;
		}
	};
	if ((candidates & uint32_t(BlockExecutorMask::Threads)) != 0) {
		consider(BlockLaunchKind::Threads, batch);
	}
	if ((candidates & uint32_t(BlockExecutorMask::Gpu)) != 0) {
		consider(BlockLaunchKind::Gpu, 0);
	}

	if (block != InvalidIndex) {
		if (!entry) {
			_cache.emplace_back(Entry{block});
			entry = &_cache.back();
		}
		entry->kind = best.kind;
		entry->batch = best.batch;
		entry->decidedAt = info.branchCount;
		entry->candidates = candidates;
		entry->forecastNs = best.forecastNs;
		++entry->decisions;
	}
	return best;
}

// A block that was handed to somebody else gets a deadline whether its author wrote one or not: a
// device that stops answering and a pool that never finishes both break a run, and a run with no
// number would wait for them forever.
uint64_t ExecutorModel::timeoutMs(BlockLaunchKind kind, const BlockLaunchInfo &info,
		uint32_t batch) const {
	if (kind == BlockLaunchKind::Machine || _timeoutMul <= 0.0) {
		return 0;
	}
	auto forecast = forecastNs(kind, info, batch) * _timeoutMul / 1'000'000.0;
	auto ms = uint64_t(forecast < 0.0 ? 0.0 : forecast);
	return sprt::max(ms, _timeoutFloorMs);
}

} // namespace stappler::flow
