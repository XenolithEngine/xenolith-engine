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

// Path-acceptance tests. The freestanding Windows libc_impl accepts paths in
// POSIX form (a "/c/dir/file" maps to "C:\dir\file") *and* in native Windows form
// ("C:\dir\file", "C:/dir/file"). These tests open the same file through several
// path spellings and check the read-back content, printing only PASS/FAIL so the
// output is identical on Linux and Windows (on Linux the native form coincides
// with the POSIX form — `__sprt_fpath_to_native` is the identity there).

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// __sprt_fpath_to_native / _to_posix are the runtime's path-format converters; on
// Windows they translate POSIX <-> "C:\\" native, on Linux they copy verbatim. When
// the sprt runtime is not on the include path (the standalone system-compiler build)
// we substitute the Linux identity behaviour so this path-acceptance test still
// builds — it then simply verifies POSIX path acceptance through the system libc.
#if __has_include(<sprt/c/__sprt_stdio.h>)
#include <sprt/c/__sprt_stdio.h>
#else
static size_t __sprt_fpath_to_native(const char *path, size_t pathSize, char *buf, size_t bufSize) {
	if (pathSize >= bufSize) {
		return 0;
	}
	memcpy(buf, path, pathSize);
	return pathSize;
}
static size_t __sprt_fpath_to_posix(const char *path, size_t pathSize, char *buf, size_t bufSize) {
	if (pathSize >= bufSize) {
		return 0;
	}
	memcpy(buf, path, pathSize);
	return pathSize;
}
#endif

#include "test_util.h"

namespace sprt::test {

static bool writeFile(const char *path, const char *content) {
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		return false;
	}
	size_t len = strlen(content);
	bool ok = (size_t)write(fd, content, len) == len;
	close(fd);
	return ok;
}

// Open `path`, read it, and compare against `expected`. Returns a stable verdict
// string (never the path itself, which differs per platform).
static const char *openReadCheck(const char *path, const char *expected) {
	int fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		return "FAIL(open)";
	}
	char buf[128];
	memset(buf, 0, sizeof(buf));
	long n = (long)read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n < 0) {
		return "FAIL(read)";
	}
	return strcmp(buf, expected) == 0 ? "PASS" : "FAIL(content)";
}

void performPathPosixTest() {
	const char *rel = "sprt_libc_path.tmp";
	const char *content = "path-content-42";
	unlink(rel);
	printf("write=%d\n", writeFile(rel, content) ? 1 : 0);

	// 1. plain relative name
	printf("relative: %s\n", openReadCheck(rel, content));

	// 2. "./" prefixed
	{
		char p[64];
		snprintf(p, sizeof(p), "./%s", rel);
		printf("dot-relative: %s\n", openReadCheck(p, content));
	}

	// 3. redundant separators collapse
	{
		char p[80];
		snprintf(p, sizeof(p), ".///%s", rel);
		printf("redundant-slash: %s\n", openReadCheck(p, content));
	}

	// 4. a subdirectory + ".." resolves back to the file
	{
		const char *sub = "sprt_libc_pdir";
		mkdir(sub, 0755);
		char p[96];
		snprintf(p, sizeof(p), "%s/../%s", sub, rel);
		printf("dotdot-relative: %s\n", openReadCheck(p, content));
		rmdir(sub);
	}

	// 5. absolute path via getcwd() — POSIX form on both targets (on Windows
	//    getcwd returns "/c/..."), exercising POSIX absolute-path acceptance.
	{
		char cwd[512];
		if (getcwd(cwd, sizeof(cwd))) {
			char p[640];
			snprintf(p, sizeof(p), "%s/%s", cwd, rel);
			printf("absolute-posix: %s\n", openReadCheck(p, content));
		} else {
			printf("absolute-posix: FAIL(getcwd)\n");
		}
	}

	unlink(rel);
}

void performPathWindowsTest() {
	const char *rel = "sprt_libc_winpath.tmp";
	const char *content = "winpath-content";
	unlink(rel);
	printf("write=%d\n", writeFile(rel, content) ? 1 : 0);

	char cwd[512];
	if (!getcwd(cwd, sizeof(cwd))) {
		printf("FAIL(getcwd)\n");
		unlink(rel);
		return;
	}
	char posixAbs[640];
	snprintf(posixAbs, sizeof(posixAbs), "%s/%s", cwd, rel);

	// The POSIX absolute form opens on both targets.
	printf("posix-abs: %s\n", openReadCheck(posixAbs, content));

	// Native form: identity on Linux, "C:\\..." on Windows. Opening through it
	// verifies the Windows-format (back-slash) path is accepted.
	char nativeAbs[640];
	size_t nlen = __sprt_fpath_to_native(posixAbs, strlen(posixAbs), nativeAbs, sizeof(nativeAbs));
	if (nlen == 0 || nlen >= sizeof(nativeAbs)) {
		printf("native-abs: FAIL(convert)\nnative-fwdslash: FAIL(convert)\nroundtrip-posix: FAIL(convert)\n");
		unlink(rel);
		return;
	}
	nativeAbs[nlen] = 0;
	printf("native-abs: %s\n", openReadCheck(nativeAbs, content));

	// Native form with forward slashes ("C:/..."): WinAPI accepts it too; on Linux
	// this is a no-op (already forward slashes).
	{
		char fwd[640];
		size_t i = 0;
		for (; nativeAbs[i] && i < sizeof(fwd) - 1; ++i) {
			fwd[i] = (nativeAbs[i] == '\\') ? '/' : nativeAbs[i];
		}
		fwd[i] = 0;
		printf("native-fwdslash: %s\n", openReadCheck(fwd, content));
	}

	// Round-trip native -> POSIX and open through the result.
	{
		char back[640];
		size_t blen = __sprt_fpath_to_posix(nativeAbs, strlen(nativeAbs), back, sizeof(back));
		if (blen == 0 || blen >= sizeof(back)) {
			printf("roundtrip-posix: FAIL(convert)\n");
		} else {
			back[blen] = 0;
			printf("roundtrip-posix: %s\n", openReadCheck(back, content));
		}
	}

	unlink(rel);
}

} // namespace sprt::test
