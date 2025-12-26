/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLCoreDeviceQueue.h"
#include "XLCoreEnum.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool DeviceQueue::init(Device &device, uint32_t index, QueueFlags flags) {
	_device = &device;
	_index = index;
	_flags = flags;
	return true;
}

Status DeviceQueue::submit(const FrameSync &sync, CommandPool &pool, Fence &fence,
		SpanView<const CommandBuffer *> buffers, DeviceIdleFlags idleFlags) {
	for (auto &it : buffers) {
		for (auto &q : it->getQueryPools()) {
			if (q->getUsedQueries()) {
				fence.bindQueries(q.get());
			}
		}
	}
	return doSubmit(&sync, &pool, fence, buffers, idleFlags);
}

Status DeviceQueue::submit(Fence &fence, const CommandBuffer *buffer, DeviceIdleFlags idleFlags) {
	for (auto &q : buffer->getQueryPools()) {
		if (q->getUsedQueries()) {
			fence.bindQueries(q.get());
		}
	}
	return doSubmit(nullptr, nullptr, fence, makeSpanView(&buffer, 1), idleFlags);
}

Status DeviceQueue::submit(Fence &fence, SpanView<const CommandBuffer *> buffers,
		DeviceIdleFlags idleFlags) {
	for (auto &it : buffers) {
		for (auto &q : it->getQueryPools()) {
			if (q->getUsedQueries()) {
				fence.bindQueries(q.get());
			}
		}
	}
	return doSubmit(nullptr, nullptr, fence, buffers, idleFlags);
}

Status DeviceQueue::waitIdle() { return Status::ErrorNotImplemented; }

uint32_t DeviceQueue::getActiveFencesCount() { return _nfences.load(); }

void DeviceQueue::retainFence(const Fence &fence) { ++_nfences; }

void DeviceQueue::releaseFence(const Fence &fence) { --_nfences; }

void DeviceQueue::setOwner(FrameHandle &frame) { _frameIdx = frame.getOrder(); }

void DeviceQueue::reset() {
	_lastStatus = Status::ErrorUnknown;
	_frameIdx = 0;
}

void CommandPool::reset(Device &dev) {
	_buffers.clear();
	_autorelease.clear();
}

void CommandPool::autorelease(Rc<Ref> &&ref) { _autorelease.emplace_back(move(ref)); }

uint32_t QueryPool::armNextQuery(uint32_t tag) {
	if (_info.type != QueryType::Timestamp) {
		return maxOf<uint32_t>();
	}

	uint32_t ret = maxOf<uint32_t>();
	if (_usedQueries < _info.queryCount) {
		_tags.emplace_back(tag);
		ret = _usedQueries;
		++_usedQueries;
	} else {
		log::source().error("QueryPool", "Exceeded timestamps limit, ", _usedQueries,
				" timestamps already in use");
	}
	return ret;
}

void QueryPool::reset(Device &) {
	_usedQueries = 0;
	_tags.clear();
}

} // namespace stappler::xenolith::core
