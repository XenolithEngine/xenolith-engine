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

#include "SPFlowExecutorThreads.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

ThreadExecutor::ThreadExecutor() : ThreadExecutor(Options()) { }

ThreadExecutor::ThreadExecutor(const Options &options) : _options(options) {
	_threads = options.threads ? options.threads : uint16_t(sprt::thread::hardware_concurrency());
	if (_threads == 0) {
		_threads = 1;
	}
	_queue = Rc<sprt::dispatch::TaskQueue>::create(sprt::dispatch::TaskQueueInfo{
		.flags = sprt::dispatch::ThreadPoolFlags::LazyInit,
		.name = StringView("GraphBranches"),
		.threadCount = _threads,
	});
}

ThreadExecutor::~ThreadExecutor() {
	sprt_passert(_launched.empty(), "ThreadExecutor: a run still holds jobs of this executor");
	if (_queue) {
		_queue->cancel();
		_queue->update();
	}
}

void ThreadExecutor::submit(const BlockTicket &ticket) { _ready.emplace_back(ticket); }

bool ThreadExecutor::poll(BlockTicket &out) {
	if (!_ready.empty()) {
		out = _ready.front();
		_ready.erase(_ready.begin());
		return true;
	}
	if (_queue) {
		_queue->update();
	}
	for (auto job : _launched) {
		if (job->isDone()) {
			out = job->getTicket();
			return true;
		}
	}
	return false;
}

void ThreadExecutor::tick() {
	if (_queue) {
		_queue->update();
	}
}

uint32_t ThreadExecutor::takesBranches(const BlockTicket &, uint32_t branchCount, uint32_t allowed) {
	if (branchCount == 0 || !_queue) {
		return 0;
	}
	if (_options.policy == Policy::Author && (allowed & uint32_t(BlockExecutorMask::Threads)) == 0) {
		return 0;
	}
	if (_options.batch) {
		return _options.batch;
	}
	auto parts = uint32_t(_threads) * 4;
	auto batch = sprt::max(uint32_t(1), (branchCount + parts - 1) / parts);
	// Measured: a batch is superlinear in its own size - one machine holding 25 000 branch
	// activations costs more than twenty-five holding a thousand, because every open and
	// close pays for the store that has grown under it. At 10^5 branches the plain "four per
	// thread" rule was three times the best batch (docs/performance.md, «Who takes a block»). Capped
	// here rather than in the choice, because it is a property of a batch and not of a decision.
	return sprt::min(batch, MaxBatch);
}

void ThreadExecutor::launch(BranchJob &job) {
	_launched.emplace_back(&job);
	_maxInFlight = sprt::max(_maxInFlight, uint32_t(_launched.size()));
	for (uint32_t i = 0; i < job.getBatchCount(); ++i) {
		auto target = &job;
		if (_queue->perform([target, i] { target->runBatch(i); }) != Status::Ok) {
			job.runBatch(i); // a pool that would not take it: the batch runs here
		}
	}
}

/* The same fan-out as launch(), minus the bookkeeping: a phase is not a flight. The machine is
waiting on the job's own counter and will take it back itself, so nothing here may answer poll()
with a ticket for work that is only half over. And that wait is why there is a threshold: a
block's branches are handed out and forgotten, so the pool's wake-up happens while the machine
goes on stepping and costs nothing anybody can see, while a phase is the opposite - the machine
stands still until the last batch reports, so every microsecond of waking sixteen sleeping
threads is on the critical path, twice per block. */
bool ThreadExecutor::takesPhase(const BranchJob &job) const {
	return _queue && job.getBatchCount() > 1 && job.getBranchCount() >= PhaseMinBranches;
}

void ThreadExecutor::perform(BranchJob &job) {
	if (!takesPhase(job)) {
		BlockExecutor::perform(job);
		return;
	}
	++_phaseHandoffs;
	for (uint32_t i = 0; i < job.getBatchCount(); ++i) {
		auto target = &job;
		if (_queue->perform([target, i] { target->runBatch(i); }) != Status::Ok) {
			job.runBatch(i);
		}
	}
}

void ThreadExecutor::forget(BranchJob &job) {
	for (auto it = _launched.begin(); it != _launched.end(); ++it) {
		if (*it == &job) {
			_launched.erase(it);
			break;
		}
	}
}

bool ThreadExecutor::waitDelivery(TimeInterval interval) {
	for (auto job : _launched) {
		if (job->isDone()) {
			return true;
		}
	}
	if (!_queue) {
		return false;
	}
	uint32_t count = 0;
	_queue->wait(interval, &count);
	return count > 0;
}

} // namespace stappler::flow
