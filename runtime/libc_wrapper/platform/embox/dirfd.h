/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#ifndef RUNTIME_LIBC_WRAPPER_PLATFORM_EMBOX_DIRFD_H_
#define RUNTIME_LIBC_WRAPPER_PLATFORM_EMBOX_DIRFD_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_stddef.h>

#if SPRT_EMBOX

/*
	Directory descriptors for Embox, and the *at() family built on them.

	Every other platform answers "which directory is this descriptor?" from the
	system: macOS with fcntl(fd, F_GETPATH), the Windows libc with
	GetFinalPathNameByHandleW over its own fd slot, the wasm memfs from the
	inode's stored path. Embox cannot answer it, because it cannot produce the
	descriptor in the first place:

	  * open() refuses a directory outright - compat/posix/fs/oldfs/open_oldfs.c
	    asserts on O_DIRECTORY and returns EISDIR once the lookup lands on a node
	    with S_ISDIR. (The dvfs flavour opens one for O_PATH, but the boards we
	    ship select rootfs_oldfs.)
	  * DIR carries no descriptor - struct DIR_struct is {dirent, path, dir_ctx,
	    inodes_list}, so dirfd()/fdopendir() have nothing to hand over.
	  * every *at() entry point is an ENOSYS stub except openat(), which just
	    drops the descriptor and calls open(path) - so a relative path silently
	    resolved against the cwd instead of the directory, which is a
	    wrong-target bug, not a missing feature.

	So the descriptor is synthesised here instead, the same shape the Windows
	libc uses: sprt keeps the fd -> absolute path mapping itself, and every *at()
	wrapper resolves through it into a plain path call that Embox does implement.
	The descriptor number is a real one (dup of an already-open descriptor) so it
	cannot collide with a libc-issued fd and close() reclaims it normally.

	Callers outside this shim never see the placeholder: fstat(), fdopendir(),
	dirfd() and closedir() consult the table first.
*/

namespace sprt::platform {

// Open `path` as a directory descriptor. `path` must name an existing directory;
// it is stored absolute (a relative path is joined onto the cwd). Returns the
// descriptor, or -1 with errno set (ENOTDIR when the path is not a directory).
int openDirFd(const char *path);

// Copy the absolute path registered for `fd` into `buf`. False when `fd` is not
// one of ours (the copy keeps the caller safe from a concurrent close()).
bool getDirFdPath(int fd, char *buf, __SPRT_ID(size_t) cap);

// Drop the registration for `fd`. Returns true if there was one. Does NOT close
// the descriptor - the caller owns that.
bool releaseDirFd(int fd);

// Register `toFd` as a second descriptor for the same directory as `fromFd`, so
// a dup()'d directory descriptor keeps working. False when `fromFd` is not one
// of ours (an ordinary dup, nothing to do) or the table is full.
bool cloneDirFd(int fromFd, int toFd);

// Bind a DIR opened by fdopendir() to the descriptor it was opened from, so that
// dirfd() can report it back and closedir() can reclaim both.
bool attachDirStream(int fd, void *dirStream);

// Descriptor bound to `dirStream`, or -1.
int getDirStreamFd(const void *dirStream);

// Forget the binding for `dirStream` and report the descriptor it held, or -1.
int detachDirStream(const void *dirStream);

// Resolve `path` against `dirfd` for an *at() call. Returns the path to hand to
// the plain path-based entry point - either `path` itself (absolute path, or
// AT_FDCWD, both of which ignore the descriptor) or `buf` holding
// "<dir>/<path>" - or nullptr with errno set:
//   EBADF    dirfd is not open
//   ENOTDIR  dirfd is open but is not a directory
//   ENAMETOOLONG  the joined path does not fit in `cap`
const char *resolveAtPath(int dirfd, const char *path, char *buf, __SPRT_ID(size_t) cap);

// access() for Embox. Its <unistd.h> defines access() as a static inline that
// ignores both arguments and returns 0, so the platform reports every path -
// including ones that do not exist - as accessible. Answer from stat() and the
// mode bits instead.
int accessPath(const char *path, int mode);

} // namespace sprt::platform

#endif // SPRT_EMBOX

#endif /* RUNTIME_LIBC_WRAPPER_PLATFORM_EMBOX_DIRFD_H_ */
