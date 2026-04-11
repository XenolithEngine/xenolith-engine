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

#ifndef SPRT_WINDOWS
#include <pthread.h>
#endif

#include "pthread_native_pthread.cc"
#include "pthread_native_winapi.cc"

#include <sprt/cxx/cstring>

namespace sprt::_thread {

thread_local __thread_slot tl_self;

static __thread_pool s_handlePool;

__thread_slot::__thread_slot() : tid(__sprt_gettid()) { }

__thread_slot::~__thread_slot() {
	if (tid != 0) {
		s_handlePool.activeThreads.erase(tid);
	}
	if (thread && hasFlag(thread->attr.attr, ThreadAttrFlags::Detached)) {
		// We are here because detached thread has completed execution
		// Destroy/return it's resources
		__detachAndDeallocateThread(thread, nullptr);
	}
	thread = nullptr;
}

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

	auto tid = __sprt_gettid();

	unique_lock globalLock(s_handlePool.mutex);
	s_handlePool.activeThreads.emplace(tid, thread);
	globalLock.unlock();

	thread->registerThread();

	tl_self.thread = thread;

	thread->state.set_and_signal(thread_t::StateInternalInit);
	thread->state.wait(thread_t::StateExternalInit);

	if (__sprt_setjmp(thread->jmpToRunthread) == 0) {
		thread->arg = thread->cb(thread->arg);
	}

	globalLock.lock();
	s_handlePool.activeThreads.erase(tid);
	globalLock.unlock();

	unique_lock lock(thread->mutex);

	auto result = thread->result;

	thread->state.set_and_signal(thread_t::StateFinalized);
	lock.unlock(); // unlock before mutex's memory destroyed

	return result;
}

static void __attachNativeThread(thread_t *thread, void *handle) { thread->handle = handle; }

static thread_t *__allocateThread(const attr_t *attr) {
	thread_t *handle = nullptr;
	unique_lock lock(s_handlePool.mutex);
	if (s_handlePool.free) {
		handle = s_handlePool.free;
		s_handlePool.free = handle->next;
		lock.unlock();
		handle->next = nullptr;
	} else {
		lock.unlock();
		handle = (thread_t *)__sprt_malloc(sizeof(thread_t));
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
	handle->next = s_handlePool.free;
	s_handlePool.free = handle;
}

static void __detachAndDeallocateThread(thread_t *thread, unique_lock<qmutex> *externalLock) {
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
	thread->next = s_handlePool.free;
	s_handlePool.free = thread;
}

thread_t *thread_t::self() {
	if (tl_self.thread == nullptr) {
		// make pthread_t for external thread

		auto thread = __allocateThread(nullptr);

		thread->attr.attr |= ThreadAttrFlags::Detached | ThreadAttrFlags::Unmanaged;

		thread->registerThread();

		thread->state.set_and_signal(StateExternalInit);

		tl_self.thread = thread;
	}
	return tl_self.thread;
}

thread_t *thread_t::self_noattach() { return tl_self.thread; }

void thread_t::registerThread() {
	if (threadMemPool) {
		return;
	}

	threadMemPool = memory::get_thread_support_pool();

	// read actual attributes
	native::__initNativeHandle(this);

	// setup data structs
	memory::perform([&] {
		threadRobustMutexes =
				new (threadMemPool) __pool_unordered_map<mutex_t *, mutex_info>(threadMemPool);
		threadRobustMutexes->max_load_factor(2.0f);
		memory::pool::cleanup_register(threadMemPool, threadRobustMutexes, [](void *data) {
			auto mutexes = (__pool_unordered_set<mutex_t *> *)data;
			for (auto &it : *mutexes) {
				it->force_unlock(); //
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
				it.first->force_unlock(true); //
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
				if (_atomic::fetchSub(&it.second.data->refcount, uint32_t(1)) == 1) {
					unique_lock lock(s_handlePool.mutex);
					s_handlePool.keys.erase(it.first);
				}
			}

			return Status::Ok;
		});
	}, threadMemPool);
}


void thread_t::addMutex(mutex_t *mtx, int32_t mutexPrio) {
	unique_lock lock(mutex);
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
	unique_lock lock(mutex);
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

void thread_t::removeMutex(mutex_t *mtx) {
	unique_lock lock(mutex);
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
	int32_t newPrio = attr.prio;

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
		void *(*cb)(void *), void *__SPRT_RESTRICT arg) {
	auto thread = __allocateThread(attr);
	thread->cb = cb;
	thread->arg = arg;

	auto ret = native::__createThread(thread, attr ? attr : &s_handlePool.defaultAttr);
	if (ret != 0) {
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
		// thread is now terminated,
		if (ret) {
			*ret = thread->arg;
		}

		__detachAndDeallocateThread(thread, &lock);

		return 0;
	}
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
		__SPRT_ID(sched_param) * __SPRT_RESTRICT p) {
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
		const __SPRT_ID(sched_param) * __SPRT_RESTRICT p) {
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
	thread->recalculateDynamicPriority(lock);
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

	unique_lock lock(thread->mutex);
	thread->attr.prio = p;
	thread->recalculateDynamicPriority(lock);
	return 0;
}

} // namespace sprt::_thread
