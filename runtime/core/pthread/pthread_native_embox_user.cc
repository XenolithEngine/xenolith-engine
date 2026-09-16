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
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_stdlib.h>

#include "../include/__el0_syscall.h"

// libc_impl/src/embox_user/startup.cc: a per-thread copy of the program's
// PT_TLS image, built the same way the main thread's was. Null when the program
// has no thread_local data.
extern "C" void *__el0_tls_alloc(void);
extern "C" void __el0_tls_free(void *tp);

// libc_impl/asm/EmboxUser/aarch64/clone.s: the one place where parent and child
// are the same instruction stream.
extern "C" long __el0_clone_thread(void *stack_top, void *tls, int *ctid,
		void *(*entry)(void *), void *arg);

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

// What the parent has to remember about a thread it started, because the two
// things that must be given back -- the stack and the TLS block -- are mappings
// the thread is still standing on when it announces that it is finished.
//
// `ctid` is the word the kernel zeroes and wakes AFTER the thread's last
// instruction (clone's CLONE_CHILD_CLEARTID). Waiting on it is the only way to
// know the mappings are free; the pthread layer's own StateFinalized is set by
// the thread itself, while it is still running.
// The ABI fixes a 4 KiB granule for every board (docs/EMBOX-SYSCALL-ABI.md
// section 2.2), and mmap rounds to it.
#define EL0_PAGE 4096u

struct el0_thread {
	int ctid;
	int tid;
	void *stack;
	__SPRT_ID(size_t) stackSize;
	void *tls;
	el0_thread *next;
};

// Threads that ended detached: they cannot free their own stack, because they
// are running on it when the last hook fires. Somebody else does it later, and
// "later" is safe because ctid says when.
static el0_thread *s_el0_zombies = nullptr;

static void __el0_release(el0_thread *rec) {
	if (rec->tls) {
		__el0_tls_free(rec->tls);
	}
	if (rec->stack) {
		__el0_munmap(rec->stack, rec->stackSize);
	}
	__sprt_free(rec);
}

// Wait until the kernel says the thread is gone. EAGAIN means it already was.
static void __el0_wait_gone(el0_thread *rec) {
	while (__atomic_load_n(&rec->ctid, __ATOMIC_SEQ_CST) != 0) {
		__el0_futex(reinterpret_cast<__SPRT_ID(uint32_t) *>(&rec->ctid),
				__SPRT_FUTEX_WAIT | __SPRT_FUTEX_PRIVATE_FLAG, 1, nullptr, 0);
	}
}

// Called with the handle pool's mutex held, so the list needs no lock of its
// own; both the pusher and the reaper are inside it.
static void __el0_reap(void) {
	el0_thread **link = &s_el0_zombies;

	while (*link) {
		el0_thread *rec = *link;
		if (__atomic_load_n(&rec->ctid, __ATOMIC_SEQ_CST) == 0) {
			*link = rec->next;
			__el0_release(rec);
		}
		else {
			link = &rec->next;
		}
	}
}

