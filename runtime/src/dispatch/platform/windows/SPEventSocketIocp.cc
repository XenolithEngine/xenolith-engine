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

#include "SPEventSocketIocp.h"
#include "SPEvent-windows.h"
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/winsock.h>

#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/__sprt_errno.h>

// NOTE: included in the SCU after SPEventSocket.cc - the shared error helpers
// (sockErrorToStatus, getSoError) and state machinery are visible here.

namespace sprt::dispatch {

bool SocketPollIocpSource::init(SocketHandle s, PollFlags f) {
	sock = uint64_t(s);
	flags = f;
	return true;
}

void SocketPollIocpSource::cancel(Handle *) {
	// the SOCKET itself is owned by the SocketState, never closed here
	if (event) {
		__sprt_CancelEventCompletion(event, true);
		event = nullptr;
	}
	if (wsaEvent) {
		::WSAEventSelect(SOCKET(sock), wsaEvent, 0);
		::WSACloseEvent(wsaEvent);
		wsaEvent = nullptr;
	}
}

bool SocketPollIocpHandle::init(HandleClass *cl, SocketHandle sock, PollFlags flags,
		CompletionHandle<PollHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	auto source = new (_data) SocketPollIocpSource;
	return source->init(sock, flags);
}

NativeHandle SocketPollIocpHandle::getNativeHandle() const {
	return NativeHandle(
			reinterpret_cast<void *>(reinterpret_cast<const SocketPollIocpSource *>(_data)->sock));
}

bool SocketPollIocpHandle::reset(PollFlags flags) {
	reinterpret_cast<SocketPollIocpSource *>(_data)->flags = flags;
	return Handle::reset();
}

static long makeNetworkEventMask(PollFlags flags) {
	long mask = FD_CLOSE; // peer-close interest is implicit (HungUp)
	if (hasFlag(flags, PollFlags::In)) {
		mask |= FD_READ | FD_ACCEPT;
	}
	if (hasFlag(flags, PollFlags::Out)) {
		mask |= FD_WRITE | FD_CONNECT;
	}
	return mask;
}

Status SocketPollIocpHandle::rearm(IocpData *iocp, SocketPollIocpSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!source->wsaEvent) {
			source->wsaEvent = ::WSACreateEvent();
			if (!source->wsaEvent) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
		}
		if (::WSAEventSelect(SOCKET(source->sock), source->wsaEvent,
					makeNetworkEventMask(source->flags))
				!= 0) {
			return sprt::status::lastErrorToStatus(uint32_t(::WSAGetLastError()));
		}
		if (!source->event) {
			source->event = __sprt_ReportEventAsCompletion(iocp->_port, source->wsaEvent, 1,
					reinterpret_cast<uintptr_t>(this), nullptr);
			if (!source->event) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
		} else {
			if (!__sprt_RestartEventCompletion2(source->event, iocp->_port, source->wsaEvent, 1,
						reinterpret_cast<uintptr_t>(this), nullptr)) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
		}
	}
	return status;
}

