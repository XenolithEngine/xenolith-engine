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

/* SPRT pthread native layer for WebAssembly.
 *
 * The whole pthread stack (mutex/cond/rwlock/keys/join/TLS-dtors on qlock/plock)
 * is portable and stays as-is; only this thin native seam is wasm-specific.
 *
 * SKELETON SCOPE (milestone 1, single-threaded): spawning is ENOSYS. Real thread
 * creation is deferred to the wasi-threads milestone (wasm-port-draft.adoc §3.2):
 * `__createThread` will forward to `wasi:thread-spawn(start_arg)` with the export
 * `wasi_thread_start` as the entry, writing the host tid into the thread so
 * `__sprt_gettid`/plock agree; the browser transport emulates the same contract
 * through a `thread_spawn` Worker broker. Everything below the spawn seam already
 * works over the wasm-atomics futex (sprt_lock.cc). Current-thread operations are
 * functional so single-threaded startup, TLS destructors and joins-of-none run.
 */

#include "pthread_impl.h"

#if SPRT_WASM

// Itanium thread-local destructor registration. clang lowers thread_local dtors
// to __cxa_thread_atexit on wasm, same as the POSIX path.
extern "C" __attribute((weak)) void *__dso_handle;
__SPRT_C_FUNC int __cxa_thread_atexit(void (*cb)(void *), void *obj,
		void *dso_symbol) __SPRT_NOEXCEPT;

// Userspace signal emulation (builtin_signal.cpp) provides sigprocmask.
__SPRT_C_FUNC int sigprocmask(int how, const __SPRT_ID(sigset_t) * set,
		__SPRT_ID(sigset_t) * oldset) __SPRT_NOEXCEPT;

extern "C" {
// T2 host imports (browser transport) / no-ops under a native wasi host.
//
// thread_spawn launches a new OS thread: the browser broker creates a Worker that
// instantiates this same module over the shared memory, sets the instance's stack pointer
// to `stackTop`, runs __wasm_init_tls, and calls __xl_thread_entry(tid, threadPtr). It
// returns the new thread id (>= 2), or a negative errno. thread_exit ends the current
// worker after TLS destructors have run.
__attribute__((import_module("sprt"), import_name("thread_spawn"))) int __sprt_host_thread_spawn(
		void *threadPtr, void *stackTop, __SPRT_ID(size_t) stackSize, void *tlsBase);
__attribute__((import_module("sprt"), import_name("thread_exit"))) void __sprt_host_thread_exit();
}

// The stack the creator hands each spawned thread (its instance's __stack_pointer is set
// here by the broker). 1 MiB matches the pthread default guardless stack.
#ifndef __SPRT_WASM_THREAD_STACK
#define __SPRT_WASM_THREAD_STACK (1u << 20)
#endif

extern "C" void *malloc(__SPRT_ID(size_t));
extern "C" void free(void *);

// sprt::_thread::__runthead (declared in pthread_impl.h, defined later in this TU) is the
// portable thread trampoline the spawned worker jumps into via __xl_thread_entry.

// Per-thread native id. The main entry thread keeps 1; a spawned thread stamps its broker
// id here from __xl_thread_entry so __getNativeThreadId / plock agree across the pool.
static _Thread_local __sprt_uint64_t tl_wasm_native_tid = 1;

