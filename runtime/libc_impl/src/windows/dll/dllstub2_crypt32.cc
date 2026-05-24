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
#include <sprt/wrappers/windows/wincrypt.h>

#include "dllloader.h"

extern "C" {
WINAPI BOOL CertFreeCertificateContext(PCCERT_CONTEXT pCertContext) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertFreeCertificateContext, pCertContext);
}

WINAPI PCCERT_CONTEXT CertFindCertificateInStore(HCERTSTORE hCertStore, DWORD dwCertEncodingType,
		DWORD dwFindFlags, DWORD dwFindType, const void *pvFindPara,
		PCCERT_CONTEXT pPrevCertContext) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertFindCertificateInStore, hCertStore,
			dwCertEncodingType, dwFindFlags, dwFindType, pvFindPara, pPrevCertContext);
}

WINAPI BOOL CertGetIntendedKeyUsage(DWORD dwCertEncodingType, PCERT_INFO pCertInfo,
		BYTE *pbKeyUsage, DWORD cbKeyUsage) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertGetIntendedKeyUsage, dwCertEncodingType,
			pCertInfo, pbKeyUsage, cbKeyUsage);
}

WINAPI BOOL CertGetEnhancedKeyUsage(PCCERT_CONTEXT pCertContext, DWORD dwFlags,
		PCERT_ENHKEY_USAGE pUsage, DWORD *pcbUsage) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertGetEnhancedKeyUsage, pCertContext, dwFlags,
			pUsage, pcbUsage);
}

WINAPI HCERTSTORE CertOpenSystemStoreA(HCRYPTPROV_LEGACY hProv, LPCSTR szSubsystemProtocol) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertOpenSystemStoreA, hProv, szSubsystemProtocol);
}

WINAPI HCERTSTORE CertOpenSystemStoreW(HCRYPTPROV_LEGACY hProv, LPCWSTR szSubsystemProtocol) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertOpenSystemStoreW, hProv, szSubsystemProtocol);
}

WINAPI BOOL CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertCloseStore, hCertStore, dwFlags);
}

WINAPI PCCERT_CONTEXT CertEnumCertificatesInStore(HCERTSTORE hCertStore,
		PCCERT_CONTEXT pPrevCertContext) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, crypt32, CertEnumCertificatesInStore, hCertStore,
			pPrevCertContext);
}
}
