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

#ifndef RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_
#define RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/new.h>
#include <sprt/runtime/mutex.h>
#include <sprt/runtime/atomic.h>
#include <sprt/runtime/waitable_address.h>
#include <sprt/runtime/mem/set.h>
#include <sprt/runtime/mem/map.h>

#include <Windows.h>
#include <process.h>
#include <setjmp.h>

#include "private/SPRTSpecific.h" // IWYU pragma: keep
#include "private/SPRTAtomics.h" // IWYU pragma: keep

#include "sched_win.h"

/* Windows pthread implementation notes

This implementation is based on WinAPI functions from scratch and is not a reworking of
pthread4w, w32-pthread or any other library. The conventions for those implementations
may not apply to this one.

For full operation of this implementation, a Windows version of at least Windows 11 (Build 22000)
or Windows Server 2022 (Build 20348) is required. The main dependency is QueueUserAPC2 for a full
implementation of pthread_cancel.

Primitives based on WaitOnAddress (qmutex and waitable_address) are used for synchronization.

= Priorities =

Unlike other implementations, for thread prioritization to work you need:

1. Disable priority inheritance (pthread_attr_setinheritsched(t, PTHREAD_EXPLICIT_SCHED).
Otherwise, the thread will always inherit the priority of the creator thread.

2. Switch priority mode (pthread_attr_setschedpolicy(t, SCHED_RR)). In the default mode
SCHED_OTHER, priorities are not applied because the way they work is not described in POSIX.

3. Assign priority to a thread using pthread_attr_setschedparam

Similar rules apply to pthread_setschedparam, priorities are applied only in SCHED_RR mode and
only if priority inheritance is disabled.

For Windows, the standard thread switching algorithm is defined as SCHED_OTHER, and the
algorithm with a specific priority is defined as SCHED_RR, which reflects the description of
the thread prioritization mechanism in MSDN.

Priorities are determined by values ​​from 0 to 6, which directly maps to the values ​​in the
Windows API:

0 - THREAD_PRIORITY_IDLE
1 - THREAD_PRIORITY_LOWEST
2 - THREAD_PRIORITY_BELOW_NORMAL
3 - THREAD_PRIORITY_NORMAL (default)
4 - THREAD_PRIORITY_ABOVE_NORMAL
5 - THREAD_PRIORITY_HIGHEST
6 - THREAD_PRIORITY_TIME_CRITICAL

= Exit and cancel =

To force a thread to terminate, SPRT will exit to the thread's initial function using jongjmp.
This method, if C++ exceptions are enabled by the compiler, allows SPRT to correctly call all
destructors and unwind the stack.

pthread_cancel in asynchronous mode uses QueueUserAPC2 + QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC.
This function cannot interrupt execution of code in user space, but it can interrupt any system
call and correctly call the destructors of the current stack frame.

pthread_testcancel is called whenever possible in all functions described in POSIX as
cancellation points and implemented in SPRT.

There is no way to gracefully terminate a thread created outside of pthread (that is, not
pthread_create) using pthread_exit. Such a thread, however, will be terminated using
_endthreadex. In this case, data leaks, deadlocks and race conditions are possible, since
destructors will not be called.

= Thread attaching =

You can use the pthread API with threads not created with pthread_create, with a number of
restrictions:

* You can get pthread_t using pthread_self()
* Calling pthread_exit leads to data leaks and other problems (see above)
* pthread_join not possible


= Mutexes =

Mutexes implemented with WaitOnAddress. Keynotes:

* Function unavailability is reported by the pthread_mutexattr_* functions by setting errno to ENOTSUP.
pthread_mutex_init cannot fail due to invalid attributes.

* pthread_mutexattr_get* always reports the actual attribute value

* Inter-process mutexes (PTHREAD_PROCESS_SHARED) not available.

* Priority options not available, only PTHREAD_PRIO_NONE is supported as protocol. You can set and get
prioceiling, but it's noop.

* Only mutex type is PTHREAD_MUTEX_RECURSIVE. PTHREAD_MUTEX_DEFAULT maps to PTHREAD_MUTEX_RECURSIVE

* PTHREAD_MUTEX_ROBUST is supported with POSIX semantics. WaitOnAddress is not robust
by default, so, PTHREAD_MUTEX_STALLED is default for mutex.

* pthread_mutex_timedwait works against CLOCK_REALTIME, see notes on clocks for condward below


= Condvars =

* Implemented on top of WaitOnAddress

* Inter-process condvars (PTHREAD_PROCESS_SHARED) not available.

* Only the same mutex can be used for simultaneous waits.

pthread_condattr_setclock restrictions:
* CLOCK_REALTIME by default. Also available are CLOCK_REALTIME_COARSE, CLOCK_MONOTONIC, CLOCK_MONOTONIC_COARSE
* The clock parameter is used only to calculate the timeout for WaitOnAddress, not as a direct kernel limit
* The timeout for WaitOnAddress is expressed in milliseconds. The minimum wait time is 1 millisecond.


= Spinlock =

* No specific, just works


= RW Locks =

* Writer starvation should be partially prevented (gatekeeper algorithm)

* Robust (locks will be safely released when thread dies)


= Barrier =

* Experimental algorithm, based on in/out tickeds with WaitOnAddress (needs confirmation in high loads)


= Keys =

* pthread_key_delete when specific values exists is safe

* PTHREAD_KEYS_MAX is UINT32_t MAX - 1

* PTHREAD_DESTRUCTOR_ITERATIONS is 4


= Other =

* pthread_atfork noop
* pthread_getconcurrency noop
* pthread_setconcurrency noop
* pthread_getcpuclockid returns CLOCK_REALTIME

*/

