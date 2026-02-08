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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/sys/__sprt_mman.h>
#include <sprt/runtime/int.h>
#include <sprt/runtime/mutex.h>
#include <sprt/runtime/mem/map.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <corecrt_io.h>
#include <psapi.h>

#include "private/SPRTSpecific.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

struct MappingRegion {
	void *addr = nullptr;
	size_t length = 0;
	int fd = -1;
};

struct MappingInfo {
	qmutex mutex;
	memory::dynmap<void *, MappingRegion> regions;

	static void attachRegion(void *, size_t, int fd);
	static void detachRegion(void *);
	static bool isRegionExists(void *, size_t, int *fd);
};

static MappingInfo s_mappingInfo;

static DWORD __getProtectFlags(int prot) {
	DWORD flProtect = 0;
	if (prot & __SPRT_PROT_EXEC) {
		if (prot & __SPRT_PROT_WRITE) {
			flProtect = PAGE_EXECUTE_READWRITE;
		} else if (prot & __SPRT_PROT_READ) {
			flProtect = PAGE_EXECUTE_READ;
		} else {
			flProtect |= PAGE_EXECUTE;
		}
	} else if (prot & __SPRT_PROT_WRITE) {
		flProtect = PAGE_READWRITE;
	} else if (prot & __SPRT_PROT_READ) {
		flProtect = PAGE_READONLY;
	} else {
		flProtect = PAGE_NOACCESS;
	}
	return flProtect;
}

static void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
	if (length == 0) {
		return __SPRT_MAP_FAILED;
	}

	auto flProtect = __getProtectFlags(prot);

	// ANONYMOUS mapping
	if (flags & __SPRT_MAP_ANONYMOUS || fd == -1) {
		auto retAddr = VirtualAlloc(addr, length, MEM_COMMIT | MEM_RESERVE, flProtect);
		if (retAddr) {
			/*MEMORY_BASIC_INFORMATION mbi;
			SIZE_T result = VirtualQuery(retAddr, &mbi, sizeof(mbi));

			if (result == 0) {
				VirtualFree(retAddr, length, MEM_RELEASE);
				errno = ENOMEM;
				return __SPRT_MAP_FAILED;
			}*/

			MappingInfo::attachRegion(retAddr, length, -1);

			return retAddr;
		} else {
			errno = ENOMEM;
			return __SPRT_MAP_FAILED;
		}
	}

	// File mapping
	if (flProtect == PAGE_NOACCESS) {
		errno = EINVAL;
		return __SPRT_MAP_FAILED;
	}

	if (fd < 0) {
		__sprt_errno = EBADF;
		return __SPRT_MAP_FAILED;
	}

	HANDLE hFile = (HANDLE)_get_osfhandle(fd);
	if (hFile == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return __SPRT_MAP_FAILED;
	}

	if (flags & __SPRT_MAP_HUGETLB) {
		flProtect |= SEC_LARGE_PAGES;
	}

	LARGE_INTEGER liOffset = {.QuadPart = offset};
	HANDLE hMap =
			CreateFileMappingW(hFile, NULL, flProtect, liOffset.HighPart, liOffset.LowPart, NULL);
	if (!hMap) {
		errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_MAP_FAILED;
	}

	DWORD dwDesiredAccess = FILE_MAP_READ;
	if (prot & __SPRT_PROT_WRITE) {
		dwDesiredAccess |= FILE_MAP_WRITE;
	}
	if (prot & __SPRT_PROT_EXEC) {
		dwDesiredAccess |= FILE_MAP_EXECUTE;
	}

	if (flags & __SPRT_MAP_PRIVATE) {
		dwDesiredAccess |= FILE_MAP_COPY;
	}

	if (flags & __SPRT_MAP_HUGETLB) {
		dwDesiredAccess |= FILE_MAP_LARGE_PAGES;
	}

	void *pMap = MapViewOfFile(hMap, dwDesiredAccess, liOffset.HighPart, liOffset.LowPart, length);

	CloseHandle(hMap); // Mapping owns ref

	if (!pMap) {
		errno = platform::lastErrorToErrno(GetLastError());
		CloseHandle(hMap);
		return __SPRT_MAP_FAILED;
	}

	MappingInfo::attachRegion(pMap, length, fd);

	return pMap;
}

