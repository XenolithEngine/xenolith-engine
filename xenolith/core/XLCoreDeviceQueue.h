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

#ifndef XENOLITH_CORE_XLCOREDEVICEQUEUE_H_
#define XENOLITH_CORE_XLCOREDEVICEQUEUE_H_

#include "XLCoreLoop.h"
#include "XLCoreFrameHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class DeviceQueue;
class CommandPool;
class QueryPool;

struct SP_PUBLIC QueryPoolInfo {
	QueryType type = QueryType::Timestamp;
	uint32_t queryCount = 0;
	QueryPipelineStatisticFlags statFlags = QueryPipelineStatisticFlags::None;

	auto operator<=>(const QueryPoolInfo &) const = default;
};

struct SP_PUBLIC DeviceQueueFamily {
	using FrameHandle = core::FrameHandle;

	struct Waiter {
		Function<void(Loop &, const Rc<DeviceQueue> &)> acquireForLoop;
		Function<void(Loop &)> releaseForLoop;
		Function<void(FrameHandle &, const Rc<DeviceQueue> &)> acquireForFrame;
		Function<void(FrameHandle &)> releaseForFrame;

		Rc<FrameHandle> handle;
		Rc<Loop> loop;
		Rc<Ref> ref;

		Waiter(Function<void(FrameHandle &, const Rc<DeviceQueue> &)> &&a,
				Function<void(FrameHandle &)> &&r, FrameHandle *h, Rc<Ref> &&ref)
		: acquireForFrame(sp::move(a))
		, releaseForFrame(sp::move(r))
		, handle(h)
		, ref(sp::move(ref)) { }

		Waiter(Function<void(Loop &, const Rc<DeviceQueue> &)> &&a, Function<void(Loop &)> &&r,
				Loop *h, Rc<Ref> &&ref)
		: acquireForLoop(sp::move(a)), releaseForLoop(sp::move(r)), loop(h), ref(sp::move(ref)) { }
	};

	uint32_t index;
	uint32_t count;
	QueueFlags preferred = QueueFlags::None;
	QueueFlags flags = QueueFlags::None;
	uint32_t timestampValidBits = 0;
	Extent3 transferGranularity;
	Vector<Rc<DeviceQueue>> queues;
	Vector<Rc<CommandPool>> pools;
	Map<QueryPoolInfo, Vector<Rc<QueryPool>>> queries;
	Vector<Waiter> waiters;
};

class SP_PUBLIC DeviceQueue : public Ref {
public:
	virtual ~DeviceQueue() = default;

	bool init(Device &, uint32_t, QueueFlags);

	Status submit(const FrameSync &, CommandPool &, Fence &, SpanView<const CommandBuffer *>,
			DeviceIdleFlags = DeviceIdleFlags::None);

	Status submit(Fence &, const CommandBuffer *, DeviceIdleFlags = DeviceIdleFlags::None);
	Status submit(Fence &, SpanView<const CommandBuffer *>,
			DeviceIdleFlags = DeviceIdleFlags::None);

	virtual Status waitIdle();

	uint32_t getActiveFencesCount();
	void retainFence(const Fence &);
	void releaseFence(const Fence &);

	uint32_t getIndex() const { return _index; }
	uint64_t getFrameIndex() const { return _frameIdx; }
	QueueFlags getFlags() const { return _flags; }
	Status getLastStatus() const { return _lastStatus; }

	void setOwner(FrameHandle &);
	void reset();

protected:
	virtual Status doSubmit(const FrameSync *, CommandPool *, Fence &,
			SpanView<const CommandBuffer *>, DeviceIdleFlags = DeviceIdleFlags::None) {
		return Status::ErrorNotImplemented;
	}

	Device *_device = nullptr;
	uint32_t _index = 0;
	uint64_t _frameIdx = 0;
	QueueFlags _flags = QueueFlags::None;
	std::atomic<uint32_t> _nfences;
	Status _lastStatus = Status::ErrorUnknown;
};

class SP_PUBLIC CommandPool : public Object {
public:
	virtual ~CommandPool() = default;

	QueueFlags getClass() const { return _class; }
	uint32_t getFamilyIdx() const { return _familyIdx; }

	virtual void reset(Device &dev);

	virtual const CommandBuffer *recordBuffer(Device &dev,
			const Callback<bool(CommandBuffer &)> &) {
		return nullptr;
	}

	void autorelease(Rc<Ref> &&);

protected:
	uint32_t _familyIdx = 0;
	uint32_t _currentComplexity = 0;
	uint32_t _bestComplexity = 0;
	bool _invalidated = false;
	QueueFlags _class = QueueFlags::Graphics;
	Vector<Rc<Ref>> _autorelease;
	Vector<Rc<CommandBuffer>> _buffers;
};

class SP_PUBLIC QueryPool : public Object {
public:
	virtual ~QueryPool() = default;

	const QueryPoolInfo &getInfo() const { return _info; }
	uint32_t getFamilyIdx() const { return _familyIdx; }

	uint32_t getUsedQueries() const { return _usedQueries; }

	uint32_t armNextQuery(uint32_t tag);

	virtual void reset(Device &);

protected:
	QueryPoolInfo _info;
	uint32_t _familyIdx = 0;

	uint32_t _usedQueries = 0;
	Vector<uint32_t> _tags;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREDEVICEQUEUE_H_ */
