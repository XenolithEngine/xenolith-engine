// Memory protection on Tau: libtau changes the rights of JIT memory (MAP_JIT) page by page;
// other mappings stay readable and writable and are never executable.

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>

#include "../../include/__impl_libc.h"

extern "C" int mprotect(void *addr, __SPRT_ID(size_t) len, int prot) __SPRT_NOEXCEPT {
	int st = __sprt_tau_mem_protect(addr, len,
			((prot & __SPRT_PROT_READ) ? __SPRT_TAU_PROT_READ : 0) | ((prot & __SPRT_PROT_WRITE) ? __SPRT_TAU_PROT_WRITE : 0)
					| ((prot & __SPRT_PROT_EXEC) ? __SPRT_TAU_PROT_EXEC : 0));
	if (st != 0) {
		__sprt_errno = -st;
		return -1;
	}
	return 0;
}

extern "C" int madvise(void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int posix_madvise(void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int mlock(const void *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return 0; }

extern "C" int munlock(const void *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return 0; }

extern "C" int mlock2(const void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int mincore(void *, __SPRT_ID(size_t), unsigned char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}
