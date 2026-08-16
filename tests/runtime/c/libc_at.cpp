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

// Exercises the *at() family and at_quick_exit() registration.
//
// The *at() calls are the ones a platform without native support has to emulate
// by resolving the directory descriptor back to a path: macOS does it with
// fcntl(F_GETPATH), the Windows libc with GetFinalPathNameByHandleW over its own
// fd slot, the wasm memfs from its inode's stored path, and Embox from the table
// in runtime/libc_wrapper/platform/embox/dirfd.cc. This pins what the emulation
// has to reproduce:
//
//   * AT_FDCWD and absolute paths ignore the descriptor entirely,
//   * a descriptor that is not a directory must fail with ENOTDIR - NOT silently
//     operate on the cwd-relative path, which is a wrong-target bug that
//     deletes or overwrites the wrong file,
//   * a real directory descriptor addresses its own children,
//   * fdopendir()/dirfd()/closedir() round-trip, which is what the recursive
//     walk in runtime/src/filesystem/SPRuntimeFilesystemPosix.cpp is built on.
//
// The read-only half runs anywhere. The write half needs a writable directory,
// which an RTOS image with a read-only rootfs may not have; it is skipped, not
// failed, when none is found.

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sprt/runtime/log.h>
#include <sprt/runtime/platform.h>

namespace sprt {

namespace {

int s_failures = 0;
int s_skipped = 0;

void check(bool cond, const char *msg) {
	printf("  %s: %s\n", cond ? "PASS" : "FAIL", msg);
	if (!cond) {
		++s_failures;
	}
}

void skip(const char *msg) {
	printf("  SKIP: %s\n", msg);
	++s_skipped;
}

// Registered through at_quick_exit() but never expected to run: quick_exit()
// terminates the process, so the test only checks that registration is accepted.
void quickExitHandler() { }

// First regular file directly inside `dir`, or false. Read-only probes need a
// file that is known to exist without being able to create one.
bool findRegularFile(const char *dir, char *nameOut, size_t cap) {
	auto dp = opendir(dir);
	if (!dp) {
		return false;
	}
	bool found = false;
	while (auto ent = readdir(dp)) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			continue;
		}
		char full[PATH_MAX];
		if (snprintf(full, sizeof(full), "%s/%s", (dir[1] == 0) ? "" : dir, ent->d_name)
				>= (int)sizeof(full)) {
			continue;
		}
		struct stat st;
		if (stat(full, &st) == 0 && S_ISREG(st.st_mode) && strlen(ent->d_name) < cap) {
			memcpy(nameOut, ent->d_name, strlen(ent->d_name) + 1);
			found = true;
			break;
		}
	}
	closedir(dp);
	return found;
}

// A subdirectory of `dir` holding a regular file whose name does NOT also exist
// in `dir`. That pair is the sharpest read-only probe available: resolving the
// name against the subdirectory's descriptor must find it, and resolving the
// same name against the cwd must not - so a descriptor that is silently ignored
// cannot pass.
bool findDistinctChild(const char *dir, char *subOut, size_t subCap, char *nameOut, size_t nameCap) {
	auto dp = opendir(dir);
	if (!dp) {
		return false;
	}
	bool found = false;
	while (auto ent = readdir(dp)) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			continue;
		}
		char full[PATH_MAX];
		if (snprintf(full, sizeof(full), "%s/%s", (dir[1] == 0) ? "" : dir, ent->d_name)
				>= (int)sizeof(full)) {
			continue;
		}
		struct stat st;
		if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode) || strlen(full) >= subCap) {
			continue;
		}

		char child[NAME_MAX + 1];
		if (!findRegularFile(full, child, sizeof(child)) || strlen(child) >= nameCap) {
			continue;
		}

		// The name must be unique to the subdirectory.
		char sibling[PATH_MAX];
		if (snprintf(sibling, sizeof(sibling), "%s/%s", (dir[1] == 0) ? "" : dir, child)
				>= (int)sizeof(sibling)) {
			continue;
		}
		if (stat(sibling, &st) == 0) {
			continue;
		}

		memcpy(subOut, full, strlen(full) + 1);
		memcpy(nameOut, child, strlen(child) + 1);
		found = true;
		break;
	}
	closedir(dp);
	return found;
}

// A directory we may create files in, or false.
bool findWritableDir(char *out, size_t cap) {
	char cwd[PATH_MAX] = {0};
	const char *candidates[] = {"/tmp", "/mnt", getcwd(cwd, sizeof(cwd)) ? cwd : nullptr};
	for (auto candidate : candidates) {
		if (!candidate) {
			continue;
		}
		char probe[PATH_MAX];
		if (snprintf(probe, sizeof(probe), "%s/.at_probe", (candidate[1] == 0) ? "" : candidate)
				>= (int)sizeof(probe)) {
			continue;
		}
		if (mkdir(probe, 0755) == 0) {
			rmdir(probe);
			if (strlen(candidate) < cap) {
				memcpy(out, candidate, strlen(candidate) + 1);
				return true;
			}
		}
	}
	return false;
}

