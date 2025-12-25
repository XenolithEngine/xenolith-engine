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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_MMAN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_MMAN_H_

#ifdef __SPRT_BUILD

#include_next <sys/mman.h>

#else

#include <c/sys/__sprt_mman.h>
#include <c/__sprt_stdarg.h>

#define MAP_FAILED __SPRT_MAP_FAILED

#define MAP_SHARED __SPRT_MAP_SHARED
#define MAP_PRIVATE __SPRT_MAP_PRIVATE
#define MAP_SHARED_VALIDATE __SPRT_MAP_SHARED_VALIDATE
#define MAP_TYPE __SPRT_MAP_TYPE
#define MAP_FIXED __SPRT_MAP_FIXED
#define MAP_ANON __SPRT_MAP_ANON
#define MAP_ANONYMOUS __SPRT_MAP_ANONYMOUS
#define MAP_NORESERVE __SPRT_MAP_NORESERVE
#define MAP_GROWSDOWN __SPRT_MAP_GROWSDOWN
#define MAP_DENYWRITE __SPRT_MAP_DENYWRITE
#define MAP_EXECUTABLE __SPRT_MAP_EXECUTABLE
#define MAP_LOCKED __SPRT_MAP_LOCKED
#define MAP_POPULATE __SPRT_MAP_POPULATE
#define MAP_NONBLOCK __SPRT_MAP_NONBLOCK
#define MAP_STACK __SPRT_MAP_STACK
#define MAP_HUGETLB __SPRT_MAP_HUGETLB
#define MAP_SYNC __SPRT_MAP_SYNC
#define MAP_FIXED_NOREPLACE __SPRT_MAP_FIXED_NOREPLACE
#define MAP_FILE __SPRT_MAP_FILE

#define MAP_HUGE_SHIFT __SPRT_MAP_HUGE_SHIFT
#define MAP_HUGE_MASK __SPRT_MAP_HUGE_MASK
#define MAP_HUGE_16KB __SPRT_MAP_HUGE_16KB
#define MAP_HUGE_64KB __SPRT_MAP_HUGE_64KB
#define MAP_HUGE_512KB __SPRT_MAP_HUGE_512KB
#define MAP_HUGE_1MB __SPRT_MAP_HUGE_1MB
#define MAP_HUGE_2MB __SPRT_MAP_HUGE_2MB
#define MAP_HUGE_8MB __SPRT_MAP_HUGE_8MB
#define MAP_HUGE_16MB __SPRT_MAP_HUGE_16MB
#define MAP_HUGE_32MB __SPRT_MAP_HUGE_32MB
#define MAP_HUGE_256MB __SPRT_MAP_HUGE_256MB
#define MAP_HUGE_512MB __SPRT_MAP_HUGE_512MB
#define MAP_HUGE_1GB __SPRT_MAP_HUGE_1GB
#define MAP_HUGE_2GB __SPRT_MAP_HUGE_2GB
#define MAP_HUGE_16GB __SPRT_MAP_HUGE_16GB

#define PROT_NONE __SPRT_PROT_NONE
#define PROT_READ __SPRT_PROT_READ
#define PROT_WRITE __SPRT_PROT_WRITE
#define PROT_EXEC __SPRT_PROT_EXEC
#define PROT_GROWSDOWN __SPRT_PROT_GROWSDOWN
#define PROT_GROWSUP __SPRT_PROT_GROWSUP

#define MS_ASYNC __SPRT_MS_ASYNC
#define MS_INVALIDATE __SPRT_MS_INVALIDATE
#define MS_SYNC __SPRT_MS_SYNC

#define MCL_CURRENT __SPRT_MCL_CURRENT
#define MCL_FUTURE __SPRT_MCL_FUTURE
#define MCL_ONFAULT __SPRT_MCL_ONFAULT

#define POSIX_MADV_NORMAL __SPRT_POSIX_MADV_NORMAL
#define POSIX_MADV_RANDOM __SPRT_POSIX_MADV_RANDOM
#define POSIX_MADV_SEQUENTIAL __SPRT_POSIX_MADV_SEQUENTIAL
#define POSIX_MADV_WILLNEED __SPRT_POSIX_MADV_WILLNEED
#define POSIX_MADV_DONTNEED __SPRT_POSIX_MADV_DONTNEED

