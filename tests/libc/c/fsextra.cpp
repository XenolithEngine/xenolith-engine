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

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

// The *at / fd-metadata family that the filesystem layer relies on but that the
// wasm libc gained late: faccessat, fsync/fdatasync, fpathconf/pathconf, utimensat
// (with a stat round-trip), and fdopendir. Output is normalised to booleans / errno
// names so the exact platform-specific limit values do not affect the diff.
void performFsExtraTest() {
	const char *f = "sprt_libc_fsx.tmp";
	unlink(f);
	int fd = open(f, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	write(fd, "hello", 5);
	close(fd);

	// faccessat: AT_FDCWD relative existence, present vs missing
	printf("faccessat(F_OK)=%d\n", faccessat(AT_FDCWD, f, F_OK, 0));
	errno = 0;
	int fa = faccessat(AT_FDCWD, "sprt_libc_fsx_nope.tmp", F_OK, 0);
	printf("faccessat(missing)=%d errno=%s\n", fa, errnoName(errno));

	// fsync on a valid fd succeeds; a bogus fd reports EBADF
	fd = open(f, O_RDWR, 0);
	printf("fsync=%d\n", fsync(fd));
	close(fd);
	errno = 0;
	int bs = fsync(31000);
	printf("fsync(badfd)=%d errno=%s\n", bs, errnoName(errno));

	// fpathconf (fd) and pathconf (path) both report PATH_MAX. Exact limits vary by
	// platform, so assert the POSIX minimum only (a fixed constant on every target;
	// NAME_MAX is derived from live volume info on Windows and is not stable to diff).
	fd = open(f, O_RDONLY, 0);
	long fpm = fpathconf(fd, _PC_PATH_MAX);
	close(fd);
	long ppm = pathconf(f, _PC_PATH_MAX);
	printf("fpathconf PATH_MAX>=256=%d pathconf PATH_MAX>=256=%d\n", fpm >= 256 ? 1 : 0,
			ppm >= 256 ? 1 : 0);

	// utimensat sets a specific mtime; stat must read the same value back
	struct timespec ts[2];
	ts[0].tv_sec = 1'000'000'000;
	ts[0].tv_nsec = 0;
	ts[1].tv_sec = 1'234'567'890;
	ts[1].tv_nsec = 0;
	int ur = utimensat(AT_FDCWD, f, ts, 0);
	struct stat st;
	int sr = stat(f, &st);
	printf("utimensat=%d stat=%d mtime_ok=%d\n", ur, sr,
			(sr == 0 && st.st_mtim.tv_sec == 1'234'567'890) ? 1 : 0);

	// fdopendir: adopt an fd opened on a directory, count its (non-dot) entries
	const char *dir = "sprt_libc_fsx_dir";
	mkdir(dir, 0755);
	int a = open("sprt_libc_fsx_dir/a", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	close(a);
	int b = open("sprt_libc_fsx_dir/b", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	close(b);
	int dfd = open(dir, O_RDONLY | O_DIRECTORY);
	DIR *d = fdopendir(dfd);
	int entries = 0;
	if (d) {
		struct dirent *e;
		while ((e = readdir(d)) != nullptr) {
			if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
				++entries;
			}
		}
		closedir(d); // also closes dfd (fdopendir ownership)
	}
	printf("fdopendir ok=%d entries=%d\n", d ? 1 : 0, entries);

	// cleanup
	unlink("sprt_libc_fsx_dir/a");
	unlink("sprt_libc_fsx_dir/b");
	rmdir(dir);
	unlink(f);
}

} // namespace sprt::test
