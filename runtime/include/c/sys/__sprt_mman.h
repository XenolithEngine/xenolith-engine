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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_MMAN_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_MMAN_H_

#include <c/bits/__sprt_ssize_t.h>
#include <c/bits/__sprt_size_t.h>

#define __SPRT_MAP_FAILED ((void *) -1)

#define __SPRT_MAP_SHARED     0x01
#define __SPRT_MAP_PRIVATE    0x02
#define __SPRT_MAP_SHARED_VALIDATE 0x03
#define __SPRT_MAP_TYPE       0x0f
#define __SPRT_MAP_FIXED      0x10
#define __SPRT_MAP_ANON       0x20
#define __SPRT_MAP_ANONYMOUS  __SPRT_MAP_ANON
#define __SPRT_MAP_NORESERVE  0x4000
#define __SPRT_MAP_GROWSDOWN  0x0100
#define __SPRT_MAP_DENYWRITE  0x0800
#define __SPRT_MAP_EXECUTABLE 0x1000
#define __SPRT_MAP_LOCKED     0x2000
#define __SPRT_MAP_POPULATE   0x8000
#define __SPRT_MAP_NONBLOCK   0x1'0000
#define __SPRT_MAP_STACK      0x2'0000
#define __SPRT_MAP_HUGETLB    0x4'0000
#define __SPRT_MAP_SYNC       0x8'0000
#define __SPRT_MAP_FIXED_NOREPLACE 0x10'0000
#define __SPRT_MAP_FILE       0

#define __SPRT_MAP_HUGE_SHIFT 26
#define __SPRT_MAP_HUGE_MASK  0x3f
#define __SPRT_MAP_HUGE_16KB  (14 << 26)
#define __SPRT_MAP_HUGE_64KB  (16 << 26)
#define __SPRT_MAP_HUGE_512KB (19 << 26)
#define __SPRT_MAP_HUGE_1MB   (20 << 26)
#define __SPRT_MAP_HUGE_2MB   (21 << 26)
#define __SPRT_MAP_HUGE_8MB   (23 << 26)
#define __SPRT_MAP_HUGE_16MB  (24 << 26)
#define __SPRT_MAP_HUGE_32MB  (25 << 26)
#define __SPRT_MAP_HUGE_256MB (28 << 26)
#define __SPRT_MAP_HUGE_512MB (29 << 26)
#define __SPRT_MAP_HUGE_1GB   (30 << 26)
#define __SPRT_MAP_HUGE_2GB   (31 << 26)
#define __SPRT_MAP_HUGE_16GB  (34U << 26)

#define __SPRT_PROT_NONE      0
#define __SPRT_PROT_READ      1
#define __SPRT_PROT_WRITE     2
#define __SPRT_PROT_EXEC      4
#define __SPRT_PROT_GROWSDOWN 0x0100'0000
#define __SPRT_PROT_GROWSUP   0x0200'0000

#define __SPRT_MS_ASYNC       1
#define __SPRT_MS_INVALIDATE  2
#define __SPRT_MS_SYNC        4

#define __SPRT_MCL_CURRENT    1
#define __SPRT_MCL_FUTURE     2
#define __SPRT_MCL_ONFAULT    4

#define __SPRT_POSIX_MADV_NORMAL     0
#define __SPRT_POSIX_MADV_RANDOM     1
#define __SPRT_POSIX_MADV_SEQUENTIAL 2
#define __SPRT_POSIX_MADV_WILLNEED   3
#define __SPRT_POSIX_MADV_DONTNEED   4

#define __SPRT_MADV_NORMAL      0
#define __SPRT_MADV_RANDOM      1
#define __SPRT_MADV_SEQUENTIAL  2
#define __SPRT_MADV_WILLNEED    3
#define __SPRT_MADV_DONTNEED    4
#define __SPRT_MADV_FREE        8
#define __SPRT_MADV_REMOVE      9
#define __SPRT_MADV_DONTFORK    10
#define __SPRT_MADV_DOFORK      11
#define __SPRT_MADV_MERGEABLE   12
#define __SPRT_MADV_UNMERGEABLE 13
#define __SPRT_MADV_HUGEPAGE    14
#define __SPRT_MADV_NOHUGEPAGE  15
#define __SPRT_MADV_DONTDUMP    16
#define __SPRT_MADV_DODUMP      17
#define __SPRT_MADV_WIPEONFORK  18
#define __SPRT_MADV_KEEPONFORK  19
#define __SPRT_MADV_COLD        20
#define __SPRT_MADV_PAGEOUT     21
#define __SPRT_MADV_HWPOISON    100
#define __SPRT_MADV_SOFT_OFFLINE 101

#define __SPRT_MREMAP_MAYMOVE 1
#define __SPRT_MREMAP_FIXED 2
#define __SPRT_MREMAP_DONTUNMAP 4

#define __SPRT_MLOCK_ONFAULT 0x01

__SPRT_BEGIN_DECL

SPRT_API void *__SPRT_ID(mmap)(void *__addr, __SPRT_ID(size_t) __size, int __prot, int __flags,
		int __fd, __SPRT_ID(off_t) __offset);

SPRT_API int __SPRT_ID(munmap)(void *, __SPRT_ID(size_t));

SPRT_API int __SPRT_ID(mprotect)(void *, __SPRT_ID(size_t), int);
SPRT_API int __SPRT_ID(msync)(void *, __SPRT_ID(size_t), int);

SPRT_API int __SPRT_ID(posix_madvise)(void *, __SPRT_ID(size_t), int);

SPRT_API int __SPRT_ID(mlock)(const void *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(munlock)(const void *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(mlockall)(int);
SPRT_API int __SPRT_ID(munlockall)(void);

SPRT_API void *__SPRT_ID(mremap)(void *, __SPRT_ID(size_t), __SPRT_ID(size_t), int, ...);
SPRT_API int __SPRT_ID(mlock2)(const void *, __SPRT_ID(size_t), int);

SPRT_API int __SPRT_ID(madvise)(void *, __SPRT_ID(size_t), int);
SPRT_API int __SPRT_ID(mincore)(void *, __SPRT_ID(size_t), unsigned char *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_MMAN_H_
