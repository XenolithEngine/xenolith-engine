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

#define __SPRT_BUILD 1

#include <sprt/c/sys/__sprt_darwin.h>
#include <sprt/runtime/log.h>

#if __SPRT_CONFIG_HAVE_DARWIN
#include <os/lock.h>
#include <os/os_sync_wait_on_address.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace sprt {

#if __SPRT_CONFIG_HAVE_DARWIN

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
static_assert(OS_UNFAIR_LOCK_FLAG_NONE == __SPRT_OS_UNFAIR_LOCK_FLAG_NONE);
static_assert(OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN == __SPRT_OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN);
#endif

static_assert(OS_SYNC_WAIT_ON_ADDRESS_NONE == __SPRT_OS_SYNC_WAIT_ON_ADDRESS_NONE);
static_assert(OS_SYNC_WAIT_ON_ADDRESS_SHARED == __SPRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED);

static_assert(OS_SYNC_WAKE_BY_ADDRESS_NONE == __SPRT_OS_SYNC_WAKE_BY_ADDRESS_NONE);
static_assert(OS_SYNC_WAKE_BY_ADDRESS_SHARED == __SPRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED);

static_assert(OS_CLOCK_MACH_ABSOLUTE_TIME == __SPRT_OS_CLOCK_MACH_ABSOLUTE_TIME);

extern "C" const __SPRT_ID(_CFRunLoopMode)
		__SPRT_ID(_kCFRunLoopDefaultMode) = (__SPRT_ID(_CFRunLoopMode))kCFRunLoopDefaultMode;

extern "C" const __SPRT_ID(_CFRunLoopMode)
		__SPRT_ID(_kCFRunLoopCommonModes) = (__SPRT_ID(_CFRunLoopMode))kCFRunLoopCommonModes;

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_lock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	os_unfair_lock_lock(os_unfair_lock_t(lock));
}

__SPRT_C_FUNC bool __SPRT_ID(_os_unfair_lock_trylock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	return os_unfair_lock_trylock(os_unfair_lock_t(lock));
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_unlock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	os_unfair_lock_unlock(os_unfair_lock_t(lock));
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_assert_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	os_unfair_lock_assert_owner((const os_unfair_lock *)lock);
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_assert_not_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	os_unfair_lock_assert_not_owner((const os_unfair_lock *)lock);
}

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_lock_with_flags)(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags) {
	os_unfair_lock_lock_with_flags(os_unfair_lock_t(lock), os_unfair_lock_flags_t(flags));
}
#else
__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_lock_with_flags)(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (MAC_OS_X_VERSION_MIN_REQUIRED < "
			"MAC_OS_X_VERSION_10_15)");
}
#endif

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return os_sync_wait_on_address(addr, value, size, os_sync_wait_on_address_flags_t(flags));
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address_with_deadline)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT {
	return os_sync_wait_on_address_with_deadline(addr, value, size,
			os_sync_wait_on_address_flags_t(flags), os_clockid_t(clockid), deadline);
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address_with_timeout)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT {
	return os_sync_wait_on_address_with_timeout(addr, value, size,
			os_sync_wait_on_address_flags_t(flags), os_clockid_t(clockid), timeout_ns);
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wake_by_address_any)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return os_sync_wake_by_address_any(addr, size, os_sync_wake_by_address_flags_t(flags));
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wake_by_address_all)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return os_sync_wake_by_address_all(addr, size, os_sync_wake_by_address_flags_t(flags));
}

__SPRT_C_FUNC void __SPRT_ID(_CFRelease)(void *ref) { CFRelease(ref); }

__SPRT_C_FUNC __SPRT_ID(_CFAbsoluteTime) __SPRT_ID(_CFAbsoluteTimeGetCurrent)(void) {
	return CFAbsoluteTimeGetCurrent();
}

__SPRT_C_FUNC __SPRT_ID(_CFStringRef) __SPRT_ID(
		_CFStringCreateWithUTF8String)(__SPRT_ID(_CFAllocatorRef) alloc, const char *cStr) {
	return (__SPRT_ID(_CFStringRef))CFStringCreateWithCString(CFAllocatorRef(alloc), cStr,
			kCFStringEncodingUTF8);
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetCurrent)(void) {
	return (__SPRT_ID(_CFRunLoopRef))CFRunLoopGetCurrent();
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetMain)(void) {
	return (__SPRT_ID(_CFRunLoopRef))CFRunLoopGetMain();
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopRun)(void) { CFRunLoopRun(); }

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRunResult)
		__SPRT_ID(_CFRunLoopRunInMode)(__SPRT_ID(_CFRunLoopMode) mode,
				__SPRT_ID(_CFTimeInterval) seconds, int returnAfterSourceHandled) {
	return (__SPRT_ID(_CFRunLoopRunResult))CFRunLoopRunInMode((CFRunLoopMode)mode, seconds,
			returnAfterSourceHandled);
}

