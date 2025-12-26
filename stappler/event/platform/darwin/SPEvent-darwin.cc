/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPEvent-darwin.h"
#include "SPEvent-kqueue.h"
#include "SPEvent-runloop.h"

namespace STAPPLER_VERSIONIZED stappler::event {

static int SignalsToIntercept[] = {SIGUSR1, SIGUSR2};

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {
	if (hasFlag(info.engineMask, QueueEngine::KQueue)) {
		// init kqueue classes

		auto queue = new (memory::pool::acquire())
				KQueueData(_info.queue, this, info, SignalsToIntercept);
		if (queue->_kqueueFd >= 0) {
			_submit = [](void *ptr) { return reinterpret_cast<KQueueData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<KQueueData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<KQueueData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<KQueueData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<KQueueData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<KQueueData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<KQueueData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<KQueueTimerHandle>::create(&data->_kqueueTimerClass, move(info));
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<KQueueThreadHandle>::create(&data->_kqueueThreadClass);
			};

			setupKQueueHandleClass<KQueueTimerHandle, KQueueTimerSource>(&_info, &_kqueueTimerClass,
					true);
			setupKQueueHandleClass<KQueueThreadHandle, KQueueThreadSource>(&_info,
					&_kqueueThreadClass, true);

			_platformQueue = queue;
			_engine = QueueEngine::KQueue;
			return;
		} else {
			queue->~KQueueData();
		}
	}

	if (hasFlag(info.engineMask, QueueEngine::RunLoop)) {
		// init runloop classes

		auto runloop = new (memory::pool::acquire()) RunLoopData(_info.queue, this, info);
		if (runloop->_runLoop) {
			_submit = [](void *ptr) { return reinterpret_cast<RunLoopData *>(ptr)->submit(); };
			_poll = [](void *ptr) { return reinterpret_cast<RunLoopData *>(ptr)->poll(); };
			_wait = [](void *ptr, TimeInterval ival) {
				return reinterpret_cast<RunLoopData *>(ptr)->wait(ival);
			};
			_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&info) {
				return reinterpret_cast<RunLoopData *>(ptr)->run(ival, info.flags, info.timeout);
			};
			_wakeup = [](void *ptr, WakeupFlags flags) {
				return reinterpret_cast<RunLoopData *>(ptr)->wakeup(flags);
			};
			_cancel = [](void *ptr) { reinterpret_cast<RunLoopData *>(ptr)->cancel(); };
			_destroy = [](void *ptr) { delete reinterpret_cast<RunLoopData *>(ptr); };

			_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<RunLoopTimerHandle>::create(&data->_runloopTimerClass, move(info));
			};

			_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
				auto data = reinterpret_cast<Queue::Data *>(d);
				return Rc<RunLoopThreadHandle>::create(&data->_runloopThreadClass);
			};

			setupRunLoopHandleClass<RunLoopTimerHandle, RunLoopTimerSource>(&_info,
					&_runloopTimerClass, true);
			setupRunLoopHandleClass<RunLoopThreadHandle, RunLoopThreadSource>(&_info,
					&_runloopThreadClass, true);

			_platformQueue = runloop;
			_engine = QueueEngine::RunLoop;
			return;
		} else {
			runloop->~RunLoopData();
		}
	}
}

} // namespace stappler::event

namespace STAPPLER_VERSIONIZED stappler::event::platform {

Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	// Allow only CFRunLoop
	info.engineMask = QueueEngine::RunLoop;

	return Queue::create(move(info));
}

} // namespace stappler::event::platform
