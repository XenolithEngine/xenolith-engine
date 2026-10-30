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

// The native thread layer on Tau. libtau makes the thread (root's thread objects, its own
// notification) and tells when it is gone; the stack and the TLS block are the runtime's,
// freed once the thread is gone. A thread that ends detached cannot free the stack it
// stands on: it is left for the next create or join to reap.

#include "pthread_impl.h"

#if SPRT_TAU

#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/c/__sprt_stdlib.h>

// libc_impl/src/tau/startup.cc: a copy of the program's PT_TLS image for a new thread,
// and the main thread's stack.
extern "C" void *__tau_tls_alloc(void);
extern "C" bool __tau_tls_used(void);
extern "C" void __tau_tls_free(void *tp);
extern "C" void __tau_main_stack(__sprt_uintptr_t *low, __sprt_uintptr_t *high);

extern "C" __attribute((weak)) void *__dso_handle;
__SPRT_C_FUNC int __cxa_thread_atexit(void (*cb)(void *), void *obj, void *dso_symbol) __SPRT_NOEXCEPT;

__SPRT_C_FUNC int sigprocmask(int how, const __SPRT_ID(sigset_t) * set, __SPRT_ID(sigset_t) * oldset) __SPRT_NOEXCEPT;

namespace sprt::_thread::native {

static uint64_t __getNativeThreadId() { return (uint64_t)__sprt_tau_thread_self(); }

static void __doDestroy(void *cb) {
	auto dtor = reinterpret_cast<void (*)(void)>(cb);
	dtor();
}

static void __registerForDestruction(void (*cb)(void)) { __cxa_thread_atexit(__doDestroy, (void *)cb, __dso_handle); }

#define TAU_PAGE 4'096u
#define TAU_THREAD_STACK_DEFAULT (128u * 1'024u)
#define TAU_MAIN_THREAD 1u

struct tau_thread_rec {
	__sprt_tau_thread *thread;
	unsigned id;
	void *stack;
	__SPRT_ID(size_t) stackSize;
	void *tls;
	tau_thread_rec *next;
};

static tau_thread_rec *s_tau_zombies = nullptr;

static void __tau_release(tau_thread_rec *rec) {
	if (rec->thread) {
		__sprt_tau_thread_release(rec->thread);
	}
	if (rec->tls) {
		__tau_tls_free(rec->tls);
	}
	if (rec->stack) {
		__sprt_tau_mem_unmap(rec->stack, rec->stackSize);
	}
	__sprt_free(rec);
}

// With the handle pool's mutex held.
static void __tau_reap(void) {
	tau_thread_rec **link = &s_tau_zombies;
	while (*link) {
		tau_thread_rec *rec = *link;
		if (__sprt_tau_thread_wait(rec->thread, 0) == 0) {
			*link = rec->next;
			__tau_release(rec);
		} else {
			link = &rec->next;
		}
	}
}

static void __tau_run(void *thread) { __runthead(thread); }

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr, __thread_pool *pool) {
	__SPRT_ID(size_t) stackSize = (attr && attr->stackSize) ? attr->stackSize : TAU_THREAD_STACK_DEFAULT;
	stackSize = (stackSize + TAU_PAGE - 1) & ~(__SPRT_ID(size_t))(TAU_PAGE - 1);

	auto rec = (tau_thread_rec *)__sprt_calloc(1, sizeof(tau_thread_rec));
	if (!rec) {
		return EAGAIN;
	}
	void *stack = nullptr;
	if (__sprt_tau_mem_map(stackSize, TAU_PAGE, __SPRT_TAU_MEM_GUARD, &stack) != 0) {
		__sprt_free(rec);
		return EAGAIN;
	}
	rec->stack = stack;
	rec->stackSize = stackSize;
	rec->tls = __tau_tls_alloc();
	if (!rec->tls && __tau_tls_used()) {
		__tau_release(rec);
		return EAGAIN;
	}
	auto stackTop = (char *)stack + stackSize;

	// Registered before the thread runs: it looks itself up by id when it starts.
	unique_lock globalLock(pool->mutex);
	__tau_reap();
	int st = __sprt_tau_thread_create(&__tau_run, thread, stackTop, rec->tls, -1, &rec->thread);
	if (st != 0) {
		rec->thread = nullptr;
		globalLock.unlock();
		__tau_release(rec);
		return -st;
	}
	rec->id = __sprt_tau_thread_id(rec->thread);

	thread->handle = rec;
	thread->attr.stack = stack;
	thread->attr.stackSize = (uint32_t)stackSize;
	thread->attr.guardSize = TAU_PAGE;
	thread->lowStack = (uintptr_t)stack;
	thread->highStack = (uintptr_t)stackTop;

	__attachNativeThread(thread, thread->handle, rec->id, globalLock);
	globalLock.unlock();
	return 0;
}