namespace sprt::_thread::native {

// The main entry thread has native id 1; a spawned thread stamps its broker id into the
// thread-local from __xl_thread_entry, so this reports the calling thread's own id.
static uint64_t __getNativeThreadId() { return tl_wasm_native_tid; }

static void __doDestroy(void *cb) {
	auto dtor = reinterpret_cast<void (*)(void)>(cb);
	dtor();
}

static void __registerForDestruction(void (*cb)(void)) {
	__cxa_thread_atexit(__doDestroy, (void *)cb, __dso_handle);
}

static int __createThread(thread_t *thread, const attr_t *__SPRT_RESTRICT attr,
		__thread_pool *pool) {
	// Allocate the new thread's stack in the shared linear memory; the broker sets the
	// spawned instance's __stack_pointer to its top before entering __xl_thread_entry.
	__SPRT_ID(size_t)
	stackSize = (attr && attr->stackSize) ? attr->stackSize
										  : (__SPRT_ID(size_t))__SPRT_WASM_THREAD_STACK;
	void *stackBase = malloc(stackSize);
	if (!stackBase) {
		return EAGAIN;
	}
	void *stackTop = (char *)stackBase + stackSize;
	// Allocate this thread's TLS block in the creator (it needs a working allocator, which
	// the not-yet-initialized new thread lacks); the worker just runs __wasm_init_tls on it.
	__SPRT_ID(size_t) tlsSize = __builtin_wasm_tls_size();
	__SPRT_ID(size_t) tlsAlign = __builtin_wasm_tls_align();
	void *tlsBase = nullptr;
	if (tlsSize) {
		void *tlsRaw = malloc(tlsSize + tlsAlign);
		if (!tlsRaw) {
			free(stackBase);
			return EAGAIN;
		}
		uintptr_t aligned =
				(reinterpret_cast<uintptr_t>(tlsRaw) + (tlsAlign - 1)) & ~(uintptr_t)(tlsAlign - 1);
		tlsBase = reinterpret_cast<void *>(aligned);
	}
	// Register the thread in the pool's active table BEFORE the spawned worker can
	// run, mirroring the pthread/winapi backends. The worker's early self()/gettid
	// (before __runthead sets tl_self) looks itself up here; without the entry it
	// falls back to attachExternalThread and gets a DIFFERENT thread_t whose
	// threadMemPool is NULL (the worker's pool subsystem is not yet initialized).
	// Holding pool->mutex across spawn+attach makes that lookup (which takes the
	// same mutex) block until the thread is registered.
	unique_lock globalLock(pool->mutex);

	int tid = __sprt_host_thread_spawn(thread, stackTop, stackSize, tlsBase);
	if (tid < 0) {
		globalLock.unlock();
		free(stackBase);
		return EAGAIN;
	}
	thread->handle = reinterpret_cast<void *>(uintptr_t(tid));
	thread->attr.stack = stackBase; // freed on join/detach teardown
	thread->attr.stackSize = stackSize;
	thread->lowStack = reinterpret_cast<uintptr_t>(stackBase);
	thread->highStack = reinterpret_cast<uintptr_t>(stackTop);

	__attachNativeThread(thread, thread->handle, static_cast<uint64_t>(static_cast<unsigned>(tid)),
			globalLock);
	globalLock.unlock();
	return 0;
}

static bool __initNativeHandle(thread_t *thread) {
	// A wasm thread's stack lives in linear memory managed by the toolchain; there
	// is no queryable native handle, so use a sentinel and leave stack bounds unset
	// (the pool-level bookkeeping does not depend on them in the single-thread path).
	thread->handle = reinterpret_cast<void *>(uintptr_t(1));
	thread->attr.stack = nullptr;
	thread->attr.stackSize = 0;
	thread->lowStack = 0;
	thread->highStack = 0;
	return true;
}

static void __closeNativeHandle(void *handle) { }

static bool __isNativeHandleValid(thread_t *thread) { return thread->handle != nullptr; }

static void __exitNativeThread(void *ret) {
	__sprt_host_thread_exit();
	__builtin_unreachable();
}

static int __cancelThreadAsync(thread_t *thread) { return ENOSYS; }

static int __applyThreadPrio(thread_t *thread, int32_t dprio) { return 0; }

SPRT_UNUSED static bool validate_attr_setguardsize(size_t size) { return false; }

SPRT_UNUSED static bool validate_attr_setstacksize(size_t size) { return true; }

SPRT_UNUSED static bool validate_attr_setstack(void *, size_t) { return false; }

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
	if (!clock) {
		return EINVAL;
	}
	// No per-thread CPU clock; report the thread CPU-time clock id so callers get a
	// usable (monotonic-approximated) value from clock_gettime.
	*clock = __SPRT_CLOCK_THREAD_CPUTIME_ID;
	return 0;
}

int thread_t::getaffinity(__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) { return ENOSYS; }

int thread_t::setaffinity(__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set) { return ENOSYS; }

int thread_t::setname_native(const char *name) { return 0; }

} // namespace sprt::_thread

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(
		pthread_sigmask)(int how, const __SPRT_ID(sigset_t) * set, __SPRT_ID(sigset_t) * oldset) {
	return sigprocmask(how, set, oldset);
}

} // namespace sprt

extern "C" __SPRT_ID(pid_t) __sprt_wasm_gettid(void) {
	return static_cast<__SPRT_ID(pid_t)>(tl_wasm_native_tid);
}

// Broker entry for a spawned thread. The worker sets this instance's __stack_pointer and
// runs __wasm_init_tls, then calls here in the new thread: stamp the native tid and run the
// portable thread trampoline. When __runthead returns the thread has finalized (state
// signalled, activeThreads erased) and the worker may terminate.
extern "C" __attribute__((export_name("__xl_thread_entry"))) void __xl_thread_entry(int tid,
		void *threadPtr) {
	tl_wasm_native_tid = (__sprt_uint64_t)(unsigned)tid;
	sprt::_thread::__runthead(threadPtr);
}

#endif // SPRT_WASM
