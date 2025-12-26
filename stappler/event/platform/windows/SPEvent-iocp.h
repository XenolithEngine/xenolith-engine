/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_IOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_IOCP_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "detail/SPEventQueueData.h"
#include <winnt.h>

namespace STAPPLER_VERSIONIZED stappler::event {

// ReportEventAsCompletion
//  - associates Event with I/O Completion Port and requests a completion packet when signalled
//  - parameters order modelled after PostQueuedCompletionStatus
//  - parameters: hIOCP - handle to I/O Completion Port
//                hEvent - handle to Event, Semaphore, Thread or Process
//                       - NOTE: Mutex is not supported, it makes no sense in this context
//                dwNumberOfBytesTransferred - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//                dwCompletionKey - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//                lpOverlapped - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//  - returns: I/O Packet HANDLE for the association
//             NULL on failure, call GetLastError () for details
//              - ERROR_INVALID_PARAMETER -
//              - ERROR_INVALID_HANDLE - provided hEvent is not supported by this API
//              - otherwise internal HRESULT is forwarded
//  - call CloseHandle to free the returned I/O Packet HANDLE when no longer needed
//
SP_PUBLIC _Ret_maybenull_ HANDLE ReportEventAsCompletion(HANDLE hIOCP, HANDLE hEvent,
		DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

// RestartEventCompletion
//  - use to wait again, after the event completion was consumed by GetQueuedCompletionStatus(Ex)
//  - parameters: hPacket - is HANDLE returned by 'ReportEventAsCompletion'
//                hIOCP - handle to I/O Completion Port
//                hEvent - handle to the Event object
//                oEntry - pointer to data provided back by GetQueuedCompletionStatus(Ex)
//  - returns: TRUE on success
//             FALSE on failure, call GetLastError () for details (TBD)
//
SP_PUBLIC BOOL RestartEventCompletion(HANDLE hPacket, HANDLE hIOCP, HANDLE hEvent,
		const OVERLAPPED_ENTRY *oEntry);
SP_PUBLIC BOOL RestartEventCompletion(HANDLE hPacket, HANDLE hIOCP, HANDLE hEvent,
		DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

// CancelEventCompletion
//  - stops the Event from completing into the I/O Completion Port
//  - call CloseHandle to free the I/O Packet HANDLE when no longer needed
//  - parameters: hPacket - is HANDLE returned by 'ReportEventAsCompletion'
//                cancel - if TRUE, if already signalled, the completion packet is removed from queue
//  - returns: TRUE on success
//             FALSE on failure, call GetLastError () for details (TBD)
//
BOOL CancelEventCompletion(HANDLE hPacket, BOOL cancel);

struct SP_PUBLIC IocpData : public PlatformQueueData {
	static constexpr DWORD InternalFlag = 1 << 29;
	static constexpr DWORD CancelFlag = 1 << 30;

	HANDLE _port = nullptr;

	mem_pool::Vector<OVERLAPPED_ENTRY> _events;

	uint32_t _receivedEvents = 0;
	uint32_t _processedEvents = 0;

	void pollMessages();

	Status runPoll(TimeInterval, bool infinite = false);
	uint32_t processEvents(RunContext *ctx);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	Status suspendHandles();

	void cancel();

	IocpData(QueueRef *, Queue::Data *data, const QueueInfo &info);
	~IocpData();
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_IOCP_H_ */
