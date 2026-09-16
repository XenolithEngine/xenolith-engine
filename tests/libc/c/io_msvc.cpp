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

// The MSVC <io.h> surface that is NOT a spelling of something POSIX: _access with
// the CRT's own mode rules, the wide file operations, and the _wfindfirst pattern
// walk. Portable code compiled for the msvc triple reaches for these because clang
// defines _MSC_VER there (sqlite's shell, curl, CPython), so what matters is that
// they answer the way the CRT does - which is what makes the host comparison the
// real check: on Windows both sides of the diff are a CRT.
//
// Windows only. Everything else has no such surface to compare against, and a
// prototype for it is not even declared there (see wrappers/unistd/io.h), so the
// whole body is compiled out and the test prints one skip line.

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "test_util.h"

#if SPRT_WINDOWS

#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <wchar.h>

#endif

namespace sprt::test {

#if SPRT_WINDOWS

static const wchar_t *kDirW = L"sprt_libc_io_msvc_d";
static const char *kDir = "sprt_libc_io_msvc_d";

static void makeFile(const char *name, const char *content) {
	char p[256];
	snprintf(p, sizeof(p), "%s/%s", kDir, name);
	if (FILE *f = fopen(p, "wb")) {
		fwrite(content, 1, strlen(content), f);
		fclose(f);
	}
}

// The attribute set, printed as letters rather than a number: ARCHIVE is set by
// some filesystems and not others, so listing the bits that were asked about
// keeps the diff about what the test controls.
static void printAttrib(const char *label, unsigned attrib) {
	printf("%s=%s%s%s\n", label, (attrib & _A_SUBDIR) ? "d" : "-",
			(attrib & _A_RDONLY) ? "r" : "-", (attrib & _A_HIDDEN) ? "h" : "-");
}

static void runFind() {
	// Pattern, not a directory: this is the whole reason the find surface exists
	// separately from opendir/readdir.
	wchar_t pattern[256];
	swprintf(pattern, 256, L"%ls\\*.txt", kDirW);

	struct _wfinddata_t info;
	auto h = _wfindfirst(pattern, &info);
	if (h == -1) {
		printf("wfindfirst=-1 errno=%s\n", errnoName(errno));
		return;
	}

	// Enumeration order is the filesystem's business, so collect and sort.
	wchar_t names[8][260];
	wchar_t *ptrs[8];
	long long sizes[8];
	int n = 0;
	do {
		if (n == 8) {
			break;
		}
		wcscpy(names[n], info.name);
		ptrs[n] = names[n];
		sizes[n] = info.size;
		++n;
	} while (_wfindnext(h, &info) == 0);

	printf("wfindnext_end_errno=%s\n", errnoName(errno));
	printf("findclose=%d\n", _findclose(h));

	// Sort names and sizes together - one bubble pass set, n is at most 8.
	for (int i = 0; i < n; ++i) {
		for (int j = i + 1; j < n; ++j) {
			if (wcscmp(ptrs[i], ptrs[j]) > 0) {
				auto *t = ptrs[i];
				ptrs[i] = ptrs[j];
				ptrs[j] = t;
				auto s = sizes[i];
				sizes[i] = sizes[j];
				sizes[j] = s;
			}
		}
	}
	printf("found=%d\n", n);
	for (int i = 0; i < n; ++i) { printf("  %ls size=%lld\n", ptrs[i], sizes[i]); }

	// A pattern that matches nothing is ENOENT, not an empty walk - the loop shape
	// every CRT caller writes depends on it.
	swprintf(pattern, 256, L"%ls\\*.nothing", kDirW);
	errno = 0;
	auto miss = _wfindfirst(pattern, &info);
	const int missErr = errno;
	printf("wfindfirst_miss=%d errno=%s\n", miss == -1 ? -1 : 0, errnoName(missErr));
	if (miss != -1) {
		_findclose(miss);
	}
}

#endif // SPRT_WINDOWS

void performIoMsvcTest() {
	printf("=== io_msvc ===\n");
#if !SPRT_WINDOWS
	printf("skip=not-windows\n");
#else
	wchar_t path[256];
	int rc = 0;
	int err = 0;

	// The call and errno are captured into locals before every printf. The order in
	// which printf's arguments are evaluated is unspecified, so errno read in one
	// argument can legally be the value from before the call made in another - which
	// prints a clean errno=0 next to a failure and hides exactly what is being checked.
	#define IO_PROBE(label, expr) \
		errno = 0; \
		rc = (expr); \
		err = errno; \
		printf(label "=%d errno=%s\n", rc, errnoName(err))

	// _wmkdir, and the second call failing rather than overwriting.
	_rmdir(kDir); // a leftover directory would make the first result depend on history
	printf("wmkdir=%d\n", _wmkdir(kDirW));
	IO_PROBE("wmkdir_again", _wmkdir(kDirW));

	makeFile("a.txt", "one");
	makeFile("bb.txt", "twotwo");
	makeFile("c.dat", "not matched by *.txt");

	// _access: mode 0 is existence, and it answers for a directory the same way it
	// answers for a file. That is the difference from access(), which refuses a
	// directory for R_OK/W_OK.
	IO_PROBE("access_dir_exist", _access(kDir, 0));
	IO_PROBE("access_dir_read", _access(kDir, 4));
	IO_PROBE("access_file", _access("sprt_libc_io_msvc_d/a.txt", 6));
	IO_PROBE("access_missing", _access("sprt_libc_io_msvc_d/nope", 0));

	// _wchmod clearing the write bit, seen through _access and through the attribute
	// the find walk reports.
	swprintf(path, 256, L"%ls\\a.txt", kDirW);
	printf("wchmod_ro=%d\n", _wchmod(path, _S_IREAD));
	IO_PROBE("access_ro_write", _access("sprt_libc_io_msvc_d/a.txt", 2));
	IO_PROBE("access_ro_read", _access("sprt_libc_io_msvc_d/a.txt", 4));

	struct _wfinddata_t one;
	errno = 0;
	auto h = _wfindfirst(path, &one);
	err = errno;
	if (h != -1) {
		printAttrib("attrib_ro", one.attrib);
		_findclose(h);
	} else {
		printf("attrib_ro=<none> errno=%s\n", errnoName(err));
	}
	printf("wchmod_rw=%d\n", _wchmod(path, _S_IREAD | _S_IWRITE));

	runFind();

	// _wunlink, including the read-only file whose attribute it has to clear first.
	printf("wchmod_ro2=%d\n", _wchmod(path, _S_IREAD));
	printf("wunlink_ro=%d\n", _wunlink(path));
	IO_PROBE("wunlink_again", _wunlink(path));

	swprintf(path, 256, L"%ls\\bb.txt", kDirW);
	printf("wunlink=%d\n", _wunlink(path));
	swprintf(path, 256, L"%ls\\c.dat", kDirW);
	printf("wunlink2=%d\n", _wunlink(path));
	printf("rmdir=%d\n", _rmdir(kDir));

	// _pclose pairs with _popen; the status is the child's, so use one that says
	// nothing on stdout and exits 0.
	if (FILE *p = _popen("cmd.exe /c exit 0", "r")) {
		char buf[64];
		while (fgets(buf, sizeof(buf), p)) { }
		printf("pclose=%d\n", _pclose(p));
	} else {
		printf("popen=NULL errno=%s\n", errnoName(errno));
	}
	IO_PROBE("pclose_null", _pclose(nullptr));

	#undef IO_PROBE
#endif
	printf("\n");
}

} // namespace sprt::test
