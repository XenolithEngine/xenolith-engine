/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#define __SPRT_BUILD 1
#define __SPRT_USE_STL 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_winapi.h>

#include "private/SPRTPrivate.h"

#if __SPRT_CONFIG_HAVE_WINAPI
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <userenv.h>

#include <Shlobj.h>
#include <sddl.h>
#include <accctrl.h>
#include <aclapi.h>

#include <wil/result_macros.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/com.h>
#include <wil\token_helpers.h>

#endif

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/log.h>

#undef __deallocate

#include "private/SPRTSpecific.h"

namespace sprt {

#if __SPRT_CONFIG_HAVE_WINAPI

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)() { return GetCurrentThreadId(); }

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)() { return GetLastError(); }

__SPRT_C_FUNC int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	return WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}
__SPRT_C_FUNC void __SPRT_ID(WakeByAddressSingle)(void *Address) { WakeByAddressSingle(Address); }

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressAll)(void *Address) { WakeByAddressAll(Address); }

__SPRT_C_FUNC int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr,
		int cchDest) {
	return LCMapStringEx((const wchar_t *)lpLocaleName, dwMapFlags, (const wchar_t *)lpSrcStr,
			cchSrc, (wchar_t *)lpDestStr, cchDest, nullptr, nullptr, 0);
}

__SPRT_C_FUNC int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpString1, int cchCount1,
		const char16_t *lpString2, int cchCount2) {
	return CompareStringEx((const wchar_t *)lpLocaleName, dwMapFlags, (wchar_t *)lpString1,
			cchCount1, (wchar_t *)lpString2, cchCount2, NULL, NULL, 0);
}

__SPRT_C_FUNC int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize) {
	return GetUserDefaultLocaleName((wchar_t *)wlocale, bufSize);
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToAscii)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return IdnToAscii(dwFlags, (const wchar_t *)lpUnicodeCharStr, cchUnicodeChar,
			(wchar_t *)lpASCIICharStr, cchASCIIChar);
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToUnicode)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return IdnToUnicode(dwFlags, (const wchar_t *)lpUnicodeCharStr, cchUnicodeChar,
			(wchar_t *)lpASCIICharStr, cchASCIIChar);
}

__SPRT_C_FUNC __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char *lpBuffer) {
	return GetCurrentDirectoryA(nBufferLength, lpBuffer);
}

#else
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)() {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)() {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressSingle)(void *Address) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
}

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressAll)(void *Address) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
}

__SPRT_C_FUNC int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr,
		int cchDest) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpString1, int cchCount1,
		const char16_t *lpString2, int cchCount2) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToAscii)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToUnicode)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char *lpBuffer) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

#endif

} // namespace sprt

#if __SPRT_CONFIG_HAVE_WINAPI

/*
	AI-Generated
*/

// WSA error code constants (from winsock2.h)
// Note: WSAGetLastError() is used for socket errors, but we map them here for completeness
#ifndef WSAECONNRESET
#define WSAECONNRESET 10'054
#define WSAENETDOWN 10'050
#define WSAENOTCONN 10'057
#define WSAEISCONN 10'056
#define WSAEAFNOSUPPORT 10'047
#define WSAEMSGSIZE 10'040
#define WSAEDESTADDRREQ 10'039
#define WSAENOPROTOOPT 10'042
#define WSAESOCKTNOSUPPORT 10'044
#define WSAEPFNOSUPPORT 10'046
#define WSAEHOSTDOWN 10'064
#define WSAETOOMANYREFS 10'059
#endif

