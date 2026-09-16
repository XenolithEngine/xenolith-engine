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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTADDRESSWAIT_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTADDRESSWAIT_H_

#include "SPEventFd.h"
#include "../../detail/SPRuntimeDispatchHandleClass.h"

#include <sprt/cxx/thread>

namespace sprt::dispatch {

// Base for the Linux wait-on-address handles: the watched word and the last delivered value live
// in the handle, the backend only decides how the loop learns about a change.
class SPRT_API AddressWaitFdHandle : public AddressWaitHandle {
public:
	virtual ~AddressWaitFdHandle() = default;

	bool init(HandleClass *, AddressWaitInfo &&);

protected:
	// Deliver the current word if it differs from the last delivered value.
	void deliverIfChanged();
};

// io_uring with IORING_OP_FUTEX_WAIT: one-shot wait, rearmed after every completion.
struct AddressWaitFutexSource {
	void cancel() { }
};

class SPRT_API AddressWaitFutexURingHandle : public AddressWaitFdHandle {
public:
	virtual ~AddressWaitFutexURingHandle() = default;

	Status rearm(URingData *, AddressWaitFutexSource *);
	Status disarm(URingData *, AddressWaitFutexSource *);

	void notify(URingData *, AddressWaitFutexSource *, const NotifyData &);
};

// A thread blocked in a futex wait that turns a change into an eventfd write, for loops that can
// not wait on a futex themselves (epoll, io_uring before 6.7). It owns the eventfd, so the fd
// outlives the thread that writes to it.
class SPRT_API AddressWaiter : public Ref {
public:
	virtual ~AddressWaiter();

	bool init();

	int getFd() const { return _fd; }

	bool start(uint32_t *address, uint32_t seen);

	// Returns once the thread has exited. Wakes the word to get there, so a peer sleeping on the
	// same word may see one spurious wakeup.
	void stop();

	bool isRunning() const { return _running; }

protected:
	void run();

	int _fd = -1;
	uint32_t *_address = nullptr;
	uint32_t _seen = 0;
	uint32_t _stopRequested = 0;
	uint32_t _exited = 0;
	bool _running = false;
	sprt::thread _thread;
};

struct AddressWaitEventFdSource {
	int32_t fd = -1;
	epoll_event event;
	uint64_t target = 0;

	void cancel() { }
};

class SPRT_API AddressWaitThreadHandle : public AddressWaitFdHandle {
public:
	virtual ~AddressWaitThreadHandle();

	bool init(HandleClass *, AddressWaitInfo &&);

protected:
	bool startWaiter(AddressWaitEventFdSource *);
	void stopWaiter();

	Rc<AddressWaiter> _waiter;
};

class SPRT_API AddressWaitEPollHandle : public AddressWaitThreadHandle {
public:
	virtual ~AddressWaitEPollHandle() = default;

	Status rearm(EPollData *, AddressWaitEventFdSource *);
	Status disarm(EPollData *, AddressWaitEventFdSource *);

	void notify(EPollData *, AddressWaitEventFdSource *, const NotifyData &);
};

class SPRT_API AddressWaitEventFdURingHandle : public AddressWaitThreadHandle {
public:
	virtual ~AddressWaitEventFdURingHandle() = default;

	Status rearm(URingData *, AddressWaitEventFdSource *);
	Status disarm(URingData *, AddressWaitEventFdSource *);

	void notify(URingData *, AddressWaitEventFdSource *, const NotifyData &);
};

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTADDRESSWAIT_H_ */
