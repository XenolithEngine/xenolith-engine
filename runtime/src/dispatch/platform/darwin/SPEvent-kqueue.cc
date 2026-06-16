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

#include "SPEvent-kqueue.h"
#include "SPEvent-darwin.h"

#include <unistd.h>
#include <sprt/runtime/log.h>

// macOS provides waitpid() in libSystem, but the runtime's freestanding include path does not expose
// <sys/wait.h>; declare the prototype directly (the exit code is decoded via decodeWaitStatus()).
extern "C" int waitpid(int __pid, int *__status, int __options);

namespace sprt::dispatch {

static constexpr uint32_t KQUEUE_CANCEL_FLAG = 0x0080'0000;

Status KQueueData::update(const struct kevent &ev) {
	struct timespec timeout;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	auto result = kevent(_kqueueFd, &ev, 1, nullptr, 0, &timeout);

	return result == 0 ? Status::Ok : sprt::status::errnoToStatus(__sprt_errno);
}

Status KQueueData::update(SpanView<struct kevent> ev) {
	struct timespec timeout;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;

	auto result = kevent(_kqueueFd, ev.data(), static_cast<int>(ev.size()), nullptr, 0, &timeout);

	return result == 0 ? Status::Ok : sprt::status::errnoToStatus(__sprt_errno);
}

Status KQueueData::runPoll(TimeInterval ival) {
	if (_processedEvents < _receivedEvents) {
		return Status::Ok;
	}

	int nevents = 0;
	struct timespec timeout;
	timeout.tv_sec = ival.toSeconds();
	timeout.tv_nsec = (ival.toMicroseconds() - ival.toSeconds() * 1'000'000) * 1'000;

	nevents = kevent(_kqueueFd, nullptr, 0, _events.data(), static_cast<int>(_events.size()),
			(ival == TimeInterval::Infinite) ? nullptr : &timeout);

	if (nevents >= 0) {
		_processedEvents = 0;
		_receivedEvents = nevents;

		return Status::Ok;
	} else {
		return sprt::status::errnoToStatus(__sprt_errno);
	}
}

uint32_t KQueueData::processEvents(RunContext *ctx) {
	uint32_t count = 0;

	auto processHandleEvent = [&](const struct kevent &ev) {
		if (ev.udata && ev.udata != this) {
			auto h = (Handle *)ev.udata;
			auto refId = sprt::retain(h);

			NotifyData data;
			data.result = ev.data;
			data.queueFlags = ev.flags;
			data.userFlags = 0;

			_data->notify(h, data);

			sprt::release(h, refId);
		}
	};

	while (_processedEvents < _receivedEvents) {
		auto &ev = _events.at(_processedEvents++);
		switch (ev.filter) {
		case EVFILT_TIMER:
			if (ev.udata == this) {
				// self-wakeup timer from run()
				stopContext(reinterpret_cast<RunContext *>(ev.ident), WakeupFlags::ContextDefault,
						false);
			} else {
				processHandleEvent(ev);
			}
			break;
		case EVFILT_SIGNAL: break;
		case EVFILT_USER:
			if (ev.ident == reinterpret_cast<uintptr_t>(this)) {
				// user wakeup signal - terminate current context
				if (ev.fflags & KQUEUE_CANCEL_FLAG) {
					stopRootContext(WakeupFlags::ContextDefault, true);
				} else {
					stopContext(ctx, WakeupFlags(ev.fflags & NOTE_FFLAGSMASK), true);
				}
			} else {
				processHandleEvent(ev);
			}
			break;
		default: processHandleEvent(ev); break;
		}

		++count;
	}
	_receivedEvents = _processedEvents = 0;
	return count;
}

Status KQueueData::submit() { return Status::Ok; }

uint32_t KQueueData::poll() {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	auto status = runPoll(TimeInterval());
	if (status == Status::Ok) {
		result = processEvents(&ctx);
	}

	popContext(&ctx);

	return result;
}

uint32_t KQueueData::wait(TimeInterval ival) {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	auto status = runPoll(ival);
	if (status == Status::Ok) {
		result = processEvents(&ctx);
	}

	popContext(&ctx);

	return result;
}

Status KQueueData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;
	ctx.runWakeupFlags = wakeupFlags;

