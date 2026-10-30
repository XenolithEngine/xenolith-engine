// Sleeping on Tau goes through libtau and tau.timer@1 (sprt_tau.h); every clock is the
// counter since boot, local time is UTC.

#include <time.h>
#include <errno.h>

#include "../../include/__impl_libc.h"
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static constexpr long long TAU_NS_PER_SEC = 1'000'000'000LL;

static bool __tau_valid(const struct __SPRT_TIMESPEC_NAME *req) {
	return req->tv_sec >= 0 && req->tv_nsec >= 0 && req->tv_nsec < TAU_NS_PER_SEC;
}

__SPRT_C_FUNC int nanosleep(const struct __SPRT_TIMESPEC_NAME *req, struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}
	if (!__tau_valid(req)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__sprt_tau_sleep_ns((unsigned long long)req->tv_sec * TAU_NS_PER_SEC + (unsigned long long)req->tv_nsec);
	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

__SPRT_C_FUNC int clock_nanosleep(__SPRT_ID(clockid_t), int flags, const struct __SPRT_TIMESPEC_NAME *req,
		struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		return EFAULT; // reported through the return value
	}
	if (!__tau_valid(req)) {
		return EINVAL;
	}
	unsigned long long ns = (unsigned long long)req->tv_sec * TAU_NS_PER_SEC + (unsigned long long)req->tv_nsec;
	if (flags & __SPRT_TIMER_ABSTIME) {
		unsigned long long now = __sprt_tau_clock_ns();
		ns = ns > now ? ns - now : 0;
	}
	if (ns) {
		__sprt_tau_sleep_ns(ns);
	}
	if (rem && !(flags & __SPRT_TIMER_ABSTIME)) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

__SPRT_C_FUNC int clock_getcpuclockid(__SPRT_ID(pid_t), __SPRT_ID(clockid_t) *) __SPRT_NOEXCEPT { return ENOSYS; }

__SPRT_C_FUNC struct tm *localtime_r(const time_t *__restrict t, struct tm *__restrict tm) __SPRT_NOEXCEPT {
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
