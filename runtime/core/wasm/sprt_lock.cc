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

// WebAssembly futex backend (wasm-port-draft.adoc §3.1).
//
// The qlock/rlock wait+wake primitives map directly onto the wasm threads
// atomic instructions — no host import is needed (T0 LOCAL):
//   * memory.atomic.wait32/wait64  (__builtin_wasm_memory_atomic_wait32/64)
//   * memory.atomic.notify         (__builtin_wasm_memory_atomic_notify)
// These require -matomics and a shared linear memory (--shared-memory /
// --import-memory at link time). memory.atomic.wait TRAPS on non-shared memory
// and is illegal on the browser main thread; a single-threaded or main-thread
// build must therefore route through the spin fallback (a later milestone —
// see the draft). rlock degrades exactly like Windows/Darwin: no priority
// inheritance, waiting on the full 64-bit word.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

// __builtin_wasm_memory_atomic_wait32/64 return: 0 = woken by notify,
// 1 = value did not equal expected (returned immediately), 2 = timed out.
static constexpr int __WASM_WAIT_OK = 0;
static constexpr int __WASM_WAIT_NOT_EQUAL = 1;
static constexpr int __WASM_WAIT_TIMED_OUT = 2;

// __SPRT_SPRT_TIMEOUT_INFINITE ns -> -1 (wait forever) for the wasm builtins.
static long long __wasm_wait_timeout(__SPRT_ID(sprt_timeout_t) timeout) {
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		return -1;
	}
	return static_cast<long long>(timeout);
}

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// Only process-private waits: a single shared linear memory is the whole
	// address space, so there is no cross-process shared-lock notion.
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int r = __builtin_wasm_memory_atomic_wait32(reinterpret_cast<int *>(value),
			static_cast<int>(expected), __wasm_wait_timeout(timeout));
	if (r == __WASM_WAIT_TIMED_OUT) {
		__sprt_errno = ETIMEDOUT;
		return -1;
	}
	// __WASM_WAIT_OK / __WASM_WAIT_NOT_EQUAL: the caller re-checks the word, so a
	// stale-value early return is treated as a (spurious) wakeup, not an error.
	return 0;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	__builtin_wasm_memory_atomic_notify(reinterpret_cast<int *>(value), 1);
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	__builtin_wasm_memory_atomic_notify(reinterpret_cast<int *>(value), 0x7FFF'FFFF);
	return 0;
}

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int r = __builtin_wasm_memory_atomic_wait64(reinterpret_cast<long long *>(&value->u64),
			static_cast<long long>(expected->u64), __wasm_wait_timeout(timeout));
	if (r == __WASM_WAIT_TIMED_OUT) {
		__sprt_errno = ETIMEDOUT;
		return -1;
	}
	return 0;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	__sprt_errno = EBUSY;
	return -1;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	_atomic::storeSeq(&value->u64, __SPRT_ID(uint64_t)(0));
	__builtin_wasm_memory_atomic_notify(reinterpret_cast<int *>(&value->u64), 1);
	return 0;
}

static __SPRT_ID(clockid_t) sprt_qlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
}

static __SPRT_ID(clockid_t) sprt_rlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
}

} // namespace sprt