	struct kevent events[1];
	// A self-wakeup timer is registered only for a finite, non-zero interval.
	const bool hasTimer = (ival && ival != TimeInterval::Infinite);
	if (hasTimer) {
		// udata must be `this` (the KQueueData sentinel processEvents() checks to
		// recognise the self-timer); the RunContext* is carried in `ident`.
		EV_SET(&events[0], reinterpret_cast<intptr_t>(&ctx), EVFILT_TIMER, EV_ADD | EV_ONESHOT,
				NOTE_USECONDS, ival.toMicros(), this);
		update(sprt::makeSpanView(events, 1));
	}

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		auto status = runPoll(TimeInterval::Infinite);
		if (status == Status::Ok) {
			processEvents(&ctx);
		} else if (status != Status::ErrorInterrupted) {
			sprt::oslog::vperror(__SPRT_LOCATION, "event::KQueueData", "kqueue error: ", status);
			ctx.wakeupStatus = status;
			break;
		}
	}

	if (hasTimer) {
		events[0].flags = EV_DELETE;
		update(sprt::makeSpanView(events, 1));
	}

	popContext(&ctx);

	return ctx.wakeupStatus;
}

Status KQueueData::wakeup(WakeupFlags flags) {
	struct kevent signal;
	EV_SET(&signal, reinterpret_cast<uintptr_t>(this), EVFILT_USER, 0,
			NOTE_TRIGGER | (NOTE_FFLAGSMASK & toInt(flags)), 0, this);
	update(signal);
	return Status::Ok;
}

void KQueueData::cancel() {
	struct kevent signal;
	EV_SET(&signal, reinterpret_cast<uintptr_t>(this), EVFILT_USER, 0,
			NOTE_TRIGGER | (NOTE_FFLAGSMASK & KQUEUE_CANCEL_FLAG), 0, this);
	update(signal);
}

KQueueData::KQueueData(QueueRef *q, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs)
: PlatformQueueData(q, data, info.flags) {

	auto cleanup = [&] {
		if (_kqueueFd >= 0) {
			::close(_kqueueFd);
			_kqueueFd = -1;
		}
	};

	_kqueueFd = kqueue();
	if (_kqueueFd < 0) {
		cleanup();
		return;
	}

	auto size = info.completeQueueSize;

	if (size == 0) {
		size = info.submitQueueSize;
	}
	_events.resize(size);

	dispatch::Vector<struct kevent> ev;
	ev.reserve(sigs.size() + 1);

	EV_SET(&ev.emplace_back(), reinterpret_cast<uintptr_t>(this), EVFILT_USER, EV_ADD | EV_CLEAR,
			NOTE_FFNOP, 0, this);
	for (auto &it : sigs) { EV_SET(&ev.emplace_back(), it, EVFILT_SIGNAL, EV_ADD, 0, 0, this); }

	update(ev);

	_data->_handle = _kqueueFd;
}

KQueueData::~KQueueData() {
	if (_kqueueFd >= 0) {
		::close(_kqueueFd);
		_kqueueFd = -1;
	}
}

bool KQueueTimerSource::init(const TimerInfo &info) {
	timeout = info.timeout;
	interval = info.interval;
	count = info.count;
	if (timeout != interval || count == 1) {
		oneshot = true;
	}
	return true;
}

void KQueueTimerSource::cancel() { }

uint64_t KQueueTimerSource::getNextInterval() const {
	return value == 0 ? timeout.toMicros() : interval.toMicros();
}

bool KQueueTimerHandle::init(HandleClass *cl, TimerInfo &&info) {
	static_assert(sizeof(KQueueTimerSource) <= DataSize
			&& sprt::is_standard_layout<KQueueTimerSource>::value);

	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	} else if (!info.timeout) {
		info.timeout = info.interval;
	}

	auto source = new (_data) KQueueTimerSource();
	return source->init(info);
}

