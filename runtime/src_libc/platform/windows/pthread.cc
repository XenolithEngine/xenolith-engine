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

#include "pthread_win.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

struct _pthread_pool_t {
	atomic<int> concurency = 0;
	atomic<uint32_t> nkeys = 1;
	memory::dynmap<pthread_key_t, pthread_key_data> keys;
	_pthread_t *active = nullptr;
	_pthread_t *free = nullptr;

	pthread_attr_t defaultAttr;

	qmutex mutex;
};

struct ThreadSlot {
	pthread_t thread = nullptr;

	~ThreadSlot();
};

static _pthread_pool_t s_handlePool;
static ThreadSlot tl_self;

static _pthread_t *__allocateThread(const pthread_attr_t *attr) {
	_pthread_t *handle = nullptr;
	unique_lock lock(s_handlePool.mutex);
	if (s_handlePool.free) {
		handle = s_handlePool.free;
		s_handlePool.free = handle->next;
		lock.unlock();
		handle->next = nullptr;
	} else {
		lock.unlock();
		handle = (_pthread_t *)__sprt_malloc(sizeof(_pthread_t));
		handle->next = nullptr;
	}

	new (handle, nothrow) _pthread_t();
	if (attr) {
		handle->attr = *attr;
	}
	return handle;
}

static void __attachThread(_pthread_t *thread, HANDLE handle) {
	thread->handle = handle;

	unique_lock lock(s_handlePool.mutex);
	thread->next = s_handlePool.active;
	s_handlePool.active = thread;
}

static void __detachThread(_pthread_t *thread) {
	unique_lock lock(s_handlePool.mutex);
	s_handlePool.active = thread->next;
	thread->next = nullptr;
}

static void __deallocateThread(_pthread_t *handle) {
	unique_lock lock(s_handlePool.mutex);

	handle->~_pthread_t();
	handle->next = s_handlePool.free;
	s_handlePool.free = handle;
}

static void __detachAndDeallocateThread(_pthread_t *thread) {
	unique_lock lock(s_handlePool.mutex);

	if (thread->handle) {
		CloseHandle(thread->handle);
		thread->handle = nullptr;
	}

	s_handlePool.active = thread->next;
	thread->~_pthread_t();
	thread->next = s_handlePool.free;
	s_handlePool.free = thread;
}

ThreadSlot::~ThreadSlot() {
	if (thread && hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		// clear thread resources and free memory
		__detachAndDeallocateThread(thread);
	}
	thread = nullptr;
}

_pthread_t *_pthread_t::self() {
	if (tl_self.thread == nullptr) {
		// make pthread_t for external thread

		auto thread = __allocateThread(nullptr);

		thread->registerThread();

		thread->attr.attr |= ThreadAttrFlags::Detached | ThreadAttrFlags::Unmanaged;

		if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
					&thread->handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			__deallocateThread(thread);
			return nullptr;
		}

		thread->attr.prio = __unmapPriority(GetThreadPriority(thread->handle));

		thread->state.set_and_signal(_pthread_t::StateExternalInit);

		tl_self.thread = thread;
	}
	return tl_self.thread;
}

struct TimeInfo {
	LARGE_INTEGER qpc_freq;

	TimeInfo() {
		QueryPerformanceFrequency(&qpc_freq); //
	}
};

static TimeInfo s_timeInfo;

ULONGLONG _pthread_t::now() {
	LARGE_INTEGER pc;
	QueryPerformanceCounter(&pc);

	// milliseconds since some monotonic epoch
	return pc.QuadPart * 1'000ULL / s_timeInfo.qpc_freq.QuadPart;
}

