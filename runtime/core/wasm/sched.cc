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

// WebAssembly scheduler backend.
//
// wasi-threads offers no scheduling control (priorities, affinity, policies) —
// exactly like Android — so every setter is ENOSYS and the priority bounds
// collapse to a single band. sched_yield spins a hint (there is no host yield in
// a synchronous wasm call); a cooperative yield only matters for the futex spin
// fallback used on the main thread where Atomics.wait is unavailable.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

// wasi-threads exposes no priority band; report a single degenerate range.
static constexpr int __SPRT_WASM_SCHED_PRIO_MIN = 0;
static constexpr int __SPRT_WASM_SCHED_PRIO_MAX = 0;

__SPRT_C_FUNC int sched_get_priority_max(int t) __SPRT_NOEXCEPT {
	return __SPRT_WASM_SCHED_PRIO_MAX;
}
__SPRT_C_FUNC int sched_get_priority_min(int t) __SPRT_NOEXCEPT {
	return __SPRT_WASM_SCHED_PRIO_MIN;
}

__SPRT_C_FUNC int sched_getparam(__SPRT_ID(pid_t) pid, struct __SPRT_SCHED_PARAM_NAME *p) __SPRT_NOEXCEPT {
	if (!p) {
		__sprt_errno = EINVAL;
		return -1;
	}
	p->sched_priority = 0;
	return 0;
}

__SPRT_C_FUNC int sched_getscheduler(__SPRT_ID(pid_t) pid) __SPRT_NOEXCEPT {
	return __SPRT_SCHED_OTHER;
}

__SPRT_C_FUNC int sched_rr_get_interval(__SPRT_ID(pid_t) pid,
		__SPRT_TIMESPEC_NAME *t) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_setparam(__SPRT_ID(pid_t) pid,
		const struct __SPRT_SCHED_PARAM_NAME *p) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_setscheduler(__SPRT_ID(pid_t) pid, int t,
		const struct __SPRT_SCHED_PARAM_NAME *p) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_yield(void) __SPRT_NOEXCEPT { return 0; }

} // namespace sprt
