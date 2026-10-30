/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
    10|    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// The scheduler on Tau: a module's priority comes from its manifest and root, so the
// setters are ENOSYS and the bounds are one band; sched_yield yields the CPU.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_sched.h>

#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

static constexpr int TAU_SCHED_PRIO_MIN = 0;
static constexpr int TAU_SCHED_PRIO_MAX = 0;

__SPRT_C_FUNC int sched_get_priority_max(int) __SPRT_NOEXCEPT { return TAU_SCHED_PRIO_MAX; }
__SPRT_C_FUNC int sched_get_priority_min(int) __SPRT_NOEXCEPT { return TAU_SCHED_PRIO_MIN; }

__SPRT_C_FUNC int sched_getparam(__SPRT_ID(pid_t),
		struct __SPRT_SCHED_PARAM_NAME *p) __SPRT_NOEXCEPT {
	if (!p) {
		__sprt_errno = EINVAL;
		return -1;
	}
	p->sched_priority = 0;
	return 0;
}

__SPRT_C_FUNC int sched_setparam(__SPRT_ID(pid_t),
		const struct __SPRT_SCHED_PARAM_NAME *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_getscheduler(__SPRT_ID(pid_t)) __SPRT_NOEXCEPT {
	return __SPRT_SCHED_OTHER;
}

__SPRT_C_FUNC int sched_setscheduler(__SPRT_ID(pid_t), int,
		const struct __SPRT_SCHED_PARAM_NAME *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_rr_get_interval(__SPRT_ID(pid_t),
		struct __SPRT_TIMESPEC_NAME *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int sched_yield(void) __SPRT_NOEXCEPT {
	// A real syscall since K6: the kernel moves this thread to the end of its
	// run queue. The `yield` hint it replaced did nothing the scheduler could
	// see -- it only asked the core to be polite to its other hardware thread.
	__sprt_tau_yield();
	return 0;
}

} // namespace sprt
