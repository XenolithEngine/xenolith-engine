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
	st->st_ino = ino->inum;
	if (ino->isLink) {
		// A link's "content" is its target path, so st_size is that path's length
		// (POSIX), and the mode is always 0777 as on Linux.
		st->st_mode = __SPRT_S_IFLNK | 0777;
		st->st_size = (__SPRT_ID(off_t))ino->size;
	} else if (ino->isDir) {
		st->st_mode = __SPRT_S_IFDIR | (ino->mode ? (ino->mode & 0777) : 0755);
	} else {
		st->st_mode = __SPRT_S_IFREG | (ino->mode ? (ino->mode & 0777) : 0644);
		st->st_size = (__SPRT_ID(off_t))ino->size;
		st->st_blocks = (__SPRT_ID(blkcnt_t))((ino->size + 511) / 512);
	}
	st->st_atim = ino->atim;
	st->st_mtim = ino->mtim;
	st->st_ctim = ino->ctim;
}

// A directory stream: a snapshot of the immediate children taken at opendir().
struct __dirstream {
	char **names; // heap: array of basename strings
	bool *isdir; // parallel: whether each child is a directory
	int count;
	int idx;
	int fd; // owned fd for fdopendir() (closed by closedir); -1 for opendir()
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
	if (sprt::__vfs_is_opfs(abs)) {
		__SPRT_ID(size_t) sz = 0;
		bool isdir = false;
		int r = sprt::__opfs_stat(abs, &sz, &isdir);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		r = sprt::__opfs_unlink(abs, isdir);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		if (sprt::__memfs_find(abs)) {
			sprt::__memfs_unlink_node(abs, isdir);
		}
		return 0;
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
	// stat() reports the target of a symlink; lstat() below reports the link.
	if (!sprt::__memfs_resolve_link(abs, abs, sizeof(abs))) {
		return -1;
	}
	auto ino = sprt::__memfs_find(abs);
	if (!ino && sprt::__vfs_is_opfs(abs)) {
		__SPRT_ID(size_t) size = 0;
		bool isdir = false;
		int r = sprt::__opfs_stat(abs, &size, &isdir);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		__builtin_memset(st, 0, sizeof(*st));
		st->st_nlink = 1;
		st->st_blksize = 65536;
		if (isdir) {
			st->st_mode = __SPRT_S_IFDIR | 0755;
		} else {
			st->st_mode = __SPRT_S_IFREG | 0644;
			st->st_size = (__SPRT_ID(off_t))size;
			st->st_blocks = (__SPRT_ID(blkcnt_t))((size + 511) / 512);
		}
		return 0;
	}
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
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	// Report the link itself when there is one; otherwise this is plain stat().
	auto link = sprt::__memfs_find(abs);
	if (link && link->isLink) {
		sprt::__memfs_fill_stat(link, st);
		return 0;
	}
	return stat(abs, st);
}

__SPRT_C_FUNC int access(const char *path, int) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (!sprt::__memfs_resolve_link(abs, abs, sizeof(abs))) {
		return -1;
	}
	auto ino = sprt::__memfs_find(abs);
	if (!ino && sprt::__vfs_is_opfs(abs)) {
		int r = sprt::__opfs_stat(abs, nullptr, nullptr);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		return 0;
	}
	if (!ino) {
		ino = sprt::__memfs_load_bundle(abs);
	}
	if (!ino) {
		__sprt_errno = ENOENT;
		return -1;
	}
	return 0; // permission model: everything accessible
}

// Remove an /opfs node (file if isDir==false, else directory) and drop any cache inode.
static int __opfs_unlink_path(const char *abs, bool isDir) {
	int r = sprt::__opfs_unlink(abs, isDir);
	if (r < 0) {
		__sprt_errno = -r;
		return -1;
	}
	if (sprt::__memfs_find(abs)) {
		sprt::__memfs_unlink_node(abs, isDir);
	}
	return 0;
}

__SPRT_C_FUNC int unlink(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (sprt::__vfs_is_opfs(abs)) {
		return __opfs_unlink_path(abs, false);
	}
	return sprt::__memfs_unlink_node(abs, false) ? 0 : -1;
}

__SPRT_C_FUNC int rmdir(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (sprt::__vfs_is_opfs(abs)) {
		int r = sprt::__opfs_unlink(abs, true);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		if (sprt::__memfs_find(abs)) {
			sprt::__memfs_unlink_node(abs, true);
		}
		return 0;
	}
	return sprt::__memfs_unlink_node(abs, true) ? 0 : -1;
}

