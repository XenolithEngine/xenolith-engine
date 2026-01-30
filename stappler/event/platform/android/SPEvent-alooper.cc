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

#include "SPEvent-alooper.h"

#include <sprt/c/sys/__sprt_alooper.h>

namespace STAPPLER_VERSIONIZED stappler::event {

static constexpr uint32_t ALOOPER_CANCEL_FLAG = 0x8000'0000;

bool ALooperData::checkSupport() {
#if __SPRT_CONFIG_HAVE_ALOOPER
	return true;
#else
	return false;
#endif
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wattribute-warning"

Status ALooperData::add(int fd, int events, Handle *handle) {
	if (__sprt_ALooper_addFd(_looper, fd, 0, events,
				[](int fd, int events, void *ptr) {
		NotifyData data;
		auto h = (Handle *)ptr;

		auto refId = h->retain();

		data.result = fd;
		data.queueFlags = events;
		data.userFlags = 0;

		h->getClass()->info->data->notify(h, data);

		auto st = h->getStatus();

		h->release(refId);

		if (st == Status::Ok) {
			return 1;
		} else {
			return 0;
		}
	}, handle)
			== 1) {

		return Status::Ok;
	}
	return Status::ErrorUnknown;
}

Status ALooperData::remove(int fd) {
	auto ret = __sprt_ALooper_removeFd(_looper, fd);
	if (ret == 1) {
		return Status::Ok;
	} else if (ret == 0) {
		return Status::Declined;
	}
	return Status::ErrorUnknown;
}

Status ALooperData::submit() { return Status::Ok; }

uint32_t ALooperData::poll() {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	auto ret = __sprt_ALooper_pollOnce(0, nullptr, nullptr, nullptr);
	while (ret == __SPRT_ALOOPER_POLL_CALLBACK) { ++result; }

	popContext(&ctx);

	return result;
}

uint32_t ALooperData::wait(TimeInterval ival) {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	auto ret = __sprt_ALooper_pollOnce(ival.toMillis(), nullptr, nullptr, nullptr);
	if (ret == __SPRT_ALOOPER_POLL_CALLBACK) {
		++result;
	}

	popContext(&ctx);

	return result;
}

Status ALooperData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;

	ctx.wakeupStatus = Status::Suspended;
	ctx.wakeupTimeout = wakeupTimeout;
	ctx.runWakeupFlags = wakeupFlags;

	Rc<Handle> timerHandle;
	if (ival && ival != TimeInterval::Infinite) {
		// set timeout
		timerHandle = _queue->get()->schedule(ival,
				[this, wakeupFlags, ctx = &ctx](Handle *handle, bool success) {
			if (success) {
				stopContext(ctx, wakeupFlags, false);
			}
		});
	}

	pushContext(&ctx, RunContext::Run);

	int ret = 0;

	while (ctx.state == RunContext::Running) {
		ret = __sprt_ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
		if (ret == __SPRT_ALOOPER_POLL_ERROR) {
			log::source().error("event::ALooperData", "ALooper error: ", ret);
			ctx.wakeupStatus = Status::ErrorUnknown;
			break;
		}
	}

	if (ret == __SPRT_ALOOPER_POLL_ERROR) {
		log::source().error("event::Queue", "ALooper failed with error");
	}

	if (timerHandle) {
		// remove timeout if set
		timerHandle->cancel();
		timerHandle = nullptr;
	}

	popContext(&ctx);

	return ctx.wakeupStatus;
}

Status ALooperData::wakeup(WakeupFlags flags) {
	_eventFd->write(1, toInt(flags));
	return Status::Ok;
}

void ALooperData::runInternalHandles() { _data->runHandle(_eventFd); }

void ALooperData::cancel() {
	_eventFd->write(1, toInt(WakeupFlags::ContextDefault) | ALOOPER_CANCEL_FLAG);
}

ALooperData::ALooperData(QueueRef *q, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs)
: PlatformQueueData(q, data, info.flags) {

	_stopContext = [](RunContext *ctx) {
		auto q = static_cast<ALooperData *>(ctx->queue);
		__sprt_ALooper_wake(q->_looper);
	};

	if (hasFlag(_flags, QueueFlags::Protected)) {
		log::source().warn("event::Queue",
				"QueueFlags::Protected is not supported by ALooper queue, ignored");
	}

	_looper = __sprt_ALooper_prepare(0);

	_eventFd = Rc<EventFdALooperHandle>::create(&data->_alooperEventFdClass,
			CompletionHandle<EventFdALooperHandle>::create<ALooperData>(this,
					[](ALooperData *data, EventFdALooperHandle *h, uint32_t value, Status st) {
		if (st == Status::Ok && data->_runContext) {
			if (value & ALOOPER_CANCEL_FLAG) {
				data->stopRootContext(WakeupFlags::ContextDefault, true);
			} else {
				data->stopContext(data->_runContext, WakeupFlags(value), true);
			}
		}
	}));
}

ALooperData::~ALooperData() {
	if (_looper) {
		__sprt_ALooper_release(_looper);
		_looper = nullptr;
	}
}

#pragma clang diagnostic pop

} // namespace stappler::event
