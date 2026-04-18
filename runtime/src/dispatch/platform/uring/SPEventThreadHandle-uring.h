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

#ifndef CORE_EVENT_PLATFORM_URING_SPEVENTTHREADHANDLE_H_
#define CORE_EVENT_PLATFORM_URING_SPEVENTTHREADHANDLE_H_

#include <sprt/runtime/dispatch/handle.h>
#include "../fd/SPEventEventFd.h"
#include "SPEvent-uring.h"

#if SPRT_LINUX || SPRT_ANDROID

namespace sprt::dispatch {

/* Thread handle implementations for io_uring.
 *
 * Classic implementation uses eventfd + mutex with write + potential futex syscalls per task
 * Modern implementation uses IORING_OP_FUTEX_WAIT with only futex syscall per task
 */

// For eventfd-based haandle - do not block on mutex, wait until nonblocking capture
// Can increase overall performance, with the cost of stable context switch time
static constexpr bool URING_THREAD_NONBLOCK = false;

// Try to observe context switch timer (in nanoseconds) to determine Handle performance
static constexpr bool URING_THREAD_DEBUG_SWITCH_TIMER = false;

// eventfd - based handler
class SPRT_API ThreadEventFdHandle : public ThreadHandle {
public:
	virtual ~ThreadEventFdHandle() = default;

	bool init(HandleClass *);

	Status rearmBuffers(EventFdSource *);

	Status rearm(URingData *, EventFdSource *, bool updateBuffers = false);
	Status disarm(URingData *, EventFdSource *);

	void notify(URingData *, EventFdSource *, const NotifyData &);

	virtual Status perform(Rc<Task> &&task) override;
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	uint16_t _bufferGroup = 0;
	sprt::mutex _mutex;
};

} // namespace sprt::dispatch

#endif

#endif /* CORE_EVENT_PLATFORM_URING_SPEVENTTHREADHANDLE_H_ */