namespace sprt::platform {

int lastErrorToErrno(unsigned long winerr) {
	switch (winerr) {
	// Success
	case ERROR_SUCCESS: return 0;

	// File and path errors
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_BAD_PATHNAME:
	case ERROR_INVALID_DRIVE:
	case ERROR_BAD_NETPATH:
	case ERROR_BAD_NET_NAME:
	case ERROR_FILENAME_EXCED_RANGE:
		return ENOENT; // No such file or directory

	// Permission and access errors
	case ERROR_ACCESS_DENIED:
	case ERROR_LOCK_VIOLATION:
	case ERROR_CANNOT_MAKE:
	case ERROR_NETWORK_ACCESS_DENIED:
		return EACCES; // Permission denied

	// Sharing violation (can be access denied or text file busy)
	case ERROR_SHARING_VIOLATION:
		return ETXTBSY; // Text file busy (sharing violation often indicates file is locked/in use)

	// Invalid handle/file descriptor
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_ACCESS:
		return EBADF; // Bad file descriptor

	// Memory errors
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
	case ERROR_ARENA_TRASHED:
		return ENOMEM; // Out of memory

	// Invalid parameter/argument
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_DATA:
	case ERROR_BAD_LENGTH:
	case ERROR_INVALID_NAME:
	case ERROR_BAD_FORMAT:
		return EINVAL; // Invalid argument

	// File already exists
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return EEXIST; // File exists

	// Too many files
	case ERROR_TOO_MANY_OPEN_FILES: return EMFILE; // Too many open files
	case ERROR_NO_PROC_SLOTS:
		return EAGAIN; // Resource temporarily unavailable

	// I/O errors
	case ERROR_WRITE_FAULT:
	case ERROR_READ_FAULT:
	case ERROR_SEEK:
	case ERROR_BAD_COMMAND:
	case ERROR_CRC:
	case ERROR_DISK_CHANGE:
	case ERROR_GEN_FAILURE:
	case ERROR_IO_DEVICE:
	case ERROR_IO_INCOMPLETE:
	case ERROR_BAD_NET_RESP:
	case ERROR_NET_WRITE_FAULT:
		return EIO; // I/O error

	// Not ready (could be I/O error or no medium)
	case ERROR_NOT_READY:
		return ENOMEDIUM; // No medium found (or device not ready)

	// No space
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		return ENOSPC; // No space left on device

	// Broken pipe
	case ERROR_BROKEN_PIPE:
		return EPIPE; // Broken pipe

	// Device errors
	case ERROR_BAD_UNIT:
	case ERROR_NOT_CONNECTED:
		return ENODEV; // No such device

	// Network errors
	case ERROR_NETWORK_BUSY: return EBUSY; // Device or resource busy
	case ERROR_NETNAME_DELETED: return ENOENT; // No such file or directory
	case ERROR_NETWORK_UNREACHABLE: return ENETUNREACH; // Network is unreachable
	case ERROR_HOST_UNREACHABLE: return EHOSTUNREACH; // Host is unreachable
	case ERROR_CONNECTION_REFUSED: return ECONNREFUSED; // Connection refused
	case ERROR_CONNECTION_ABORTED: return ECONNABORTED; // Connection aborted
	case WSAECONNRESET: return ECONNRESET; // Connection reset by peer
	case WSAENETDOWN:
		return ENETDOWN; // Network is down

	// Timeout
	case ERROR_SEM_TIMEOUT:
	case WAIT_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Socket errors
	case WSAENOTCONN: return ENOTCONN; // Transport endpoint is not connected
	case WSAEISCONN: return EISCONN; // Transport endpoint is already connected
	case ERROR_TOO_MANY_NAMES:
		return EMFILE; // Too many open files

	// Directory errors
	case ERROR_DIRECTORY:
	case ERROR_CURRENT_DIRECTORY: return ENOTDIR; // Not a directory
	case ERROR_DIR_NOT_EMPTY: return ENOTEMPTY; // Directory not empty
	case ERROR_DIRECTORY_NOT_SUPPORTED:
		return ENOSYS; // Function not implemented

	// Is a directory
	case ERROR_CANTOPEN:
		return EISDIR; // Is a directory

	// Operation in progress
	case ERROR_OPERATION_IN_PROGRESS:
		return EINPROGRESS; // Operation now in progress

	// Already in progress
	case ERROR_ALREADY_ASSIGNED:
		return EALREADY; // Operation already in progress

	// Interrupted
	case ERROR_INVALID_AT_INTERRUPT_TIME:
		return EINTR; // Interrupted system call

	// Canceled
	case ERROR_OPERATION_ABORTED:
	case ERROR_CANCELLED:
		return ECANCELED; // Operation canceled

	// Device or resource busy
	case ERROR_BUSY:
	case ERROR_BUSY_DRIVE:
	case ERROR_DEVICE_IN_USE:
		return EBUSY; // Device or resource busy

	// Function not implemented
	case ERROR_CALL_NOT_IMPLEMENTED:
		return ENOSYS; // Function not implemented

	// Address family not supported
	case WSAEAFNOSUPPORT:
		return EAFNOSUPPORT; // Address family not supported by protocol

	// Operation not supported
	case ERROR_NOT_SUPPORTED:
		return EOPNOTSUPP; // Operation not supported on transport endpoint

	// Address already in use
	case ERROR_ADDRESS_ALREADY_ASSOCIATED:
		return EADDRINUSE; // Address already in use

	// Address not available
	case ERROR_ADDRESS_NOT_ASSOCIATED:
		return EADDRNOTAVAIL; // Cannot assign requested address

	// Message too long
	case WSAEMSGSIZE:
		return EMSGSIZE; // Message too long

	// Protocol errors
	case ERROR_PROTOCOL_UNREACHABLE:
		return ENOLINK; // Link has been severed (protocol unreachable)

	// Would block / pending operations
	case ERROR_IO_PENDING:
		return EWOULDBLOCK; // Operation would block

	// Buffer overflow
	case ERROR_INSUFFICIENT_BUFFER: return ENAMETOOLONG; // File name too long
	case ERROR_BUFFER_OVERFLOW:
		return ENAMETOOLONG; // File name too long

	// Semaphore errors
	case ERROR_TOO_MANY_SEMAPHORES: return ENFILE; // Too many open files in system
	case ERROR_EXCL_SEM_ALREADY_OWNED: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_IS_SET: return EAGAIN; // Resource temporarily unavailable
	case ERROR_TOO_MANY_SEM_REQUESTS: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_OWNER_DIED:
		return EOWNERDEAD; // Owner died

	// Mutex errors (using same mapping as semaphore)

	// Not same device
	case ERROR_NOT_SAME_DEVICE:
		return EXDEV; // Cross-device link

	// Read-only file system
	case ERROR_WRITE_PROTECT:
		return EROFS; // Read-only file system (write protected)

	// Range error
	case ERROR_INVALID_ADDRESS:
		return ERANGE; // Numerical result out of range

	// Domain error
	case ERROR_INVALID_DOMAIN_ROLE:
	case ERROR_INVALID_DOMAIN_STATE:
		return EDOM; // Numerical argument out of domain

	// No message available
	case ERROR_NO_DATA:
		return ENOMSG; // No message of desired type

	// Identifier removed
	case ERROR_NO_TOKEN:
		return EIDRM; // Identifier removed

	// Invalid handle state / link number out of range
	case ERROR_INVALID_HANDLE_STATE:
		return ELNRNG; // Link number out of range

	// Medium type wrong - ERROR_BAD_MEDIA_TYPE may not be defined in all Windows versions
	// Using ERROR_BAD_UNIT as alternative for media-related errors

	// Socket operation on non-socket
	case ERROR_INVALID_LEVEL:
		return ENOTSOCK; // Socket operation on non-socket

	// Destination address required
	case WSAEDESTADDRREQ:
		return EDESTADDRREQ; // Destination address required

	// Protocol not available
	case WSAENOPROTOOPT:
		return ENOPROTOOPT; // Protocol not available

	// Socket type not supported
	case WSAESOCKTNOSUPPORT:
		return ESOCKTNOSUPPORT; // Socket type not supported

	// Protocol family not supported
	case WSAEPFNOSUPPORT:
		return EPFNOSUPPORT; // Protocol family not supported

	// Oplock not granted
	case ERROR_OPLOCK_NOT_GRANTED:
		return EACCES; // Permission denied

	// Connection timed out
	case ERROR_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Connection refused
	case ERROR_CONNECTION_UNAVAIL:
		return ECONNREFUSED; // Connection refused

	// Host is down
	case WSAEHOSTDOWN:
		return EHOSTDOWN; // Host is down

	// Too many references
	case WSAETOOMANYREFS:
		return ETOOMANYREFS; // Too many references: cannot splice

	// Stale file handle
	case ERROR_FILE_CORRUPT:
	case ERROR_DISK_CORRUPT:
		return ESTALE; // Stale file handle

	// Default: return generic error
	default: return EINVAL; // Invalid argument (as a generic fallback)
	}
	return EINVAL;
}

static DWORD s_defaultAppContainerCaps[] = {
	SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER,
	SECURITY_CAPABILITY_PICTURES_LIBRARY,
	SECURITY_CAPABILITY_VIDEOS_LIBRARY,
	SECURITY_CAPABILITY_MUSIC_LIBRARY,
	SECURITY_CAPABILITY_DOCUMENTS_LIBRARY,
	SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES,
	SECURITY_CAPABILITY_REMOVABLE_STORAGE,
};

static const KNOWNFOLDERID *s_knownFoldersToAllow[] = {&FOLDERID_Profile, &FOLDERID_Public};

struct StaticInit {
	StaticInit() {
		initResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		wil::get_token_is_app_container_nothrow(nullptr, isAppContainer);
	}

