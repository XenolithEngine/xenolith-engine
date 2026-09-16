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
#include "../fd/SPEventProcess.h"

#include <unistd.h>

// macOS provides waitpid() in libSystem, but the runtime's freestanding include path does not expose
// <sys/wait.h>; declare the prototype directly (the exit code is decoded via decodeWaitStatus()). The
// option value 1 is WNOHANG (stable across the BSD/macOS ABI) — poll the child without blocking.
extern "C" int waitpid(int __pid, int *__status, int __options);

namespace sprt::dispatch {

static constexpr int RunLoopWaitNoHang = 1; // WNOHANG

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

bool RunLoopProcessHandle::init(HandleClass *cl, int pid, CompletionHandle<ProcessHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	_pid = pid;
	return true;
}

void RunLoopProcessHandle::start() {
	auto qdata = _class->info->data;

	// A small repeating reactor timer drives both reading and reaping. 1ms keeps the
	// output near-live while costing only a non-blocking read + waitpid() per fire
	// (cheap no-ops while the child has nothing to say). The driver is a separate,
	// reactor-suspendable handle: a graceful wakeup suspends it alongside this one.
	TimerInfo tinfo;
	tinfo.timeout = TimeInterval::milliseconds(1);
	tinfo.interval = TimeInterval::milliseconds(1);
	tinfo.count = TimerInfo::Infinite;
	tinfo.completion = TimerInfo::Completion::create<RunLoopProcessHandle>(this,
			[](RunLoopProcessHandle *self, TimerHandle *, uint32_t, Status status) {
		if (status == Status::Ok) {
			self->poll();
		}
	});

	auto timer = qdata->scheduleTimer(sprt::move(tinfo));
	if (!timer) {
		// the RunLoop backend always provides a timer factory, so this is effectively
		// unreachable; finish defensively rather than poll a child that can never be reaped
		_exitCode = -1;
		finish();
		return;
	}
	_driver = timer;
	qdata->runHandle(timer.get());
}

void RunLoopProcessHandle::poll() {
	if (_finishing) {
		return;
	}

	auto state = static_cast<ProcessState *>(getUserdata());

	// drain whatever the child has produced so far (non-blocking; loops to EAGAIN/EOF)
	if (state && state->readFd >= 0) {
		drainProcessPipe(state->readFd, state);
	}

	// has the child terminated? WNOHANG reaps the zombie when it has, returns 0 while
	// it still runs, and -1 (ECHILD) if it is already gone
	int status = 0;
	auto r = ::waitpid(_pid, &status, RunLoopWaitNoHang);
	if (r == 0) {
		return; // still running; poll again on the next fire
	}
	_reaped = true; // reaped here: terminate() must not kill a recycled pid
	_exitCode = (r > 0) ? decodeWaitStatus(status) : -1;

	// flush any bytes buffered in the pipe at exit, then stop reading
	if (state && state->readFd >= 0) {
		drainProcessPipe(state->readFd, state);
		::close(state->readFd);
		state->readFd = -1;
	}

	finish();
}

void RunLoopProcessHandle::finish() {
	if (_finishing) {
		return;
	}
	_finishing = true;

	// Defer the completion out of the driver-timer callback: cancel(Done) stops the
	// driver, which must not happen on the timer's own notify stack. perform() runs
	// the cancel in this cycle's runAllTasks, after the callback unwinds; the captured
	// Rc keeps this handle alive until then. Mirrors FileState::finalizeChannel.
	auto qdata = _class->info->data;
	Rc<ProcessHandle> self(this);
	auto code = uint32_t(_exitCode);
	if (qdata->perform([self, code]() { self->cancel(Status::Done, code); }, this) != Status::Ok) {
		// not inside a notify cycle: complete directly
		cancel(Status::Done, code);
	}
}

void RunLoopProcessHandle::terminate() {
	_finishing = true;
	if (_driver) {
		_driver->cancel();
		_driver = nullptr;
	}
	auto state = static_cast<ProcessState *>(getUserdata());
	if (state && state->readFd >= 0) {
		::close(state->readFd);
		state->readFd = -1;
	}
	// If the handle is cancelled while the child is still running (poll() never reaped
	// it), terminate and reap it so it neither outlives its handle nor leaks a zombie.
	// `_reaped` guards against signalling an already-reaped (recycled) pid.
	if (!_reaped && _pid > 0) {
		killProcessChild(_pid);
		_reaped = true;
	}
}

NativeHandle RunLoopProcessHandle::getNativeHandle() const { return _pid; }

void setupRunLoopProcessHandleClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_cast<RunLoopProcessHandle *>(handle)->start();
		return HandleClass::run(cl, handle, data);
	};

	// Plain bookkeeping (like the inline file handle): the driver timer is itself a
	// suspendable handle, so a graceful wakeup suspends it directly; marking this
	// handle resumable is what keeps it alive in the queue's _suspendableHandles set
	// until it completes. Teardown happens in cancelFn, not here.
	cl->suspendFn = HandleClass::suspend;
	cl->resumeFn = HandleClass::resume;

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		static_cast<RunLoopProcessHandle *>(handle)->terminate();
		return HandleClass::cancel(cl, handle, data, st);
	};
}

Rc<ProcessHandle> spawnProcessRunLoop(QueueData *data, HandleClass *processClass, ProcessInfo &&info,
		Ref *ref) {
	int pid = -1;
	int readFd = -1;
	if (!posixSpawnPipe(info.command, &pid, &readFd)) {
		return nullptr;
	}

	auto state = Rc<ProcessState>::alloc();
	state->reader = sprt::move(info.reader);
	state->userRef = ref;
	state->readFd = readFd;

	auto proc = Rc<RunLoopProcessHandle>::create(processClass, pid, sprt::move(info.completion));
	if (!proc) {
		::close(readFd);
		int status = 0;
		::waitpid(pid, &status, 0);
		return nullptr;
	}

	// the process handle owns ProcessState (userdata); the timer-driven reader/reaper
	// is created when the handle is run (runFn -> start())
	proc->setUserdata(state);
	return proc;
}

} // namespace sprt::dispatch
