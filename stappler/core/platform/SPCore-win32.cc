/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPLog.h"
#include "SPStatus.h"
#include "SPString.h"

#if WIN32
#include "SPPlatformUnistd.h"
#include "SPSharedModule.h"
#include "SPPlatform.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <processenv.h>
#include <securitybaseapi.h>
#include <strsafe.h>
#include <synchapi.h>
#include <winnt.h>
#include <objbase.h>
#include <shlwapi.h>
#include <Shobjidl.h>
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

namespace STAPPLER_VERSIONIZED stappler::platform {

static PSID s_containerId = nullptr;

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

class RandomSequence {
public:
	RandomSequence(void) : hProvider(NULL) {
		if (FALSE == CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL, 0)) {
			// failed, should we try to create a default provider?
			if (NTE_BAD_KEYSET == GetLastError()) {
				if (FALSE
						== CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL,
								CRYPT_NEWKEYSET)) {
					// ensure the provider is NULL so we could use a backup plan
					hProvider = NULL;
				}
			}
		}
	}

	~RandomSequence(void) {
		if (NULL != hProvider) {
			CryptReleaseContext(hProvider, 0U);
		}
	}

	BOOL generate(BYTE *buf, DWORD len) {
		if (NULL != hProvider) {
			return CryptGenRandom(hProvider, len, buf);
		}
		return FALSE;
	}

private:
	HCRYPTPROV hProvider;
};

static auto mapBuffer(WideStringView data, char16_t *buf, size_t count, int flags) {
	return LCMapStringEx(LOCALE_NAME_SYSTEM_DEFAULT, flags, (wchar_t *)data.data(), data.size(),
			(wchar_t *)buf, int(count), nullptr, nullptr, 0);
}

template <typename Interface>
auto mapString(WideStringView data, int flags) {
	auto bufSize = LCMapStringEx(LOCALE_NAME_SYSTEM_DEFAULT, flags, (wchar_t *)data.data(),
			data.size(), (wchar_t *)nullptr, 0, nullptr, nullptr, 0);

	typename Interface::WideStringType ret;
	ret.resize(bufSize);

	mapBuffer(data, ret.data(), ret.size(), flags);

	return ret;
}

template <typename Interface>
auto mapString(StringView data, int flags) {
	return string::toUtf8<Interface>(mapString<Interface>(string::toUtf16<Interface>(data), flags));
}

char32_t tolower(char32_t c) {
	char16_t bufA[2];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_LOWERCASE);
	if (bufSize > 0) {
		return unicode::utf16Decode32(bufB);
	}
	return c;
}

char32_t toupper(char32_t c) {
	char16_t bufA[2];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_UPPERCASE);
	if (bufSize > 0) {
		return unicode::utf16Decode32(bufB);
	}
	return c;
}

char32_t totitle(char32_t c) {
	char16_t bufA[2];
	char16_t bufB[8];

	auto size = unicode::utf16EncodeBuf(bufA, c);
	auto bufSize = mapBuffer(WideStringView(bufA, size), bufB, 8, LCMAP_TITLECASE);
	if (bufSize > 0) {
		return unicode::utf16Decode32(bufB);
	}
	return c;
}

template <>
auto tolower<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	return mapString<memory::PoolInterface>(data, LCMAP_LOWERCASE);
}

template <>
auto tolower<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	return mapString<memory::StandartInterface>(data, LCMAP_LOWERCASE);
}

template <>
auto toupper<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	return mapString<memory::PoolInterface>(data, LCMAP_UPPERCASE);
}

template <>
auto toupper<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	return mapString<memory::StandartInterface>(data, LCMAP_UPPERCASE);
}

template <>
auto totitle<memory::PoolInterface>(StringView data) -> memory::PoolInterface::StringType {
	return mapString<memory::PoolInterface>(data, LCMAP_TITLECASE);
}

template <>
auto totitle<memory::StandartInterface>(StringView data) -> memory::StandartInterface::StringType {
	return mapString<memory::StandartInterface>(data, LCMAP_TITLECASE);
}

template <>
auto tolower<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	return mapString<memory::PoolInterface>(data, LCMAP_LOWERCASE);
}

