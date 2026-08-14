
/*
 * SPLICED IN by open-sysroot.mk (the libplatform stamp) — not part of libplatform.
 *
 * macOS 15 added the "flags" os_unfair_lock entry point. libplatform-316.100.10 is
 * the macOS 14.5 tag this sysroot pins, so its <os/lock.h> predates it — but
 * AvailabilityVersions-141 does define __MAC_15_0, and compiler-rt's Darwin tsan
 * interceptors (LLVM >= 22) gate the os_unfair_lock_lock_with_flags interceptor on
 * exactly that macro, so the type and the prototype have to exist for the sysroot
 * to be self-consistent.
 *
 * Declared the way the 15.0 SDK declares it: with the deployment target at 14.5 the
 * __API_AVAILABLE annotation turns the reference into a weak import, so a binary
 * linked against this sysroot still loads on macOS 14 (the real SDK's flags type is
 * an OS_OPTIONS enum; a plain uint32_t typedef is ABI-identical and does not need
 * the OS_ENUM machinery, which <os/base.h> here is a shim for).
 *
 * The matching _os_unfair_lock_lock_with_flags export lives in
 * functions_{x86_64,arm64}.txt and the baked libSystem-family .tbds.
 */

typedef uint32_t os_unfair_lock_flags_t;

enum {
	OS_UNFAIR_LOCK_FLAG_NONE = 0x00000000,
	OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN = 0x00040000,
};

__API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0))
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
void os_unfair_lock_lock_with_flags(os_unfair_lock_t lock,
		os_unfair_lock_flags_t flags);
