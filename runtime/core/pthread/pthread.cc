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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_sched.h>

#ifndef SPRT_WINDOWS
#include <pthread.h>

#include "pthread_native_pthread.cc"
#else
#include "pthread_native_winapi.cc"
#endif

#include <sprt/cxx/cstring>

__SPRT_C_FUNC __sprt_uint64_t __libc_main_thread;

namespace sprt::_thread {

thread_local __thread_slot tl_self;

static __thread_pool s_handlePool;

__thread_pool *__thread_pool::get() { return &s_handlePool; }

__thread_pool::__thread_pool() {
	// Acquire OS schedulers limits on startup to use it with attr_t
	fifoPrioMin = __sprt_sched_get_priority_min(__SPRT_SCHED_FIFO);
	fifoPrioMax = __sprt_sched_get_priority_max(__SPRT_SCHED_FIFO);
	rrPrioMin = __sprt_sched_get_priority_min(__SPRT_SCHED_RR);
	rrPrioMax = __sprt_sched_get_priority_max(__SPRT_SCHED_RR);
	otherPrioMin = __sprt_sched_get_priority_min(__SPRT_SCHED_OTHER);
	otherPrioMax = __sprt_sched_get_priority_max(__SPRT_SCHED_OTHER);
}

__thread_pool::~__thread_pool() {
	main.state.set_and_signal(thread_t::StateFinalized);

	if (main.threadMemPool) {
		memory::pool::destroy(main.threadMemPool);
		memory::allocator::terminate(&main.threadAlloc);
		main.threadMemPool = nullptr;
	}

	if (main.handle) {
		native::__closeNativeHandle(main.handle);
		main.handle = nullptr;
	}

	while (free) {
		auto tmp = free;
		free = static_cast<thread_t *>(free->next);

		__sprt_local_free(tmp, sizeof(thread_t));
	}
}

bool __thread_pool::isPrioValid(int policy, int prio) {
	switch (policy) {
	case __SPRT_SCHED_RR:
		if (prio < rrPrioMin || prio > rrPrioMax) {
			return false;
		}
		break;
	case __SPRT_SCHED_FIFO:
		if (prio < fifoPrioMin || prio > fifoPrioMax) {
			return false;
		}
		break;
	case __SPRT_SCHED_OTHER:
		if (prio < otherPrioMin || prio > otherPrioMax) {
			return false;
		}
		break;
	default: return false; break;
	}
	return true;
}

bool __thread_pool::isPrioValid(ThreadAttrFlags attr, int prio) {
	auto policy = attr & ThreadAttrFlags::PrioMask;
	switch (policy) {
	case ThreadAttrFlags::PrioRR:
		if (prio < rrPrioMin || prio > rrPrioMax) {
			return false;
		}
		break;
	case ThreadAttrFlags::PrioFifo:
		if (prio < fifoPrioMin || prio > fifoPrioMax) {
			return false;
		}
		break;
	case ThreadAttrFlags::None:
		if (prio < otherPrioMin || prio > otherPrioMax) {
			return false;
		}
		break;
	default: return false; break;
	}
	return true;
}

static SPRT_RUNTHREAD_CALLCONV thread_result_t __runthead(void *arg) {
	auto thread = (thread_t *)arg;

	// Must match the key used by __attachNativeThread (native::__getNativeThreadId),
	// not the kernel tid, otherwise the erase below never removes the entry.
	auto tid = native::__getNativeThreadId();

	if (!thread->registerThread()) {
		unique_lock globalLock(s_handlePool.mutex);
		s_handlePool.activeThreads.erase(tid);
		globalLock.unlock();

		thread->state.set_and_signal(thread_t::StateFinalized);

		__sprt_libc_thread_exit(true);

		return 0;
	}

	tl_self.thread = thread;

	thread->state.set_and_signal(thread_t::StateInternalInit);
	thread->state.wait(thread_t::StateExternalInit);

	if (__sprt_setjmp(thread->jmpToRunthread) == 0) {
		thread->arg = thread->cb(thread);
	}

	unique_lock globalLock(s_handlePool.mutex);
	s_handlePool.activeThreads.erase(tid);
	// Remove the tid->thread mapping while the thread is still alive, so a stale tid
	// (which the OS may reuse) never resolves to a finalized/recycled thread.
	s_handlePool.activeThreadsByTid.erase(uint32_t(thread->threadId));
	globalLock.unlock();

	unique_lock lock(thread->mutex);

	auto result = thread->result;

	// Publish StateFinalized and decide ownership of finalization while still
	// holding the mutex. A concurrent join()/detach() inspects the flags and state
	// under the same mutex, so exactly one party frees the thread:
	//  - detached -> free here under the lock (no joiner can exist for a detached
	//    thread). Freeing under the lock keeps a racing detach()/join() validity
	//    check from observing a half-destroyed object.
	//  - joinable -> just release the mutex; the joining/detaching thread frees it
	//    after it re-acquires the mutex and observes StateFinalized. Do NOT touch
	//    `thread` afterwards: the joiner may already have freed it.
	// `result` is read above (before signalling) because the joiner may free the
	// thread the moment we release the mutex.
	thread->state.set_and_signal(thread_t::StateFinalized);

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		// __detachAndDeallocateThread releases the lock right before ~thread_t()
		__detachAndDeallocateThread(thread, &lock);
	} else {
		lock.unlock();
	}
	thread = nullptr;

