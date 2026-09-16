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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTSOCKETIOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTSOCKETIOCP_H_

#include <sprt/runtime/dispatch/handle.h>

#if SPRT_WINDOWS

#include "SPEvent-iocp.h"
#include "../fd/SPEventSocket.h"

// Socket readiness for the IOCP backend (QueueData::_socketPoll). The IOCP
// PollIocpHandle waits on generic waitable kernel HANDLEs, which a winsock
// SOCKET is not - so the readiness set is adapted through
// WSAEventSelect(sock, event, mask): the auto-reset WSAEVENT becomes the
// waitable HANDLE fed into the same __sprt_ReportEventAsCompletion path, and
// WSAEnumNetworkEvents (which also re-enables the selection) translates the
// fired network events into PollFlags on each completion.
//
// WSAEventSelect semantics fit the shared state machine's drain-until-
// EWOULDBLOCK discipline: FD_WRITE is edge-ish (signalled after connect and
// after a WSAEWOULDBLOCK send), FD_READ re-signals while unread data remains,
// FD_CLOSE arrives once.

namespace sprt::dispatch {

struct SocketPollIocpSource {
	uint64_t sock = ~uint64_t(0); // winsock SOCKET (64-bit)
	void *wsaEvent = nullptr; // WSACreateEvent (auto-reset via WSAEnumNetworkEvents)
	void *event = nullptr; // completion registration token (ReportEventAsCompletion)
	PollFlags flags = PollFlags::None;

	bool init(SocketHandle, PollFlags);
	void cancel(Handle *);
};

class SPRT_API SocketPollIocpHandle : public PollHandle {
public:
	virtual ~SocketPollIocpHandle() = default;

	bool init(HandleClass *, SocketHandle, PollFlags, CompletionHandle<PollHandle> &&);

	virtual NativeHandle getNativeHandle() const override;

	virtual bool reset(PollFlags) override;

	Status rearm(IocpData *, SocketPollIocpSource *);
	Status disarm(IocpData *, SocketPollIocpSource *);

	void notify(IocpData *, SocketPollIocpSource *, const NotifyData &);
};

// IOCP-native stream strategy: the SOCKET is associated with the completion
// port (key = the stream handle) and data moves through overlapped
// WSARecvFrom/WSASendTo (WSARecv/WSASend equivalents that ws2_32.def already
// exports). RECV and SEND completions on the one key are told apart by the
// OVERLAPPED pointer forwarded in NotifyData::ptr. Works both on real Windows
// and under wine (needs no wait-completion packets). Listen and connect stay on
// the readiness strategy (AcceptEx/ConnectEx live in the unwrapped mswsock.dll)
// - accepted server-side streams get the native path.
void setupIocpSocketStreamClass(QueueHandleClassInfo *, HandleClass *);

Rc<StreamHandle> makeSocketStreamIocpHandle(QueueData *, HandleClass *, Rc<StreamState> &&);

} // namespace sprt::dispatch

#endif

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENTSOCKETIOCP_H_ */
