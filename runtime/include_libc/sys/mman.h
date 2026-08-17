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

/*
	Dispatch header for the POSIX <sys/mman.h> (memory mapping):
	- hosted SPRT build -> forwards to the system <sys/mman.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them).

	Macros:
	  MAP_FAILED      - mmap failure sentinel return value
	  MAP_* flags     - MAP_SHARED, MAP_PRIVATE, MAP_SHARED_VALIDATE, MAP_TYPE,
	                    MAP_FIXED, MAP_ANON(YMOUS), MAP_NORESERVE, MAP_GROWSDOWN,
	                    MAP_DENYWRITE, MAP_EXECUTABLE, MAP_LOCKED, MAP_POPULATE,
	                    MAP_NONBLOCK, MAP_STACK, MAP_HUGETLB, MAP_SYNC,
	                    MAP_FIXED_NOREPLACE, MAP_FILE
	  MAP_HUGE_*      - MAP_HUGE_SHIFT/MAP_HUGE_MASK and the per-size selectors
	                    MAP_HUGE_16KB .. MAP_HUGE_16GB
	  PROT_*          - PROT_NONE, PROT_READ, PROT_WRITE, PROT_EXEC, PROT_GROWSDOWN,
	                    PROT_GROWSUP (page-protection bits)
	  MS_*            - MS_ASYNC, MS_INVALIDATE, MS_SYNC (msync flags)
	  MCL_*           - MCL_CURRENT, MCL_FUTURE, MCL_ONFAULT (mlockall flags)
	  POSIX_MADV_*    - portable madvise advice values
	  MADV_*          - the full Linux madvise advice set
	  MREMAP_*        - MREMAP_MAYMOVE, MREMAP_FIXED, MREMAP_DONTUNMAP
	  MLOCK_ONFAULT   - mlock2 flag
	  MFD_*           - MFD_CLOEXEC, MFD_ALLOW_SEALING, MFD_HUGETLB (memfd_create)
	  mmap64 -> mmap  - LFS alias

	Types:
	  size_t, off_t

	Always-available functions:
	  mmap          - map files or anonymous memory into the address space
	  munmap        - remove a mapping
	  mprotect      - change the protection of a mapped range
	  msync         - flush a mapping to its backing store
	  madvise       - give the kernel advice about a range (Linux)
	  posix_madvise - portable form of madvise
	  mlock/munlock - lock/unlock pages into RAM
	  mlock2        - lock pages with flags (e.g. MLOCK_ONFAULT)
	  mincore       - report which pages of a range are resident

	Gated functions:
	  mlockall/munlockall - lock/unlock the whole address space [gate: MMAN_MLOCKALL]
	  mremap              - resize/move a mapping (variadic for MREMAP_FIXED)
	                        [gate: MMAN_MREMAP]
	  memfd_create        - create an anonymous file in memory [gate: MMAN_MEMFD]
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/mman.h>

#else

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_stdarg.h>

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
#ifdef __SPRT_MADV_FREE // absent on NuttX
#define MADV_FREE __SPRT_MADV_FREE
#endif
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

#define MFD_CLOEXEC __SPRT_MFD_CLOEXEC
#define MFD_ALLOW_SEALING __SPRT_MFD_ALLOW_SEALING
#define MFD_HUGETLB __SPRT_MFD_HUGETLB

#define mmap64 mmap

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;

SPRT_UMBRELLA_FUNC
void *mmap(void *__addr, size_t __size, int __prot, int __flags, int __fd,
		off_t __offset) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mmap(__addr, __size, __prot, __flags, __fd, __offset);
}
#endif

SPRT_UMBRELLA_FUNC
int munmap(void *__addr, size_t __size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_munmap(__addr, __size);
}
#endif

SPRT_UMBRELLA_FUNC
int mprotect(void *__addr, size_t __size, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mprotect(__addr, __size, __flags);
}
#endif

SPRT_UMBRELLA_FUNC
int msync(void *__addr, size_t __size, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_msync(__addr, __size, __flags);
}
#endif

SPRT_UMBRELLA_FUNC
int posix_madvise(void *__addr, size_t __size, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_posix_madvise(__addr, __size, __flags);
}
#endif

SPRT_UMBRELLA_FUNC
int mlock(const void *__addr, size_t __size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mlock(__addr, __size);
}
#endif

SPRT_UMBRELLA_FUNC
int munlock(const void *__addr, size_t __size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_munlock(__addr, __size);
}
#endif

#if __SPRT_CONFIG_HAVE_MMAN_MLOCKALL || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int mlockall(int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mlockall(__flags);
}
#endif

SPRT_UMBRELLA_FUNC
int munlockall(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_munlockall();
}
#endif
#endif // __SPRT_CONFIG_HAVE_MMAN_MLOCKALL

#if __SPRT_CONFIG_HAVE_MMAN_MREMAP || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
void *mremap(void *__addr, size_t __old_size, size_t __new_size, int __flags, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list ap;
	void *new_addr = 0;
	if (__flags & MREMAP_FIXED) {
		__sprt_va_start(ap, __flags);
		new_addr = __sprt_va_arg(ap, void *);
		__sprt_va_end(ap);
	}

	return __sprt_mremap(__addr, __old_size, __new_size, __flags, new_addr);
}
#endif
#endif // __SPRT_CONFIG_HAVE_MMAN_MREMAP

SPRT_UMBRELLA_FUNC
int mlock2(const void *__addr, size_t __size, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mlock2(__addr, __size, __flags);
}
#endif

SPRT_UMBRELLA_FUNC
int madvise(void *__addr, size_t __size, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_madvise(__addr, __size, __flags);
}
#endif

SPRT_UMBRELLA_FUNC
int mincore(void *__addr, size_t __size, unsigned char *__vec) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mincore(__addr, __size, __vec);
}
#endif

#if __SPRT_CONFIG_HAVE_MMAN_MEMFD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int memfd_create(const char *name, unsigned flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memfd_create(name, flags);
}
#endif
#endif // __SPRT_CONFIG_HAVE_MMAN_MEMFD

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_MMAN_H_
