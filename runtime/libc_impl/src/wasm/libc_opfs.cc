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

// The persistent "/opfs" mount, backed by the browser Origin Private File System.
// Part of the memfs TU (included by builtin_libc.cpp after libc_file_ops.cc, before
// libc_path.cc), so it shares s_memfs and the __memfs_* helpers.
//
// OPFS handles are async to acquire and SyncAccessHandle is worker-only, so each op
// is brokered to a dedicated OPFS worker via a single host import: the JS side blocks
// this thread in Atomics.wait while the OPFS worker (Atomics.waitAsync) executes it.
// Paths and buffers are pointers into the shared wasm memory, read/written there.
//
// Files use a load-on-open / write-back-on-close cache: a file opened under /opfs is
// hydrated into a memfs inode (so read/write/seek reuse the in-memory fast path) and
// flushed back on close/fsync when dirty. Directory metadata ops go straight through.
// The op codes + control-block layout must match runtime/wasm-js/opfs-worker.mjs.

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>

extern "C" {
// Broker one OPFS op to the OPFS worker. `a0..a3` are op-specific (pointers into this
// shared memory / lengths / sizes). Returns the op result (>= 0) or -errno.
__attribute__((import_module("sprt"), import_name("opfs_call"))) int __sprt_host_opfs_call(int op,
		int a0, int a1, int a2, int a3);
}

