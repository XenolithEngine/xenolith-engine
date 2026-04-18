/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPEventThreadHandle-uring.h"

#include <sprt/runtime/log.h>
#include <sprt/c/sys/__sprt_eventfd.h>
#include <sprt/c/sys/__sprt_futex.h>

namespace sprt::dispatch {

bool ThreadEventFdHandle::init(HandleClass *cl) {
	if (!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = reinterpret_cast<EventFdSource *>(_data);
	return source->init();
}

Status ThreadEventFdHandle::rearm(URingData *uring, EventFdSource *source, bool updateBuffers) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (hasFlag(uring->_uflags, URingFlags::ReadMultishotSupported)) {
			auto fillMultishot = [&](io_uring_sqe *sqe) {
				sqe->fd = source->fd;
				sqe->buf_group = _bufferGroup;
				sqe->off = -1;
				sqe->user_data = reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK);
				sqe->flags |= IOSQE_BUFFER_SELECT;
			};

			if (!_bufferGroup) {
				_bufferGroup = uring->registerBufferGroup(EventFdSource::TARGET_BUFFER_COUNT,
						sizeof(uint64_t), (uint8_t *)source->target);
			} else if (updateBuffers) {
				_bufferGroup =
						uring->reloadBufferGroup(_bufferGroup, EventFdSource::TARGET_BUFFER_COUNT,
								sizeof(uint64_t), (uint8_t *)source->target);
			}

			status = uring->pushSqe({IORING_OP_READ_MULTISHOT}, [&](io_uring_sqe *sqe, uint32_t) {
				fillMultishot(sqe);
			}, URingPushFlags::Submit);
		} else {
			status = uring->pushSqe({IORING_OP_READ}, [&](io_uring_sqe *sqe, uint32_t) {
				sqe->fd = source->fd;
				sqe->addr = reinterpret_cast<uintptr_t>(source->target);
				sqe->len = sizeof(uint64_t);
				sqe->off = -1;
				sqe->user_data = reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK);
			}, URingPushFlags::Submit);
		}
	}
	return status;
}

Status ThreadEventFdHandle::disarm(URingData *uring, EventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
		if (_bufferGroup) {
			uring->unregisterBufferGroup(_bufferGroup, EventFdSource::TARGET_BUFFER_COUNT);
			_bufferGroup = 0;
		}
	}
	return status;
}

void ThreadEventFdHandle::notify(URingData *uring, EventFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	//// we don't need this result here
	// uint64_t target = &_targetBuf[0];
	// if (flags & IORING_CQE_F_BUFFER) {
	// 	target = &_targetBuf[(flags >> 16) & 0xFFFF];
	// }

	bool more = (data.queueFlags & IORING_CQE_F_MORE);
	if (!more) {
		// if more - we still running
		_status = Status::Suspended;
	}

	auto performUnlock = [&] {
		performAll([&](uint32_t count) {
			if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
				if (count == 1) {
					oslog::vpinfo(__SPRT_LOCATION, "dispatch::ThreadUringHandle", "C ",
							__sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC) - _switchTimer);
				}
			}
			_mutex.unlock();
			if (!more) {
				rearm(uring, source);
			}
		});

		if (more) {
			// no need to rearm, handle is still armed
			_status = Status::Ok;
		}
	};

	if (data.result == sizeof(uint64_t)) {
		if constexpr (URING_THREAD_NONBLOCK) {
			if (_mutex.try_lock()) {
				performUnlock();
			} else {
				if (!more) {
					rearm(uring, source);
				}
				uint64_t value = 1;
				::__sprt_eventfd_write(source->fd, value);
			}
		} else {
			_mutex.lock();
			performUnlock();
		}
	} else if (data.result == -ENOBUFS) {
		// Rearm buffers only when multishot is failed
		rearm(uring, source, true);
	} else {
		cancel(URingData::getErrnoStatus(data.result));
	}
}

Status ThreadEventFdHandle::perform(Rc<Task> &&task) {
	sprt::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
		_switchTimer = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC);
	}

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

Status ThreadEventFdHandle::perform(dispatch::Function<void()> &&func, Ref *target,
		StringView tag) {
	sprt::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});

	if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
		_switchTimer = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC);
	}

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

} // namespace sprt::dispatch