template <>
auto tolower<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	return mapString<memory::StandartInterface>(data, LCMAP_LOWERCASE);
}

template <>
auto toupper<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	return mapString<memory::PoolInterface>(data, LCMAP_UPPERCASE);
}

template <>
auto toupper<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	return mapString<memory::StandartInterface>(data, LCMAP_UPPERCASE);
}

template <>
auto totitle<memory::PoolInterface>(WideStringView data) -> memory::PoolInterface::WideStringType {
	return mapString<memory::PoolInterface>(data, LCMAP_TITLECASE);
}

template <>
auto totitle<memory::StandartInterface>(WideStringView data)
		-> memory::StandartInterface::WideStringType {
	return mapString<memory::StandartInterface>(data, LCMAP_TITLECASE);
}

int compare_u(StringView l, StringView r) {
	return compare_u(string::toUtf16<memory::StandartInterface>(l),
			string::toUtf16<memory::StandartInterface>(r));
}

int compare_u(WideStringView l, WideStringView r) {
	return CompareStringEx(LOCALE_NAME_SYSTEM_DEFAULT, NORM_LINGUISTIC_CASING, (wchar_t *)l.data(),
			l.size(), (wchar_t *)r.data(), r.size(), NULL, NULL, 0);
}

int caseCompare_u(StringView l, StringView r) {
	return caseCompare_u(string::toUtf16<memory::StandartInterface>(l),
			string::toUtf16<memory::StandartInterface>(r));
}

int caseCompare_u(WideStringView l, WideStringView r) {
	return CompareStringEx(LOCALE_NAME_SYSTEM_DEFAULT,
			NORM_LINGUISTIC_CASING | NORM_IGNORECASE | LINGUISTIC_IGNORECASE, (wchar_t *)l.data(),
			l.size(), (wchar_t *)r.data(), r.size(), NULL, NULL, 0);
}

size_t makeRandomBytes(uint8_t *buf, size_t len) {
	RandomSequence seq;
	if (seq.generate(buf, len)) {
		return len;
	}
	return 0;
}

// based on https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows

static LARGE_INTEGER getFILETIMEoffset() {
	SYSTEMTIME s;
	FILETIME f;
	LARGE_INTEGER t;

	s.wYear = 1'970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return (t);
}

uint64_t clock(ClockType type) {
	if (type == ClockType::Hardware) {
		return __rdtsc();
	}

	LARGE_INTEGER t;
	static LARGE_INTEGER offset;
	static int64_t frequencyToMicroseconds;
	static int initialized = 0;
	static BOOL usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter(&offset);
			frequencyToMicroseconds = performanceFrequency.QuadPart / 1'000'000;
		} else {
			offset = getFILETIMEoffset();
			frequencyToMicroseconds = 10;
		}
	}

	if (usePerformanceCounter) {
		QueryPerformanceCounter(&t);
	} else {
		FILETIME f;
		GetSystemTimeAsFileTime(&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	return t.QuadPart / frequencyToMicroseconds;
}

void sleep(uint64_t microseconds) {
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10
			* microseconds); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}