static bool __initNativeHandle(thread_t *thread) {
	if (__sprt_tau_thread_self() != TAU_MAIN_THREAD) {
		return true;
	}
	__sprt_uintptr_t low = 0, high = 0;
	__tau_main_stack(&low, &high);
	thread->handle = reinterpret_cast<void *>(uintptr_t(1));
	thread->attr.stack = reinterpret_cast<void *>(low);
	thread->attr.stackSize = uint32_t(high - low);
	thread->lowStack = low;
	thread->highStack = high;
	return true;
}

// With the handle pool's mutex held: on the joiner for a joined thread, on the thread itself for a
// detached one. A thread not gone yet - one standing on the stack it would free, or a joined one
// still running its TLS destructors - is left for the next create or join to reap: waiting for it
// here would hold the mutex every thread and key operation of the module takes.
static void __closeNativeHandle(void *handle) {
	if (!handle || handle == reinterpret_cast<void *>(uintptr_t(1))) {
		return;
	}
	auto rec = (tau_thread_rec *)handle;
	if (rec->id != __sprt_tau_thread_self() && __sprt_tau_thread_wait(rec->thread, 0) == 0) {
		__tau_release(rec);
	} else {
		rec->next = s_tau_zombies;
		s_tau_zombies = rec;
	}
	__tau_reap();
}

static bool __isNativeHandleValid(thread_t *thread) { return thread->handle != nullptr; }

// The main thread's end is the module's.
static void __exitNativeThread(void *) {
	if (__sprt_tau_thread_self() == TAU_MAIN_THREAD) {
		__sprt_tau_exit(0);
	}
	__sprt_tau_thread_exit();
}

static int __cancelThreadAsync(thread_t *) { return ENOSYS; }

// Priorities come from the manifest; root bounds a thread's by its module's.
static int __applyThreadPrio(thread_t *, int32_t) { return ENOSYS; }

// The stack is mapped by __createThread with one guard page.
SPRT_UNUSED static bool validate_attr_setstack(void *, size_t) { return false; }

SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) { return size <= TAU_PAGE; }

SPRT_UNUSED static bool validate_attr_setstacksize(size_t) { return true; }

SPRT_UNUSED static bool validate_attr_setschedpolicy(int) { return true; }

SPRT_UNUSED static bool validate_attr_setschedparam(int) { return true; }

SPRT_UNUSED static bool validate_attr_setinheritsched(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprioceiling(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprotocol(int) { return true; }

// The futex is private to the module: nothing is shared with another process.
SPRT_UNUSED static bool validate_mutexattr_setpshared(int v) { return v != __SPRT_PTHREAD_PROCESS_SHARED; }

// One clock, the counter since boot.
SPRT_UNUSED static bool validate_condattr_setclock(int clock) { return clock == __SPRT_CLOCK_MONOTONIC; }

SPRT_UNUSED static bool validate_condattr_setpshared(int v) { return v != __SPRT_PTHREAD_PROCESS_SHARED; }

SPRT_UNUSED static bool validate_rwlockattr_setpshared(int v) { return v != __SPRT_PTHREAD_PROCESS_SHARED; }

SPRT_UNUSED static bool validate_barrierattr_setpshared(int v) { return v != __SPRT_PTHREAD_PROCESS_SHARED; }

} // namespace sprt::_thread::native

namespace sprt::_thread {

int thread_t::getcpuclockid(__sprt_clockid_t *clock) const { return clock ? ENOSYS : EINVAL; }

// Threads run on the CPUs of the module's manifest.
int thread_t::getaffinity(__SPRT_ID(size_t), __SPRT_ID(cpu_set_t) *) { return ENOSYS; }

int thread_t::setaffinity(__SPRT_ID(size_t), const __SPRT_ID(cpu_set_t) *) { return ENOSYS; }

int thread_t::setname_native(const char *) { return 0; }

} // namespace sprt::_thread

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(pthread_sigmask)(int how, const __SPRT_ID(sigset_t) * set, __SPRT_ID(sigset_t) * oldset) {
	return sigprocmask(how, set, oldset);
}

} // namespace sprt

#endif // SPRT_TAU
