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

#ifndef STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORMODEL_H_
#define STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORMODEL_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowMachine.h"

/* What a block costs under each executor, and which one is therefore worth taking. A model and not
an order, because a fixed preference - the GPU if a device was there, else the threads, else the
front - was wrong at both ends: three branches of two operations went to a thread pool that costs
more to wake than the work, and a hundred thousand of them would have stayed on the front if no
device happened to be attached. Both ends are a number question, and the numbers are a measurement;
a host that measured its own machine replaces the table and nothing else. An author who named one
executor gets that executor, because a named one is a demand and not a hint, so the model
arbitrates only where the author left it open. A decision is held because two executors that
differ by a few per cent of a frame would otherwise swap every time N drifted by one: a decision
is kept per block while N stays within a factor of k, and the question is reopened when the set of
candidates changes. */
namespace STAPPLER_VERSIONIZED stappler::flow {

// Nanoseconds, all of it measured. The defaults are the reference host's.
struct SP_PUBLIC ExecutorCosts {
	// `serial`: the machine's own front.
	double machineStepNs = 0; // one unit of work
	double machineBranchNs = 0; // opening and closing one branch's activation

	// `threads`: the same branch in a batch machine of its own, and what the pool costs.
	double threadStepNs = 0;
	double threadBranchNs = 0; // seeded, opened and closed in the batch's own store
	double poolBatchNs = 0; // one batch handed over and taken back
	double poolBlockNs = 0; // the job itself: the scene view, the batch machines, the teardown
	double importBranchNs = 0; // one branch's frames copied back - on the pool too

	/* What a branch costs per branch of the block. The odd one out, and it is measured: a batch of
	a thousand branches costs more per branch than a batch of a hundred, because every open and
	close pays for the store that grew under it. The term covers what the stores alone cost,
	since the copying back is on the pool; it does not overtake the machine's own front inside
	anything measured. */
	double threadGrowthNs = 0;

	uint32_t threads = 0; // 0 folds the thread count into the constants above, as measured

	// `gpu`: the round trip, which is the CPU side of it.
	double gpuBlockNs = 0; // load, dispatch, fold and delivery, whatever N is
	double gpuBranchNs = 0; // one row loaded and one branch replayed
	double gpuByteNs = 0; // per byte of a branch's In and Out records
};

class SP_PUBLIC ExecutorModel {
public:
	struct Decision {
		BlockLaunchKind kind = BlockLaunchKind::Machine;
		uint32_t batch = 0;
		double forecastNs = 0;
		bool held = false; // the cache answered: N had not moved and the candidates had not changed
	};

	// What is remembered per block. `decisions` is what a test counts instead of timing anything.
	struct Entry {
		uint32_t block = InvalidIndex;
		BlockLaunchKind kind = BlockLaunchKind::Machine;
		uint32_t batch = 0;
		uint32_t decidedAt = 0; // the branch count the decision was taken at
		uint32_t candidates = 0;
		uint32_t decisions = 0;
		double forecastNs = 0;
	};

	static const ExecutorCosts &getDefaultCosts();

	const ExecutorCosts &getCosts() const { return _costs; }
	void setCosts(const ExecutorCosts &costs) { _costs = costs; }

	// How far N may drift before the question is asked again: a factor, not a difference. 1
	// disables the cache entirely.
	void setHysteresis(uint32_t k) { _hysteresis = k > 0 ? k : 1; }
	uint32_t getHysteresis() const { return _hysteresis; }

	// The default deadline of a launch: `multiplier` times the forecast, never under `floorMs`.
	void setTimeoutRule(double multiplier, uint64_t floorMs);
	double getTimeoutMultiplier() const { return _timeoutMul; }
	uint64_t getTimeoutFloorMs() const { return _timeoutFloorMs; }

	// What one launch of this kind would cost, in nanoseconds. `batch` is read for Threads only.
	double forecastNs(BlockLaunchKind, const BlockLaunchInfo &, uint32_t batch = 0) const;

	// The cheapest of `candidates` (a BlockExecutorMask; the machine's own front is always in),
	// with the block's last answer kept while it stands.
	Decision choose(uint32_t candidates, uint32_t batch, const BlockLaunchInfo &);

	uint64_t timeoutMs(BlockLaunchKind, const BlockLaunchInfo &, uint32_t batch = 0) const;

	// A graph that was replaced means the block indices name other blocks.
	void reset();
	const Entry *getEntry(uint32_t block) const;

protected:
	// How many units of work one branch is expected to take. A body with a scope inside it has no
	// static answer - the turns are data - so the build's own budget stands in for one.
	uint32_t branchSteps(const BlockShape &) const;

	ExecutorCosts _costs = getDefaultCosts();
	uint32_t _hysteresis = 4;
	double _timeoutMul = 8.0;
	uint64_t _timeoutFloorMs = 250;
	mem_std::Vector<Entry> _cache;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORMODEL_H_ */