void _pthread_t::registerThread() {
	if (threadMemPool) {
		return;
	}

	threadMemPool = memory::get_thread_support_pool();

	memory::perform([&] {
		threadRobustMutexes = new (threadMemPool) memory::set<pthread_mutex_t *>(threadMemPool);
		threadRobustMutexes->set_memory_persistent(true);
		memory::pool::cleanup_register(threadMemPool, threadRobustMutexes, [](void *data) {
			auto mutexes = (memory::set<pthread_mutex_t *> *)data;
			for (auto &it : *mutexes) {
				it->force_unlock(); //
			}
			return Status::Ok;
		});

		threadWrLocks = new (threadMemPool) memory::set<pthread_rwlock_t *>(threadMemPool);
		threadWrLocks->set_memory_persistent(true);
		memory::pool::cleanup_register(threadMemPool, threadWrLocks, [](void *data) {
			auto locks = (memory::set<pthread_rwlock_t *> *)data;
			for (auto &it : *locks) {
				it->force_unlock(false); //
			}
			return Status::Ok;
		});

		threadRdLocks =
				new (threadMemPool) memory::map<pthread_rwlock_t *, uint32_t>(threadMemPool);
		threadRdLocks->set_memory_persistent(true);
		memory::pool::cleanup_register(threadMemPool, threadRdLocks, [](void *data) {
			auto locks = (memory::set<pair<pthread_rwlock_t *, uint32_t>> *)data;
			for (auto &it : *locks) {
				it.first->force_unlock(true); //
			}
			return Status::Ok;
		});


		threadKeyStorage =
				new (threadMemPool) memory::map<uint32_t, pthread_key_specific>(threadMemPool);
		threadKeyStorage->set_memory_persistent(true);
		memory::pool::cleanup_register(threadMemPool, threadKeyStorage, [](void *data) {
			bool empty = true;
			auto specs = (memory::map<uint32_t, pthread_key_specific> *)data;
			// interate until all the keys freed
			auto iter = __SPRT_PTHREAD_DESTRUCTOR_ITERATIONS;
			do {
				empty = true;
				for (auto &it : *specs) {
					if (it.second.value) {
						empty = false;
						auto value = it.second.data;
						it.second.value = nullptr;
						it.second.data->destructor(value);
					}
				}
			} while (!empty && --iter > 0);

			for (auto &it : *specs) {
				// spec created only via increment for refcount, decrement it
				if (atomicFetchSub(&it.second.data->refcount, uint32_t(1)) == 1) {
					unique_lock lock(s_handlePool.mutex);
					s_handlePool.keys.erase(it.first);
				}
			}

			return Status::Ok;
		});
	}, threadMemPool);

	GetCurrentThreadStackLimits(&lowStack, &highStack);

	attr.stackSize = (highStack - lowStack);
}

bool _pthread_t::has_wrlock(const pthread_rwlock_t *lock) const {
	return threadWrLocks->find(lock) != threadWrLocks->end();
}

bool _pthread_t::has_rdlock(const pthread_rwlock_t *lock) const {
	return threadRdLocks->find(lock) != threadRdLocks->end();
}

static void __registerThread(_pthread_t *thread) { }

static __stdcall unsigned int __runthead(void *arg) {
	auto thread = (_pthread_t *)arg;

	thread->registerThread();

	tl_self.thread = thread;

	thread->state.set_and_signal(_pthread_t::StateInternalInit);
	thread->state.wait_value(_pthread_t::StateExternalInit);

	if (setjmp(thread->jmpToRunthread) == 0) {
		thread->arg = thread->cb(thread->arg);
	}

	unique_lock lock(thread->mutex);

	auto result = thread->result;

	thread->state.set_and_signal(_pthread_t::StateFinalized);
	lock.unlock(); // unlock before mutex's memory destroyed

	return result;
}

static int pthread_create(pthread_t *__SPRT_RESTRICT outthread,
		const pthread_attr_t *__SPRT_RESTRICT attr, void *(*cb)(void *),
		void *__SPRT_RESTRICT arg) {
	auto thread = __allocateThread(attr);
	thread->cb = cb;
	thread->arg = arg;

	auto handle = (HANDLE)_beginthreadex(nullptr, thread->attr.stackSize, &__runthead, thread,
			((thread->attr.stackSize > 0) ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0),
			&thread->threadId);
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		__deallocateThread(thread);
		return platform::lastErrorToErrno(GetLastError());
	}

	__attachThread(thread, handle);

	thread->state.wait_value(_pthread_t::StateInternalInit);

	// handle priority
	if (hasFlagAll(thread->attr.attr,
				ThreadAttrFlags::ApplyPrio | ThreadAttrFlags::ExplicitSched)) {
		SetThreadPriority(thread->handle, __mapPriority(thread->attr.prio));
	}

	// handle detached
	// No special handling required for now
	/*if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		CloseHandle(thread->handle);
		thread->handle = nullptr;
	}*/

	thread->state.set_and_signal(_pthread_t::StateExternalInit);
	*outthread = thread;
	return 0;
}

