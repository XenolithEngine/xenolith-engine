/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_CORE_PTHREAD_IMPL_H_
#define RUNTIME_CORE_PTHREAD_IMPL_H_

#include <sprt/runtime/dispatch/entry.h>
#include <sprt/runtime/dispatch/thread_info.h>

#include <sprt/runtime/thread/qbarrier.h>
#include <sprt/runtime/thread/qcondvar.h>
#include <sprt/runtime/thread/qrwlock.h>

#include "pthread_thread_t.h"

namespace sprt::_thread {

using spinlock_t = uint32_t;

using once_t = uint32_t;

enum class MutexAttrFlags : uint16_t {
	None = 0,
	Robust = 1 << 0,
	OwnerDied = 1 << 1,
	NotRecoverable = 1 << 2,
	Shared = 1 << 3,

	TypeErrorCheck = 1 << 12,
	TypeRecursive = 2 << 12,
	TypeNMask = TypeErrorCheck | TypeRecursive,

	PrioProtect = 1 << 14,
	PrioInherit = 2 << 14,
	PrioMask = PrioProtect | PrioInherit,
};

SPRT_DEFINE_ENUM_AS_MASK(MutexAttrFlags)

struct alignas(COMMON_ALIGNMENT) mutexattr_t {
	MutexAttrFlags flags = MutexAttrFlags::None;
	int16_t prio = 0;
};

struct alignas(COMMON_ALIGNMENT) mutex_t {
	static bool isValid(const mutexattr_t &);

	mutexattr_t attr;

	union {
		__sprt_sprt_qlock_t qlock;
		__rmutex_data rlock = {{0}};
	};

	int lock(timeout_t);
	int unlock();
	int try_lock();

	int consistent();

	// unlock robust mutex when owner dies
	void force_unlock();

	bool is_locked() const;
	bool has_ownedship() const;
	bool is_owned_by_this_thread() const;
};

enum class CondAttrFlags : uint32_t {
	None = 0,
	Shared = 1,
	ClockRealtime = 2,
};

SPRT_DEFINE_ENUM_AS_MASK(CondAttrFlags)

struct alignas(COMMON_ALIGNMENT) condattr_t {
	CondAttrFlags flags = CondAttrFlags::None;
};

struct alignas(COMMON_ALIGNMENT) cond_t {
	__qcondvar_data data;

	int wait(mutex_t *mutex, timeout_t timeout);

	int signal();
	int broadcast();
};

enum class RwlockAttrFlags : uint32_t {
	None = 0,
	Shared = 1,
	ClockRealtime = 2,
};

SPRT_DEFINE_ENUM_AS_MASK(RwlockAttrFlags)

struct alignas(COMMON_ALIGNMENT) rwlockattr_t {
	RwlockAttrFlags flags = RwlockAttrFlags::None;
};

struct alignas(COMMON_ALIGNMENT) rwlock_t {
	static constexpr uint32_t ReadLock = 1;
	static constexpr uint32_t WriteLock = 2;
	static constexpr uint32_t Waiters = 4;

	RwlockAttrFlags flags = RwlockAttrFlags::None;
	qrwlock_base::rwlock_data data;

	rwlock_t();

	int rdlock(timeout_t timeout);
	int tryrdlock();

	int wrlock(timeout_t timeout);
	int trywrlock();

	int unlock();

	void force_unlock(bool isReadLock);
};

enum class BarrierAttrFlags : uint32_t {
	None = 0,
	Shared = 1,
};

SPRT_DEFINE_ENUM_AS_MASK(BarrierAttrFlags)

struct alignas(COMMON_ALIGNMENT) barrierattr_t {
	BarrierAttrFlags flags = BarrierAttrFlags::None;
};

struct alignas(COMMON_ALIGNMENT) barrier_t {
	static constexpr uint32_t ValueMask = 0x7FFF'FFFF;
	static constexpr uint32_t PassFlag = 0x8000'0000;

	qbarrier_base::barrier_data data;
	BarrierAttrFlags flags = BarrierAttrFlags::None;

