/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Included into runtime/SPRuntimeCPlatform.cpp. See dirfd.h for why Embox needs
// this at all.

#include "dirfd.h"

#if SPRT_EMBOX

#include <sprt/cxx/__mutex/unique_lock.h>
#include <sprt/runtime/thread/qmutex.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sprt::platform {

namespace {

// One entry per open directory descriptor. Embox's own per-task descriptor table
// is 64 entries by default (kernel/task/resource/idesc_table, idesc_table_size),
// and the deepest tree walk the runtime performs holds one descriptor per level,
// so 16 is ample. The path is stored inline rather than allocated: Embox's
// PATH_MAX is 128, which puts the whole table at ~2 KB of .bss and keeps the
// shim free of any allocation on the open/close path.
constexpr unsigned DIR_FD_MAX = 16;

struct DirFdEntry {
	int fd;
	void *dirStream;
	char path[PATH_MAX];
};

struct DirFdTable {
	DirFdEntry entries[DIR_FD_MAX];
	qmutex lock;
};

// fd == 0 marks a free slot: descriptor 0 is stdin, which is never a directory,
// so it can never be a valid entry here and zero-initialised .bss starts empty.
DirFdTable s_dirFds;

DirFdEntry *findLocked(int fd) {
	if (fd <= 0) {
		return nullptr;
	}
	for (auto &e : s_dirFds.entries) {
		if (e.fd == fd) {
			return &e;
		}
	}
	return nullptr;
}

// Join `dir` and `name` into `buf`. `dir` is absolute; "/" is special-cased so
// the result is "/name" rather than "//name".
bool joinPath(char *buf, __SPRT_ID(size_t) cap, const char *dir, const char *name) {
	auto written = ::snprintf(buf, cap, "%s/%s", (dir[0] == '/' && dir[1] == 0) ? "" : dir, name);
	if (written < 0 || static_cast<__SPRT_ID(size_t)>(written) >= cap) {
		errno = ENAMETOOLONG;
		return false;
	}
	return true;
}

// A descriptor number to stand in for the directory. Embox will not open a
// directory, so this is a duplicate of an already-open descriptor: the number is
// issued by the kernel (hence cannot collide with a real one, and close()
// reclaims it normally) and nothing ever reads through it, because every
// operation on a directory descriptor is answered from the table.
int allocatePlaceholder() {
	static const int bases[] = {0, 1, 2};
	for (auto base : bases) {
		auto fd = ::dup(base);
		if (fd > 0) {
			return fd;
		}
		if (fd == 0) {
			// Descriptor 0 would alias the free-slot marker; hand it back.
			::close(fd);
		}
	}
	errno = EMFILE;
	return -1;
}

} // namespace

int openDirFd(const char *path) {
	if (!path || !path[0]) {
		errno = ENOENT;
		return -1;
	}

	char abs[PATH_MAX];
	if (path[0] == '/') {
		if (::snprintf(abs, sizeof(abs), "%s", path) >= static_cast<int>(sizeof(abs))) {
			errno = ENAMETOOLONG;
			return -1;
		}
	} else {
		char cwd[PATH_MAX];
		if (!::getcwd(cwd, sizeof(cwd))) {
			return -1;
		}
		if (!joinPath(abs, sizeof(abs), cwd, path)) {
			return -1;
		}
	}

	// Drop a trailing slash so the stored path joins cleanly ("/a/" + "b").
	auto len = ::strlen(abs);
	while (len > 1 && abs[len - 1] == '/') {
		abs[--len] = 0;
	}

	struct stat st;
	if (::stat(abs, &st) != 0) {
		return -1; // errno from stat
	}
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	auto fd = allocatePlaceholder();
	if (fd < 0) {
		return -1;
	}

	{
		unique_lock lock(s_dirFds.lock);
		for (auto &e : s_dirFds.entries) {
			if (e.fd == 0) {
				e.fd = fd;
				e.dirStream = nullptr;
				::memcpy(e.path, abs, len + 1);
				return fd;
			}
		}
	}

	::close(fd);
	errno = EMFILE;
	return -1;
}

bool getDirFdPath(int fd, char *buf, __SPRT_ID(size_t) cap) {
	unique_lock lock(s_dirFds.lock);
	auto e = findLocked(fd);
	if (!e) {
		return false;
	}
	auto len = ::strlen(e->path);
	if (len + 1 > cap) {
		errno = ENAMETOOLONG;
		return false;
	}
	::memcpy(buf, e->path, len + 1);
	return true;
}

