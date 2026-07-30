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

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

// ssize_t/off_t are pointer-width (64-bit) on both targets, but print them via
// (long long)/%lld so the format is width-stable regardless.
static long long ll(long long v) { return v; }

// Raw descriptor I/O via <unistd.h> + <fcntl.h>, relative path. The freestanding
// Windows libc_impl accepts the same relative POSIX path, so every byte count,
// offset, content snapshot and errno name matches the host.
void performUnistdTest() {
	const char *path = "sprt_libc_unistd.tmp";
	unlink(path);

	// open(O_WRONLY|O_CREAT|O_TRUNC) + write + lseek(CUR)
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	printf("open(wronly,creat)=%s\n", fd >= 0 ? "ok" : "fail");
	const char data[] = "0123456789";
	long long wn = write(fd, data, 10);
	printf("write=%lld pos=%lld\n", ll(wn), ll(lseek(fd, 0, SEEK_CUR)));
	printf("close=%d\n", close(fd));

	// open(O_RDONLY) + read + the three lseek whences
	fd = open(path, O_RDONLY, 0);
	char buf[32];
	memset(buf, 0, sizeof(buf));
	long long rn = read(fd, buf, 5);
	printf("read(5)=%lld [%.*s] pos=%lld\n", ll(rn), (int)rn, buf, ll(lseek(fd, 0, SEEK_CUR)));
	printf("lseek SET 2=%lld\n", ll(lseek(fd, 2, SEEK_SET)));
	printf("lseek END 0=%lld\n", ll(lseek(fd, 0, SEEK_END)));
	printf("lseek CUR -3=%lld\n", ll(lseek(fd, -3, SEEK_CUR)));
	// pread reads at an explicit offset and leaves the file offset untouched
	off_t before = lseek(fd, 4, SEEK_SET);
	memset(buf, 0, sizeof(buf));
	rn = pread(fd, buf, 4, 1);
	printf("pread(4,@1)=%lld [%.*s] pos-unchanged=%d\n", ll(rn), (int)rn, buf,
			lseek(fd, 0, SEEK_CUR) == before ? 1 : 0);
	close(fd);

	// pwrite at an explicit offset, then read the result back
	fd = open(path, O_RDWR, 0);
	printf("pwrite(XY,@3)=%lld\n", ll(pwrite(fd, "XY", 2, 3)));
	memset(buf, 0, sizeof(buf));
	pread(fd, buf, 10, 0);
	printf("after pwrite: [%.*s]\n", 10, buf);
	close(fd);

	// dup / dup2 share the open file description (and its offset)
	fd = open(path, O_RDONLY, 0);
	int fd2 = dup(fd);
	printf("dup ok=%d distinct=%d\n", fd2 >= 0 ? 1 : 0, fd2 != fd ? 1 : 0);
	read(fd, buf, 2);
	printf("dup shares offset=%lld\n", ll(lseek(fd2, 0, SEEK_CUR)));
	// POSIX: dup() clears FD_CLOEXEC on the new descriptor (== fcntl F_DUPFD);
	// both targets must report 0. Guards libc_impl against regressing to a
	// close-on-exec default for dup().
	printf("dup fd_cloexec=%d\n", (fcntl(fd2, F_GETFD) & FD_CLOEXEC) != 0);
	close(fd2);
	int fd3 = open(path, O_RDONLY, 0);
	int dd = dup2(fd, fd3); // fd3 now refers to the same description as fd
	printf("dup2 returns target=%d\n", dd == fd3 ? 1 : 0);
	close(fd3);
	close(fd);

	// ftruncate shrinks the file. Sequence the truncate before the size probe:
	// printf argument evaluation order is unspecified (and differs by ABI).
	fd = open(path, O_RDWR, 0);
	int tr = ftruncate(fd, 4);
	long long sz = lseek(fd, 0, SEEK_END);
	printf("ftruncate(4)=%d size=%lld\n", tr, sz);
	close(fd);

	// access: existing vs missing (read errno after the call, not inside printf)
	printf("access(F_OK)=%d\n", access(path, F_OK));
	errno = 0;
	int am = access("sprt_libc_nope.tmp", F_OK);
	int ae = errno;
	printf("access(missing)=%d errno=%s\n", am, errnoName(ae));

	// open of a missing file for reading fails with ENOENT
	errno = 0;
	int bad = open("sprt_libc_nope.tmp", O_RDONLY, 0);
	printf("open(missing)=%d errno=%s\n", bad >= 0 ? bad : -1, errnoName(errno));
	if (bad >= 0) {
		close(bad);
	}

	// O_CREAT|O_EXCL refuses to reopen an existing file
	unlink(path);
	int e1 = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	errno = 0;
	int e2 = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	printf("O_EXCL fresh=%d existing=%d errno=%s\n", e1 >= 0 ? 1 : 0, e2 >= 0 ? 1 : 0,
			errnoName(errno));
	if (e1 >= 0) {
		close(e1);
	}
	if (e2 >= 0) {
		close(e2);
	}

	// read from / write to the wrong-mode descriptor reports EBADF
	fd = open(path, O_RDONLY, 0);
	errno = 0;
	long long bw = write(fd, "x", 1);
	int bwe = errno;
	printf("write(rdonly fd)=%lld errno=%s\n", ll(bw), errnoName(bwe));
	close(fd);
	fd = open(path, O_WRONLY, 0);
	errno = 0;
	char one[1];
	long long br = read(fd, one, 1);
	int bre = errno;
	printf("read(wronly fd)=%lld errno=%s\n", ll(br), errnoName(bre));
	close(fd);

	// unlink removes it; a second unlink fails with ENOENT
	printf("unlink=%d\n", unlink(path));
	errno = 0;
	int ua = unlink(path);
	int ue = errno;
	printf("unlink(again)=%d errno=%s\n", ua, errnoName(ue));

	/*
		The standard descriptors are writable.

		Worth its own check because nothing else here reaches them: stdio drives the
		console handle directly, so printf can keep working while raw write() to the
		very same descriptor fails. That is not hypothetical - the freestanding
		Windows libc left fd 0/1/2 at flags 0 (which is O_RDONLY), so every write to
		stdout returned EBADF while this suite stayed green, and any consumer that
		writes through a descriptor rather than a FILE* (LLVM's raw_fd_ostream, for
		one) silently produced nothing at all.

		stdout is flushed on both sides of the raw write so the interleaving is
		determined here rather than by whatever buffering policy each libc applies.
	*/
	fflush(stdout);
	errno = 0;
	long long dw = ll(write(STDOUT_FILENO, "write(stdout) ok\n", 17));
	int dwe = errno;
	errno = 0;
	long long ew = ll(write(STDERR_FILENO, "", 0));
	int ewe = errno;
	fflush(stdout);
	printf("write(stdout)=%lld errno=%s\n", dw, errnoName(dwe));
	printf("write(stderr,0)=%lld errno=%s\n", ew, errnoName(ewe));

	// ...and stdin is not. POSIX reports EBADF for a write to a read-only descriptor.
	errno = 0;
	long long iw = ll(write(STDIN_FILENO, "x", 1));
	int iwe = errno;
	printf("write(stdin)=%lld errno=%s\n", iw, errnoName(iwe));
}

} // namespace sprt::test
