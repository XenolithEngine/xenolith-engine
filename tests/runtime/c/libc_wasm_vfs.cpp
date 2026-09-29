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

// The wasm VFS pieces the browser has no native form of: the virtual cwd (chdir/getcwd,
// and relative paths resolved against it), the lazy listing of bundled directories
// (opendir/stat see bundle entries nothing has opened yet), the read-only guarantee of the
// bundle, and /dev/urandom. The bundle half needs the tree tests/runtime/run-wasm.sh
// drops into the runner's launch directory and prints a SKIP without it. On every other
// target the test prints a SKIP line.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include "../tests.h"

namespace sprt {

#if SPRT_WASM

namespace {

static int s_failures = 0;

static void check(bool ok, const char *what) {
	printf("%s  %s\n", ok ? "PASS" : sprt::test::failed("FAIL"), what);
	if (!ok) {
		++s_failures;
	}
}

static bool cwdIs(const char *expected) {
	char buf[PATH_MAX];
	return getcwd(buf, sizeof(buf)) && strcmp(buf, expected) == 0;
}

// True when `path` lists exactly `expected` (any order), not counting "." and "..".
static bool listDir(const char *path, const char *const *expected, size_t nexpected) {
	auto d = opendir(path);
	if (!d) {
		return false;
	}
	size_t found = 0, total = 0;
	while (auto e = readdir(d)) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
			continue;
		}
		++total;
		for (size_t i = 0; i < nexpected; ++i) {
			if (strcmp(e->d_name, expected[i]) == 0) {
				++found;
				break;
			}
		}
	}
	closedir(d);
	return found == nexpected && total == nexpected;
}

static bool readAll(const char *path, char *buf, size_t cap, ssize_t *n) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	*n = read(fd, buf, cap);
	close(fd);
	return *n >= 0;
}

