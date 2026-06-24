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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "test_util.h"

namespace sprt::test {

// Streamed FILE* I/O. All files are opened in binary mode ("...b") and use a
// relative path, so the byte streams — and therefore every printed result — are
// identical on Linux and on the freestanding Windows libc_impl (which accepts
// the same relative POSIX path). Each test leaves no file behind.
void performStdioFileTest() {
	const char *path = "sprt_libc_stdio.tmp";
	remove(path); // clean slate regardless of a previous run

	// fopen "wb" + fwrite + ftell + fclose
	FILE *f = fopen(path, "wb");
	printf("fopen(wb)=%s\n", f ? "ok" : "null");
	const char text[] = "Hello, stdio!\nsecond line\n";
	size_t textLen = sizeof(text) - 1;
	size_t wn = fwrite(text, 1, textLen, f);
	printf("fwrite=%zu ftell=%ld\n", wn, ftell(f));
	printf("fclose=%d\n", fclose(f));

	// fopen "rb" + fread the whole file back
	f = fopen(path, "rb");
	char buf[64];
	memset(buf, 0, sizeof(buf));
	size_t rn = fread(buf, 1, sizeof(buf) - 1, f);
	printf("fread=%zu content=[%s]\n", rn, buf);
	printf("feof(after full read)=%d\n", feof(f) ? 1 : 0);
	// a further read on an exhausted stream returns 0 and sets EOF
	printf("fgetc(at eof)=%d feof=%d\n", fgetc(f), feof(f) ? 1 : 0);

	// fseek / ftell with the three whences. ftell and the following fgetc must be
	// sequenced into separate statements: they both touch the stream position and
	// the order of evaluation of printf's arguments is unspecified (and in fact
	// differs between the SysV and Windows x64 ABIs).
	long pos;
	int ch;
	fseek(f, 7, SEEK_SET);
	pos = ftell(f);
	ch = fgetc(f);
	printf("seek SET 7: ftell=%ld fgetc=%c\n", pos, ch);
	fseek(f, -1, SEEK_END);
	pos = ftell(f);
	ch = fgetc(f);
	printf("seek END -1: ftell=%ld fgetc=%c\n", pos, ch);
	fseek(f, 0, SEEK_SET);
	fseek(f, 2, SEEK_CUR);
	pos = ftell(f);
	ch = fgetc(f);
	printf("seek CUR +2: ftell=%ld fgetc=%c\n", pos, ch);
	rewind(f);
	printf("rewind: ftell=%ld\n", ftell(f));

	// fgetpos / fsetpos round-trip: save a position, read past it, then restore.
	fseek(f, 5, SEEK_SET);
	fpos_t fp;
	int gp = fgetpos(f, &fp);
	fgetc(f);
	fgetc(f);
	int sp = fsetpos(f, &fp);
	long fposTell = ftell(f);
	int fposC = fgetc(f);
	printf("fgetpos=%d fsetpos=%d ftell=%ld c=%c\n", gp, sp, fposTell, fposC);

	// ungetc pushes a byte back
	rewind(f);
	int c0 = fgetc(f);
	int pushed = ungetc(c0, f);
	printf("fgetc=%c ungetc=%d next=%c\n", c0, pushed, fgetc(f));

	// fgets reads line by line, keeping the newline
	rewind(f);
	char line[32];
	printf("fgets1=[%s]\n", fgets(line, sizeof(line), f) ? line : "(null)");
	printf("fgets2=[%s]\n", fgets(line, sizeof(line), f) ? line : "(null)");
	printf("fgets3=%s\n", fgets(line, sizeof(line), f) ? "line" : "(null)");
	fclose(f);

	// Append mode extends the file
	f = fopen(path, "ab");
	fputs("third\n", f);
	fputc('!', f);
	long apos = ftell(f);
	int acl = fclose(f); // sequence ftell before fclose (printf arg order is unspecified)
	printf("append ftell=%ld fclose=%d\n", apos, acl);
	f = fopen(path, "rb");
	memset(buf, 0, sizeof(buf));
	rn = fread(buf, 1, sizeof(buf) - 1, f);
	printf("after append: fread=%zu content=[%s]\n", rn, buf);
	fclose(f);

	// fprintf into a file, fscanf back out
	f = fopen(path, "wb");
	int pn = fprintf(f, "%d %s %.3f", -42, "token", 2.5);
	printf("fprintf=%d fclose=%d\n", pn, fclose(f));
	f = fopen(path, "rb");
	int iv = 0;
	char sv[16] = {0};
	double dv = 0;
	int sn = fscanf(f, "%d %15s %lf", &iv, sv, &dv);
	printf("fscanf=%d -> %d [%s] %.3f\n", sn, iv, sv, dv);
	fclose(f);

	// setvbuf (full buffering) then write/read round-trip
	f = fopen(path, "wb");
	char vbuf[64];
	printf("setvbuf=%d\n", setvbuf(f, vbuf, _IOFBF, sizeof(vbuf)));
	fwrite("buffered", 1, 8, f);
	fflush(f);
	fclose(f);
	f = fopen(path, "rb");
	memset(buf, 0, sizeof(buf));
	rn = fread(buf, 1, sizeof(buf) - 1, f);
	printf("buffered round-trip: fread=%zu [%s]\n", rn, buf);
	// fileno returns a valid (>=0) descriptor
	printf("fileno>=0=%d\n", fileno(f) >= 0 ? 1 : 0);
	fclose(f);

	// fopen of a missing file for reading fails with ENOENT
	errno = 0;
	FILE *nf = fopen("sprt_libc_missing_xyz.tmp", "rb");
	printf("fopen(missing,rb)=%s errno=%s\n", nf ? "ok" : "null", errnoName(errno));
	if (nf) {
		fclose(nf);
	}

	// remove deletes the file; removing again fails with ENOENT
	printf("remove=%d\n", remove(path));
	errno = 0;
	int rr = remove(path);
	printf("remove(again)=%d errno=%s\n", rr, errnoName(errno));
}

} // namespace sprt::test