__SPRT_C_FUNC int mkdir(const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (sprt::__vfs_is_opfs(abs)) {
		int r = sprt::__opfs_mkdir(abs);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		// Cache a directory inode so later stat/opendir need no round-trip.
		if (!sprt::__memfs_find(abs)) {
			auto d = sprt::__memfs_create(abs, true, mode & 0777);
			if (d) {
				d->opfs = true;
			}
		}
		return 0;
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
	bool fromOpfs = sprt::__vfs_is_opfs(a), toOpfs = sprt::__vfs_is_opfs(b);
	if (fromOpfs || toOpfs) {
		if (fromOpfs != toOpfs) {
			__sprt_errno = EXDEV; // no cross-mount rename (tmpfs <-> opfs)
			return -1;
		}
		int r = sprt::__opfs_rename(a, b);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		// Drop stale cache for both endpoints; they re-hydrate from OPFS on next open.
		if (sprt::__memfs_find(a)) {
			sprt::__memfs_unlink_node(a, false);
		}
		if (sprt::__memfs_find(b)) {
			sprt::__memfs_unlink_node(b, false);
		}
		return 0;
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
	// Renaming a directory moves its whole subtree. The registry is flat and keyed
	// by full path, so every descendant has to be re-keyed by hand — otherwise the
	// children would keep answering at the OLD name, under a directory that no
	// longer exists.
	if (ino->isDir) {
		sprt::__memfs_reparent(a, b);
	}
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
	sprt::__memfs_now(&h->ino->mtim);
	h->ino->ctim = h->ino->mtim;
	h->ino->dirty = true; // write back if OPFS-backed
	return 0;
}

__SPRT_C_FUNC char *getcwd(char *buf, __SPRT_ID(size_t) size) __SPRT_NOEXCEPT {
	if (!buf) {
		// GNU extension (glibc/musl, and what the runtime's callers use): a null
		// buffer means "allocate one for me", with size 0 meaning "as large as
		// needed". The caller frees it.
		if (size != 0 && size < 2) {
			__sprt_errno = ERANGE;
			return nullptr;
		}
		auto p = (char *)__sprt_malloc(size ? size : 2);
		if (!p) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
		p[0] = '/';
		p[1] = '\0';
		return p;
	}
	if (size >= 2) {
		buf[0] = '/';
		buf[1] = '\0';
		return buf;
	}
	__sprt_errno = ERANGE;
	return nullptr;
}

// --- directory streams ---------------------------------------------------

// Build a directory-stream snapshot for an already-normalized absolute path.
// `ownedFd` is stored so fdopendir()'s DIR closes its backing fd at closedir();
// opendir() passes -1.
static __SPRT_ID(DIR) * __memfs_opendir_abs(const char *abs, int ownedFd) {
	bool isRoot = (abs[0] == '/' && abs[1] == '\0');
	if (!isRoot) {
		auto d = sprt::__memfs_find(abs);
		if (!d) {
			d = sprt::__memfs_load_bundle(abs); // read-only Bundled directory
		}
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
	s->fd = ownedFd;
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

// Build a directory-stream snapshot for an /opfs directory: one readdir round-trip to
// the OPFS worker, which serialises entries as "<name>\0<type-byte>". `ownedFd` is the
// fd fdopendir() must close at closedir() (-1 for opendir()).
static __SPRT_ID(DIR) * __opfs_opendir(const char *abs, int ownedFd) {
	__SPRT_ID(size_t) cap = 16 * 1024;
	auto buf = (unsigned char *)__sprt_malloc(cap);
	if (!buf) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	int count = sprt::__opfs_readdir(abs, buf, cap);
	if (count < 0) {
		__sprt_free(buf);
		__sprt_errno = -count;
		return nullptr;
	}
	auto s = (sprt::__dirstream *)__sprt_malloc(sizeof(sprt::__dirstream));
	if (!s) {
		__sprt_free(buf);
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	s->idx = 0;
	s->fd = ownedFd;
	int total = count + 2; // "." and ".."
	s->names = (char **)__sprt_malloc((__SPRT_ID(size_t))total * sizeof(char *));
	s->isdir = (bool *)__sprt_malloc((__SPRT_ID(size_t))total * sizeof(bool));
	auto dup = [&](const char *d, __SPRT_ID(size_t) l) {
		char *nm = (char *)__sprt_malloc(l + 1);
		__builtin_memcpy(nm, d, l);
		nm[l] = '\0';
		return nm;
	};
	s->names[0] = dup(".", 1);
	s->isdir[0] = true;
	s->names[1] = dup("..", 2);
	s->isdir[1] = true;
	int i = 2;
	unsigned char *p = buf;
	unsigned char *endp = buf + cap;
	for (int e = 0; e < count && i < total && p < endp; ++e) {
		auto name = (const char *)p;
		__SPRT_ID(size_t) nl = __builtin_strlen(name);
		p += nl + 1;
		if (p >= endp) {
			break;
		}
		unsigned char type = *p++;
		s->names[i] = dup(name, nl);
		s->isdir[i] = (type == 1);
		++i;
	}
	s->count = i;
	__sprt_free(buf);
	return reinterpret_cast<__SPRT_ID(DIR) *>(s);
}

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *path) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return nullptr;
	}
	if (sprt::__vfs_is_opfs(abs)) {
		return __opfs_opendir(abs, -1);
	}
	return __memfs_opendir_abs(abs, -1);
}

// Adopt an fd previously opened on a directory: the returned DIR owns `fd` and
// closedir() will close it. memfs is a flat namespace, so the snapshot is taken
// from the fd's inode path.
__SPRT_C_FUNC __SPRT_ID(DIR) * fdopendir(int fd) __SPRT_NOEXCEPT {
	auto h = (sprt::__wasm_fnode *)sprt::__libc::get()->get_fd_handle(fd);
	if (!h || h->isConsole || !h->ino) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (!h->ino->isDir) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}
	if (h->ino->opfs) {
		return __opfs_opendir(h->ino->path, fd);
	}
	return __memfs_opendir_abs(h->ino->path, fd);
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
	if (s->fd >= 0) {
		close(s->fd); // fdopendir(): the DIR owns the backing fd
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

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) * dir) __SPRT_NOEXCEPT {
	auto s = reinterpret_cast<sprt::__dirstream *>(dir);
	if (!s) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (s->fd >= 0) {
		return s->fd; // opened via fdopendir(): the DIR owns a real fd
	}
	__sprt_errno = ENOTSUP; // opened via opendir(): no backing fd
	return -1;
}

// --- *at variants / links / realpath -------------------------------------
// memfs is a single flat namespace with no symlinks, but the *at forms honor the dir
// fd for RELATIVE paths (POSIX): a relative path is resolved against the directory the
// fd refers to (its inode's absolute path), so fd-relative tree walks (openat over an
// fdopendir'd directory, as ftw/nftw do) address the right children. Absolute paths and
// AT_FDCWD resolve against the "/" root as before. The link family is unsupported.

namespace sprt {

// Resolve `path` relative to `dirfd` into `buf`. Returns the string to open: `path`
// itself when it is absolute or dirfd is AT_FDCWD/none, else "<dirfd-dir>/<path>".
// Sets errno + returns nullptr if dirfd is not an open directory, or on overflow.
static const char *__resolve_at(int dirfd, const char *path, char *buf,
		__SPRT_ID(size_t) cap) {
	if (path[0] == '/' || dirfd == __SPRT_AT_FDCWD || dirfd < 0) {
		return path;
	}
	auto h = (__wasm_fnode *)__libc::get()->get_fd_handle(dirfd);
	if (!h || h->isConsole || !h->ino) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (!h->ino->isDir) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}
	__SPRT_ID(size_t) dl = __builtin_strlen(h->ino->path);
	__SPRT_ID(size_t) pl = __builtin_strlen(path);
	if (dl + 1 + pl + 1 > cap) {
		__sprt_errno = ENAMETOOLONG;
		return nullptr;
	}
	__builtin_memcpy(buf, h->ino->path, dl);
	buf[dl] = '/';
	__builtin_memcpy(buf + dl + 1, path, pl + 1);
	return buf;
}

} // namespace sprt

