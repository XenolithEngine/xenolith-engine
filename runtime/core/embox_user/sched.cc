
// Embox EL0 scheduler backend.
//
// Embox HAS a priority scheduler -- the hosted target uses it through
// pthread_setschedparam -- but no syscall reaches it from EL0 (sched_yield is
// 124, sched_setscheduler 119, both M2 or later). So every setter is ENOSYS and
// the priority bounds collapse to a single band: reporting a range the caller
// cannot actually select within would invite a setter call that then fails.
//
// sched_yield spins a `yield` hint. That is not a scheduling operation -- the
// kernel's timer interrupt is what actually preempts this thread -- but it is
// the correct instruction to execute while waiting, and it lets the caller's
// spin loop be written as if a yield existed. Same posture as time.cc's sleep.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

static constexpr int EL0_SCHED_PRIO_MIN = 0;
static constexpr int EL0_SCHED_PRIO_MAX = 0;

__SPRT_C_FUNC int sched_get_priority_max(int) __SPRT_NOEXCEPT { return EL0_SCHED_PRIO_MAX; }
__SPRT_C_FUNC int sched_get_priority_min(int) __SPRT_NOEXCEPT { return EL0_SCHED_PRIO_MIN; }

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
	__asm__ __volatile__("yield" ::: "memory");
	return 0;
}

} // namespace sprt