static int pthread_detach(pthread_t thread) {
	if (!thread) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	// Chack if HANDLE valid with GetHandleInformation
	DWORD dwInfo = 0;
	if (!thread->handle || !GetHandleInformation(thread->handle, &dwInfo)) {
		return ESRCH;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		return EINVAL;
	}

	// Thread is joinable
	// Check if thread's execution is complete
	// We hold the thread's mutex, so, it can not change state by itself
	// Finalizer (if ready) will be called after we release mutex

	if (thread->state.try_value(_pthread_t::StateFinalized)) {
		// thread was finalized as joinable, release it's resources
		CloseHandle(thread->handle);
		__detachAndDeallocateThread(thread);
	} else if (!hasFlag(thread->attr.attr, ThreadAttrFlags::JoinRequested)) {
		// mark as detached
		// with this flag, thread's resources will be destroyed right before main func returns
		thread->attr.attr |= ThreadAttrFlags::Detached;
	} else {
		// pthread_detach called when other thread calls pthread_join
		return EINVAL;
	}
	return 0;
}

static __SPRT_NORETURN void pthread_exit(void *exitCode) {
	if (tl_self.thread == nullptr
			|| hasFlag(tl_self.thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		_endthreadex((unsigned int)(ptrdiff_t)exitCode);
		__builtin_unreachable();
	} else {
		// longjmp on Windows will unwind the stack to call all finalizers, when C++ exceptions are enabled,
		// see https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/longjmp?view=msvc-170
		if (exitCode != 0) {
			tl_self.thread->arg = exitCode;
		}
		longjmp(tl_self.thread->jmpToRunthread, 1);
	}
}


static int __pthread_join(pthread_t thread, void **ret, DWORD timeout, bool tryjoin) {
	if (!thread) {
		return ESRCH;
	}

	if (thread == tl_self.thread) {
		return EDEADLK;
	}

	unique_lock lock(thread->mutex);

	// Chack if HANDLE valid with GetHandleInformation
	DWORD dwInfo = 0;
	if (!thread->handle || !GetHandleInformation(thread->handle, &dwInfo)) {
		return ESRCH;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		return EINVAL;
	}

	// thread is joinable

	// release all data if already complete
	if (thread->state.try_value(_pthread_t::StateFinalized)) {
		// thread was finalized as joinable, release it's resources
		CloseHandle(thread->handle);
		*ret = thread->arg;
		__detachAndDeallocateThread(thread);
		return 0;
	}

	if (tryjoin) {
		return EBUSY;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::JoinRequested)) {
		// Another thread is already waiting to join this thread.
		return EINVAL;
	}

	thread->attr.attr |= ThreadAttrFlags::JoinRequested;

	// Release the lock so the thread can terminate
	lock.unlock();

	auto waitret = WaitForSingleObject(thread->handle, timeout);
	if (waitret == WAIT_TIMEOUT) {
		lock.lock();
		thread->attr.attr &= ThreadAttrFlags::JoinRequested;
		return ETIMEDOUT;
	} else if (waitret == WAIT_FAILED) {
		return platform::lastErrorToErrno(GetLastError());
	}

	// thread is now terminated, WaitForSingleObject triggered after __runthead returns

	*ret = thread->arg;

	__detachAndDeallocateThread(thread);

	return 0;
}

static int pthread_join(pthread_t thread, void **ret) {
	return __pthread_join(thread, ret, INFINITE, false);
}

static pthread_t pthread_self(void) { return _pthread_t::self(); }

static int pthread_equal(pthread_t t1, pthread_t t2) { return t1 == t2; }