Status KQueueTimerHandle::rearm(KQueueData *queue, KQueueTimerSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		struct kevent ev;
		if (source->oneshot) {
			EV_SET(&ev, reinterpret_cast<intptr_t>(source), EVFILT_TIMER, EV_ADD | EV_ONESHOT,
					NOTE_USECONDS, source->getNextInterval(), this);
		} else {
			EV_SET(&ev, reinterpret_cast<intptr_t>(source), EVFILT_TIMER, EV_ADD | EV_CLEAR,
					NOTE_USECONDS, source->getNextInterval(), this);
		}
		status = queue->update(ev);
	}
	return status;
}

Status KQueueTimerHandle::disarm(KQueueData *queue, KQueueTimerSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, reinterpret_cast<intptr_t>(source), EVFILT_TIMER, EV_DELETE, 0, 0, this);
		status = queue->update(ev);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void KQueueTimerHandle::notify(KQueueData *queue, KQueueTimerSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// event handling is suspended when we receive notification
	if (source->oneshot) {
		_status = Status::Suspended;
	}

	auto count = source->count;
	auto current = source->value;

	++current;
	source->value = current;

	if (count == TimerInfo::Infinite || current < count) {
		if (source->oneshot) {
			source->oneshot = false;
			rearm(queue, source);
		}
		_status = Status::Ok;
	} else {
		cancel(Status::Done, source->value);
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

bool KQueueTimerHandle::reset(TimerInfo &&info) {
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto source = reinterpret_cast<KQueueTimerSource *>(_data);
	return source->init(info) && Handle::reset();
}

bool KQueueThreadSource::init() { return true; }

void KQueueThreadSource::cancel() { }

bool KQueueThreadHandle::init(HandleClass *cl) {
	static_assert(sizeof(KQueueThreadSource) <= DataSize
			&& sprt::is_standard_layout<KQueueThreadSource>::value);

	if (!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = new (_data) KQueueThreadSource();
	return source->init();
}

Status KQueueThreadHandle::rearm(KQueueData *queue, KQueueThreadSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, reinterpret_cast<uintptr_t>(source), EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0,
				this);
		status = queue->update(ev);
	}
	return status;
}

Status KQueueThreadHandle::disarm(KQueueData *queue, KQueueThreadSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, reinterpret_cast<uintptr_t>(source), EVFILT_USER, EV_DELETE, 0, 0, this);
		status = queue->update(ev);
	}
	return status;
}

void KQueueThreadHandle::notify(KQueueData *queue, KQueueThreadSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return; // just exit
	}

	auto performUnlock = [&] { performAll([&](uint32_t count) { _mutex.unlock(); }); };

	if (data.result > 0) {
		if constexpr (KQUEUE_THREAD_NONBLOCK) {
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

Status KQueueThreadHandle::perform(Rc<Task> &&task) {
	auto q = reinterpret_cast<KQueueData *>(_class->info->data->_platformQueue);
	sprt::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	struct kevent ev;
	EV_SET(&ev, reinterpret_cast<uintptr_t>(_data), EVFILT_USER, 0, NOTE_TRIGGER, 1, this);
	q->update(ev);

	return Status::Ok;
}

Status KQueueThreadHandle::perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) {
	auto q = reinterpret_cast<KQueueData *>(_class->info->data->_platformQueue);

	sprt::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});

	struct kevent ev;
	EV_SET(&ev, reinterpret_cast<uintptr_t>(_data), EVFILT_USER, 0, NOTE_TRIGGER, 1, this);
	q->update(ev);

	return Status::Ok;
}

bool ReadKQueueSource::init(int f, PollFlags fl) {
	fd = f;
	flags = fl;
	return true;
}