	~StaticInit() {
		if (!SUCCEEDED(initResult)) {
			CoUninitialize();
		}

		if (containerId) {
			FreeSid(containerId);
		}
	}

	HRESULT initResult;
	PSID containerId = nullptr;
	bool isAppContainer = false;
};

static StaticInit s_staticInit __attribute__((used));

bool isAppContainer() { return s_staticInit.isAppContainer; }

void destroyAppContainer(StringView appConfigBundleName) {
	char16_t profileName[64] = {0};
	unicode::toUtf16(profileName, 63, appConfigBundleName);

	auto hr = ::DeleteAppContainerProfile((const wchar_t *)profileName);
	if (!SUCCEEDED(hr)) {
		log::vpwarn(__SPRT_LOCATION, "rt-libc",
				"Fail to delete app profile: ", appConfigBundleName);
	}
}

static bool allowNamedObjectAccess(PSID appContainerSid, PWSTR name, SE_OBJECT_TYPE type,
		ACCESS_MASK accessMask) {
	PACL oldAcl, newAcl = nullptr;
	DWORD status;
	EXPLICIT_ACCESSW access;
	do {
		access.grfAccessMode = GRANT_ACCESS;
		access.grfAccessPermissions = accessMask;
		access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
		access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		access.Trustee.pMultipleTrustee = nullptr;
		access.Trustee.ptstrName = (PWSTR)appContainerSid;
		access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		access.Trustee.TrusteeType = TRUSTEE_IS_GROUP;

		status = ::GetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				&oldAcl, nullptr, nullptr);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetEntriesInAclW(1, &access, oldAcl, &newAcl);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				newAcl, nullptr);
		if (status != ERROR_SUCCESS) {
			break;
		}
	} while (false);

	if (newAcl) {
		::LocalFree(newAcl);
	}

	return status == ERROR_SUCCESS;
}