static int pthread_setcancelstate(int v, int *p) {
	auto thread = pthread_self();

	unique_lock lock(thread->mutex);

	if (p) {
		*p = hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityDisabled)
				? __SPRT_PTHREAD_CANCEL_DISABLE
				: __SPRT_PTHREAD_CANCEL_ENABLE;
	}

	if (v == __SPRT_PTHREAD_CANCEL_ENABLE) {
		thread->attr.attr &= ~ThreadAttrFlags::CancelabilityDisabled;
	} else if (v == __SPRT_PTHREAD_CANCEL_DISABLE) {
		thread->attr.attr |= ThreadAttrFlags::CancelabilityDisabled;
	} else {
		return EINVAL;
	}

	return 0;
}

static int pthread_setcanceltype(int v, int *p) {
	auto thread = pthread_self();

	unique_lock lock(thread->mutex);

	if (p) {
		*p = hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)
				? __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS
				: __SPRT_PTHREAD_CANCEL_DEFERRED;
	}

	if (v == __SPRT_PTHREAD_CANCEL_DEFERRED) {
		thread->attr.attr &= ~ThreadAttrFlags::CancelabilityAsync;
	} else if (v == __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS) {
		thread->attr.attr |= ThreadAttrFlags::CancelabilityAsync;
	} else {
		return EINVAL;
	}

	return 0;
}

static void pthread_testcancel(void) {
	auto thread = tl_self.thread;

	if (!thread) {
		return;
	}

	unique_lock lock(thread->mutex);

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::CancelRequested)
			&& !hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityDisabled)
			&& !hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)) {
		// __pthread_cancel is noreturn, release lock before it
		lock.unlock();
		pthread_exit(__SPRT_PTHREAD_CANCELED);
	}
}

static void __pthread_cancel_callback(ULONG_PTR Parameter) { pthread_exit((void *)Parameter); }

static int pthread_cancel(pthread_t thread) {
	if (!thread) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);
	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)) {
		thread->attr.attr |= ThreadAttrFlags::CancelRequested;
		return 0;
	}

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityDisabled)) {
		if (tl_self.thread == thread) {
			lock.unlock();
			pthread_exit(__SPRT_PTHREAD_CANCELED); // <-- noreturn
		} else if (thread->state.get_value() < _pthread_t::StateCancelling) {
			thread->state.set_and_signal(_pthread_t::StateCancelling);

			// Break user's context to call __pthread_cancel_callback on target thread
			// QueueUserAPC2 will unwind the stack for current user context
			// Then, pthread_exit should unwind the remaining stack with longjmp, if possible.

			QueueUserAPC2(__pthread_cancel_callback, thread->handle,
					(ULONG_PTR)__SPRT_PTHREAD_CANCELED, QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC);
		}
	} else {
		/* Docs says:

			If a thread has disabled cancelation, then
			a cancelation request remains queued until the thread enables
			cancelation.  If a thread has enabled cancelation, then its
			cancelability type determines when cancelation occurs.

		 So, we set CancelRequested flag.

		 Note, that doc does not says anything about cancelling in pthread_setcancelstate,
		 when pthread_cancel become enabled. On linux, this behavior is not implemented in libc's,
		 so, we omiting it too.
		*/
		thread->attr.attr |= ThreadAttrFlags::CancelRequested;
	}
	return 0;
}

static int pthread_getschedparam(pthread_t thread, int *__SPRT_RESTRICT policy,
		struct sched_param *__SPRT_RESTRICT p) {
	if (!thread) {
		return ESRCH;
	}

	if (policy) {
		if (hasFlag(thread->attr.attr, ThreadAttrFlags::ApplyPrio)) {
			*policy = __SPRT_SCHED_RR;
		} else {
			*policy = __SPRT_SCHED_OTHER;
		}
	}

	if (p) {
		p->sched_priority = thread->attr.prio;
	}
	return 0;
}

