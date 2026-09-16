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

// WebAssembly clock backend.
//
// The wall clock and monotonic clock come from a single typed host import
// (T1 SYNC, see wasm-port-draft.adoc §3.5): `clock_now(clkid) -> f64` returns
// nanoseconds. REALTIME maps to Date.now()*1e6, MONOTONIC to
// performance.now()+timeOrigin. CPU-time clocks have no wasm equivalent and are
// approximated by the monotonic clock. Nothing here traps, so it is usable from
// both a worker and the main thread.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/cross/__sprt_syscall.h>

extern "C" {

// T1 host import: nanoseconds for the given clock id (see JS `clock_now`).
__attribute__((import_module("sprt"), import_name("clock_now"))) double __sprt_host_clock_now(
		int clkid);

// T1 host import: clock resolution in nanoseconds.
__attribute__((import_module("sprt"), import_name("clock_res"))) double __sprt_host_clock_res(
		int clkid);
}

namespace sprt {

static void __wasm_ns_to_timespec(double ns, struct __SPRT_TIMESPEC_NAME *tp) {
	// 2^53 ns is ~104 days; f64 keeps ns precision well past any realistic uptime
	// and up to year ~2255 for the wall clock, so no BigInt round-trip is needed.
	double sec = __builtin_floor(ns / 1'000'000'000.0);
	tp->tv_sec = static_cast<decltype(tp->tv_sec)>(sec);
	tp->tv_nsec = static_cast<decltype(tp->tv_nsec)>(ns - sec * 1'000'000'000.0);
}

__SPRT_C_FUNC int clock_getres(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__wasm_ns_to_timespec(__sprt_host_clock_res(static_cast<int>(clk_id)), tp);
	return 0;
}

__SPRT_C_FUNC int clock_gettime(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	__wasm_ns_to_timespec(__sprt_host_clock_now(static_cast<int>(clk_id)), tp);
	return 0;
}

// The host has no notion of a settable wall clock in a sandbox.
static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	__sprt_errno = ENOSYS;
	return -1;
}

} // namespace sprt
