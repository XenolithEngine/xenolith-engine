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

// The path / file / directory libc family for wasm, over the memfs registry defined in
// libc_file_ops.cc (same TU). Backends by mount (see SPRuntimeFilesystem-wasm): tmp and
// cwd-relative -> in-memory tmpfs; LocationCategory::Bundled -> read-only browser fetch
// (loaded on open via __memfs_load_bundle); persistent -> OPFS (later, with workers).
// cwd is "/". __strcoll is a plain byte compare (C locale only).

#include "dirent.h"
#include "fcntl.h"
#include "stdio.h"
#include "unistd.h"
#include "sys/stat.h"
#include "string.h"
#include "stdarg.h"

#include "../../include/__impl_libc.h"
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>

// dirent d_type values (BSD/Linux): directory / regular file.
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif

namespace sprt {

// Fill a stat buffer from an inode (shared by stat/lstat/access paths).
static void __memfs_fill_stat(const __memfs_inode *ino, struct __SPRT_STAT_NAME *st) {
	__builtin_memset(st, 0, sizeof(*st));
	st->st_nlink = 1;
	st->st_blksize = 65536;
	if (ino->isDir) {
		st->st_mode = __SPRT_S_IFDIR | (ino->mode ? (ino->mode & 0777) : 0755);
	} else {
		st->st_mode = __SPRT_S_IFREG | (ino->mode ? (ino->mode & 0777) : 0644);
		st->st_size = (__SPRT_ID(off_t))ino->size;
		st->st_blocks = (__SPRT_ID(blkcnt_t))((ino->size + 511) / 512);
	}
}

// A directory stream: a snapshot of the immediate children taken at opendir().
struct __dirstream {
	char **names; // heap: array of basename strings
	bool *isdir; // parallel: whether each child is a directory
	int count;
	int idx;
	struct __SPRT_DIRENT_NAME ent;
};

} // namespace sprt

// --- open / stdio path entry points --------------------------------------

__SPRT_C_FUNC int open(const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))va_arg(ap, int);
		va_end(ap);
	}
	return sprt::__memfs_openfd(path, flags, mode);
}

// Translate an fopen() mode string to open() flags.
static int __fopen_flags(const char *m) {
	bool plus = false;
	for (const char *p = m; *p; ++p) {
		if (*p == '+') {
			plus = true;
		}
	}
	switch (m[0]) {
	case 'r': return plus ? __SPRT_O_RDWR : __SPRT_O_RDONLY;
	case 'w': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_TRUNC;
	case 'a': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_APPEND;
	default: return -1;
	}
}

__SPRT_C_FUNC FILE *fopen(const char *path, const char *mode) __SPRT_NOEXCEPT {
	int flags = __fopen_flags(mode);
	if (flags < 0) {
		__sprt_errno = EINVAL;
		return nullptr;
	}
	int fd = sprt::__memfs_openfd(path, flags, 0644);
	if (fd < 0) {
		return nullptr;
	}
	FILE *f = fdopen(fd, mode);
	if (!f) {
		close(fd);
	}
	return f;
}

__SPRT_C_FUNC int remove(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	auto ino = sprt::__memfs_find(abs);
	if (!ino) {
		__sprt_errno = ENOENT;
		return -1;
	}
	return sprt::__memfs_unlink_node(abs, ino->isDir) ? 0 : -1;
}

// --- path metadata / manipulation ----------------------------------------

// libc_impl provides the plain path-stat; the wrapper's __sprt_stat forwards here on the
// freestanding (wasm) path.
__SPRT_C_FUNC int stat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	auto ino = sprt::__memfs_find(abs);
	if (!ino) {
		ino = sprt::__memfs_load_bundle(abs);
	}
	if (!ino) {
		__sprt_errno = ENOENT;
		return -1;
	}
	sprt::__memfs_fill_stat(ino, st);
	return 0;
}

__SPRT_C_FUNC int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	return stat(path, st); // no symlinks in memfs
}

__SPRT_C_FUNC int access(const char *path, int) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	auto ino = sprt::__memfs_find(abs);
	if (!ino) {
		ino = sprt::__memfs_load_bundle(abs);
	}
	if (!ino) {
		__sprt_errno = ENOENT;
		return -1;
	}
	return 0; // permission model: everything accessible
}

