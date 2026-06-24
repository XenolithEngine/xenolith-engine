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

// Directory enumeration via <dirent.h>. readdir order is unspecified, so entries
// are collected and sorted before printing; the per-platform "." / ".." entries
// are reported as presence flags rather than listed, and opaque values (d_ino,
// d_off, telldir cookies) are never printed. With a relative directory path the
// freestanding Windows libc_impl (which builds dirents from
// FileIdBothDirectoryInfo) produces output identical to the host.

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

static const char *kDir = "sprt_libc_dirent_d";

static void joinPath(char *out, size_t outSize, const char *a, const char *b) {
	snprintf(out, outSize, "%s/%s", a, b);
}

static void makeFile(const char *name, const char *content) {
	char p[256];
	joinPath(p, sizeof(p), kDir, name);
	int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		write(fd, content, strlen(content));
		close(fd);
	}
}

static void removeChild(const char *name) {
	char p[256];
	joinPath(p, sizeof(p), kDir, name);
	unlink(p);
}

struct Ent {
	char name[256];
	int type;
};

static int cmpEnt(const void *a, const void *b) {
	return strcmp(((const Ent *)a)->name, ((const Ent *)b)->name);
}

static const char *typeName(int t) {
	if (t == DT_DIR) {
		return "DIR";
	}
	if (t == DT_REG) {
		return "REG";
	}
	return "OTH";
}

// scandir filter: drop the "." and ".." entries so the listing is deterministic.
static int noDot(const struct dirent *e) {
	if (e->d_name[0] == '.'
			&& (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0))) {
		return 0;
	}
	return 1;
}

void performDirentTest() {
	// Fresh, known tree: 3 files + 1 subdirectory.
	char subPath[256];
	joinPath(subPath, sizeof(subPath), kDir, "sub");
	// Best-effort cleanup of any leftovers from a previous run.
	removeChild("a.txt");
	removeChild("b.txt");
	removeChild("c.dat");
	rmdir(subPath);
	rmdir(kDir);

	printf("mkdir=%d\n", mkdir(kDir, 0755));
	makeFile("a.txt", "aaa");
	makeFile("b.txt", "bbbb");
	makeFile("c.dat", "cc");
	printf("mkdir(sub)=%d\n", mkdir(subPath, 0755));

	// opendir + readdir: collect non-dot entries, flag "." / "..".
	DIR *d = opendir(kDir);
	printf("opendir=%s\n", d ? "ok" : "null");
	Ent ents[32];
	int n = 0;
	int hasDot = 0, hasDotDot = 0;
	struct dirent *e;
	while ((e = readdir(d)) != nullptr) {
		if (strcmp(e->d_name, ".") == 0) {
			hasDot = 1;
			continue;
		}
		if (strcmp(e->d_name, "..") == 0) {
			hasDotDot = 1;
			continue;
		}
		if (n < 32) {
			strncpy(ents[n].name, e->d_name, sizeof(ents[n].name) - 1);
			ents[n].name[sizeof(ents[n].name) - 1] = 0;
			ents[n].type = e->d_type;
			++n;
		}
	}
	printf("has '.'=%d has '..'=%d entries=%d\n", hasDot, hasDotDot, n);
	qsort(ents, n, sizeof(Ent), cmpEnt);
	for (int i = 0; i < n; ++i) {
		printf("  %s type=%s\n", ents[i].name, typeName(ents[i].type));
	}

	// rewinddir restarts the stream: the non-dot entry count must match.
	rewinddir(d);
	int n2 = 0;
	while ((e = readdir(d)) != nullptr) {
		if (noDot(e)) {
			++n2;
		}
	}
	printf("rewinddir entries=%d\n", n2);

	// telldir/seekdir round-trip: re-reading from a saved cookie reproduces the
	// same entry (cookie value itself is opaque and not printed).
	rewinddir(d);
	struct dirent *e0 = readdir(d);
	(void)e0;
	long pos = telldir(d);
	struct dirent *eAt = readdir(d);
	char savedName[256] = {0};
	if (eAt) {
		strncpy(savedName, eAt->d_name, sizeof(savedName) - 1);
	}
	readdir(d); // advance further
	seekdir(d, pos);
	struct dirent *eBack = readdir(d);
	bool replay = eBack && strcmp(eBack->d_name, savedName) == 0;
	printf("telldir/seekdir replay=%s\n", replay ? "PASS" : "FAIL");
	closedir(d);

	// dirfd returns a usable descriptor.
	d = opendir(kDir);
	printf("dirfd>=0=%d\n", dirfd(d) >= 0 ? 1 : 0);
	closedir(d);

	// scandir + alphasort: deterministic sorted listing (dots filtered out).
	struct dirent **list = nullptr;
	int sc = scandir(kDir, &list, &noDot, &alphasort);
	printf("scandir=%d\n", sc);
	for (int i = 0; i < sc; ++i) {
		printf("  scandir[%d]=%s type=%s\n", i, list[i]->d_name, typeName(list[i]->d_type));
		free(list[i]);
	}
	free(list);

	// opendir of a missing directory fails with ENOENT.
	errno = 0;
	DIR *nd = opendir("sprt_libc_dirent_nope");
	int ne = errno;
	printf("opendir(missing)=%s errno=%s\n", nd ? "ok" : "null", errnoName(ne));
	if (nd) {
		closedir(nd);
	}

	// Teardown.
	removeChild("a.txt");
	removeChild("b.txt");
	removeChild("c.dat");
	printf("rmdir(sub)=%d\n", rmdir(subPath));
	printf("rmdir=%d\n", rmdir(kDir));
}

} // namespace sprt::test