	__sprt_libc_thread_exit(true);

	return result;
}

static void __attachNativeThread(thread_t *thread, void *handle, uint64_t id,
		unique_lock<qmutex> &lock) {
	thread->handle = handle;

	s_handlePool.activeThreads.emplace(id, thread);
}

static thread_t *__allocateThread(const attr_t *attr) {
	thread_t *handle = nullptr;
	unique_lock lock(s_handlePool.mutex);
	if (s_handlePool.free) {
		handle = s_handlePool.free;
		s_handlePool.free = static_cast<thread_t *>(handle->next);
		lock.unlock();
		handle->next = nullptr;
	} else {
		lock.unlock();
		handle = (thread_t *)__sprt_local_alloc(sizeof(thread_t));
		handle->next = nullptr;
	}

	auto thread = new (handle, nothrow) thread_t();
	if (attr) {
		thread->attr = *attr;
	}
	memset(thread->threadName.data(), 0, thread->threadName.size());
	return thread;
}

static void __deallocateThread(thread_t *handle) {
	unique_lock lock(s_handlePool.mutex);

	handle->~thread_t();
	if (handle != &s_handlePool.main) {
		handle->next = s_handlePool.free;
		s_handlePool.free = handle;
	}
}

static void __detachAndDeallocateThread(thread_t *thread, unique_lock<qmutex> *externalLock) {
	if (thread->threadMemPool) {
		memory::pool::destroy(thread->threadMemPool);
		thread->threadMemPool = nullptr;
	}
	memory::allocator::terminate(&thread->threadAlloc);

	unique_lock lock(s_handlePool.mutex);

	if (thread->handle) {
		native::__closeNativeHandle(thread->handle);
		thread->handle = nullptr;
	}

	// External lock should be released before thread's destructor called
	if (externalLock && externalLock->owns_lock()) {
		externalLock->unlock();
	}
	thread->~thread_t();
	if (native::__getNativeThreadId() != __libc_main_thread) {
		thread->next = s_handlePool.free;
		s_handlePool.free = thread;
	}
}

static void destroyThisThread() {
	auto thread = tl_self.thread;
	if (!thread) {
		return;
	}

	{
		// External thread is leaving; drop its tid mapping before it is freed.
		unique_lock lock(s_handlePool.mutex);
		s_handlePool.activeThreadsByTid.erase(uint32_t(thread->threadId));
	}

	thread->state.set_and_signal(thread_t::StateFinalized);

	__detachAndDeallocateThread(thread, nullptr);
};

