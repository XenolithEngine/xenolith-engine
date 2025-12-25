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

#include <c/__sprt_sched.h>
#include <c/__sprt_string.h>

#include <sched.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(sched_get_priority_max)(int t) { return ::sched_get_priority_max(t); }

__SPRT_C_FUNC int __SPRT_ID(sched_get_priority_min)(int t) { return ::sched_get_priority_min(t); }

__SPRT_C_FUNC int __SPRT_ID(
		sched_getparam)(__SPRT_ID(pid_t) pid, struct __SPRT_ID(sched_param) * p) {
	struct sched_param param;
	auto ret = ::sched_getparam(pid, &param);
	if (p) {
		p->sched_priority = param.sched_priority;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(sched_getscheduler)(__SPRT_ID(pid_t) pid) {
	return ::sched_getscheduler(pid);
}

__SPRT_C_FUNC int __SPRT_ID(sched_rr_get_interval)(__SPRT_ID(pid_t) pid, __SPRT_TIMESPEC_NAME *t) {
	struct timespec ts;
	auto ret = ::sched_rr_get_interval(pid, &ts);
	if (t) {
		t->tv_sec = ts.tv_sec;
		t->tv_nsec = ts.tv_nsec;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		sched_setparam)(__SPRT_ID(pid_t) pid, const struct __SPRT_ID(sched_param) * p) {
	struct sched_param param;
	__sprt_memset(&param, 0, sizeof(struct sched_param));
	if (p) {
		param.sched_priority = p->sched_priority;
	}
	return ::sched_setparam(pid, &param);
}

__SPRT_C_FUNC int __SPRT_ID(
		sched_setscheduler)(__SPRT_ID(pid_t) pid, int t, const struct __SPRT_ID(sched_param) * p) {
	struct sched_param param;
	__sprt_memset(&param, 0, sizeof(struct sched_param));
	if (p) {
		param.sched_priority = p->sched_priority;
	}
	return ::sched_setscheduler(pid, t, &param);
}

__SPRT_C_FUNC int __SPRT_ID(sched_yield)(void) { return ::sched_yield(); }

} // namespace sprt
