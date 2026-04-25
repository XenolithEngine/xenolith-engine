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

#ifndef RUNTIME_SRC_LIBC_PTHREAD_PTHREAD_NATIVE_H_
#define RUNTIME_SRC_LIBC_PTHREAD_PTHREAD_NATIVE_H_

#include "private/SPRTThread.h"

#include <sprt/cxx/bitset>

#if SPRT_WINDOWS
#define SPRT_RUNTHREAD_CALLCONV __stdcall
#else
#define SPRT_RUNTHREAD_CALLCONV
#endif

namespace sprt::_thread {

static constexpr size_t MaxActiveThreads = 8'192;

// thread_local slot to store current thread info
struct __thread_slot {
	__sprt_pid_t tid = 0;
	thread_t *thread = nullptr;

	__thread_slot();
	~__thread_slot();
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
	sprt::__malloc_unordered_map<uint32_t, thread_t *> activeThreads;

	// thread_t *active = nullptr;
	thread_t *free = nullptr;

	attr_t defaultAttr;

	qmutex mutex;

	__thread_pool();
	~__thread_pool();

	bool isPrioValid(int policy, int prio);
	bool isPrioValid(ThreadAttrFlags policy, int prio);
};

// Initial function
SPRT_UNUSED static SPRT_RUNTHREAD_CALLCONV thread_result_t __runthead(void *arg);

SPRT_UNUSED static void __attachNativeThread(thread_t *thread, void *handle);

// Remove thread from active list and deallocate it's resources atomically,
// Some memory can be preserved to use for future threads;
// Caller can protect thread's data with externalLock
SPRT_UNUSED static void __detachAndDeallocateThread(thread_t *thread,
		unique_lock<qmutex> *externalLock);

SPRT_UNUSED static int __pthread_join(thread_t *thread, void **ret, timeout_t timeout,
		bool tryjoin);

namespace native {

SPRT_UNUSED static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr);

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

#endif // RUNTIME_SRC_LIBC_PTHREAD_PTHREAD_NATIVE_H_