static bool attachExternalThread(thread_t *thread) {
	thread->attr.attr |= ThreadAttrFlags::Detached | ThreadAttrFlags::Unmanaged;

	memory::allocator::initialize(&thread->threadAlloc);
	thread->threadMemPool = memory::pool::create(&thread->threadAlloc);

	if (!thread->registerThread()) {
		// Tear down the pool/allocator we just created; the caller's __deallocateThread
		// only runs ~thread_t() and would otherwise leak them.
		if (thread->threadMemPool) {
			memory::pool::destroy(thread->threadMemPool);
			thread->threadMemPool = nullptr;
		}
		memory::allocator::terminate(&thread->threadAlloc);
		return false;
	}

	thread->state.set_and_signal(thread_t::StateExternalInit);

	tl_self.thread = thread;

	if (thread != &s_handlePool.main) {
		// WARNING: This will init thread-local storage on Windows
		native::__registerForDestruction(&destroyThisThread);
	}
	return true;
}

thread_t *thread_t::self() {
	if (tl_self.thread) {
		return tl_self.thread;
	}

	auto nativeId = native::__getNativeThreadId();

	if (__libc_main_thread == nativeId) {
		if (!attachExternalThread(&s_handlePool.main)) {
			// Fail to attach main thread - unrecoverable
			__sprt_abort();
		}
		return tl_self.thread;
	}

	if (tl_self.thread == nullptr) {
		// We can be here for two reasons.
		// The first one is an external thread created outside of SPRT.
		// The second is that we are trying to access the thread data
		// before its main function called

		// First - check, if we have registred thread with out tid:
		unique_lock lock(s_handlePool.mutex);
		auto it = s_handlePool.activeThreads.find(nativeId);
		if (it != s_handlePool.activeThreads.end()) {
			tl_self.thread = it->second;
			return it->second;
		}

		lock.unlock();

		// it's an external thread, create pthread_t handle
		auto nthread = __allocateThread(nullptr);
		if (!attachExternalThread(nthread)) {
			__deallocateThread(nthread);
			return nullptr;
		}
	}
	return tl_self.thread;
}

thread_t *thread_t::self_noattach() {
	if (tl_self.thread) {
		return tl_self.thread;
	}
	return nullptr;
}

