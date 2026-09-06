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
#define __SPRT_BUILD 1

/* SPRT pthread native layer for Embox user mode (EL0).
 *
 * The whole pthread stack -- mutexes, condition variables, rwlocks, barriers,
 * keys, join, TLS destructors, C11 threads -- is portable and stays as-is; it
 * runs on qlock/plock, and the qlock backend for this target already exists
 * (core/embox_user/sprt_lock.cc). Only this seam is platform-specific, and of
 * its twelve hooks exactly ONE needs a kernel facility we do not have.
 *
 * SINGLE-THREADED, AND THAT IS THE POINT (phase L3a). clone(220) and futex(98)
 * are milestone M2, so `__createThread` is ENOSYS. Everything else here is a
 * real implementation for a process whose thread count is one:
 *
 *   - the thread id is the kernel's, from gettid(178);
 *   - the main thread's stack bounds are the ABI's, not zeros, so
 *     pthread_getattr_np answers truthfully;
 *   - exiting the only thread IS exiting the process, so __exitNativeThread
 *     issues exit_group(94);
 *   - TLS destructors go through __cxa_thread_atexit, which libc_impl's
 *     embox_user/terminate.cc implements.
 *
 * This is not a stub standing in for a real backend: it is what a correct
 * backend looks like when the platform can only have one thread. Phase L3b
 * replaces __createThread and switches sprt_lock from polling to futex waits;
 * nothing else in this file changes.
 */

#include "pthread_impl.h"

#if SPRT_EMBOX_USER

#include <sprt/c/cross/embox_user_sprt/aarch64_sprt/memmap.h>

#include "../include/__el0_syscall.h"

// Itanium thread-local destructor registration. clang lowers thread_local
// destructors to __cxa_thread_atexit on ELF, same as the POSIX path.
extern "C" __attribute((weak)) void *__dso_handle;
__SPRT_C_FUNC int __cxa_thread_atexit(void (*cb)(void *), void *obj,
		void *dso_symbol) __SPRT_NOEXCEPT;

// Userspace signal mask (libc_impl builtin_signal.cpp). There is no signal
// delivery at EL0 yet (phase K8), but the mask itself is real and process-local.
__SPRT_C_FUNC int sigprocmask(int how, const __SPRT_ID(sigset_t) * set,
		__SPRT_ID(sigset_t) * oldset) __SPRT_NOEXCEPT;

namespace sprt::_thread::native {

// The kernel's thread id, not a thread-local counter: gettid(178) is a real
// syscall here, and its answer is the identity every other subsystem sees
// (plock, the stdio lock owner, __libc::mainThread). Taking it from the kernel
// rather than from TLS is the same argument runtime_core_defaults.cpp makes for
// __sprt_gettid.
static uint64_t __getNativeThreadId() { return (uint64_t)__el0_gettid(); }

static void __doDestroy(void *cb) {
	auto dtor = reinterpret_cast<void (*)(void)>(cb);
	dtor();
}

static void __registerForDestruction(void (*cb)(void)) {
	__cxa_thread_atexit(__doDestroy, (void *)cb, __dso_handle);
}

// The one hook that needs the kernel. clone(220) is M2; until it lands there is
// no way to get a second EL0 context, and no amount of userspace code changes
// that.
//
// ENOSYS rather than EAGAIN deliberately: EAGAIN means "no resources, try
// later", which invites a retry loop that can never succeed. ENOSYS says the
// operation does not exist, which is the truth and which callers handle by
// falling back to synchronous work.
static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr,
		__thread_pool *pool) {
	(void)thread;
	(void)attr;
	(void)pool;
	return ENOSYS;
}

static bool __initNativeHandle(thread_t *thread) {
	// Called for the thread that is already running -- the main one, since it is
	// the only one there can be. Its stack is the one the kernel placed at
	// eret (ABI doc section 2.2), so unlike the wasm sibling (whose stack lives
	// in toolchain-managed linear memory with no queryable bounds) the real
	// numbers are known and worth reporting: pthread_getattr_np and the stack
	// checks in the pool read them.
	thread->handle = reinterpret_cast<void *>(uintptr_t(1));
	thread->attr.stack = reinterpret_cast<void *>(__SPRT_EL0_STACK_BASE);
	thread->attr.stackSize = __SPRT_EL0_STACK_TOP - __SPRT_EL0_STACK_BASE;
	thread->lowStack = __SPRT_EL0_STACK_BASE;
	thread->highStack = __SPRT_EL0_STACK_TOP;
	return true;
}

static void __closeNativeHandle(void *handle) { (void)handle; }