namespace sprt {

typedef __SPRT_ID(cpu_set_t) cpu_set_t;

enum class MutexAttrFlags : uint16_t {
	None = 0,
	Robust = 1 << 0,
	OwnerDied = 1 << 1,
	NotRecoverable = 1 << 2,
};

SPRT_DEFINE_ENUM_AS_MASK(MutexAttrFlags)

struct pthread_mutexattr_t {
	MutexAttrFlags flags = MutexAttrFlags::None;
	int16_t prio = 0;
};

static_assert(sizeof(pthread_mutexattr_t) <= sizeof(__sprt_pthread_mutexattr_t));

struct pthread_mutex_t {
	static constexpr uint64_t StateThreadMask = static_cast<uint64_t>(Max<uint32_t>);
	static constexpr uint64_t StateWaiters = 1ULL << 33;
	static constexpr uint64_t StateNotRecoverable = 1ULL << 35;

	pthread_mutexattr_t attr;
	uint32_t counter = 0;
	uint64_t value = 0; // ThreadId + flags

	int lock(DWORD);
	int unlock();
	int try_lock();

	// unlock robust mutex when owner dies
	void force_unlock();

	bool is_owned_by_this_thread();
};

static_assert(sizeof(pthread_mutex_t) == sizeof(__sprt_pthread_mutex_t));

enum class CondAttrFlags : uint32_t {
	None = 0,
	ClockRealtimeCoarse,
	ClockMonotomic,
	ClockMonotomicCoarse,
	ClockProcessCpu,
	ClockThreadCpu,
	ClockMask = 0b111,
};

SPRT_DEFINE_ENUM_AS_MASK(CondAttrFlags)

struct pthread_condattr_t {
	CondAttrFlags flags = CondAttrFlags::None;
};

static_assert(sizeof(pthread_condattr_t) <= sizeof(__sprt_pthread_condattr_t));

struct pthread_cond_t {
	CondAttrFlags flags = CondAttrFlags::None;
	uint32_t counter = 0;
	uint32_t value = 0;
	uint32_t previous = 0;
	uint64_t mutexid = 0;