bool thread_t::registerThread() {
	threadId = __sprt_gettid();

	// read actual attributes
	if (!native::__initNativeHandle(this)) {
		return false;
	}

	// Seed the PI base priority from the actual scheduling priority that
	// __initNativeHandle just resolved (mirrors dynamicPrio's initialization).
	basePrio = attr.prio;

	// setup data structs
	memory::perform([&] {
		threadRobustMutexes =
				new (threadMemPool) __pool_unordered_map<mutex_t *, mutex_info>(threadMemPool);
		threadRobustMutexes->max_load_factor(2.0f);
		memory::pool::cleanup_register(threadMemPool, threadRobustMutexes, [](void *data) {
			auto mutexes = (__pool_unordered_map<mutex_t *, mutex_info> *)data;
			for (auto &it : *mutexes) {
				it.first->force_unlock(); //
			}
			return Status::Ok;
		});

		threadWrLocks = new (threadMemPool) __pool_unordered_set<rwlock_t *>(threadMemPool);
		threadWrLocks->max_load_factor(2.0f);
		memory::pool::cleanup_register(threadMemPool, threadWrLocks, [](void *data) {
			auto locks = (__pool_unordered_set<rwlock_t *> *)data;
			for (auto &it : *locks) {
				it->force_unlock(false); //
			}
			return Status::Ok;
		});

		threadRdLocks =
				new (threadMemPool) __pool_unordered_map<rwlock_t *, uint32_t>(threadMemPool);
		threadRdLocks->max_load_factor(2.0f);
		memory::pool::cleanup_register(threadMemPool, threadRdLocks, [](void *data) {
			auto locks = (__pool_unordered_map<rwlock_t *, uint32_t> *)data;
			for (auto &it : *locks) {
				// it.second is the recursive read-lock depth; the kernel-side reader
				// counter was incremented once per rdlock, so release it that many
				// times or the leftover count starves writers forever.
				for (uint32_t i = 0; i < it.second; ++i) {
					it.first->force_unlock(true); //
				}
			}
			return Status::Ok;
		});

		threadKeyStorage =
				new (threadMemPool) __pool_unordered_map<uint32_t, __key_specific>(threadMemPool);
		threadKeyStorage->max_load_factor(2.0f);
		memory::pool::cleanup_register(threadMemPool, threadKeyStorage, [](void *data) {
			bool empty = true;
			auto specs = (__pool_unordered_map<uint32_t, __key_specific> *)data;
			// interate until all the keys freed
			auto iter = DESTRUCTOR_ITERATIONS;
			do {
				empty = true;
				for (auto &it : *specs) {
					// POSIX permits a NULL destructor (pthread_key_create(&k, NULL));
					// such keys carry no cleanup, so skip them instead of calling NULL.
					if (it.second.value && it.second.data->destructor) {
						empty = false;
						auto value = const_cast<void *>(it.second.value);
						it.second.value = nullptr;
						it.second.data->destructor(value);
					}
				}
			} while (!empty && --iter > 0);

			for (auto &it : *specs) {
				// spec created only via increment for refcount, decrement it
				if (_atomic::fetchSub(&it.second.data->refcount, uint32_t(1)) == 1) {
					unique_lock lock(s_handlePool.mutex);
					s_handlePool.keys.erase(it.first);
				}
			}

			return Status::Ok;
		});
	}, threadMemPool);

	// Make this thread discoverable by kernel tid for the PI boost path. Done last so
	// the thread is fully set up before another thread can find and boost it.
	{
		unique_lock lock(s_handlePool.mutex);
		s_handlePool.activeThreadsByTid.emplace(uint32_t(threadId), this);
	}
	return true;
}


void thread_t::addMutex(mutex_t *mtx, int32_t mutexPrio) {
	unique_lock lock(prioMutex);
	if (threadRobustMutexes) {
		auto it = threadRobustMutexes->find(mtx);
		if (it != threadRobustMutexes->end()) {
			it->second.prio = mutexPrio;
		} else {
			threadRobustMutexes->emplace(mtx, mutex_info{mutexPrio});
		}

		if (mutexPrio > dynamicPrio.load()) {
			// We already know the result of recalculateDynamicPriority, apply it
			updateThreadPrio(lock, mutexPrio);
		}
	}
}

void thread_t::promoteMutex(mutex_t *mtx, int32_t mutexPrio) {
	unique_lock lock(prioMutex);
	if (threadRobustMutexes) {
		auto it = threadRobustMutexes->find(mtx);
		if (it != threadRobustMutexes->end()) {
			it->second.prio = mutexPrio;

			if (mutexPrio > dynamicPrio.load()) {
				// We already know the result of recalculateDynamicPriority, apply it
				updateThreadPrio(lock, mutexPrio);
			}
		}
	}
}

void thread_t::demoteMutex(mutex_t *mtx, int32_t boostPrio) {
	unique_lock lock(prioMutex);
	if (threadRobustMutexes) {
		auto it = threadRobustMutexes->find(mtx);
		// Only revert our own boost: if the recorded priority no longer equals the
		// value we raised it to, another waiter has since boosted this mutex higher,
		// and clobbering it would drop that thread's inheritance (ABA). Otherwise reset
		// the mutex's contribution to the base and recompute downward.
		if (it != threadRobustMutexes->end() && it->second.prio == boostPrio) {
			it->second.prio = basePrio;
			recalculateDynamicPriority(lock);
		}
	}
}

void thread_t::removeMutex(mutex_t *mtx) {
	unique_lock lock(prioMutex);
	if (threadRobustMutexes) {
		threadRobustMutexes->erase(mtx);
		recalculateDynamicPriority(lock);
	}
}

