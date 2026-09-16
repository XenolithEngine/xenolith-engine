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

#include "SPEventSocketFd.h"
#include "SPEventFd.h"
#include "../uring/SPEvent-uring.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt::dispatch {

namespace {

class UringSocketListenHandle : public ListenHandle {
public:
	virtual ~UringSocketListenHandle() = default;

	bool init(HandleClass *cl, CompletionHandle<void> &&c) {
		static_assert(sizeof(SocketUringSource) <= Handle::DataSize
				&& sprt::is_standard_layout<SocketUringSource>::value);
		if (!Handle::init(cl, sprt::move(c))) {
			return false;
		}
		new (_data) SocketUringSource;
		return true;
	}

	SocketUringSource *src() { return reinterpret_cast<SocketUringSource *>(_data); }
};

class UringSocketStreamHandle : public StreamHandle {
public:
	virtual ~UringSocketStreamHandle() = default;

	bool init(HandleClass *cl) {
		if (!Handle::init(cl, CompletionHandle<void>())) {
			return false;
		}
		new (_data) SocketUringSource;
		return true;
	}

	SocketUringSource *src() { return reinterpret_cast<SocketUringSource *>(_data); }
};

// SEND CQE target: forwards completions back into the owning stream. Its
// userdata pins the stream handle (the cycle is broken in the stream cancelFn
// via SocketState::strategy).
class UringSocketSendHandle : public Handle {
public:
	virtual ~UringSocketSendHandle() = default;

	bool init(HandleClass *cl) { return Handle::init(cl, CompletionHandle<void>()); }

