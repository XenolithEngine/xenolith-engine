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

#ifndef CORE_EVENT_PLATFORM_NUTTX_SPEVENT_NUTTX_H_
#define CORE_EVENT_PLATFORM_NUTTX_SPEVENT_NUTTX_H_

// NuttX dispatch platform — M3 minimum.
//
// NuttX has poll() but no epoll/uring/eventfd. The real poll-based reactor
// (built on poll() + a timer heap, like the darwin kqueue reactor) arrives in
// M5 alongside the threads/Looper tests that exercise it. For M3 we only need
// Queue::Data to be a complete type so libsprt.a links; single-threaded M3/M4
// builds never construct a Queue and so never reach any of these methods.
//
// The struct carries no NuttX-specific handle classes yet — when M5 lands,
// _nuttxPollClass and friends get added here the way _epollPollClass does in
// platform/linux/SPEvent-linux.h.

#include <sprt/runtime/dispatch/queue.h>
#include "../../detail/SPRuntimeDispatchHandleClass.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

namespace sprt::dispatch {

struct SPRT_API Queue::Data : public QueueData {
	Data(QueueRef *q, const QueueInfo &info);
};

} // namespace sprt::dispatch

#endif  // CORE_EVENT_PLATFORM_NUTTX_SPEVENT_NUTTX_H_