bool thread_t::has_wrlock(const rwlock_t *lock) const {
	return threadWrLocks->find(lock) != threadWrLocks->end();
}

bool thread_t::has_rdlock(const rwlock_t *lock) const {
	return threadRdLocks->find(lock) != threadRdLocks->end();
}

void thread_t::recalculateDynamicPriority(unique_lock<qmutex> &lock) {
	// basePrio is guarded by prioMutex (held by the caller), so the recalculation
	// never has to read attr.prio under thread->mutex.
	int32_t newPrio = basePrio;

	if (threadRobustMutexes) {
		for (auto &it : *threadRobustMutexes) {
			newPrio = sprt::max(newPrio, it.second.prio); //
		}
	}

	if (newPrio != dynamicPrio.load()) {
		updateThreadPrio(lock, newPrio);
	}
}

void thread_t::updateThreadPrio(unique_lock<qmutex> &, int32_t dprio) {
	dynamicPrio = dprio;
	native::__applyThreadPrio(this, dprio);
}

int thread_t::create(thread_t **__SPRT_RESTRICT outthread, const attr_t *__SPRT_RESTRICT attr,
		void *(*cb)(thread_base_t *), void *__SPRT_RESTRICT arg,
		const Callback<void(uint8_t *st, size_t stSize)> &dataCallback) {
	auto thread = __allocateThread(attr);
	thread->cb = cb;
	thread->arg = arg;

	if (dataCallback) {
		dataCallback(thread->storage, sizeof(thread->storage));
	}

	// Thread locals will be initialized before we acquire control over the new thread;
	// We need to provide allocator and pool before it
	memory::allocator::initialize(&thread->threadAlloc);
	thread->threadMemPool = memory::pool::create(&thread->threadAlloc);

	auto ret =
			native::__createThread(thread, attr ? attr : &s_handlePool.defaultAttr, &s_handlePool);
	if (ret != 0) {
		// The native thread never started, so it cannot register/free itself.
		// Tear down the pool and allocator we created above before recycling,
		// otherwise they leak (~thread_t() does not own them).
		if (thread->threadMemPool) {
			memory::pool::destroy(thread->threadMemPool);
			thread->threadMemPool = nullptr;
		}
		memory::allocator::terminate(&thread->threadAlloc);
		__deallocateThread(thread);
		return ret;
	}

	// Here we have running thread, that will regiter itself,
	// Signal with thread_t::StateInternalInit,
	// then wait for  thread_t::StateExternalInit

	// Until thread is completely setup - user should not call any function on it,
	// neither from parent or thread itself. We should know, that setup was successful,
	// and make the thread know, that we know it.

	thread->state.wait(thread_t::StateInternalInit);

	// If we will need for additional setup on parent's side - place it here
	if (thread->state.get_value() == thread_t::StateFinalized) {
		// The new thread failed to initialize itself (registerThread), and has
		// already erased itself from activeThreads, signalled StateFinalized and
		// exited. It was never published to the caller, so this thread owns it.
		// Tear down its resources (pool, allocator, native handle) and recycle it;
		// returning here without this leaks the thread object and its pool.
		if (thread->threadMemPool) {
			memory::pool::destroy(thread->threadMemPool);
			thread->threadMemPool = nullptr;
		}
		memory::allocator::terminate(&thread->threadAlloc);
		if (thread->handle) {
			native::__closeNativeHandle(thread->handle);
			thread->handle = nullptr;
		}
		__deallocateThread(thread);
		// TODO: find a better value?
		return EINVAL;
	}

	thread->state.set_and_signal(thread_t::StateExternalInit);

	// now both threads are synchronized

	*outthread = thread;

	return 0;
}

