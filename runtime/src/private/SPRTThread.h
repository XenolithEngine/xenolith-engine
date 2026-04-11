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

#ifndef RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_
#define RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_

#include <sprt/runtime/thread/entry.h>
#include <sprt/runtime/thread/info.h>

#include <sprt/cxx/unordered_map>
#include <sprt/cxx/unordered_set>
#include <sprt/runtime/thread/rmutex.h>
#include <sprt/runtime/thread/qmutex.h>
#include <sprt/runtime/thread/qtimeline.h>
#include <sprt/runtime/thread/qonce.h>
#include <sprt/runtime/thread/qrwlock.h>
#include <sprt/runtime/thread/qbarrier.h>
#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/c/__sprt_setjmp.h>
#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_pthread.h>

#include <unwind.h>

namespace sprt::_thread {

// platform-specific unwinding information
struct __UnwindInfo {
	_Unwind_Exception excpt;
};

static constexpr int DESTRUCTOR_ITERATIONS = 4;
static constexpr int COMMON_ALIGNMENT = 8;

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

	PrioRR = 1 << 14,
	PrioFifo = 2 << 14,
	PrioMask = PrioRR | PrioFifo,
};

struct mutex_t;
struct rwlock_t;

// in nanoseconds
using timeout_t = __sprt_sprt_timeout_t;

using key_t = uint32_t;

#if SPRT_WINDOWS
using thread_result_t = unsigned int;
#else
using thread_result_t = void *;
#endif

static constexpr timeout_t Infinite = __SPRT_SPRT_TIMEOUT_INFINITE;

struct __key_data {
	void (*destructor)(void *);
	uint32_t refcount = 1;
};

struct __key_specific {
	__key_data *data = nullptr;
	const void *value = nullptr;
};

SPRT_DEFINE_ENUM_AS_MASK(ThreadAttrFlags)

struct alignas(COMMON_ALIGNMENT) attr_t {
	ThreadAttrFlags attr = ThreadAttrFlags::None;
	int16_t prio = 0;
	uint32_t stackSize = 0;

	uint32_t guardSize = 0;
	void *stack = 0;
};

struct thread_t {
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
			__SPRT_ID(sched_param) * __SPRT_RESTRICT p);
	static int setschedparam(thread_t *thread, int n,
			const __SPRT_ID(sched_param) * __SPRT_RESTRICT p);
	static int setschedprio(thread_t *thread, int p);

	thread_t *next = nullptr;

	// Memory pool to use with thread-scoped memory,
	// Should be acquired from memory::get_thread_support_pool() in thread's context
	memory::pool_t *threadMemPool = nullptr;

	struct mutex_info {
		int32_t prio;
	};

	__pool_unordered_map<mutex_t *, mutex_info> *threadRobustMutexes = nullptr;
	__pool_unordered_set<rwlock_t *> *threadWrLocks = nullptr;
	__pool_unordered_map<rwlock_t *, uint32_t> *threadRdLocks = nullptr;
	__pool_unordered_map<uint32_t, __key_specific> *threadKeyStorage = nullptr;

	void *handle = nullptr;
	attr_t attr;

	// qmutex and qtimeline has no priority invertion mitigation - use with care
	qmutex mutex;
	qtimeline state;

	unsigned int threadId = 0;
	thread_result_t result = 0;
	void *(*cb)(void *) = nullptr;
	void *arg = nullptr;

	uintptr_t lowStack = 0;
	uintptr_t highStack = 0;

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
	CondAttrFlags flags = CondAttrFlags::None;
	uint32_t counter = 0;
	uint32_t value = 0;
	uint32_t previous = 0;
	uint64_t mutexid = 0;

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

struct rwlock_t {
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

SPRT_LOCAL void _entry_platform(const callbacks &, NotNull<Ref>);

SPRT_LOCAL void _init(const callbacks &cb, Ref *tm);
SPRT_LOCAL bool _worker(const callbacks &cb, Ref *tm);
SPRT_LOCAL void _dispose(const callbacks &cb, Ref *tm);

} // namespace sprt::_thread

#endif // RUNTIME_SRC_PRIVATE_SPRTTHREAD_H_
