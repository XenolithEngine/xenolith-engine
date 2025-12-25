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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FSTYPES_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FSTYPES_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_uint32_t.h>
#include <c/bits/__sprt_uint64_t.h>
#include <c/bits/__sprt_int64_t.h>

/* WARNING!

 We use POSIX constants, not platform libc's constants

 Implementation should convert them into native ones for non-POSIX platforms!
*/

// clang-format off
typedef __SPRT_ID(uint32_t) __SPRT_ID(mode_t);
typedef __SPRT_ID(uint64_t) __SPRT_ID(nlink_t);
typedef __SPRT_ID(uint64_t) __SPRT_ID(ino_t);
typedef __SPRT_ID(uint64_t) __SPRT_ID(dev_t);
typedef __SPRT_ID(int64_t) __SPRT_ID(blksize_t);
typedef __SPRT_ID(int64_t) __SPRT_ID(blkcnt_t);

#define __SPRT_S_IFMT  0170000

#define __SPRT_S_IFDIR 0040000
#define __SPRT_S_IFCHR 0020000
#define __SPRT_S_IFBLK 0060000
#define __SPRT_S_IFREG 0100000
#define __SPRT_S_IFIFO 0010000
#define __SPRT_S_IFLNK 0120000
#define __SPRT_S_IFSOCK 0140000

#define __SPRT_S_TYPEISMQ(buf)  0
#define __SPRT_S_TYPEISSEM(buf) 0
#define __SPRT_S_TYPEISSHM(buf) 0
#define __SPRT_S_TYPEISTMO(buf) 0

#define __SPRT_S_ISDIR(mode)  (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFDIR)
#define __SPRT_S_ISCHR(mode)  (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFCHR)
#define __SPRT_S_ISBLK(mode)  (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFBLK)
#define __SPRT_S_ISREG(mode)  (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFREG)
#define __SPRT_S_ISFIFO(mode) (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFIFO)
#define __SPRT_S_ISLNK(mode)  (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFLNK)
#define __SPRT_S_ISSOCK(mode) (((mode) & __SPRT_S_IFMT) == __SPRT_S_IFSOCK)

#define __SPRT_S_ISUID 04000
#define __SPRT_S_ISGID 02000
#define __SPRT_S_ISVTX 01000
#define __SPRT_S_IRUSR 0400
#define __SPRT_S_IWUSR 0200
#define __SPRT_S_IXUSR 0100
#define __SPRT_S_IRWXU 0700
#define __SPRT_S_IRGRP 0040
#define __SPRT_S_IWGRP 0020
#define __SPRT_S_IXGRP 0010
#define __SPRT_S_IRWXG 0070
#define __SPRT_S_IROTH 0004
#define __SPRT_S_IWOTH 0002
#define __SPRT_S_IXOTH 0001
#define __SPRT_S_IRWXO 0007

#define __SPRT_S_IREAD __SPRT_S_IRUSR
#define __SPRT_S_IWRITE __SPRT_S_IWUSR
#define __SPRT_S_IEXEC __SPRT_S_IXUSR

#define __SPRT_UTIME_NOW  0x3fffffff
#define __SPRT_UTIME_OMIT 0x3ffffffe
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FSID_H_