bool allowNamedObjectAccess(PSID appContainerSid, PWSTR name, SE_OBJECT_TYPE type,
		ACCESS_MASK accessMask) {
	PACL oldAcl, newAcl = nullptr;
	DWORD status;
	EXPLICIT_ACCESS access;
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

bool initialize(int &resultCode) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	int appPathCommon = 0;
	if (auto v = SharedModule::acquireTypedSymbol<int *>(buildconfig::MODULE_APPCONFIG_NAME,
				"APPCONFIG_APP_PATH_COMMON")) {
		appPathCommon = *v;
	}

	bool isAppContainer = false;
	wil::get_token_is_app_container_nothrow(nullptr, isAppContainer);

	if (isAppContainer || appPathCommon < 2) {
		return true;
	}

	auto appConfigName = getAppconfigAppName();
	auto appConfigBundleName = getAppconfigBundleName();
	if (!isAppContainer && appConfigName && appConfigBundleName) {
		char16_t profileName[64] = {0};
		char16_t publicName[512] = {0};
		const wchar_t *desc = L"Stappler Application";

		unicode::toUtf16(profileName, 63, appConfigBundleName);
		unicode::toUtf16(publicName, 511, appConfigName);

		HRESULT hr;

		//hr = ::DeleteAppContainerProfile((const wchar_t *)profileName);
		//if (!SUCCEEDED(hr)) {
		//	log::source().warn("core", "Fail to delete temporary profile");
		//}

		hr = ::CreateAppContainerProfile((const wchar_t *)profileName, (const wchar_t *)publicName,
				desc, nullptr, 0, &s_containerId);
		if (!SUCCEEDED(hr)) {
			if (hr == E_ACCESSDENIED) {
				log::source().warn("core", "Fail to create temporary profile: E_ACCESSDENIED");
			} else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
				hr = ::DeriveAppContainerSidFromAppContainerName((const wchar_t *)profileName,
						&s_containerId);
				if (!SUCCEEDED(hr)) {
					log::source().warn("core",
							"Fail to create temporary profile: ERROR_ALREADY_EXISTS");
				}
			} else if (hr == E_INVALIDARG) {
				log::source().warn("core", "Fail to create temporary profile: E_INVALIDARG");
			}
		} else {
			wchar_t *commonDirPath = nullptr;
			for (auto &it : s_knownFoldersToAllow) {
				SHGetKnownFolderPath(*it, 0, nullptr, &commonDirPath);
				if (commonDirPath) {
					allowNamedObjectAccess(s_containerId, commonDirPath, SE_FILE_OBJECT,
							FILE_GENERIC_READ);
					CoTaskMemFree(commonDirPath);
					commonDirPath = nullptr;
				}
			}
		}
	}

	if (appPathCommon == 2) {
		// only use container for paths
		return true;
	}

	// run self in app container
	auto capsCount = sizeof(s_defaultAppContainerCaps) / sizeof(DWORD);

	SID_AND_ATTRIBUTES capsAttrs[capsCount];
	SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;

	int i = 0;
	for (auto &it : s_defaultAppContainerCaps) {
		if (!AllocateAndInitializeSid(&authority, SECURITY_BUILTIN_CAPABILITY_RID_COUNT,
					SECURITY_CAPABILITY_BASE_RID, it, 0, 0, 0, 0, 0, 0, &capsAttrs[i].Sid)) {
			log::source().warn("core", "Fail to allocate capability SID");
		}
		capsAttrs[i].Attributes = SE_GROUP_ENABLED;
		++i;
	}

	// Run self in container
	SECURITY_CAPABILITIES sc;
	sc.AppContainerSid = s_containerId;
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
		log::source().error("core", "Fail to update proc attributes for AppContainer: ",
				status::lastErrorToStatus(GetLastError()));
		return false;
	}

	wchar_t fullpath[NTFS_MAX_PATH] = {0};
	GetModuleFileNameW(NULL, fullpath, NTFS_MAX_PATH - 1);

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
		log::source().error("core",
				"Fail to create AppContainer process: ", status::lastErrorToStatus(lastError));
		resultCode = -1'024;
	}

	DeleteProcThreadAttributeList(si.lpAttributeList);
	for (auto &it : capsAttrs) { FreeSid(it.Sid); }

	return false;
}

void terminate() {
	if (s_containerId) {
		FreeSid(s_containerId);
	}

	CoUninitialize();
}

uint32_t getMemoryPageSize() {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
}

StringView getOsLocale() {
	static char locale[32] = {0};
	static wchar_t wlocale[32] = {0};
	auto len = ::GetUserDefaultLocaleName(wlocale, 32);

	auto writePtr = locale;
	auto ptr = (char16_t *)wlocale;
	auto end = ptr + len - 1;
	while (*ptr && ptr < end) {
		uint8_t offset = 0;
		auto c = unicode::utf16Decode32(ptr, offset);
		if (offset > 0) {
			writePtr += unicode::utf8EncodeBuf(writePtr, c);
			ptr += offset;
		} else {
			break;
		}
	}
	return StringView(locale);
}

} // namespace stappler::platform

#endif