void ReadKQueueSource::cancel() {
	if (hasFlag(flags, PollFlags::CloseFd) && fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

bool ReadKQueueHandle::init(HandleClass *cl, int fd, PollFlags flags,
		CompletionHandle<PollHandle> &&c) {
	static_assert(sizeof(ReadKQueueSource) <= DataSize
			&& sprt::is_standard_layout<ReadKQueueSource>::value);
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	auto source = new (_data) ReadKQueueSource();
	return source->init(fd, flags);
}

Status ReadKQueueHandle::rearm(KQueueData *queue, ReadKQueueSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, source->fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, this);
		status = queue->update(ev);
	}
	return status;
}

Status ReadKQueueHandle::disarm(KQueueData *queue, ReadKQueueSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, source->fd, EVFILT_READ, EV_DELETE, 0, 0, this);
		status = queue->update(ev);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ReadKQueueHandle::notify(KQueueData *queue, ReadKQueueSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}
	PollFlags pollFlags = PollFlags::In; // EVFILT_READ fired: data (or EOF) available
	if (data.queueFlags & EV_EOF) {
		pollFlags |= PollFlags::HungUp;
	}
	if (data.queueFlags & EV_ERROR) {
		pollFlags |= PollFlags::Err;
	}
	sendCompletion(toInt(pollFlags), Status::Ok);
}

NativeHandle ReadKQueueHandle::getNativeHandle() const {
	return reinterpret_cast<const ReadKQueueSource *>(_data)->fd;
}

bool ReadKQueueHandle::reset(PollFlags flags) {
	reinterpret_cast<ReadKQueueSource *>(_data)->flags = flags;
	return Handle::reset();
}

bool ProcessKQueueSource::init(int p) {
	pid = p;
	return true;
}

void ProcessKQueueSource::cancel() { }

bool ProcessKQueueHandle::init(HandleClass *cl, int pid, CompletionHandle<ProcessHandle> &&c) {
	static_assert(sizeof(ProcessKQueueSource) <= DataSize
			&& sprt::is_standard_layout<ProcessKQueueSource>::value);
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	auto source = new (_data) ProcessKQueueSource();
	return source->init(pid);
}

Status ProcessKQueueHandle::rearm(KQueueData *queue, ProcessKQueueSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, source->pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT | NOTE_EXITSTATUS, 0,
				this);
		status = queue->update(ev);
	}
	return status;
}

Status ProcessKQueueHandle::disarm(KQueueData *queue, ProcessKQueueSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		struct kevent ev;
		EV_SET(&ev, source->pid, EVFILT_PROC, EV_DELETE, 0, 0, this);
		status = queue->update(ev);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ProcessKQueueHandle::notify(KQueueData *queue, ProcessKQueueSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// the child exited (NOTE_EXIT). data.result carries the wait-status when NOTE_EXITSTATUS is set,
	// but the zombie must still be reaped; use the reap status for the exit code.
	int status = 0;
	::waitpid(source->pid, &status, 0);
	_exitCode = decodeWaitStatus(status);

	auto state = static_cast<ProcessState *>(getUserdata());
	if (state) {
		drainProcessPipe(state->readFd, state); // flush any final output first
		state->readFd = -1;
		if (state->readerHandle) {
			state->readerHandle->cancel();
		}
	}

	// EVFILT_PROC was registered EV_ONESHOT and is already consumed, so skip the disarm.
	_status = Status::Suspended;
	cancel(Status::Done, uint32_t(_exitCode));
}

NativeHandle ProcessKQueueHandle::getNativeHandle() const {
	return reinterpret_cast<const ProcessKQueueSource *>(_data)->pid;
}

Rc<ProcessHandle> spawnProcessKQueue(QueueData *data, HandleClass *processClass, ProcessInfo &&info,
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

	auto proc = Rc<ProcessKQueueHandle>::create(processClass, pid, sprt::move(info.completion));
	if (!proc) {
		::close(readFd);
		int status = 0;
		::waitpid(pid, &status, 0);
		return nullptr;
	}

	// the process handle owns ProcessState (userdata); ProcessState owns the reader sub-handle.
	proc->setUserdata(state);
	state->readerHandle = createProcessReader(data, readFd, state.get());
	return proc;
}

} // namespace sprt::dispatch