static void testCwd() {
	check(cwdIs("/"), "cwd starts at /");

	mkdir("/tmp", 0755);
	check(mkdir("/tmp/vfscwd", 0755) == 0 && mkdir("/tmp/vfscwd/a", 0755) == 0,
			"mkdir /tmp/vfscwd/a");
	check(chdir("/tmp/vfscwd") == 0 && cwdIs("/tmp/vfscwd"), "chdir to an absolute directory");

	int fd = open("a/f.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
	check(fd >= 0 && write(fd, "x", 1) == 1, "create a relative path under the cwd");
	if (fd >= 0) {
		close(fd);
	}
	struct stat st;
	check(stat("/tmp/vfscwd/a/f.txt", &st) == 0 && st.st_size == 1,
			"the relative create landed under the cwd");
	check(stat("/a/f.txt", &st) != 0, "the relative create did not land under /");

	check(chdir("a") == 0 && cwdIs("/tmp/vfscwd/a"), "chdir to a relative directory");
	check(access("f.txt", F_OK) == 0, "access() resolves against the cwd");
	fd = openat(AT_FDCWD, "f.txt", O_RDONLY);
	check(fd >= 0, "openat(AT_FDCWD) resolves against the cwd");
	if (fd >= 0) {
		close(fd);
	}
	check(chdir("..") == 0 && cwdIs("/tmp/vfscwd"), "chdir(\"..\")");

	errno = 0;
	check(chdir("missing") == -1 && errno == ENOENT, "chdir to a missing directory: ENOENT");
	errno = 0;
	check(chdir("a/f.txt") == -1 && errno == ENOTDIR, "chdir to a file: ENOTDIR");
	check(cwdIs("/tmp/vfscwd"), "a rejected chdir keeps the cwd");

	char small[4];
	errno = 0;
	check(getcwd(small, sizeof(small)) == nullptr && errno == ERANGE,
			"getcwd into a short buffer: ERANGE");

	char resolved[PATH_MAX];
	check(realpath("a/../a/f.txt", resolved) && strcmp(resolved, "/tmp/vfscwd/a/f.txt") == 0,
			"realpath of a relative path");

	check(chdir("/") == 0 && cwdIs("/"), "chdir back to /");
}

// Tree dropped by run-wasm.sh:
//   /vfs-bundle/top.txt           "bundle top\n"
//   /vfs-bundle/sub/mid.txt
//   /vfs-bundle/sub/deep/leaf.txt
//   /vfs-bundle/other/x.txt       "other x\n"
static void testBundle() {
	static constexpr char TopText[] = "bundle top\n";
	static constexpr char OtherText[] = "other x\n";

	if (access("/vfs-bundle/top.txt", F_OK) != 0) {
		printf("SKIP  bundled directory checks (no /vfs-bundle in the launch directory)\n");
		return;
	}

	// chdir first, into a bundled directory nothing has touched yet.
	check(chdir("/vfs-bundle/other") == 0 && cwdIs("/vfs-bundle/other"),
			"chdir into an untouched bundled directory");
	char buf[256];
	ssize_t n = 0;
	check(readAll("x.txt", buf, sizeof(buf), &n) && n == ssize_t(sizeof(OtherText) - 1)
					&& memcmp(buf, OtherText, n) == 0,
			"read a bundled file relative to the cwd");
	chdir("/");

	struct stat st;
	check(stat("/vfs-bundle/sub/mid.txt", &st) == 0 && S_ISREG(st.st_mode),
			"stat a bundled file before anything opened it");

	const char *top[] = {"top.txt", "sub", "other"};
	check(listDir("/vfs-bundle", top, 3), "opendir lists a bundled directory");
	check(listDir("/vfs-bundle", top, 3), "a second opendir lists it once more, no duplicates");
	check(stat("/vfs-bundle/top.txt", &st) == 0 && S_ISREG(st.st_mode)
					&& st.st_size == off_t(sizeof(TopText) - 1),
			"a listed file reports its bundle size before it is opened");

	const char *sub[] = {"mid.txt", "deep"};
	check(listDir("/vfs-bundle/sub", sub, 2), "opendir lists a bundled subdirectory");
	const char *deep[] = {"leaf.txt"};
	check(listDir("/vfs-bundle/sub/deep", deep, 1),
			"opendir lists a grandchild directory (the ftw case)");

	errno = 0;
	int fd = open("/vfs-bundle/top.txt", O_WRONLY | O_TRUNC);
	check(fd < 0 && errno == EROFS, "a write-open of a bundled file fails with EROFS");
	if (fd >= 0) {
		close(fd);
	}
	errno = 0;
	fd = open("/vfs-bundle/sub/mid.txt", O_RDWR);
	check(fd < 0 && errno == EROFS, "a write-open of an unopened bundled file fails too");
	if (fd >= 0) {
		close(fd);
	}
	check(readAll("/vfs-bundle/top.txt", buf, sizeof(buf), &n)
					&& n == ssize_t(sizeof(TopText) - 1) && memcmp(buf, TopText, n) == 0,
			"the refused O_TRUNC left the bundled content intact");
}

static void testUrandom() {
	unsigned char buf[64] = {0};
	int fd = open("/dev/urandom", O_RDONLY);
	check(fd >= 0, "open /dev/urandom");
	if (fd < 0) {
		return;
	}
	bool nonzero = false;
	if (read(fd, buf, sizeof(buf)) == ssize_t(sizeof(buf))) {
		for (auto b : buf) {
			nonzero = nonzero || b != 0;
		}
	}
	close(fd);
	check(nonzero, "read 64 random bytes from /dev/urandom");
}

} // namespace

void performWasmVfsTest() {
	s_failures = 0;
	testCwd();
	testBundle();
	testUrandom();
	printf("--- wasm VFS test done (failures=%d) ---\n", s_failures);
}

#else

void performWasmVfsTest() { printf("SKIP  wasm VFS test (not a wasm build)\n"); }

#endif

} // namespace sprt
