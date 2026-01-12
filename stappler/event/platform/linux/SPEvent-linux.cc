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

#include "SPEvent-linux.h"
#include "SPEventQueue.h"
#include "SPEventThreadHandle.h"
#include "detail/SPEventQueueData.h"
#include "SPMemory.h"

#if LINUX

#include "../fd/SPEventFd.h"
#include "../fd/SPEventTimerFd.h"
#include "../fd/SPEventPollFd.h"
#include "../epoll/SPEvent-epoll.h"
#include "../epoll/SPEventThreadHandle-epoll.h"
#include "../uring/SPEventThreadHandle-uring.h"
#include "../uring/SPEventTimer-uring.h"
#include "../android/SPEventThreadHandle-alooper.h"

#include <signal.h>

namespace STAPPLER_VERSIONIZED stappler::event {

static int SignalsToIntercept[] = {SIGUSR1, SIGUSR2};

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {

	if (hasFlag(info.flags, QueueFlags::ThreadNative)
			&& hasFlag(info.engineMask, QueueEngine::ALooper)
			&& !hasFlag(info.flags, QueueFlags::Protected) && ALooperData::checkSupport()) {

		setupALooperHandleClass<TimerFdALooperHandle, TimerFdSource>(&_info, &_alooperTimerFdClass,
				true);
		setupALooperHandleClass<ThreadALooperHandle, EventFdSource>(&_info, &_alooperThreadClass,
				true);
		setupALooperHandleClass<EventFdALooperHandle, EventFdSource>(&_info, &_alooperEventFdClass,
				true);
		setupALooperHandleClass<SignalFdALooperHandle, SignalFdSource>(&_info,
				&_alooperSignalFdClass, true);
		setupALooperHandleClass<PollFdALooperHandle, PollFdSource>(&_info, &_alooperPollFdClass,
				true);

		auto alooper = new (memory::pool::acquire())
				ALooperData(_info.queue, this, info, SignalsToIntercept);
		if (alooper->_looper != nullptr) {
			_submit = [](void *ptr) { return reinterpret_cast<ALooperData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<ALooperData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<ALooperData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<ALooperData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<ALooperData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<ALooperData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<ALooperData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<TimerFdEPollHandle>::create(&data->_alooperTimerFdClass, move(info));
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<ThreadEPollHandle>::create(&data->_alooperThreadClass);
			};

			_platformQueue = alooper;
			alooper->runInternalHandles();
			_engine = QueueEngine::ALooper;
			return;
		} else {
			alooper->~ALooperData();
		}
	}

	if (hasFlag(info.engineMask, QueueEngine::URing) && URingData::checkSupport()) {
		setupUringHandleClass<TimerFdURingHandle, TimerFdSource>(&_info, &_uringTimerFdClass, true);
		setupUringHandleClass<TimerURingHandle, TimerUringSource>(&_info, &_uringTimerClass, true);
		setupUringHandleClass<ThreadEventFdHandle, EventFdSource>(&_info, &_uringThreadEventFdClass,
				true);
		setupUringHandleClass<ThreadUringHandle, ThreadUringSource>(&_info, &_uringThreadFenceClass,
				true);
		setupUringHandleClass<EventFdURingHandle, EventFdSource>(&_info, &_uringEventFdClass, true);
		setupUringHandleClass<SignalFdURingHandle, SignalFdSource>(&_info, &_uringSignalFdClass,
				true);
		setupUringHandleClass<PollFdURingHandle, PollFdSource>(&_info, &_uringPollFdClass, true);

		auto uring = new (memory::pool::acquire())
				URingData(_info.queue, this, info, SignalsToIntercept);
		if (uring->_ringFd >= 0) {
			_submit = [](void *ptr) { return reinterpret_cast<URingData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<URingData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<URingData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<URingData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<URingData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<URingData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<URingData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto uring = reinterpret_cast<URingData *>(ptr);
				auto data = reinterpret_cast<Queue::Data *>(d);
				if (!info.resetable
						&& ((hasFlag(uring->_uflags, URingFlags::TimerMultishotSupported)
									&& info.count == TimerInfo::Infinite)
								|| info.count == 1)) {
					return Rc<TimerURingHandle>::create(&data->_uringTimerClass, move(info));
				} else {
					return Rc<TimerFdURingHandle>::create(&data->_uringTimerFdClass, move(info));
				}
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				if constexpr (URING_THREAD_USE_FUTEX_HANDLE) {
#ifdef SP_URING_THREAD_FENCE_HANDLE
					auto uring = reinterpret_cast<URingData *>(ptr);
					if (hasFlag(uring->_uflags, URingFlags::FutexSupported)) {
						return Rc<ThreadUringHandle>::create(&data->_uringThreadFenceClass);
					} else {
						return Rc<ThreadEventFdHandle>::create(&data->_uringThreadEventFdClass);
					}
#else
					return Rc<ThreadEventFdHandle>::create(&data->_uringThreadEventFdClass);
#endif
				} else {
					return Rc<ThreadEventFdHandle>::create(&data->_uringThreadEventFdClass);
				}
			};

			_listenHandle = [](QueueData *d, void *ptr, NativeHandle handle, PollFlags flags,
									CompletionHandle<PollHandle> &&cb) -> Rc<PollHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<PollFdURingHandle>::create(&data->_uringPollFdClass, handle, flags,
						sp::move(cb));
			};

			_platformQueue = uring;
			uring->runInternalHandles();
			_engine = QueueEngine::URing;
			return;
		} else {
			uring->~URingData();
		}
	}

	if (hasFlag(info.engineMask, QueueEngine::EPoll)) {

		setupEpollHandleClass<TimerFdEPollHandle, TimerFdSource>(&_info, &_epollTimerFdClass, true);
		setupEpollHandleClass<ThreadEPollHandle, EventFdSource>(&_info, &_epollThreadClass, true);
		setupEpollHandleClass<EventFdEPollHandle, EventFdSource>(&_info, &_epollEventFdClass, true);
		setupEpollHandleClass<SignalFdEPollHandle, SignalFdSource>(&_info, &_epollSignalFdClass,
				true);
		setupEpollHandleClass<PollFdEPollHandle, PollFdSource>(&_info, &_epollPollFdClass, true);

		auto epoll = new (memory::pool::acquire())
				EPollData(_info.queue, this, info, SignalsToIntercept);
		if (epoll->_epollFd >= 0) {
			_submit = [](void *ptr) { return reinterpret_cast<EPollData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<EPollData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<EPollData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<EPollData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<EPollData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<EPollData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<EPollData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<TimerFdEPollHandle>::create(&data->_epollTimerFdClass, move(info));
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<ThreadEPollHandle>::create(&data->_epollThreadClass);
			};
			_listenHandle = [](QueueData *d, void *ptr, NativeHandle handle, PollFlags flags,
									CompletionHandle<PollHandle> &&cb) -> Rc<PollHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<PollFdEPollHandle>::create(&data->_epollPollFdClass, handle, flags,
						sp::move(cb));
			};

			_platformQueue = epoll;
			epoll->runInternalHandles();
			_engine = QueueEngine::EPoll;
		} else {
			epoll->~EPollData();
		}
	}
}

} // namespace stappler::event

namespace STAPPLER_VERSIONIZED stappler::event::platform {

Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	// Just create the queue, Linux has no specifics
	info.engineMask = QueueEngine::URing;
	return Queue::create(move(info));
}

} // namespace stappler::event::platform

#endif