static int munmap(void *addr, size_t length) {
	int fd = -1;
	if (!MappingInfo::isRegionExists(addr, length, &fd)) {
		errno = EINVAL;
		return -1;
	}

	if (fd > -1) {
		BOOL ok = UnmapViewOfFile(addr);
		if (!ok) {
			errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		MappingInfo::detachRegion(addr);
		return 0;
	} else {
		BOOL ok = VirtualFree(addr, 0, MEM_RELEASE);
		if (!ok) {
			errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		MappingInfo::detachRegion(addr);
		return 0;
	}
}

static int mprotect(void *addr, size_t len, int prot) {
	if (!addr || len == 0) {
		errno = EINVAL;
		return -1;
	}

	auto flProtect = __getProtectFlags(prot);

	DWORD old_protect = 0;
	BOOL ok = VirtualProtect(addr, len, flProtect, &old_protect);

	if (!ok) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return 0;
}

static int msync(void *addr, size_t length, int flags) {
	if (!addr || length == 0) {
		errno = EINVAL;
		return -1;
	}

	int fd = -1;
	if (!MappingInfo::isRegionExists(addr, length, &fd)) {
		errno = ENOMEM;
		return -1;
	}

	if (fd < 0) {
		errno = ENOMEM;
		return -1;
	}

	// MS_ASYNC: Windows schedules automatically (noop)
	if (flags == __SPRT_MS_ASYNC) {
		return 0;
	}

	// MS_SYNC or MS_INVALIDATE: flush dirty pages
	if (flags != __SPRT_MS_SYNC && flags != __SPRT_MS_INVALIDATE) {
		errno = EINVAL;
		return -1;
	}

	BOOL ok = FlushViewOfFile(addr, length);
	if (!ok) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// Flush file buffers for disk durability
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	HANDLE hFile = (HANDLE)_get_osfhandle(fd);
	if (hFile != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(hFile);
		return 0;
	}

	errno = EBADF;
	return -1;
}

static int posix_madvise(void *addr, size_t len, int advice) {
	if (!addr || len == 0) {
		errno = EINVAL;
		return -1;
	}

	switch (advice) {
	case __SPRT_POSIX_MADV_NORMAL:
	case __SPRT_POSIX_MADV_RANDOM:
		// Windows default prefetch: no-op
		return 0;

	case __SPRT_POSIX_MADV_SEQUENTIAL:
		// No sequential hint API
		return 0;

	case __SPRT_POSIX_MADV_WILLNEED:
		// Pre-fault pages (noop: access triggers)
		return 0;

	case __SPRT_POSIX_MADV_DONTNEED: return DiscardVirtualMemory(addr, len) ? 0 : -1;

	default: errno = EINVAL; return -1;
	}
}

static int mlock(const void *addr, size_t len) {
	if (!addr || len == 0) {
		errno = EINVAL;
		return -1;
	}

	BOOL ok = VirtualLock((void *)addr, len);
	if (!ok) {
		DWORD err = GetLastError();
		if (err == ERROR_WORKING_SET_QUOTA || err == ERROR_NOT_ENOUGH_MEMORY) {
			errno = ENOMEM;
		}
		errno = EACCES;
		return -1;
	}

	return 0;
}

static int munlock(const void *addr, size_t len) {
	if (!addr || len == 0) {
		errno = EINVAL;
		return -1;
	}

	BOOL ok = VirtualUnlock((void *)addr, len);
	if (!ok) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return 0;
}

static int mlock2(const void *addr, size_t len, int __flags) {
	if (__flags != 0) {
		errno = ENOTSUP;
		return -1;
	}
	return mlock(addr, len);
}

static int madvise(void *addr, size_t len, int advice) {
	if (!addr || len == 0) {
		errno = EINVAL;
		return -1;
	}

	switch (advice) {
	case __SPRT_MADV_NORMAL:
	case __SPRT_MADV_RANDOM:
		// Default behavior
		return 0;

	case __SPRT_MADV_SEQUENTIAL:
		// File hint: handled by CreateFile(FILE_FLAG_SEQUENTIAL_SCAN)
		return 0;

	case __SPRT_MADV_WILLNEED:
		// Prefetch (Win8+)
		return 0;

	case __SPRT_MADV_DONTNEED: return DiscardVirtualMemory(addr, len) ? 0 : -1;


	case __SPRT_MADV_FREE:
	case __SPRT_MADV_REMOVE:
		// Stronger: decommit
		return VirtualFree(addr, len, MEM_DECOMMIT) ? 0 : -1;

	case __SPRT_MADV_MERGEABLE:
	case __SPRT_MADV_UNMERGEABLE:
	case __SPRT_MADV_HUGEPAGE:
	case __SPRT_MADV_NOHUGEPAGE:
		// No KSM/THP on Windows
		return 0;

	case __SPRT_MADV_DONTFORK:
	case __SPRT_MADV_DOFORK:
		// Windows fork = copy
		return 0;

	default: errno = EINVAL; return -1;
	}
}

static int mincore(void *addr, size_t length, unsigned char *vec) {
	if (!addr || !length || !vec) {
		errno = EINVAL;
		return -1;
	}

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	size_t page_size = si.dwPageSize;

	if (reinterpret_cast<uintptr_t>(addr) % page_size != 0) {
		errno = EINVAL;
		return -1;
	}

	auto npages = (length + page_size - 1) / page_size;

	memset(vec, 0, npages);

	auto WsInfo = __sprt_typed_malloca(PSAPI_WORKING_SET_EX_INFORMATION, npages);

	if (WsInfo == NULL) {
		errno = ENOMEM;
		return -1;
	}

	char *StartPtr = (char *)addr;

	for (DWORD_PTR i = 0; i < npages; i++) { WsInfo[i].VirtualAddress = StartPtr + i * page_size; }

	BOOL bResult = QueryWorkingSetEx(GetCurrentProcess(), WsInfo,
			(DWORD)page_size * sizeof(PSAPI_WORKING_SET_EX_INFORMATION));

	if (!bResult) {
		__sprt_freea(WsInfo);
		errno = ENOMEM;
		return -1;
	}

	for (DWORD_PTR i = 0; i < npages; i++) {
		vec[i] = WsInfo[i].VirtualAttributes.Valid ? 1 : 0; //
	}

	__sprt_freea(WsInfo);
	return 0;
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
