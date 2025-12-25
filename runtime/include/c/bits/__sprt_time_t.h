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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_TIME_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_TIME_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>
#include <c/bits/__sprt_int64_t.h>

// time_t is always 64-bit for sprt

// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64) || defined(_WIN32) || defined(__ILP32__)

typedef long long int __SPRT_ID(time_t);
#define __SPRT_TIME_MAX __SPRT_LLINT_MAX
#define __SPRT_TIME_WIDTH __SPRT_LLINT_WIDTH

#elif defined(__LP64__)

typedef long int __SPRT_ID(time_t);
#define __SPRT_TIME_MAX __SPRT_LINT_MAX
#define __SPRT_TIME_WIDTH __SPRT_LINT_WIDTH

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif

typedef __SPRT_ID(time_t) __SPRT_ID(clock_t);

typedef __SPRT_ID(int64_t) __SPRT_ID(suseconds_t);

#ifdef __SPRT_BUILD
#define __SPRT_TM_NAME __SPRT_ID(tm)
#define __SPRT_TIMESPEC_NAME __SPRT_ID(timespec)
#define __SPRT_TIMEVAL_NAME __SPRT_ID(timeval)
#else
#define __SPRT_TM_NAME tm
#define __SPRT_TIMESPEC_NAME timespec
#define __SPRT_TIMEVAL_NAME timeval
#endif

struct __SPRT_TIMESPEC_NAME {
	__SPRT_ID(time_t) tv_sec;
	__SPRT_ID(int64_t) tv_nsec;
};

struct __SPRT_TIMEVAL_NAME {
	__SPRT_ID(time_t) tv_sec;
	__SPRT_ID(suseconds_t) tv_usec;
};

typedef int __SPRT_ID(clockid_t);

#ifdef __SPRT_BUILD
#define __SPRT_TM_NAME __SPRT_ID(tm)
#else
#define __SPRT_TM_NAME tm
#endif

struct __SPRT_TM_NAME;

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_TIME_T_H_