static int pthread_setschedparam(pthread_t thread, int n, const struct sched_param *p) {
	if (!thread || !thread->handle) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::ExplicitSched)) {
		// Thread scheduling is inherited, we can not change it
		return EINVAL;
	}

	if (p) {
		if (p->sched_priority < __SPRT_SCHED_PRIO_MIN) {
			thread->attr.prio = __SPRT_SCHED_PRIO_MIN;
		} else if (p->sched_priority > __SPRT_SCHED_PRIO_MAX) {
			thread->attr.prio = __SPRT_SCHED_PRIO_MAX;
		} else {
			thread->attr.prio = p->sched_priority;
		}
	}

	switch (n) {
	case __SPRT_SCHED_OTHER:
		if (hasFlag(thread->attr.attr, ThreadAttrFlags::ApplyPrio)) {
			thread->attr.attr &= ~ThreadAttrFlags::ApplyPrio;
			SetThreadPriority(thread->handle, THREAD_PRIORITY_NORMAL);
		}
		break;
	case __SPRT_SCHED_RR:
		thread->attr.attr |= ThreadAttrFlags::ApplyPrio;
		SetThreadPriority(thread->handle, __mapPriority(thread->attr.prio));
		break;
	default: return ENOTSUP;
	}

	return 0;
}

static int pthread_setschedprio(pthread_t thread, int p) {
	if (!thread || !thread->handle) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::ExplicitSched)) {
		// Thread scheduling is inherited, we can not change it
		return EINVAL;
	}

	if (p < __SPRT_SCHED_PRIO_MIN) {
		thread->attr.prio = __SPRT_SCHED_PRIO_MIN;
	} else if (p > __SPRT_SCHED_PRIO_MAX) {
		thread->attr.prio = __SPRT_SCHED_PRIO_MAX;
	} else {
		thread->attr.prio = p;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::ApplyPrio)) {
		SetThreadPriority(thread->handle, __mapPriority(thread->attr.prio));
	}

	return 0;
}

static int pthread_once(pthread_once_t *once, void (*cb)(void)) {
	static constexpr pthread_once_t StartedFlag = 1 << 0;
	static constexpr pthread_once_t CompleteFlag = 1 << 1;
	static constexpr pthread_once_t WaitFlag = 1 << 2;

	auto val = __atomic_fetch_or(once, StartedFlag, __ATOMIC_SEQ_CST);
	if (val == 0) {
		// The First One
		cb();

		// set complete flag and check for a waiters
		val = __atomic_fetch_or(once, CompleteFlag, __ATOMIC_SEQ_CST);
		if (val & WaitFlag) {

			// wake waiters if any
			WakeByAddressAll(once);
		}
	} else if (val & CompleteFlag) {
		// already complete, return
		return 0;
	} else {
		// set flag to notify The First One that we are waiting for him
		val = __atomic_fetch_or(once, WaitFlag, __ATOMIC_SEQ_CST);
		while ((val & CompleteFlag) == 0) {
			WaitOnAddress(once, &val, sizeof(pthread_once_t), INFINITE);
			val = __atomic_load_n(once, __ATOMIC_SEQ_CST);
		}
	}
	return 0;
}

static int pthread_attr_init(pthread_attr_t *attr) {
	unique_lock lock(s_handlePool.mutex);
	new (attr, sprt::nothrow) pthread_attr_t(s_handlePool.defaultAttr);
	return 0;
}

static int pthread_attr_destroy(pthread_attr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	attr->~pthread_attr_t();
	return 0;
}

static int pthread_attr_getguardsize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) {
	return ENOSYS;
}

static int pthread_attr_setguardsize(pthread_attr_t *attr, size_t v) { return ENOSYS; }

