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

#include "SPEventEventFd.h"
#include "../uring/SPEvent-uring.h"
#include "../epoll/SPEvent-epoll.h"
#include "../android/SPEvent-alooper.h"

#include <sprt/c/sys/__sprt_eventfd.h>

namespace STAPPLER_VERSIONIZED stappler::event {

bool EventFdSource::init() {
	fd = ::__sprt_eventfd(0, __SPRT_EFD_CLOEXEC | __SPRT_EFD_NONBLOCK);
	if (fd < 0) {
		return false;
	}

	return true;
}

void EventFdSource::cancel() {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

bool EventFdHandle::init(HandleClass *cl, CompletionHandle<void> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	auto source = reinterpret_cast<EventFdSource *>(_data);
	return source->init();
}

Status EventFdHandle::read(uint64_t *target) {
	auto source = reinterpret_cast<EventFdSource *>(_data);
	auto ret = ::__sprt_eventfd_read(source->fd, target ? target : source->target);
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status EventFdHandle::write(uint64_t val, uint32_t value) {
	auto source = reinterpret_cast<EventFdSource *>(_data);
	if (value) {
		__atomic_or_fetch(&source->eventValue, value, __ATOMIC_SEQ_CST);
	}
	auto ret = ::__sprt_eventfd_write(source->fd, val);
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status EventFdURingHandle::rearm(URingData *uring, EventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->target[0] = 0;

		status = uring->pushRead(source->fd, (uint8_t *)source->target, sizeof(uint64_t),
				reinterpret_cast<uintptr_t>(this) | (_timeline & URING_USERDATA_SERIAL_MASK));
	}
	return status;
}

Status EventFdURingHandle::disarm(URingData *uring, EventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void EventFdURingHandle::notify(URingData *uring, EventFdSource *source, const NotifyData &notify) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	if (notify.result == sizeof(uint64_t)) {
		rearm(uring, source);
		sendCompletion(__atomic_exchange_n(&source->eventValue, 0, __ATOMIC_SEQ_CST), Status::Ok);
	} else {
		cancel(URingData::getErrnoStatus(notify.result));
	}
}

Status EventFdEPollHandle::rearm(EPollData *epoll, EventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = __SPRT_EPOLLIN;
		source->eventTarget = 0;
		source->eventValue = 0;

		status = epoll->add(source->fd, source->event);
	}
	return status;
}

Status EventFdEPollHandle::disarm(EPollData *epoll, EventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void EventFdEPollHandle::notify(EPollData *epoll, EventFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	bool notify = false;

	if (data.queueFlags & __SPRT_EPOLLIN) {
		while (read(&source->eventTarget) == Status::Ok) { notify = true; }
	}

	if ((data.queueFlags & __SPRT_EPOLLERR) || (data.queueFlags & __SPRT_EPOLLHUP)) {
		cancel();
	} else if (notify) {
		sendCompletion(__atomic_exchange_n(&source->eventValue, 0, __ATOMIC_SEQ_CST), Status::Ok);
	}
}

Status EventFdALooperHandle::rearm(ALooperData *alooper, EventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = alooper->add(source->fd, __SPRT_ALOOPER_EVENT_INPUT, this);
	}
	return status;
}

Status EventFdALooperHandle::disarm(ALooperData *alooper, EventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void EventFdALooperHandle::notify(ALooperData *alooper, EventFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	bool notify = false;

	if (data.queueFlags & __SPRT_ALOOPER_EVENT_INPUT) {
		while (read(&source->eventTarget) == Status::Ok) { notify = true; }
	}

	if ((data.queueFlags & __SPRT_ALOOPER_EVENT_ERROR)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_INVALID)) {
		cancel();
	} else if (notify) {
		sendCompletion(__atomic_exchange_n(&source->eventValue, 0, __ATOMIC_SEQ_CST), Status::Ok);
	}
}

} // namespace stappler::event