Status SocketPollIocpHandle::disarm(IocpData *iocp, SocketPollIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		if (source->event) {
			__sprt_CancelEventCompletion(source->event, true);
			source->event = nullptr;
		}
		if (source->wsaEvent) {
			::WSAEventSelect(SOCKET(source->sock), source->wsaEvent, 0);
		}
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void SocketPollIocpHandle::notify(IocpData *iocp, SocketPollIocpSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// the event-completion wait is one-shot: suspended until rearmed below
	_status = Status::Suspended;

	if (data.result <= 0) {
		cancel();
		sendCompletion(0, _status == Status::Suspended ? Status::Ok : _status);
		return;
	}

	// translate the fired network events; this also resets the WSAEVENT and
	// re-enables the (level-checked) selection for the next wait
	WSANETWORKEVENTS nev;
	for (size_t i = 0; i < sizeof(nev); ++i) { reinterpret_cast<char *>(&nev)[i] = 0; }

	PollFlags pollFlags = PollFlags::None;
	if (::WSAEnumNetworkEvents(SOCKET(source->sock), source->wsaEvent, &nev) == 0) {
		if (nev.lNetworkEvents & (FD_READ | FD_ACCEPT)) {
			pollFlags |= PollFlags::In;
		}
		if (nev.lNetworkEvents & (FD_WRITE | FD_CONNECT)) {
			pollFlags |= PollFlags::Out;
		}
		if (nev.lNetworkEvents & FD_CLOSE) {
			pollFlags |= PollFlags::In | PollFlags::HungUp; // drain remaining data + EOF
		}
		// surface per-event errors (a failed non-blocking connect lands here);
		// the shared state machine reads the specifics via SO_ERROR
		if ((nev.lNetworkEvents & FD_CONNECT) && nev.iErrorCode[FD_CONNECT_BIT] != 0) {
			pollFlags |= PollFlags::Err;
		}
		if ((nev.lNetworkEvents & FD_CLOSE) && nev.iErrorCode[FD_CLOSE_BIT] != 0) {
			pollFlags |= PollFlags::Err;
		}
	} else {
		pollFlags |= PollFlags::Err;
	}

	rearm(iocp, source);

	sendCompletion(toInt(pollFlags), _status == Status::Suspended ? Status::Ok : _status);
}

// --- IOCP-native overlapped stream -------------------------------------------

namespace {

// heavy per-stream strategy data (OVERLAPPEDs must outlive their ops)
struct SocketIocpOps : public Ref {
	OVERLAPPED recvOv;
	OVERLAPPED sendOv;
	uint64_t recvRef = 0; // handle retain while the op is in flight
	uint64_t sendRef = 0;
	bool recvInFlight = false;
	bool sendInFlight = false;
	bool associated = false;
};

struct SocketStreamIocpSource {
	SocketIocpOps *ops = nullptr; // raw; the Rc lives in SocketState::strategy
};

class SocketStreamIocpHandle : public StreamHandle {
public:
	virtual ~SocketStreamIocpHandle() = default;

	bool init(HandleClass *cl) {
		static_assert(sizeof(SocketStreamIocpSource) <= Handle::DataSize
				&& sprt::is_standard_layout<SocketStreamIocpSource>::value);
		if (!Handle::init(cl, CompletionHandle<void>())) {
			return false;
		}
		new (_data) SocketStreamIocpSource;
		return true;
	}

	SocketStreamIocpSource *src() { return reinterpret_cast<SocketStreamIocpSource *>(_data); }
};

static void engageIocpStream(StreamState *state) {
	if (state->terminating || state->finalized) {
		return;
	}
	auto h = static_cast<SocketStreamIocpHandle *>(state->handle);
	auto ops = static_cast<SocketIocpOps *>(state->strategy.get());
	auto iocp = reinterpret_cast<IocpData *>(state->qdata->_platformQueue);
	if (!ops) {
		return;
	}

	if (!ops->associated) {
		if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(uintptr_t(state->sock)),
					iocp->_port, reinterpret_cast<uintptr_t>(h), 0)) {
			state->finalizeSocket(sprt::status::lastErrorToStatus(GetLastError()));
			return;
		}
		ops->associated = true;
	}

	// keep a RECV in flight while an active reader wants data, and after our
	// write side is done (drain-and-discard until the peer's EOF)
	if (!ops->recvInFlight && !state->readEof
			&& ((state->reader && !state->readStopped) || state->shutdownDone)) {
		for (size_t i = 0; i < sizeof(OVERLAPPED); ++i) {
			reinterpret_cast<char *>(&ops->recvOv)[i] = 0;
		}
		WSABUF b;
		b.len = ULONG(SocketChunkSize);
		b.buf = reinterpret_cast<CHAR *>(state->chunkBuf);
		DWORD flags = 0;
		ops->recvInFlight = true;
		ops->recvRef = sprt::retain(h);
		if (::WSARecvFrom(SOCKET(state->sock), &b, 1, nullptr, &flags, nullptr, nullptr,
					&ops->recvOv, nullptr)
						!= 0
				&& ::WSAGetLastError() != ERROR_IO_PENDING
				&& ::WSAGetLastError() != WSA_IO_PENDING) {
			auto e = ::WSAGetLastError();
			ops->recvInFlight = false;
			sprt::release(h, ops->recvRef);
			state->finalizeSocket(sockErrorToStatus(e));
			return;
		}
	}

	if (!ops->sendInFlight && !state->shutdownDone && state->outPos < state->outBuf.size()) {
		for (size_t i = 0; i < sizeof(OVERLAPPED); ++i) {
			reinterpret_cast<char *>(&ops->sendOv)[i] = 0;
		}
		WSABUF b;
		b.len = ULONG(state->outBuf.size() - state->outPos);
		b.buf = reinterpret_cast<CHAR *>(state->outBuf.data() + state->outPos);
		ops->sendInFlight = true;
		state->sendBusy = true;
		ops->sendRef = sprt::retain(h);
		if (::WSASendTo(SOCKET(state->sock), &b, 1, nullptr, 0, nullptr, 0, &ops->sendOv,
					nullptr)
						!= 0
				&& ::WSAGetLastError() != ERROR_IO_PENDING
				&& ::WSAGetLastError() != WSA_IO_PENDING) {
			auto e = ::WSAGetLastError();
			ops->sendInFlight = false;
			state->sendBusy = false;
			sprt::release(h, ops->sendRef);
			state->finalizeSocket(sockErrorToStatus(e));
			return;
		}
	}

	if (!state->sendBusy && state->outPos >= state->outBuf.size() && state->shutdownRequested
			&& !state->shutdownDone) {
		::__sprt_shutdown(SOCKET(state->sock), __SPRT_SHUT_WR);
		state->shutdownDone = true;
	}
	state->checkFinished();
}

} // namespace

void setupIocpSocketStreamClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;
	cl->suspendFn = HandleClass::suspend;
	cl->resumeFn = HandleClass::resume;
	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (!state) {
			return Status::ErrorInvalidArguemnt;
		}
		engageIocpStream(state);
		state->fireConnect(Status::Ok); // native streams are always established
		return HandleClass::run(cl, handle, data);
	};
	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			// closing the socket aborts pending overlapped ops; their error
			// completions still arrive and drop the per-op retains
			const Status final = (st == Status::Done) ? state->closeStatus : st;
			state->fireConnect(isSuccessful(final) ? Status::ErrorCancelled : final);
			state->closeSocket();
			if (state->onClose) {
				auto cb = sprt::move(state->onClose);
				state->onClose = nullptr;
				cb(final);
			}
			state->reader = nullptr;
			state->userRef = nullptr;
			// NOTE: strategy (SocketIocpOps) intentionally stays set until the
			// in-flight completions drain - notify still needs the OVERLAPPEDs
			state->engageFn = nullptr;
		}
		return HandleClass::cancel(cl, handle, data, st);
	};
	cl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
							const NotifyData &n) {
		auto h = static_cast<SocketStreamIocpHandle *>(handle);
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (!state) {
			return;
		}
		auto ops = static_cast<SocketIocpOps *>(state->strategy.get());
		if (!ops) {
			return;
		}

		if (n.ptr == &ops->recvOv) {
			auto ref = ops->recvRef;
			ops->recvInFlight = false;
			const auto ntStatus = uintptr_t(ops->recvOv.Internal);
			const auto bytes = intptr_t(n.result);
			if (!state->terminating && !state->finalized) {
				if (ntStatus != 0) {
					auto s = sockErrorToStatus(WSAECONNRESET);
					state->closeStatus = s;
					state->readEof = true;
					state->finalizeSocket(s);
				} else if (bytes == 0) {
					state->readEof = true;
					if (state->reader && !state->readStopped) {
						state->readStopped = true;
						state->reader(BytesView()); // EOF marker
					}
					if (!state->terminating && !state->finalized) {
						engageIocpStream(state); // may finalize via checkFinished
					}
				} else {
					if (state->reader && !state->readStopped) {
						if (state->reader(BytesView(state->chunkBuf, size_t(bytes)))
								!= Status::Ok) {
							state->readStopped = true;
						}
					}
					if (!state->terminating && !state->finalized) {
						engageIocpStream(state);
					}
				}
			}
			sprt::release(handle, ref);
			return;
		}
		if (n.ptr == &ops->sendOv) {
			auto ref = ops->sendRef;
			ops->sendInFlight = false;
			state->sendBusy = false;
			const auto ntStatus = uintptr_t(ops->sendOv.Internal);
			const auto bytes = intptr_t(n.result);
			if (!state->terminating && !state->finalized) {
				if (ntStatus != 0) {
					auto s = sockErrorToStatus(WSAECONNRESET);
					state->closeStatus = s;
					state->finalizeSocket(s);
				} else {
					state->outPos += size_t(bytes);
					if (state->outPos >= state->outBuf.size()) {
						state->outBuf.clear();
						state->outPos = 0;
					}
					engageIocpStream(state);
				}
			}
			sprt::release(handle, ref);
			return;
		}
	};
}

Rc<StreamHandle> makeSocketStreamIocpHandle(QueueData *q, HandleClass *cl,
		Rc<StreamState> &&state) {
	auto h = Rc<SocketStreamIocpHandle>::create(cl);
	if (!h) {
		return nullptr;
	}
	auto ops = Rc<SocketIocpOps>::alloc();
	h->src()->ops = ops.get();
	state->strategy = ops;
	state->engageFn = &engageIocpStream;
	h->setUserdata(state.get());
	state->handle = h.get();
	return h;
}

} // namespace sprt::dispatch
