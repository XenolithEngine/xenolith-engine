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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_

#include <sprt/c/sys/__sprt_darwin.h>

// clang-format off

#define _OS_UNFAIR_LOCK_FLAG_NONE __SPRT_OS_UNFAIR_LOCK_FLAG_NONE
#define _OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN __SPRT_OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN

#define _OS_SYNC_WAIT_ON_ADDRESS_NONE __SPRT_OS_SYNC_WAIT_ON_ADDRESS_NONE
#define _OS_SYNC_WAIT_ON_ADDRESS_SHARED __SPRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED

#define _OS_SYNC_WAKE_BY_ADDRESS_NONE __SPRT_OS_SYNC_WAKE_BY_ADDRESS_NONE
#define _OS_SYNC_WAKE_BY_ADDRESS_SHARED __SPRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED

#define _OS_CLOCK_MACH_ABSOLUTE_TIME __SPRT_OS_CLOCK_MACH_ABSOLUTE_TIME

#define _OS_UNFAIR_LOCK_INIT __SPRT_OS_UNFAIR_LOCK_INIT

// clang-format on

typedef __SPRT_ID(_CFTypeID) _CFTypeID;
typedef __SPRT_ID(_CFOptionFlags) _CFOptionFlags;
typedef __SPRT_ID(_CFHashCode) _CFHashCode;
typedef __SPRT_ID(_CFIndex) _CFIndex;

typedef __SPRT_ID(_CFTimeInterval) _CFTimeInterval;
typedef __SPRT_ID(_CFAbsoluteTime) _CFAbsoluteTime;

typedef __SPRT_ID(_CFAllocatorRef) _CFAllocatorRef;

#define _kCFAllocatorDefault __sprt_kCFAllocatorDefault

typedef __SPRT_ID(_CFStringRef) _CFStringRef;

typedef __SPRT_ID(_CFRunLoopRef) _CFRunLoopRef;
typedef __SPRT_ID(_CFRunLoopSourceRef) _CFRunLoopSourceRef;
typedef __SPRT_ID(_CFRunLoopObserverRef) _CFRunLoopObserverRef;
typedef __SPRT_ID(_CFRunLoopTimerRef) _CFRunLoopTimerRef;

typedef __SPRT_ID(_CFRunLoopTimerContext) _CFRunLoopTimerContext;
typedef __SPRT_ID(_CFRunLoopTimerCallBack) _CFRunLoopTimerCallBack;

#define _kCFRunLoopRunFinished __SPRT_ID(_kCFRunLoopRunFinished)
#define _kCFRunLoopRunStopped __SPRT_ID(_kCFRunLoopRunStopped)
#define _kCFRunLoopRunTimedOut __SPRT_ID(_kCFRunLoopRunTimedOut)
#define _kCFRunLoopRunHandledSource __SPRT_ID(_kCFRunLoopRunHandledSource)

#if __SPRT_CONFIG_HAVE_DARWIN || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

#define _kCFRunLoopDefaultMode __SPRT_ID(_kCFRunLoopDefaultMode)
#define _kCFRunLoopCommonModes __SPRT_ID(_kCFRunLoopCommonModes)

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int _os_sync_wait_on_address(void *addr, __SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__os_sync_wait_on_address(addr, value, size, flags);
}
#endif

SPRT_UMBRELLA_FUNC
int _os_sync_wait_on_address_with_deadline(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) deadline) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__os_sync_wait_on_address_with_deadline(addr, value, size, flags, clockid,
			deadline);
}
#endif

SPRT_UMBRELLA_FUNC
int _os_sync_wait_on_address_with_timeout(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) timeout_ns) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__os_sync_wait_on_address_with_timeout(addr, value, size, flags, clockid,
			timeout_ns);
}
#endif

SPRT_UMBRELLA_FUNC
int _os_sync_wake_by_address_any(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__os_sync_wake_by_address_any(addr, size, flags);
}
#endif

SPRT_UMBRELLA_FUNC
int _os_sync_wake_by_address_all(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__os_sync_wake_by_address_all(addr, size, flags);
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRelease(void *ptr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRelease(ptr);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFAbsoluteTime)
_CFAbsoluteTimeGetCurrent(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFAbsoluteTimeGetCurrent();
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFStringRef)
_CFStringCreateWithUTF8String(__SPRT_ID(_CFAllocatorRef alloc), const char *cStr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFStringCreateWithUTF8String(alloc, cStr);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFRunLoopRef)
_CFRunLoopGetCurrent(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopGetCurrent();
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFRunLoopRef)
_CFRunLoopGetMain(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopGetMain();
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopRun(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopRun();
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFRunLoopRunResult)
_CFRunLoopRunInMode(__SPRT_ID(_CFRunLoopMode) mode, __SPRT_ID(_CFTimeInterval) seconds,
		int returnAfterSourceHandled) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
}
#endif

SPRT_UMBRELLA_FUNC
int _CFRunLoopIsWaiting(__SPRT_ID(_CFRunLoopRef) rl) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopIsWaiting(rl);
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopWakeUp(__SPRT_ID(_CFRunLoopRef) rl) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopWakeUp(rl);
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopStop(__SPRT_ID(_CFRunLoopRef) rl) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopStop(rl);
}
#endif

#if defined(__has_feature) && __has_feature(blocks)
SPRT_UMBRELLA_FUNC
void _CFRunLoopPerformBlock(__SPRT_ID(_CFRunLoopRef) rl, const void *mode,
		void (^block)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopPerformBlock(rl, mode, block);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopAddCommonMode(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopMode) mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopAddCommonMode(rl, mode);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(_CFRunLoopTimerRef)
_CFRunLoopTimerCreate(__SPRT_ID(_CFAllocatorRef) a, __SPRT_ID(_CFAbsoluteTime) fireDate,
		__SPRT_ID(_CFTimeInterval) interval, __SPRT_ID(_CFOptionFlags) flags,
		__SPRT_ID(_CFIndex) order, __SPRT_ID(_CFRunLoopTimerCallBack) callout,
		__SPRT_ID(_CFRunLoopTimerContext) * context) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopTimerCreate(a, fireDate, interval, flags, order, callout, context);
}
#endif

SPRT_UMBRELLA_FUNC
int _CFRunLoopContainsTimer(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopTimerRef) timer,
		__SPRT_ID(_CFRunLoopMode) mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt__CFRunLoopContainsTimer(rl, timer, mode);
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopAddTimer(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopTimerRef) timer,
		__SPRT_ID(_CFRunLoopMode) mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopAddTimer(rl, timer, mode);
}
#endif

SPRT_UMBRELLA_FUNC
void _CFRunLoopRemoveTimer(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopTimerRef) timer,
		__SPRT_ID(_CFRunLoopMode) mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__CFRunLoopRemoveTimer(rl, timer, mode);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_
