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

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/log.h>

#include "private/SPRTSpecific.h"

#if SPRT_WINDOWS
#include "../platform/windows/mman.cc"
#else
#include <sys/mman.h>
#endif

namespace sprt {

__SPRT_C_FUNC void *__SPRT_ID(mmap)(void *__addr, __SPRT_ID(size_t) __size, int __prot, int __flags,
		int __fd, __SPRT_ID(off_t) __offset) {
	return mmap64(__addr, __size, __prot, __flags, __fd, __offset);
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MREMAP)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(munlockall)(void) {
#if __SPRT_CONFIG_HAVE_MMAN_MLOCKALL
	return ::munlockall();
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return mlock2(__addr, __size, __flags);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(madvise)(void *__addr, __SPRT_ID(size_t) __size, int __flags) {
	return madvise(__addr, __size, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(mincore)(void *__addr, __SPRT_ID(size_t) __size, unsigned char *__vec) {
	return mincore(__addr, __size, __vec);
}

__SPRT_C_FUNC int __SPRT_ID(memfd_create)(const char *name, unsigned flags) {
#if __SPRT_CONFIG_HAVE_MMAN_MEMFD
#if SPRT_ANDROID
	if (platform::_memfd_create) {
		return platform::_memfd_create(name, flags);
	}
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::memfd_create(name, flags);
#endif
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_MMAN_MEMFD)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

} // namespace sprt
