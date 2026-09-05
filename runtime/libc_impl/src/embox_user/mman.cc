
// Embox EL0 memory-management backend.
//
// mmap/munmap live in the generic builtin_mman.cpp over __file_mmap_anon (see
// libc_file_ops.cc); what is left here is the protection/advice/residency family
// plus the includes that body needs.
//
// The answers below are not stubs picked for convenience. An EL0 address space
// on Embox has no paging, no swap and no page cache: every mapped page is
// resident from the moment it is mapped until it is unmapped. That makes the
// locking calls truthful successes and the advisory calls truthful no-ops -- but
// it does NOT make mincore answerable, and it does not make msync a promise
// anything here can keep.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>

#include "../../include/__impl_libc.h"

#include "../../../core/include/__el0_syscall.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

// The one real syscall in this file. PROT_* cross the boundary as the ABI's
// values (asm-generic), which is what xl_mm.c reads.
__SPRT_C_FUNC int mprotect(void *addr, size_t len, int prot) __SPRT_NOEXCEPT {
	return (int)__el0_ret(__el0_mprotect(addr, len, prot));
}

// madvise is advisory by definition: an implementation is free to ignore every
// hint and still be conforming. Reporting success for advice nobody acts on is
// the standard answer, not a lie -- unlike msync, where success means data was
// written.
__SPRT_C_FUNC int madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT {
	(void)addr;
	(void)len;
	(void)advice;
	return 0;
}

__SPRT_C_FUNC int posix_madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT {
	(void)addr;
	(void)len;
	(void)advice;
	return 0;
}

// Nothing is ever evicted, so every page in the address space is already locked
// in the sense mlock asks about.
__SPRT_C_FUNC int mlock(const void *addr, size_t len) __SPRT_NOEXCEPT {
	(void)addr;
	(void)len;
	return 0;
}

__SPRT_C_FUNC int munlock(const void *addr, size_t len) __SPRT_NOEXCEPT {
	(void)addr;
	(void)len;
	return 0;
}

__SPRT_C_FUNC int mlock2(const void *addr, size_t len, int flags) __SPRT_NOEXCEPT {
	(void)addr;
	(void)len;
	(void)flags;
	return 0;
}

// Residency cannot be queried -- there is no syscall for it and no notion of a
// page being absent. Filling `vec` with ones would be defensible and is what
// makes this worth a comment: it would also be indistinguishable from a real
// answer, so a caller could never tell it was never asked.
__SPRT_C_FUNC int mincore(void *addr, size_t length, unsigned char *vec) __SPRT_NOEXCEPT {
	(void)addr;
	(void)length;
	(void)vec;
	__sprt_errno = ENOSYS;
	return -1;
}

} // namespace sprt