	int wait();
};


// thread_local slot to store current thread info
// It should not have thread-local constructor to be accessable before any initializers
struct __thread_slot {
	thread_t *thread = nullptr;
};

// application-wide thread pool, with linked list for active and free threads
struct __thread_pool {
	static __thread_pool *get();

	int fifoPrioMin = 0;
	int fifoPrioMax = 0;
	int rrPrioMin = 0;
	int rrPrioMax = 0;
	int otherPrioMin = 0;
	int otherPrioMax = 0;
	atomic<int> concurency = 0;
	atomic<uint32_t> nkeys = 1;
	sprt::__malloc_unordered_map<key_t, __key_data> keys;

	// Thread locators are system-specific type with 64-bit width max
	sprt::__malloc_unordered_map<uint64_t, thread_t *> activeThreads;

	// thread_t *active = nullptr;
	thread_t *free = nullptr;

	thread_t main;

	attr_t defaultAttr;

	qmutex mutex;

	__thread_pool();
	~__thread_pool();

	bool isPrioValid(int policy, int prio);
	bool isPrioValid(ThreadAttrFlags policy, int prio);
};

// Initial function
SPRT_UNUSED static SPRT_RUNTHREAD_CALLCONV thread_result_t __runthead(void *arg);

SPRT_UNUSED static void __attachNativeThread(thread_t *thread, void *handle, uint64_t id,
		unique_lock<qmutex> &lock);

// Remove thread from active list and deallocate it's resources atomically,
// Some memory can be preserved to use for future threads;
// Caller can protect thread's data with externalLock
SPRT_UNUSED static void __detachAndDeallocateThread(thread_t *thread,
		unique_lock<qmutex> *externalLock);

SPRT_UNUSED static void __deallocateThread(thread_t *handle);

SPRT_UNUSED static int __pthread_join(thread_t *thread, void **ret, timeout_t timeout,
		bool tryjoin);

namespace native {

SPRT_UNUSED static uint64_t __getNativeThreadId();

SPRT_UNUSED static void __registerForDestruction(void (*)(void));

SPRT_UNUSED static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr,
		__thread_pool *pool);

// Setup thread with actual native thread's attributes;
// Also, this will replace temporary native handle with the permanent one
SPRT_UNUSED static void __initNativeHandle(thread_t *thread);

SPRT_UNUSED static void __closeNativeHandle(void *handle);

SPRT_UNUSED static bool __isNativeHandleValid(thread_t *thread);

// Try to exit unmanaged thread
SPRT_UNUSED static void __exitNativeThread(void *ret);

// Try to cancel managed thread with async cancel policy
SPRT_UNUSED static int __cancelThreadAsync(thread_t *thread);

// Apply current priority parameters for OS thread, both managed and unmanaged
SPRT_UNUSED static int __applyThreadPrio(thread_t *thread, int32_t dprio);

// Native attributes and capabilities validation
// Should return false if capability is not supported on target system
// Note that common POSIX sanity checks done in pthread_* function itself,
// you should not repeat it, only native capabilities should be tested
SPRT_UNUSED static bool validate_attr_setguardsize(size_t);
SPRT_UNUSED static bool validate_attr_setstacksize(size_t);
SPRT_UNUSED static bool validate_attr_setstack(void *, size_t);
SPRT_UNUSED static bool validate_attr_setschedpolicy(int);
SPRT_UNUSED static bool validate_attr_setschedparam(int);
SPRT_UNUSED static bool validate_attr_setinheritsched(int);
SPRT_UNUSED static bool validate_mutexattr_setprioceiling(int);
SPRT_UNUSED static bool validate_mutexattr_setprotocol(int);
SPRT_UNUSED static bool validate_mutexattr_setpshared(int);
SPRT_UNUSED static bool validate_condattr_setclock(int);
SPRT_UNUSED static bool validate_condattr_setpshared(int);
SPRT_UNUSED static bool validate_rwlockattr_setpshared(int);
SPRT_UNUSED static bool validate_barrierattr_setpshared(int);

} // namespace native

} // namespace sprt::_thread

#endif // RUNTIME_CORE_PTHREAD_IMPL_H_
