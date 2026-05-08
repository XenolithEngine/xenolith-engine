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

#include <sprt/wrappers/windows/security_api.h>

#include "dllloader.h"

extern "C" {

// ---- Security Functions (advapi32) ----

BOOL AllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
		BYTE nSubAuthorityCount, DWORD nSubAuthority0, DWORD nSubAuthority1, DWORD nSubAuthority2,
		DWORD nSubAuthority3, DWORD nSubAuthority4, DWORD nSubAuthority5, DWORD nSubAuthority6,
		DWORD nSubAuthority7, PSID *pSid) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&AllocateAndInitializeSid)>(
			loader->advapi32.AllocateAndInitializeSid, pIdentifierAuthority, nSubAuthorityCount,
			nSubAuthority0, nSubAuthority1, nSubAuthority2, nSubAuthority3, nSubAuthority4,
			nSubAuthority5, nSubAuthority6, nSubAuthority7, pSid);
}

LPBYTE GetSidSubAuthorityCount(LPSID Sid) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetSidSubAuthorityCount)>(
			loader->advapi32.GetSidSubAuthorityCount, Sid);
}

LPDWORD GetSidSubAuthority(LPSID Sid, DWORD nSubAuthority) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetSidSubAuthority)>(loader->advapi32.GetSidSubAuthority,
			Sid, nSubAuthority);
}

PVOID FreeSid(PSID pSid) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&FreeSid)>(loader->advapi32.FreeSid, pSid);
}

DWORD GetSecurityInfo(HANDLE handle, SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
		PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl, PACL *ppSacl,
		PSECURITY_DESCRIPTOR *ppSecurityDescriptor) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetSecurityInfo)>(loader->advapi32.GetSecurityInfo,
			handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl,
			ppSecurityDescriptor);
}

DWORD GetNamedSecurityInfoW(LPCWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl,
		PACL *ppSacl, PSECURITY_DESCRIPTOR *ppSecurityDescriptor) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetNamedSecurityInfoW)>(
			loader->advapi32.GetNamedSecurityInfoW, pObjectName, ObjectType, SecurityInfo,
			ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
}

DWORD SetEntriesInAclW(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESS_W pListOfExplicitEntries,
		PACL OldAcl, PACL *NewAcl) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&SetEntriesInAclW)>(loader->advapi32.SetEntriesInAclW,
			cCountOfExplicitEntries, pListOfExplicitEntries, OldAcl, NewAcl);
}

DWORD SetNamedSecurityInfoW(LPWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&SetNamedSecurityInfoW)>(
			loader->advapi32.SetNamedSecurityInfoW, pObjectName, ObjectType, SecurityInfo,
			psidOwner, psidGroup, pDacl, pSacl);
}

BOOL ConvertSidToStringSidW(PSID Sid, LPWSTR *StringSid) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&ConvertSidToStringSidW)>(
			loader->advapi32.ConvertSidToStringSidW, Sid, StringSid);
}

// ---- AppContainer Functions (shell32/onecore) ----

HRESULT CreateAppContainerProfile(PCWSTR pszAppContainerName, PCWSTR pszDisplayName,
		PCWSTR pszDescription, PSID_AND_ATTRIBUTES pCapabilities, DWORD dwCapabilityCount,
		PSID *ppSidAppContainerSid) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&CreateAppContainerProfile)>(
			loader->userenv.CreateAppContainerProfile, pszAppContainerName, pszDisplayName,
			pszDescription, pCapabilities, dwCapabilityCount, ppSidAppContainerSid);
}

HRESULT DeriveAppContainerSidFromAppContainerName(PCWSTR pszAppContainerName,
		PSID *ppsidAppContainerSid) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&DeriveAppContainerSidFromAppContainerName)>(
			loader->userenv.DeriveAppContainerSidFromAppContainerName, pszAppContainerName,
			ppsidAppContainerSid);
}

HRESULT GetAppContainerFolderPath(PCWSTR pszAppContainerSid, PWSTR *ppszPath) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&GetAppContainerFolderPath)>(
			loader->userenv.GetAppContainerFolderPath, pszAppContainerSid, ppszPath);
}

HRESULT DeleteAppContainerProfile(PCWSTR pszAppContainerName) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&DeleteAppContainerProfile)>(
			loader->userenv.DeleteAppContainerProfile, pszAppContainerName);
}

} // extern "C"