bool initAppContainer(StringView appConfigBundleName, StringView appConfigName) {
	char16_t profileName[64] = {0};
	char16_t publicName[512] = {0};
	const wchar_t *desc = L"Stappler Application";

	unicode::toUtf16(profileName, 63, appConfigBundleName);
	unicode::toUtf16(publicName, 511, appConfigName);

	HRESULT hr = ::CreateAppContainerProfile((const wchar_t *)profileName,
			(const wchar_t *)publicName, desc, nullptr, 0, &s_staticInit.containerId);
	if (!SUCCEEDED(hr)) {
		if (hr == E_ACCESSDENIED) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_ACCESSDENIED");
		} else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
			hr = ::DeriveAppContainerSidFromAppContainerName((const wchar_t *)profileName,
					&s_staticInit.containerId);
			if (!SUCCEEDED(hr)) {
				log::vpwarn(__SPRT_LOCATION, "rt-libc",
						"Fail to create temporary profile: ERROR_ALREADY_EXISTS");
			}
			return true;
		} else if (hr == E_INVALIDARG) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_INVALIDARG");
		}
		return false;
	} else {
		wchar_t *commonDirPath = nullptr;
		for (auto &it : s_knownFoldersToAllow) {
			SHGetKnownFolderPath(*it, 0, nullptr, &commonDirPath);
			if (commonDirPath) {
				allowNamedObjectAccess(s_staticInit.containerId, commonDirPath, SE_FILE_OBJECT,
						FILE_GENERIC_READ);
				CoTaskMemFree(commonDirPath);
				commonDirPath = nullptr;
			}
		}
		return true;
	}
}

