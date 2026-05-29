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

#include <sprt/c/__sprt_unistd.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utime.h>

#if SPRT_WINDOWS
#include "windows/unistd.cc"
#endif

__SPRT_C_FUNC char *__strchrnul(const char *s, int c);

namespace sprt {

__SPRT_C_FUNC ssize_t read(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_read) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_read(fdSlot, __buf, __nbytes, nullptr, 0);
}

__SPRT_C_FUNC ssize_t write(int __fd, const void *__buf,
		__SPRT_ID(size_t) __nbytes) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_write) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_write(fdSlot, __buf, __nbytes, nullptr, 0);
}

__SPRT_C_FUNC ssize_t pread64(int __fd, void *__buf, size_t __nbytes,
		off64_t __offset) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_read) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_read(fdSlot, __buf, __nbytes, &__offset, 0);
}

__SPRT_C_FUNC ssize_t pwrite64(int __fd, const void *__buf, size_t __nbytes,
		off64_t __offset) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_write) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_write(fdSlot, __buf, __nbytes, &__offset, 0);
}

__SPRT_C_FUNC ssize_t readv(int __fd, const struct iovec *iov, int iovcnt) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_readv) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_readv(fdSlot, iov, iovcnt);
}

__SPRT_C_FUNC ssize_t writev(int __fd, const struct iovec *iov, int iovcnt) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_writev) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_writev(fdSlot, iov, iovcnt);
}

__SPRT_C_FUNC int close(int __fd) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_close) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (fdSlot->ops->fo_close(fdSlot) == 0) {
		libc->release_fd(__fd);
		return 0;
	}
	return -1;
}

__SPRT_C_FUNC int dup(int __fd) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_dup) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_dup(fdSlot, nullptr, 0);
}

__SPRT_C_FUNC int dup2(int __fd, int __target) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_dup) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (__fd == __target) {
		return __target;
	}

	return fdSlot->ops->fo_dup(fdSlot, &__target, 0);
}

__SPRT_C_FUNC int dup3(int __fd, int __target, int __flags) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_dup) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (__fd == __target) {
		__sprt_errno = EINVAL;
		return -1;
	}

	if ((__flags & ~__SPRT_FD_CLOEXEC) != 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	return fdSlot->ops->fo_dup(fdSlot, nullptr, __flags);
}

__SPRT_C_FUNC off_t lseek(int __fd, off_t off, int whence) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_seek) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_seek(fdSlot, off, whence);
}

__SPRT_C_FUNC int execv(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT {
	return execve(__path, __argv, nullptr);
}

__SPRT_C_FUNC int execvp(const char *__path, char *const __argv[]) __SPRT_NOEXCEPT {
	return execvpe(__path, __argv, nullptr);
}

__SPRT_C_FUNC int execvpe(const char *file, char *const argv[],
		char *const envp[]) __SPRT_NOEXCEPT {
	const char *p, *z, *path = getenv("PATH");
	size_t l, k;
	int seen_eacces = 0;

	errno = ENOENT;
	if (!*file) {
		return -1;
	}

	if (strchr(file, '/')) {
		return execve(file, argv, envp);
	}

	if (!path) {
		path = "/usr/local/bin:/bin:/usr/bin";
	}
	k = strnlen(file, NAME_MAX + 1);
	if (k > NAME_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}
	l = strnlen(path, PATH_MAX - 1) + 1;

	for (p = path;; p = z) {
		char b[l + k + 1];
		z = __strchrnul(p, ':');
		if (z - p >= l) {
			if (!*z++) {
				break;
			}
			continue;
		}
		memcpy(b, p, z - p);
		b[z - p] = '/';
		memcpy(b + (z - p) + (z > p), file, k + 1);
		execve(b, argv, envp);
		switch (errno) {
		case EACCES: seen_eacces = 1;
		case ENOENT:
		case ENOTDIR: break;
		default: return -1;
		}
		if (!*z++) {
			break;
		}
	}
	if (seen_eacces) {
		errno = EACCES;
	}
	return -1;
}

__SPRT_C_FUNC int isatty(int fd) __SPRT_NOEXCEPT {
	struct winsize wsz;
	/* +1 converts from error status (0/-1) to boolean (1/0) */
	return ioctl(fd, TIOCGWINSZ, &wsz) + 1;
}

__SPRT_C_FUNC void sync() __SPRT_NOEXCEPT { }

__SPRT_C_FUNC int utime(const char *path, const struct __SPRT_UTIMBUF_NAME *times) __SPRT_NOEXCEPT {
	return utimensat(AT_FDCWD, path,
			times ? ((struct timespec[2]){{.tv_sec = times->actime}, {.tv_sec = times->modtime}})
				  : 0,
			0);
}

} // namespace sprt

namespace sprt::platform {

__attribute__((weak)) StringView getUniqueDeviceId() { return StringView(); }

} // namespace sprt::platform
