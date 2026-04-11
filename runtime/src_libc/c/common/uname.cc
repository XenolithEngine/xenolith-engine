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

#define __SPRT_BUILD 1

#include <sprt/c/sys/__sprt_utsname.h>

#ifndef SPRT_WINDOWS
#include <sys/utsname.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	return ::uname((struct utsname *)buf);
}

} // namespace sprt

#else // SPRT_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>

namespace sprt {

static bool __queryRegisterVersion(struct __SPRT_UTSNAME_NAME *buf) {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
				KEY_READ, &hKey)
			== ERROR_SUCCESS) {
		auto wbuf = __sprt_typed_malloca(wchar_t, 256);

		DWORD type;
		DWORD dataSize = 0;
		LONG result = 0;

		result = RegGetValueW(hKey, nullptr, L"ProductName", RRF_RT_REG_SZ, &type, nullptr,
				&dataSize);
		if (result == ERROR_SUCCESS) {
			result = RegGetValueW(hKey, nullptr, L"ProductName", RRF_RT_REG_SZ, &type, wbuf,
					&dataSize);

			unicode::toUtf8(buf->release, 64, WideStringView((char16_t *)wbuf, dataSize));
		}

		result = RegGetValueW(hKey, nullptr, L"DisplayVersion", RRF_RT_REG_SZ, &type, nullptr,
				&dataSize);
		if (result == ERROR_SUCCESS) {
			result = RegGetValueW(hKey, nullptr, L"DisplayVersion", RRF_RT_REG_SZ, &type, wbuf,
					&dataSize);

			unicode::toUtf8(buf->version, 64, WideStringView((char16_t *)wbuf, dataSize));
		}

		__sprt_freea(wbuf);
		RegCloseKey(hKey);
		return true;
	}
	return false;
}

static bool __queryWmiVersion(struct __SPRT_UTSNAME_NAME *buf) {
	auto hres = CoInitializeSecurity(NULL,
			-1, // COM authentication
			NULL, // Authentication services
			NULL, // Reserved
			RPC_C_AUTHN_LEVEL_DEFAULT, // Default authentication
			RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
			NULL, // Authentication info
			EOAC_NONE, // Additional capabilities
			NULL // Reserved
	);

	if (FAILED(hres)) {
		return false;
	}

	IWbemLocator *pLoc = NULL;

	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
			(LPVOID *)&pLoc);

	if (FAILED(hres)) {
		return false;
	}

	// Connect to WMI through the IWbemLocator::ConnectServer method
	IWbemServices *pSvc = NULL;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
			NULL, // User name. NULL = current user
			NULL, // User password. NULL = current
			0, // Locale. NULL indicates current
			NULL, // Security flags.
			0, // Authority (for example, Kerberos)
			0, // Context object
			&pSvc // pointer to IWbemServices proxy
	);

	if (FAILED(hres)) {
		pLoc->Release();
		return false;
	}

	// Set security levels on the proxy -------------------------

	hres = CoSetProxyBlanket(pSvc, // Indicates the proxy to set
			RPC_C_AUTHN_WINNT, // RPC_C_AUTHN_xxx
			RPC_C_AUTHZ_NONE, // RPC_C_AUTHZ_xxx
			NULL, // Server principal name
			RPC_C_AUTHN_LEVEL_CALL, // RPC_C_AUTHN_LEVEL_xxx
			RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
			NULL, // client identity
			EOAC_NONE // proxy capabilities
	);

	if (FAILED(hres)) {
		pSvc->Release();
		pLoc->Release();
		return false;
	}

	// Use the IWbemServices pointer to make requests of WMI ----

	// For example, get the name of the operating system
	IEnumWbemClassObject *pEnumerator = NULL;

	hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_OperatingSystem"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

	if (SUCCEEDED(hres)) {
		IWbemClassObject *pclsObj = NULL;
		ULONG uReturn = 0;

		// @AI-Generated
		while (pEnumerator) {
			HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

			if (0 == uReturn) {
				break;
			}

			// Iterate through all property names in this object
			hr = pclsObj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
			if (SUCCEEDED(hr)) {
				BSTR strName = NULL;
				VARIANT vtProp;
				VariantInit(&vtProp);
				while (pclsObj->Next(0, &strName, &vtProp, NULL, NULL) == S_OK) {
					if (wcscmp(strName, L"Caption") == 0 && vtProp.vt == VT_BSTR) {
						unicode::toUtf8(buf->release, 64,
								WideStringView((char16_t *)vtProp.bstrVal));
					} else if (wcscmp(strName, L"Version") == 0 && vtProp.vt == VT_BSTR) {
						if (buf->version[0] != 0) {
							// preserve version string
							char preserveVersion[65];
							memcpy(preserveVersion, buf->version, 65);

							size_t offset = 0;
							unicode::toUtf8(buf->version, 64,
									WideStringView((char16_t *)vtProp.bstrVal), &offset);
							auto target = &buf->version[offset];
							auto bufSize = 64 - offset;

							target = strappend(target, &bufSize, " (", 2);
							target = strappend(target, &bufSize, preserveVersion,
									strlen(preserveVersion));
							target = strappend(target, &bufSize, ")", 1);
						} else {
							unicode::toUtf8(buf->version, 64,
									WideStringView((char16_t *)vtProp.bstrVal));
						}
					}
					/*wprintf(L"  %s: ", strName);
				if (vtProp.vt == VT_BSTR && vtProp.bstrVal) {
					wprintf(L"%s", vtProp.bstrVal);
				} else if (vtProp.vt == VT_I4) {
					wprintf(L"%ld", vtProp.lVal);
				} else if (vtProp.vt == VT_UI4) {
					wprintf(L"%lu", vtProp.ulVal);
				} else if (vtProp.vt == VT_BOOL) {
					wprintf(L"%s", vtProp.boolVal ? L"True" : L"False");
				}
				wprintf(L"\n");*/
					SysFreeString(strName);
					VariantClear(&vtProp);
					VariantInit(&vtProp);
				}
				VariantClear(&vtProp);
				pclsObj->EndEnumeration();
			}

			pclsObj->Release();
		}
		pEnumerator->Release();
		// end @AI-Generated
	}

	hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_ComputerSystem "),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

	if (SUCCEEDED(hres)) {
		IWbemClassObject *pclsObj = NULL;
		ULONG uReturn = 0;

		// @AI-Generated
		while (pEnumerator) {
			HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

			if (0 == uReturn) {
				break;
			}

			// Iterate through all property names in this object
			hr = pclsObj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
			if (SUCCEEDED(hr)) {
				BSTR strName = NULL;
				VARIANT vtProp;
				VariantInit(&vtProp);
				while (pclsObj->Next(0, &strName, &vtProp, NULL, NULL) == S_OK) {
					if (wcscmp(strName, L"Domain") == 0 && vtProp.vt == VT_BSTR) {
						unicode::toUtf8(buf->domainname, 64,
								WideStringView((char16_t *)vtProp.bstrVal));
					}
					/*wprintf(L"  %s: ", strName);
					if (vtProp.vt == VT_BSTR && vtProp.bstrVal) {
						wprintf(L"%s", vtProp.bstrVal);
					} else if (vtProp.vt == VT_I4) {
						wprintf(L"%ld", vtProp.lVal);
					} else if (vtProp.vt == VT_UI4) {
						wprintf(L"%lu", vtProp.ulVal);
					} else if (vtProp.vt == VT_BOOL) {
						wprintf(L"%s", vtProp.boolVal ? L"True" : L"False");
					}
					wprintf(L"\n");*/
					SysFreeString(strName);
					VariantClear(&vtProp);
					VariantInit(&vtProp);
				}
				VariantClear(&vtProp);
				pclsObj->EndEnumeration();
			}

			pclsObj->Release();
		}
		pEnumerator->Release();
		// end @AI-Generated
	}

	pSvc->Release();
	pLoc->Release();
	return true;
}