bool runSelfInContainer(int &resultCode) {
	// run self in app container
	constexpr auto capsCount = sizeof(s_defaultAppContainerCaps) / sizeof(DWORD);

	SID_AND_ATTRIBUTES capsAttrs[capsCount];
	SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;

	int i = 0;
	for (auto &it : s_defaultAppContainerCaps) {
		if (!AllocateAndInitializeSid(&authority, SECURITY_BUILTIN_CAPABILITY_RID_COUNT,
					SECURITY_CAPABILITY_BASE_RID, it, 0, 0, 0, 0, 0, 0, &capsAttrs[i].Sid)) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc", "Fail to allocate capability SID");
		}
		capsAttrs[i].Attributes = SE_GROUP_ENABLED;
		++i;
	}

	// Run self in container
	SECURITY_CAPABILITIES sc;
	sc.AppContainerSid = s_staticInit.containerId;
	sc.Capabilities = nullptr;
	sc.CapabilityCount = 0;
	sc.Reserved = 0;
	sc.Capabilities = capsAttrs;
	sc.CapabilityCount = capsCount;

	STARTUPINFOEXW si;
	memset(&si, 0, sizeof(STARTUPINFOEXW));
	si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	SIZE_T AttributesSize;
	InitializeProcThreadAttributeList(NULL, 1, NULL, &AttributesSize);
	si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			AttributesSize);
	InitializeProcThreadAttributeList(si.lpAttributeList, 1, NULL, &AttributesSize);

	if (!::UpdateProcThreadAttribute(si.lpAttributeList, 0,
				PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &sc, sizeof(SECURITY_CAPABILITIES),
				nullptr, nullptr)) {
		log::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to update proc attributes for AppContainer: ",
				status::lastErrorToStatus(GetLastError()));
		return false;
	}

	wchar_t fullpath[32'768] = {0};
	fullpath[GetModuleFileNameW(NULL, fullpath, 32'768)] = 0;

	auto commandLine = GetCommandLineW();

	BOOL created = ::CreateProcessW(fullpath, commandLine, nullptr, nullptr, TRUE,
			EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, (LPSTARTUPINFOW)&si, &pi);

	if (created) {
		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);

		resultCode = code;
	} else {
		auto lastError = GetLastError();
		log::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to create AppContainer process: ", status::lastErrorToStatus(lastError));
		resultCode = -1'024;
	}

	DeleteProcThreadAttributeList(si.lpAttributeList);
	for (auto &it : capsAttrs) { FreeSid(it.Sid); }

	return false;
}

bool getAppPath(const callback<void(StringView)> &cb) {
	wchar_t fullpath[32'768] = {0};
	auto n = GetModuleFileNameW(NULL, fullpath, 32'768);

	if (n == 0) {
		return false;
	}

	unicode::toUtf8(cb, WideStringView((char16_t *)fullpath, n));
	return true;
}

bool getHomePath(const callback<void(StringView)> &cb) {
	PWSTR pPath = NULL;
	// Use FOLDERID_Documents to get the path to the Documents folder
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &pPath);
	if (SUCCEEDED(hr)) {
		unicode::toUtf8(cb, WideStringView((char16_t *)pPath));
		CoTaskMemFree(pPath); // Free memory allocated by the function
		return true;
	}
	return false;
}

bool getMachineId(const callback<void(StringView)> &cb) {
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey)
			!= ERROR_SUCCESS) {
		throw std::runtime_error("Could not open registry key");
	}

	DWORD type;
	DWORD dataSize = 0;
	// Call RegGetValue once to get the buffer size
	LONG result =
			RegGetValueA(hKey, nullptr, "MachineGuid", RRF_RT_REG_SZ, &type, nullptr, &dataSize);

	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Allocate a buffer of the required size
	auto buf = (char *)__sprt_malloca(dataSize + 1);
	result = RegGetValueA(hKey, nullptr, "MachineGuid", RRF_RT_REG_SZ, &type, buf, &dataSize);

	if (result == ERROR_SUCCESS) {
		cb(StringView(buf, dataSize));
	}

	RegCloseKey(hKey);

	__sprt_freea(buf);
	return false;
}

} // namespace sprt::platform

