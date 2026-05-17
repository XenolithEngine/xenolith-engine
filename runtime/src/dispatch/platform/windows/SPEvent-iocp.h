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

#include <sprt/runtime/dispatch/queue.h>
#include "../../detail/SPRuntimeDispatchQueueData.h"
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/cxx/bitset>

namespace sprt::dispatch {

static inline void timeToFileTime(LARGE_INTEGER &ftime, TimeInterval ival) {
	// ticks in 100ns
	auto ticks = ival.toMicros() * 10;
	ftime.QuadPart = -ticks;
}

struct SPRT_API IocpData : public PlatformQueueData {
	static constexpr DWORD InternalFlag = 1 << 29;
	static constexpr DWORD CancelFlag = 1 << 30;

	HANDLE _port = nullptr;
	bool _hasCompletionPackage = true;

	Queue::Vector<HANDLE> _winHandles;
	Queue::HashMap<HANDLE, Handle *> _queueHandles;

	Queue::Vector<OVERLAPPED_ENTRY> _events;

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

	void addWaitableObject(HANDLE, Handle *);
	void removeWaitableObject(HANDLE);

	IocpData(QueueRef *, Queue::Data *data, const QueueInfo &info);
	~IocpData();
};

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_IOCP_H_ */