int thread_t::detach(thread_t *thread) {
	if (!thread) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	if (!native::__isNativeHandleValid(thread)) {
		return ESRCH;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return EINVAL;
	}

	// Thread is joinable
	// Check if thread's execution is complete
	// We hold the thread's mutex, so, it can not change state by itself
	// Finalizer (if ready) will be called after we release mutex

	if (thread->state.try_wait(thread_t::StateFinalized)) {
		// thread was finalized as joinable, release it's resources
		__detachAndDeallocateThread(thread, &lock);
	} else if (!hasFlag(thread->attr.attr, ThreadAttrFlags::JoinRequested)) {
		// Mark the thread as detached;
		// with this flag, thread's resources will be destroyed right before __runthread func returns
		thread->attr.attr |= ThreadAttrFlags::Detached;
	} else {
		// pthread_detach called when other thread calls pthread_join
		return EINVAL;
	}
	return 0;
}

__SPRT_NORETURN void thread_t::exit(void *exitCode) {
	if (tl_self.thread == nullptr
			|| hasFlag(tl_self.thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		// this is unmanaged thread, use native exit function
		native::__exitNativeThread(exitCode);
		__builtin_unreachable();
	} else {
		if (exitCode != 0) {
			tl_self.thread->arg = exitCode;
		}
		__sprt_longjmp(tl_self.thread->jmpToRunthread, 1);
		__builtin_unreachable();
	}
}

static int __pthread_join(thread_t *thread, void **ret, timeout_t timeout, bool tryjoin) {
	if (!thread) {
		return ESRCH;
	}

	if (thread == tl_self.thread) {
		return EDEADLK;
	}

	unique_lock lock(thread->mutex);

	// Chack if HANDLE valid with GetHandleInformation
	if (!native::__isNativeHandleValid(thread)) {
		return ESRCH;
	}

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return EINVAL;
	}

	// thread is joinable

	// release all data if already complete
	if (thread->state.try_wait(thread_t::StateFinalized)) {
		// thread was finalized as joinable, release it's resources
		if (ret) {
			*ret = thread->arg;
		}
		__detachAndDeallocateThread(thread, &lock);
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

	Status st;
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		st = thread->state.wait(thread_t::StateFinalized);
	} else {
		st = thread->state.wait(thread_t::StateFinalized, timeout);
	}

	if (st == Status::Ok) {
		// thread is now terminated. Re-acquire the mutex before freeing: __runthead
		// publishes StateFinalized while still holding it, so locking here guarantees
		// __runthead has released the mutex and is no longer touching the thread.
		lock.lock();
		if (ret) {
			*ret = thread->arg;
		}

		__detachAndDeallocateThread(thread, &lock);

		return 0;
	}

	// The wait timed out (or errored). Re-acquire the mutex and relinquish our join
	// claim so the thread stays joinable/detachable, per POSIX pthread_timedjoin_np.
	// Leaving JoinRequested set would make every later join/detach return EINVAL and
	// leak the thread once it finalizes (it is neither detached nor has a joiner).
	lock.lock();
	if (thread->state.try_wait(thread_t::StateFinalized)) {
		// It finalized in the race window between the timeout and re-locking; complete
		// the join now rather than forcing the caller to retry.
		if (ret) {
			*ret = thread->arg;
		}
		__detachAndDeallocateThread(thread, &lock);
		return 0;
	}

	thread->attr.attr &= ~ThreadAttrFlags::JoinRequested;
	return status::toErrno(st);
}

int thread_t::join(thread_t *thread, void **ret) {
	return __pthread_join(thread, ret, Infinite, false);
}

int thread_t::setcancelstate(int v, int *p) {
	auto thread = tl_self.thread;
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

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

int thread_t::setcanceltype(int v, int *p) {
	auto thread = tl_self.thread;
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	if (p) {
		*p = hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)
				? __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS
				: __SPRT_PTHREAD_CANCEL_DEFERRED;
	}

	if (v == __SPRT_PTHREAD_CANCEL_DEFERRED) {
		thread->attr.attr &= ~ThreadAttrFlags::CancelabilityAsync;
	} else if (hasFlag(thread->attr.attr, ThreadAttrFlags::CancelAsyncSupported)
			&& v == __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS) {
		thread->attr.attr |= ThreadAttrFlags::CancelabilityAsync;
	} else {
		return EINVAL;
	}

	return 0;
}