	int wait(pthread_mutex_t *mutex, DWORD timeout);

	int signal();
	int broadcast();
};

static_assert(sizeof(pthread_cond_t) == sizeof(__sprt_pthread_cond_t));

enum class RwlockAttrFlags : uint32_t {
	None = 0,
};

SPRT_DEFINE_ENUM_AS_MASK(RwlockAttrFlags)

struct pthread_rwlockattr_t {
	RwlockAttrFlags flags = RwlockAttrFlags::None;
};

static_assert(sizeof(pthread_rwlockattr_t) <= sizeof(__sprt_pthread_rwlockattr_t));

struct pthread_rwlock_t {
	static constexpr uint32_t ReadLock = 1;
	static constexpr uint32_t WriteLock = 2;
	static constexpr uint32_t Waiters = 4;

	uint32_t value = 0;
	uint32_t counter = 0;

	// Prewent writer starwation by protect writing lock process by simple mutex;
	// rdlock will perfrom lock/unlock on this mutex before readlock process,
	// so, if there are writer - read locking process will wait on this mutex until
	// writer unlocks it
	uint32_t mutex = 0;
	uint32_t padding = 0;

	int rdlock(DWORD timeout);
	int tryrdlock();

	int wrlock(DWORD timeout);
	int trywrlock();

	int unlock();

	void force_unlock(bool isReadLock);
};

static_assert(sizeof(pthread_rwlock_t) == sizeof(__sprt_pthread_rwlock_t));


enum class BarrierAttrFlags : uint32_t {
	None = 0,
};

SPRT_DEFINE_ENUM_AS_MASK(BarrierAttrFlags)

struct pthread_barrierattr_t {
	BarrierAttrFlags flags = BarrierAttrFlags::None;
};

static_assert(sizeof(pthread_barrierattr_t) <= sizeof(__sprt_pthread_barrierattr_t));

struct pthread_barrier_t {
	static constexpr uint32_t ValueMask = 0x7FFF'FFFF;
	static constexpr uint32_t PassFlag = 0x8000'0000;

	uint32_t nthreads = 0; // number of threads in single exit batch
	uint32_t nIn = 0; // number of input threads
	uint32_t nOut = 0; // number of outgoing threads
	uint32_t generations = 0; // counter to drop nOut only single time per batch

	int wait();
};

static_assert(sizeof(pthread_barrier_t) == sizeof(__sprt_pthread_barrier_t));

using pthread_spinlock_t = uint32_t;

static_assert(sizeof(pthread_spinlock_t) == sizeof(__sprt_pthread_spinlock_t));

using pthread_key_t = uint32_t;

struct pthread_key_data {
	void (*destructor)(void *);
	uint32_t refcount = 1;
};

struct pthread_key_specific {
	pthread_key_data *data = nullptr;
	const void *value = nullptr;
};

enum class ThreadAttrFlags : uint16_t {
	None = 0,
	Detached = 1 << 0,
	ApplyPrio = 1 << 1,
	ExplicitSched = 1 << 2,
	Unmanaged = 1 << 3,
	CancelabilityDisabled = 1 << 4,
	CancelabilityAsync = 1 << 5,
	CancelRequested = 1 << 6,

	JoinRequested = 1 << 7,
};

SPRT_DEFINE_ENUM_AS_MASK(ThreadAttrFlags)

struct pthread_attr_t {
	uint32_t stackSize = 0;
	ThreadAttrFlags attr = ThreadAttrFlags::None;
	int16_t prio = __SPRT_SCHED_PRIO_DEFAULT;
};

static_assert(sizeof(pthread_attr_t) == sizeof(__sprt_pthread_attr_t));

struct _pthread_t {
	// set by new thread when it successfully initializes it's data
	// pthread_create will wait for it
	static constexpr waitable_address::value_type StateInternalInit = 1;