bool releaseDirFd(int fd) {
	unique_lock lock(s_dirFds.lock);
	if (auto e = findLocked(fd)) {
		e->fd = 0;
		e->dirStream = nullptr;
		e->path[0] = 0;
		return true;
	}
	return false;
}

bool cloneDirFd(int fromFd, int toFd) {
	if (toFd <= 0) {
		return false;
	}
	unique_lock lock(s_dirFds.lock);
	auto src = findLocked(fromFd);
	if (!src) {
		return false;
	}
	// The destination may already be registered (dup2 over a directory
	// descriptor); overwrite it rather than leaking a stale entry.
	auto dst = findLocked(toFd);
	if (!dst) {
		for (auto &e : s_dirFds.entries) {
			if (e.fd == 0) {
				dst = &e;
				break;
			}
		}
	}
	if (!dst) {
		return false;
	}
	dst->fd = toFd;
	dst->dirStream = nullptr;
	::memcpy(dst->path, src->path, ::strlen(src->path) + 1);
	return true;
}

bool attachDirStream(int fd, void *dirStream) {
	unique_lock lock(s_dirFds.lock);
	if (auto e = findLocked(fd)) {
		e->dirStream = dirStream;
		return true;
	}
	return false;
}

int getDirStreamFd(const void *dirStream) {
	if (!dirStream) {
		return -1;
	}
	unique_lock lock(s_dirFds.lock);
	for (auto &e : s_dirFds.entries) {
		if (e.fd != 0 && e.dirStream == dirStream) {
			return e.fd;
		}
	}
	return -1;
}

int detachDirStream(const void *dirStream) {
	if (!dirStream) {
		return -1;
	}
	unique_lock lock(s_dirFds.lock);
	for (auto &e : s_dirFds.entries) {
		if (e.fd != 0 && e.dirStream == dirStream) {
			auto fd = e.fd;
			e.fd = 0;
			e.dirStream = nullptr;
			e.path[0] = 0;
			return fd;
		}
	}
	return -1;
}

const char *resolveAtPath(int dirfd, const char *path, char *buf, __SPRT_ID(size_t) cap) {
	if (!path) {
		errno = EFAULT;
		return nullptr;
	}

	// An absolute path ignores the descriptor (POSIX). AT_FDCWD resolves against
	// the task's cwd, which is what Embox's own lookup already does for a
	// relative path - fs_perm_lookup() starts from vfs_get_leaf_path(). A
	// negative descriptor is spelled the same way by the runtime's own callers
	// (SPRuntimeFilesystemPosix.cpp passes -1) and by the wasm resolver.
	if (path[0] == '/' || dirfd == AT_FDCWD || dirfd < 0) {
		return path;
	}

	char dir[PATH_MAX];
	if (getDirFdPath(dirfd, dir, sizeof(dir))) {
		return joinPath(buf, cap, dir, path) ? buf : nullptr;
	}

	// Not one of ours, and Embox has no other way to produce a directory
	// descriptor - so the only question left is whether it is open at all.
	struct stat st;
	if (::fstat(dirfd, &st) != 0) {
		errno = EBADF;
		return nullptr;
	}
	errno = ENOTDIR;
	return nullptr;
}

int accessPath(const char *path, int mode) {
	struct stat st;
	if (::stat(path, &st) != 0) {
		return -1; // errno from stat
	}
	if (mode == F_OK) {
		return 0;
	}

	// Embox has no per-user credentials worth consulting here (getuid() is a
	// constant), so check the permission bits for any of owner/group/other -
	// which is what its own fs_perm_check() effectively reduces to.
	unsigned want = 0;
	if (mode & R_OK) {
		want |= S_IRUSR | S_IRGRP | S_IROTH;
	}
	if (mode & W_OK) {
		want |= S_IWUSR | S_IWGRP | S_IWOTH;
	}
	if (mode & X_OK) {
		want |= S_IXUSR | S_IXGRP | S_IXOTH;
	}

	if ((st.st_mode & want) == 0 && want != 0) {
		errno = EACCES;
		return -1;
	}
	return 0;
}

} // namespace sprt::platform

#endif // SPRT_EMBOX
