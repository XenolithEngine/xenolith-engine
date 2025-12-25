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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_PTHREAD_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_PTHREAD_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int32_t.h>
#include <c/bits/__sprt_int64_t.h>

#if SPRT_LINUX

/* We need to provide structures that correspond to the largest possible size for their implementation in the  platform's libc */

#define __SPRT_PTHREAD_COMMON_ALIGNMENT 8

#define __SPRT_SIZEOF_PTHREAD_MUTEXATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_COND_T 56
#define __SPRT_SIZEOF_PTHREAD_CONDATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_RWLOCKATTR_T 8
#define __SPRT_SIZEOF_PTHREAD_BARRIERATTR_T 8

#ifdef __x86_64__
#if defined(__LP64__)
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 40
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 56
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 56
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 32
#else
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 32
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 32
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 44
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 20
#endif
#else
#define __SPRT_SIZEOF_PTHREAD_MUTEX_T 24
#define __SPRT_SIZEOF_PTHREAD_ATTR_T 36
#define __SPRT_SIZEOF_PTHREAD_RWLOCK_T 32
#define __SPRT_SIZEOF_PTHREAD_BARRIER_T 20
#endif

typedef unsigned long __SPRT_ID(pthread_t);
typedef int __SPRT_ID(pthread_once_t);
typedef unsigned int __SPRT_ID(pthread_key_t);
typedef volatile int __SPRT_ID(pthread_spinlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_MUTEXATTR_T];
} __SPRT_ID(pthread_mutexattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_COND_T];
} __SPRT_ID(pthread_cond_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_CONDATTR_T];
} __SPRT_ID(pthread_condattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_RWLOCKATTR_T];
} __SPRT_ID(pthread_rwlockattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_BARRIERATTR_T];
} __SPRT_ID(pthread_barrierattr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_MUTEX_T];
} __SPRT_ID(pthread_mutex_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_ATTR_T];
} __SPRT_ID(pthread_attr_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_RWLOCK_T];
} __SPRT_ID(pthread_rwlock_t);

typedef struct alignas(__SPRT_PTHREAD_COMMON_ALIGNMENT) {
	char __size[__SPRT_SIZEOF_PTHREAD_BARRIER_T];
} __SPRT_ID(pthread_barrier_t);

#elif SPRT_ANDROID

typedef long __SPRT_ID(pthread_t);
typedef int __SPRT_ID(pthread_once_t);
typedef int __SPRT_ID(pthread_key_t);

typedef struct {
#if defined(__LP64__)
	int64_t __private;
#else
	int32_t __private[2];
#endif
} __SPRT_ID(pthread_spinlock_t);

typedef long __SPRT_ID(pthread_mutexattr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[12];
#else
	int32_t __private[1];
#endif
} __SPRT_ID(pthread_cond_t);

typedef long __SPRT_ID(pthread_condattr_t);

typedef long __SPRT_ID(pthread_rwlockattr_t);

typedef int __SPRT_ID(pthread_barrierattr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[10];
#else
	int32_t __private[1];
#endif
} __SPRT_ID(pthread_mutex_t);

typedef struct {
	uint32_t flags;
	void *stack_base;
	size_t stack_size;
	size_t guard_size;
	int32_t sched_policy;
	int32_t sched_priority;
#ifdef __LP64__
	char __reserved[16];
#endif
} __SPRT_ID(pthread_attr_t);

typedef struct {
#if defined(__LP64__)
	int32_t __private[14];
#else
	int32_t __private[10];
#endif
} __SPRT_ID(pthread_rwlock_t);

typedef struct {
#if defined(__LP64__)
	int64_t __private[4];
#else
	int32_t __private[8];
#endif
} __SPRT_ID(pthread_barrier_t);


#elif SPRT_WINDOWS

#error "Unknown OS"

#elif SPRT_MACOS

#error "Unknown OS"

#else

#error "Unknown OS"

#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_PTHREAD_H_
