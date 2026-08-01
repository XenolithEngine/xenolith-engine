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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTSOCKETFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTSOCKETFD_H_

#include "SPEventSocket.h"

// io_uring-native strategy for the stream-socket API: instead of a readiness
// poll + non-blocking syscalls, the operations themselves are submitted as SQEs
// (IORING_OP_ACCEPT / RECV / SEND; non-blocking connect resolution via a
// one-shot POLL_ADD). The state machine data stays in the shared
// ListenState/StreamState; concurrent RECV and SEND ops are disambiguated by
// targeting different handles - RECV/POLL CQEs land on the stream handle,
// SEND CQEs on a private send sub-handle (its class only forwards the CQE back
// into the state). All SQEs are submitted with URING_USERDATA_RETAIN_BIT, so
// the ring pins the handle until the terminal CQE - a cancelled handle can
// never dangle under an in-flight op.

namespace sprt::dispatch {

struct URingData;

// per-stream flags kept in the main handle's Source slot
struct SocketUringSource {
	Handle *send = nullptr; // raw ptr; the Rc lives in SocketState::strategy
	bool mainOpInFlight = false; // ACCEPT (listen) / RECV or connect-POLL (stream)
	bool connectPoll = false; // the in-flight main op is the connect POLL_ADD
};

void setupUringSocketClasses(QueueHandleClassInfo *, HandleClass *listenCl,
		HandleClass *streamCl, HandleClass *sendCl);

Rc<ListenHandle> makeSocketListenUringHandle(QueueData *, HandleClass *listenCl,
		Rc<ListenState> &&);
Rc<StreamHandle> makeSocketStreamUringHandle(QueueData *, HandleClass *streamCl,
		HandleClass *sendCl, Rc<StreamState> &&);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTSOCKETFD_H_ */
