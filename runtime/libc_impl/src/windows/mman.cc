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
#include <sprt/cxx/mutex>
#include <sprt/cxx/unordered_map>
#include <sys/mman.h>

#include "specific.h"

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/process_api.h>

#include "../../include/__impl_libc.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

__SPRT_C_FUNC int mprotect(void *addr, size_t len, int prot) __SPRT_NOEXCEPT {
	if (!addr || len == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	auto flProtect = platform::__getProtectFlags(prot);

	DWORD old_protect = 0;
	BOOL ok = VirtualProtect(addr, len, flProtect, &old_protect);

	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return 0;
}

__SPRT_C_FUNC int posix_madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT {
	if (!addr || len == 0) {
		__sprt_errno = EINVAL;
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

	default: __sprt_errno = EINVAL; return -1;
	}
}

__SPRT_C_FUNC int mlock(const void *addr, size_t len) __SPRT_NOEXCEPT {
	if (!addr || len == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	BOOL ok = VirtualLock((void *)addr, len);
	if (!ok) {
		DWORD err = GetLastError();
		if (err == ERROR_WORKING_SET_QUOTA || err == ERROR_NOT_ENOUGH_MEMORY) {
			__sprt_errno = ENOMEM;
		}
		__sprt_errno = EACCES;
		return -1;
	}

	return 0;
}

__SPRT_C_FUNC int munlock(const void *addr, size_t len) __SPRT_NOEXCEPT {
	if (!addr || len == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	BOOL ok = VirtualUnlock((void *)addr, len);
	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return 0;
}

__SPRT_C_FUNC int mlock2(const void *addr, size_t len, int __flags) __SPRT_NOEXCEPT {
	if (__flags != 0) {
		__sprt_errno = ENOTSUP;
		return -1;
	}
	return mlock(addr, len);
}

__SPRT_C_FUNC int madvise(void *addr, size_t len, int advice) __SPRT_NOEXCEPT {
	if (!addr || len == 0) {
		__sprt_errno = EINVAL;
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

	default: __sprt_errno = EINVAL; return -1;
	}
}

__SPRT_C_FUNC int mincore(void *addr, size_t length, unsigned char *vec) __SPRT_NOEXCEPT {
	if (!addr || !length || !vec) {
		__sprt_errno = EINVAL;
		return -1;
	}

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	size_t page_size = si.dwPageSize;

	if (reinterpret_cast<uintptr_t>(addr) % page_size != 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	auto npages = (length + page_size - 1) / page_size;

	memset(vec, 0, npages);

	auto WsInfo = __sprt_typed_malloca(PSAPI_WORKING_SET_EX_INFORMATION, npages);

	if (WsInfo == NULL) {
		__sprt_errno = ENOMEM;
		return -1;
	}

	char *StartPtr = (char *)addr;

	for (DWORD_PTR i = 0; i < npages; i++) { WsInfo[i].VirtualAddress = StartPtr + i * page_size; }

	BOOL bResult = QueryWorkingSetEx(GetCurrentProcess(), WsInfo,
			(DWORD)page_size * sizeof(PSAPI_WORKING_SET_EX_INFORMATION));

	if (!bResult) {
		__sprt_freea(WsInfo);
		__sprt_errno = ENOMEM;
		return -1;
	}

	for (DWORD_PTR i = 0; i < npages; i++) {
		vec[i] = WsInfo[i].VirtualAttributes.Valid ? 1 : 0; //
	}

	__sprt_freea(WsInfo);
	return 0;
}

} // namespace sprt