__SPRT_C_FUNC int __SPRT_ID(_CFRunLoopIsWaiting)(__SPRT_ID(_CFRunLoopRef) rl) {
	return CFRunLoopIsWaiting((CFRunLoopRef)rl);
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopWakeUp)(__SPRT_ID(_CFRunLoopRef) rl) {
	CFRunLoopWakeUp((CFRunLoopRef)rl);
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopStop)(__SPRT_ID(_CFRunLoopRef) rl) {
	CFRunLoopStop((CFRunLoopRef)rl);
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopPerformBlock)(__SPRT_ID(_CFRunLoopRef) rl, const void *mode,
		void (^block)(void)) {
	CFRunLoopPerformBlock((CFRunLoopRef)rl, mode, block);
}

__SPRT_C_FUNC void __SPRT_ID(
		_CFRunLoopAddCommonMode)(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopMode) mode) {
	CFRunLoopAddCommonMode((CFRunLoopRef)rl, (CFRunLoopMode)mode);
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopTimerRef) __SPRT_ID(
		_CFRunLoopTimerCreate)(__SPRT_ID(_CFAllocatorRef) alloc,
		__SPRT_ID(_CFAbsoluteTime) fireDate, __SPRT_ID(_CFTimeInterval) interval,
		__SPRT_ID(_CFOptionFlags) flags, __SPRT_ID(_CFIndex) order,
		__SPRT_ID(_CFRunLoopTimerCallBack) callout, __SPRT_ID(_CFRunLoopTimerContext) * context) {
	return (__SPRT_ID(_CFRunLoopTimerRef))CFRunLoopTimerCreate((CFAllocatorRef)alloc, fireDate,
			interval, flags, order, (CFRunLoopTimerCallBack)callout,
			(CFRunLoopTimerContext *)context);
}

__SPRT_C_FUNC int __SPRT_ID(_CFRunLoopContainsTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	return CFRunLoopContainsTimer((CFRunLoopRef)rl, (CFRunLoopTimerRef)timer, (CFRunLoopMode)mode);
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopAddTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	CFRunLoopAddTimer((CFRunLoopRef)rl, (CFRunLoopTimerRef)timer, (CFRunLoopMode)mode);
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopRemoveTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	CFRunLoopRemoveTimer((CFRunLoopRef)rl, (CFRunLoopTimerRef)timer, (CFRunLoopMode)mode);
}

#else

extern "C" const __SPRT_ID(_CFRunLoopMode) __SPRT_ID(_kCFRunLoopDefaultMode) = nullptr;

extern "C" const __SPRT_ID(_CFRunLoopMode) __SPRT_ID(_kCFRunLoopCommonModes) = nullptr;

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_lock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC bool __SPRT_ID(_os_unfair_lock_trylock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return false;
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_unlock)(
		__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_assert_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_assert_not_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_os_unfair_lock_lock_with_flags)(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address_with_deadline)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wait_on_address_with_timeout)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wake_by_address_any)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_os_sync_wake_by_address_all)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return -1;
}

__SPRT_C_FUNC void __SPRT_ID(_CFRelease)(void *ref) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC __SPRT_ID(_CFAbsoluteTime) __SPRT_ID(_CFAbsoluteTimeGetCurrent)(void) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return 0);
}

__SPRT_C_FUNC __SPRT_ID(_CFStringRef) __SPRT_ID(
		_CFStringCreateWithUTF8String)(__SPRT_ID(_CFAllocatorRef) alloc, const char *cStr) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return nullptr;
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetCurrent)(void) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return nullptr;
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRef) __SPRT_ID(_CFRunLoopGetMain)(void) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return nullptr;
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopRun)(void) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopRunResult)
		__SPRT_ID(_CFRunLoopRunInMode)(__SPRT_ID(_CFRunLoopMode) mode,
				__SPRT_ID(_CFTimeInterval) seconds, int returnAfterSourceHandled) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_CFRunLoopIsWaiting)(__SPRT_ID(_CFRunLoopRef) rl) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return 0;
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopWakeUp)(__SPRT_ID(_CFRunLoopRef) rl) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopStop)(__SPRT_ID(_CFRunLoopRef) rl) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopPerformBlock)(__SPRT_ID(_CFRunLoopRef) rl, const void *mode,
		void (^block)(void)) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(
		_CFRunLoopAddCommonMode)(__SPRT_ID(_CFRunLoopRef) rl, __SPRT_ID(_CFRunLoopMode) mode) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC __SPRT_ID(_CFRunLoopTimerRef) __SPRT_ID(
		_CFRunLoopTimerCreate)(__SPRT_ID(_CFAllocatorRef) alloc,
		__SPRT_ID(_CFAbsoluteTime) fireDate, __SPRT_ID(_CFTimeInterval) interval,
		__SPRT_ID(_CFOptionFlags) flags, __SPRT_ID(_CFIndex) order,
		__SPRT_ID(_CFRunLoopTimerCallBack) callout, __SPRT_ID(_CFRunLoopTimerContext) * context) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return nullptr;
}

__SPRT_C_FUNC int __SPRT_ID(_CFRunLoopContainsTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
	return 0;
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopAddTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

__SPRT_C_FUNC void __SPRT_ID(_CFRunLoopRemoveTimer)(__SPRT_ID(_CFRunLoopRef) rl,
		__SPRT_ID(_CFRunLoopTimerRef) timer, __SPRT_ID(_CFRunLoopMode) mode) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_DARWIN)");
}

#endif

} // namespace sprt

#if SPRT_MACOS
#include "../platform/darwin/unicode.cc"
#endif