__SPRT_C_FUNC int openat(int dirfd, const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))va_arg(ap, int);
		va_end(ap);
	}
	char buf[512];
	auto resolved = sprt::__resolve_at(dirfd, path, buf, sizeof(buf));
	if (!resolved) {
		return -1;
	}
	return sprt::__memfs_openfd(resolved, flags, mode);
}

__SPRT_C_FUNC int mkdirat(int dirfd, const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	char buf[512];
	auto resolved = sprt::__resolve_at(dirfd, path, buf, sizeof(buf));
	if (!resolved) {
		return -1;
	}
	return mkdir(resolved, mode);
}

// memfs has a single permission model, so the requested mode bits and AT_EACCESS
// are irrelevant: only existence is checked, at the dir-fd-resolved path.
__SPRT_C_FUNC int faccessat(int dirfd, const char *path, int, int) __SPRT_NOEXCEPT {
	char buf[512];
	auto resolved = sprt::__resolve_at(dirfd, path, buf, sizeof(buf));
	if (!resolved) {
		return -1;
	}
	return access(resolved, 0);
}

__SPRT_C_FUNC int unlinkat(int dirfd, const char *path, int flags) __SPRT_NOEXCEPT {
	char buf[512];
	auto resolved = sprt::__resolve_at(dirfd, path, buf, sizeof(buf));
	if (!resolved) {
		return -1;
	}
	return (flags & __SPRT_AT_REMOVEDIR) ? rmdir(resolved) : unlink(resolved);
}

