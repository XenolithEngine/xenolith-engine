#ifndef RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_
#define RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_

#include <sprt/c/cross/__sprt_fstypes.h>
#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/cross/__sprt_locale.h>
#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_float.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/__sprt_sched.h>
#include <sprt/c/__sprt_pthread.h>

typedef __SPRT_ID(intmax_t) intmax_t;
typedef __SPRT_ID(intptr_t) intptr_t;
typedef __SPRT_ID(int8_t) int8_t;
typedef __SPRT_ID(int16_t) int16_t;
typedef __SPRT_ID(int32_t) int32_t;
typedef __SPRT_ID(int64_t) int64_t;

typedef __SPRT_ID(uintmax_t) uintmax_t;
typedef __SPRT_ID(uintptr_t) uintptr_t;
typedef __SPRT_ID(uint8_t) uint8_t;
typedef __SPRT_ID(uint16_t) uint16_t;
typedef __SPRT_ID(uint32_t) uint32_t;
typedef __SPRT_ID(uint64_t) uint64_t;

typedef __SPRT_ID(float_t) float_t;
typedef __SPRT_ID(double_t) double_t;

typedef __SPRT_ID(clockid_t) clockid_t;
typedef __SPRT_ID(clock_t) clock_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(timer_t) timer_t;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(locale_t) locale_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(pid_t) pid_t;
typedef __SPRT_ID(gid_t) gid_t;
typedef __SPRT_ID(mode_t) mode_t;

typedef __SPRT_ID(pthread_t) pthread_t;
typedef __SPRT_ID(pthread_once_t) pthread_once_t;
typedef __SPRT_ID(pthread_key_t) pthread_key_t;
typedef __SPRT_ID(pthread_spinlock_t) pthread_spinlock_t;
typedef __SPRT_ID(pthread_mutexattr_t) pthread_mutexattr_t;
typedef __SPRT_ID(pthread_cond_t) pthread_cond_t;
typedef __SPRT_ID(pthread_condattr_t) pthread_condattr_t;
typedef __SPRT_ID(pthread_rwlockattr_t) pthread_rwlockattr_t;
typedef __SPRT_ID(pthread_barrierattr_t) pthread_barrierattr_t;
typedef __SPRT_ID(pthread_mutex_t) pthread_mutex_t;
typedef __SPRT_ID(pthread_attr_t) pthread_attr_t;
typedef __SPRT_ID(pthread_rwlock_t) pthread_rwlock_t;
typedef __SPRT_ID(pthread_barrier_t) pthread_barrier_t;

typedef __SPRT_ID(sigset_t) sigset_t;

#ifndef __cplusplus
#if SPRT_WINDOWS
// Musl expects 32-bit wchar_t
typedef __SPRT_ID(uint16_t) wchar_t;
typedef __SPRT_ID(uint16_t) wint_t;
typedef __SPRT_ID(uint16_t) wctype_t;
#else
// Musl expects 32-bit wchar_t
typedef __SPRT_ID(uint32_t) wchar_t;
typedef unsigned long wint_t;
typedef unsigned long wctype_t;
#endif
#endif


typedef struct __mbstate_t {
	unsigned __opaque1, __opaque2;
} mbstate_t;

typedef struct _IO_FILE FILE;

typedef __builtin_va_list __isoc_va_list;

#endif // RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_
