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

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "SPTime.h"
#include "SPEvent-epoll.h"

#include <signal.h>

namespace STAPPLER_VERSIONIZED stappler::event {

static constexpr uint32_t EPOLL_CANCEL_FLAG = 0x8000'0000;

#if LINUX
static constexpr int sp_sys_epoll_pwait2 = 441;

// We use the old glibc, so we implement it ourselves
// The actual presence of a function in the kernel can be checked using EPollFlags::HaveEPollPWait2
static int sp_epoll_pwait2(int fd, struct epoll_event *ev, int maxev, const struct timespec *tmo,
		const sigset_t *s) {
	struct _linux_timespec tmo64, *ptmo64 = NULL;
	if (tmo != NULL) {
		tmo64.tv_sec = tmo->tv_sec;
		tmo64.tv_nsec = tmo->tv_nsec;
		ptmo64 = &tmo64;
	}

	return syscall(sp_sys_epoll_pwait2, fd, ev, maxev, ptmo64, s, _NSIG / 8);
}
#endif

Status EPollData::add(int fd, const epoll_event &ev) {
	auto ret = ::epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, const_cast<epoll_event *>(&ev));
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status EPollData::remove(int fd) {
	auto ret = ::epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, nullptr);
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status EPollData::runPoll(TimeInterval ival) {
	if (_processedEvents < _receivedEvents) {
		return Status::Ok;
	}

	const sigset_t *sigset = nullptr;
	if (hasFlag(_flags, QueueFlags::Protected)) {
		sigset = _signalFd->getCurrentSigset();
	}

	int nevents = 0;
#if LINUX && __USE_GNU
	if (hasFlag(_eflags, EPollFlags::HaveEPollPWait2) && ival && ival != TimeInterval::Infinite) {
		struct timespec timeout;

		setNanoTimespec(timeout, ival);

		nevents = sp_epoll_pwait2(_epollFd, _events.data(), _events.size(), &timeout, sigset);
	} else {
		nevents = ::epoll_pwait(_epollFd, _events.data(), _events.size(),
				(ival == TimeInterval::Infinite) ? -1 : ival.toMillis(), sigset);
	}
#else
	nevents = ::epoll_pwait(_epollFd, _events.data(), _events.size(),
			(ival == TimeInterval::Infinite) ? -1 : ival.toMillis(), sigset);
#endif

	if (nevents >= 0) {
		_processedEvents = 0;
		_receivedEvents = nevents;

		return Status::Ok;
	} else {
		return sprt::status::errnoToStatus(errno);
	}
}

uint32_t EPollData::processEvents() {
	uint32_t count = 0;
	NotifyData data;

	while (_processedEvents < _receivedEvents) {
		auto &ev = _events.at(_processedEvents++);

		auto h = (Handle *)ev.data.ptr;
		if (h) {
			auto refId = h->retain();

			data.result = 0;
			data.queueFlags = ev.events;
			data.userFlags = 0;

			_data->notify(h, data);

			h->release(refId);
		}
		++count;
	}
	_receivedEvents = _processedEvents = 0;
	return count;
}

Status EPollData::submit() { return Status::Ok; }

uint32_t EPollData::poll() {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	auto status = runPoll(TimeInterval());
	if (status == Status::Ok) {
		result = processEvents();
	}

	popContext(&ctx);

	return result;
}

uint32_t EPollData::wait(TimeInterval ival) {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	auto status = runPoll(ival);
	if (status == Status::Ok) {
		result = processEvents();
	}

	popContext(&ctx);

	return result;
}

Status EPollData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;
	ctx.wakeupStatus = Status::Suspended;
	ctx.runWakeupFlags = wakeupFlags;

	Rc<Handle> timerHandle;
	if (ival && ival != TimeInterval::Infinite) {
		// set timeout
		timerHandle = _queue->get()->schedule(ival,
				[this, wakeupFlags, ctx = &ctx](Handle *, bool success) {
			if (success) {
				stopContext(ctx, wakeupFlags, false);
			}
		});
	}

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		auto status = runPoll(TimeInterval::Infinite);
		if (status == Status::Ok) {
			processEvents();
		} else if (status != Status::ErrorInterrupted) {
			log::source().error("event::EPollData", "epoll error: ", status);
			ctx.wakeupStatus = status;
			break;
		}
	}

	if (timerHandle) {
		// remove timeout if set
		timerHandle->cancel();
		timerHandle = nullptr;
	}

	popContext(&ctx);

	return ctx.wakeupStatus;
}

Status EPollData::wakeup(WakeupFlags flags) {
	_eventFd->write(1, toInt(flags));
	return Status::Ok;
}

void EPollData::runInternalHandles() {
	// run internal services
	if (_signalFd && hasFlag(_flags, QueueFlags::Protected)) {
		// enable with current mask
		_signalFd->enable();
		_data->runHandle(_signalFd);
	}

	_data->runHandle(_eventFd);
}

void EPollData::cancel() {
	_eventFd->write(1, toInt(WakeupFlags::ContextDefault) | EPOLL_CANCEL_FLAG);
}

EPollData::EPollData(QueueRef *q, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs)
: PlatformQueueData(q, data, info.flags) {

	_eventFd = Rc<EventFdEPollHandle>::create(&data->_epollEventFdClass,
			CompletionHandle<EventFdEPollHandle>::create<EPollData>(this,
					[](EPollData *data, EventFdEPollHandle *h, uint32_t value, Status st) {
		if (st == Status::Ok && data->_runContext) {
			if (value & EPOLL_CANCEL_FLAG) {
				data->stopRootContext(WakeupFlags::ContextDefault, true);
			} else {
				data->stopContext(data->_runContext, WakeupFlags(value), true);
			}
		}
	}));

	if (!_eventFd) {
		log::source().error("event::Queue", "Fail to initialize eventfd");
		return;
	}

	if (hasFlag(_flags, QueueFlags::Protected)) {
		_signalFd = Rc<SignalFdEPollHandle>::create(&data->_epollSignalFdClass, sigs);
		if (!_signalFd) {
			log::source().error("event::Queue", "Fail to initialize signalfd");
			return;
		}
	}

#if LINUX
	struct utsname buffer;
	if (uname(&buffer) != 0) {
		log::source().info("event::EPollData", "Fail to detect kernel version");
		return;
	}

	if (strverscmp(buffer.release, "5.11.0") >= 0) {
		_eflags |= EPollFlags::HaveEPollPWait2;
	}
#endif

	auto cleanup = [&] {
		if (_epollFd >= 0) {
			::close(_epollFd);
			_epollFd = -1;
		}
	};

	_epollFd = ::epoll_create1(EPOLL_CLOEXEC);
	if (_epollFd < 0) {
		cleanup();
		return;
	}

	auto size = info.completeQueueSize;

	if (size == 0) {
		size = info.submitQueueSize;
	}
	_events.resize(size);

	_data->_handle = _epollFd;
}

EPollData::~EPollData() {
	if (_epollFd >= 0) {
		::close(_epollFd);
		_epollFd = -1;
	}
}

} // namespace stappler::event