__SPRT_C_FUNC int renameat(int oldfd, const char *oldPath, int newfd,
		const char *newPath) __SPRT_NOEXCEPT {
	char oldBuf[512], newBuf[512];
	auto from = sprt::__resolve_at(oldfd, oldPath, oldBuf, sizeof(oldBuf));
	if (!from) {
		return -1;
	}
	auto to = sprt::__resolve_at(newfd, newPath, newBuf, sizeof(newBuf));
	if (!to) {
		return -1;
	}
	return rename(from, to);
}

// AT_SYMLINK_NOFOLLOW is accepted but has no effect: memfs has no symlinks, so
// stat() and lstat() are the same call.
__SPRT_C_FUNC int fstatat(int dirfd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st, int) __SPRT_NOEXCEPT {
	char buf[512];
	auto resolved = sprt::__resolve_at(dirfd, path, buf, sizeof(buf));
	if (!resolved) {
		return -1;
	}
	return stat(resolved, st);
}

// Set a file's timestamps (the primitive builtin utime()/utimes() forward to). The
// dir fd and AT_SYMLINK_NOFOLLOW flag are ignored (flat namespace, no symlinks); a
// null `times` sets both to now, and per-element UTIME_NOW/UTIME_OMIT are honored.
__SPRT_C_FUNC int utimensat(int, const char *path, const struct __SPRT_TIMESPEC_NAME *times, int)
		__SPRT_NOEXCEPT {
	if (!path) {
		__sprt_errno = EINVAL;
		return -1;
	}
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
	sprt::__memfs_apply_times(ino, times);
	return 0;
}

__SPRT_C_FUNC int linkat(int, const char *, int, const char *, int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS; // memfs has no hard links
	return -1;
}

// Create a symbolic link at `linkpath` holding `target` verbatim (POSIX does not
// resolve or validate the target at creation time — a dangling link is legal).
__SPRT_C_FUNC int symlink(const char *target, const char *linkpath) __SPRT_NOEXCEPT {
	if (!target || !linkpath || !*target) {
		__sprt_errno = EINVAL;
		return -1;
	}
	char abs[512];
	if (!sprt::__memfs_normpath(linkpath, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	if (sprt::__vfs_is_opfs(abs)) {
		__sprt_errno = EPERM; // OPFS has no link concept
		return -1;
	}
	if (sprt::__memfs_find(abs)) {
		__sprt_errno = EEXIST;
		return -1;
	}
	auto n = sprt::__memfs_create(abs, false, 0777);
	if (!n) {
		return -1;
	}
	__SPRT_ID(size_t) tlen = __builtin_strlen(target);
	if (!sprt::__memfs_reserve(n, tlen)) {
		sprt::__memfs_unlink_node(abs, false);
		return -1;
	}
	__builtin_memcpy(n->data, target, tlen);
	n->size = tlen;
	n->isLink = true;
	return 0;
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) readlink(const char *path, char *buf, __SPRT_ID(size_t) bufsiz)
		__SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	auto n = sprt::__memfs_find(abs);
	if (!n) {
		__sprt_errno = ENOENT;
		return -1;
	}
	if (!n->isLink) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (!buf || bufsiz == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	// POSIX: truncate silently to bufsiz and do NOT terminate.
	__SPRT_ID(size_t) cnt = n->size < bufsiz ? n->size : bufsiz;
	__builtin_memcpy(buf, n->data, cnt);
	return (__SPRT_ID(ssize_t))cnt;
}

__SPRT_C_FUNC char *realpath(const char *path, char *resolved) __SPRT_NOEXCEPT {
	char abs[512];
	if (!sprt::__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return nullptr;
	}
	// realpath() names the file a path finally reaches, so links are followed.
	if (!sprt::__memfs_resolve_link(abs, abs, sizeof(abs))) {
		return nullptr;
	}
	if (!sprt::__memfs_find(abs) && !sprt::__memfs_is_dir_path(abs)
			&& !sprt::__memfs_load_bundle(abs)) {
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

// --- fsync / fdatasync ---------------------------------------------------
// A tmpfs write is already durable (linear memory); an OPFS-backed write must be
// pushed to the persistent store. Validate the descriptor either way.

__SPRT_C_FUNC int fsync(int fd) __SPRT_NOEXCEPT {
	auto h = (sprt::__wasm_fnode *)sprt::__libc::get()->get_fd_handle(fd);
	if (!h) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (!h->isConsole && h->ino && h->ino->opfs && h->ino->dirty && !h->ino->isDir) {
		int r = sprt::__opfs_store(h->ino->path, h->ino->data, h->ino->size);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		h->ino->dirty = false;
	}
	return 0;
}

__SPRT_C_FUNC int fdatasync(int fd) __SPRT_NOEXCEPT { return fsync(fd); }

// --- collation (C locale) ------------------------------------------------

__SPRT_C_FUNC int __strcoll(const char *l, const char *r) __SPRT_NOEXCEPT {
	if (!l || !r) {
		return 0;
	}
	return strcmp(l, r);
}
