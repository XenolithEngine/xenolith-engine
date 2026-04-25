/**
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

#include "SPEvent-runloop.h"
#include "SPEvent-darwin.h"

namespace sprt::dispatch {

static void RunLoopData_terminate(_CFRunLoopTimerRef timer, void *ptr) {
	auto ctx = reinterpret_cast<RunLoopData::RunContext *>(ptr);
	ctx->queue->stopContext(ctx, ctx->runWakeupFlags, false);
}

static const void *RunLoopData_retainTimer(const void *ptr) {
	sprt::retain(((RunLoopTimerHandle *)ptr));
	return ptr;
}

static void RunLoopData_releaseTimer(const void *ptr) {
	sprt::release(((RunLoopTimerHandle *)ptr), 0);
}

static void RunLoopData_performTimer(_CFRunLoopTimerRef timer, void *ptr) {
	auto handle = (RunLoopTimerHandle *)ptr;
	auto d = handle->getClass()->info->data;
	auto l = (RunLoopData *)d->_platformQueue;

	NotifyData data;
	data.result = 1;
	data.queueFlags = 0;
	data.userFlags = 0;

	d->notify(handle, data);

	if (l->_runContext) {
		++l->_runContext->nevents;
	}
}

void RunLoopData::addTimer(RunLoopTimerHandle *handle, RunLoopTimerSource *source) {
	auto init = _CFAbsoluteTimeGetCurrent() + source->timeout.toDoubleSeconds();
	auto interval = source->interval.toDoubleSeconds();

	// set timer then run
	_CFRunLoopTimerContext context{
		.version = 0,
		.info = handle,
		.retain = &RunLoopData_retainTimer,
		.release = &RunLoopData_releaseTimer,
		.copyDescription = nullptr,
	};

	source->timer = _CFRunLoopTimerCreate(_kCFAllocatorDefault, init, interval, 0, 0,
			&RunLoopData_performTimer, &context);

	_CFRunLoopAddTimer(_runLoop, source->timer, _kCFRunLoopCommonModes);
}

void RunLoopData::removeTimer(RunLoopTimerHandle *handle, RunLoopTimerSource *source) {
	if (source->timer) {
		_CFRunLoopRemoveTimer(_runLoop, source->timer, _kCFRunLoopCommonModes);
		_CFRelease(source->timer);
		source->timer = nullptr;
	}
}

void RunLoopData::trigger(Handle *handle, NotifyData notifyData) {
	auto hRefId = sprt::retain(handle); // protect handle from removal
	auto qRefId = sprt::retain(_queue); // protect self from removal
	_CFRunLoopPerformBlock(_runLoop, _kCFRunLoopCommonModes, ^{
	  if (_runContext) {
		  ++_runContext->nevents;
	  }
	  _data->notify(handle, notifyData);
	  sprt::release(handle, hRefId);
	  sprt::release(_queue, qRefId);
	});
	_CFRunLoopWakeUp(_runLoop);
}

uint32_t RunLoopData::enter(RunContext *ctx, TimeInterval ival) {
	pushContext(ctx, ctx->mode);

	if (!ival) {
		auto result = _CFRunLoopRunInMode(_kCFRunLoopDefaultMode, 0, true);
		while (ctx->state == RunContext::Running && result == _kCFRunLoopRunHandledSource) {
			result = _CFRunLoopRunInMode(_kCFRunLoopDefaultMode, 0, true);
		}
	} else {
		_CFRunLoopRun();
	}

	popContext(ctx);

	return ctx->nevents;
}

Status RunLoopData::submit() { return Status::Ok; }

uint32_t RunLoopData::poll() {
	RunContext ctx;
	ctx.mode = RunContext::Poll;

	return enter(&ctx, TimeInterval());
}

uint32_t RunLoopData::wait(TimeInterval ival) {
	RunContext ctx;
	ctx.mode = RunContext::Wait;

	// set timer then run
	_CFRunLoopTimerContext context{
		.version = 0,
		.info = &ctx,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr,
	};

	_CFRunLoopTimerRef timer = nullptr;
	if (ival && ival != TimeInterval::Infinite) {
		timer = _CFRunLoopTimerCreate(_kCFAllocatorDefault,
				_CFAbsoluteTimeGetCurrent() + ival.toDoubleSeconds(), 0, 0, 0,
				&RunLoopData_terminate, &context);
		_CFRunLoopAddTimer(_runLoop, timer, _kCFRunLoopCommonModes);
	}

	auto ret = enter(&ctx, ival);

	if (timer) {
		_CFRunLoopRemoveTimer(_runLoop, timer, _kCFRunLoopCommonModes);
		_CFRelease(timer);
	}

	return ret;
}

Status RunLoopData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;
	ctx.mode = RunContext::Run;
	ctx.runWakeupFlags = wakeupFlags;

	// set timer then run
	_CFRunLoopTimerContext context{
		.version = 0,
		.info = &ctx,
		.retain = nullptr,
		.release = nullptr,
		.copyDescription = nullptr,
	};

	_CFRunLoopTimerRef timer = nullptr;
	if (ival && ival != TimeInterval::Infinite) {
		timer = _CFRunLoopTimerCreate(_kCFAllocatorDefault,
				_CFAbsoluteTimeGetCurrent() + ival.toDoubleSeconds(), 0, 0, 0,
				&RunLoopData_terminate, &context);
		_CFRunLoopAddTimer(_runLoop, timer, _kCFRunLoopCommonModes);
	}

	while (ctx.state == RunContext::Running) { enter(&ctx, ival); }

	if (timer) {
		_CFRunLoopRemoveTimer(_runLoop, timer, _kCFRunLoopCommonModes);
		_CFRelease(timer);
	}

	return ctx.wakeupStatus;
}

Status RunLoopData::wakeup(WakeupFlags flags) {
	auto refId = sprt::retain(_queue);
	_CFRunLoopPerformBlock(_runLoop, _kCFRunLoopCommonModes, ^{
	  stopContext(nullptr, flags, true);
	  sprt::release(_queue, refId);
	});
	return Status::Ok;
}

void RunLoopData::cancel() {
	// we do not need to explicitly stop RunContext, if we on main thread, and we don't have one
	if (_data->_threadId != dispatch::Thread::getCurrentThreadId() || _runContext) {
		auto refId = sprt::retain(_queue);
		_CFRunLoopPerformBlock(_runLoop, _kCFRunLoopCommonModes, ^{
		  stopRootContext(WakeupFlags::ContextDefault, true);
		  sprt::release(_queue, refId);
		});
	}
}

RunLoopData::RunLoopData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) {
	_stopContext = [](RunContext *ctx) {
		auto q = static_cast<RunLoopData *>(ctx->queue);
		_CFRunLoopStop(q->_runLoop);
	};

	_runLoop = _CFRunLoopGetCurrent();
	_runMode = _CFStringCreateWithUTF8String(nullptr, "org.stappler.event.DefaultRunMode");

	_CFRunLoopAddCommonMode(_runLoop, _runMode);
}

RunLoopData::~RunLoopData() {
	_CFRelease(_runMode);
	_runLoop = nullptr;
}

bool RunLoopTimerSource::init(const TimerInfo &info) {
	timeout = info.timeout;
	interval = info.interval;
	count = info.count;
	if (timeout != interval || count == 1) {
		//oneshot = true;
	}
	return true;
}

void RunLoopTimerSource::cancel() { }

double RunLoopTimerSource::getNextInterval() const {
	return value == 0 ? timeout.toDoubleSeconds() : interval.toDoubleSeconds();
}

bool RunLoopTimerHandle::init(HandleClass *cl, TimerInfo &&info) {
	static_assert(sizeof(RunLoopTimerSource) <= DataSize
			&& sprt::is_standard_layout<RunLoopTimerSource>::value);

	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	} else if (!info.timeout) {
		info.timeout = info.interval;
	}

	auto source = new (_data) RunLoopTimerSource();
	return source->init(info);
}

Status RunLoopTimerHandle::rearm(RunLoopData *queue, RunLoopTimerSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		queue->addTimer(this, source);
	}
	return status;
}

Status RunLoopTimerHandle::disarm(RunLoopData *queue, RunLoopTimerSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		queue->removeTimer(this, source);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void RunLoopTimerHandle::notify(RunLoopData *queue, RunLoopTimerSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	auto count = source->count;
	auto current = source->value;

	++current;
	source->value = current;

	if (count == TimerInfo::Infinite || current < count) {
		_status = Status::Ok;
	} else {
		cancel(Status::Done, source->value);
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

bool RunLoopTimerHandle::reset(TimerInfo &&info) {
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto source = reinterpret_cast<RunLoopTimerSource *>(_data);
	return source->init(info) && Handle::reset();
}

bool RunLoopThreadSource::init() { return true; }

void RunLoopThreadSource::cancel() { }

bool RunLoopThreadHandle::init(HandleClass *cl) {
	static_assert(sizeof(RunLoopThreadSource) <= DataSize
			&& sprt::is_standard_layout<RunLoopThreadSource>::value);

	if (!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = new (_data) RunLoopThreadSource();
	return source->init();
}

Status RunLoopThreadHandle::rearm(RunLoopData *queue, RunLoopThreadSource *source) {
	return prepareRearm();
}

Status RunLoopThreadHandle::disarm(RunLoopData *queue, RunLoopThreadSource *source) {
	return prepareDisarm();
}

void RunLoopThreadHandle::notify(RunLoopData *queue, RunLoopThreadSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return; // just exit
	}

	auto performUnlock = [&] { performAll([&](uint32_t count) { _mutex.unlock(); }); };

	if (data.result > 0) {
		if constexpr (RUNLOOP_THREAD_NONBLOCK) {
			if (_mutex.try_lock()) {
				performUnlock();
			}
		} else {
			_mutex.lock();
			performUnlock();
		}
	} else {
		cancel(data.result == 0 ? Status::Done : Status(data.result));
	}
}

Status RunLoopThreadHandle::perform(Rc<Task> &&task) {
	auto q = reinterpret_cast<RunLoopData *>(_class->info->data->_platformQueue);
	sprt::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	NotifyData n{1, 0, 0};
	q->trigger(this, n);
	return Status::Ok;
}

Status RunLoopThreadHandle::perform(dispatch::Function<void()> &&func, Ref *target,
		StringView tag) {
	auto q = reinterpret_cast<RunLoopData *>(_class->info->data->_platformQueue);

	sprt::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});

	NotifyData n{1, 0, 0};
	q->trigger(this, n);
	return Status::Ok;
}

} // namespace sprt::dispatch
