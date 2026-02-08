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

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/sys/__sprt_random.h>

#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"

#include <Windows.h>
#include <corecrt_io.h>
#include <process.h>
#include <tlhelp32.h>
#include <lmcons.h>
#include <fcntl.h> // O_RDONLY, etc.

#include "private/SPRTSpecific.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static __SPRT_ID(ssize_t) read(int __fd, void *__buf, __SPRT_ID(size_t) __nbytes) {
	if (__fd < 0) {
		errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(__fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	DWORD read;
	if (!ReadFile(h, __buf, (DWORD)__nbytes, &read, NULL)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return (ssize_t)read;
}

static __SPRT_ID(ssize_t) write(int __fd, const void *__buf, __SPRT_ID(size_t) __n) {
	if (__fd < 0) {
		errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(__fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	DWORD written;
	if (!WriteFile(h, __buf, (DWORD)__n, &written, NULL)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return (ssize_t)written;
}

// @AI-geerated
static ssize_t pread64(int fd, void *buf, size_t count, off64_t offset) {
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	LARGE_INTEGER orig_pos;

	// 1. Save current position
	if (!SetFilePointerEx(h, LARGE_INTEGER{{0, 0}}, &orig_pos, FILE_CURRENT)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// 2. Seek to target
	LARGE_INTEGER target = {.QuadPart = offset};
	if (!SetFilePointerEx(h, target, NULL, FILE_BEGIN)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	DWORD read;
	if (!ReadFile(h, buf, (DWORD)count, &read, NULL)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// 4. Restore original position
	SetFilePointerEx(h, orig_pos, NULL, FILE_BEGIN);

	return (ssize_t)read;
}

// @AI-geerated
ssize_t pwrite64(int fd, const void *buf, size_t count, off64_t offset) {
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	LARGE_INTEGER orig_pos, target;
	target.QuadPart = offset;

	// 1. Save current position
	if (!SetFilePointerEx(h, LARGE_INTEGER{{0, 0}}, &orig_pos, FILE_CURRENT)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// 2. Seek to write offset
	if (!SetFilePointerEx(h, target, NULL, FILE_BEGIN)) {
		SetFilePointerEx(h, orig_pos, NULL, FILE_BEGIN); // Restore
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// 3. Write data
	DWORD written;
	if (!WriteFile(h, buf, (DWORD)count, &written, NULL)) {
		SetFilePointerEx(h, orig_pos, NULL, FILE_BEGIN); // Restore on error
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// 4. Restore original position
	SetFilePointerEx(h, orig_pos, NULL, FILE_BEGIN);

	return (ssize_t)written;
}

// @AI-geerated
int usleep(unsigned long us) {
	if (us >= 1'000) {
		Sleep(us / 1'000);
		us %= 1'000;
	}

	if (us > 0) {
		// Spin remainder (<1ms)
		LARGE_INTEGER freq, start, now;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&start);

		LARGE_INTEGER target;
		target.QuadPart = start.QuadPart + (us * freq.QuadPart / 1'000'000LL);

		while (1) {
			YieldProcessor();
			now.QuadPart = 0;
			QueryPerformanceCounter(&now);
			if (now.QuadPart >= target.QuadPart) {
				break;
			}
			target.QuadPart -= now.QuadPart;
		}
	}

	return 0;
}

int fchdir(int fdDir) {
	if (fdDir < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE handle = (HANDLE)_get_osfhandle(fdDir);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleW(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto buf = __sprt_typed_malloca(wchar_t, pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleW(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_freea(buf);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = SetCurrentDirectoryW(buf);

	__sprt_freea(buf);

	if (ret) {
		return 0;
	}

	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

static int fexecve(int __fd, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE handle = (HANDLE)_get_osfhandle(__fd);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleW(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto buf = __sprt_typed_malloca(wchar_t, pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleW(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_free(buf);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ustr = __MALLOCA_STRINGS(buf, writtenLen);

	auto ret = int(::_execve(ustr, _argv, __envp));
	__sprt_freea(ustr);
	__sprt_freea(buf);
	return ret;
}

// @AI-geerated
static long __wdiskpathconf(const wchar_t *diskPath, int name) {
	switch (name) {
	case __SPRT_PC_LINK_MAX: {
		DWORD fsFlags;
		GetVolumeInformationW(diskPath, NULL, 0, NULL, NULL, &fsFlags, NULL, 0);
		if (fsFlags & FILE_SUPPORTS_HARD_LINKS) {
			return 1'024;
		}
		return 1;
	}

	case __SPRT_PC_NAME_MAX: {
		DWORD max_comp;
		GetVolumeInformationW(diskPath, NULL, 0, NULL, &max_comp, NULL, NULL, 0);
		return (long)max_comp; // Usually 255 NTFS
	}

	case __SPRT_PC_PATH_MAX: return MAX_PATH; // MAX_PATH legacy; 32K with "\\?\"

	case __SPRT_PC_PIPE_BUF: return 512; // Named pipe granularity

	case __SPRT_PC_CHOWN_RESTRICTED: return 0; // Windows allows "chown" via TakeOwn

	case __SPRT_PC_NO_TRUNC: return 1; // NTFS truncates long names

	case __SPRT_PC_VDISABLE: return (long)'\0';

	case __SPRT_PC_SYNC_IO: return 1; // Supported via FILE_FLAG_WRITE_THROUGH

	case __SPRT_PC_FILESIZEBITS: return 64; // NTFS 64-bit files

	case __SPRT_PC_ALLOC_SIZE_MIN:
		ULARGE_INTEGER avail, total, free;
		GetDiskFreeSpaceExW(diskPath, &avail, &total, &free);
		return (long)avail.QuadPart; // Free bytes proxy

	default: errno = EINVAL; return -1;
	}
}

// @AI-geerated
static long pathconf(const char *path, int name) {
	wchar_t root[4] = {wchar_t(path[0]), L':', L'\\', L'\0'}; // e.g., "C:\\"
	return __wdiskpathconf(root, name);
}

static long fpathconf(int __fd, int name) {
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE handle = (HANDLE)_get_osfhandle(__fd);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleW(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto buf = __sprt_typed_malloca(wchar_t, pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleW(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_freea(buf);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	wchar_t root[4] = {buf[0], L':', L'\\', L'\0'}; // e.g., "C:\\"

	auto ret = __wdiskpathconf(root, name);

	__sprt_freea(buf);

	return ret;
}

// @AI-geerated
static long sysconf(int name) {
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	switch (name) {
	case __SPRT_SC_ARG_MAX: return 32'767; // Cmdline limit [prior]

	case __SPRT_SC_CHILD_MAX: return 65'536; // Arbitrary process limit

	case __SPRT_SC_CLK_TCK: return 1'000; // ms resolution proxy

	case __SPRT_SC_NPROCESSORS_ONLN: {
		DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		return (long)count;
	}

	case __SPRT_SC_NPROCESSORS_CONF: {
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION *proc_info;
		DWORD proc_info_size = 0;
		GetLogicalProcessorInformation(NULL, &proc_info_size);
		proc_info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)__sprt_malloca(proc_info_size);
		GetLogicalProcessorInformation(proc_info, &proc_info_size);

		int cores = 0;
		for (DWORD i = 0; i < proc_info_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i++) {
			if (proc_info[i].Relationship == RelationProcessorCore) {
				cores++;
			}
		}
		__sprt_freea(proc_info);
		return cores;
	}

	case __SPRT_SC_OPEN_MAX: return 2'048; // Handle table limit proxy

	case __SPRT_SC_PAGESIZE: return (long)si.dwPageSize;

	case __SPRT_SC_PHYS_PAGES:
		MEMORYSTATUSEX mem;
		mem.dwLength = sizeof(mem);
		GlobalMemoryStatusEx(&mem);
		return mem.ullTotalPhys / si.dwPageSize;

	case __SPRT_SC_AVPHYS_PAGES: { // Heuristic
		MEMORYSTATUSEX mem;
		mem.dwLength = sizeof(mem);
		GlobalMemoryStatusEx(&mem);
		return mem.ullTotalPhys / (si.dwPageSize * 2);
	}

	case __SPRT_SC_TZNAME_MAX: return 255; // Registry limit proxy

	default: errno = EINVAL; return -1;
	}
}

// @AI-geerated
static DWORD getppid(void) {
	DWORD pid = GetCurrentProcessId();
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 pe = {.dwSize = sizeof(PROCESSENTRY32)};
	if (!Process32First(snapshot, &pe)) {
		CloseHandle(snapshot);
		return 0;
	}

	DWORD ppid = 0;
	do {
		if (pe.th32ProcessID == pid) {
			ppid = pe.th32ParentProcessID;
			break;
		}
	} while (Process32Next(snapshot, &pe));

	CloseHandle(snapshot);
	return ppid;
}

// @AI-geerated
static __SPRT_ID(uid_t) getuid(void) {
	HANDLE hToken = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_ID(uid_t)(-1);
	}

	DWORD size = 0;
	GetTokenInformation(hToken, TokenUser, NULL, 0, &size);
	PTOKEN_USER pUser = (PTOKEN_USER)__sprt_malloca(size);
	BOOL ok = GetTokenInformation(hToken, TokenUser, pUser, size, &size);
	CloseHandle(hToken);

	if (!ok) {
		__sprt_freea(pUser);
		errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_ID(uid_t)(-1);
	}

	auto uid = platform::getRid(pUser->User.Sid);
	__sprt_freea(pUser);
	return uid;
}

static __SPRT_ID(uid_t) geteuid(void) {
	auto uid = platform::getEffectiveFileUid();
	if (uid) {
		return uid;
	}
	return getuid();
}

// @AI-geerated
static __SPRT_ID(gid_t) getgid(void) {
	HANDLE hToken = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_ID(gid_t)(-1);
	}

	DWORD size = 0;
	GetTokenInformation(hToken, TokenUser, NULL, 0, &size);

	PTOKEN_PRIMARY_GROUP pGroup = (PTOKEN_PRIMARY_GROUP)__sprt_malloca(size);

	BOOL ok = GetTokenInformation(hToken, TokenPrimaryGroup, pGroup, size, &size);
	CloseHandle(hToken);

	if (!ok || !pGroup->PrimaryGroup) {
		__sprt_freea(pGroup);
		errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_ID(gid_t)(-1);
	}

	auto gid = platform::getRid(pGroup->PrimaryGroup);
	__sprt_freea(pGroup);
	return gid;
}

static __SPRT_ID(gid_t) getegid(void) {
	auto uid = platform::getEffectiveFileGid();
	if (uid) {
		return uid;
	}
	return getgid();
}

// @AI-geerated
static int getgroups(int size, __SPRT_ID(gid_t) list[]) {
	if (size < 0) {
		errno = EINVAL;
		return -1;
	}

	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	DWORD needed = 0;
	GetTokenInformation(hToken, TokenGroups, NULL, 0, &needed);
	PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)__sprt_malloca(needed);

	BOOL ok = GetTokenInformation(hToken, TokenGroups, pGroups, needed, &needed);
	CloseHandle(hToken);

	if (!ok) {
		__sprt_freea(pGroups);
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	int count = 0;
	for (DWORD i = 0; i < pGroups->GroupCount && count < size; i++) {
		PSID sid = pGroups->Groups[i].Sid;
		DWORD sub_count = GetSidSubAuthorityCount(sid) ? *GetSidSubAuthorityCount(sid) : 0;
		if (sub_count == 0) {
			continue;
		}

		DWORD rid = *GetSidSubAuthority(sid, sub_count - 1);
		list[count++] = __SPRT_ID(gid_t)(rid);
	}

	__sprt_freea(pGroups);
	return count;
}

static BOOL __isdir(const wchar_t *path) {
	DWORD attr = GetFileAttributesW(path);
	return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// @AI-geerated

using dynwstring = memory::basic_string<wchar_t, memory::detail::DynamicAllocator<wchar_t>>;

static dynwstring __readlink_str(const wchar_t *path) {
	HANDLE h =
			CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		errno = platform::lastErrorToErrno(GetLastError());
		return dynwstring();
	}

	auto pathLen = GetFinalPathNameByHandleW(h, NULL, 0, 0);
	if (pathLen == 0) {
		CloseHandle(h);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return dynwstring();
	}

	dynwstring ret;
	ret.resize(pathLen);

	auto writtenLen = GetFinalPathNameByHandleW(h, ret.data(), pathLen + 1, FILE_NAME_NORMALIZED);
	CloseHandle(h);

	if (writtenLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return dynwstring();
	}

	ret.resize(writtenLen);
	return ret;
}

static int link(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__from, [&](const char *from) {
		// call with native path
		return internal::performWithNativePath(__to, [&](const char *to) {
			// call with native path
			auto wFrom = __MALLOCA_WSTRING(from);
			auto wTo = __MALLOCA_WSTRING(to);
			auto ret = CreateHardLinkW(wTo, wFrom, nullptr);
			__sprt_freea(wTo);
			__sprt_freea(wFrom);
			if (ret) {
				return 0;
			}
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}, -1);
	}, -1);
}

static int symlink(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__from, [&](const char *from) {
		// call with native path
		return internal::performWithNativePath(__to, [&](const char *to) {
			// call with native path
			auto wFrom = __MALLOCA_WSTRING(from);
			auto wTo = __MALLOCA_WSTRING(to);
			auto ret = CreateSymbolicLinkW(wTo, wFrom,
					__isdir(wFrom) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
			__sprt_freea(wTo);
			__sprt_freea(wFrom);
			if (ret) {
				return 0;
			}
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}, -1);
	}, -1);
}

// @AI-geerated
static int __access(const char *path, int mode, int flags) {
	if (!path || !*path) {
		errno = ENOENT;
		return -1;
	}

	auto wpath = __MALLOCA_WSTRING(path);

	DWORD attr = GetFileAttributesW(wpath);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		__sprt_freea(wpath);
		DWORD err = GetLastError();
		errno = (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? ENOENT : EACCES;
		return -1;
	}

	// F_OK: exists
	if (mode == __SPRT_F_OK) {
		__sprt_freea(wpath);
		return 0;
	}

	// Directory? Fail (POSIX: not regular file)
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
		__sprt_freea(wpath);
		errno = EISDIR;
		return -1;
	}

	DWORD fileFlags = 0;
	if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) && (flags & __SPRT_AT_SYMLINK_NOFOLLOW)) {
		fileFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
	}

	// X_OK: executable (any file)
	if (mode & __SPRT_X_OK) {
		__sprt_freea(wpath);
		return 0;
	}

	// R_OK: try read-only open
	if (mode & __SPRT_R_OK) {
		HANDLE h = CreateFileW(wpath, GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | fileFlags, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			__sprt_freea(wpath);
			errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
			return -1;
		}
		CloseHandle(h);
	}

	// W_OK: try write-only open
	if (mode & __SPRT_W_OK) {
		HANDLE h = CreateFileW(wpath, GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | fileFlags, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			__sprt_freea(wpath);
			errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
			return -1;
		}
		CloseHandle(h);
	}

	__sprt_freea(wpath);
	return 0;
}

static int access(const char *__path, int __mode) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __access(path, __mode, 0); //
	}, -1);
}

static int eaccess(const char *__path, int __mode) { return access(__path, __mode); }

static ssize_t __readlink(const char *path, char *buf, size_t bufsiz) {
	auto wpath = __MALLOCA_WSTRING(path);
	auto strPath = __readlink_str(wpath);
	if (strPath.empty()) {
		return -1;
	}

	WideStringView wstr((char16_t *)strPath.data(), strPath.size());

	size_t ulen = 0;
	unicode::toUtf8(buf, bufsiz, wstr, &ulen);

	// convert in place
	return __sprt_fpath_to_posix(buf, ulen, buf, bufsiz);
}

static ssize_t readlink(const char *__path, char *buf, size_t bufsiz) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __readlink(path, buf, bufsiz); //
	}, ssize_t(-1));
}

// @AI-geerated
static int __unlink(const char *path) {
	// Clear read-only attribute first
	auto wpath = __MALLOCA_WSTRING(path);
	DWORD attr = GetFileAttributesW(wpath);
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
		if (!SetFileAttributesW(wpath, attr & ~FILE_ATTRIBUTE_READONLY)) {
			// Still try to delete even if attr change fails
		}
	}

	auto ret = DeleteFileW(wpath);
	__sprt_freea(wpath);
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
		case ERROR_ACCESS_DENIED: errno = EACCES; break;
		case ERROR_SHARING_VIOLATION: errno = EBUSY; break;
		default: errno = platform::lastErrorToErrno(err);
		}
		return -1;
	}

	return 0;
}

static int unlink(const char *__path) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __unlink(path); //
	}, -1);
}

// @AI-geerated
static int __rmdir(const char *path) {
	auto wpath = __MALLOCA_WSTRING(path);
	auto ret = RemoveDirectoryW(wpath);
	__sprt_freea(wpath);
	if (!ret) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_PATH_NOT_FOUND:
		case ERROR_FILE_NOT_FOUND: errno = ENOENT; break;
		case ERROR_ACCESS_DENIED: errno = EACCES; break;
		case ERROR_DIR_NOT_EMPTY: errno = ENOTEMPTY; break;
		default: errno = platform::lastErrorToErrno(err);
		}
		return -1;
	}

	return 0;
}

static int rmdir(const char *__path) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __rmdir(path); //
	}, -1);
}

