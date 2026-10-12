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

#include "SPFlowExecutorGpu.h"

#include "SPPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

bool GpuRequest::complete(State state, BytesView data) {
	sprt::unique_lock<sprt::mutex> lock(_mutex);
	if (_state.load() != State::Pending) {
		return false; // abandoned, or answered twice
	}
	if (state == State::Done && !data.empty()) {
		_data = mem_std::Vector<uint8_t>(data.data(), data.data() + data.size());
	}
	_state.store(state);
	return true;
}

void GpuRequest::abandon() {
	sprt::unique_lock<sprt::mutex> lock(_mutex);
	_state.store(State::Abandoned);
	_data.clear();
}

BytesView GpuRequest::getData() const {
	sprt::unique_lock<sprt::mutex> lock(_mutex);
	return BytesView(_data.data(), _data.size());
}

bool GpuBufferSlot::reserve(uint64_t bytes) {
	if (bytes <= capacity && capacity > 0) {
		return false;
	}
	uint64_t next = capacity > 0 ? capacity : 4096;
	while (next < bytes) {
		next *= 2;
	}
	capacity = next;
	++allocations;
	return true;
}

void GpuBufferSlot::release() { capacity = 0; }

GpuExecutor::GpuExecutor(GpuDevice *device) : GpuExecutor(device, Options()) { }

GpuExecutor::GpuExecutor(GpuDevice *device, const Options &options)
: _device(device), _options(options) { }

GpuExecutor::~GpuExecutor() {
	sprt_passert(_flights.empty(), "GpuExecutor: a run still holds blocks of this executor");
}

uint64_t GpuExecutor::nowMs() const {
	if (_options.wallClock) {
		return _options.wallClock->nowMs();
	}
	return platform::clock(ClockType::Monotonic) / 1000;
}

void GpuExecutor::submit(const BlockTicket &ticket) { _ready.emplace_back(ticket); }

BlockLaunch GpuExecutor::chooseLaunch(const BlockTicket &, const BlockLaunchInfo &info) {
	if (!_device || !info.gpu || !info.gpuTable) {
		return BlockLaunch();
	}
	if (_options.policy == Policy::Author && (info.allowed & uint32_t(BlockExecutorMask::Gpu)) == 0) {
		return BlockLaunch();
	}
	if (!_device->isAvailable()) {
		return BlockLaunch();
	}
	// The pipelines are compiled once per table; the blocks that open before that run on the CPU,
	// which is the same answer taken a different way.
	if (!_device->isPrepared(info.gpuTable->getIdentity())) {
		prepare(*info.gpuTable);
		return BlockLaunch();
	}
	return BlockLaunch{BlockLaunchKind::Gpu, 0};
}

void GpuExecutor::prepare(const GpuShaderTable &table) {
	if (!_device || _device->isPrepared(table.getIdentity())) {
		return;
	}
	mem_std::Vector<GpuPipelineSpec> specs;
	for (auto &block : table.getBlocks()) {
		specs.emplace_back(GpuPipelineSpec{GpuPipelineSpec::Kind::Block, StringView(), block.textHash,
			SpanView<uint32_t>(block.spirv.data(), block.spirv.size())});
		if (!_options.reducers) {
			continue;
		}
		for (auto &r : block.reducers) {
			bool known = false;
			for (auto &it : specs) {
				known = known || it.hash == r.textHash;
			}
			if (!known) {
				specs.emplace_back(GpuPipelineSpec{GpuPipelineSpec::Kind::Reduce, StringView(r.name),
					r.textHash, SpanView<uint32_t>(r.spirv.data(), r.spirv.size())});
			}
		}
	}
	_device->prepare(table.getIdentity(), specs);
}

bool GpuExecutor::isPrepared(const GpuShaderTable &table) const {
	return _device && _device->isPrepared(table.getIdentity());
}

bool GpuExecutor::launchGpu(GpuBlockJob &job) {
	if (!_device || !_device->isAvailable()) {
		return false;
	}
	auto &shaders = job.getShaders();
	GpuDispatchCall call;
	call.table = job.getTableIdentity();
	call.slot = shaders.block;
	call.pipeline = shaders.textHash;
	call.block = job.getBlockBytes();
	call.ins = job.getInBytes();
	call.count = job.getBranchCount();
	call.inStride = job.getInStride();
	call.outStride = job.getOutStride();
	auto request = _device->dispatch(call);
	if (!request) {
		return false;
	}
	auto start = nowMs();
	request->setStartMs(start);
	_flights.emplace_back(Flight{&job, request, Stage::Dispatch, start});
	++_dispatches;
	return true;
}