#define MADV_NORMAL __SPRT_MADV_NORMAL
#define MADV_RANDOM __SPRT_MADV_RANDOM
#define MADV_SEQUENTIAL __SPRT_MADV_SEQUENTIAL
#define MADV_WILLNEED __SPRT_MADV_WILLNEED
#define MADV_DONTNEED __SPRT_MADV_DONTNEED
#define MADV_FREE __SPRT_MADV_FREE
#define MADV_REMOVE __SPRT_MADV_REMOVE
#define MADV_DONTFORK __SPRT_MADV_DONTFORK
#define MADV_DOFORK __SPRT_MADV_DOFORK
#define MADV_MERGEABLE __SPRT_MADV_MERGEABLE
#define MADV_UNMERGEABLE __SPRT_MADV_UNMERGEABLE
#define MADV_HUGEPAGE __SPRT_MADV_HUGEPAGE
#define MADV_NOHUGEPAGE __SPRT_MADV_NOHUGEPAGE
#define MADV_DONTDUMP __SPRT_MADV_DONTDUMP
#define MADV_DODUMP __SPRT_MADV_DODUMP
#define MADV_WIPEONFORK __SPRT_MADV_WIPEONFORK
#define MADV_KEEPONFORK __SPRT_MADV_KEEPONFORK
#define MADV_COLD __SPRT_MADV_COLD
#define MADV_PAGEOUT __SPRT_MADV_PAGEOUT
#define MADV_HWPOISON __SPRT_MADV_HWPOISON
#define MADV_SOFT_OFFLINE __SPRT_MADV_SOFT_OFFLINE

#define MREMAP_MAYMOVE __SPRT_MREMAP_MAYMOVE
#define MREMAP_FIXED __SPRT_MREMAP_FIXED
#define MREMAP_DONTUNMAP __SPRT_MREMAP_DONTUNMAP

#define MLOCK_ONFAULT __SPRT_MLOCK_ONFAULT

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;

SPRT_FORCEINLINE inline void *mmap(void *__addr, size_t __size, int __prot, int __flags, int __fd,
		off_t __offset) {
	return __sprt_mmap(__addr, __size, __prot, __flags, __fd, __offset);
}

SPRT_FORCEINLINE inline int munmap(void *__addr, size_t __size) {
	return __sprt_munmap(__addr, __size);
}

SPRT_FORCEINLINE inline int mprotect(void *__addr, size_t __size, int __flags) {
	return __sprt_mprotect(__addr, __size, __flags);
}

SPRT_FORCEINLINE inline int msync(void *__addr, size_t __size, int __flags) {
	return __sprt_msync(__addr, __size, __flags);
}

SPRT_FORCEINLINE inline int posix_madvise(void *__addr, size_t __size, int __flags) {
	return __sprt_posix_madvise(__addr, __size, __flags);
}

SPRT_FORCEINLINE inline int mlock(const void *__addr, size_t __size) {
	return __sprt_mlock(__addr, __size);
}
SPRT_FORCEINLINE inline int munlock(const void *__addr, size_t __size) {
	return __sprt_munlock(__addr, __size);
}
SPRT_FORCEINLINE inline int mlockall(int __flags) { return __sprt_mlockall(__flags); }
SPRT_FORCEINLINE inline int munlockall(void) { return __sprt_munlockall(); }

SPRT_FORCEINLINE inline void *mremap(void *__addr, size_t __old_size, size_t __new_size,
		int __flags, ...) {
	__sprt_va_list ap;
	void *new_addr = 0;
	if (__flags & MREMAP_FIXED) {
		__sprt_va_start(ap, __flags);
		new_addr = __sprt_va_arg(ap, void *);
		__sprt_va_end(ap);
	}

	return __sprt_mremap(__addr, __old_size, __new_size, __flags, new_addr);
}

SPRT_FORCEINLINE inline int mlock2(const void *__addr, size_t __size, int __flags) {
	return __sprt_mlock2(__addr, __size, __flags);
}

SPRT_FORCEINLINE inline int madvise(void *__addr, size_t __size, int __flags) {
	return __sprt_madvise(__addr, __size, __flags);
}

SPRT_FORCEINLINE inline int mincore(void *__addr, size_t __size, unsigned char *__vec) {
	return __sprt_mincore(__addr, __size, __vec);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_MMAN_H_