__SPRT_C_FUNC int __SPRT_ID(uname)(struct __SPRT_UTSNAME_NAME *buf) {
	ZeroMemory(buf, sizeof(struct __SPRT_UTSNAME_NAME));

	memcpy(buf->sysname, "Windows", 8);

	DWORD bufSize = MAX_COMPUTERNAME_LENGTH + 1;
	WCHAR computerNameBuf[MAX_COMPUTERNAME_LENGTH + 1];
	GetComputerNameW(computerNameBuf, &bufSize);

	unicode::toUtf8(buf->nodename, 64, WideStringView((char16_t *)computerNameBuf, bufSize));

	SYSTEM_INFO sysinfo;
	GetNativeSystemInfo(&sysinfo);

	switch (sysinfo.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL: memcpy(buf->machine, "i386", 4); break;
	case PROCESSOR_ARCHITECTURE_MIPS: memcpy(buf->machine, "mips", 4); break;
	case PROCESSOR_ARCHITECTURE_ALPHA: memcpy(buf->machine, "alpha", 5); break;
	case PROCESSOR_ARCHITECTURE_PPC: memcpy(buf->machine, "ppc", 3); break;
	case PROCESSOR_ARCHITECTURE_SHX: memcpy(buf->machine, "shx", 3); break;
	case PROCESSOR_ARCHITECTURE_ARM: memcpy(buf->machine, "arm", 3); break;
	case PROCESSOR_ARCHITECTURE_IA64: memcpy(buf->machine, "ia64", 4); break;
	case PROCESSOR_ARCHITECTURE_ALPHA64: memcpy(buf->machine, "alpha64", 7); break;
	case PROCESSOR_ARCHITECTURE_MSIL: memcpy(buf->machine, "msil", 4); break;
	case PROCESSOR_ARCHITECTURE_AMD64: memcpy(buf->machine, "x86_64", 6); break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64: memcpy(buf->machine, "i386", 4); break;
	case PROCESSOR_ARCHITECTURE_NEUTRAL: memcpy(buf->machine, "neutral", 7); break;
	case PROCESSOR_ARCHITECTURE_ARM64: memcpy(buf->machine, "aarch64", 7); break;
	case PROCESSOR_ARCHITECTURE_ARM32_ON_WIN64: memcpy(buf->machine, "arm", 3); break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_ARM64: memcpy(buf->machine, "i386", 4); break;
	}

	RTL_OSVERSIONINFOEXW verInfo;
	verInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

	// Try Register first
	__queryRegisterVersion(buf);

	// Then try WMI - more accurate but less stable
	__queryWmiVersion(buf);

	return 0;
}

} // namespace sprt

#endif
