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

#include "SPFlowExecutorChoice.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

ChoiceExecutor::ChoiceExecutor(const Options &options) : _options(options) {
	if (_options.costs) {
		_model.setCosts(*_options.costs);
	}
}

void ChoiceExecutor::forgetChoices() {
	_choices.clear();
	_model.reset();
	_counts[0] = _counts[1] = _counts[2] = 0;
}

void ChoiceExecutor::submit(const BlockTicket &ticket) {
	// A block that ran on the machine's front is this executor's own to deliver; a block that went
	// to one of the two is submitted to it as well, because that is where its job is.
	auto kind = getChosen(ticket);
	if (kind == BlockLaunchKind::Gpu && _options.gpu) {
		_options.gpu->submit(ticket);
	} else if (kind == BlockLaunchKind::Threads && _options.threads) {
		_options.threads->submit(ticket);
	} else {
		_ready.emplace_back(ticket);
	}
}

bool ChoiceExecutor::poll(BlockTicket &out) {
	if (!_ready.empty()) {
		out = _ready.front();
		_ready.erase(_ready.begin());
		return true;
	}
	if (_options.gpu && _options.gpu->poll(out)) {
		return true;
	}
	if (_options.threads && _options.threads->poll(out)) {
		return true;
	}
	return false;
}

void ChoiceExecutor::tick() {
	if (_options.gpu) {
		_options.gpu->tick();
	}
	if (_options.threads) {
		_options.threads->tick();
	}
}

BlockLaunch ChoiceExecutor::chooseLaunch(const BlockTicket &ticket,
		const BlockLaunchInfo &info) {
	using Mask = BlockExecutorMask;
	const auto threadsBit = uint32_t(Mask::Threads);
	const auto gpuBit = uint32_t(Mask::Gpu);

	// 1. What the author allows. Nothing named means every one of them.
	const bool open = info.allowed == 0;
	uint32_t wanted = 0;
	if (_options.threads && (open || (info.allowed & threadsBit) != 0)) {
		wanted |= threadsBit;
	}
	if (_options.gpu && (open || (info.allowed & gpuBit) != 0)) {
		wanted |= gpuBit;
	}

	// 2. What is there. Each is asked with a mask that names it, so an author who named nothing is
	// not vetoed by the sub-executor's own `Policy::Author`; what comes back also carries the batch
	// the pool would cut, and kicks the device's pipelines into compiling.
	uint32_t candidates = 0;
	uint32_t batch = 0;
	if ((wanted & gpuBit) != 0) {
		auto asked = info;
		asked.allowed = gpuBit;
		if (_options.gpu->chooseLaunch(ticket, asked).kind == BlockLaunchKind::Gpu) {
			candidates |= gpuBit;
		}
	}
	if ((wanted & threadsBit) != 0) {
		auto asked = info;
		asked.allowed = threadsBit;
		auto answer = _options.threads->chooseLaunch(ticket, asked);
		if (answer.kind == BlockLaunchKind::Threads && answer.batch > 0) {
			candidates |= threadsBit;
			batch = answer.batch;
		}
	}

	// 3. A single name is a demand, not a hint: the model is not asked, and nothing is cached.
	BlockLaunch launch;
	if (!open && (info.allowed == threadsBit || info.allowed == gpuBit)) {
		if ((candidates & info.allowed) != 0) {
			launch.kind = info.allowed == gpuBit ? BlockLaunchKind::Gpu : BlockLaunchKind::Threads;
			launch.batch = launch.kind == BlockLaunchKind::Threads ? batch : 0;
		}
	} else if (candidates != 0) {
		auto decision = _model.choose(candidates, batch, info);
		launch.kind = decision.kind;
		launch.batch = decision.batch;
	}

	auto kind = launch.kind;
	bool found = false;
	for (auto &it : _choices) {
		if (it.node == ticket.node && it.activation == ticket.activation) {
			it.kind = kind;
			found = true;
			break;
		}
	}
	if (!found) {
		_choices.emplace_back(Choice{ticket.node, ticket.activation, kind});
	}
	++_counts[uint32_t(kind)];
	return launch;
}

void ChoiceExecutor::launch(BranchJob &job) {
	if (_options.threads) {
		_options.threads->launch(job);
	}
}

// A phase belongs to the pool whoever chose the block: it is the pool's batches being worked on,
// and the device has no part in one.
void ChoiceExecutor::perform(BranchJob &job) {
	if (_options.threads) {
		_options.threads->perform(job);
	} else {
		BlockExecutor::perform(job);
	}
}

bool ChoiceExecutor::takesPhase(const BranchJob &job) const {
	return _options.threads ? _options.threads->takesPhase(job) : false;
}

void ChoiceExecutor::forget(BranchJob &job) {
	if (_options.threads) {
		_options.threads->forget(job);
	}
}

bool ChoiceExecutor::launchGpu(GpuBlockJob &job) {
	return _options.gpu ? _options.gpu->launchGpu(job) : false;
}

void ChoiceExecutor::forgetGpu(GpuBlockJob &job) {
	if (_options.gpu) {
		_options.gpu->forgetGpu(job);
	}
}

uint64_t ChoiceExecutor::chooseTimeoutMs(const BlockTicket &, const BlockLaunchInfo &info,
		const BlockLaunch &launch) {
	return _model.timeoutMs(launch.kind, info, launch.batch);
}

bool ChoiceExecutor::waitDelivery(TimeInterval interval) {
	if (_options.gpu && _options.gpu->waitDelivery(interval)) {
		return true;
	}
	return _options.threads ? _options.threads->waitDelivery(interval) : false;
}

BlockLaunchKind ChoiceExecutor::getChosen(const BlockTicket &ticket) const {
	for (auto &it : _choices) {
		if (it.node == ticket.node && it.activation == ticket.activation) {
			return it.kind;
		}
	}
	return BlockLaunchKind::Machine;
}

uint32_t ChoiceExecutor::getChoiceCount(BlockLaunchKind kind) const {
	return _counts[uint32_t(kind)];
}

} // namespace stappler::flow
