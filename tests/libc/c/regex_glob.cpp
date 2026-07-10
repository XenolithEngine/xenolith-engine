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

// <fnmatch.h>, <regex.h> and <glob.h> exercised through the SPRT libc forward.
// Output is normalized to be identical across every backend (glibc, musl, the
// freestanding musl adapter): a match is reported as its 0 / FNM_NOMATCH /
// REG_NOMATCH / GLOB_NOMATCH status, match offsets are printed explicitly, and
// regerror's platform-specific message text is reduced to empty/nonempty. glob
// runs against a freshly-built known tree and its (sorted) results are printed.

#include <fnmatch.h>
#include <regex.h>
#include <glob.h>

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

static const char *fnmYes(int r) { return r == 0 ? "match" : (r == FNM_NOMATCH ? "no" : "err"); }

void performFnmatchTest() {
	printf("== fnmatch ==\n");
	printf("*.txt/file.txt = %s\n", fnmYes(fnmatch("*.txt", "file.txt", 0)));
	printf("*.txt/file.md  = %s\n", fnmYes(fnmatch("*.txt", "file.md", 0)));
	printf("a?c/abc        = %s\n", fnmYes(fnmatch("a?c", "abc", 0)));
	printf("a?c/ac         = %s\n", fnmYes(fnmatch("a?c", "ac", 0)));
	printf("[a-c]x/bx      = %s\n", fnmYes(fnmatch("[a-c]x", "bx", 0)));
	printf("[a-c]x/dx      = %s\n", fnmYes(fnmatch("[a-c]x", "dx", 0)));
	printf("[!a-c]x/dx     = %s\n", fnmYes(fnmatch("[!a-c]x", "dx", 0)));
	// FNM_PATHNAME: '*' does not cross '/'.
	printf("PN */foo,a/foo = %s\n", fnmYes(fnmatch("*/foo", "a/foo", FNM_PATHNAME)));
	printf("PN */foo,a/b/f = %s\n", fnmYes(fnmatch("*/foo", "a/b/foo", FNM_PATHNAME)));
	// FNM_CASEFOLD.
	printf("CF FILE/file   = %s\n", fnmYes(fnmatch("FILE", "file", FNM_CASEFOLD)));
	printf("FILE/file      = %s\n", fnmYes(fnmatch("FILE", "file", 0)));
}

// nmatch: how many regmatch_t slots to request & print (0 = match/no only, e.g.
// for REG_NOSUB where subexpression offsets are not reported).
static void regShow(const char *label, const char *pat, int cflags, const char *text, int nmatch) {
	regex_t re;
	int ce = regcomp(&re, pat, cflags);
	if (ce != 0) {
		// The specific REG_* error code for a malformed pattern is
		// implementation-defined (glibc says REG_BADPAT, musl REG_EBRACK for
		// "a["), so only the fact of the error + a non-empty message are checked.
		char buf[128];
		size_t n = regerror(ce, &re, buf, sizeof(buf));
		printf("%s: comp-err msg=%s\n", label, (n > 0 && buf[0]) ? "nonempty" : "empty");
		return;
	}
	regmatch_t m[8];
	int xe = regexec(&re, text, (size_t) nmatch, nmatch ? m : nullptr, 0);
	if (xe == 0) {
		printf("%s: match", label);
		for (int i = 0; i < nmatch; ++i) {
			if (m[i].rm_so < 0) {
				printf(" g%d=-", i);
			} else {
				printf(" g%d=[%d,%d]", i, (int) m[i].rm_so, (int) m[i].rm_eo);
			}
		}
		printf("\n");
	} else {
		printf("%s: %s\n", label, xe == REG_NOMATCH ? "no" : "err");
	}
	regfree(&re);
}

void performRegexTest() {
	printf("== regex ==\n");
	regShow("anchor.dot ", "^h.llo$", REG_EXTENDED, "hello", 1);
	regShow("anchor.miss", "^h.llo$", REG_EXTENDED, "world", 1);
	regShow("trail.dot  ", "he.", REG_EXTENDED, "hello", 1);
	regShow("groups     ", "h(e)(l+)o", REG_EXTENDED, "hello", 4);
	regShow("class.quant", "[0-9]+", REG_EXTENDED, "abc123", 1);
	regShow("icase      ", "^hello$", REG_EXTENDED | REG_ICASE, "HeLLo", 1);
	// REG_NOSUB: only whole-match status is defined, so request no offsets.
	regShow("nosub      ", "ab*c", REG_EXTENDED | REG_NOSUB, "abbbc", 0);
	// Malformed pattern -> compile error (only the empty/nonempty of the message
	// is printed; the wording differs per libc).
	regShow("badpat     ", "a[", REG_EXTENDED, "a", 1);
}

static const char *kGlobDir = "sprt_libc_glob_d";

static void makeFile(const char *dir, const char *name) {
	char p[256];
	snprintf(p, sizeof(p), "%s/%s", dir, name);
	int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		close(fd);
	}
}

static void rmFile(const char *dir, const char *name) {
	char p[256];
	snprintf(p, sizeof(p), "%s/%s", dir, name);
	unlink(p);
}

static void globRun(const char *label, const char *pattern, int flags) {
	glob_t g;
	int r = glob(pattern, flags, nullptr, &g);
	if (r == 0) {
		printf("%s: count=%zu", label, (size_t) g.gl_pathc);
		for (size_t i = 0; i < g.gl_pathc; ++i) { printf(" [%s]", g.gl_pathv[i]); }
		printf("\n");
		globfree(&g);
	} else {
		printf("%s: %s\n", label, r == GLOB_NOMATCH ? "nomatch" : "err");
	}
}

void performGlobTest() {
	printf("== glob ==\n");
	rmFile(kGlobDir, "a.txt");
	rmFile(kGlobDir, "b.txt");
	rmFile(kGlobDir, "c.dat");
	rmdir(kGlobDir);
	mkdir(kGlobDir, 0755);
	makeFile(kGlobDir, "a.txt");
	makeFile(kGlobDir, "b.txt");
	makeFile(kGlobDir, "c.dat");

	char pat[256];
	snprintf(pat, sizeof(pat), "%s/*.txt", kGlobDir);
	globRun("txt   ", pat, 0);
	snprintf(pat, sizeof(pat), "%s/*", kGlobDir);
	globRun("all   ", pat, 0);
	snprintf(pat, sizeof(pat), "%s/nope_*", kGlobDir);
	globRun("nomatch", pat, 0);
	snprintf(pat, sizeof(pat), "%s/[a-b].txt", kGlobDir);
	globRun("class ", pat, 0);

	rmFile(kGlobDir, "a.txt");
	rmFile(kGlobDir, "b.txt");
	rmFile(kGlobDir, "c.dat");
	rmdir(kGlobDir);
}

} // namespace sprt::test
