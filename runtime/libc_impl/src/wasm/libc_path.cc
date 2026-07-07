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

// The path / file / directory libc family for wasm. A persistent filesystem
// (OPFS-backed, PROXY open) and an in-memory path tree over the memfs nodes are a
// later milestone; until then these entry points fail with ENOSYS so freestanding
// code links and degrades gracefully rather than silently misbehaving. getcwd
// reports "/" (the only meaningful path pre-VFS). __strcoll is real: the wasm
// runtime is C-locale only, so collation is a plain byte comparison.

#include "dirent.h"
#include "fcntl.h"
#include "stdio.h"
#include "unistd.h"
#include "sys/stat.h"
#include "string.h"

#include "../../include/__impl_libc.h"
#include <sprt/c/__sprt_errno.h>

// --- open / stdio path entry points --------------------------------------

__SPRT_C_FUNC int open(const char *, int, ...) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC FILE *fopen(const char *, const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC int remove(const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

// --- path metadata / manipulation ----------------------------------------

__SPRT_C_FUNC int access(const char *, int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int unlink(const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int rmdir(const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int mkdir(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int ftruncate(int, __SPRT_ID(off_t)) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC char *getcwd(char *buf, __SPRT_ID(size_t) size) __SPRT_NOEXCEPT {
	if (buf && size >= 2) {
		buf[0] = '/';
		buf[1] = '\0';
		return buf;
	}
	__sprt_errno = ERANGE;
	return nullptr;
}

// --- directory streams ---------------------------------------------------

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) *, long) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

// --- collation (C locale) ------------------------------------------------

__SPRT_C_FUNC int __strcoll(const char *l, const char *r) __SPRT_NOEXCEPT {
	if (!l || !r) {
		return 0;
	}
	return strcmp(l, r);
}
