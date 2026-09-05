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

#include "sys/mman.h"

#if SPRT_WINDOWS
#include "windows/mman.cc"
#elif SPRT_WASM
#include "wasm/mman.cc"
#elif SPRT_EMBOX_USER
#include "embox_user/mman.cc"
#endif

namespace sprt {

struct MappingRegion {
	void *addr = nullptr;
	size_t length = 0;
	int fd = -1;
};

struct MappingInfo {
	qmutex mutex;
	__malloc_unordered_map<void *, MappingRegion> regions;

	static void attachRegion(void *, size_t, int fd);
	static void detachRegion(void *);
	static bool isRegionExists(void *, size_t, int *fd);
};

static MappingInfo s_mappingInfo;

// Whether an ANONYMOUS mapping gets an entry in the registry above.
//
// Where mimalloc reaches the OS through this very mmap() -- Embox EL0 does, and
// it is the first target that does -- the entry cannot be made. Inserting into
// the map allocates, allocating calls mimalloc, and mimalloc calls mmap(), which
// arrives back at attachRegion() and at a mutex its own caller is holding one
// frame up. It is a self-deadlock, and it fires on the first allocation of the
// first program: the very first mmap() IS mimalloc reserving its arena. Windows
// and wasm never met it because their mimalloc goes to VirtualAlloc and to sbrk
// instead of coming back through here.
//
// Nothing is lost by leaving anonymous mappings out. An entry exists to remember
// which fd a mapping came from so munmap and msync can route to that file's ops,
// and an anonymous mapping has no fd. It is also what makes a PARTIAL unmap
// possible: POSIX lets munmap release part of a mapping, an allocator that wants
// aligned memory over-allocates and hands the slack back, and the exact-length
// lookup here would refuse it.
//
// It is still needed where __file_munmap_anon cannot tell a mapping of its own
// from a stray pointer. On wasm anonymous memory is allocator memory and munmap
// is free(), which validates nothing -- there the registry is what stands
// between a bad munmap and a corrupted heap. On Windows VirtualFree does its own
// validation, but it also cannot release part of a reservation, so there is
// nothing to gain by dropping the bookkeeping there.
#if SPRT_EMBOX_USER
static constexpr bool s_trackAnonMappings = false;
#else
static constexpr bool s_trackAnonMappings = true;
#endif

__SPRT_C_FUNC void *mmap(void *addr, size_t length, int prot, int flags, int __fd,
		off_t offset) __SPRT_NOEXCEPT {
	if (length == 0) {
		return __SPRT_MAP_FAILED;
	}

	void *pMap = __SPRT_MAP_FAILED;
	// ANONYMOUS mapping
	if (flags & __SPRT_MAP_ANONYMOUS || __fd == -1) {
		pMap = __file_mmap_anon(addr, length, prot, flags, offset);
	} else {
		if (__fd < 0) {
			__sprt_errno = EBADF;
			return __SPRT_MAP_FAILED;
		}

		auto libc = __libc::get();
		auto fdSlot = libc->get_fd_slot(__fd);
		if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_mmap) {
			__sprt_errno = EBADF;
			return __SPRT_MAP_FAILED;
		}

		pMap = fdSlot->ops->fo_mmap(fdSlot, addr, length, prot, flags, offset);
	}

	// `pMap != MAP_FAILED` and not `pMap`: MAP_FAILED is (void *)-1, so the old
	// test registered a region for every failed mapping too.
	if ((pMap != __SPRT_MAP_FAILED) && pMap && (s_trackAnonMappings || __fd >= 0)) {
		MappingInfo::attachRegion(pMap, length, __fd);
	}
	return pMap;
}

__SPRT_C_FUNC int munmap(void *addr, size_t length) __SPRT_NOEXCEPT {
	int __fd = -1;
	bool known = MappingInfo::isRegionExists(addr, length, &__fd);

	if (known && (__fd > -1)) {
		auto libc = __libc::get();
		auto fdSlot = libc->get_fd_slot(__fd);
		if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_munmap) {
			__sprt_errno = EBADF;
			return -1;
		}

		auto ret = fdSlot->ops->fo_munmap(fdSlot, addr, length);
		if (ret == 0) {
			MappingInfo::detachRegion(addr);
		}
		return ret;
	}

	if (!known && s_trackAnonMappings) {
		// Every mapping is in the registry on this platform, so an address that
		// is not there was never mapped -- and handing it to the anonymous
		// backend would mean free()ing a pointer it never allocated.
		__sprt_errno = EINVAL;
		return -1;
	}

	// Anonymous. Untracked platforms pass the range straight down: the kernel
	// owns the mapping, validates the range, and is the only one that can
	// honour an unmap of part of it.
	//
	// The lookup above is by exact length, so part of a FILE-backed mapping
	// arrives here too and goes to the anonymous backend. Nothing can reach that
	// today -- no platform supports a file mapping that can be partly unmapped:
	// Windows cannot free part of a reservation, wasm has no file mappings, and
	// Embox EL0 answers mmap-with-fd with ENOSYS until device mmap lands (K5).
	// When it does, this needs a lookup by containment rather than by length.
	if (__file_munmap_anon(addr, length) == 0) {
		if (known) {
			MappingInfo::detachRegion(addr);
		}
		return 0;
	}
	return -1;
}

__SPRT_C_FUNC int msync(void *addr, size_t length, int flags) __SPRT_NOEXCEPT {
	if (!addr || length == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	int __fd = -1;
	if (!MappingInfo::isRegionExists(addr, length, &__fd)) {
		__sprt_errno = ENOMEM;
		return -1;
	}

	if (__fd < 0) {
		__sprt_errno = ENOMEM;
		return -1;
	}

	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_msync) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_msync(fdSlot, addr, length, flags);
}

void MappingInfo::attachRegion(void *ptr, size_t size, int fd) {
	unique_lock lock(s_mappingInfo.mutex);
	s_mappingInfo.regions.emplace(ptr, MappingRegion(ptr, size, fd));
}

void MappingInfo::detachRegion(void *ptr) {
	unique_lock lock(s_mappingInfo.mutex);
	s_mappingInfo.regions.erase(ptr);
}

bool MappingInfo::isRegionExists(void *ptr, size_t size, int *fd) {
	unique_lock lock(s_mappingInfo.mutex);
	auto it = s_mappingInfo.regions.find(ptr);
	if (it != s_mappingInfo.regions.end()) {
		if (it->second.length == size) {
			if (fd) {
				*fd = it->second.fd;
			}
			return true;
		}
	}
	return false;
}

} // namespace sprt
