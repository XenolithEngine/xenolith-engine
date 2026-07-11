/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_QUEUE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_QUEUE_H_

#include <sprt/runtime/dispatch/event.h>
#include <sprt/runtime/dispatch/thread.h>
#include <sprt/runtime/dispatch/thread_pool.h>
#include <sprt/runtime/mem/pool_ref.h>

namespace sprt::dispatch {

enum class QueueFlags : uint32_t {
	None,

	// try to protect operations from interrupting with signals
	Protected = 1 << 0,

	// submit all operations as they added, no need to call `submitPending`
	SubmitImmediate = 1 << 1,

	// Engine flags
	// use thread-native backend (used by Looper, do not use this on Queue directly)
	ThreadNative = 1 << 15,
};

SPRT_DEFINE_ENUM_AS_MASK(QueueFlags)

enum class QueueEngine : uint32_t {
	None,
	URing = 1 << 0, // Linux io_uring backend
	EPoll = 1 << 1, // Linux/Android epoll backend
	ALooper = 1 << 2, // Android ALooper backend
	IOCP = 1 << 3, // Windows IOCP
	KQueue = 1 << 4, // BSD/MacOS kqueue
	RunLoop = 1 << 5, // MacOS CFRunLoop
	Wasm = 1 << 6, // WebAssembly futex/condvar reactor (no fds)
	Any = URing | EPoll | ALooper | IOCP | KQueue | RunLoop | Wasm,
};

SPRT_DEFINE_ENUM_AS_MASK(QueueEngine)

enum class WakeupFlags : uint32_t {
	None,
	Graceful = 1 << 0,
	SuspendThreads = 1 << 1, // Looper should suspend worker threads
	ContextDefault = 1 << 2, // Use default wakeup flags, passed into 'run'
	All = Graceful | SuspendThreads | ContextDefault
};

SPRT_DEFINE_ENUM_AS_MASK(WakeupFlags)

struct SPRT_API QueueInfo {
	static constexpr uint32_t DefaultQueueSize = 32;

	QueueFlags flags = QueueFlags::None;
	QueueEngine engineMask = QueueEngine::Any;

	uint32_t submitQueueSize = DefaultQueueSize;
	uint32_t completeQueueSize = 0; // or 0 for default size, based on submitQueueSize
	TimeInterval
			osIdleInterval; // interval, on which internal OS systems will be put to sleep, if idle

	uint32_t externalHandles = 0; // limit for externally opened handles (if applicable)
	uint32_t internalHandles = 0; // limit for internally opened handles (if applicable)
};

// If Graceful flag is set - wait until all operations are completed, and forbid a new ones from running
// If timeout is set, queue will issue a graceful wakeup, but after timeout hard wakeup will be performed
// Only full-async backends (like io_uring) actually use timeout value, on other backends graceful wakeup
// blocks thread until done
struct SPRT_API QueueWakeupInfo {
	WakeupFlags flags = WakeupFlags::None;
	TimeInterval timeout = TimeInterval();
};

/* Simple IO event loop interface
 *
 * Interface is single-threaded, no submission allowed from other threads
 */
class SPRT_API Queue : public PoolObject {
public:
	struct Data; // private platform data

	static Rc<SharedRef<Queue>> create(QueueInfo &&);

	virtual ~Queue();

	bool init(const QueueInfo & = QueueInfo());

	// Uses Handle userdata slot for the Ref
	Rc<TimerHandle> scheduleTimer(TimerInfo &&, Ref * = nullptr);

	// Uses Handle userdata slot for a private data
	// Please, do not try to reset this timer
	Rc<Handle> schedule(TimeInterval, dispatch::Function<void(Handle *, bool)> &&, Ref * = nullptr);

	// Value in completion is PollFlags
	// Uses Handle userdata slot for the Ref
	Rc<PollHandle> listenPollableHandle(NativeHandle, PollFlags, CompletionHandle<PollHandle> &&,
			Ref * = nullptr);

	// Uses Handle userdata slot for a private data
	Rc<PollHandle> listenPollableHandle(NativeHandle, PollFlags,
			dispatch::Function<Status(NativeHandle, PollFlags)> &&, Ref * = nullptr);

