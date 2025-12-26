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

#include "SPEventPollFd.h"
#include "../linux/SPEvent-linux.h"
#include "../android/SPEvent-android.h"
#include "SPEventQueue.h"

namespace STAPPLER_VERSIONIZED stappler::event {

bool PollFdSource::init(int f, PollFlags fl) {
	fd = f;
	flags = fl;
	return true;
}

void PollFdSource::cancel() {
	if (hasFlag(flags, PollFlags::CloseFd) && fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

bool PollFdHandle::init(HandleClass *cl, int fd, PollFlags flags,
		CompletionHandle<PollFdHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	auto source = new (_data) PollFdSource;
	return source->init(fd, flags);
}

bool PollFdHandle::reset(PollFlags flags) {
	reinterpret_cast<PollFdSource *>(_data)->flags = flags;
	return Handle::reset();
}

NativeHandle PollFdHandle::getNativeHandle() const {
	return reinterpret_cast<const PollFdSource *>(_data)->fd;
}

#ifdef SP_EVENT_URING
Status PollFdURingHandle::rearm(URingData *uring, PollFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		uring->pushSqe({IORING_OP_POLL_ADD}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = source->fd;
			if (hasFlag(source->flags, PollFlags::AllowMulti)) {
				sqe->len = IORING_POLL_ADD_MULTI;
			}
			sqe->poll_events = toInt(source->flags & PollFlags::PollMask);
			sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
					| (_timeline & URING_USERDATA_SERIAL_MASK);
		}, URingPushFlags::Submit);
	}
	return status;
}

Status PollFdURingHandle::disarm(URingData *uring, PollFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		uring->pushSqe({IORING_OP_POLL_REMOVE}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = source->fd;
			sqe->user_data = URING_USERDATA_IGNORED;
		}, URingPushFlags::Submit);
		++_timeline;
	}
	return status;
}

void PollFdURingHandle::notify(URingData *uring, PollFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return; // just exit
	}

	if ((data.queueFlags & IORING_CQE_F_MORE) == 0) {
		_status = Status::Suspended;
	}

	if (data.result < 0 && data.result != -EAGAIN) {
		cancel(URingData::getErrnoStatus(data.result));
		return;
	}

	if (_status == Status::Suspended) {
		rearm(uring, source);
	}

	sendCompletion(uint32_t(data.result), Status::Ok);
}
#endif

Status PollFdEPollHandle::rearm(EPollData *epoll, PollFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;

		source->event.events = 0;
		if (hasFlag(source->flags, PollFlags::In)) {
			source->event.events |= EPOLLIN;
		}
		if (hasFlag(source->flags, PollFlags::Pri)) {
			source->event.events |= EPOLLPRI;
		}
		if (hasFlag(source->flags, PollFlags::Out)) {
			source->event.events |= EPOLLOUT;
		}
		if (hasFlag(source->flags, PollFlags::Err)) {
			source->event.events |= EPOLLERR;
		}
		if (hasFlag(source->flags, PollFlags::HungUp)) {
			source->event.events |= EPOLLHUP;
		}

		status = epoll->add(source->fd, source->event);
	}
	return status;
}

Status PollFdEPollHandle::disarm(EPollData *epoll, PollFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void PollFdEPollHandle::notify(EPollData *epoll, PollFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	PollFlags pollFlags = PollFlags::None;
	if (data.queueFlags & EPOLLIN) {
		pollFlags |= PollFlags::In;
	}
	if (data.queueFlags & EPOLLPRI) {
		pollFlags |= PollFlags::Pri;
	}
	if (data.queueFlags & EPOLLOUT) {
		pollFlags |= PollFlags::Out;
	}
	if (data.queueFlags & EPOLLHUP) {
		pollFlags |= PollFlags::HungUp;
	}
	if (data.queueFlags & EPOLLERR) {
		pollFlags |= PollFlags::Err;
	}

	sendCompletion(toInt(pollFlags), Status::Ok);

	if ((data.queueFlags & EPOLLERR) || (data.queueFlags & EPOLLHUP)) {
		cancel();
	}
}

#if ANDROID
Status PollFdALooperHandle::rearm(ALooperData *alooper, PollFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		int events = 0;
		if (hasFlag(source->flags, PollFlags::In)) {
			events |= ALOOPER_EVENT_INPUT;
		}
		if (hasFlag(source->flags, PollFlags::Out)) {
			events |= ALOOPER_EVENT_OUTPUT;
		}
		if (hasFlag(source->flags, PollFlags::Err)) {
			events |= ALOOPER_EVENT_ERROR;
		}
		if (hasFlag(source->flags, PollFlags::HungUp)) {
			events |= ALOOPER_EVENT_HANGUP;
		}

		status = alooper->add(source->fd, events, this);
	}
	return status;
}

Status PollFdALooperHandle::disarm(ALooperData *alooper, PollFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void PollFdALooperHandle::notify(ALooperData *alooper, PollFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	PollFlags pollFlags = PollFlags::None;
	if (data.queueFlags & ALOOPER_EVENT_INPUT) {
		pollFlags |= PollFlags::In;
	}
	if (data.queueFlags & ALOOPER_EVENT_OUTPUT) {
		pollFlags |= PollFlags::Out;
	}
	if (data.queueFlags & ALOOPER_EVENT_ERROR) {
		pollFlags |= PollFlags::HungUp;
	}
	if (data.queueFlags & ALOOPER_EVENT_HANGUP) {
		pollFlags |= PollFlags::Err;
	}
	if (data.queueFlags & ALOOPER_EVENT_INVALID) {
		pollFlags |= PollFlags::Err;
	}

	sendCompletion(toInt(pollFlags), Status::Ok);

	if ((data.queueFlags & ALOOPER_EVENT_ERROR) || (data.queueFlags & ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & ALOOPER_EVENT_INVALID)) {
		cancel();
	}
}
#endif

} // namespace stappler::event