#undef interface

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include "private/SPRTFilesystem.h"

namespace sprt::filesystem::detail {

struct KnownFolderInfo {
	const KNOWNFOLDERID *folder;
	LocationCategory category;
	LookupFlags flags;
};

static KnownFolderInfo s_defaultKnownFolders[] = {
	KnownFolderInfo{&FOLDERID_AppDataDesktop, LocationCategory::UserDesktop, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_Desktop, LocationCategory::UserDesktop, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDesktop, LocationCategory::UserDesktop, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Pictures, LocationCategory::UserPictures, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicPictures, LocationCategory::UserPictures, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Videos, LocationCategory::UserVideos, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicVideos, LocationCategory::UserVideos, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Music, LocationCategory::UserMusic, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicMusic, LocationCategory::UserMusic, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Downloads, LocationCategory::UserDownload, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDownloads, LocationCategory::UserDownload, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Documents, LocationCategory::UserDocuments, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDocuments, LocationCategory::UserDocuments,
		LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Profile, LocationCategory::UserHome, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_Public, LocationCategory::UserHome, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Fonts, LocationCategory::Fonts, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_InternetCache, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_LocalAppData, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_RoamingAppData, LocationCategory::CommonData, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_ProgramData, LocationCategory::CommonData, LookupFlags::Shared},
};

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	} else {
		char *buf = nullptr;
		size_t size = 0;
		if (_dupenv_s(&buf, &size, key.data()) == 0) {
			return StringView(buf, size).pdup(pool);
		}
	}
	return StringView();
}

static void processKnownDir(LookupData &data, const KnownFolderInfo &info, IKnownFolder *dir) {
	KNOWNFOLDER_DEFINITION def;
	dir->GetFolderDefinition(&def);

	wchar_t *pathAppWide = nullptr;

	auto dirFlagsAppWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
			| KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET | KF_FLAG_CREATE;

	dir->GetPath(dirFlagsAppWide, &pathAppWide);

	if (pathAppWide) {
		unicode::toUtf8([&](StringView uPath) {
			auto len = __sprt_fpath_to_posix(uPath.data(), uPath.size(), (char *)uPath.data(),
					uPath.size() + 1);


			auto &res = data._resourceLocations[toInt(info.category)];

			res.paths.emplace_back(LocationInfo{
				.path = StringView(uPath.data(), len).pdup(data._pool),
				.lookupType = info.flags,
				.locationFlags = LocationFlags::Locateable,
				.interface = getDefaultInterface(),
			});
			res.init = true;
		}, WideStringView((const char16_t *)pathAppWide));

		CoTaskMemFree(pathAppWide);
	}
}

