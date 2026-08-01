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

#ifndef CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_
#define CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_

#include <sprt/runtime/dispatch/queue.h>
#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchQueueData.h"
#include "../fd/SPEventProcess.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

namespace sprt::dispatch {

static constexpr bool KQUEUE_THREAD_NONBLOCK = false;

struct SPRT_API KQueueData : public PlatformQueueData {
	int _kqueueFd = -1;

	Queue::Vector<struct kevent> _events;

	uint32_t _receivedEvents = 0;
	uint32_t _processedEvents = 0;

	Status update(const struct kevent &ev);
	Status update(SpanView<struct kevent> ev);

	Status runPoll(TimeInterval);
	uint32_t processEvents(RunContext *);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	void cancel();

	KQueueData(QueueRef *, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs);
	~KQueueData();
};

struct SPRT_API KQueueTimerSource {
	TimeInterval timeout;
	TimeInterval interval;
	uint32_t count = 0;
	uint32_t value = 0;
	bool oneshot = false;

	bool init(const TimerInfo &info);
	void cancel();

	uint64_t getNextInterval() const;
};

class SPRT_API KQueueTimerHandle : public TimerHandle {
public:
	virtual ~KQueueTimerHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	Status rearm(KQueueData *, KQueueTimerSource *);
	Status disarm(KQueueData *, KQueueTimerSource *);

	void notify(KQueueData *, KQueueTimerSource *source, const NotifyData &);

	virtual bool reset(TimerInfo &&) override;
};

struct SPRT_API KQueueThreadSource {
	bool init();
	void cancel();
};

class SPRT_API KQueueThreadHandle : public ThreadHandle {
public:
	virtual ~KQueueThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(KQueueData *, KQueueThreadSource *);
	Status disarm(KQueueData *, KQueueThreadSource *);

	void notify(KQueueData *, KQueueThreadSource *, const NotifyData &);

	virtual Status perform(Rc<Task> &&task) override;
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	sprt::mutex _mutex;
};

// Pollable-fd handle over EVFILT_READ (+ EVFILT_WRITE when PollFlags::Out is
// requested — needed by the stream-socket machinery for writability and
// non-blocking connect). kqueue has no generic listenPollableHandle path of its
// own; this provides one so the process reader sub-handle (and any other fd
// watcher) can reuse it.
struct SPRT_API ReadKQueueSource {
	// which filters are actually registered (rearm sets, disarm consumes) —
	// `flags` alone is not enough: reset() rewrites it before the disarm runs
	static constexpr uint16_t ArmedRead = 1 << 0;
	static constexpr uint16_t ArmedWrite = 1 << 1;

	int fd = -1;
	PollFlags flags = PollFlags::None;
	uint16_t armed = 0;

	bool init(int, PollFlags);
	void cancel();
};

class SPRT_API ReadKQueueHandle : public PollHandle {
public:
	virtual ~ReadKQueueHandle() = default;

	bool init(HandleClass *, int fd, PollFlags, CompletionHandle<PollHandle> &&);

	Status rearm(KQueueData *, ReadKQueueSource *);
	Status disarm(KQueueData *, ReadKQueueSource *);

	void notify(KQueueData *, ReadKQueueSource *, const NotifyData &);

	virtual NativeHandle getNativeHandle() const override;
	virtual bool reset(PollFlags) override;
};

// EVFILT_PROC process-exit handle: fires NOTE_EXIT when the child terminates.
struct SPRT_API ProcessKQueueSource {
	int pid = -1;
	bool exited = false; // child reaped via the exit path; cancel() must not kill a recycled pid

	bool init(int);
	void cancel();
};

class SPRT_API ProcessKQueueHandle : public ProcessHandle {
public:
	virtual ~ProcessKQueueHandle() = default;

	bool init(HandleClass *, int pid, CompletionHandle<ProcessHandle> &&);

	Status rearm(KQueueData *, ProcessKQueueSource *);
	Status disarm(KQueueData *, ProcessKQueueSource *);

	void notify(KQueueData *, ProcessKQueueSource *, const NotifyData &);

	virtual NativeHandle getNativeHandle() const override;
};

// Full spawn for the kqueue backend: posix child + EVFILT_READ reader + EVFILT_PROC exit handle.
Rc<ProcessHandle> spawnProcessKQueue(QueueData *data, HandleClass *processClass, ProcessInfo &&info,
		Ref *ref);

// EVFILT_VNODE file-watch: two O_EVTONLY vnode watches under one handle — the
// parent directory (tracks the watched *name*: create/delete/rename-over, and
// the directory's own lifecycle) plus the current file inode (tracks content /
// attribute changes). Both kevents carry `this` as udata; processEvents marshals
// ident/fflags through NotifyData so notify() can tell which vnode fired.
struct SPRT_API KQueueWatchSource {
	int dirFd = -1;
	int fileFd = -1;

	void cancel();
};

class SPRT_API KQueueWatchHandle : public WatchHandle {
public:
	virtual ~KQueueWatchHandle() = default;

	bool init(HandleClass *, StringView path, WatchFlags, CompletionHandle<WatchHandle> &&);

	Status rearm(KQueueData *, KQueueWatchSource *);
	Status disarm(KQueueData *, KQueueWatchSource *);

	void notify(KQueueData *, KQueueWatchSource *, const NotifyData &);

protected:
	// Open the file (if present) and register its vnode kevent; refreshes
	// _exists/_ino from the opened fd.
	Status registerFile(KQueueData *, KQueueWatchSource *);
	// Deregister + close the file vnode watch (the directory watch stays).
	void closeFile(KQueueData *, KQueueWatchSource *);
	// Re-stat the watched name after a directory write and reconcile: reports
	// Created / MovedTo (inode changed) / Deleted and re-targets the file watch.
	void rescan(KQueueData *, KQueueWatchSource *, WatchFlags &pending);

	String _dir; // parent directory, null-terminated for open()
	uint64_t _ino = 0; // inode currently occupying the watched name (0 = none)
	bool _exists = false;
};

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_ */
