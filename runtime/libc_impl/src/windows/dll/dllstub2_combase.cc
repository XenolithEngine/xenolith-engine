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

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/com_api.h>

#include "dllloader.h"

extern "C" {
WINAPI HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
		SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel,
		DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, CoInitializeSecurity, pSecDesc, cAuthSvc, asAuthSvc,
			pReserved1, dwAuthnLevel, dwImpLevel, pAuthList, dwCapabilities, pReserved3);
}

WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext,
		REFIID riid, LPVOID *ppv) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, CoCreateInstance, rclsid, pUnkOuter, dwClsContext,
			riid, ppv);
}

WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
		LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthIdentity,
		DWORD dwCapabilities) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, CoSetProxyBlanket, pProxy, dwAuthnSvc, dwAuthzSvc,
			pServerPrincName, dwAuthnLevel, dwImpLevel, pAuthIdentity, dwCapabilities);
}

WINAPI HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, CoInitializeEx, pvReserved, dwCoInit);
}

WINAPI void CoUninitialize() {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, combase, CoUninitialize);
}

WINAPI void CoTaskMemFree(LPVOID pv) {
	auto loader = sprt::DllLoader::get();
	DLL_LOAD_AND_CALL(loader, combase, CoTaskMemFree, pv);
}

WINAPI HRESULT StringFromCLSID(REFCLSID rclsid, LPOLESTR *lplpsz) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, StringFromCLSID, rclsid, lplpsz);
}

WINAPI HRESULT CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, CLSIDFromString, lpsz, pclsid);
}

WINAPI HRESULT IIDFromString(LPCOLESTR lpsz, LPIID lpiid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, combase, IIDFromString, lpsz, lpiid);
}
}
