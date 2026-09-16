/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "SPEventAddressWait.h"
#include "../uring/SPEvent-uring.h"
#include "../epoll/SPEvent-epoll.h"

#include <sprt/c/sys/__sprt_eventfd.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/sys/__sprt_sprt.h>

namespace sprt::dispatch {

bool AddressWaitFdHandle::init(HandleClass *cl, AddressWaitInfo &&info) {
	if (!info.address || !Handle::init(cl, move(info.completion))) {
		return false;
	}
	_address = info.address;
	_last = info.expected;
	return true;
}

void AddressWaitFdHandle::deliverIfChanged() {
	auto value = __atomic_load_n(_address, __ATOMIC_SEQ_CST);
	if (value != _last) {
		_last = value;
		sendCompletion(value, Status::Ok);
	}
}

Status AddressWaitFutexURingHandle::rearm(URingData *uring, AddressWaitFutexSource *) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = uring->pushSqe({IORING_OP_FUTEX_WAIT}, [&](io_uring_sqe *sqe, uint32_t) {
			// futex2 flags travel in `fd`. Never FUTEX2_PRIVATE: a queued private wait is lost when
			// the process-private futex hash is resized as threads are created (Linux 6.16+).
			sqe->fd = int(__SPRT_FUTEX2_SIZE_U32);
			sqe->addr = reinterpret_cast<uintptr_t>(_address);
			sqe->addr2 = _last;
			sqe->addr3 = __SPRT_FUTEX_BITSET_MATCH_ANY;
			sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
					| (_timeline & URING_USERDATA_SERIAL_MASK);
		}, URingPushFlags::Submit);
	}
	return status;
}

Status AddressWaitFutexURingHandle::disarm(URingData *uring, AddressWaitFutexSource *) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void AddressWaitFutexURingHandle::notify(URingData *uring, AddressWaitFutexSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	// 0: woken; -EAGAIN: the word already differed from the expected value
	if (data.result < 0 && data.result != -EAGAIN) {
		cancel(URingData::getErrnoStatus(data.result));
		return;
	}

	auto value = __atomic_load_n(_address, __ATOMIC_SEQ_CST);
	auto changed = (value != _last);
	if (changed) {
		_last = value;
	}

	rearm(uring, source);

	if (changed) {
		sendCompletion(value, Status::Ok);
	}
}

AddressWaiter::~AddressWaiter() {
	stop();
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

bool AddressWaiter::init() {
	_fd = ::__sprt_eventfd(0, __SPRT_EFD_CLOEXEC | __SPRT_EFD_NONBLOCK);
	return _fd >= 0;
}

bool AddressWaiter::start(uint32_t *address, uint32_t seen) {
	if (_running) {
		return true;
	}
	_address = address;
	_seen = seen;
	__atomic_store_n(&_stopRequested, uint32_t(0), __ATOMIC_SEQ_CST);
	__atomic_store_n(&_exited, uint32_t(0), __ATOMIC_SEQ_CST);
	_thread = sprt::thread([this] { run(); });
	_running = _thread.joinable();
	return _running;
}

void AddressWaiter::stop() {
	if (!_running) {
		return;
	}
	__atomic_store_n(&_stopRequested, uint32_t(1), __ATOMIC_SEQ_CST);
	// The thread may be between reading the word and sleeping on it, so one wake is not enough.
	while (__atomic_load_n(&_exited, __ATOMIC_SEQ_CST) == 0) {
		__sprt_sprt_qlock_wake_all(_address, __SPRT_SPRT_LOCK_FLAG_SHARED);
		sprt::this_thread::sleep_for(1'000'000);
	}
	_thread.join();
	_running = false;
}

void AddressWaiter::run() {
	while (__atomic_load_n(&_stopRequested, __ATOMIC_SEQ_CST) == 0) {
		auto value = __atomic_load_n(_address, __ATOMIC_SEQ_CST);
		if (value != _seen) {
			_seen = value;
			::__sprt_eventfd_write(_fd, 1);
			continue;
		}
		__sprt_sprt_qlock_wait(_address, value, __SPRT_SPRT_TIMEOUT_INFINITE,
				__SPRT_SPRT_LOCK_FLAG_SHARED);
	}
	__atomic_store_n(&_exited, uint32_t(1), __ATOMIC_SEQ_CST);
}

AddressWaitThreadHandle::~AddressWaitThreadHandle() { stopWaiter(); }

bool AddressWaitThreadHandle::init(HandleClass *cl, AddressWaitInfo &&info) {
	if (!AddressWaitFdHandle::init(cl, move(info))) {
		return false;
	}
	_waiter = Rc<AddressWaiter>::create();
	if (!_waiter) {
		return false;
	}
	reinterpret_cast<AddressWaitEventFdSource *>(_data)->fd = _waiter->getFd();
	return true;
}

bool AddressWaitThreadHandle::startWaiter(AddressWaitEventFdSource *) {
	return _waiter && _waiter->start(_address, _last);
}

void AddressWaitThreadHandle::stopWaiter() {
	if (_waiter) {
		_waiter->stop();
	}
}

Status AddressWaitEPollHandle::rearm(EPollData *epoll, AddressWaitEventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = __SPRT_EPOLLIN;
		source->target = 0;

		status = epoll->add(source->fd, source->event);
		if (status == Status::Ok && !startWaiter(source)) {
			epoll->remove(source->fd);
			status = Status::ErrorUnknown;
		}
	}
	return status;
}

Status AddressWaitEPollHandle::disarm(EPollData *epoll, AddressWaitEventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		stopWaiter();
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void AddressWaitEPollHandle::notify(EPollData *epoll, AddressWaitEventFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if ((data.queueFlags & __SPRT_EPOLLERR) || (data.queueFlags & __SPRT_EPOLLHUP)) {
		cancel();
		return;
	}

	if (data.queueFlags & __SPRT_EPOLLIN) {
		while (::__sprt_eventfd_read(source->fd, &source->target) >= 0) { }
		deliverIfChanged();
	}
}

Status AddressWaitEventFdURingHandle::rearm(URingData *uring, AddressWaitEventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->target = 0;

		status = uring->pushSqe({IORING_OP_READ}, [&](io_uring_sqe *sqe, uint32_t) {
			sqe->fd = source->fd;
			sqe->addr = reinterpret_cast<uintptr_t>(&source->target);
			sqe->len = sizeof(uint64_t);
			sqe->off = -1;
			sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
					| (_timeline & URING_USERDATA_SERIAL_MASK);
		}, URingPushFlags::Submit);

		if (status == Status::Ok && !startWaiter(source)) {
			status = Status::ErrorUnknown;
		}
	}
	return status;
}

Status AddressWaitEventFdURingHandle::disarm(URingData *uring, AddressWaitEventFdSource *) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		stopWaiter();
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void AddressWaitEventFdURingHandle::notify(URingData *uring, AddressWaitEventFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	if (data.result != sizeof(uint64_t)) {
		cancel(URingData::getErrnoStatus(data.result));
		return;
	}

	rearm(uring, source);
	deliverIfChanged();
}

} // namespace sprt::dispatch
