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

#ifndef STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORCHOICE_H_
#define STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORCHOICE_H_

#include "SPFlowExecutorGpu.h"
#include "SPFlowExecutorModel.h"
#include "SPFlowExecutorThreads.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

/* One executor over the others. A run is given one executor, and which one a block should go to is
a property of the block and not of the run: the author's `executors`, what is there to take it, and
- where the author left the question open - what it would cost. Three steps, in this order.
1. The candidates. The machine's own front always; the threads and the GPU when the author named
   them, or named nothing at all. An empty list means the heuristic chooses among all three, and
   this is the only place where that is interpreted.
2. What is there. Each candidate is asked, with a mask that names it, so that its own
   `Policy::Author` does not veto an author who named nothing. A device that is gone, a shader that
   was never attached, pipelines still compiling: the candidate drops out, this frame.
3. The demand, then the model. An author who named exactly one executor gets it, because a named
   one is a demand and a cost model is not entitled to overrule it. Everywhere else
   `ExecutorModel` arbitrates on the measured numbers, and holds its answer per block so a block
   does not move between the CPU and a device because N drifted by one.
It holds none of the three: the caller owns them and outlives the run. A graph that is replaced
under a living executor must call `forgetChoices()`, because the model's cache is keyed by block
index and block 2 of another graph is another block. */
class SP_PUBLIC ChoiceExecutor final : public BlockExecutor {
public:
	struct Options {
		GpuExecutor *gpu = nullptr;
		ThreadExecutor *threads = nullptr;
		const ExecutorCosts *costs = nullptr; // null is the measured default table
	};

	explicit ChoiceExecutor(const Options &);

	// The model, to read what it decided and to tell it what this machine costs.
	ExecutorModel &getModel() { return _model; }
	const ExecutorModel &getModel() const { return _model; }

	// A graph was replaced, or a run was thrown away: the decisions were about other blocks.
	void forgetChoices();

	void submit(const BlockTicket &) override;
	bool poll(BlockTicket &) override;
	void tick() override;
	StringView getName() const override { return StringView("choice"); }

	BlockLaunch chooseLaunch(const BlockTicket &, const BlockLaunchInfo &) override;
	uint64_t chooseTimeoutMs(const BlockTicket &, const BlockLaunchInfo &,
			const BlockLaunch &) override;
	void launch(BranchJob &) override;
	void perform(BranchJob &) override;
	bool takesPhase(const BranchJob &) const override;
	void forget(BranchJob &) override;
	bool launchGpu(GpuBlockJob &) override;
	void forgetGpu(GpuBlockJob &) override;

	bool waitDelivery(TimeInterval);

	// What was chosen for a block, and how often each kind was: what a test asks instead of
	// guessing from timings.
	BlockLaunchKind getChosen(const BlockTicket &) const;
	uint32_t getChoiceCount(BlockLaunchKind) const;

private:
	struct Choice {
		uint32_t node = InvalidIndex;
		uint32_t activation = 0;
		BlockLaunchKind kind = BlockLaunchKind::Machine;
	};

	Options _options;
	ExecutorModel _model;
	mem_std::Vector<BlockTicket> _ready;
	mem_std::Vector<Choice> _choices;
	uint32_t _counts[3] = {0, 0, 0};
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_EXECUTORS_SPFLOWEXECUTORCHOICE_H_ */
