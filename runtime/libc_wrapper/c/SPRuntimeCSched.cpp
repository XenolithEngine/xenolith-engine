/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_errno.h>

#include <sprt/runtime/log.h>

#include <sched.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(
		sched_getparam)(__SPRT_ID(pid_t) pid, struct __SPRT_SCHED_PARAM_NAME *p) {
#if __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
	struct sched_param param;
	auto ret = sched_getparam(pid, &param);
	if (p) {
		p->sched_priority = param.sched_priority;
	}
	return ret;
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sched_getscheduler)(__SPRT_ID(pid_t) pid) {
#if __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
	return sched_getscheduler(pid);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sched_rr_get_interval)(__SPRT_ID(pid_t) pid, __SPRT_TIMESPEC_NAME *t) {
#if __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
#if SPRT_WINDOWS
	return sched_rr_get_interval(pid, t);
#else
	struct timespec ts;
	auto ret = sched_rr_get_interval(pid, &ts);
	if (t) {
		t->tv_sec = ts.tv_sec;
		t->tv_nsec = ts.tv_nsec;
	}
	return ret;
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		sched_setparam)(__SPRT_ID(pid_t) pid, const struct __SPRT_SCHED_PARAM_NAME *p) {
#if __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
	struct sched_param param;
	__sprt_memset(&param, 0, sizeof(struct sched_param));
	if (p) {
		param.sched_priority = p->sched_priority;
	}
	return sched_setparam(pid, &param);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		sched_setscheduler)(__SPRT_ID(pid_t) pid, int t, const struct __SPRT_SCHED_PARAM_NAME *p) {
#if __SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER
	struct sched_param param;
	__sprt_memset(&param, 0, sizeof(struct sched_param));
	if (p) {
		param.sched_priority = p->sched_priority;
	}
	return sched_setscheduler(pid, t, &param);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_SETSCHEDULER)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

#if __SPRT_CONFIG_HAVE_SCHED_AFFINITY
#define __SPRT_SCHED_AFFINITY_UNAVAILABLE(ret)
#else
#define __SPRT_SCHED_AFFINITY_UNAVAILABLE(ret) \
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__, \
			" not available for this platform (__SPRT_CONFIG_HAVE_SCHED_AFFINITY)"); \
	*__sprt___errno_location() = ENOSYS; \
	return ret;
#endif

__SPRT_C_FUNC int __SPRT_ID(sched_getcpu)(void) {
#if __SPRT_CONFIG_HAVE_SCHED_AFFINITY
	return sched_getcpu();
#else
	__SPRT_SCHED_AFFINITY_UNAVAILABLE(-1)
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		sched_getaffinity)(__SPRT_ID(pid_t) pid, __SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) {
#if __SPRT_CONFIG_HAVE_SCHED_AFFINITY
#if SPRT_EMBOX
	// Embox's cpu_set_t is one word, bit i = core i; ours is Linux's array of
	// longs, so the first long carries it, as in thread_t::getaffinity.
	if (!set || n < sizeof(set->__bits[0])) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	cpu_set_t mask = 0;
	auto ret = sched_getaffinity(pid, sizeof(mask), &mask);
	if (ret == 0) {
		__sprt_memset(set, 0, n);
		set->__bits[0] = mask;
	}
	return ret;
#else
	return sched_getaffinity(pid, n, reinterpret_cast<cpu_set_t *>(set));
#endif
#else
	__SPRT_SCHED_AFFINITY_UNAVAILABLE(-1)
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sched_setaffinity)(__SPRT_ID(pid_t) pid, __SPRT_ID(size_t) n,
		const __SPRT_ID(cpu_set_t) * set) {
#if __SPRT_CONFIG_HAVE_SCHED_AFFINITY
#if SPRT_EMBOX
	if (!set || n < sizeof(set->__bits[0])) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	cpu_set_t mask = static_cast<cpu_set_t>(set->__bits[0]);
	return sched_setaffinity(pid, sizeof(mask), &mask);
#else
	return sched_setaffinity(pid, n, reinterpret_cast<const cpu_set_t *>(set));
#endif
#else
	__SPRT_SCHED_AFFINITY_UNAVAILABLE(-1)
#endif
}

} // namespace sprt