static bool __isNativeHandleValid(thread_t *thread) { return thread->handle != nullptr; }

// The only thread's exit is the process's exit. exit_group(94) is answered in
// the trap handler itself, which unwinds the EL0 thread rather than returning to
// it -- so this genuinely does not come back.
static void __exitNativeThread(void *ret) {
	(void)ret;
	__el0_exit_group(0);
	__builtin_unreachable();
}

// Asynchronous cancellation needs a way to interrupt another context; there is
// no other context, and no signal delivery either (K8).
static int __cancelThreadAsync(thread_t *thread) {
	(void)thread;
	return ENOSYS;
}

// No sched_setscheduler/sched_setparam syscall (119/118 are M2 at the earliest).
// Returning 0 as the wasm backend does would report success for a priority that
// was never applied; a caller that checks gets the truth instead.
static int __applyThreadPrio(thread_t *thread, int32_t dprio) {
	(void)thread;
	(void)dprio;
	return ENOSYS;
}

// --- attribute validation ---------------------------------------------------
//
// These gate what pthread_attr_set* accepts. The rule followed here: reject at
// set time only what can never work, and let what merely cannot be APPLIED fail
// at apply time -- an attribute stored and then refused by __createThread is a
// clearer error than one refused two calls earlier.

// The kernel places the stack; a caller-provided one has nowhere to be honoured.
SPRT_UNUSED static bool validate_attr_setstack(void *, size_t) { return false; }

// No guard page below a thread stack: there are no thread stacks to guard yet.
SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) {
	(void)size;
	return false;
}

// Accepted and stored; __createThread is what refuses to use it.
SPRT_UNUSED static bool validate_attr_setstacksize(size_t size) {
	(void)size;
	return true;
}

SPRT_UNUSED static bool validate_attr_setschedpolicy(int) { return true; }

SPRT_UNUSED static bool validate_attr_setschedparam(int) { return true; }

SPRT_UNUSED static bool validate_attr_setinheritsched(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprioceiling(int) { return true; }

SPRT_UNUSED static bool validate_mutexattr_setprotocol(int) { return true; }

// PROCESS_SHARED asks whether the lock backend can wait on memory another
// process also maps. Ours cannot -- and there is no second process to share with
// anyway (D5) -- but the answer is derived from the backend rather than asserted
// here, so it stays right when L3b replaces the polling qlock with futex.
SPRT_UNUSED static bool validate_mutexattr_setpshared(int v) {
	if (v == __SPRT_PTHREAD_PROCESS_SHARED) {
		return __sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0
				&& __sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_SHARED) == 0;
	}
	return true;
}

// Our qlock measures deadlines against CLOCK_MONOTONIC and ignores the flag
// (core/embox_user/sprt_lock.cc, and for a stated reason: a wall-clock step must
// not turn a bounded wait into an unbounded one). So an explicit REALTIME
// condvar would wait against the wrong clock, and the two differ by whatever the
// RTC read at boot. Refusing the request is loud and wrong-by-EINVAL; accepting
// it is quiet and wrong-by-hours. L3b makes the backend honour the flag and this
// becomes `true`.
SPRT_UNUSED static bool validate_condattr_setclock(int clock) {
	switch (clock) {
	case __SPRT_CLOCK_MONOTONIC: return true;
	case __SPRT_CLOCK_REALTIME: return false;
	default: return false;
	}
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

// No per-thread CPU clock: clock_gettime(113) accepts only REALTIME and
// MONOTONIC (core/embox_user/clock_gettime.cc), and answering MONOTONIC here
// would claim that this thread's CPU time equals elapsed time -- true only for a
// thread that never blocks, which is not a property anything can promise.
int thread_t::getcpuclockid(__sprt_clockid_t *clock) const {
	if (!clock) {
		return EINVAL;
	}
	return ENOSYS;
}

int thread_t::getaffinity(__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) {
	(void)n;
	(void)set;
	return ENOSYS;
}

int thread_t::setaffinity(__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set) {
	(void)n;
	(void)set;
	return ENOSYS;
}

// The name is kept in the thread object by the layer above; there is no kernel
// call that would also record it, so there is nothing left to do here.
int thread_t::setname_native(const char *name) {
	(void)name;
	return 0;
}

} // namespace sprt::_thread

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(
		pthread_sigmask)(int how, const __SPRT_ID(sigset_t) * set, __SPRT_ID(sigset_t) * oldset) {
	return sigprocmask(how, set, oldset);
}

} // namespace sprt

#endif // SPRT_EMBOX_USER