	// Spawn a child process running `command` via the system shell. The returned
	// handle represents the process; its completion fires once on exit with the
	// exit code in `value`. ProcessInfo::reader receives merged stdout/stderr.
	Rc<ProcessHandle> spawnProcess(ProcessInfo &&, Ref * = nullptr);

	// Convenience form: `reader` receives output chunks; `onExit` receives the
	// exit code and final Status. Uses the Handle userdata slot for private data.
	Rc<ProcessHandle> spawnProcess(StringView command,
			dispatch::Function<void(StringView)> &&reader,
			dispatch::Function<void(int exitCode, Status)> &&onExit, Ref * = nullptr);

	// Asynchronous file read: streams the file contents to FileReadInfo::reader
	// (each chunk on the looper thread) and fires the completion once when the
	// whole file is read (value = total bytes) or on error.
	Rc<FileHandle> readFile(FileReadInfo &&, Ref * = nullptr);

	// Asynchronous file write: writes FileWriteInfo::data (honoring Append /
	// CreateExclusive) and fires the completion when all bytes are written.
	Rc<FileHandle> writeFile(FileWriteInfo &&, Ref * = nullptr);

	// Convenience forms (path + plain callbacks). Chain further operations onto
	// the returned handle with FileHandle::appendRead / appendWrite.
	Rc<FileHandle> readFile(StringView path, dispatch::Function<void(BytesView)> &&reader,
			dispatch::Function<void(Status)> &&onDone, Ref * = nullptr);
	Rc<FileHandle> writeFile(StringView path, BytesView data, OpenFlags,
			dispatch::Function<void(Status)> &&onDone, Ref * = nullptr);

	// Watch a single file by name for filesystem changes. The completion fires on
	// each change with the observed WatchFlags in `value` and stays armed until
	// the handle is cancelled. Returns nullptr where the backend has no file-watch
	// implementation (currently non-Linux/Android). Uses the Handle userdata slot
	// for the Ref.
	Rc<WatchHandle> watchFile(WatchInfo &&, Ref * = nullptr);

	// Convenience form: `onChange` receives the WatchFlags of each change; return
	// anything other than Status::Ok to cancel the watch. Uses the Handle userdata
	// slot for private data.
	Rc<WatchHandle> watchFile(StringView path, WatchFlags,
			dispatch::Function<Status(WatchFlags)> &&onChange, Ref * = nullptr);

	Rc<ThreadHandle> addThreadHandle();

	// run custom handle
	Status runHandle(Handle *);

	Status submitPending();

	// non-blocking poll
	uint32_t poll();

	// wait until next event, or timeout
	uint32_t wait(TimeInterval = TimeInterval());

	// run for some time or infinite (when no timeout)
	// QueueWakeupFlags can be defined for a wakeup on timer
	// Done when timeout expired
	// Ok on graceful wakeup
	// Suspended on forced wakeup
	// ErrorTimerExpired when graceful wakeup failed on timeout
	//
	// You can set QueueWakeupInfo for timeout wakeup mode
	Status run(TimeInterval = TimeInterval(), QueueWakeupInfo && = QueueWakeupInfo());

	// wakeup queue from `run`
	// If wakeup timeout is set on run() - it will be used, if applicable on queue engine
	// returns ErrorNotImplemented if requested parameters is not supported
	Status wakeup(WakeupFlags = WakeupFlags::ContextDefault);

	void cancel();

	// shutdown any pending async ops in true-async (uring) mode.
	// noop on non-async modes
	void shutdown();

	Data *getData() const { return _data; }

	QueueFlags getFlags() const;

	// Returns actual engine of the queue
	QueueEngine getEngine() const;

	// Schedule task for execution after current event
	// Can be used only from within event processing, returns Declined otherwise
	// Returns Ok if task was scheduled to execution successfully
	Status performNext(Rc<Task> &&);
	Status performNext(dispatch::Function<void()> &&, Ref * = nullptr,
			StringView tag = StringView());

	NativeHandle getHandle() const;

	using PoolObject::PoolObject;

protected:
	Thread::Id _ownerThread;
	Data *_data = nullptr;
};

using QueueRef = SharedRef<Queue>;

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_QUEUE_H_ */