	bool opInFlight = false;
};

static uint64_t uringUserdata(Handle *h) {
	return reinterpret_cast<uintptr_t>(h) | URING_USERDATA_RETAIN_BIT;
}

static URingData *uringOf(SocketState *state) {
	return reinterpret_cast<URingData *>(state->qdata->_platformQueue);
}

// cancel an in-flight op by its exact user_data (the op's terminal -ECANCELED
// CQE then releases the ring retain)
static void uringCancelOp(URingData *uring, Handle *h) {
	uring->pushSqe({IORING_OP_ASYNC_CANCEL}, [&](io_uring_sqe *sqe, uint32_t) {
		sqe->addr = uringUserdata(h);
		sqe->user_data = URING_USERDATA_IGNORED;
	}, URingPushFlags::Submit);
}

static void uringSubmitAccept(ListenState *state, UringSocketListenHandle *h) {
	auto src = h->src();
	if (src->mainOpInFlight || state->terminating || state->finalized) {
		return;
	}
	src->mainOpInFlight = true;
	uringOf(state)->pushSqe({IORING_OP_ACCEPT}, [&](io_uring_sqe *sqe, uint32_t) {
		sqe->fd = int(state->sock);
		sqe->addr = 0; // no peer-address output
		sqe->addr2 = 0;
		sqe->accept_flags = __SPRT_SOCK_NONBLOCK;
		sqe->user_data = uringUserdata(h);
	}, URingPushFlags::Submit);
}

static void uringSubmitRecv(StreamState *state, UringSocketStreamHandle *h) {
	auto src = h->src();
	if (src->mainOpInFlight || state->terminating || state->finalized) {
		return;
	}
	src->mainOpInFlight = true;
	src->connectPoll = false;
	uringOf(state)->pushSqe({IORING_OP_RECV}, [&](io_uring_sqe *sqe, uint32_t) {
		sqe->fd = int(state->sock);
		sqe->addr = reinterpret_cast<uintptr_t>(state->chunkBuf);
		sqe->len = uint32_t(SocketChunkSize);
		sqe->msg_flags = 0;
		sqe->user_data = uringUserdata(h);
	}, URingPushFlags::Submit);
}

static void uringSubmitConnectPoll(StreamState *state, UringSocketStreamHandle *h) {
	auto src = h->src();
	if (src->mainOpInFlight || state->terminating || state->finalized) {
		return;
	}
	src->mainOpInFlight = true;
	src->connectPoll = true;
	uringOf(state)->pushSqe({IORING_OP_POLL_ADD}, [&](io_uring_sqe *sqe, uint32_t) {
		sqe->fd = int(state->sock);
		sqe->poll_events = uint16_t(
				toInt(PollFlags::Out | PollFlags::Err | PollFlags::HungUp));
		sqe->user_data = uringUserdata(h);
	}, URingPushFlags::Submit);
}

static void uringSubmitSend(StreamState *state, UringSocketSendHandle *send) {
	if (send->opInFlight || state->terminating || state->finalized || state->shutdownDone) {
		return;
	}
	if (state->outPos >= state->outBuf.size()) {
		return;
	}
	send->opInFlight = true;
	state->sendBusy = true;
	uringOf(state)->pushSqe({IORING_OP_SEND}, [&](io_uring_sqe *sqe, uint32_t) {
		sqe->fd = int(state->sock);
		sqe->addr = reinterpret_cast<uintptr_t>(state->outBuf.data() + state->outPos);
		sqe->len = uint32_t(state->outBuf.size() - state->outPos);
		sqe->msg_flags = __SPRT_MSG_NOSIGNAL;
		sqe->user_data = uringUserdata(send);
	}, URingPushFlags::Submit);
}

static void engageUringStream(StreamState *state) {
	if (state->terminating || state->finalized) {
		return;
	}
	auto h = static_cast<UringSocketStreamHandle *>(state->handle);
	auto send = static_cast<UringSocketSendHandle *>(state->strategy.get());

	if (state->connecting) {
		uringSubmitConnectPoll(state, h);
		return;
	}
	// keep a RECV in flight while an active reader wants data, AND after our
	// write side is done (drain-and-discard): without a readiness HUP signal the
	// EOF from the peer is the only way to observe the connection ending
	if (!state->readEof
			&& ((state->reader && !state->readStopped) || state->shutdownDone)) {
		uringSubmitRecv(state, h);
	}
	if (send) {
		uringSubmitSend(state, send);
	}
	if (!state->sendBusy && state->outPos >= state->outBuf.size() && state->shutdownRequested
			&& !state->shutdownDone) {
		::__sprt_shutdown(SOCKET(state->sock), __SPRT_SHUT_WR);
		state->shutdownDone = true;
	}
	state->checkFinished();
}

static void uringStreamNotify(UringSocketStreamHandle *h, StreamState *state, intptr_t res) {
	auto src = h->src();
	src->mainOpInFlight = false;
	if (state->terminating || state->finalized) {
		return;
	}

	if (src->connectPoll) {
		src->connectPoll = false;
		auto err = getSoError(state->sock);
		if (err != 0) {
			auto s = sockErrorToStatus(err);
			state->closeStatus = s;
			state->fireConnect(s);
			state->finalizeSocket(s);
			return;
		}
		state->connecting = false;
		state->fireConnect(Status::Ok);
		if (!state->terminating && !state->finalized) {
			engageUringStream(state);
		}
		return;
	}

	// RECV completion
	if (res < 0) {
		if (res == -ECANCELED || res == -EINTR) {
			engageUringStream(state); // resubmit if still wanted
			return;
		}
		auto s = sockErrorToStatus(int(-res));
		state->closeStatus = s;
		state->readEof = true;
		state->finalizeSocket(s);
		return;
	}
	if (res == 0) {
		state->readEof = true;
		if (state->reader && !state->readStopped) {
			state->readStopped = true;
			state->reader(BytesView()); // EOF marker
		}
		if (!state->terminating && !state->finalized) {
			engageUringStream(state); // may finalize via checkFinished
		}
		return;
	}
	if (state->reader && !state->readStopped) {
		if (state->reader(BytesView(state->chunkBuf, size_t(res))) != Status::Ok) {
			state->readStopped = true;
		}
	}
	if (!state->terminating && !state->finalized) {
		engageUringStream(state);
	}
}

static void uringSendNotify(UringSocketSendHandle *send, StreamState *state, intptr_t res) {
	send->opInFlight = false;
	state->sendBusy = false;
	if (state->terminating || state->finalized) {
		return;
	}
	if (res < 0) {
		if (res == -ECANCELED || res == -EINTR || res == -EAGAIN) {
			engageUringStream(state);
			return;
		}
		auto s = sockErrorToStatus(int(-res));
		state->closeStatus = s;
		state->finalizeSocket(s);
		return;
	}
	state->outPos += size_t(res);
	if (state->outPos >= state->outBuf.size()) {
		state->outBuf.clear();
		state->outPos = 0;
	}
	engageUringStream(state);
}

} // namespace