__SPRT_C_FUNC int unlink(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	return sprt::__memfs_unlink_node(abs, false) ? 0 : -1;
}

__SPRT_C_FUNC int rmdir(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	return sprt::__memfs_unlink_node(abs, true) ? 0 : -1;
}

__SPRT_C_FUNC int mkdir(const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (sprt::__memfs_find(abs)) {
		__sprt_errno = EEXIST;
		return -1;
	}
	return sprt::__memfs_create(abs, true, mode & 0777) ? 0 : -1;
}

__SPRT_C_FUNC int rename(const char *from, const char *to) __SPRT_NOEXCEPT {
	char a[512], b[512];
	if (!sprt::__memfs_normpath(from, a, sizeof(a)) || !sprt::__memfs_normpath(to, b, sizeof(b))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	auto ino = sprt::__memfs_find(a);
	if (!ino) {
		__sprt_errno = ENOENT;
		return -1;
	}
	sprt::__memfs_unlink_node(b, ino->isDir); // best-effort replace of the destination
	__SPRT_ID(size_t) blen = __builtin_strlen(b);
	char *np = (char *)__sprt_malloc(blen + 1);
	if (!np) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	__builtin_memcpy(np, b, blen + 1);
	__sprt_free(ino->path);
	ino->path = np;
	return 0;
}

__SPRT_C_FUNC int ftruncate(int fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	auto h = (sprt::__wasm_fnode *)sprt::__libc::get()->get_fd_handle(fd);
	if (!h || h->isConsole || !h->ino) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (length < 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if ((__SPRT_ID(size_t))length > h->ino->size) {
		if (!sprt::__memfs_reserve(h->ino, (__SPRT_ID(size_t))length)) {
			return -1;
		}
		__builtin_memset(h->ino->data + h->ino->size, 0,
				(__SPRT_ID(size_t))length - h->ino->size);
	}
	h->ino->size = (__SPRT_ID(size_t))length;
	return 0;
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

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return nullptr;
	}
	bool isRoot = (abs[0] == '/' && abs[1] == '\0');
	if (!isRoot) {
		auto d = sprt::__memfs_find(abs);
		if (!d) {
			__sprt_errno = ENOENT;
			return nullptr;
		}
		if (!d->isDir) {
			__sprt_errno = ENOTDIR;
			return nullptr;
		}
	}
	auto s = (sprt::__dirstream *)__sprt_malloc(sizeof(sprt::__dirstream));
	if (!s) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	s->names = nullptr;
	s->isdir = nullptr;
	s->count = 0;
	s->idx = 0;
	__SPRT_ID(size_t) plen = __builtin_strlen(abs);
	// A child's path is "<abs>/<basename>" (or "/<basename>" at root) with no more '/'.
	auto childBase = [&](const char *cp) -> const char * {
		if (__builtin_strncmp(cp, abs, plen) != 0) {
			return nullptr;
		}
		const char *rest;
		if (isRoot) {
			rest = cp + 1;
		} else {
			if (cp[plen] != '/') {
				return nullptr;
			}
			rest = cp + plen + 1;
		}
		if (*rest == '\0' || __builtin_strchr(rest, '/')) {
			return nullptr;
		}
		return rest;
	};
	int nchild = 0;
	for (auto n = sprt::s_memfs; n; n = n->next) {
		if (childBase(n->path)) {
			++nchild;
		}
	}
	// Entries: "." and ".." (both directories) followed by the immediate children.
	int total = nchild + 2;
	s->names = (char **)__sprt_malloc((__SPRT_ID(size_t))total * sizeof(char *));
	s->isdir = (bool *)__sprt_malloc((__SPRT_ID(size_t))total * sizeof(bool));
	auto dot = [&](const char *d) {
		__SPRT_ID(size_t) dl = __builtin_strlen(d);
		char *nm = (char *)__sprt_malloc(dl + 1);
		__builtin_memcpy(nm, d, dl + 1);
		return nm;
	};
	s->names[0] = dot(".");
	s->isdir[0] = true;
	s->names[1] = dot("..");
	s->isdir[1] = true;
	int i = 2;
	for (auto n = sprt::s_memfs; n && i < total; n = n->next) {
		const char *rest = childBase(n->path);
		if (!rest) {
			continue;
		}
		__SPRT_ID(size_t) rl = __builtin_strlen(rest);
		char *nm = (char *)__sprt_malloc(rl + 1);
		__builtin_memcpy(nm, rest, rl + 1);
		s->names[i] = nm;
		s->isdir[i] = n->isDir;
		++i;
	}
	s->count = i;
	return reinterpret_cast<__SPRT_ID(DIR) *>(s);
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) * dir) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s || s->idx >= s->count) {
		return nullptr;
	}
	int i = s->idx++;
	__builtin_memset(&s->ent, 0, sizeof(s->ent));
	__SPRT_ID(size_t) rl = __builtin_strlen(s->names[i]);
	if (rl > 255) {
		rl = 255;
	}
	__builtin_memcpy(s->ent.d_name, s->names[i], rl);
	s->ent.d_name[rl] = '\0';
	s->ent.d_ino = (__SPRT_ID(ino_t))(i + 1);
	s->ent.d_type = s->isdir[i] ? DT_DIR : DT_REG;
	// d_reclen is the byte size of this record; scandir() copies exactly this many bytes,
	// so a zero here would yield empty-name/DT_UNKNOWN entries.
	s->ent.d_reclen = (unsigned short)sizeof(s->ent);
	return &s->ent;
}

