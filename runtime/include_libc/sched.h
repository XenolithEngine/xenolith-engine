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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SCHED_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SCHED_H_

#ifdef __SPRT_BUILD

#include_next <sched.h>

#else

#include <c/__sprt_sched.h>

__SPRT_BEGIN_DECL

typedef __SPRT_ID(pid_t) pid_t;
typedef struct __SPRT_ID(sched_param) sched_param;
typedef __SPRT_TIMESPEC_NAME timespec;
typedef __SPRT_ID(cpu_set_t) cpu_set_t;

SPRT_FORCEINLINE inline int sched_get_priority_max(int t) {
	return __sprt_sched_get_priority_max(t);
}

SPRT_FORCEINLINE inline int sched_get_priority_min(int t) {
	return __sprt_sched_get_priority_min(t);
}

SPRT_FORCEINLINE inline int sched_getparam(pid_t pid, sched_param *p) {
	return __sprt_sched_getparam(pid, p);
}

SPRT_FORCEINLINE inline int sched_getscheduler(pid_t pid) { return __sprt_sched_getscheduler(pid); }

SPRT_FORCEINLINE inline int sched_rr_get_interval(pid_t pid, timespec *t) {
	return __sprt_sched_rr_get_interval(pid, t);
}

SPRT_FORCEINLINE inline int sched_setparam(pid_t pid, const sched_param *p) {
	return __sprt_sched_setparam(pid, p);
}

SPRT_FORCEINLINE inline int sched_setscheduler(pid_t pid, int t, const sched_param *p) {
	return __sprt_sched_setscheduler(pid, t, p);
}

SPRT_FORCEINLINE inline int sched_yield(void) { return __sprt_sched_yield(); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SCHED_H_
