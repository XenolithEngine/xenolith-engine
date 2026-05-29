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

#ifndef RUNTIME_CORE_PTHREAD_THREAD_T_H_
#define RUNTIME_CORE_PTHREAD_THREAD_T_H_

#include "pthread_common.h"

#include <sprt/runtime/thread/qtimeline.h>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/unordered_set>
#include <sprt/cxx/mutex>
#include <sprt/c/__sprt_setjmp.h>

namespace sprt::_thread {

enum class ThreadAttrFlags : uint16_t {
	None = 0,
	Detached = 1 << 0,
	ExplicitSched = 1 << 2,
	Unmanaged = 1 << 3,
	CancelabilityDisabled = 1 << 4,
	CancelabilityAsync = 1 << 5,
	CancelAsyncSupported = 1 << 6,
	CancelRequested = 1 << 7,

	JoinRequested = 1 << 8,
	GuardSizeCustomized = 1 << 9,
	StackPointerCustomized = 1 << 10,
	StackSizeCustomized = 1 << 11,
	CreateSuspended = 1 << 12,

	PrioRR = 1 << 14,
	PrioFifo = 2 << 14,
	PrioMask = PrioRR | PrioFifo,
};

SPRT_DEFINE_ENUM_AS_MASK(ThreadAttrFlags)

struct alignas(COMMON_ALIGNMENT) attr_t {
	ThreadAttrFlags attr = ThreadAttrFlags::None;
	int16_t prio = 0;
	uint32_t stackSize = 0;

	uint32_t guardSize = 0;
	uint32_t padding = 0;

	void *stack = 0;
};

struct thread_t : thread_base_t {
	// set by new thread when it successfully initializes it's data
	// pthread_create will wait for it
	static constexpr qtimeline::value_type StateInternalInit = 1;

	// set by pthread_create after it's completely initialize thread
	// thread will wait for it before run user's callback
	static constexpr qtimeline::value_type StateExternalInit = 2;

	static constexpr qtimeline::value_type StateCancelling = 4;

	// set when thread ready to be joined or detached to free it's resources
	static constexpr qtimeline::value_type StateFinalized = 6;

	static thread_t *self();
	static thread_t *self_noattach();

	static int create(thread_t **__SPRT_RESTRICT outthread, const attr_t *__SPRT_RESTRICT attr,
			void *(*cb)(void *), void *__SPRT_RESTRICT arg);
	static int detach(thread_t *thread);

	__SPRT_NORETURN static void exit(void *exitCode);

	static int join(thread_t *thread, void **exitCode);

	static int setcancelstate(int v, int *p);
	static int setcanceltype(int v, int *p);
	static void testcancel(void);

	static int cancel(thread_t *thread);

	static int getschedparam(thread_t *thread, int *__SPRT_RESTRICT policy,
			struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p);
	static int setschedparam(thread_t *thread, int n,
			const struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p);
	static int setschedprio(thread_t *thread, int p);

	struct mutex_info {
		int32_t prio;
	};

	__pool_unordered_map<mutex_t *, mutex_info> *threadRobustMutexes = nullptr;
	__pool_unordered_set<rwlock_t *> *threadWrLocks = nullptr;
	__pool_unordered_map<rwlock_t *, uint32_t> *threadRdLocks = nullptr;
	__pool_unordered_map<uint32_t, __key_specific> *threadKeyStorage = nullptr;

	attr_t attr;

	// qmutex and qtimeline has no priority invertion mitigation - use with care
	qmutex mutex;
	qtimeline state;

	thread_result_t result = 0;

	// Note that SPRT's jmp_buf is safe for destructors
	__sprt_jmp_buf jmpToRunthread;
	__UnwindInfo unwinder;

	sprt::atomic<int32_t> dynamicPrio;

	sprt::array<char, __SPRT_PTHREAD_NAMEMAXLEN + 1> threadName;

	// should be called from thread itself to acquire actual attibutes it running with
	void registerThread();

	void addMutex(mutex_t *, int32_t mutexPrio);

	// Priority inheritance protocol - some thread with higher priority wants this mutex
	void promoteMutex(mutex_t *, int32_t mutexPrio);

	void removeMutex(mutex_t *);

	bool has_wrlock(const rwlock_t *) const;
	bool has_rdlock(const rwlock_t *) const;

	void recalculateDynamicPriority(unique_lock<qmutex> &);

	void updateThreadPrio(unique_lock<qmutex> &, int32_t);

	int getcpuclockid(__SPRT_ID(clockid_t) *) const;
	int getaffinity(__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set);
	int setaffinity(__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set);

	int setname(const char *name);
	int getname(char *buf, size_t bufLen);

	int setname_native(const char *name);
};

} // namespace sprt::_thread

#endif // RUNTIME_CORE_PTHREAD_THREAD_T_H_