__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) * dir) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s) {
		__sprt_errno = EBADF;
		return -1;
	}
	for (int i = 0; i < s->count; ++i) {
		__sprt_free(s->names[i]);
	}
	if (s->names) {
		__sprt_free(s->names);
	}
	if (s->isdir) {
		__sprt_free(s->isdir);
	}
	__sprt_free(s);
	return 0;
}

__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) * dir) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s) {
		__sprt_errno = EBADF;
		return -1;
	}
	s->idx = 0;
	return 0;
}

__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) * dir, long loc) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s) {
		__sprt_errno = EBADF;
		return -1;
	}
	s->idx = (loc < 0) ? 0 : (int)loc;
	return 0;
}

__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) * dir) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s) {
		__sprt_errno = EBADF;
		return -1;
	}
	return (long)s->idx;
}

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOTSUP;
	return -1;
}

// --- *at variants / links / realpath -------------------------------------
// memfs is a single flat namespace with no per-fd directories and no symlinks; the *at
// forms therefore ignore the dir fd (paths are resolved against the "/" root, matching
// AT_FDCWD), and the link family reports the appropriate "unsupported" errno.

__SPRT_C_FUNC int openat(int, const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))va_arg(ap, int);
		va_end(ap);
	}
	return sprt::__memfs_openfd(path, flags, mode);
}

__SPRT_C_FUNC int mkdirat(int, const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return mkdir(path, mode);
}

__SPRT_C_FUNC int linkat(int, const char *, int, const char *, int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS; // memfs has no hard links
	return -1;
}

__SPRT_C_FUNC int symlink(const char *, const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS; // memfs has no symbolic links
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) readlink(const char *path, char *, __SPRT_ID(size_t))
		__SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	// A path that exists but is not a symlink -> EINVAL; otherwise ENOENT.
	__sprt_errno = sprt::__memfs_find(abs) ? EINVAL : ENOENT;
	return -1;
}

__SPRT_C_FUNC char *realpath(const char *path, char *resolved) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return nullptr;
	}
	if (!sprt::__memfs_find(abs) && !sprt::__memfs_load_bundle(abs)) {
		__sprt_errno = ENOENT;
		return nullptr;
	}
	__SPRT_ID(size_t) len = __builtin_strlen(abs);
	char *out = resolved ? resolved : (char *)__sprt_malloc(len + 1);
	if (!out) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	__builtin_memcpy(out, abs, len + 1);
	return out;
}

// --- collation (C locale) ------------------------------------------------

__SPRT_C_FUNC int __strcoll(const char *l, const char *r) __SPRT_NOEXCEPT {
	if (!l || !r) {
		return 0;
	}
	return strcmp(l, r);
}
