/**
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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTSOCKET_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTSOCKET_H_

#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchHandleClass.h"

// Platform-neutral pieces of the stream-socket API (Looper::listenSocket /
// connectSocket). The whole protocol state machine lives here, written once
// against the cross-typed __sprt_socket surface (which already fronts winsock
// on Windows); the only thing a backend contributes is a readiness poll for a
// socket descriptor (QueueData::_socketPoll). Because epoll registers per fd
// and WSAEventSelect allows one event object per socket, every socket owns
// exactly ONE inner poller whose interest set is switched via
// PollHandle::reset(PollFlags) - deferred through QueueData::perform when
// requested from inside a notify cycle. Every readiness handler drains until
// EWOULDBLOCK, so the machine is correct under both level- and edge-triggered
// backends. The heavy state is held in a ref-counted XxxState as the handle's
// userdata with a raw back-pointer - the same ownership shape as FileState.

namespace sprt::dispatch {

struct QueueData;

// One bounded recv per readiness step.
static constexpr size_t SocketChunkSize = 16 * 1'024;

// Shared base for listen/stream socket state. malloc-backed (pool-independent),
// held alive as the handle's userdata. The same state object serves both
// strategies: the readiness-based one (poller + non-blocking syscalls) and the
// native ones (io_uring SQEs / IOCP overlapped), which keep their private data
// in `strategy` and hook state changes through StreamState::engageFn.
struct SPRT_API SocketState : public Ref {
	QueueData *qdata = nullptr;
	Handle *handle = nullptr; // raw back-ptr (the handle owns this state)
	SocketHandle sock = InvalidSocket;
	Rc<PollHandle> poller; // readiness strategy: the single poll for `sock`
	PollFlags interest = PollFlags::None; // what the poller is armed with
	Rc<Ref> userRef; // keeps convenience-callback closures alive
	Rc<Ref> strategy; // native-strategy private data (sub-handles, overlapped)
	bool terminating = false; // teardown started; accept no new work
	bool finalized = false; // handle finalize already requested

	virtual ~SocketState();

	// Create + run the inner poller with the current `interest` (called from the
	// handle's runFn). Returns false when the backend has no _socketPoll or the
	// poller could not be armed.
	bool startPoller(CompletionHandle<PollHandle> &&);

	// Switch the poller's interest set. Safe to call from inside a notify
	// callback: the actual reset is deferred through QueueData::perform then.
	void setInterest(PollFlags);

	// Request the handle to finalize with `st`: cancels it outside the current
	// notify cycle (the FileState::finalizeChannel pattern).
	void finalizeSocket(Status st);

	// Close `sock` via __sprt_closesocket (close() is wrong for winsock sockets).
	void closeSocket();
};

struct SPRT_API ListenState : public SocketState {
	SocketAddress address; // resolved address (actual port after getsockname)
	ListenInfo::AcceptCallback onAccept;
	// the listener's terminal completion, carried from prepareListenState to the
	// strategy factory that builds the handle (Handle::init consumes it)
	ListenInfo::Completion pendingCompletion;
	bool ownsUnixPath = false; // unlink address.path on teardown

	virtual ~ListenState();

	// Readiness event from the poller: drain-accept every pending connection.
	void handleEvents(PollFlags, Status);
};

struct SPRT_API StreamState : public SocketState {
	Function<Status(BytesView)> reader;
	Function<void(Status)> onClose;
	ConnectInfo::Completion connectCompletion; // fired exactly once
	// native-strategy hook: called after reader/outBuf/shutdown state changes
	// to (re)engage the strategy's I/O (submit missing SQEs / overlapped ops);
	// null = readiness strategy (updateInterest)
	void (*engageFn)(StreamState *) = nullptr;
	Vector<uint8_t> outBuf;
	size_t outPos = 0;
	bool connecting = false; // non-blocking connect still in flight
	bool connectFired = false;
	bool readEof = false; // peer closed its write side
	bool readStopped = false; // reader returned non-Ok (or was never set)
	bool shutdownRequested = false; // shutdownWrite() called
	bool shutdownDone = false; // SHUT_WR actually issued
	bool sendBusy = false; // native strategy: an async send op is in flight
	Status closeStatus = Status::Done;

	uint8_t chunkBuf[SocketChunkSize];

	// route a state change into the active strategy
	void engage();

	// Readiness event from the poller.
	void handleEvents(PollFlags, Status);

	// Drain __sprt_recv until EWOULDBLOCK / EOF, feeding `reader`.
	void drainRead();

	// Flush outBuf until EWOULDBLOCK / drained; issues SHUT_WR when requested.
	void flushWrite();

	// Recompute + apply the poller interest from the current machine state.
	void updateInterest();

	// Fire connectCompletion exactly once.
	void fireConnect(Status);

	// Finalize when nothing can happen anymore (both directions finished).
	void checkFinished();
};

// Generic (backend-neutral) handle implementations. The backend-specific part -
// the inner readiness poller - is created through QueueData::_socketPoll.
class SPRT_API SocketListenHandle : public ListenHandle {
public:
	virtual ~SocketListenHandle() = default;

	bool init(HandleClass *, CompletionHandle<void> &&);

	// arm the accept poller (called from runFn)
	Status start();

	ListenState *getState() const { return static_cast<ListenState *>(getUserdata()); }
};

class SPRT_API SocketStreamHandle : public StreamHandle {
public:
	virtual ~SocketStreamHandle() = default;

	bool init(HandleClass *);

	// arm the poller according to the connect/read/write state (from runFn)
	Status start();

	StreamState *getState() const { return static_cast<StreamState *>(getUserdata()); }
};

// HandleClass setup for both socket handle kinds (backend-agnostic, like
// setupInlineFileHandleClass): runFn arms the poller, suspend/resume are plain
// bookkeeping (the poller is a separate reactor-suspendable handle), cancelFn
// tears the socket down and breaks the handle<->closure reference cycles.
void setupSocketHandleClasses(QueueHandleClassInfo *, QueueData *);

// Portable socket-readiness prober for backends without any fd primitive
// (CFRunLoop): a repeating reactor timer probes the socket with a zero-timeout
// __sprt_poll (WSAPoll on Windows) - the same timer-driven strategy the
// file/watch fallbacks use. Registered as QueueData::_socketProbeClass.
class SPRT_API SocketProbeHandle : public PollHandle {
public:
	virtual ~SocketProbeHandle() = default;

	bool init(HandleClass *, SocketHandle, PollFlags, CompletionHandle<PollHandle> &&);

	virtual NativeHandle getNativeHandle() const override;
	virtual bool reset(PollFlags) override;

	// one zero-timeout poll probe (driver-timer tick)
	void probe();

	Status startProbeTimer();
	void stopProbeTimer();
};

void setupSocketProbeClass(QueueHandleClassInfo *, HandleClass *);

// _socketPoll implementation over the probe class (assign in backends without
// an fd primitive): creates a SocketProbeHandle on QueueData::_socketProbeClass.
Rc<PollHandle> makeSocketProbeHandle(QueueData *, SocketHandle, PollFlags,
		CompletionHandle<PollHandle> &&);

// Shared synchronous preparation, exported for the native strategies:
// socket() + non-blocking + bind/listen/getsockname (listen) or non-blocking
// connect() (stream, EINPROGRESS -> state->connecting). On failure the info's
// completion fires and nullptr is returned.
Rc<ListenState> prepareListenState(QueueData *, ListenInfo &&, Ref *);
Rc<StreamState> prepareConnectState(QueueData *, ConnectInfo &&, Ref *);

// Readiness-strategy handle builders over prepared states (the default when a
// backend sets no native _makeSocketListen/_makeSocketStream override).
Rc<ListenHandle> makeSocketListenPollHandle(QueueData *, Rc<ListenState> &&);
Rc<StreamHandle> makeSocketStreamPollHandle(QueueData *, Rc<StreamState> &&);

// Build a fresh StreamState around an existing socket (used for accepted
// connections and by connectSocket).
Rc<StreamState> makeStreamState(QueueData *, SocketHandle, bool connecting);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTSOCKET_H_ */
