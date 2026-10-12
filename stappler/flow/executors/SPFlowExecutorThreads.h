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

#ifndef STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORTHREADS_H_
#define STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORTHREADS_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowMachine.h"

#include <sprt/runtime/dispatch/task_queue.h>

namespace STAPPLER_VERSIONIZED stappler::flow {

/* The `threads` executor: a block's branches, cut into batches, run on the engine's task pool, each
batch a machine over a store of its own; the machine takes a job back when every batch is done.
Deliveries are asked for on the machine's thread, which is also the thread the pool reports to. A
run that has handed this executor a job must be reset before the executor goes: the machine waits
for its jobs and tells the executor it gave them up. */
class SP_PUBLIC ThreadExecutor final : public BlockExecutor {
public:
	enum class Policy : uint8_t {
		Author, // only blocks whose `executors` lists `threads`
		Always, // every block: the write discipline is one for every executor
	};

	struct Options {
		uint16_t threads = 0; // 0 is the hardware's
		uint32_t batch = 0; // branches per batch; 0 cuts each block into about four per thread
		Policy policy = Policy::Author;
	};

	ThreadExecutor();
	explicit ThreadExecutor(const Options &);
	~ThreadExecutor();

	void submit(const BlockTicket &) override;
	bool poll(BlockTicket &) override;
	void tick() override;
	StringView getName() const override { return StringView("threads"); }

	/* The widest batch the pool cuts on its own, measured. `Options::batch` overrides it,
	deliberately - a test that names a batch means that batch. At a hundred thousand branches the
	sweep is flat-bottomed around 500 and rises on both sides, for four threads and for eight
	alike (docs/performance.md); past about a thousand a batch machine's store costs more than
	the parallelism buys, and a batch's store is built and copied back on the pool too, so a
	batch's fixed cost is divided as well. */
	static constexpr uint32_t MaxBatch = 512;

	// How wide a block must be before a phase of its job is worth handing to the pool. Not the same
	// question as MaxBatch and not the same answer: the machine waits for a phase, so the pool's
	// wake-up is on the critical path rather than hidden behind the front. Below it the phase runs
	// on the machine's thread.
	static constexpr uint32_t PhaseMinBranches = 256;

	uint32_t takesBranches(const BlockTicket &, uint32_t branchCount, uint32_t allowed) override;
	void launch(BranchJob &) override;
	void perform(BranchJob &) override;
	bool takesPhase(const BranchJob &) const override;
	void forget(BranchJob &) override;

	// Waits at most `interval` for a batch to finish; false when none did.
	bool waitDelivery(TimeInterval interval);

	uint16_t getThreadCount() const { return _threads; }

	// The most jobs this executor held at once.
	uint32_t getMaxInFlight() const { return _maxInFlight; }

	// How many phases went to the pool rather than being run where they were asked for. A count,
	// not a time: it is how a check sees that a block was wide enough to be built and taken back in
	// parallel, without timing anything.
	uint32_t getPhaseHandoffs() const { return _phaseHandoffs; }

private:
	Options _options;
	uint16_t _threads = 1;
	Rc<sprt::dispatch::TaskQueue> _queue;
	mem_std::Vector<BlockTicket> _ready;
	mem_std::Vector<BranchJob *> _launched;
	uint32_t _maxInFlight = 0;
	uint32_t _phaseHandoffs = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORTHREADS_H_ */
