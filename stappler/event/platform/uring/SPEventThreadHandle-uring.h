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

#include "SPEventThreadHandle.h"
#include "../fd/SPEventEventFd.h"
#include "SPEvent-uring.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::event {

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

// For now, classic implementation gives more stable context switch time, so, ThreadUringHandle is disabled
static constexpr bool URING_THREAD_USE_FUTEX_HANDLE = false;

// Thread dispatch control with a single futex (requires FUTEX2 syscalls)
// On client side - works normally, but always calls futex_wake on unlock, to signal server thread
// Server thread uses uring instead of syscalls to do async lockless processing
class SP_PUBLIC FutexImpl {
public:
	static constexpr uint32_t CLIENT_MASK = 0x01;
	static constexpr uint32_t SERVER_MASK = 0x02;
	static constexpr uint32_t FULL_MASK = CLIENT_MASK | SERVER_MASK;

	void client_lock();
	bool client_try_lock();
	bool server_try_lock();

	bool server_unlock();
	void client_unlock();

	volatile uint32_t *getAddr() { return &_futex; }

	uint32_t load();

private:
	// 0 means unlocked
	// 1 means locked, no waiters
	// 2 means locked, there are waiters in lock()
	volatile uint32_t _futex = 0;
};

struct ThreadUringSource {
	bool failsafe = false;
	FutexImpl futex;
	thread::Thread::Id thisThread;

	_linux_timespec interval;

	bool init(TimeInterval);
	void cancel();
};

// IORING_OP_FUTEX_WAIT - based handler
class SP_PUBLIC ThreadUringHandle : public ThreadHandle {
public:
	// Most of the events handled via futex notification,
	// but in very rare cases uring-based futex events can be stalled due ABA problem.
	// In this case, failsafe timer can unstall process.
	// Discovered ABA problem, for now, observed only with debugger-issues signals
	// and should not occur in production environment.
	static constexpr TimeInterval FAILSAFE_TIMER_INTERVAL = TimeInterval::microseconds(500);

	virtual ~ThreadUringHandle() = default;

	bool init(HandleClass *);

	Status rearm(URingData *, ThreadUringSource *, bool unlock = false, bool init = true);
	Status disarm(URingData *, ThreadUringSource *);

	void notify(URingData *, ThreadUringSource *source, const NotifyData &);

	virtual Status perform(Rc<thread::Task> &&task) override;
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	void rearmFailsafe(URingData *, ThreadUringSource *);
};

// eventfd - based handler
class SP_PUBLIC ThreadEventFdHandle : public ThreadHandle {
public:
	virtual ~ThreadEventFdHandle() = default;

	bool init(HandleClass *);

	Status rearmBuffers(EventFdSource *);

	Status rearm(URingData *, EventFdSource *, bool updateBuffers = false);
	Status disarm(URingData *, EventFdSource *);

	void notify(URingData *, EventFdSource *, const NotifyData &);

	virtual Status perform(Rc<thread::Task> &&task) override;
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	uint16_t _bufferGroup = 0;
	std::mutex _mutex;
};

} // namespace stappler::event

#endif

#endif /* CORE_EVENT_PLATFORM_URING_SPEVENTTHREADHANDLE_H_ */