void runReadOnlyChecks(const char *dir) {
	printf("  read-only probes in %s\n", dir);

	char name[NAME_MAX + 1] = {0};
	bool haveFile = findRegularFile(dir, name, sizeof(name));
	if (haveFile) {
		printf("  probe file: %s\n", name);
	} else {
		skip("no regular file to probe with");
	}

	// --- a descriptor that is NOT a directory -----------------------------

	if (haveFile) {
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", (dir[1] == 0) ? "" : dir, name);

		int filefd = open(full, O_RDONLY);
		check(filefd >= 0, "open a regular file for the ENOTDIR probes");
		if (filefd >= 0) {
			errno = 0;
			check(faccessat(filefd, "nested", F_OK, 0) != 0 && errno == ENOTDIR,
					"faccessat(non-directory fd) -> ENOTDIR");

			errno = 0;
			check(openat(filefd, "nested", O_RDONLY) < 0 && errno == ENOTDIR,
					"openat(non-directory fd) -> ENOTDIR");

			struct stat probeSt;
			errno = 0;
			check(fstatat(filefd, "nested", &probeSt, 0) != 0 && errno == ENOTDIR,
					"fstatat(non-directory fd) -> ENOTDIR");

			// An absolute path ignores the descriptor even when it is not a directory.
			check(faccessat(filefd, full, F_OK, 0) == 0,
					"faccessat(non-directory fd, absolute) ignores the fd");

			close(filefd);
		}
	}

	// A path that does not exist must not report as accessible.
	errno = 0;
	check(faccessat(AT_FDCWD, "/definitely-not-here", F_OK, 0) != 0,
			"faccessat rejects a nonexistent path");
	errno = 0;
	check(access("/definitely-not-here", F_OK) != 0, "access rejects a nonexistent path");

	// --- a real directory descriptor --------------------------------------

	int dirFd = open(dir, O_DIRECTORY | O_RDONLY);
	if (dirFd < 0) {
		printf("  open(%s, O_DIRECTORY) failed: errno=%d\n", dir, errno);
		skip("directory descriptors unavailable on this platform");
		return;
	}

	struct stat st;
	memset(&st, 0, sizeof(st));
	check(fstat(dirFd, &st) == 0 && S_ISDIR(st.st_mode), "fstat(dirfd) reports a directory");

	if (haveFile) {
		check(faccessat(dirFd, name, F_OK, 0) == 0, "faccessat(dirfd, relative)");

		memset(&st, 0, sizeof(st));
		check(fstatat(dirFd, name, &st, 0) == 0 && S_ISREG(st.st_mode),
				"fstatat(dirfd, relative)");

		int nested = openat(dirFd, name, O_RDONLY);
		check(nested >= 0, "openat(dirfd, relative)");
		if (nested >= 0) {
			close(nested);
		}

		errno = 0;
		check(openat(dirFd, "definitely-not-here", O_RDONLY) < 0,
				"openat(dirfd, missing child) fails");
	}

	// fdopendir()/dirfd()/closedir() round-trip.
	if (auto dp = fdopendir(dirFd)) {
		int entries = 0;
		bool sawFile = false;
		while (auto ent = readdir(dp)) {
			++entries;
			if (haveFile && strcmp(ent->d_name, name) == 0) {
				sawFile = true;
			}
		}
		check(entries > 0, "fdopendir(dirfd) enumerates the directory");
		if (haveFile) {
			check(sawFile, "fdopendir(dirfd) lists the probe file");
		}
		check(dirfd(dp) == dirFd, "dirfd(fdopendir(fd)) round-trips");
		closedir(dp); // takes the descriptor with it
	} else {
		check(false, "fdopendir(dirfd)");
		close(dirFd);
	}

	// --- the descriptor must actually be honoured --------------------------
	// Everything above resolves inside the cwd, where ignoring the descriptor
	// looks identical to honouring it. This pair does not: the name exists only
	// under the subdirectory.

	char sub[PATH_MAX] = {0};
	char child[NAME_MAX + 1] = {0};
	if (!findDistinctChild(dir, sub, sizeof(sub), child, sizeof(child))) {
		skip("no subdirectory with a uniquely-named file to probe the descriptor with");
		return;
	}

	printf("  distinct child: %s/%s\n", sub, child);

	// Precondition: the name is NOT reachable from the cwd.
	check(faccessat(AT_FDCWD, child, F_OK, 0) != 0, "the probe name is absent from the cwd");

	int subFd = open(sub, O_DIRECTORY | O_RDONLY);
	check(subFd >= 0, "open the subdirectory as a descriptor");
	if (subFd >= 0) {
		check(faccessat(subFd, child, F_OK, 0) == 0,
				"faccessat(subdir fd) resolves against the RIGHT directory");

		struct stat childSt;
		memset(&childSt, 0, sizeof(childSt));
		check(fstatat(subFd, child, &childSt, 0) == 0 && S_ISREG(childSt.st_mode),
				"fstatat(subdir fd) resolves against the RIGHT directory");

		int nested = openat(subFd, child, O_RDONLY);
		check(nested >= 0, "openat(subdir fd) resolves against the RIGHT directory");
		if (nested >= 0) {
			close(nested);
		}

		close(subFd);
	}
}

