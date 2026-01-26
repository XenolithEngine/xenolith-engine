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

// @AI-geerated
static ssize_t pread64(int fd, void *buf, size_t count, off64_t offset) {
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
	HANDLE handle = (HANDLE)_get_osfhandle(fdDir);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleA(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}


	auto buf = (char *)__sprt_malloca(pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleA(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = SetCurrentDirectoryA(buf);

	__sprt_freea(buf);

	if (ret) {
		return 0;
	}

	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

static int fexecve(int __fd, char *const _argv[], char *const __envp[]) __SPRT_NOEXCEPT {
	HANDLE handle = (HANDLE)_get_osfhandle(__fd);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleA(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto buf = (char *)__sprt_malloc(pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleA(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = int(::_execve(buf, _argv, __envp));
	__sprt_free(buf);
	return ret;
}

// @AI-geerated
static long pathconf(const char *path, int name) {
	char root[4] = {path[0], ':', '\\', '\0'}; // e.g., "C:\\"

	switch (name) {
	case __SPRT_PC_LINK_MAX: return 1; // Windows hard links limited [web:375]

	case __SPRT_PC_NAME_MAX:
		DWORD max_comp;
		GetVolumeInformationA(root, NULL, 0, NULL, &max_comp, NULL, NULL, 0);
		return (long)max_comp; // Usually 255 NTFS

	case __SPRT_PC_PATH_MAX: return MAX_PATH; // MAX_PATH legacy; 32K with "\\?\"

	case __SPRT_PC_PIPE_BUF: return 512; // Named pipe granularity

	case __SPRT_PC_CHOWN_RESTRICTED: return 0; // Windows allows "chown" via TakeOwn

	case __SPRT_PC_NO_TRUNC: return 1; // NTFS truncates long names

	case __SPRT_PC_VDISABLE: return (long)'\0';

	case __SPRT_PC_SYNC_IO: return 1; // Supported via FILE_FLAG_WRITE_THROUGH

	case __SPRT_PC_FILESIZEBITS: return 64; // NTFS 64-bit files

	case __SPRT_PC_ALLOC_SIZE_MIN:
		ULARGE_INTEGER avail, total, free;
		GetDiskFreeSpaceExA(root, &avail, &total, &free);
		return (long)avail.QuadPart; // Free bytes proxy

	default: errno = EINVAL; return -1;
	}
}

static long fpathconf(int __fd, int name) {
	HANDLE handle = (HANDLE)_get_osfhandle(__fd);
	if (handle == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	auto pathLen = GetFinalPathNameByHandleA(handle, NULL, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto buf = (char *)__sprt_malloca(pathLen + 1);

	auto writtenLen = GetFinalPathNameByHandleA(handle, buf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = pathconf(buf, name);

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

	PSID sid = pUser->User.Sid;
	if (!GetSidSubAuthorityCount(sid)) {
		__sprt_freea(pUser);
		errno = EINVAL;
		return __SPRT_ID(uid_t)(-1);
	}

	DWORD sub_count = *GetSidSubAuthorityCount(sid);

	auto rid = GetSidSubAuthority(sid, sub_count - 1);
	__sprt_freea(pUser);

	return __SPRT_ID(uid_t)(*rid);
}

static __SPRT_ID(uid_t) geteuid(void) { return getuid(); }

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

	PSID sid = pGroup->PrimaryGroup;
	if (!GetSidSubAuthorityCount(sid)) {
		__sprt_freea(pGroup);
		errno = EINVAL;
		return __SPRT_ID(gid_t)(-1);
	}

	DWORD sub_count = *GetSidSubAuthorityCount(sid);

	auto rid = GetSidSubAuthority(sid, sub_count - 1);
	__sprt_freea(pGroup);

	return __SPRT_ID(gid_t)(*rid);
}

static __SPRT_ID(gid_t) getegid(void) { return getuid(); }

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

static BOOL __isdir(const char *path) {
	DWORD attr = GetFileAttributesA(path);
	return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// @AI-geerated
static memory::dynstring __readlink_str(const char *path) {
#warning Needs heavy testing
	HANDLE h = CreateFileA(path, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		errno = platform::lastErrorToErrno(GetLastError());
		return memory::dynstring();
	}

	auto pathLen = GetFinalPathNameByHandleA(h, NULL, 0, 0);
	if (pathLen == 0) {
		CloseHandle(h);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return memory::dynstring();
	}

	memory::dynstring ret;
	ret.resize(pathLen);

	auto writtenLen = GetFinalPathNameByHandleA(h, ret.data(), pathLen + 1, FILE_NAME_NORMALIZED);
	CloseHandle(h);

	if (writtenLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return memory::dynstring();
	}

	ret.resize(writtenLen - 1);
	return ret;
}

static int link(const char *__from, const char *__to) __SPRT_NOEXCEPT {
	return internal::performWithNativePath(__from, [&](const char *from) {
		// call with native path
		return internal::performWithNativePath(__to, [&](const char *to) {
			memory::dynstring tmpPath;
			DWORD attr = 0;
			do {
				auto attr = GetFileAttributesA(from);
				if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
					tmpPath = __readlink_str(from);
					from = tmpPath.data();
				}
			} while (attr & FILE_ATTRIBUTE_REPARSE_POINT);

			// call with native path
			if (CreateHardLinkA(to, from, nullptr)) {
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
			if (CreateSymbolicLinkA(to, from, __isdir(from) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)) {
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

	DWORD attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		DWORD err = GetLastError();
		errno = (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) ? ENOENT : EACCES;
		return -1;
	}

	// F_OK: exists
	if (mode == __SPRT_F_OK) {
		return 0;
	}

	// Directory? Fail (POSIX: not regular file)
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
		errno = EISDIR;
		return -1;
	}

	DWORD fileFlags = 0;
	if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) && (flags & __SPRT_AT_SYMLINK_NOFOLLOW)) {
		fileFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
	}

	// X_OK: executable (any file)
	if (mode & __SPRT_X_OK) {
		return 0;
	}

	// R_OK: try read-only open
	if (mode & __SPRT_R_OK) {
		HANDLE h = CreateFileA(path, GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | fileFlags, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
			return -1;
		}
		CloseHandle(h);
	}

	// W_OK: try write-only open
	if (mode & __SPRT_W_OK) {
		HANDLE h = CreateFileA(path, GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | fileFlags, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
			return -1;
		}
		CloseHandle(h);
	}

	return 0;
}

static int access(const char *__path, int __mode) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __access(path, __mode, 0); //
	}, -1);
}

static int eaccess(const char *__path, int __mode) { return access(__path, __mode); }

static ssize_t __readlink(const char *path, char *buf, size_t bufsiz) {
	auto strPath = __readlink_str(path);
	if (strPath.empty()) {
		return -1;
	}

	// convert in place
	auto writtenLen = __sprt_fpath_to_posix(strPath.data(), strPath.size(), strPath.data(),
			strPath.size() + 1);

	if (!buf) {
		return writtenLen;
	}

	if (writtenLen <= bufsiz) {
		memcpy(buf, strPath.data(), writtenLen);
		return writtenLen;
	} else {
		memcpy(buf, strPath.data(), bufsiz);
		return bufsiz;
	}
}

static ssize_t readlink(const char *__path, char *buf, size_t bufsiz) {
	return internal::performWithNativePath(__path, [&](const char *path) {
		return __readlink(path, buf, bufsiz); //
	}, ssize_t(-1));
}

// @AI-geerated
static int __unlink(const char *path) {
	// Clear read-only attribute first
	DWORD attr = GetFileAttributesA(path);
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
		if (!SetFileAttributesA(path, attr & ~FILE_ATTRIBUTE_READONLY)) {
			// Still try to delete even if attr change fails
		}
	}

	if (!DeleteFileA(path)) {
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
	if (!RemoveDirectoryA(path)) {
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
	static char username[UNLEN + 1] = {0};
	DWORD username_len = UNLEN + 1;
	GetUserNameA(username, &username_len);
	return username;
}

static int getlogin_r(char *__name, size_t __name_len) {
	DWORD username_len = __name_len;
	if (GetUserNameA(__name, &username_len)) {
		return 0;
	}
	errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

// @AI-geerated
static int getdomainname(char *name, size_t namelen) {
	if (namelen == 0) {
		errno = EINVAL;
		return -1;
	}

	DWORD size = (DWORD)namelen;
	if (!GetComputerNameExA(ComputerNameDnsDomain, name, &size)) {
		DWORD err = GetLastError();
		switch (err) {
		case ERROR_MORE_DATA: errno = ENOSPC; break;
		default: errno = platform::lastErrorToErrno(GetLastError());
		}
		return -1;
	}

	return 0;
}

// @AI-geerated
static int fsync(int fd) {
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
	HANDLE h = CreateFileA(path, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
			length == 0 ? TRUNCATE_EXISTING : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

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
	if (!platform::openAtPath(__new_dir_fd, __new_path, [&](const char *path, size_t size) {
		ret = internal::performWithNativePath(__old_path, [&](const char *from) {
			if (CreateSymbolicLinkA(path, __old_path,
						__isdir(__old_path) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)) {
				return 0;
			} else {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}
		}, -1);
	}, platform::FdHandleType::File)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static ssize_t readlinkat(int __dir_fd, const char *__path, char *__buf, size_t __buf_size) {
	int ret = 0;
	if (!platform::openAtPath(__dir_fd, __path, [&](const char *path, size_t size) {
		ret = __readlink(path, __buf, __buf_size);
	}, platform::FdHandleType::File)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int faccessat(int __dirfd, const char *__path, int __mode, int __flags) {
	int ret = 0;
	if (!platform::openAtPath(__dirfd, __path, [&](const char *path, size_t size) {
		ret = __access(path, __mode, __flags);
	}, platform::FdHandleType::File)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

static int linkat(int __old_dir_fd, const char *__old_path, int __new_dir_fd,
		const char *__new_path, int __flags) {
	int ret = 0;
	if (!platform::openAtPath(__old_dir_fd, __old_path, [&](const char *from, size_t size) {
		if (!platform::openAtPath(__new_dir_fd, __new_path, [&](const char *to, size_t size) {
			memory::dynstring tmpPath;
			if (__flags & __SPRT_AT_SYMLINK_FOLLOW) {
				DWORD attr = 0;
				do {
					attr = GetFileAttributesA(from);
					if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
						tmpPath = __readlink_str(from);
						from = tmpPath.data();
					}
				} while (attr & FILE_ATTRIBUTE_REPARSE_POINT);
			}

			// call with native path
			if (CreateHardLinkA(to, from, nullptr)) {
				ret = 0;
			} else {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				ret = -1;
			}
		}, platform::FdHandleType::File)) {
			__sprt_errno = EINVAL;
			ret = -1;
			return;
		}
	}, platform::FdHandleType::File)) {
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
	}, platform::FdHandleType::File)) {
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

} // namespace sprt
