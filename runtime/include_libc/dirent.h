/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_

/*
	Dispatch header for the POSIX <dirent.h> (directory traversal):
	- hosted SPRT build -> forwards to the system <dirent.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	struct dirent comes in via <sprt/c/__sprt_dirent.h>.

	Macros:
	  d_type values: DT_UNKNOWN, DT_FIFO, DT_CHR, DT_DIR, DT_BLK, DT_REG, DT_LNK,
	                 DT_SOCK, DT_WHT
	  LFS aliases:   dirent64->dirent, readdir64->readdir, alphasort64->alphasort,
	                 scandir64->scandir, scandirat64->scandirat

	Types:
	  DIR     - opaque directory-stream handle
	  ssize_t, off_t

	Functions:
	  opendir    - open a directory stream by path
	  fdopendir  - open a directory stream from an existing descriptor
	  readdir    - read the next entry from a directory stream
	  closedir   - close a directory stream
	  rewinddir  - reset a stream to its first entry
	  seekdir    - set a stream's position (to a value from telldir)
	  telldir    - report a stream's current position
	  dirfd      - get the descriptor behind a directory stream
	  scandir    - read a directory into an array, with a filter and comparator
	  scandirat  - scandir relative to a directory descriptor
	  alphasort  - comparator for scandir that orders entries alphabetically
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <dirent.h>

#else

#include <sprt/c/__sprt_dirent.h>

#define DT_UNKNOWN __SPRT_DT_UNKNOWN
#define DT_FIFO __SPRT_DT_FIFO
#define DT_CHR __SPRT_DT_CHR
#define DT_DIR __SPRT_DT_DIR
#define DT_BLK __SPRT_DT_BLK
#define DT_REG __SPRT_DT_REG
#define DT_LNK __SPRT_DT_LNK
#define DT_SOCK __SPRT_DT_SOCK
#define DT_WHT __SPRT_DT_WHT

#define dirent64 dirent
#define readdir64 readdir
#define alphasort64 alphasort
#define scandir64 scandir
#define scandirat64 scandirat

typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(DIR) DIR;

#include <sprt/gen/c/umbrella/dirent.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_
