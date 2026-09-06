// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// sys/__sprt_darwin.h <-> the real os/ + CoreFoundation declarations.
//
// This is the Darwin analogue of the Windows harness's wrappers/windows/abi/*
// checks: a header where sprt re-declares a *platform* API by hand rather than
// including the vendor's, so that the runtime can be built without the SDK.
// runtime/core/darwin/sprt_lock.cc calls os_sync_wait_on_address() as its futex
// and runtime/src/dispatch/platform/darwin/SPEvent-runloop.cc drives a real
// CFRunLoop through these declarations, so a wrong flag value or a mistyped
// parameter is a live ABI break, not a compile-time nicety.
//
// Beyond values and layouts this TU compares whole *function types*: sprt's
// declaration and the vendor's must be the same type, which catches a widened
// parameter or a changed return type that no constant check would see.
//
// Compile-time only; see check.sh.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <stdint.h>
#include <os/lock.h>
#include <os/clock.h>
#include <os/os_sync_wait_on_address.h>
#include <CoreFoundation/CoreFoundation.h>

#define SPRT_ABI_HEADER <sprt/c/sys/__sprt_darwin.h>
#include "abi_check.h"

// === os_unfair_lock ========================================================
// OS_UNFAIR_LOCK_FLAG_NONE / _ADAPTIVE_SPIN belong to the macOS 15
// os_unfair_lock_lock_with_flags() API. The 14.5 SDK does not declare them at
// all; the +open sysroot does, because open-sysroot.mk splices the 15.0-style
// declarations in from open/patches/os-lock-flags.h (tsan's Darwin interceptors
// gate on __MAC_15_0). So they are pinned only where the headers actually have
// them -- against the SDK this block simply is not compiled, which is the same
// situation that puts the symbol itself in tbd-exceptions.txt.
#ifdef __MAC_15_0
SPRT_CONST(OS_UNFAIR_LOCK_FLAG_NONE);
SPRT_CONST(OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN);
#endif
static_assert(__SPRT_OS_UNFAIR_LOCK_INIT == 0,
		"os_unfair_lock's zero-init contract changed");
static_assert(sizeof(os_unfair_lock) == sizeof(uint32_t)
				&& alignof(os_unfair_lock) == alignof(uint32_t),
		"os_unfair_lock is no longer a bare 32-bit word -- sprt_lock.cc embeds it");

// === os_sync_wait_on_address (the Darwin futex) ============================
SPRT_CONST_MAP(OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_SYNC_WAIT_ON_ADDRESS_NONE);
SPRT_CONST_MAP(OS_SYNC_WAIT_ON_ADDRESS_SHARED, OS_SYNC_WAIT_ON_ADDRESS_SHARED);
SPRT_CONST_MAP(OS_SYNC_WAKE_BY_ADDRESS_NONE, OS_SYNC_WAKE_BY_ADDRESS_NONE);
SPRT_CONST_MAP(OS_SYNC_WAKE_BY_ADDRESS_SHARED, OS_SYNC_WAKE_BY_ADDRESS_SHARED);
SPRT_CONST_MAP(OS_CLOCK_MACH_ABSOLUTE_TIME, OS_CLOCK_MACH_ABSOLUTE_TIME);

SPRT_SIGNATURE(__sprt__os_sync_wait_on_address, os_sync_wait_on_address);
SPRT_SIGNATURE(__sprt__os_sync_wait_on_address_with_deadline,
		os_sync_wait_on_address_with_deadline);
SPRT_SIGNATURE(__sprt__os_sync_wait_on_address_with_timeout,
		os_sync_wait_on_address_with_timeout);
SPRT_SIGNATURE(__sprt__os_sync_wake_by_address_any, os_sync_wake_by_address_any);
SPRT_SIGNATURE(__sprt__os_sync_wake_by_address_all, os_sync_wake_by_address_all);

// === CoreFoundation scalar types ===========================================
SPRT_TYPE_SIZE(__sprt__CFTypeID, CFTypeID);
SPRT_TYPE_SIGN(__sprt__CFTypeID, CFTypeID);
SPRT_TYPE_SIZE(__sprt__CFOptionFlags, CFOptionFlags);
SPRT_TYPE_SIGN(__sprt__CFOptionFlags, CFOptionFlags);
SPRT_TYPE_SIZE(__sprt__CFHashCode, CFHashCode);
SPRT_TYPE_SIGN(__sprt__CFHashCode, CFHashCode);
SPRT_TYPE_SIZE(__sprt__CFIndex, CFIndex);
SPRT_TYPE_SIGN(__sprt__CFIndex, CFIndex);
static_assert(sizeof(sprt_abi::__sprt__CFTimeInterval) == sizeof(CFTimeInterval),
		"CFTimeInterval width != Darwin");
static_assert(sizeof(sprt_abi::__sprt__CFAbsoluteTime) == sizeof(CFAbsoluteTime),
		"CFAbsoluteTime width != Darwin");

// === CFRunLoop =============================================================
static_assert((long long)sprt_abi::__sprt__kCFRunLoopRunFinished == (long long)kCFRunLoopRunFinished,
		"sprt _kCFRunLoopRunFinished != Darwin kCFRunLoopRunFinished");
static_assert((long long)sprt_abi::__sprt__kCFRunLoopRunStopped == (long long)kCFRunLoopRunStopped,
		"sprt _kCFRunLoopRunStopped != Darwin kCFRunLoopRunStopped");
static_assert((long long)sprt_abi::__sprt__kCFRunLoopRunTimedOut == (long long)kCFRunLoopRunTimedOut,
		"sprt _kCFRunLoopRunTimedOut != Darwin kCFRunLoopRunTimedOut");
static_assert((long long)sprt_abi::__sprt__kCFRunLoopRunHandledSource == (long long)kCFRunLoopRunHandledSource,
		"sprt _kCFRunLoopRunHandledSource != Darwin kCFRunLoopRunHandledSource");

SPRT_SIZE(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext);
SPRT_ALIGN(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext);
SPRT_OFFSET(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext, version);
SPRT_OFFSET(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext, info);
SPRT_OFFSET(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext, retain);
SPRT_OFFSET(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext, release);
SPRT_OFFSET(__sprt__CFRunLoopTimerContext, CFRunLoopTimerContext, copyDescription);

SPRT_SIGNATURE(__sprt__CFRunLoopGetCurrent, CFRunLoopGetCurrent);
SPRT_SIGNATURE(__sprt__CFRunLoopGetMain, CFRunLoopGetMain);
SPRT_SIGNATURE(__sprt__CFRunLoopRun, CFRunLoopRun);
SPRT_SIGNATURE(__sprt__CFAbsoluteTimeGetCurrent, CFAbsoluteTimeGetCurrent);