static char *getlogin(void) {
	static wchar_t wusername[UNLEN + 1] = {0};
	static char username[UNLEN * 3 + 1] = {0};

	DWORD username_len = UNLEN + 1;
	if (!GetUserNameW(wusername, &username_len)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	size_t len = 0;
	unicode::toUtf8(username, UNLEN * 3 + 1, WideStringView((char16_t *)wusername, username_len),
			&len);
	username[len] = 0;

	return username;
}

static int getlogin_r(char *__name, size_t __name_len) {
	wchar_t wusername[UNLEN + 1] = {0};
	DWORD username_len = UNLEN + 1;
	if (!GetUserNameW(wusername, &username_len)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ulen = unicode::getUtf8Length(WideStringView((char16_t *)wusername, username_len));
	if (__name_len < ulen) {
		errno = ERANGE;
		return -1;
	}

	size_t len = 0;
	unicode::toUtf8(__name, __name_len, WideStringView((char16_t *)wusername, username_len), &len);
	if (len < __name_len) {
		__name[len] = 0;
	}
	return 0;
}

// @AI-geerated
static int getdomainname(char *__name, size_t __name_len) {
	if (__name_len == 0 || __name == nullptr) {
		errno = EINVAL;
		return -1;
	}

	DWORD nSymbols = 0;
	GetComputerNameExW(ComputerNameDnsDomain, nullptr, &nSymbols);

	if (nSymbols == 0) {
		errno = EINVAL;
		return -1;
	}

	nSymbols += 1;
	auto wbuf = __sprt_typed_malloca(wchar_t, nSymbols);

	if (!GetComputerNameExW(ComputerNameDnsDomain, wbuf, &nSymbols)) {
		errno = EINVAL;
		return -1;
	}

	auto ulen = unicode::getUtf8Length(WideStringView((char16_t *)wbuf, nSymbols));
	if (__name_len < ulen) {
		errno = EINVAL;
		return -1;
	}

	size_t len = 0;
	unicode::toUtf8(__name, __name_len, WideStringView((char16_t *)wbuf, nSymbols), &len);
	if (len < __name_len) {
		__name[len] = 0;
	}
	return 0;
}

// @AI-geerated
static int fsync(int fd) {
	if (fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	if (!FlushFileBuffers(h)) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return 0;
}

// @AI-geerated
static int getpagesize(void) {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (int)si.dwPageSize; // 4096 (x86), 65536 (x64 typical)
}

// @AI-geerated
static int getdtablesize(void) {
	return 2'048; // Conservative process handle table limit
}

// @AI-geerated
static int __truncate(const char *path, off64_t length) {
	auto wpath = __MALLOCA_WSTRING(path);
	HANDLE h = CreateFileW(wpath, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
			length == 0 ? TRUNCATE_EXISTING : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	__sprt_freea(wpath);
	if (h == INVALID_HANDLE_VALUE) {
		errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (length != 0) {
		// Seek to new length
		LARGE_INTEGER li = {.QuadPart = length};
		LARGE_INTEGER new_pos;
		if (!SetFilePointerEx(h, li, &new_pos, FILE_BEGIN) || new_pos.QuadPart != length) {
			CloseHandle(h);
			errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}

		// Truncate
		if (!SetEndOfFile(h)) {
			CloseHandle(h);
			errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
	}

	CloseHandle(h);
	return 0;
}

static int truncate64(const char *__path, off64_t length) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __truncate(path, length); //
	}, -1);
}

// @AI-geerated
static int ftruncate64(int __fd, off64_t length) {
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE handle = (HANDLE)_get_osfhandle(__fd);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	LARGE_INTEGER li = {.QuadPart = length};
	LARGE_INTEGER new_pos;

	if (!SetFilePointerEx(handle, li, &new_pos, FILE_BEGIN) || new_pos.QuadPart != length) {
		errno = GetLastError();
		return -1;
	}

	if (!SetEndOfFile(handle)) {
		errno = GetLastError();
		return -1;
	}

	return 0;
}


struct flock {
	short int l_type; /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.	*/
	short int l_whence; /* Where `l_start' is relative to (like `lseek').  */
	off_t l_start; /* Offset where the lock begins.  */
	off_t l_len; /* Size of the locked area; zero means until EOF.  */
	__SPRT_ID(pid_t) l_pid; /* Process holding the lock.  */
};

// from musl-libc
static int lockf64(int fd, int op, off_t size) {
	flock l = {
		.l_type = __SPRT_F_WRLCK,
		.l_whence = SEEK_CUR,
		.l_len = size,
	};
	switch (op) {
	case __SPRT_F_TEST:
		l.l_type = __SPRT_F_RDLCK;
		if (__sprt_fcntl(fd, __SPRT_F_GETLK, &l) < 0) {
			return -1;
		}
		if (l.l_type == __SPRT_F_UNLCK || l.l_pid == GetCurrentProcessId()) {
			return 0;
		}
		errno = EACCES;
		return -1;
	case __SPRT_F_ULOCK: l.l_type = __SPRT_F_UNLCK;
	case __SPRT_F_TLOCK: return __sprt_fcntl(fd, __SPRT_F_SETLK, &l);
	case __SPRT_F_LOCK: return __sprt_fcntl(fd, __SPRT_F_SETLKW, &l);
	}
	errno = EINVAL;
	return -1;
}

// @AI-geerated
static ssize_t copy_file_range(int fd_in, long long *off_in, int fd_out, long long *off_out,
		size_t len, unsigned int flags) {
	if (flags != 0) {
		errno = EINVAL;
		return -1;
	}

	HANDLE hin = (HANDLE)_get_osfhandle(fd_in);
	HANDLE hout = (HANDLE)_get_osfhandle(fd_out);
	if (hin == INVALID_HANDLE_VALUE || hout == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	const DWORD BUF_SIZE = 1 << 16; // 64 KiB
	char buffer[BUF_SIZE];
	size_t total = 0;

	while (total < len) {
		DWORD chunk = (DWORD)((len - total) > BUF_SIZE ? BUF_SIZE : (len - total));

		LARGE_INTEGER pos;
		LARGE_INTEGER newpos;

		// Seek input if off_in provided
		if (off_in) {
			pos.QuadPart = *off_in;
			if (!SetFilePointerEx(hin, pos, &newpos, FILE_BEGIN)) {
				errno = EIO;
				return (ssize_t)(total ? total : -1);
			}
		}

		DWORD nread = 0;
		if (!ReadFile(hin, buffer, chunk, &nread, NULL)) {
			DWORD err = GetLastError();
			if (err == ERROR_HANDLE_EOF || err == ERROR_BROKEN_PIPE) {
				break; // EOF
			}
			errno = EIO;
			return (ssize_t)(total ? total : -1);
		}
		if (nread == 0) {
			break; // EOF
		}

		if (off_in) {
			*off_in += nread;
		}

		// Seek output if off_out provided
		if (off_out) {
			pos.QuadPart = *off_out;
			if (!SetFilePointerEx(hout, pos, &newpos, FILE_BEGIN)) {
				errno = EIO;
				return (ssize_t)(total ? total : -1);
			}
		}

		DWORD nwritten = 0;
		if (!WriteFile(hout, buffer, nread, &nwritten, NULL) || nwritten < nread) {
			errno = EIO;
			return (ssize_t)(total ? total : -1);
		}

		if (off_out) {
			*off_out += nwritten;
		}

		total += nwritten;
		if (nread < chunk) {
			break; // Short read (likely EOF)
		}
	}

	return (ssize_t)total;
}

static int getentropy(void *__buffer, __SPRT_ID(size_t) __length) {
	if (__length > 256 || __length == 0 || !__buffer) {
		errno = EINVAL;
		return -1;
	}

	return __sprt_getrandom(__buffer, __length, __SPRT_GRND_RANDOM);
}

static int symlinkat(const char *__old_path, int __new_dir_fd, const char *__new_path) {
	int ret = 0;
	if (!platform::openAtPath(__new_dir_fd, __new_path, [&](const char *to, size_t size) {
		ret = internal::performWithNativePath(__old_path, [&](const char *from) {
			auto wFrom = __MALLOCA_WSTRING(from);
			auto wTo = __MALLOCA_WSTRING(to);
			auto ret = CreateSymbolicLinkW(wTo, wFrom,
					__isdir(wFrom) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
			__sprt_freea(wTo);
			__sprt_freea(wFrom);
			if (ret) {
				return 0;
			} else {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}
		}, -1);
	})) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static ssize_t readlinkat(int __dir_fd, const char *__path, char *__buf, size_t __buf_size) {
	int ret = 0;
	if (!platform::openAtPath(__dir_fd, __path, [&](const char *path, size_t size) {
		ret = __readlink(path, __buf, __buf_size);
	})) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	int ret = 0;
	if (!platform::openAtPath(__dirfd, __path,
				[&](const char *path, size_t size) { ret = __access(path, __mode, __flags); })) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int linkat(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	int ret = 0;
	if (!platform::openAtPath(__old_dir_fd, __old_path, [&](const char *from, size_t fromSize) {
		if (!platform::openAtPath(__new_dir_fd, __new_path, [&](const char *to, size_t toSize) {
			auto wFrom = __MALLOCA_WSTRINGS(from, fromSize);
			auto wFromBuf = wFrom;
			auto wTo = __MALLOCA_WSTRINGS(to, toSize);

			dynwstring tmpPath;
			if (__flags & __SPRT_AT_SYMLINK_FOLLOW) {
				DWORD attr = 0;
				do {
					attr = GetFileAttributesW(wFrom);
					if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
						auto newPath = __readlink_str(wFrom);
						if (newPath == tmpPath) {
							break;
						}
						tmpPath = sprt::move(newPath);
						wFrom = tmpPath.data();
					}
				} while (attr & FILE_ATTRIBUTE_REPARSE_POINT);
			}

			// call with native path
			auto ret = CreateHardLinkW(wTo, wFrom, nullptr);
			__sprt_freea(wTo);
			__sprt_freea(wFromBuf);
			if (ret) {
				ret = 0;
			} else {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				ret = -1;
			}
		})) {
			__sprt_errno = EINVAL;
			ret = -1;
			return;
		}
	})) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int unlinkat(int __dirfd, const char *__path, int __flags) {
	int ret = 0;
	if (!platform::openAtPath(__dirfd, __path, [&](const char *path, size_t size) {
		if (__flags & __SPRT_AT_REMOVEDIR) {
			ret = __rmdir(path);
		} else {
			ret = __unlink(path);
		}
	})) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int pipe2(int fds[2], int flags) {
	SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE}; // Inheritable

	HANDLE hRead, hWrite;
	BOOL ok = CreatePipe(&hRead, &hWrite, (flags & __SPRT_O_CLOEXEC) ? nullptr : &sa,
			0); // 0=default buffer

	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// Convert to fds: read first, write second (POSIX convention)
	fds[0] = _open_osfhandle((intptr_t)hRead, _O_RDONLY | _O_BINARY);
	fds[1] = _open_osfhandle((intptr_t)hWrite, _O_WRONLY | _O_BINARY);

	if (fds[0] == -1 || fds[1] == -1) {
		CloseHandle(hRead);
		CloseHandle(hWrite);
		errno = EMFILE;
		return -1;
	}

	return 0;
}

static int pipe(int fds[2]) { return pipe2(fds, __SPRT_O_CLOEXEC); }

static char *getcwd(char *buf, size_t bufSize) {
	if (buf && bufSize == 0) {
		errno = EINVAL;
		return nullptr;
	}

	auto requiredBufferLen = GetCurrentDirectoryW(0, nullptr);
	if (requiredBufferLen == 0) {
		errno = EACCES;
		return nullptr;
	}

	auto wbuf = __sprt_typed_malloca(wchar_t, requiredBufferLen + 1);
	auto цbufferLen = GetCurrentDirectoryW(requiredBufferLen + 1, wbuf);
	if (цbufferLen == 0) {
		errno = EACCES;
		return nullptr;
	}

	auto requiredLen = unicode::getUtf8Length(WideStringView((char16_t *)wbuf, цbufferLen)) + 1;

	if (bufSize > 0 && bufSize < requiredLen) {
		errno = ERANGE;
		return nullptr;
	}

	if (!buf) {
		if (bufSize == 0) {
			buf = (char *)malloc(requiredLen);
			bufSize = requiredLen;
		} else {
			buf = (char *)malloc(bufSize);
		}
	}

	size_t retLen = 0;
	unicode::toUtf8(buf, bufSize, WideStringView((char16_t *)wbuf, цbufferLen), &retLen);
	if (retLen < bufSize) {
		buf[retLen] = 0;
	}

	if (!__sprt_fpath_is_posix(buf, retLen)) {
		// convert path in place
		if (__sprt_fpath_to_posix(buf, retLen, buf, bufSize) == 0) {
			*__sprt___errno_location() = EINVAL;
			return nullptr;
		}
	}
	return buf;
}

} // namespace sprt
