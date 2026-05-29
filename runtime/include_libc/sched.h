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

/*
	Dispatch header for the POSIX <sched.h>:
	- hosted SPRT build -> forwards to the system <sched.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	struct sched_param comes in via <sprt/c/__sprt_sched.h>.

	Types:
	  pid_t      - process identifier
	  timespec   - seconds/nanoseconds time value (for the RR interval)
	  cpu_set_t  - bitset of CPUs for affinity operations
	  cpuset_t   - BSD-style alias of cpu_set_t

	Macros:
	  CPU_SETSIZE - number of CPUs a fixed cpu_set_t can hold
	  fixed-size set ops:  CPU_SET, CPU_CLR, CPU_ISSET, CPU_AND, CPU_OR, CPU_XOR,
	                       CPU_COUNT, CPU_ZERO, CPU_EQUAL
	  explicit-size ops:   CPU_SET_S, CPU_CLR_S, CPU_ISSET_S, CPU_AND_S, CPU_OR_S,
	                       CPU_XOR_S, CPU_COUNT_S, CPU_ZERO_S, CPU_EQUAL_S
	  dynamic allocation:  CPU_ALLOC_SIZE, CPU_ALLOC, CPU_FREE

	Functions:
	  sched_get_priority_max - highest priority allowed for a policy
	  sched_get_priority_min - lowest priority allowed for a policy
	  sched_getparam         - read a process's scheduling parameters
	  sched_setparam         - set a process's scheduling parameters
	  sched_getscheduler     - read a process's scheduling policy
	  sched_setscheduler     - set a process's scheduling policy and parameters
	  sched_rr_get_interval  - round-robin time quantum for a process
	  sched_yield            - yield the processor to another runnable thread
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sched.h>

#else

#include <sprt/c/__sprt_sched.h>

__SPRT_BEGIN_DECL

typedef __SPRT_ID(pid_t) pid_t;
typedef struct __SPRT_TIMESPEC_NAME timespec;
typedef __SPRT_ID(cpu_set_t) cpu_set_t;
typedef __SPRT_ID(cpu_set_t) cpuset_t;

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

SPRT_UMBRELLA_FUNC
int sched_get_priority_max(int t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_get_priority_max(t);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_get_priority_min(int t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_get_priority_min(t);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_getparam(pid_t pid, struct __SPRT_SCHED_PARAM_NAME *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_getparam(pid, p);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_getscheduler(pid_t pid) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_getscheduler(pid);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_rr_get_interval(pid_t pid, timespec *t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_rr_get_interval(pid, t);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_setparam(pid_t pid, const struct __SPRT_SCHED_PARAM_NAME *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_setparam(pid, p);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_setscheduler(pid_t pid, int t, const struct __SPRT_SCHED_PARAM_NAME *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_setscheduler(pid, t, p);
}
#endif

SPRT_UMBRELLA_FUNC
int sched_yield(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sched_yield();
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SCHED_H_