static int pthread_attr_getstacksize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	*ret = attr->stackSize;
	return 0;
}
static int pthread_attr_setstacksize(pthread_attr_t *attr, size_t v) {
	if (!attr || v > (1 << 24)) {
		return EINVAL;
	}

	attr->stackSize = static_cast<DWORD>(v);
	return 0;
}
static int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	if (hasFlag(attr->attr, ThreadAttrFlags::Detached)) {
		*ret = __SPRT_PTHREAD_CREATE_DETACHED;
	} else {
		*ret = __SPRT_PTHREAD_CREATE_JOINABLE;
	}

	return 0;
}
static int pthread_attr_setdetachstate(pthread_attr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_CREATE_JOINABLE: attr->attr &= ~ThreadAttrFlags::Detached; break;
	case __SPRT_PTHREAD_CREATE_DETACHED: attr->attr |= ThreadAttrFlags::Detached; break;
	default: return ENOTSUP;
	}
	return 0;
}
static int pthread_attr_getstack(const pthread_attr_t *__SPRT_RESTRICT attr,
		void **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz) {
	return ENOSYS;
}
static int pthread_attr_setstack(pthread_attr_t *attr, void *ptr, size_t sz) { return ENOSYS; }
static int pthread_attr_getscope(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_SCOPE_SYSTEM;
	return 0;
}
static int pthread_attr_setscope(pthread_attr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	if (v == __SPRT_PTHREAD_SCOPE_SYSTEM) {
		return 0;
	} else {
		return ENOTSUP;
	}
}
static int pthread_attr_getschedpolicy(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	if (hasFlag(attr->attr, ThreadAttrFlags::ApplyPrio)) {
		*ret = __SPRT_SCHED_RR;
	} else {
		*ret = __SPRT_SCHED_OTHER;
	}
	return 0;
}
static int pthread_attr_setschedpolicy(pthread_attr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_SCHED_OTHER: attr->attr &= ~ThreadAttrFlags::ApplyPrio; break;
	case __SPRT_SCHED_RR: attr->attr |= ThreadAttrFlags::ApplyPrio; break;
	default: return ENOTSUP;
	}
	return 0;
}
static int pthread_attr_getschedparam(const pthread_attr_t *__SPRT_RESTRICT attr,
		struct sched_param *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	ret->sched_priority = static_cast<int>(attr->prio);
	return 0;
}

static int pthread_attr_setschedparam(pthread_attr_t *__SPRT_RESTRICT attr,
		const struct sched_param *__SPRT_RESTRICT val) {
	if (!attr || !val) {
		return EINVAL;
	}

	if (val->sched_priority < __SPRT_SCHED_PRIO_MIN) {
		attr->prio = __SPRT_SCHED_PRIO_MIN;
	} else if (val->sched_priority > __SPRT_SCHED_PRIO_MAX) {
		attr->prio = __SPRT_SCHED_PRIO_MAX;
	} else {
		attr->prio = val->sched_priority;
	}
	return 0;
}

static int pthread_attr_getinheritsched(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!ret || !attr) {
		return EINVAL;
	}

	if (hasFlag(attr->attr, ThreadAttrFlags::ExplicitSched)) {
		*ret = __SPRT_PTHREAD_EXPLICIT_SCHED;
	} else {
		*ret = __SPRT_PTHREAD_INHERIT_SCHED;
	}
	return 0;
}
static int pthread_attr_setinheritsched(pthread_attr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_INHERIT_SCHED: attr->attr &= ~ThreadAttrFlags::ExplicitSched; break;
	case __SPRT_PTHREAD_EXPLICIT_SCHED: attr->attr |= ThreadAttrFlags::ExplicitSched; break;
	default: return ENOTSUP;
	}
	return 0;
}

static int pthread_key_create(pthread_key_t *key, void (*cb)(void *)) {
	if (!key) {
		return EINVAL;
	}

	*key = s_handlePool.nkeys.fetch_add(1);

	unique_lock lock(s_handlePool.mutex);
	s_handlePool.keys.emplace(*key, pthread_key_data{cb, 1});
	return 0;
}

static int pthread_key_delete(pthread_key_t key) {
	unique_lock lock(s_handlePool.mutex);

	auto it = s_handlePool.keys.find(key);
	if (it == s_handlePool.keys.end()) {
		return EINVAL;
	}

	if (atomicFetchSub(&it->second.refcount, uint32_t(1)) == 1) {
		s_handlePool.keys.erase(it);
	}
	return 0;
}

