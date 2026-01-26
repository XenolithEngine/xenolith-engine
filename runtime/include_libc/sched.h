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

#include <sprt/c/__sprt_sched.h>

__SPRT_BEGIN_DECL

typedef __SPRT_ID(pid_t) pid_t;
typedef struct __SPRT_ID(sched_param) sched_param;
typedef __SPRT_TIMESPEC_NAME timespec;
typedef __SPRT_ID(cpu_set_t) cpu_set_t;

#define CPU_SETSIZE __SPRT_CPU_SETSIZE

#define CPU_SET_S(i, size, set) __SPRT_CPU_SET_S(i, size, set)
#define CPU_CLR_S(i, size, set) __SPRT_CPU_SET_S(i, size, set)
#define CPU_ISSET_S(i, size, set) __SPRT_CPU_SET_S(i, size, set)

#define CPU_AND_S(a, b, c, d) __SPRT_CPU_AND_S(a,b,c,d)
#define CPU_OR_S(a, b, c, d) __SPRT_CPU_OR_S(a,b,c,d)
#define CPU_XOR_S(a, b, c, d) __SPRT_CPU_XOR_S(a,b,c,d)

#define CPU_COUNT_S(size, set) __SPRT_CPU_COUNT_S(size,set)
#define CPU_ZERO_S(size, set) __SPRT_CPU_ZERO_S(size,set)
#define CPU_EQUAL_S(size, set1, set2) __SPRT_CPU_EQUAL_S(size,set1,set2)

#define CPU_ALLOC_SIZE(n) __SPRT_CPU_ALLOC_SIZE(n)
#define CPU_ALLOC(n) __SPRT_CPU_ALLOC(n)
#define CPU_FREE(set) __SPRT_CPU_FREE(set)

#define CPU_SET(i, set) __SPRT_CPU_SET(i, set)
#define CPU_CLR(i, set) __SPRT_CPU_CLR(i, set)
#define CPU_ISSET(i, set) __SPRT_CPU_ISSET(i, set)
#define CPU_AND(d, s1, s2) __SPRT_CPU_AND(d,s1,s2)
#define CPU_OR(d, s1, s2) __SPRT_CPU_OR(d,s1,s2)
#define CPU_XOR(d, s1, s2) __SPRT_CPU_XOR(d,s1,s2)
#define CPU_COUNT(set) __SPRT_CPU_COUNT(set)
#define CPU_ZERO(set) __SPRT_CPU_ZERO(set)
#define CPU_EQUAL(s1, s2) __SPRT_CPU_EQUAL(s1,s2)

SPRT_FORCEINLINE int sched_get_priority_max(int t) { return __sprt_sched_get_priority_max(t); }

SPRT_FORCEINLINE int sched_get_priority_min(int t) { return __sprt_sched_get_priority_min(t); }

SPRT_FORCEINLINE int sched_getparam(pid_t pid, sched_param *p) {
	return __sprt_sched_getparam(pid, p);
}

SPRT_FORCEINLINE int sched_getscheduler(pid_t pid) { return __sprt_sched_getscheduler(pid); }

SPRT_FORCEINLINE int sched_rr_get_interval(pid_t pid, timespec *t) {
	return __sprt_sched_rr_get_interval(pid, t);
}

SPRT_FORCEINLINE int sched_setparam(pid_t pid, const sched_param *p) {
	return __sprt_sched_setparam(pid, p);
}

SPRT_FORCEINLINE int sched_setscheduler(pid_t pid, int t, const sched_param *p) {
	return __sprt_sched_setscheduler(pid, t, p);
}

SPRT_FORCEINLINE int sched_yield(void) { return __sprt_sched_yield(); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SCHED_H_
