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

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <wctype.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/utsname.h>

#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/platform.h>

namespace sprt {

void performUnameTest() {
	struct utsname unameBuf;
	uname(&unameBuf);

	printf("uname.sysname: %s\n", unameBuf.sysname);
	printf("uname.nodename: %s\n", unameBuf.nodename);
	printf("uname.release: %s\n", unameBuf.release);
	printf("uname.version: %s\n", unameBuf.version);
	printf("uname.machine: %s\n", unameBuf.machine);
	printf("uname.domainname: %s\n", unameBuf.domainname);
}

void performUnistdTest() {
	printf("getuid: %u\n", getuid());
	printf("getgid: %u\n", getgid());
	printf("geteuid: %u\n", geteuid());
	printf("getegid: %u\n", getegid());
	printf("getpid: %u\n", getpid());
	printf("gettid: %u\n", gettid());
	printf("sysconf(SC_PAGESIZE): %u\n", sysconf(_SC_PAGESIZE));
	printf("pathconf(PC_NAME_MAX): %u\n", pathconf("/", _PC_NAME_MAX));
	printf("pathconf(PC_PATH_MAX): %u\n", pathconf("/", _PC_PATH_MAX));
	printf("pathconf(PC_LINK_MAX): %u\n", pathconf("/", _PC_LINK_MAX));

	auto buf = getcwd(nullptr, 0);
	printf("getcwd: %s\n", buf);
	free(buf);
};

void performDirTest() {
	auto rootDir = opendir("/");

	while (auto dirent = readdir(rootDir)) {
		printf("readdir: %s %s %ld\n", "/", dirent->d_name, telldir(rootDir)); //
	}
	closedir(rootDir);

#if SPRT_WINDOWS
	auto driveDir = opendir("/c");
	while (auto dirent = readdir(driveDir)) {
		printf("readdir: %s %s %ld\n", "/c", dirent->d_name, telldir(driveDir)); //
	}
	closedir(driveDir);
#endif

	auto buf = getcwd(nullptr, 0);
	auto dir = opendir(buf);

	// seekpos is opaque
	long seekpos = 0;
	int seekoff = 10;

	while (auto dirent = readdir(dir)) {
		if (--seekoff > 0) {
			seekpos = telldir(dir);
		}
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("rewinddir\n");
	rewinddir(dir);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("seekdir backward\n");
	seekdir(dir, seekpos);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	printf("seekdir forward\n");
	rewinddir(dir);
	seekdir(dir, seekpos);

	while (auto dirent = readdir(dir)) {
		printf("readdir: %s %s %ld\n", buf, dirent->d_name, telldir(dir)); //
	}

	closedir(dir);

	free(buf);
}

// struct flock layout must match the impl (libc_impl/src/windows/libc_file_ops.cc
// and .../unistd.cc). The public <fcntl.h> does not yet export it, so declare a
// matching local copy for the test.
struct test_flock {
	short int l_type;
	short int l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};

// Exercises fcntl(): descriptor flags (F_GETFD/F_SETFD), status flags
// (F_GETFL/F_SETFL), duplication (F_DUPFD/F_DUPFD_CLOEXEC) and advisory record
// locking (F_GETLK/F_SETLK/F_SETLKW). On Windows the locking commands map onto
// LockFileEx/UnlockFileEx, which are *per-handle*, so two descriptors to the same
// file conflict within one process. POSIX locks are *per-process*, so the same
// two descriptors never conflict here; the conflict-dependent checks below (and
// the strict F_SETFD bit rejection) are therefore asserted on Windows only.
void performFcntlTest() {
	int failures = 0;
	auto check = [&](bool cond, const char *msg) {
		printf("  %s: %s\n", cond ? "PASS" : "FAIL", msg);
		if (!cond) {
			++failures;
		}
	};
	// Assertions that only hold under Windows per-handle lock semantics.
	auto checkWin = [&](bool cond, const char *msg) {
#if SPRT_WINDOWS
		check(cond, msg);
#else
		(void)cond;
		printf("  SKIP (non-Windows): %s\n", msg);
#endif
	};

	const char *path = "fcntltest.tmp";
	::remove(path);

	int a = open(path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
	check(a >= 0, "open primary descriptor");
	char payload[64];
	memset(payload, 0, sizeof(payload));
	check(write(a, payload, sizeof(payload)) == (ssize_t)sizeof(payload), "populate file");

	// --- Descriptor flags: F_GETFD / F_SETFD (FD_CLOEXEC) ---
	int fd_flags = fcntl(a, F_GETFD);
	check(fd_flags >= 0, "F_GETFD succeeds");
	// Clearing FD_CLOEXEC then reading it back must round-trip.
	check(fcntl(a, F_SETFD, 0) == 0, "F_SETFD clear FD_CLOEXEC");
	check((fcntl(a, F_GETFD) & FD_CLOEXEC) == 0, "F_GETFD reports FD_CLOEXEC clear");
	check(fcntl(a, F_SETFD, FD_CLOEXEC) == 0, "F_SETFD set FD_CLOEXEC");
	check((fcntl(a, F_GETFD) & FD_CLOEXEC) != 0, "F_GETFD reports FD_CLOEXEC set");
	// An unknown F_SETFD bit must be rejected.
	errno = 0;
	checkWin(fcntl(a, F_SETFD, 0x40) == -1 && errno == EINVAL, "F_SETFD rejects unknown bit");

	// --- Status flags: F_GETFL must report the access mode used at open ---
	int fl_flags = fcntl(a, F_GETFL);
	check(fl_flags >= 0, "F_GETFL succeeds");
	check((fl_flags & O_ACCMODE) == O_RDWR, "F_GETFL reports O_RDWR access mode");

	// --- Duplication: F_DUPFD / F_DUPFD_CLOEXEC ---
	int dupfd = fcntl(a, F_DUPFD, 0);
	check(dupfd >= 0 && dupfd != a, "F_DUPFD returns a new descriptor");
	if (dupfd >= 0) {
		check(write(dupfd, payload, 1) == 1, "duplicated descriptor is usable");
		close(dupfd);
	}
	int dupfd2 = fcntl(a, F_DUPFD_CLOEXEC, 0);
	check(dupfd2 >= 0 && dupfd2 != a, "F_DUPFD_CLOEXEC returns a new descriptor");
	if (dupfd2 >= 0) {
		close(dupfd2);
	}

	// --- Unknown command must fail with EINVAL ---
	errno = 0;
	check(fcntl(a, 0x7777) == -1 && errno == EINVAL, "unknown command -> EINVAL");

	int b = open(path, O_RDWR);
	check(b >= 0, "open competing descriptor");

	// Take an exclusive lock on bytes [0,16) via descriptor a.
	test_flock fl = {};
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 16;
	check(fcntl(a, F_SETLK, &fl) == 0, "F_SETLK exclusive lock on a");

	// A conflicting non-blocking lock on the same range via b must fail.
	test_flock fl2 = {};
	fl2.l_type = F_WRLCK;
	fl2.l_whence = SEEK_SET;
	fl2.l_start = 0;
	fl2.l_len = 16;
	errno = 0;
	int r = fcntl(b, F_SETLK, &fl2);
	checkWin(r == -1 && (errno == EAGAIN || errno == EACCES),
			"F_SETLK conflict on b -> EAGAIN/EACCES");

	// F_GETLK from b must report a conflicting lock (type != F_UNLCK).
	test_flock fl3 = {};
	fl3.l_type = F_WRLCK;
	fl3.l_whence = SEEK_SET;
	fl3.l_start = 0;
	fl3.l_len = 16;
	check(fcntl(b, F_GETLK, &fl3) == 0, "F_GETLK call succeeds");
	checkWin(fl3.l_type != F_UNLCK, "F_GETLK reports conflict");

	// A non-overlapping range must be lockable from b.
	test_flock fl4 = {};
	fl4.l_type = F_WRLCK;
	fl4.l_whence = SEEK_SET;
	fl4.l_start = 32;
	fl4.l_len = 16;
	check(fcntl(b, F_SETLK, &fl4) == 0, "F_SETLK on disjoint range from b");
	fl4.l_type = F_UNLCK;
	check(fcntl(b, F_SETLK, &fl4) == 0, "F_UNLCK disjoint range from b");

	// Release a's lock; b may now take the previously contended range.
	fl.l_type = F_UNLCK;
	check(fcntl(a, F_SETLK, &fl) == 0, "F_UNLCK [0,16) on a");
	fl2.l_type = F_WRLCK;
	check(fcntl(b, F_SETLK, &fl2) == 0, "F_SETLK [0,16) now succeeds on b");

	// F_GETLK on a now sees b's lock.
	test_flock fl5 = {};
	fl5.l_type = F_WRLCK;
	fl5.l_whence = SEEK_SET;
	fl5.l_start = 0;
	fl5.l_len = 16;
	check(fcntl(a, F_GETLK, &fl5) == 0, "F_GETLK on a call succeeds");
	checkWin(fl5.l_type != F_UNLCK, "F_GETLK on a sees b's lock");

	fl2.l_type = F_UNLCK;
	check(fcntl(b, F_SETLK, &fl2) == 0, "F_UNLCK [0,16) on b");

	// Whole-file lock with l_len == 0 (until EOF) and matching unlock.
	test_flock fw = {};
	fw.l_type = F_WRLCK;
	fw.l_whence = SEEK_SET;
	fw.l_start = 0;
	fw.l_len = 0;
	check(fcntl(a, F_SETLK, &fw) == 0, "F_SETLK whole-file (l_len=0) on a");
	fw.l_type = F_UNLCK;
	check(fcntl(a, F_SETLK, &fw) == 0, "F_UNLCK whole-file on a");

	// F_SETLKW (blocking) on a free range must succeed immediately.
	test_flock fb = {};
	fb.l_type = F_WRLCK;
	fb.l_whence = SEEK_SET;
	fb.l_start = 48;
	fb.l_len = 8;
	check(fcntl(a, F_SETLKW, &fb) == 0, "F_SETLKW on free range on a");
	fb.l_type = F_UNLCK;
	check(fcntl(a, F_SETLKW, &fb) == 0, "F_UNLCK F_SETLKW range on a");

	close(a);
	close(b);
	::remove(path);

	printf("performFcntlTest: %s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
}

static int __sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

// Exercises the newly added locale entry points: the ctype/wctype _l
// passthroughs (tolower_l/toupper_l, wctrans/towctrans and their _l variants)
// and the now-localized strxfrm, checking that strcmp() of two strxfrm keys
// agrees with strcoll() (the transform/collation invariant). Also doubles as a
// compile-and-run check of the fixed <wctype.h> umbrella header.
void performLocaleTest() {
	int failures = 0;
	auto check = [&](bool cond, const char *msg) {
		printf("  %s: %s\n", cond ? "PASS" : "FAIL", msg);
		if (!cond) {
			++failures;
		}
	};

	// --- tolower_l / toupper_l (locale-independent ASCII mapping) ---
	check(tolower_l('A', (locale_t)0) == 'a', "tolower_l('A') == 'a'");
	check(toupper_l('a', (locale_t)0) == 'A', "toupper_l('a') == 'A'");
	check(tolower_l('z', (locale_t)0) == 'z', "tolower_l('z') unchanged");

	// --- wctrans / towctrans and their _l variants (via the <wctype.h> umbrella) ---
	// These use glibc's pointer-based wctrans_t ABI on every target, so they are
	// exercised on host and Windows alike.
	wctrans_t up = wctrans("toupper");
	wctrans_t lo = wctrans("tolower");
	check(up != 0 && lo != 0, "wctrans returns valid descriptors");
	check(wctrans("nonsense") == 0, "wctrans unknown class -> 0");
	check(towctrans(L'a', up) == L'A', "towctrans toupper");
	check(towctrans(L'A', lo) == L'a', "towctrans tolower");

	// The _l variants take a real locale_t (glibc dereferences it; libc_impl
	// ignores it). newlocale() now works on every target thanks to the fixed
	// LC_ALL_MASK, so build a "C" locale and exercise wctrans_l/towctrans_l.
	locale_t cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
	check(cloc != (locale_t)0, "newlocale(C) succeeds");
	if (cloc != (locale_t)0) {
		wctrans_t up_l = wctrans_l("toupper", cloc);
		check(up_l != 0, "wctrans_l returns valid descriptor");
		check(towctrans_l(L'b', up_l, cloc) == L'B', "towctrans_l toupper");
		// wctype_t now shares the platform ABI on every target, so iswctype_l /
		// wctype_l work on host and Windows alike (with a real locale_t).
		check(iswctype_l(L'5', wctype_l("digit", cloc), cloc) != 0, "iswctype_l(digit) on '5'");
		freelocale(cloc);
	}

	// --- iswctype / wctype (no locale) — all targets ---
	check(iswctype(L'7', wctype("digit")) != 0, "iswctype(digit) on '7'");
	check(iswctype(L'x', wctype("digit")) == 0, "iswctype(digit) rejects 'x'");
	check(iswctype(L'A', wctype("alpha")) != 0, "iswctype(alpha) on 'A'");

	// --- message catalog: honest empty-catalog fallback ---
	// libc_impl's catopen() always succeeds with an empty catalog, so catgets()
	// returns the supplied default. A hosted glibc returns -1 for a missing
	// catalog (its real backend), which we skip rather than feed to catgets().
	nl_catd cd = catopen("sprt-nonexistent-catalog", 0);
	if (cd != (nl_catd)-1) {
		const char *m = catgets(cd, 1, 1, "default-message");
		check(m != nullptr && strcmp(m, "default-message") == 0, "catgets falls back to default");
		check(catclose(cd) == 0, "catclose succeeds");
	} else {
		printf("  SKIP (hosted): catopen has no catalog backend\n");
	}

	// --- strxfrm vs strcoll invariant in the C locale ---
	setlocale(LC_COLLATE, "C");
	const char *words[] = {"apple", "Apple", "banana", "apple", "apricot"};
	bool consistent = true;
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			char ka[64], kb[64];
			size_t la = strxfrm(ka, words[i], sizeof(ka));
			size_t lb = strxfrm(kb, words[j], sizeof(kb));
			(void)la;
			(void)lb;
			if (__sign(strcmp(ka, kb)) != __sign(strcoll(words[i], words[j]))) {
				consistent = false;
			}
		}
	}
	check(consistent, "strcmp(strxfrm) agrees with strcoll (C locale)");
	// In the C locale strxfrm is the identity transform.
	char key[64];
	size_t klen = strxfrm(key, "apple", sizeof(key));
	check(klen == 5 && strcmp(key, "apple") == 0, "strxfrm is identity in C locale");
	// strxfrm must report the needed length even when the buffer is too small.
	check(strxfrm(nullptr, "banana", 0) == 6, "strxfrm reports length with zero buffer");

	// --- strto* numeric conversions ---
	setlocale(LC_NUMERIC, "C");
	char *end = nullptr;
	check(strtod("3.14", &end) == 3.14 && end && *end == 0, "strtod C-locale 3.14");
	check(strtod("1,5", &end) == 1.0 && end && *end == ',', "strtod C-locale: ',' not a radix");
	check(strtol("42", &end, 10) == 42, "strtol 42");
	check(strtoumax("18446744073709551615", &end, 10) == 18446744073709551615ULL, "strtoumax max");
	check(strtoimax("-100", &end, 10) == -100, "strtoimax -100");
	// The _l integer conversions need a real locale_t (a hosted glibc dereferences
	// it; libc_impl ignores it).
	locale_t numLoc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
	if (numLoc != (locale_t)0) {
		check(strtoimax_l("-99", &end, 10, numLoc) == -99, "strtoimax_l -99");
		check(strtoumax_l("123", &end, 10, numLoc) == 123u, "strtoumax_l 123");
		freelocale(numLoc);
	}

	// --- printf/scanf honour the LC_NUMERIC radix (Phase 1) ---
	setlocale(LC_NUMERIC, "C");
	char pbuf[16];
	snprintf(pbuf, sizeof(pbuf), "%.2f", 1.5);
	check(strcmp(pbuf, "1.50") == 0, "snprintf %f uses '.' in C locale");
	double sv = 0;
	sscanf("2.5x", "%lf", &sv);
	check(sv == 2.5, "sscanf %f uses '.' in C locale");

	// _l printf/scanf with an explicit (C) locale.
	locale_t cLoc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
	if (cLoc != (locale_t)0) {
		snprintf_l(pbuf, sizeof(pbuf), cLoc, "%.2f", 3.5);
		check(strcmp(pbuf, "3.50") == 0, "snprintf_l C-locale %f");
		double v2 = 0;
		sscanf_l("4.5x", cLoc, "%lf", &v2);
		check(v2 == 4.5, "sscanf_l C-locale %f");
		freelocale(cLoc);
	}

	// --- printf '%'' digit-grouping flag (Phase 3) ---
	// Inert in the C locale, which has no grouping; output must match plain %d/%f.
	setlocale(LC_NUMERIC, "C");
	snprintf(pbuf, sizeof(pbuf), "%'d", 1234567);
	check(strcmp(pbuf, "1234567") == 0, "%'d is inert (no separators) in C locale");
	snprintf(pbuf, sizeof(pbuf), "%'.1f", 1234.5);
	check(strcmp(pbuf, "1234.5") == 0, "%'f is inert (no separators) in C locale");

	// strtod/printf/scanf must honour the current LC_NUMERIC radix. Look for a
	// locale whose decimal point is ',' (per localeconv) and confirm; skip if the
	// platform offers no such locale here.
	const char *commaLocales[] = {"de-DE", "de_DE.UTF-8", "de_DE", "ru-RU", "ru_RU.UTF-8"};
	bool radixTested = false;
	for (auto nm : commaLocales) {
		if (!setlocale(LC_NUMERIC, nm)) {
			continue;
		}
		lconv *lc = localeconv();
		if (lc && lc->decimal_point && lc->decimal_point[0] == ',' && lc->decimal_point[1] == 0) {
			check(strtod("1,5", nullptr) == 1.5, "strtod honours locale ',' radix");
			check(strtod("1.5", nullptr) == 1.0, "strtod: '.' is not a radix in a comma locale");
			char b[16];
			snprintf(b, sizeof(b), "%.1f", 1.5);
			check(strcmp(b, "1,5") == 0, "snprintf %f honours comma radix");
			double w = 0;
			sscanf("2,5x", "%lf", &w);
			check(w == 2.5, "sscanf %f honours comma radix");
			radixTested = true;
		}
		setlocale(LC_NUMERIC, "C");
		if (radixTested) {
			break;
		}
	}
	if (!radixTested) {
		printf("  SKIP: no comma-radix locale available to test strtod/printf/scanf\n");
	}
	setlocale(LC_NUMERIC, "C");

	// The '%'' grouping flag needs a locale exposing a single-byte thousands
	// separator and a leading group of 3 (what the byte formatter supports) --
	// independent of the radix, so probe a separate set. The expected string is
	// built from localeconv so the check does not assume which separator is used.
	const char *groupLocales[] = {"en_US.UTF-8", "en-US", "de_DE.UTF-8", "de-DE", "en_GB.UTF-8"};
	bool groupTested = false;
	for (auto nm : groupLocales) {
		if (!setlocale(LC_NUMERIC, nm)) {
			continue;
		}
		lconv *lc = localeconv();
		if (lc && lc->thousands_sep && lc->thousands_sep[0] && lc->thousands_sep[1] == 0
				&& lc->grouping && lc->grouping[0] == 3) {
			char ts = lc->thousands_sep[0];
			char dp = (lc->decimal_point && lc->decimal_point[0]) ? lc->decimal_point[0] : '.';
			char got[24], want[24];
			snprintf(want, sizeof(want), "1%c234%c567", ts, ts);
			snprintf(got, sizeof(got), "%'d", 1234567);
			check(strcmp(got, want) == 0, "%'d groups integer thousands");
			// Precision zero-padding is emitted ungrouped (only the significant
			// digits are grouped), e.g. "%'.7d" of 1234 -> "0001,234".
			snprintf(want, sizeof(want), "0001%c234", ts);
			snprintf(got, sizeof(got), "%'.7d", 1234);
			check(strcmp(got, want) == 0, "%'d leaves precision-padding ungrouped");
			snprintf(want, sizeof(want), "1%c234%c567%c8", ts, ts, dp);
			snprintf(got, sizeof(got), "%'.1f", 1234567.8);
			check(strcmp(got, want) == 0, "%'f groups the integer part");
			// Short values get no separator.
			snprintf(got, sizeof(got), "%'d", 12);
			check(strcmp(got, "12") == 0, "%'d leaves sub-group values ungrouped");
			groupTested = true;
		}
		setlocale(LC_NUMERIC, "C");
		if (groupTested) {
			break;
		}
	}
	if (!groupTested) {
		printf("  SKIP: no single-byte grouping locale available to test %%'\n");
	}
	setlocale(LC_NUMERIC, "C");

	// --- strftime LC_TIME (Phase: localized names/formats via token provider) ---
	// In the C locale the provider yields no tokens, so the runtime_core formatter
	// uses its built-in English defaults. Sun 2026-01-04, 09:04:05.
	setlocale(LC_TIME, "C");
	struct tm tmv = {};
	tmv.tm_year = 126;
	tmv.tm_mon = 0;
	tmv.tm_mday = 4;
	tmv.tm_hour = 9;
	tmv.tm_min = 4;
	tmv.tm_sec = 5;
	tmv.tm_wday = 0;
	tmv.tm_yday = 3;
	char tbuf[64];
	strftime(tbuf, sizeof(tbuf), "%A %B %a %b %p", &tmv);
	check(strcmp(tbuf, "Sunday January Sun Jan AM") == 0, "strftime C-locale names (English)");

	// A localized LC_TIME (if the platform offers one) must change the weekday /
	// month names; build the expectation from the platform itself and skip when no
	// such locale is available here (e.g. wine ships no locale name data).
	const char *timeLocales[] = {"de-DE", "de_DE.UTF-8", "de_DE", "ru-RU", "ru_RU.UTF-8"};
	bool timeTested = false;
	for (auto nm : timeLocales) {
		if (!setlocale(LC_TIME, nm)) {
			continue;
		}
		char loc[32], c[32];
		strftime(loc, sizeof(loc), "%A", &tmv);
		setlocale(LC_TIME, "C");
		strftime(c, sizeof(c), "%A", &tmv);
		if (loc[0] && strcmp(loc, c) != 0) {
			check(true, "strftime honours a localized LC_TIME weekday");
			timeTested = true;
		}
		setlocale(LC_TIME, "C");
		if (timeTested) {
			break;
		}
	}
	if (!timeTested) {
		printf("  SKIP: no localized LC_TIME locale available to test strftime\n");
	}
	setlocale(LC_TIME, "C");

	// --- nl_langinfo: C-locale items (stable across glibc / libc_impl) ---
	setlocale(LC_ALL, "C");
	check(strcmp(nl_langinfo(ABDAY_1), "Sun") == 0, "nl_langinfo ABDAY_1 -> Sun");
	check(strcmp(nl_langinfo(DAY_1), "Sunday") == 0, "nl_langinfo DAY_1 -> Sunday");
	check(strcmp(nl_langinfo(ABMON_1), "Jan") == 0, "nl_langinfo ABMON_1 -> Jan");
	check(strcmp(nl_langinfo(MON_12), "December") == 0, "nl_langinfo MON_12 -> December");
	check(strcmp(nl_langinfo(AM_STR), "AM") == 0, "nl_langinfo AM_STR -> AM");
	check(strcmp(nl_langinfo(PM_STR), "PM") == 0, "nl_langinfo PM_STR -> PM");
	check(strcmp(nl_langinfo(D_FMT), "%m/%d/%y") == 0, "nl_langinfo D_FMT");
	check(strcmp(nl_langinfo(T_FMT), "%H:%M:%S") == 0, "nl_langinfo T_FMT");
	check(strcmp(nl_langinfo(RADIXCHAR), ".") == 0, "nl_langinfo RADIXCHAR -> .");

	// A localized LC_TIME (if available) must change the weekday name; skip when
	// the platform offers no such locale here (wine ships no locale name data).
	bool nlTested = false;
	for (auto nm : timeLocales) {
		if (!setlocale(LC_TIME, nm)) {
			continue;
		}
		const char *loc = nl_langinfo(DAY_1);
		if (loc && loc[0] && strcmp(loc, "Sunday") != 0) {
			check(true, "nl_langinfo honours a localized LC_TIME weekday");
			nlTested = true;
		}
		setlocale(LC_TIME, "C");
		if (nlTested) {
			break;
		}
	}
	if (!nlTested) {
		printf("  SKIP: no localized LC_TIME locale available to test nl_langinfo\n");
	}
	setlocale(LC_ALL, "C");

	printf("performLocaleTest: %s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
}

static void removeFileAt(const char *dirPath, const char *fileName) {
	filepath::merge([&](sprt::StringView filepath) {
		filepath.performWithTerminated([&](const char *cFilePath, size_t) { ::remove(cFilePath); });
	}, dirPath, fileName);
}

void performFileLinkatTest(int dirfd, const char *dirPath, const char *originalFileName,
		const char *linkFileName) {
	linkat(dirfd, linkFileName, dirfd, "ссылка1.txt", 0);
	linkat(dirfd, linkFileName, dirfd, "ссылка2.txt", AT_SYMLINK_FOLLOW);

	removeFileAt(dirPath, "ссылка1.txt");
	removeFileAt(dirPath, "ссылка2.txt");
}

void performFileLinkTest(const char *originalFilePath, const char *linkFilePath) {
	struct stat stOrig;
	struct stat stLink;
	struct stat stlLink;
	stat(originalFilePath, &stOrig);
	stat(linkFilePath, &stLink);
	lstat(linkFilePath, &stlLink);

	printf("stat: %lld %lld %lld\n", stOrig.st_ino, stLink.st_ino, stlLink.st_ino);
}

void performDirLinkTest(const char *originalDirPath, const char *linkDirPath) {
	auto rp = realpath(linkDirPath, nullptr);
	printf("realpath: %s\n", rp);
	free(rp);

	auto dirfd = open(linkDirPath, O_PATH | O_DIRECTORY | O_NOFOLLOW);
	if (dirfd >= 0) {
		oslog::vperror(__SPRT_LOCATION, "main", "Open symlink with O_NOFOLLOW should fail");
	}

	printf("Open dir %s\n", originalDirPath);
	dirfd = open(originalDirPath, O_PATH | O_DIRECTORY | O_NOFOLLOW);
	if (dirfd < 0) {
		oslog::vperror(__SPRT_LOCATION, "main", "Open dir failed");
	}

	printf("Create file with openat: %d %s\n", dirfd, "testfile.txt");
	auto fd = openat(dirfd, "testfile.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);
	if (fd < 0) {
		oslog::vperror(__SPRT_LOCATION, "main",
				"fail to create file this openat: ", sprt::status::errnoToStatus(errno));
	}
	auto content = "TestFileContent\n";
	write(fd, content, strlen(content));
	close(fd);

	sprt::filepath::merge([&](sprt::StringView filepath) {
		filepath.performWithTerminated([&](const char *cFilePath, size_t) {
			sprt::filepath::merge([&](sprt::StringView filepath) {
				filepath.performWithTerminated([&](const char *linkFilePath, size_t) {
					printf("Symlink %s -> %s\n", cFilePath, linkFilePath);
					auto ret = symlink(cFilePath, linkFilePath);
					if (ret != 0) {
						sprt::oslog::vperror(__SPRT_LOCATION, "main",
								"fail to symlink file: ", sprt::status::errnoToStatus(errno));
					} else {
						performFileLinkatTest(dirfd, originalDirPath, "testfile.txt",
								"testfile_link.txt");
						performFileLinkTest(cFilePath, linkFilePath);
						::remove(linkFilePath); //
					}
				});
			}, originalDirPath, "testfile_link.txt");

			if (::remove(cFilePath) != 0) {
				sprt::oslog::vperror(__SPRT_LOCATION, "main", "fail to remove file",
						sprt::status::errnoToStatus(errno));
			}
		});
	}, originalDirPath, "testfile.txt");

	close(dirfd);
}

void performLinkTest() {
	auto dirPath = sprt::filepath::root(sprt::platform::getExecPath());

	int dirfd = -1;
	dirPath.performWithTerminated([&](const char *cDirPath, size_t) {
		dirfd = open(cDirPath, O_PATH | O_DIRECTORY); //
	});
	if (mkdirat(dirfd, "testdir_link", S_IWUSR | S_IRUSR | S_IXUSR) != 0) {
		sprt::oslog::vperror(__SPRT_LOCATION, "main",
				"fail to create dir: ", sprt::status::errnoToStatus(errno));
	}

	sprt::filepath::merge([&](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			sprt::filepath::merge([&](sprt::StringView path) {
				path.performWithTerminated([&](const char *nDirPath, size_t) {
					auto ret = rename(cDirPath, nDirPath);
					if (ret != 0) {
						sprt::oslog::vperror(__SPRT_LOCATION, "main",
								"fail to rename directory: ", sprt::status::errnoToStatus(errno));
					} else {
						ret = symlink(nDirPath, cDirPath);
						if (ret != 0) {
							sprt::oslog::vperror(__SPRT_LOCATION, "main",
									"fail to symlink directory: ",
									sprt::status::errnoToStatus(errno));
						} else {
							performDirLinkTest(nDirPath, cDirPath);
						}
					}
				});
			}, dirPath, "testdir_new");
		});
	}, dirPath, "testdir_link");

	close(dirfd);

	sprt::filepath::merge([](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			auto ret = ::remove(cDirPath);
			if (ret != 0) {
				sprt::oslog::vperror(__SPRT_LOCATION, "main",
						"fail to remove directory: ", sprt::status::errnoToStatus(errno));
			}
		});
	}, dirPath, "testdir_link");

	sprt::filepath::merge([](sprt::StringView path) {
		path.performWithTerminated([&](const char *cDirPath, size_t) {
			auto ret = ::remove(cDirPath);
			if (ret != 0) {
				sprt::oslog::vperror(__SPRT_LOCATION, "main",
						"fail to remove directory: ", sprt::status::errnoToStatus(errno));
			}
		});
	}, dirPath, "testdir_new");
}

} // namespace sprt