static const void *pthread_getspecific(pthread_key_t key) {
	if (!tl_self.thread) {
		return nullptr;
	}

	auto it = tl_self.thread->threadKeyStorage->find(key);
	if (it != tl_self.thread->threadKeyStorage->end()) {
		return it->second.value;
	}

	return nullptr;
}

static int pthread_setspecific(pthread_key_t key, const void *val) {
	auto self = pthread_self();

	if (!self) {
		return EINVAL;
	}

	auto it = tl_self.thread->threadKeyStorage->find(key);
	if (it == tl_self.thread->threadKeyStorage->end()) {
		pthread_key_data *data = nullptr;

		unique_lock lock(s_handlePool.mutex);
		auto keyIt = s_handlePool.keys.find(key);
		if (keyIt == s_handlePool.keys.end()) {
			return EINVAL;
		}

		atomicFetchAdd(&keyIt->second.refcount, uint32_t(1));
		data = &keyIt->second;
		lock.unlock();

		tl_self.thread->threadKeyStorage->emplace(key, pthread_key_specific{data, val});
	} else {
		it->second.value = val;
	}
	return 0;
}

static int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
	return ENOSYS;
}

static int pthread_getconcurrency(void) { return s_handlePool.concurency.load(); }
static int pthread_setconcurrency(int v) {
	if (v < 0) {
		return EINVAL;
	}
	s_handlePool.concurency.store(v);
	return 0;
}

static int pthread_getcpuclockid(pthread_t thread, __sprt_clockid_t *clock) {
	if (!thread) {
		return ESRCH;
	}
	if (!clock) {
		return EINVAL;
	}

	*clock = __SPRT_CLOCK_REALTIME;
	return 0;
}

static int pthread_getaffinity_np(pthread_t thread, size_t n, __sprt_cpu_set_t *set) {
	if (!thread || !set) {
		return EINVAL;
	}

#warning @TODO: https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets

	return 0;
}

static int pthread_setaffinity_np(pthread_t thread, size_t n, const __sprt_cpu_set_t *set) {
	if (!thread || !set) {
		return EINVAL;
	}

#warning @TODO: https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets

	return 0;
}

static int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
	if (!thread || !attr) {
		return EINVAL;
	}

	__sprt_memcpy(attr, &thread->attr, sizeof(pthread_attr_t));

	return 0;
}
static int pthread_setname_np(pthread_t thread, const char *name) {
	if (!thread || !name) {
		return EINVAL;
	}

	int ret = 0;

	unicode::toUtf16([&](WideStringView wname) {
		auto hr = SetThreadDescription(thread->handle, (wchar_t *)wname.data());
		if (FAILED(hr)) {
			ret = EINVAL;
		}
	}, StringView(name));

	return ret;
}

static int pthread_getname_np(pthread_t thread, char *buf, size_t len) {
	if (!thread || !buf) {
		return EINVAL;
	}

	wchar_t *wbuf = nullptr;
	auto hr = GetThreadDescription(thread->handle, &wbuf);
	if (FAILED(hr)) {
		return EINVAL;
	}

	int ret = 0;
	unicode::toUtf8([&](StringView name) {
		if (name.size() <= len) {
			__sprt_memcpy(buf, name.data(), name.size());
		} else {
			__sprt_memcpy(buf, name.data(), len);
			ret = ERANGE;
		}
	}, WideStringView((char16_t *)wbuf));

	return ret;
}

static int pthread_getattr_default_np(pthread_attr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	unique_lock lock(s_handlePool.mutex);
	*attr = s_handlePool.defaultAttr;
	return 0;
}

static int pthread_setattr_default_np(const pthread_attr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	unique_lock lock(s_handlePool.mutex);
	s_handlePool.defaultAttr = *attr;

	return 0;
}

static int pthread_tryjoin_np(pthread_t thread, void **ret) {
	return __pthread_join(thread, ret, INFINITE, true);
}

static int pthread_timedjoin_np(pthread_t thread, void **ret, const __SPRT_TIMESPEC_NAME *tv) {
	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	DWORD millis = diffTv.tv_sec * 1'000 + diffTv.tv_nsec / 1'000'000;
	return __pthread_join(thread, ret, millis, false);
}

} // namespace sprt
