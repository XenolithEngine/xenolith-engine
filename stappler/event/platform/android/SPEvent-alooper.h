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

#ifndef CORE_EVENT_PLATFORM_ANDROID_SPEVENT_ALOOPER_H_
#define CORE_EVENT_PLATFORM_ANDROID_SPEVENT_ALOOPER_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "detail/SPEventQueueData.h"

#include "../fd/SPEventSignalFd.h"
#include "../fd/SPEventEventFd.h"

#include <sprt/c/sys/__sprt_alooper.h>

namespace STAPPLER_VERSIONIZED stappler::event {

struct SP_PUBLIC ALooperData : public PlatformQueueData {
	static bool checkSupport();

	__SPRT_ID(ALooper) *_looper = nullptr;

	Rc<EventFdHandle> _eventFd;

	Status add(int fd, int events, Handle *);
	Status remove(int fd);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	void runInternalHandles();

	void cancel();

	ALooperData(QueueRef *, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs);
	~ALooperData();
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_ANDROID_SPEVENT_ALOOPER_H_ */