void thread_t::testcancel(void) {
	auto thread = tl_self.thread;
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return;
	}

	unique_lock lock(thread->mutex);

	if (hasFlag(thread->attr.attr, ThreadAttrFlags::CancelRequested)
			&& !hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityDisabled)
			&& !hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)) {
		// __pthread_cancel is noreturn, release lock before it
		lock.unlock();
		thread_t::exit(__SPRT_PTHREAD_CANCELED);
	}
}

int thread_t::cancel(thread_t *thread) {
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

	unique_lock lock(thread->mutex);

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityAsync)) {
		// We in the deferred mode, mark thread to be cancelled on testcancel;
		// testcancel() also tests for CancelabilityDisabled, we need not check it here
		thread->attr.attr |= ThreadAttrFlags::CancelRequested;
		return 0;
	}

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::CancelabilityDisabled)) {
		// thread can be cancelled in async mode
		if (tl_self.thread == thread) {
			// It it's out thread - just exit
			lock.unlock();
			thread_t::exit(__SPRT_PTHREAD_CANCELED); // <-- noreturn
		} else if (thread->state.get_value() < thread_t::StateCancelling) {
			thread->state.set_and_signal(thread_t::StateCancelling);

			native::__cancelThreadAsync(thread);
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

int thread_t::getschedparam(thread_t *thread, int *__SPRT_RESTRICT policy,
		struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p) {
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

	if (policy) {
		auto p = thread->attr.attr & ThreadAttrFlags::PrioMask;
		switch (p) {
		case ThreadAttrFlags::PrioRR: *policy = __SPRT_SCHED_RR; break;
		case ThreadAttrFlags::PrioFifo: *policy = __SPRT_SCHED_FIFO; break;
		default: *policy = __SPRT_SCHED_OTHER; break;
		}
	}

	if (p) {
		p->sched_priority = thread->attr.prio;
	}
	return 0;
}

int thread_t::setschedparam(thread_t *thread, int n,
		const struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p) {
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

	if (!p || !s_handlePool.isPrioValid(n, p->sched_priority)) {
		return EINVAL;
	}

	unique_lock lock(thread->mutex);

	if (n == __SPRT_SCHED_OTHER) {
		thread->attr.attr &= ~ThreadAttrFlags::PrioMask;
	} else if (n == __SPRT_SCHED_RR) {
		thread->attr.attr &= ~ThreadAttrFlags::PrioMask;
		thread->attr.attr |= ThreadAttrFlags::PrioRR;
	} else if (n == __SPRT_SCHED_FIFO) {
		thread->attr.attr &= ~ThreadAttrFlags::PrioMask;
		thread->attr.attr |= ThreadAttrFlags::PrioFifo;
	} else {
		return EINVAL;
	}

	thread->attr.prio = p->sched_priority;
	lock.unlock();

	// Update the PI base priority and recalculate under prioMutex (leaf lock).
	unique_lock prioLock(thread->prioMutex);
	thread->basePrio = p->sched_priority;
	thread->recalculateDynamicPriority(prioLock);
	return 0;
}

int thread_t::setschedprio(thread_t *thread, int p) {
	if (!thread || !native::__isNativeHandleValid(thread)
			|| hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		return ESRCH;
	}

	if (!s_handlePool.isPrioValid(thread->attr.attr, p)) {
		return EINVAL;
	}

	{
		unique_lock lock(thread->mutex);
		thread->attr.prio = p;
	}

	// Update the PI base priority and recalculate under prioMutex (leaf lock).
	unique_lock prioLock(thread->prioMutex);
	thread->basePrio = p;
	thread->recalculateDynamicPriority(prioLock);
	return 0;
}

} // namespace sprt::_thread
