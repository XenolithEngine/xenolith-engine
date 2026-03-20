/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

typedef __SPRT_ID(_os_unfair_lock) _os_unfair_lock;

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

SPRT_FORCEINLINE void _os_unfair_lock_lock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_lock(lock);
}

SPRT_FORCEINLINE bool _os_unfair_lock_trylock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	return __sprt__os_unfair_lock_trylock(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_unlock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_unlock(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_assert_owner(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_assert_owner(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_assert_not_owner(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_assert_not_owner(lock);
}

// clang-format off
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
SPRT_FORCEINLINE void _os_unfair_lock_lock_with_flags(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags) {
	__sprt__os_unfair_lock_lock_with_flags(lock, flags);
}
#endif
// clang-format on

SPRT_FORCEINLINE int _os_sync_wait_on_address(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address(addr, value, size, flags);
}

SPRT_FORCEINLINE int _os_sync_wait_on_address_with_deadline(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address_with_deadline(addr, value, size, flags, clockid,
			deadline);
}

SPRT_FORCEINLINE int _os_sync_wait_on_address_with_timeout(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address_with_timeout(addr, value, size, flags, clockid,
			timeout_ns);
}

SPRT_FORCEINLINE int _os_sync_wake_by_address_any(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wake_by_address_any(addr, size, flags);
}

SPRT_FORCEINLINE int _os_sync_wake_by_address_all(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wake_by_address_all(addr, size, flags);
}

SPRT_FORCEINLINE void _CFRelease(void *ptr) { __sprt__CFRelease(ptr); }

SPRT_FORCEINLINE __SPRT_ID(_CFAbsoluteTime) _CFAbsoluteTimeGetCurrent(void) {
	return __sprt__CFAbsoluteTimeGetCurrent();
}

SPRT_FORCEINLINE __SPRT_ID(_CFStringRef)
		_CFStringCreateWithUTF8String(__SPRT_ID(_CFAllocatorRef alloc), const char *cStr) {
	return __sprt__CFStringCreateWithUTF8String(alloc, cStr);
}

SPRT_FORCEINLINE __SPRT_ID(_CFRunLoopRef) _CFRunLoopGetCurrent(void) {
	return __sprt__CFRunLoopGetCurrent();
}

SPRT_FORCEINLINE __SPRT_ID(_CFRunLoopRef) _CFRunLoopGetMain(void) {
	return __sprt__CFRunLoopGetMain();
}

SPRT_FORCEINLINE void _CFRunLoopRun(void) { __sprt__CFRunLoopRun(); }

SPRT_FORCEINLINE __SPRT_ID(_CFRunLoopRunResult) _CFRunLoopRunInMode(__SPRT_ID(_CFRunLoopMode) mode,
		__SPRT_ID(_CFTimeInterval) seconds, int returnAfterSourceHandled) {
	return __sprt__CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
}

SPRT_FORCEINLINE int _CFRunLoopIsWaiting(__SPRT_ID(_CFRunLoopRef) rl) {
	return __sprt__CFRunLoopIsWaiting(rl);
}

SPRT_FORCEINLINE void _CFRunLoopWakeUp(__SPRT_ID(_CFRunLoopRef) rl) { __sprt__CFRunLoopWakeUp(rl); }

SPRT_FORCEINLINE void _CFRunLoopStop(__SPRT_ID(_CFRunLoopRef) rl) { __sprt__CFRunLoopStop(rl); }

#if defined(__has_feature) && __has_feature(blocks)
SPRT_FORCEINLINE void _CFRunLoopPerformBlock(__SPRT_ID(_CFRunLoopRef) rl, const void *mode,
		void (^block)(void)) {
	__sprt__CFRunLoopPerformBlock(rl, mode, block);
}
#endif

SPRT_FORCEINLINE void _CFRunLoopAddCommonMode(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopMode) mode) {
	__sprt__CFRunLoopAddCommonMode(rl, mode);
}

SPRT_FORCEINLINE __SPRT_ID(_CFRunLoopTimerRef) _CFRunLoopTimerCreate(__SPRT_ID(_CFAllocatorRef) a,
		__SPRT_ID(_CFAbsoluteTime) fireDate, __SPRT_ID(_CFTimeInterval) interval,
		__SPRT_ID(_CFOptionFlags) flags, __SPRT_ID(_CFIndex) order,
		__SPRT_ID(_CFRunLoopTimerCallBack) callout, __SPRT_ID(_CFRunLoopTimerContext) * context) {
	return __sprt__CFRunLoopTimerCreate(a, fireDate, interval, flags, order, callout, context);
}

SPRT_FORCEINLINE int _CFRunLoopContainsTimer(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	return __sprt__CFRunLoopContainsTimer(rl, timer, mode);
}

SPRT_FORCEINLINE void _CFRunLoopAddTimer(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	__sprt__CFRunLoopAddTimer(rl, timer, mode);
}

SPRT_FORCEINLINE void _CFRunLoopRemoveTimer(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	__sprt__CFRunLoopRemoveTimer(rl, timer, mode);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_
