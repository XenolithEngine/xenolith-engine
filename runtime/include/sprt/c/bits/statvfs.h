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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS_STATVFS_H_
#define CORE_RUNTIME_INCLUDE_C_BITS_STATVFS_H_

#include <sprt/c/cross/__sprt_config.h>

// Like struct stat (bits/stat.h): under __SPRT_BUILD the struct is the mangled
// __sprt_statvfs so a wrapper TU can hold both it and the native struct statvfs
// side by side; otherwise it is the plain POSIX name the public umbrella exposes.
#ifdef __SPRT_BUILD
#define __SPRT_STATVFS_NAME __SPRT_ID(statvfs)
#else
#define __SPRT_STATVFS_NAME statvfs
#endif

// POSIX fsblkcnt_t/fsfilcnt_t. sprt has no dedicated cross typedef for these; on
// every LP64 target sprt supports they are unsigned long (matches glibc/musl). The
// wrapper converts field-by-field, so exact width parity with the native struct is
// not required for correctness.
typedef unsigned long __SPRT_ID(fsblkcnt_t);
typedef unsigned long __SPRT_ID(fsfilcnt_t);

struct __SPRT_STATVFS_NAME {
	unsigned long f_bsize;             /* filesystem block size */
	unsigned long f_frsize;            /* fragment size */
	__SPRT_ID(fsblkcnt_t) f_blocks;    /* size of fs in f_frsize units */
	__SPRT_ID(fsblkcnt_t) f_bfree;     /* free blocks */
	__SPRT_ID(fsblkcnt_t) f_bavail;    /* free blocks for unprivileged users */
	__SPRT_ID(fsfilcnt_t) f_files;     /* inodes */
	__SPRT_ID(fsfilcnt_t) f_ffree;     /* free inodes */
	__SPRT_ID(fsfilcnt_t) f_favail;    /* free inodes for unprivileged users */
	unsigned long f_fsid;              /* filesystem ID */
	unsigned long f_flag;              /* mount flags (ST_*) */
	unsigned long f_namemax;           /* maximum filename length */
};

#endif // CORE_RUNTIME_INCLUDE_C_BITS_STATVFS_H_
