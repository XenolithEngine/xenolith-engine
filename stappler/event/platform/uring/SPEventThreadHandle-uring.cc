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

#include <sprt/c/sys/__sprt_eventfd.h>
#include <sprt/c/sys/__sprt_futex.h>

// Available sinve 5.16
static constexpr int sp_sys_futex_wake = 454;
static constexpr int sp_sys_futex_wait = 455;

namespace STAPPLER_VERSIONIZED stappler::event {

// futex implementation based on https://github.com/eliben/code-for-blog/blob/main/2018/futex-basics/mutex-using-futex.cpp

static uint32_t atomicLoadSeq(volatile uint32_t *ptr) {
	uint32_t ret;
	__atomic_load(ptr, &ret, __ATOMIC_SEQ_CST);
	return ret;
}

static uint32_t atomicFetchOr(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
}

static uint32_t atomicFetchAnd(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
}

static uint32_t atomicExchange(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

static constexpr uint32_t LOCK_VALUE = 0b0001;
static constexpr uint32_t WAIT_VALUE = 0b0010;
static constexpr uint32_t SIGNAL_VALUE = 0b0100;
static constexpr uint32_t FULL_VALUE = LOCK_VALUE | SIGNAL_VALUE | WAIT_VALUE;

void FutexImpl::client_lock() {
	// try to mark futex to own it
	uint32_t c = atomicFetchOr(&_futex, LOCK_VALUE | SIGNAL_VALUE);
	if ((c & LOCK_VALUE) != 0) {
		// prev value already has LOCK flag, wait
		do {
			// if wait flag is set already or we still locked
			if ((c & WAIT_VALUE) != 0 || (atomicFetchOr(&_futex, WAIT_VALUE) & LOCK_VALUE) != 0) {
				// futex should have all three flags set at this moment
				// wait for unlock
				__sprt_futex2_wait(&_futex, FULL_VALUE, CLIENT_MASK,
						__SPRT_FUTEX2_SIZE_U32 | __SPRT_FUTEX2_PRIVATE, nullptr, CLOCK_MONOTONIC);
			}
			// check if lock still in place by fetching value and set all flags
		} while (((c = atomicFetchOr(&_futex, FULL_VALUE)) & LOCK_VALUE) != 0);
	}
}

bool FutexImpl::client_try_lock() {
	// returns true if successfully locked
	// set LOCK flag, return true if it was not set (so, we successfully locked)
	return (atomicFetchOr(&_futex, LOCK_VALUE | SIGNAL_VALUE) & LOCK_VALUE) == 0;
}

// client will always call FUTEX_WAKE, because server is always waiting
void FutexImpl::client_unlock() {
	// drop LOCK flag, leave WAIT and SIGNAL in place
	if ((atomicFetchAnd(&_futex, SIGNAL_VALUE | WAIT_VALUE) & SIGNAL_VALUE) == 0) {
		//std::cout << "+";
	}

	// wake server or clients
	__sprt_futex2_wake(&_futex, FULL_MASK, 1, __SPRT_FUTEX2_SIZE_U32 | __SPRT_FUTEX2_PRIVATE);
}

bool FutexImpl::server_try_lock() {
	// returns true if successfully locked
	// set LOCK flag, return true if it was not set (so, we successfully locked)
	return (atomicFetchOr(&_futex, LOCK_VALUE) & LOCK_VALUE) == 0;
}

bool FutexImpl::server_unlock() {
	// unset all, return true if WAIT was set
	if ((atomicExchange(&_futex, 0) & WAIT_VALUE) != 0) {
		__sprt_futex2_wake(&_futex, CLIENT_MASK, 1, __SPRT_FUTEX2_SIZE_U32 | __SPRT_FUTEX2_PRIVATE);
		return true;
	}
	return false;
}

uint32_t FutexImpl::load() { return atomicLoadSeq(&_futex); }

bool ThreadUringSource::init(TimeInterval ival) {
	setNanoTimespec(interval, ival);
	return true;
}

void ThreadUringSource::cancel() { }

bool ThreadUringHandle::init(HandleClass *cl) {
	if (!!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = reinterpret_cast<ThreadUringSource *>(_data);
	return source->init(FAILSAFE_TIMER_INTERVAL);
}

Status ThreadUringHandle::rearm(URingData *uring, ThreadUringSource *source, bool unlock,
		bool init) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (unlock) {
			source->futex.server_unlock();
		}

		if (init) {
			source->thisThread = thread::Thread::getCurrentThreadId();
		}

		if (!source->failsafe) {
			rearmFailsafe(uring, source);
		}

		auto result = uring->pushSqe({IORING_OP_FUTEX_WAIT}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = __SPRT_FUTEX2_SIZE_U32 | __SPRT_FUTEX2_PRIVATE;
			sqe->futex_flags = 0;
			sqe->len = 0;
			sqe->addr = reinterpret_cast<uintptr_t>(source->futex.getAddr());
			sqe->addr2 = 0; // we can lock only on 0
			sqe->addr3 = FutexImpl::SERVER_MASK;
			sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT;
		}, URingPushFlags::Submit);

		if (isSuccessful(result)) {
			_status = Status::Ok;
		}
	} else if (unlock) {
		// force client unlock if server is dead
		source->futex.client_unlock();
	}
	return status;
}

