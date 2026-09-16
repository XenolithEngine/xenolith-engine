
// Embox EL0 time backend.
//
// clock_gettime(113) is real and lives in runtime/core/embox_user; this unit
// covers sleeping and local time, and brings the public time/errno names into
// scope for the generic builtin_time.cpp body.
//
// SLEEPING WAS A BUSY WAIT until K6, because nanosleep(101) and
// clock_nanosleep(115) did not exist and an EL0 thread had no way to ask the
// kernel to stop running it. It spun on the monotonic clock: correct in the one
// sense that matters -- the time really had elapsed on return -- and wasteful in
// every other, which stopped being tolerable the moment threads became real and
// a sleeping thread meant a held core.
//
// Both are syscalls now. The spin is kept for one case the kernel cannot answer:
// a clock the kernel refuses (it takes MONOTONIC and REALTIME only), where
// returning ENOSYS would fail a call that used to work.

#include <time.h>
#include <errno.h>

#include "../../include/__impl_libc.h"
#include "../../../core/include/__el0_syscall.h"

namespace sprt {

static constexpr long long EL0_NS_PER_SEC = 1'000'000'000LL;

static long long __el0_now_ns(__SPRT_ID(clockid_t) clock) {
	struct __SPRT_TIMESPEC_NAME ts;
	if (__sprt_clock_gettime(clock, &ts) != 0) {
		return -1;
	}
	return (long long)ts.tv_sec * EL0_NS_PER_SEC + ts.tv_nsec;
}

// Spin on the monotonic clock until `ns` have passed. Monotonic and not
// realtime: a wall-clock step would otherwise make the sleep return early or
// never.
static void __el0_spin_ns(long long ns) {
	if (ns <= 0) {
		return;
	}
	auto start = __el0_now_ns(__SPRT_CLOCK_MONOTONIC);
	if (start < 0) {
		return; // no clock to wait against; returning beats spinning forever
	}
	auto deadline = start + ns;
	while (true) {
		auto now = __el0_now_ns(__SPRT_CLOCK_MONOTONIC);
		if (now < 0 || now >= deadline) {
			return;
		}
		// The kernel runs the timer interrupt regardless of what EL0 is doing,
		// so this loop is preemptible; there is no yield to issue.
		__asm__ __volatile__("yield" ::: "memory");
	}
}

__SPRT_C_FUNC int nanosleep(const struct __SPRT_TIMESPEC_NAME *req,
		struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}
	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= EL0_NS_PER_SEC) {
		__sprt_errno = EINVAL;
		return -1;
	}
	auto res = __el0_nanosleep(req, rem);
	if (res < 0) {
		__sprt_errno = (int)-res;
		return -1;
	}
	if (rem) {
		// Nothing interrupts the sleep -- there are no signals at EL0 yet (K8) --
		// so the remaining time is always zero.
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

__SPRT_C_FUNC int clock_nanosleep(__SPRT_ID(clockid_t) clock, int flags,
		const struct __SPRT_TIMESPEC_NAME *req,
		struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}
	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= EL0_NS_PER_SEC) {
		__sprt_errno = EINVAL;
		return -1;
	}

	if (clock == __SPRT_CLOCK_MONOTONIC || clock == __SPRT_CLOCK_REALTIME) {
		// TIMER_ABSTIME is 1 on the wire (Linux) and 2 in this runtime's
		// headers; __el0_clock_nanosleep takes the wire value.
		int wire = (flags & __SPRT_TIMER_ABSTIME) ? 1 : 0;
		auto res = __el0_clock_nanosleep((int)clock, wire, req, rem);
		if (res < 0) {
			__sprt_errno = (int)-res;
			return -1;
		}
		if (rem && !(flags & __SPRT_TIMER_ABSTIME)) {
			rem->tv_sec = 0;
			rem->tv_nsec = 0;
		}
		return 0;
	}

	// A clock the kernel does not take: the old spin, so that a call which used
	// to work does not start failing.
	long long ns;
	if (flags & __SPRT_TIMER_ABSTIME) {
		auto now = __el0_now_ns(clock);
		if (now < 0) {
			__sprt_errno = EINVAL;
			return -1;
		}
		ns = ((long long)req->tv_sec * EL0_NS_PER_SEC + req->tv_nsec) - now;
		if (ns <= 0) {
			return 0; // the deadline is already behind us
		}
	} else {
		ns = (long long)req->tv_sec * EL0_NS_PER_SEC + req->tv_nsec;
	}
	__el0_spin_ns(ns);
	if (rem && !(flags & __SPRT_TIMER_ABSTIME)) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

// No per-thread CPU clock: clock_gettime(113) accepts only REALTIME and
// MONOTONIC, and answering with either would claim this thread's CPU time equals
// elapsed time -- true only for a thread that never blocks. Same answer the
// pthread backend gives for thread_t::getcpuclockid, for the same reason.
__SPRT_C_FUNC int clock_getcpuclockid(__SPRT_ID(pid_t) pid,
		__SPRT_ID(clockid_t) *clock) __SPRT_NOEXCEPT {
	(void)pid;
	(void)clock;
	return ENOSYS; // this one reports through the return value, not errno
}

__SPRT_C_FUNC struct tm *localtime_r(const time_t *__restrict t,
		struct tm *__restrict tm) __SPRT_NOEXCEPT {
	// Local time is UTC: Embox has no timezone database and the kernel's RTC
	// reports UTC. Saying so through tm_gmt_type/tm_zone is better than pretending
	// an offset was applied.
	if (gmtime_r(t, tm)) {
		tm->tm_usec = 0;
		tm->tm_gmtoff = 0;
		tm->tm_isdst = 0;
		tm->tm_gmt_type = __sprt_gmt_local;
		tm->tm_zone = __utc;
		return tm;
	}
	return nullptr;
}

} // namespace sprt
