// Memory protection on Tau: mappings are readable and writable, and nothing changes that.

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>

#include "../../include/__impl_libc.h"

extern "C" int mprotect(void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int madvise(void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int posix_madvise(void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int mlock(const void *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return 0; }

extern "C" int munlock(const void *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT { return 0; }

extern "C" int mlock2(const void *, __SPRT_ID(size_t), int) __SPRT_NOEXCEPT { return 0; }

extern "C" int mincore(void *, __SPRT_ID(size_t), unsigned char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}