Status ThreadUringHandle::disarm(URingData *uring, ThreadUringSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		if (source->failsafe) {
			uring->pushSqe({IORING_OP_TIMEOUT_REMOVE}, [&](io_uring_sqe *sqe, uint32_t n) {
				sqe->len = 0;
				sqe->addr = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_ALT_BIT;
				sqe->off = 0;
				sqe->user_data = URING_USERDATA_IGNORED;
			}, URingPushFlags::None);
			source->failsafe = false;
		}

		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT,
				URingCancelFlags::Suspend);
	}
	return status;
}

void ThreadUringHandle::notify(URingData *uring, ThreadUringSource *source,
		const NotifyData &data) {
	// !!! futex now should be on 0
	if (_status != Status::Ok) {
		return; // just exit
	}

	if (hasFlag(data.userFlags, uint32_t(URING_USERDATA_ALT_BIT))) {
		if ((data.queueFlags & IORING_CQE_F_MORE) == 0) {
			// rearm failsafe
			rearmFailsafe(uring, source);
		}
		if (source->futex.server_try_lock()) {
			auto ev = performAll([&](uint32_t count) {
				if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
					if (count == 1) {
						log::source().info("event::ThreadUringHandle", "B ",
								sp::platform::nanoclock(ClockType::Monotonic) - _switchTimer);
					}
				}
				source->futex.server_unlock();
			});
			if (ev > 0) {
				//log::source().info("event::ThreadUringHandle", "events was processed with failsafe timer: ", ev);
			}
		}
	} else {
		_status = Status::Suspended;

		if (data.result < 0 && data.result != -EAGAIN) {
			cancel(URingData::getErrnoStatus(data.result));
			return;
		}

		// try to capture futex
		auto success = source->futex.server_try_lock();
		if (!success) {
			// wait for a new wakeup
			rearm(uring, source, false, false);
			return;
		} else {
			// now we own futex
			auto ev = performAll([&](uint32_t count) {
				if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
					if (count == 1) {
						log::source().info("event::ThreadUringHandle", "A ",
								sp::platform::nanoclock(ClockType::Monotonic) - _switchTimer);
					}
				}
				rearm(uring, source, true, false);
			});
			if (ev > 0) {
				//log::source().info("event::ThreadUringHandle", "events was processed with futex: ", ev);
			}
		}
	}
}

Status ThreadUringHandle::perform(Rc<thread::Task> &&task) {
	auto source = reinterpret_cast<ThreadUringSource *>(_data);
	if (thread::Thread::getCurrentThreadId() == source->thisThread) {
		// Ensure that server will be notified by setting SIGNAL flag.
		// Other thread will issue FITEX_WAKE, if we fail to lock, so
		// just add task into non-protected queue
		if (source->futex.client_try_lock()) {
			_outputQueue.emplace_back(move(task));
			source->futex.client_unlock();
		} else {
			_unsafeQueue.emplace_back(move(task));
		}
	} else {
		source->futex.client_lock();
		_outputQueue.emplace_back(move(task));
		if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
			_switchTimer = sp::platform::nanoclock(ClockType::Monotonic);
		}
		source->futex.client_unlock();
	}
	return Status::Ok;
}

Status ThreadUringHandle::perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) {
	auto source = reinterpret_cast<ThreadUringSource *>(_data);
	if (thread::Thread::getCurrentThreadId() == source->thisThread) {
		// Ensure that server will be notified by setting SIGNAL flag.
		// Other thread will issue FITEX_WAKE, if we fail to lock, so
		// just add task into non-protected queue
		if (source->futex.client_try_lock()) {
			_outputCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});
			source->futex.client_unlock();
		} else {
			_unsafeCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});
		}
	} else {
		source->futex.client_lock();
		_outputCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});
		if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
			_switchTimer = sp::platform::nanoclock(ClockType::Monotonic);
		}
		source->futex.client_unlock();
	}
	return Status::Ok;
}

void ThreadUringHandle::rearmFailsafe(URingData *uring, ThreadUringSource *source) {
	uring->pushSqe({IORING_OP_TIMEOUT}, [&](io_uring_sqe *sqe, uint32_t n) {
		sqe->fd = -1;
		sqe->len = 1;
		sqe->addr = reinterpret_cast<uintptr_t>(&source->interval);
		sqe->off = 0;
		sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_ALT_BIT;
		sqe->timeout_flags = IORING_TIMEOUT_MULTISHOT | IORING_TIMEOUT_ETIME_SUCCESS;
	}, URingPushFlags::Submit);
	source->failsafe = true;
}

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
					log::source().info("event::ThreadUringHandle", "C ",
							sp::platform::nanoclock(ClockType::Monotonic) - _switchTimer);
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

Status ThreadEventFdHandle::perform(Rc<thread::Task> &&task) {
	std::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
		_switchTimer = sp::platform::nanoclock(ClockType::Monotonic);
	}

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

Status ThreadEventFdHandle::perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) {
	std::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});

	if constexpr (URING_THREAD_DEBUG_SWITCH_TIMER) {
		_switchTimer = sp::platform::nanoclock(ClockType::Monotonic);
	}

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

} // namespace stappler::event
