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

#include "SPEventTimerFd.h"

#include "../uring/SPEvent-uring.h"

#include <sys/timerfd.h>

namespace STAPPLER_VERSIONIZED stappler::event {

bool TimerFdSource::init(const TimerInfo &info) {
	int clockid = CLOCK_MONOTONIC;

	switch (info.type) {
	case ClockType::Default:
	case ClockType::Monotonic: clockid = CLOCK_MONOTONIC; break;
	case ClockType::Realtime: clockid = CLOCK_REALTIME; break;
	case ClockType::Process:
		log::source().error("event::Queue",
				"ClockType::Process is not supported for a timer on this system");
		return false;
		break;
	case ClockType::Thread:
		log::source().error("event::Queue",
				"ClockType::Thread is not supported for a timer on this system");
		return false;
		break;
	case ClockType::Hardware:
		log::source().error("event::Queue",
				"ClockType::Hardware is not supported for a timer on this system");
		return false;
		break;
	}

	if (fd < 0) {
		fd = ::timerfd_create(clockid, TFD_NONBLOCK | TFD_CLOEXEC);
	}

	if (fd < 0) {
		log::source().error("event::Queue", "fail to timerfd_create");
		return false;
	}

	itimerspec spec;
	if (info.timeout) {
		setNanoTimespec(spec.it_value, info.timeout);
	} else {
		setNanoTimespec(spec.it_value, info.interval);
	}

	if (info.count > 1 && !info.interval) {
		setNanoTimespec(spec.it_interval, info.timeout);
	} else {
		setNanoTimespec(spec.it_interval, info.interval);
	}

	::timerfd_settime(fd, 0, &spec, nullptr);

	value = 0;
	count = info.count;

	return true;
}

void TimerFdSource::cancel() {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

bool TimerFdHandle::init(HandleClass *cl, TimerInfo &&info) {
	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	}

	auto source = new (_data) TimerFdSource;
	return source->init(info);
}

bool TimerFdHandle::reset(TimerInfo &&info) {
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto source = reinterpret_cast<TimerFdSource *>(_data);
	return source->init(info) && Handle::reset();
}

Status TimerFdHandle::read(uint64_t *target) {
	auto source = reinterpret_cast<TimerFdSource *>(_data);
	auto ret = ::read(source->fd, target, sizeof(uint64_t));
	if (ret == sizeof(uint64_t)) {
		return Status::Ok;
	} else if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Declined;
}

#ifdef SP_EVENT_URING
Status TimerFdURingHandle::rearm(URingData *uring, TimerFdSource *source) {
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
	}
	return status;
}

Status TimerFdURingHandle::disarm(URingData *uring, TimerFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	}
	return status;
}

void TimerFdURingHandle::notify(URingData *uring, TimerFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	if (data.result < 0) {
		cancel(URingData::getErrnoStatus(data.result));
	}

	auto count = source->count;
	auto current = source->value;

	if (data.result == sizeof(uint64_t)) {
		// successful read
		current += static_cast<uint32_t>(source->target);
		if (count != Infinite) {
			source->value = std::min(current, count);
		}
	}

	if (current >= count && _status == Status::Suspended) {
		cancel(Status::Done);
	}

	if (_status == Status::Suspended && (count == maxOf<uint32_t>() || current < count)) {
		rearm(uring, source);
	}

	sendCompletion(source->value, _status == Status::Suspended ? Status::Ok : _status);
}
#endif

Status TimerFdEPollHandle::rearm(EPollData *epoll, TimerFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = EPOLLIN;

		status = epoll->add(source->fd, source->event);
	}
	return status;
}

Status TimerFdEPollHandle::disarm(EPollData *epoll, TimerFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void TimerFdEPollHandle::notify(EPollData *epoll, TimerFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.queueFlags & EPOLLIN) {
		auto count = source->count;
		auto current = source->value;

		uint64_t value = 0;
		while (read(&value) == Status::Ok) {
			current += static_cast<uint32_t>(value);
			if (count != Infinite) {
				source->value = std::min(current, count);
			}

			if (current >= count) {
				cancel(Status::Done);
				break;
			}
		}

		if (value > 0) {
			sendCompletion(source->value, _status);
		}
	}

	if ((data.queueFlags & EPOLLERR) || (data.queueFlags & EPOLLHUP)) {
		cancel();
	}
}

#if ANDROID
Status TimerFdALooperHandle::rearm(ALooperData *alooper, TimerFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = alooper->add(source->fd, ALOOPER_EVENT_INPUT, this);
	}
	return status;
}

Status TimerFdALooperHandle::disarm(ALooperData *alooper, TimerFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void TimerFdALooperHandle::notify(ALooperData *alooper, TimerFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.queueFlags & ALOOPER_EVENT_INPUT) {
		auto count = source->count;
		auto current = source->value;

		uint64_t value = 0;
		while (read(&value) == Status::Ok) {
			current += static_cast<uint32_t>(value);
			if (count != Infinite) {
				source->value = std::min(current, count);
			}

			if (current >= count) {
				cancel(Status::Done);
				break;
			}
		}

		if (value > 0) {
			sendCompletion(source->value, _status);
		}
	}

	if ((data.queueFlags & ALOOPER_EVENT_ERROR) || (data.queueFlags & ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & ALOOPER_EVENT_INVALID)) {
		cancel();
	}
}
#endif

} // namespace stappler::event
