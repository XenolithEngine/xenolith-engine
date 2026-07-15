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

#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/sys/__sprt_statvfs.h>

#if SPRT_WINDOWS
#include "windows/stat.cc"
#endif

#include "../include/__impl_libc.h"
#include "sys/stat.h" // IWYU pragma: keep

namespace sprt {

__SPRT_C_FUNC int fstat(int __fd, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_stat) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_stat(fdSlot, __stat);
}

__SPRT_C_FUNC int fchmod(int __fd, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_chmod) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_chmod(fdSlot, mode);
}

__SPRT_C_FUNC int futimens(int __fd, const struct __SPRT_TIMESPEC_NAME *times) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_utimens) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_utimens(fdSlot, times);
}

__SPRT_C_FUNC mode_t umask(mode_t mask) __SPRT_NOEXCEPT {
	mask &= 0777;
	return __libc::get()->umask.exchange(mask);
}

#if SPRT_WINDOWS
// Path-based statvfs() lives in windows/stat.cc (WinAPI, included above); fstatvfs()
// resolves the descriptor to its native HANDLE and forwards to the same backend.
__SPRT_C_FUNC int fstatvfs(int __fd, struct __SPRT_STATVFS_NAME *buf) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle) {
		__sprt_errno = EBADF;
		return -1;
	}
	return platform::hstatvfs(fdSlot->handle, buf);
}
#else
// No filesystem-statistics backend on freestanding targets (wasm): report an
// unknown/empty volume as a successful no-op (zeroed buffer), so callers such as
// std::filesystem::space see well-defined values instead of a failure.
__SPRT_C_FUNC int statvfs(const char *__SPRT_RESTRICT path,
		struct __SPRT_STATVFS_NAME *__SPRT_RESTRICT buf) __SPRT_NOEXCEPT {
	(void)path;
	__builtin_memset(buf, 0, sizeof(*buf));
	return 0;
}

__SPRT_C_FUNC int fstatvfs(int __fd, struct __SPRT_STATVFS_NAME *buf) __SPRT_NOEXCEPT {
	(void)__fd;
	__builtin_memset(buf, 0, sizeof(*buf));
	return 0;
}
#endif

} // namespace sprt