void GpuExecutor::forgetGpu(GpuBlockJob &job) {
	for (auto it = _flights.begin(); it != _flights.end(); ++it) {
		if (it->job == &job) {
			if (it->request) {
				it->request->abandon();
			}
			_flights.erase(it);
			return;
		}
	}
}

// The reduce stage: every collector of the block in one call, so a block is two round trips at
// most.
void GpuExecutor::sendReduce(Flight &flight) {
	auto &shaders = flight.job->getShaders();
	GpuReduceCall call;
	call.table = flight.job->getTableIdentity();
	call.slot = shaders.block;
	call.inputs = flight.job->getReduceInputs();
	auto request = _device->reduce(call);
	if (!request) {
		flight.job->complete(); // the collectors fold on the CPU
		return;
	}
	request->setStartMs(nowMs());
	flight.request = request;
	flight.stage = Stage::Reduce;
	flight.start = nowMs();
	++_reduces;
}

// One flight, on the machine's thread. True when the job finished and its ticket can be handed
// back.
bool GpuExecutor::advance(Flight &flight) {
	if (flight.job->isDone()) {
		return true;
	}
	auto request = flight.request;
	if (!request) {
		flight.job->abort(GpuAbort::Invalid);
		return true;
	}
	switch (request->getState()) {
	case GpuRequest::State::Done:
		if (flight.stage == Stage::Dispatch) {
			if (flight.job->acceptOuts(request->getData()) != Status::Ok) {
				flight.job->abort(GpuAbort::Invalid);
				return true;
			}
			if (_options.reducers && !flight.job->getReduceInputs().empty()) {
				sendReduce(flight);
				return flight.job->isDone();
			}
			flight.job->complete();
			return true;
		}
		// The folds come back in the order they were sent: a value and a presence flag each.
		{
			auto data = request->getData();
			auto inputs = flight.job->getReduceInputs();
			size_t offset = 0;
			for (auto &in : inputs) {
				auto stride = in.stride;
				if (offset + stride + 4 > data.size()) {
					break;
				}
				uint32_t present = 0;
				sprt::memcpy(&present, data.data() + offset + stride, 4);
				if (present != 0) {
					flight.job->acceptReduced(in.reducer,
							readGpuValue(data.data() + offset, in.type));
				}
				offset += stride + 4;
			}
		}
		flight.job->complete();
		return true;
	case GpuRequest::State::Failed:
	case GpuRequest::State::Lost:
		flight.job->abort(GpuAbort::Lost);
		return true;
	case GpuRequest::State::Abandoned: flight.job->abort(GpuAbort::Cancelled); return true;
	case GpuRequest::State::Pending: break;
	}

	// Still out. A device that was lost, or a loop that was stopped, answers nothing at all: the
	// flight is broken off here rather than waited for.
	if (_device->isLost()) {
		request->abandon();
		flight.job->abort(GpuAbort::Lost);
		return true;
	}
	if (!_device->isAvailable()) {
		request->abandon();
		flight.job->abort(GpuAbort::Silent);
		return true;
	}
	if (_options.silenceMs > 0 && nowMs() - flight.start > _options.silenceMs) {
		request->abandon();
		flight.job->abort(GpuAbort::Silent);
		return true;
	}
	return false;
}

bool GpuExecutor::poll(BlockTicket &out) {
	if (!_ready.empty()) {
		out = _ready.front();
		_ready.erase(_ready.begin());
		return true;
	}
	if (!_device) {
		return false;
	}
	_device->update();
	for (auto &flight : _flights) {
		if (advance(flight)) {
			out = flight.job->getTicket();
			return true; // the machine takes the job at delivery, and forgetGpu drops the flight
		}
	}
	return false;
}

void GpuExecutor::tick() {
	if (_device) {
		_device->update();
	}
}

bool GpuExecutor::waitDelivery(TimeInterval interval) {
	if (!_device) {
		return false;
	}
	// A delivery already made is not waited for, and with nothing on the device there is nothing to
	// wait for: a caller over several executors (ChoiceExecutor) asks this one first, and spending
	// the whole interval here while the threads have the block costs every threaded block of a
	// frame that interval.
	if (!_ready.empty()) {
		return true;
	}
	if (_flights.empty()) {
		return false;
	}
	return _device->waitCompletion(interval);
}

} // namespace stappler::flow
