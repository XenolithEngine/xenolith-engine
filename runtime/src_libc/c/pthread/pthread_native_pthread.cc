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

/* SPRT pthread implementation over native pthreads */

#include "pthread_native.h"

#include <pthread.h>

namespace sprt::_thread::native {

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr) {
	pthread_attr_t pattr;
	pthread_attr_init(&pattr);

	// create thread as detached to use custom join implementation
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);

	if (hasFlag(attr->attr, ThreadAttrFlags::ExplicitSched)) {
		pthread_attr_setinheritsched(&pattr, PTHREAD_EXPLICIT_SCHED);

		if (hasFlag(attr->attr, ThreadAttrFlags::PrioRR)) {
			pthread_attr_setschedpolicy(&pattr, SCHED_RR);
			struct sched_param param{attr->prio};
			pthread_attr_setschedparam(&pattr, &param);
		}
	} else {
		pthread_attr_setinheritsched(&pattr, PTHREAD_INHERIT_SCHED);
	}

	if (hasFlag(attr->attr, ThreadAttrFlags::GuardSizeCustomized)) {
		pthread_attr_setguardsize(&pattr, attr->guardSize);
	}

	if (hasFlag(attr->attr, ThreadAttrFlags::StackPointerCustomized)) {
		pthread_attr_setstack(&pattr, attr->stack, attr->stackSize);
	} else if (hasFlag(attr->attr, ThreadAttrFlags::StackSizeCustomized)) {
		pthread_attr_setstacksize(&pattr, attr->stackSize);
	}

	pthread_t pthread;
	auto ret = pthread_create(&pthread, &pattr, __runthead, thread);
	if (ret == 0) {
		__attachNativeThread(thread, reinterpret_cast<void *>(pthread));
	}

	pthread_attr_destroy(&pattr);
	return ret;
}

static void __initNativeHandle(thread_t *thread) {
	thread->handle = reinterpret_cast<void *>(pthread_self());

	pthread_attr_t attr;
	pthread_getattr_np(reinterpret_cast<pthread_t>(thread->handle),
			&attr); // Get current thread's actual attributes

	size_t stackSize = 0;
	void *stackptr = nullptr;

	pthread_attr_getstack(&attr, &stackptr, &stackSize);

	int sched = 0;
	struct sched_param param;

	pthread_getschedparam(reinterpret_cast<pthread_t>(thread->handle), &sched, &param);

	switch (sched) {
	case SCHED_OTHER: thread->attr.attr &= ~ThreadAttrFlags::PrioMask; break;
	case SCHED_FIFO:
		thread->attr.attr &= ~ThreadAttrFlags::PrioMask;
		thread->attr.attr |= ThreadAttrFlags::PrioFifo;
		break;
	case SCHED_RR:
		thread->attr.attr &= ~ThreadAttrFlags::PrioMask;
		thread->attr.attr |= ThreadAttrFlags::PrioRR;
		break;
	}

	thread->dynamicPrio = thread->attr.prio = param.sched_priority;

	thread->attr.stack = stackptr;
	thread->attr.stackSize = static_cast<uint32_t>(stackSize);

	thread->lowStack = reinterpret_cast<uintptr_t>(stackptr);
	thread->highStack = thread->lowStack + stackSize;

	if (!hasFlag(thread->attr.attr, ThreadAttrFlags::Unmanaged)) {
		// if it's SPRT's thread, we need to setup async cancel to use it
		int oldv = 0;
		if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldv) == 0
				&& pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldv) == 0) {
			// set flag to notify, that thread can be cancelled with PTHREAD_CANCEL_ASYNCHRONOUS
			thread->attr.attr |= ThreadAttrFlags::CancelAsyncSupported;
		}
	}
}

static void __closeNativeHandle(void *handle) { }

static bool __isNativeHandleValid(thread_t *thread) { return thread->handle; }

static void __exitNativeThread(void *ret) { pthread_exit(ret); }

static int __applyThreadPrio(thread_t *thread, int32_t dprio) {
	struct sched_param param{dprio};

	int ipolicy = 0;

	auto policy = thread->attr.attr & ThreadAttrFlags::PrioMask;
	switch (policy) {
	case ThreadAttrFlags::PrioRR: ipolicy = SCHED_RR; break;
	case ThreadAttrFlags::PrioFifo: ipolicy = SCHED_FIFO; break;
	default: ipolicy = SCHED_OTHER; break;
	}

	return pthread_setschedparam(reinterpret_cast<pthread_t>(thread->handle), ipolicy, &param);
}

static int __cancelThreadAsync(thread_t *thread) {
	return pthread_cancel(reinterpret_cast<pthread_t>(thread->handle));
}

SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) { return true; }

SPRT_UNUSED static bool validate_attr_setstacksize(size_t size) { return true; }

SPRT_UNUSED static bool validate_attr_setstack(void *, size_t) { return true; }

SPRT_UNUSED static bool validate_attr_setschedpolicy(int) { return true; }

SPRT_UNUSED static bool validate_attr_setschedparam(int) { return true; }

SPRT_UNUSED static bool validate_attr_setinheritsched(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprioceiling(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprotocol(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_condattr_setclock(int clock) {
	switch (clock) {
	case __SPRT_CLOCK_REALTIME:
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME);
		break;
	case __SPRT_CLOCK_MONOTONIC: return true; break;
	}
	return true;
}

SPRT_UNUSED static bool validate_condattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_rwlockattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

SPRT_UNUSED static bool validate_barrierattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

} // namespace sprt::_thread::native

namespace sprt::_thread {

int thread_t::getcpuclockid(__sprt_clockid_t *clock) const {
	return pthread_getcpuclockid(reinterpret_cast<pthread_t>(handle), clock);
}

int thread_t::getaffinity(__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) {
	return pthread_getaffinity_np(reinterpret_cast<pthread_t>(handle), n,
			reinterpret_cast<cpu_set_t *>(set));
}

int thread_t::setaffinity(__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set) {
	return pthread_setaffinity_np(reinterpret_cast<pthread_t>(handle), n,
			reinterpret_cast<const cpu_set_t *>(set));
}

int thread_t::setname_native(const char *name) {
	return pthread_setname_np(reinterpret_cast<pthread_t>(handle), name);
}

} // namespace sprt::_thread