	// set by pthread_create after it's completely initialize thread
	// thread will wait for it before run user's callback
	static constexpr waitable_address::value_type StateExternalInit = 2;

	static constexpr waitable_address::value_type StateCancelling = 4;

	// set when thread ready to be joined or detached to free it's resources
	static constexpr waitable_address::value_type StateFinalized = 6;

	_pthread_t *next = nullptr;
	memory::pool_t *threadMemPool = nullptr;
	memory::set<pthread_mutex_t *> *threadRobustMutexes = nullptr;
	memory::set<pthread_rwlock_t *> *threadWrLocks = nullptr;
	memory::map<pthread_rwlock_t *, uint32_t> *threadRdLocks = nullptr;
	memory::map<uint32_t, pthread_key_specific> *threadKeyStorage = nullptr;

	HANDLE handle = nullptr;
	pthread_attr_t attr;
	qmutex mutex;
	waitable_address state;

	unsigned int threadId = 0;
	unsigned int result = 0;
	void *(*cb)(void *) = nullptr;
	void *arg = nullptr;

	ULONG_PTR lowStack = 0;
	ULONG_PTR highStack = 0;

	jmp_buf jmpToRunthread;

	static _pthread_t *self();

	// current precise timestamp in milliseconds
	static ULONGLONG now();

	void registerThread();

	bool has_wrlock(const pthread_rwlock_t *) const;
	bool has_rdlock(const pthread_rwlock_t *) const;
};

using pthread_t = _pthread_t *;

using pthread_once_t = uint32_t;

static_assert(sizeof(pthread_once_t) == sizeof(__sprt_pthread_once_t));

static constexpr uint32_t MUTEX_LOCK = 1;
static constexpr uint32_t MUTEX_WAIT = 2;

static inline bool mutexLock(uint32_t *value, DWORD *timeout) {
	ULONGLONG now = (*timeout != INFINITE) ? _pthread_t::now() : 0, next = 0;

	// try to mark futex to own it
	uint32_t waitvalue = MUTEX_LOCK | MUTEX_WAIT;
	uint32_t c = atomicFetchOr(value, MUTEX_LOCK);
	if ((c & MUTEX_LOCK) != 0) {
		// prev value already has LOCK flag, wait
		do {
			if (*timeout == 0) {
				return false;
			}

			// if wait flag is set already or we still locked
			if ((c & MUTEX_WAIT) != 0 || (atomicFetchOr(value, MUTEX_WAIT) & MUTEX_LOCK) != 0) {
				// wait for unlock, if futex has both WAIT_VALUE and LOCK_VALUE
				if (WaitOnAddress(value, &waitvalue, sizeof(uint32_t), *timeout)) {
					if (GetLastError() == ERROR_TIMEOUT) {
						return false;
					}
				}

				if (*timeout != INFINITE) {
					next = _pthread_t::now();
					*timeout -= min((next - now), *timeout);
				}
				now = next;
			}
			// check if lock still in place by fetching value and set all flags
		} while (((c = atomicFetchOr(value, waitvalue)) & MUTEX_LOCK) != 0);
	}

	return true;
}

static inline bool mutexTryLock(uint32_t *value) {
	return (atomicFetchOr(value, MUTEX_LOCK) & MUTEX_LOCK) == 0;
}

static inline void mutexUnlock(uint32_t *value) {
	if ((atomicExchange(value, uint32_t(0)) & MUTEX_WAIT) != 0) {
		// WAIT was set, we need to signal
		WakeByAddressSingle(value);
	}
}

static inline void mutexUnlockAll(uint32_t *value) {
	if ((atomicExchange(value, uint32_t(0)) & MUTEX_WAIT) != 0) {
		// WAIT was set, we need to signal
		WakeByAddressAll(value);
	}
}

} // namespace sprt

#endif // RUNTIME_SRC_LIBC_PLATFORM_WINDOWS_PTHREAD_WIN_H_
