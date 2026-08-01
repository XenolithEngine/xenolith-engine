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

#ifndef CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_
#define CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_

#include <sprt/runtime/dispatch/queue.h>

#include "SPRuntimeDispatchHandleClass.h"

struct _linux_timespec {
	sprt::int64_t tv_sec; /* seconds */
	sprt::int64_t tv_nsec; /* nanoseconds */
};

struct _linux_itimerspec {
	_linux_timespec it_interval; /* timer period */
	_linux_timespec it_value; /* timer expiration */
};

namespace sprt::dispatch {

struct PlatformQueueData;
struct FileState;
struct ListenState;
struct StreamState;

// PerformEngine can be used for resumable nested 'perform' variants
// Action, that performed within engine, can safely call Queue::run, that also can cause 'perform'
struct SPRT_API PerformEngine : public sprt::detail::AllocPool {
	struct Block : sprt::detail::AllocPool {
		Block *next = nullptr;
		Rc<Task> task;
		dispatch::Function<void()> fn;
		Rc<Ref> ref;
		StringView tag;
	};

	uint32_t _performEnabled = 0;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_tmpPool = nullptr;
	Block *_pendingBlocksFront = nullptr;
	Block *_pendingBlocksTail = nullptr;
	Block *_emptyBlocks = nullptr;
	uint32_t _blocksAllocated = 0;
	uint32_t _blocksWaiting = 0;
	uint32_t _blocksFree = 0;

	Status perform(Rc<Task> &&);
	Status perform(dispatch::Function<void()> &&, Ref * = nullptr, StringView tag = StringView());

	uint32_t runAllTasks(memory::pool_t *);

	void cleanup();

	PerformEngine(memory::pool_t *);
};

struct SPRT_API QueueData : public PerformEngine {
	using SubmitCallback = Status (*)(void *);
	using PollCallback = uint32_t (*)(void *);
	using WaitCallback = uint32_t (*)(void *, TimeInterval ival);
	using RunCallback = Status (*)(void *, TimeInterval ival, QueueWakeupInfo &&info);
	using WakeupCallback = Status (*)(void *, WakeupFlags);
	using CancelCallback = void (*)(void *);
	using DestroyCallback = void (*)(void *);

	using TimerCallback = Rc<TimerHandle> (*)(QueueData *, void *, TimerInfo &&info);
	using ThreadCallback = Rc<ThreadHandle> (*)(QueueData *, void *);
	using ListenHandleCallback = Rc<PollHandle> (*)(QueueData *, void *, NativeHandle, PollFlags,
			CompletionHandle<PollHandle> &&);
	using SpawnProcessCallback = Rc<ProcessHandle> (*)(QueueData *, void *, ProcessInfo &&, Ref *);
	// Per-backend factory: turns a prepared FileState into the right FileHandle
	// (io_uring native, IOCP/overlapped native, or the portable inline handle).
	using MakeFileHandleCallback = Rc<FileHandle> (*)(QueueData *, void *, Rc<FileState> &&);
	using WatchFileCallback = Rc<WatchHandle> (*)(QueueData *, void *, WatchInfo &&, Ref *);
	// Per-backend readiness poll for a socket descriptor - the single primitive
	// the shared stream-socket state machine (SPEventSocket) needs from a
	// backend for the readiness-based strategy. Takes the socket directly (not
	// NativeHandle) so the 64-bit winsock SOCKET never truncates. Backends
	// without one (wasm) leave it null and listenSocket/connectSocket return
	// nullptr.
	using SocketPollCallback = Rc<PollHandle> (*)(QueueData *, void *, SocketHandle, PollFlags,
			CompletionHandle<PollHandle> &&);
	// Optional native-strategy overrides (io_uring ACCEPT/RECV/SEND SQEs, IOCP
	// overlapped I/O): turn a prepared socket state into the strategy's handle.
	// When null, the shared readiness-based handles (driven by _socketPoll) are
	// used. _makeSocketStream also wraps every ACCEPTED connection.
	using MakeSocketListenCallback = Rc<ListenHandle> (*)(QueueData *, void *, Rc<ListenState> &&);
	using MakeSocketStreamCallback = Rc<StreamHandle> (*)(QueueData *, void *, Rc<StreamState> &&);

	QueueHandleClassInfo _info;
	QueueFlags _flags = QueueFlags::None;
	QueueEngine _engine = QueueEngine::None;

	bool _running = true;

	Queue::Set<Rc<Handle>> _pendingHandles;
	Queue::Set<Rc<Handle>> _suspendableHandles;

	PlatformQueueData *_platformQueue = nullptr;

	SubmitCallback _submit = nullptr;
	PollCallback _poll = nullptr;
	WaitCallback _wait = nullptr;
	RunCallback _run = nullptr;
	WakeupCallback _wakeup = nullptr;
	CancelCallback _cancel = nullptr;
	CancelCallback _shutdown = nullptr;
	DestroyCallback _destroy = nullptr;
	TimerCallback _timer = nullptr;
	ThreadCallback _thread = nullptr;
	ListenHandleCallback _listenHandle = nullptr;
	SpawnProcessCallback _spawnProcess = nullptr;
	MakeFileHandleCallback _makeFileHandle = nullptr;
	WatchFileCallback _watchFile = nullptr;
	SocketPollCallback _socketPoll = nullptr;
	MakeSocketListenCallback _makeSocketListen = nullptr;
	MakeSocketStreamCallback _makeSocketStream = nullptr;

