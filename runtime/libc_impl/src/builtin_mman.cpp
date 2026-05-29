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

	if (pMap) {
		MappingInfo::attachRegion(pMap, length, __fd);
	}
	return pMap;
}

__SPRT_C_FUNC int munmap(void *addr, size_t length) __SPRT_NOEXCEPT {
	int __fd = -1;
	if (!MappingInfo::isRegionExists(addr, length, &__fd)) {
		__sprt_errno = EINVAL;
		return -1;
	}

	if (__fd > -1) {
		auto libc = __libc::get();
		auto fdSlot = libc->get_fd_slot(__fd);
		if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_munmap) {
			__sprt_errno = EBADF;
			return -1;
		}

		auto ret = fdSlot->ops->fo_munmap(fdSlot, addr, length);
		if (ret == 0) {
			MappingInfo::detachRegion(addr);
			return 0;
		}
		return ret;
	} else {
		if (__file_munmap_anon(addr, length) == 0) {
			MappingInfo::detachRegion(addr);
			return 0;
		}
		return -1;
	}
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
