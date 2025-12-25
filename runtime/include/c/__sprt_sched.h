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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_SCHED_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_SCHED_H_

#include <c/bits/__sprt_time_t.h>
#include <c/cross/__sprt_sysid.h>

__SPRT_BEGIN_DECL

struct __SPRT_ID(sched_param) {
	int sched_priority;
};

typedef struct {
	unsigned long __bits[128 / sizeof(long)];
} __SPRT_ID(cpu_set_t);

SPRT_API int __SPRT_ID(sched_get_priority_max)(int);
SPRT_API int __SPRT_ID(sched_get_priority_min)(int);
SPRT_API int __SPRT_ID(sched_getparam)(__SPRT_ID(pid_t), struct __SPRT_ID(sched_param) *);
SPRT_API int __SPRT_ID(sched_getscheduler)(__SPRT_ID(pid_t));
SPRT_API int __SPRT_ID(sched_rr_get_interval)(__SPRT_ID(pid_t), __SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(sched_setparam)(__SPRT_ID(pid_t), const struct __SPRT_ID(sched_param) *);
SPRT_API int __SPRT_ID(
		sched_setscheduler)(__SPRT_ID(pid_t), int, const struct __SPRT_ID(sched_param) *);
SPRT_API int __SPRT_ID(sched_yield)(void);

#define __SPRT_SCHED_OTHER 0
#define __SPRT_SCHED_FIFO 1
#define __SPRT_SCHED_RR 2
#define __SPRT_SCHED_BATCH 3
#define __SPRT_SCHED_IDLE 5
#define __SPRT_SCHED_DEADLINE 6
#define __SPRT_SCHED_RESET_ON_FORK 0x4000'0000

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_SCHED_H_