	// Backend-agnostic socket handle classes (like the inotify watch class):
	// set up via setupSocketHandleClasses() by every backend that assigns
	// _socketPoll; untouched otherwise. _socketProbeClass is the portable
	// timer + poll(2) readiness prober for backends without an fd primitive
	// (CFRunLoop) - set up via setupSocketProbeClass().
	HandleClass _socketListenClass;
	HandleClass _socketStreamClass;
	HandleClass _socketProbeClass;

	// Wrap an (accepted) socket into the active stream strategy: native when
	// _makeSocketStream is set, the shared readiness-based handle otherwise.
	Rc<StreamHandle> makeStreamFromSocket(SocketHandle, bool connecting);

	Thread::Id _threadId;

	NativeHandle _handle = NativeHandle(0);

	bool isValid() const { return _platformQueue != nullptr; }

	bool isRunning() const { return _running; }
	bool isWithinNotify() const { return _performEnabled > 0; }

	// returns number of operations suspended
	// Suspended handle pointers written to buffer provided
	uint32_t suspendAll(Handle **);

	// returns number of operations suspended
	uint32_t resumeAll();

	Status runHandle(Handle *);

	void cancel(Handle *);

	void cleanup();

	void notify(Handle *, const NotifyData &);

	void notifySuspendedAll();

	Status submit();

	uint32_t poll();
	uint32_t wait(TimeInterval ival);

	Status run(TimeInterval ival, QueueWakeupInfo &&info);
	Status wakeup(WakeupFlags flags);

	void cancel();
	void shutdown();

	Rc<TimerHandle> scheduleTimer(TimerInfo &&);
	Rc<PollHandle> listenHandle(NativeHandle, PollFlags, CompletionHandle<PollHandle> &&);
	Rc<ProcessHandle> spawnProcess(ProcessInfo &&, Ref *);
	Rc<FileHandle> readFile(FileReadInfo &&, Ref *);
	Rc<FileHandle> writeFile(FileWriteInfo &&, Ref *);
	Rc<WatchHandle> watchFile(WatchInfo &&, Ref *);
	// Implemented in SPEventSocket.cc; nullptr when _socketPoll is not wired.
	Rc<ListenHandle> listenSocket(ListenInfo &&, Ref *);
	Rc<StreamHandle> connectSocket(ConnectInfo &&, Ref *);
	Rc<ThreadHandle> addThreadHandle();

	~QueueData();

	QueueData(QueueRef *, QueueFlags);
};

struct SPRT_API PlatformQueueData;

struct alignas(32) PlatformQueueData : public sprt::detail::AllocPool {
	struct alignas(32) RunContext {
		enum CallMode {
			Poll,
			Wait,
			Run,
		};

		enum State {
			Running,
			Signaled, // next control function should send CFRunLoopStop
			Stopping, // context should wait until all handles will become suspended or wakeup timeout expires
			Stopped, // CFRunLoopStop was sent
		};

		CallMode mode = Poll;
		State state = Running;
		PlatformQueueData *queue = nullptr;

		WakeupFlags runWakeupFlags = WakeupFlags::None;
		uint32_t wakeupCounter = 0; // how many handles we need suspend for a graceful wakeup
		Status wakeupStatus = Status::Suspended;
		TimeInterval wakeupTimeout;

		RunContext *prev = nullptr;
		uint32_t nevents = 0;

		_linux_timespec wakeupTimespec;

		HashSet<Handle *> _awaitingHandles;

		// Context-local snapshot of one poll batch, used by fd-based backends. A
		// handler dispatched through notify() can re-enter the loop (a nested run()
		// with its own RunContext) and reuse the queue's shared event buffer, so
		// each invocation must iterate its own copy here. Each handle is pinned
		// (retain id) up front while still valid, so an earlier event's callback
		// freeing a later batch entry's handle cannot cause a use-after-free. The
		// buffer is reused across a context's run-loop iterations.
		struct EventSlot {
			Handle *handle = nullptr;
			uint32_t flags = 0;
			uint64_t refId = 0;
		};
		Vector<EventSlot> eventBatch;
	};

	using StopContextCallback = void (*)(RunContext *);
	using SuspendCallback = Status (*)(RunContext *, uint32_t nhandles, Handle **);
	using SuspendedCallback = void (*)(RunContext *);

	QueueRef *_queue = nullptr;
	Queue::Data *_data = nullptr;
	QueueFlags _flags = QueueFlags::None;
	RunContext *_runContext = nullptr;

	StopContextCallback _stopContext = nullptr;
	SuspendCallback _suspend = nullptr;
	SuspendedCallback _suspended = nullptr;

	Status suspendHandles(RunContext *);
	Status stopContext(RunContext *, WakeupFlags, bool external);
	Status stopRootContext(WakeupFlags, bool external);

	void pushContext(RunContext *, RunContext::CallMode);
	void popContext(RunContext *);

	bool hasContext(void *);

	void handleSuspendedAll();

	PlatformQueueData(QueueRef *, Queue::Data *data, QueueFlags);
};

} // namespace sprt::dispatch

#endif /* CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_ */