// Default stack for a thread the caller did not size. Small next to the main
// thread's 8 MiB: these are the engine's pool workers, and the address space
// they come out of is an arena shared with every other mapping.
#define EL0_THREAD_STACK_DEFAULT (256u * 1024u)

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr,
		__thread_pool *pool) {
	__SPRT_ID(size_t) stackSize = (attr && attr->stackSize) ? attr->stackSize
															: EL0_THREAD_STACK_DEFAULT;
	__SPRT_ID(size_t) guardSize = (attr && attr->guardSize) ? attr->guardSize : EL0_PAGE;

	stackSize = (stackSize + EL0_PAGE - 1) & ~(__SPRT_ID(size_t))(EL0_PAGE - 1);
	guardSize = (guardSize + EL0_PAGE - 1) & ~(__SPRT_ID(size_t))(EL0_PAGE - 1);

	auto rec = (el0_thread *)__sprt_calloc(1, sizeof(el0_thread));
	if (!rec) {
		return EAGAIN;
	}

	auto total = stackSize + guardSize;
	auto raw = __el0_mmap(nullptr, total, __SPRT_PROT_READ | __SPRT_PROT_WRITE,
			__SPRT_MAP_PRIVATE | __SPRT_MAP_ANONYMOUS, -1, 0);
	if (__el0_is_err(raw)) {
		__sprt_free(rec);
		return EAGAIN;
	}

	// The guard is the lowest page of the mapping: a stack that runs past its
	// end faults instead of quietly writing over whatever is mapped below.
	if (guardSize
			&& __el0_is_err(__el0_mprotect((void *)raw, guardSize, __SPRT_PROT_NONE))) {
		__el0_munmap((void *)raw, total);
		__sprt_free(rec);
		return EAGAIN;
	}

	rec->stack = (void *)raw;
	rec->stackSize = total;
	rec->tls = __el0_tls_alloc();
	rec->ctid = 1;

	auto stackTop = (char *)raw + total;

	// Registered before the child can run, as every other backend does: the
	// child looks itself up by tid the moment it starts, and a lookup that
	// misses builds a second, crippled thread_t.
	unique_lock globalLock(pool->mutex);

	__el0_reap();

	long tid = __el0_clone_thread(stackTop, rec->tls, &rec->ctid, &__runthead, thread);
	if (tid < 0) {
		globalLock.unlock();
		__el0_release(rec);
		return (int)-tid;
	}
	rec->tid = (int)tid;

	thread->handle = rec;
	thread->attr.stack = (void *)((char *)raw + guardSize);
	thread->attr.stackSize = (uint32_t)stackSize;
	thread->attr.guardSize = (uint32_t)guardSize;
	thread->lowStack = (uintptr_t)raw + guardSize;
	thread->highStack = (uintptr_t)stackTop;

	__attachNativeThread(thread, thread->handle, (uint64_t)tid, globalLock);
	globalLock.unlock();

	return 0;
}

// The main thread's tid, learned when its handle is initialised. It is what
// tells "this thread is ending" from "the program is ending".
static int s_el0_main_tid = 0;

static bool __initNativeHandle(thread_t *thread) {
	// Called for the thread that is already running -- the main one, since it is
	// the only one there can be. Its stack is the one the kernel placed at
	// eret (ABI doc section 2.2), so unlike the wasm sibling (whose stack lives
	// in toolchain-managed linear memory with no queryable bounds) the real
	// numbers are known and worth reporting: pthread_getattr_np and the stack
	// checks in the pool read them.
	s_el0_main_tid = (int)__el0_gettid();
	thread->handle = reinterpret_cast<void *>(uintptr_t(1));
	thread->attr.stack = reinterpret_cast<void *>(__SPRT_EL0_STACK_BASE);
	thread->attr.stackSize = __SPRT_EL0_STACK_TOP - __SPRT_EL0_STACK_BASE;
	thread->lowStack = __SPRT_EL0_STACK_BASE;
	thread->highStack = __SPRT_EL0_STACK_TOP;
	return true;
}

// Runs on the joiner for a joined thread, and ON THE THREAD ITSELF for a
// detached one (pthread.cc: __runthead calls __detachAndDeallocateThread). The
// two cases cannot share an answer: a thread standing on the stack it would
// unmap has to hand the job over instead.
static void __closeNativeHandle(void *handle) {
	if (!handle || handle == reinterpret_cast<void *>(uintptr_t(1))) {
		return; // the main thread: its stack is the kernel's, not ours
	}

	auto rec = (el0_thread *)handle;

	if (rec->tid == (int)__el0_gettid()) {
		rec->next = s_el0_zombies;
		s_el0_zombies = rec;
		return;
	}

	__el0_wait_gone(rec);
	__el0_release(rec);
	__el0_reap();
}

static bool __isNativeHandleValid(thread_t *thread) { return thread->handle != nullptr; }

// exit(93) ends this thread, exit_group(94) ends the program -- since K6 the
// kernel tells them apart, so the main thread's exit is still the process's and
// nobody else's takes the program down with it.
static void __exitNativeThread(void *ret) {
	(void)ret;
	if ((int)__el0_gettid() == s_el0_main_tid) {
		__el0_exit_group(0);
	}
	__el0_exit(0);
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

// A guard page is one mprotect(PROT_NONE) below the stack __createThread maps.
SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) {
	(void)size;
	return true;
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
