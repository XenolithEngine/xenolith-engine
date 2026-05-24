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
#include "sprt/wrappers/windows/wincrypt.h"

#include "dllloader.h"

extern "C" {
BOOL AllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
		BYTE nSubAuthorityCount, DWORD nSubAuthority0, DWORD nSubAuthority1, DWORD nSubAuthority2,
		DWORD nSubAuthority3, DWORD nSubAuthority4, DWORD nSubAuthority5, DWORD nSubAuthority6,
		DWORD nSubAuthority7, PSID *pSid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, AllocateAndInitializeSid, pIdentifierAuthority,
			nSubAuthorityCount, nSubAuthority0, nSubAuthority1, nSubAuthority2, nSubAuthority3,
			nSubAuthority4, nSubAuthority5, nSubAuthority6, nSubAuthority7, pSid);
}

LPBYTE GetSidSubAuthorityCount(LPSID Sid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetSidSubAuthorityCount, Sid);
}

LPDWORD GetSidSubAuthority(LPSID Sid, DWORD nSubAuthority) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetSidSubAuthority, Sid, nSubAuthority);
}

PVOID FreeSid(PSID pSid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, FreeSid, pSid);
}

DWORD GetSecurityInfo(HANDLE handle, SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
		PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl, PACL *ppSacl,
		PSECURITY_DESCRIPTOR *ppSecurityDescriptor) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetSecurityInfo, handle, ObjectType, SecurityInfo,
			ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

DWORD GetNamedSecurityInfoW(LPCWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl,
		PACL *ppSacl, PSECURITY_DESCRIPTOR *ppSecurityDescriptor) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetNamedSecurityInfoW, pObjectName, ObjectType,
			SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

DWORD SetEntriesInAclW(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESS_W pListOfExplicitEntries,
		PACL OldAcl, PACL *NewAcl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, SetEntriesInAclW, cCountOfExplicitEntries,
			pListOfExplicitEntries, OldAcl, NewAcl);
}

DWORD SetNamedSecurityInfoW(LPWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, SetNamedSecurityInfoW, pObjectName, ObjectType,
			SecurityInfo, psidOwner, psidGroup, pDacl, pSacl);
}

BOOL ConvertSidToStringSidW(PSID Sid, LPWSTR *StringSid) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, ConvertSidToStringSidW, Sid, StringSid);
}

WINAPI HANDLE RegisterEventSourceW(LPCWSTR lpUNCServerName, LPCWSTR lpSourceName) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, RegisterEventSourceW, lpUNCServerName, lpSourceName);
}

WINAPI BOOL DeregisterEventSource(HANDLE hEventLog) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, DeregisterEventSource, hEventLog);
}

WINAPI BOOL ReportEventW(HANDLE hEventLog, WORD wType, WORD wCategory, DWORD dwEventID,
		PSID lpUserSid, WORD wNumStrings, DWORD dwDataSize, LPCWSTR *lpStrings, LPVOID lpRawData) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, ReportEventW, hEventLog, wType, wCategory, dwEventID,
			lpUserSid, wNumStrings, dwDataSize, lpStrings, lpRawData);
}

WINAPI BOOL CryptAcquireContextW(HCRYPTPROV *phProv, LPCWSTR szContainer, LPCWSTR szProvider,
		DWORD dwProvType, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptAcquireContextW, phProv, szContainer,
			szProvider, dwProvType, dwFlags);
}

WINAPI BOOL CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptReleaseContext, hProv, dwFlags);
}

WINAPI BOOL CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, BYTE *pbBuffer) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptGenRandom, hProv, dwLen, pbBuffer);
}

WINAPI BOOL CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType, DWORD dwFlags,
		BYTE *pbData, DWORD *pdwDataLen) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptExportKey, hKey, hExpKey, dwBlobType, dwFlags,
			pbData, pdwDataLen);
}

WINAPI BOOL CryptImportKey(HCRYPTPROV hProv, const BYTE *pbData, DWORD dwDataLen, HCRYPTKEY hPubKey,
		DWORD dwFlags, HCRYPTKEY *phKey) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptImportKey, hProv, pbData, dwDataLen, hPubKey,
			dwFlags, phKey);
}

WINAPI BOOL CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwBufLen) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptEncrypt, hKey, hHash, Final, dwFlags, pbData,
			pdwDataLen, dwBufLen);
}

WINAPI BOOL CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE *pbData,
		DWORD *pdwDataLen) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptDecrypt, hKey, hHash, Final, dwFlags, pbData,
			pdwDataLen);
}

WINAPI BOOL CryptDestroyKey(HCRYPTKEY hKey) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptDestroyKey, hKey);
}

WINAPI BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData, DWORD *pdwDataLen,
		DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptGetHashParam, hHash, dwParam, pbData,
			pdwDataLen, dwFlags);
}

WINAPI BOOL CryptCreateHash(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey, DWORD dwFlags,
		HCRYPTHASH *phHash) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptCreateHash, hProv, Algid, hKey, dwFlags,
			phHash);
}

WINAPI BOOL CryptHashData(HCRYPTHASH hHash, const BYTE *pbData, DWORD dwDataLen, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptHashData, hHash, pbData, dwDataLen, dwFlags);
}

WINAPI BOOL CryptHashSessionKey(HCRYPTHASH hHash, HCRYPTKEY hKey, DWORD dwFlags) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptHashSessionKey, hHash, hKey, dwFlags);
}

WINAPI BOOL CryptDestroyHash(HCRYPTHASH hHash) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, CryptDestroyHash, hHash);
}

WINAPI BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, OpenProcessToken, ProcessHandle, DesiredAccess,
			TokenHandle);
}

WINAPI BOOL GetTokenInformation(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass,
		LPVOID TokenInformation, DWORD TokenInformationLength, PDWORD ReturnLength) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetTokenInformation, TokenHandle,
			TokenInformationClass, TokenInformation, TokenInformationLength, ReturnLength);
}

WINAPI BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD lpnSize) {
	auto loader = sprt::DllLoader::get();
	return DLL_LOAD_AND_CALL(loader, advapi32, GetUserNameW, lpBuffer, lpnSize);
}
}