namespace sprt {

enum : int {
	__OPFS_STAT = 1,
	__OPFS_LOAD = 2,
	__OPFS_STORE = 3,
	__OPFS_MKDIR = 4,
	__OPFS_UNLINK = 5,
	__OPFS_RENAME = 6,
	__OPFS_READDIR = 7,
};

#define __OPFS_P(x) ((int)(__SPRT_ID(intptr_t))(x))

// "/opfs" or "/opfs/..." — the persistent mount.
static bool __vfs_is_opfs(const char *abs) {
	return abs[0] == '/' && abs[1] == 'o' && abs[2] == 'p' && abs[3] == 'f' && abs[4] == 's'
			&& (abs[5] == '\0' || abs[5] == '/');
}

// The OPFS-relative path (everything after "/opfs"): "" for the mount root, "/a/b"
// otherwise. The JS side filters empty segments, so the leading '/' is harmless.
static const char *__opfs_rel(const char *abs) { return abs + 5; }

// stat: fills *size / *isdir; returns 0 or -errno.
static int __opfs_stat(const char *abs, __SPRT_ID(size_t) *size, bool *isdir) {
	const char *rel = __opfs_rel(abs);
	int out[2] = {0, 0};
	int r = __sprt_host_opfs_call(__OPFS_STAT, __OPFS_P(rel), (int)__builtin_strlen(rel),
			__OPFS_P(out), 0);
	if (r < 0) {
		return r;
	}
	if (size) {
		*size = (__SPRT_ID(size_t))out[0];
	}
	if (isdir) {
		*isdir = out[1] != 0;
	}
	return 0;
}

// load: read up to `cap` bytes into `buf`; returns bytes read or -errno.
static int __opfs_load(const char *abs, unsigned char *buf, __SPRT_ID(size_t) cap) {
	const char *rel = __opfs_rel(abs);
	return __sprt_host_opfs_call(__OPFS_LOAD, __OPFS_P(rel), (int)__builtin_strlen(rel),
			__OPFS_P(buf), (int)cap);
}

// store: write `size` bytes back (create/truncate); returns 0 or -errno.
static int __opfs_store(const char *abs, const unsigned char *data, __SPRT_ID(size_t) size) {
	if (!__vfs_is_opfs(abs)) {
		return 0;
	}
	const char *rel = __opfs_rel(abs);
	return __sprt_host_opfs_call(__OPFS_STORE, __OPFS_P(rel), (int)__builtin_strlen(rel),
			__OPFS_P(data), (int)size);
}

static int __opfs_mkdir(const char *abs) {
	const char *rel = __opfs_rel(abs);
	return __sprt_host_opfs_call(__OPFS_MKDIR, __OPFS_P(rel), (int)__builtin_strlen(rel), 0, 0);
}

static int __opfs_unlink(const char *abs, bool isdir) {
	const char *rel = __opfs_rel(abs);
	return __sprt_host_opfs_call(__OPFS_UNLINK, __OPFS_P(rel), (int)__builtin_strlen(rel),
			isdir ? 1 : 0, 0);
}

static int __opfs_rename(const char *from, const char *to) {
	const char *rf = __opfs_rel(from), *rt = __opfs_rel(to);
	return __sprt_host_opfs_call(__OPFS_RENAME, __OPFS_P(rf), (int)__builtin_strlen(rf),
			__OPFS_P(rt), (int)__builtin_strlen(rt));
}

// readdir: worker writes entries as "<name>\0<type-byte>" (0 file / 1 dir), repeated,
// into `out`; returns the entry count or -errno.
static int __opfs_readdir(const char *abs, unsigned char *out, __SPRT_ID(size_t) cap) {
	const char *rel = __opfs_rel(abs);
	return __sprt_host_opfs_call(__OPFS_READDIR, __OPFS_P(rel), (int)__builtin_strlen(rel),
			__OPFS_P(out), (int)cap);
}

// Resolve an /opfs path to a ready cache inode for open(): hydrate an existing file,
// materialise a directory placeholder, or create a new (dirty) file. Applies O_CREAT/
// O_EXCL/O_TRUNC. Returns null + errno on failure.
static __memfs_inode *__opfs_resolve_inode(const char *abs, int flags, __SPRT_ID(mode_t) mode) {
	auto ino = __memfs_find(abs);
	if (ino) { // already hydrated
		if ((flags & __SPRT_O_CREAT) && (flags & __SPRT_O_EXCL)) {
			__sprt_errno = EEXIST;
			return nullptr;
		}
		if (ino->isDir && ((flags & __SPRT_O_ACCMODE) != __SPRT_O_RDONLY)) {
			__sprt_errno = EISDIR;
			return nullptr;
		}
		if (flags & __SPRT_O_TRUNC) {
			ino->size = 0;
			ino->dirty = true;
		}
		return ino;
	}

	__SPRT_ID(size_t) size = 0;
	bool isdir = false;
	int st = __opfs_stat(abs, &size, &isdir);
	if (st == 0) { // exists in OPFS
		if ((flags & __SPRT_O_CREAT) && (flags & __SPRT_O_EXCL)) {
			__sprt_errno = EEXIST;
			return nullptr;
		}
		if (isdir) {
			auto d = __memfs_create(abs, true, 0755);
			if (d) {
				d->opfs = true;
			}
			return d;
		}
		auto f = __memfs_create(abs, false, mode ? (mode & 0777) : 0644);
		if (!f) {
			return nullptr;
		}
		f->opfs = true;
		if (flags & __SPRT_O_TRUNC) {
			f->size = 0;
			f->dirty = true;
		} else if (size > 0) {
			if (!__memfs_reserve(f, size)) {
				__memfs_unlink_node(abs, false);
				return nullptr;
			}
			int rd = __opfs_load(abs, f->data, size);
			f->size = (rd > 0) ? (__SPRT_ID(size_t))rd : 0;
		}
		return f;
	}
	if (st == -ENOENT) {
		if (!(flags & __SPRT_O_CREAT)) {
			__sprt_errno = ENOENT;
			return nullptr;
		}
		auto f = __memfs_create(abs, false, mode & 0777);
		if (!f) {
			return nullptr;
		}
		f->opfs = true;
		f->dirty = true; // brand-new: must be written back on close
		return f;
	}
	__sprt_errno = -st; // EIO etc.
	return nullptr;
}

} // namespace sprt
