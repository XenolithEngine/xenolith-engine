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
#include "detail/SPEventQueueData.h"
#include <sys/winapi.h>

namespace sprt::dispatch {

struct SPRT_API IocpData : public PlatformQueueData {
	static constexpr uint32_t InternalFlag = 1 << 29;
	static constexpr uint32_t CancelFlag = 1 << 30;

	void *_port = nullptr;

	mem_pool::Vector<overlapped_entry> _events;

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

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_IOCP_H_ */