static void defineAppPathFromCommon(LookupData &data, StringView bundleName) {
	// init with CommonData and CommonCache paths
	auto makeLocation = [&](LocationCategory cat, StringView root, StringView subname) {
		auto &res = data._resourceLocations[toInt(cat)];
		filepath::merge([&](StringView path) {
			res.paths.emplace_back(LocationInfo{
				.path = path.pdup(data._pool),
				.lookupType = LookupFlags::Private | LookupFlags::Writable,
				.locationFlags = LocationFlags::Locateable,
				.interface = getDefaultInterface(),
			});
		}, root, bundleName, subname);
	};

	auto commonData = &data._resourceLocations[toInt(LocationCategory::CommonData)];
	auto commonCache = &data._resourceLocations[toInt(LocationCategory::CommonCache)];

	if (commonCache->paths.empty()) {
		commonCache = commonData;
	}

	makeLocation(LocationCategory::AppData, commonData->paths.front().path, "Data");
	makeLocation(LocationCategory::AppConfig, commonData->paths.front().path, "Config");
	makeLocation(LocationCategory::AppState, commonData->paths.front().path, "State");
	makeLocation(LocationCategory::AppCache, commonCache->paths.front().path, "Cache");
	makeLocation(LocationCategory::AppRuntime, commonCache->paths.front().path, "Runtime");
}

static memory::dynstring getAppContainerPath(PSID sid) {
	memory::dynstring ret;
	PWSTR str = nullptr, path = nullptr;
	::ConvertSidToStringSidW(sid, &str);

	if (SUCCEEDED(::GetAppContainerFolderPath(str, &path))) {
		unicode::toUtf8([&](StringView str) {
			ret.resize(str.size() + 1);
			ret.resize(__sprt_fpath_to_posix(str.data(), str.size(), ret.data(), ret.size()));
		}, WideStringView((const char16_t *)path));
		::CoTaskMemFree(path);
	}
	::LocalFree(str);
	return ret;
}

static memory::dynstring s_containerPath;

void _initSystemPaths(LookupData &data) {
	auto exeecPath = platform::getExecPath();
	auto defaultInterface = getDefaultInterface();

	auto manager = wil::CoCreateInstance<KnownFolderManager, IKnownFolderManager,
			wil::err_returncode_policy>();
	if (manager) {
		HRESULT hr;
		IKnownFolder *pKnownFolder = nullptr;
		for (auto &it : s_defaultKnownFolders) {
			hr = manager->GetFolder(*it.folder, &pKnownFolder);
			if (SUCCEEDED(hr)) {
				processKnownDir(data, it, pKnownFolder);
				pKnownFolder->Release();
			}
		}
	}

	auto &appConfig = getAppConfig();

	if (!appConfig.bundlePath.empty()) {
		auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

		appConfig.bundlePath.split<StringView::Chars<':'>>([&](StringView str) {
			auto value = readVariable(data._pool, str);
			if (!value.empty()) {
				bundledLoc.paths.emplace_back(LocationInfo{
					value,
					LookupFlags::Private,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}
		});
	}

	auto pathEnv = __sprt_getenv("PATH");
	if (pathEnv) {
		auto &res = data._resourceLocations[toInt(LocationCategory::Exec)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(LocationInfo{
				value.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		});
		res.init = true;
	}

	if (appConfig.pathScheme == AppLocationScheme::ExecutableRelative) {
		auto rootPath = filepath::root(exeecPath);
		data.initAppPaths(rootPath);
	} else if (appConfig.pathScheme == AppLocationScheme::SystemRelative) {
		defineAppPathFromCommon(data, appConfig.bundleName);
	} else if (appConfig.pathScheme == AppLocationScheme::ContainerRelative) {
		// first, determine app container path, then - set paths within it

		s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	} else if (appConfig.pathScheme == AppLocationScheme::ForceContainer) {
		// CommonData and CommonCache already within container
		DWORD returnLength = 0;
		TOKEN_APPCONTAINER_INFORMATION info = {nullptr};
		::GetTokenInformation(::GetCurrentThreadEffectiveToken(), TokenAppContainerSid, &info,
				sizeof(TOKEN_APPCONTAINER_INFORMATION), &returnLength);
		if (info.TokenAppContainer) {
			s_containerPath = getAppContainerPath(info.TokenAppContainer);
		} else {
			s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);
		}

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	}
}

void _termSystemPaths(LookupData &data) { }

} // namespace sprt::filesystem::detail

#endif
