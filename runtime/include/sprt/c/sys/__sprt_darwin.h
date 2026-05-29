/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/c/bits/__sprt_int64_t.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_null.h>

// clang-format off

#define __SPRT_OS_UNFAIR_LOCK_FLAG_NONE 0x00000000U
#define __SPRT_OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN 0x00040000U

#define __SPRT_OS_SYNC_WAIT_ON_ADDRESS_NONE  0x00000000U
#define __SPRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED 0x00000001U

#define __SPRT_OS_SYNC_WAKE_BY_ADDRESS_NONE  0x00000000U
#define __SPRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED 0x00000001U

#define __SPRT_OS_CLOCK_MACH_ABSOLUTE_TIME 32U

#define __SPRT_OS_UNFAIR_LOCK_INIT (0)

// clang-format on

typedef __SPRT_ID(uint64_t) __SPRT_ID(_CFTypeID);
typedef __SPRT_ID(uint64_t) __SPRT_ID(_CFOptionFlags);
typedef __SPRT_ID(uint64_t) __SPRT_ID(_CFHashCode);
typedef __SPRT_ID(int64_t) __SPRT_ID(_CFIndex);

typedef void *__SPRT_ID(_CFAllocatorRef);

#define __sprt_kCFAllocatorDefault ((__SPRT_ID(_CFAllocatorRef) *)__SPRT_NULL)

typedef void *__SPRT_ID(_CFStringRef);

typedef double __SPRT_ID(_CFTimeInterval);
typedef __SPRT_ID(_CFTimeInterval) __SPRT_ID(_CFAbsoluteTime);

typedef __SPRT_ID(_CFStringRef) __SPRT_ID(_CFRunLoopMode);

typedef void *__SPRT_ID(_CFRunLoopRef);
typedef void *__SPRT_ID(_CFRunLoopSourceRef);
typedef void *__SPRT_ID(_CFRunLoopObserverRef);
typedef void *__SPRT_ID(_CFRunLoopTimerRef);

typedef enum {
	__SPRT_ID(_kCFRunLoopRunFinished) = 1,
	__SPRT_ID(_kCFRunLoopRunStopped) = 2,
	__SPRT_ID(_kCFRunLoopRunTimedOut) = 3,
	__SPRT_ID(_kCFRunLoopRunHandledSource) = 4
} __SPRT_ID(_CFRunLoopRunResult);

typedef struct {
	__SPRT_ID(_CFIndex) version;
	void *info;
	const void *(*retain)(const void *info);
	void (*release)(const void *info);
	__SPRT_ID(_CFStringRef) (*copyDescription)(const void *info);
} __SPRT_ID(_CFRunLoopTimerContext);

typedef void (*__SPRT_ID(_CFRunLoopTimerCallBack))(__SPRT_ID(_CFRunLoopTimerRef) timer, void *info);


#if __SPRT_CONFIG_HAVE_DARWIN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API extern const __SPRT_ID(_CFRunLoopMode) __SPRT_ID(_kCFRunLoopDefaultMode);

SPRT_API extern const __SPRT_ID(_CFRunLoopMode) __SPRT_ID(_kCFRunLoopCommonModes);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_os_sync_wait_on_address)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_os_sync_wait_on_address_with_deadline)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT;

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_os_sync_wait_on_address_with_timeout)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT;

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_os_sync_wake_by_address_any)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_os_sync_wake_by_address_all)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRelease)(void *);

/* absolute time is the time interval since the reference date */
/* the reference date (epoch) is 00:00:00 1 January 2001. */
__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFAbsoluteTime) __SPRT_ID(_CFAbsoluteTimeGetCurrent)(void);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFStringRef) __SPRT_ID(
		_CFStringCreateWithUTF8String)(__SPRT_ID(_CFAllocatorRef) alloc, const char *cStr);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetCurrent)(void);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetMain)(void);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRunLoopRun)(void);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFRunLoopRunResult)
		__SPRT_ID(_CFRunLoopRunInMode)(__SPRT_ID(_CFRunLoopMode) mode,
				__SPRT_ID(_CFTimeInterval) seconds, int returnAfterSourceHandled);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_CFRunLoopIsWaiting)(__SPRT_ID(_CFRunLoopRef) rl);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRunLoopWakeUp)(__SPRT_ID(_CFRunLoopRef) rl);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRunLoopStop)(__SPRT_ID(_CFRunLoopRef) rl);

#if defined(__has_feature) && __has_feature(blocks)
__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(
		_CFRunLoopPerformBlock)(__SPRT_ID(_CFRunLoopRef) rl, const void *mode, void (^block)(void));
#endif

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(
		_CFRunLoopAddCommonMode)(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopMode) mode);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API __SPRT_ID(_CFRunLoopTimerRef) __SPRT_ID(_CFRunLoopTimerCreate)(__SPRT_ID(_CFAllocatorRef),
		__SPRT_ID(_CFAbsoluteTime) fireDate, __SPRT_ID(_CFTimeInterval) interval,
		__SPRT_ID(_CFOptionFlags) flags, __SPRT_ID(_CFIndex) order,
		__SPRT_ID(_CFRunLoopTimerCallBack) callout, __SPRT_ID(_CFRunLoopTimerContext) * context);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API int __SPRT_ID(_CFRunLoopContainsTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRunLoopAddTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode);

__SPRT_CONFIG_HAVE_DARWIN_NOTICE
SPRT_API void __SPRT_ID(_CFRunLoopRemoveTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_
