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

#define __SPRT_BUILD 1
// musl only exposes the *64 file API when _LARGEFILE64_SOURCE is set (its plain
// symbols are already 64-bit); on glibc this is already implied by _GNU_SOURCE.
#define _LARGEFILE64_SOURCE 1

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/log.h>

#include <sys/mman.h>

#if SPRT_ANDROID
#include "../src/private/SPRTSpecific.h"
#endif

namespace sprt {

// The wrappers below forward the runtime __SPRT_PROT_*/__SPRT_MAP_*/__SPRT_MS_*/
// __SPRT_MADV_* values straight to the native mman(2) calls, so the runtime
// constants must equal the host's. Assert the portable set the wrappers rely on
// (same pattern as the kevent/darwin wrappers); a future drift fails the build.
// The POSIX core below is identical on Linux/macOS/Android.
#if !SPRT_NUTTX
static_assert(PROT_NONE == __SPRT_PROT_NONE);
static_assert(PROT_READ == __SPRT_PROT_READ);
static_assert(PROT_WRITE == __SPRT_PROT_WRITE);
static_assert(PROT_EXEC == __SPRT_PROT_EXEC);

static_assert(MAP_SHARED == __SPRT_MAP_SHARED);
static_assert(MAP_PRIVATE == __SPRT_MAP_PRIVATE);
static_assert(MAP_FIXED == __SPRT_MAP_FIXED);

static_assert(MS_ASYNC == __SPRT_MS_ASYNC);
static_assert(MS_INVALIDATE == __SPRT_MS_INVALIDATE);

static_assert(MADV_NORMAL == __SPRT_MADV_NORMAL);
static_assert(MADV_RANDOM == __SPRT_MADV_RANDOM);
static_assert(MADV_SEQUENTIAL == __SPRT_MADV_SEQUENTIAL);
static_assert(MADV_WILLNEED == __SPRT_MADV_WILLNEED);
static_assert(MADV_DONTNEED == __SPRT_MADV_DONTNEED);

static_assert(MCL_CURRENT == __SPRT_MCL_CURRENT);
static_assert(MCL_FUTURE == __SPRT_MCL_FUTURE);

static_assert(MAP_ANON == __SPRT_MAP_ANON);
static_assert(MAP_ANONYMOUS == __SPRT_MAP_ANONYMOUS);
static_assert(MAP_NORESERVE == __SPRT_MAP_NORESERVE);
static_assert(MS_SYNC == __SPRT_MS_SYNC);
static_assert(MADV_FREE == __SPRT_MADV_FREE);
#endif // !SPRT_NUTTX

__SPRT_C_FUNC void *__SPRT_ID(mmap)(void *__addr, __SPRT_ID(size_t) __size, int __prot, int __flags,
		int __fd, __SPRT_ID(off_t) __offset) {
#if SPRT_APPLE || SPRT_NUTTX
	return mmap(__addr, __size, __prot, __flags, __fd, __offset);
#else
	return mmap64(__addr, __size, __prot, __flags, __fd, __offset);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(munmap)(void *__addr, __SPRT_ID(size_t) __size) {
	return munmap(__addr, __size);
}

__SPRT_C_FUNC int __SPRT_ID(mprotect)(void *__addr, __SPRT_ID(size_t) __size, int __flags) {
	return mprotect(__addr, __size, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(msync)(void *__addr, __SPRT_ID(size_t) __size, int __flags) {
	return msync(__addr, __size, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(posix_madvise)(void *__addr, __SPRT_ID(size_t) __size, int __flags) {
	return posix_madvise(__addr, __size, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(mlock)(const void *__addr, __SPRT_ID(size_t) __size) {
	return mlock(__addr, __size);
}
__SPRT_C_FUNC int __SPRT_ID(munlock)(const void *__addr, __SPRT_ID(size_t) __size) {
	return munlock(__addr, __size);
}

__SPRT_C_FUNC int __SPRT_ID(mlockall)(int __flags) {
#if __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
	return ::mlockall(__flags);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MREMAP)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(munlockall)(void) {
#if __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
	return ::munlockall();
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MREMAP)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC void *__SPRT_ID(mremap)(void *__addr, __SPRT_ID(size_t) __old_size,
		__SPRT_ID(size_t) __new_size, int __flags, ...) {
#if __SPRT_CONFIG_HAVE_MMAN_MREMAP
	__sprt_va_list ap;
	void *new_addr = 0;
	if (__flags & MREMAP_FIXED) {
		__sprt_va_start(ap, __flags);
		new_addr = __sprt_va_arg(ap, void *);
		__sprt_va_end(ap);
	}

	return ::mremap(__addr, __old_size, __new_size, __flags, new_addr);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MREMAP)");
	__sprt_errno = ENOSYS;
	return __SPRT_MAP_FAILED;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(mlock2)(const void *__addr, __SPRT_ID(size_t) __size, int __flags) {
#if SPRT_ANDROID
	if (platform::_mlock2) {
		return platform::_mlock2(__addr, __size, __flags);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_APPLE || SPRT_NUTTX
	if (__flags == 0) {
		return mlock(__addr, __size);
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
#else
	return mlock2(__addr, __size, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(madvise)(void *__addr, __SPRT_ID(size_t) __size, int __flags) {
	return madvise(__addr, __size, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(mincore)(void *__addr, __SPRT_ID(size_t) __size, unsigned char *__vec) {
#if SPRT_NUTTX
	// NuttX has no mincore.
	(void)__addr; (void)__size; (void)__vec;
	*__sprt___errno_location() = ENOSYS;
	return -1;
#elif SPRT_APPLE
	return mincore(__addr, __size, (char *)__vec);
#else
	return mincore(__addr, __size, __vec);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(memfd_create)(const char *name, unsigned flags) {
#if __SPRT_CONFIG_HAVE_MMAN_MEMFD
#if SPRT_ANDROID
	if (platform::_memfd_create) {
		return platform::_memfd_create(name, flags);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::memfd_create(name, flags);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MEMFD)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

} // namespace sprt