void runWriteChecks(const char *root) {
	printf("  write probes in %s\n", root);

	char base[PATH_MAX] = {0};
	snprintf(base, sizeof(base), "%s/at_test", (root[1] == 0) ? "" : root);

	// Best-effort cleanup of a previous run.
	char stale[PATH_MAX];
	snprintf(stale, sizeof(stale), "%s/file", base);
	unlink(stale);
	snprintf(stale, sizeof(stale), "%s/sub", base);
	rmdir(stale);
	rmdir(base);

	check(mkdirat(AT_FDCWD, base, 0755) == 0, "mkdirat(AT_FDCWD, absolute)");

	char file[PATH_MAX];
	snprintf(file, sizeof(file), "%s/file", base);

	int fd = openat(AT_FDCWD, file, O_CREAT | O_RDWR, 0644);
	check(fd >= 0, "openat(AT_FDCWD, absolute, O_CREAT)");
	if (fd >= 0) {
		check(write(fd, "at", 2) == 2, "write to the created file");
		close(fd);
	}

	struct stat st;
	memset(&st, 0, sizeof(st));
	check(fstatat(AT_FDCWD, file, &st, 0) == 0 && S_ISREG(st.st_mode) && st.st_size == 2,
			"fstatat(AT_FDCWD) reports the file it created");

	int dirFd = open(base, O_DIRECTORY | O_RDONLY);
	if (dirFd < 0) {
		skip("directory descriptors unavailable; relative *at writes not exercised");
	} else {
		check(mkdirat(dirFd, "sub", 0755) == 0, "mkdirat(dirfd, relative)");

		// Proves the descriptor was honoured rather than ignored: with the
		// descriptor dropped this would have created <cwd>/sub instead.
		char expect[PATH_MAX];
		snprintf(expect, sizeof(expect), "%s/sub", base);
		memset(&st, 0, sizeof(st));
		check(stat(expect, &st) == 0 && S_ISDIR(st.st_mode),
				"mkdirat(dirfd) created the child of the RIGHT directory");

		check(unlinkat(dirFd, "sub", AT_REMOVEDIR) == 0, "unlinkat(dirfd, AT_REMOVEDIR)");

		int nested = openat(dirFd, "file", O_RDONLY);
		check(nested >= 0, "openat(dirfd, relative) opens the child");
		if (nested >= 0) {
			char buf[4] = {0};
			check(read(nested, buf, 2) == 2 && memcmp(buf, "at", 2) == 0,
					"openat(dirfd, relative) opened the right file");
			close(nested);
		}

		check(renameat(dirFd, "file", dirFd, "moved") == 0, "renameat(dirfd, dirfd)");
		check(faccessat(dirFd, "moved", F_OK, 0) == 0, "renameat moved the child");
		check(renameat(dirFd, "moved", dirFd, "file") == 0, "renameat back");

		close(dirFd);
	}

	check(unlinkat(AT_FDCWD, file, 0) == 0, "unlinkat(AT_FDCWD) removes the file");
	check(unlinkat(AT_FDCWD, base, AT_REMOVEDIR) == 0, "unlinkat(AT_FDCWD, AT_REMOVEDIR)");
}

} // namespace

void performAtFunctionsTest() {
	s_failures = 0;
	s_skipped = 0;

	char cwd[PATH_MAX] = {0};
	printf("  cwd: %s\n", getcwd(cwd, sizeof(cwd)) ? cwd : "(unavailable)");

	runReadOnlyChecks(cwd[0] ? cwd : "/");

	char writable[PATH_MAX] = {0};
	if (findWritableDir(writable, sizeof(writable))) {
		runWriteChecks(writable);
	} else {
		skip("no writable directory; *at write paths not exercised");
	}

	// --- at_quick_exit ----------------------------------------------------
	// C requires at least 32 registrations to be accepted. Before the Embox
	// fallback existed at_quick_exit() there returned 0 without recording
	// anything and quick_exit() went straight to _Exit(), so a handler was
	// silently dropped; there is no portable way to observe that from inside the
	// process short of terminating it, so this pins registration only.

	int accepted = 0;
	for (int i = 0; i < 32; ++i) {
		if (at_quick_exit(&quickExitHandler) == 0) {
			++accepted;
		}
	}
	check(accepted == 32, "at_quick_exit accepts 32 handlers");

	printf("performAtFunctionsTest: %s (%d failures, %d skipped)\n",
			s_failures == 0 ? "ALL PASS" : "FAILED", s_failures, s_skipped);
}

} // namespace sprt
