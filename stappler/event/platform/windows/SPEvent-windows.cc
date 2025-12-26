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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_WINDOWS_CC_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_WINDOWS_CC_

#include "SPEvent-windows.h"
#include "SPEventTimerIocp.h"
#include "SPEventThreadIocp.h"
#include "SPEventPollIocp.h"
#include "platform/windows/SPEvent-iocp.h"

namespace STAPPLER_VERSIONIZED stappler::event {

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {
	if (hasFlag(info.engineMask, QueueEngine::IOCP)) {
		setupIocpHandleClass<TimerIocpHandle, TimerIocpSource>(&_info, &_iocpTimerClass, true);
		setupIocpHandleClass<ThreadIocpHandle, ThreadIocpSource>(&_info, &_iocpThreadClass, true);
		setupIocpHandleClass<PollIocpHandle, PollIocpSource>(&_info, &_iocpPollClass, true);

		auto iocp = new (memory::pool::acquire()) IocpData(_info.queue, this, info);
		if (iocp->_port) {
			_submit = [](void *ptr) { return reinterpret_cast<IocpData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<IocpData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<IocpData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<IocpData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<IocpData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<IocpData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<IocpData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<TimerIocpHandle>::create(&data->_iocpTimerClass, move(info));
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<ThreadIocpHandle>::create(&data->_iocpThreadClass);
			};

			_listenHandle = [](QueueData *d, void *ptr, NativeHandle handle, PollFlags flags,
									CompletionHandle<PollHandle> &&cb) -> Rc<PollHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<PollIocpHandle>::create(&data->_iocpPollClass, handle, flags,
						sp::move(cb));
			};

			_platformQueue = iocp;
			_engine = QueueEngine::IOCP;
		} else {
			iocp->~IocpData();
		}
	}
}

} // namespace stappler::event

namespace STAPPLER_VERSIONIZED stappler::event::platform {

Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	// Just create the queue, Linux has no specifics
	return Queue::create(move(info));
}

} // namespace stappler::event::platform

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENT_WINDOWS_CC_ */