void setupUringSocketClasses(QueueHandleClassInfo *info, HandleClass *listenCl,
		HandleClass *streamCl, HandleClass *sendCl) {
	// listen
	listenCl->info = info;
	listenCl->createFn = HandleClass::create;
	listenCl->destroyFn = HandleClass::destroy;
	listenCl->suspendFn = HandleClass::suspend;
	listenCl->resumeFn = HandleClass::resume;
	listenCl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto h = static_cast<UringSocketListenHandle *>(handle);
		auto state = static_cast<ListenState *>(handle->getUserdata());
		if (!state) {
			return Status::ErrorInvalidArguemnt;
		}
		uringSubmitAccept(state, h);
		return HandleClass::run(cl, handle, data);
	};
	listenCl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
								 Status st) {
		auto h = static_cast<UringSocketListenHandle *>(handle);
		auto state = static_cast<ListenState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			if (h->src()->mainOpInFlight) {
				uringCancelOp(uringOf(state), handle);
			}
			state->closeSocket();
			if (state->ownsUnixPath && !state->address.path.empty()
					&& state->address.path[0] != '@') {
				::__sprt_unlink(state->address.path.data());
			}
			state->onAccept = nullptr;
			state->userRef = nullptr;
			state->strategy = nullptr;
		}
		return HandleClass::cancel(cl, handle, data, st);
	};
	listenCl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
								 const NotifyData &n) {
		auto h = static_cast<UringSocketListenHandle *>(handle);
		auto state = static_cast<ListenState *>(handle->getUserdata());
		if (!state) {
			return;
		}
		h->src()->mainOpInFlight = false;
		if (state->terminating || state->finalized) {
			return;
		}
		if (n.result >= 0) {
			auto stream = state->qdata->makeStreamFromSocket(SocketHandle(n.result), false);
			if (stream && state->qdata->runHandle(stream) == Status::Ok) {
				if (state->onAccept) {
					state->onAccept(Rc<StreamHandle>(stream.get()));
				} else {
					stream->cancel();
				}
			} else if (!stream) {
				::__sprt_closesocket(SOCKET(n.result));
			}
		} else if (n.result == -ECANCELED) {
			return; // teardown in progress
		}
		// transient errors (ECONNABORTED and friends) and successes alike:
		// keep accepting
		uringSubmitAccept(state, h);
	};

	// stream (RECV / connect-POLL target)
	streamCl->info = info;
	streamCl->createFn = HandleClass::create;
	streamCl->destroyFn = HandleClass::destroy;
	streamCl->suspendFn = HandleClass::suspend;
	streamCl->resumeFn = HandleClass::resume;
	streamCl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (!state) {
			return Status::ErrorInvalidArguemnt;
		}
		engageUringStream(state);
		if (!state->connecting) {
			// already-established socket: report the connect completion now
			state->fireConnect(Status::Ok);
		}
		return HandleClass::run(cl, handle, data);
	};
	streamCl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
								 Status st) {
		auto h = static_cast<UringSocketStreamHandle *>(handle);
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			auto uring = uringOf(state);
			if (h->src()->mainOpInFlight) {
				uringCancelOp(uring, handle);
			}
			if (auto send = static_cast<UringSocketSendHandle *>(state->strategy.get())) {
				if (send->opInFlight) {
					uringCancelOp(uring, send);
				}
			}
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
			state->strategy = nullptr;
			state->engageFn = nullptr;
		}
		return HandleClass::cancel(cl, handle, data, st);
	};
	streamCl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
								 const NotifyData &n) {
		auto h = static_cast<UringSocketStreamHandle *>(handle);
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (state) {
			uringStreamNotify(h, state, n.result);
		}
	};

	// send sub-handle (SEND target; forwards into the stream state)
	sendCl->info = info;
	sendCl->createFn = HandleClass::create;
	sendCl->destroyFn = HandleClass::destroy;
	sendCl->suspendFn = HandleClass::suspend;
	sendCl->resumeFn = HandleClass::resume;
	sendCl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		return HandleClass::run(cl, handle, data);
	};
	sendCl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
							   const NotifyData &n) {
		auto send = static_cast<UringSocketSendHandle *>(handle);
		auto main = static_cast<Handle *>(handle->getUserdata());
		if (!main) {
			return;
		}
		auto state = static_cast<StreamState *>(main->getUserdata());
		if (state) {
			uringSendNotify(send, state, n.result);
		}
	};
}

Rc<ListenHandle> makeSocketListenUringHandle(QueueData *q, HandleClass *listenCl,
		Rc<ListenState> &&state) {
	CompletionHandle<void> completion;
	completion = state->pendingCompletion;
	auto h = Rc<UringSocketListenHandle>::create(listenCl, sprt::move(completion));
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	return h;
}

Rc<StreamHandle> makeSocketStreamUringHandle(QueueData *q, HandleClass *streamCl,
		HandleClass *sendCl, Rc<StreamState> &&state) {
	auto h = Rc<UringSocketStreamHandle>::create(streamCl);
	if (!h) {
		return nullptr;
	}
	auto send = Rc<UringSocketSendHandle>::create(sendCl);
	if (!send) {
		return nullptr;
	}
	send->setUserdata(h.get()); // pin the stream while SEND CQEs may arrive
	h->src()->send = send.get();
	state->strategy = send;
	state->engageFn = &engageUringStream;
	h->setUserdata(state.get());
	state->handle = h.get();
	return h;
}

} // namespace sprt::dispatch
