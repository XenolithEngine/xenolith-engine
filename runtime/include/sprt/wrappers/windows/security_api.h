/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_SECURITY_API_H_
#define SPRT_WRAPPERS_WINDOWS_SECURITY_API_H_

#include <sprt/wrappers/windows/basic_types.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/structures.h>

// clang-format off
#define SECURITY_CAPABILITY_INTERNET_CLIENT                     (0x00000001L)
#define SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER              (0x00000002L)
#define SECURITY_CAPABILITY_PRIVATE_NETWORK_CLIENT_SERVER       (0x00000003L)
#define SECURITY_CAPABILITY_PICTURES_LIBRARY                    (0x00000004L)
#define SECURITY_CAPABILITY_VIDEOS_LIBRARY                      (0x00000005L)
#define SECURITY_CAPABILITY_MUSIC_LIBRARY                       (0x00000006L)
#define SECURITY_CAPABILITY_DOCUMENTS_LIBRARY                   (0x00000007L)
#define SECURITY_CAPABILITY_ENTERPRISE_AUTHENTICATION           (0x00000008L)
#define SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES            (0x00000009L)
#define SECURITY_CAPABILITY_REMOVABLE_STORAGE                   (0x0000000AL)
#define SECURITY_CAPABILITY_APPOINTMENTS                        (0x0000000BL)
#define SECURITY_CAPABILITY_CONTACTS                            (0x0000000CL)
#define SECURITY_CAPABILITY_INTERNET_EXPLORER                   (0x00001000L)

#define OWNER_SECURITY_INFORMATION                  (0x00000001L)
#define GROUP_SECURITY_INFORMATION                  (0x00000002L)
#define DACL_SECURITY_INFORMATION                   (0x00000004L)
#define SACL_SECURITY_INFORMATION                   (0x00000008L)
#define LABEL_SECURITY_INFORMATION                  (0x00000010L)
#define ATTRIBUTE_SECURITY_INFORMATION              (0x00000020L)
#define SCOPE_SECURITY_INFORMATION                  (0x00000040L)
#define PROCESS_TRUST_LABEL_SECURITY_INFORMATION    (0x00000080L)
#define ACCESS_FILTER_SECURITY_INFORMATION          (0x00000100L)
#define BACKUP_SECURITY_INFORMATION                 (0x00010000L)

#define PROTECTED_DACL_SECURITY_INFORMATION         (0x80000000L)
#define PROTECTED_SACL_SECURITY_INFORMATION         (0x40000000L)
#define UNPROTECTED_DACL_SECURITY_INFORMATION       (0x20000000L)
#define UNPROTECTED_SACL_SECURITY_INFORMATION       (0x10000000L)

#define OBJECT_INHERIT_ACE                (0x1)
#define CONTAINER_INHERIT_ACE             (0x2)
#define NO_PROPAGATE_INHERIT_ACE          (0x4)
#define INHERIT_ONLY_ACE                  (0x8)
#define INHERITED_ACE                     (0x10)
#define VALID_INHERIT_FLAGS               (0x1F)

#define SECURITY_APP_PACKAGE_AUTHORITY              {0,0,0,0,0,15}

#define SECURITY_APP_PACKAGE_BASE_RID               (0x00000002L)
#define SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT      (2L)
#define SECURITY_APP_PACKAGE_RID_COUNT              (8L)
#define SECURITY_CAPABILITY_BASE_RID                (0x00000003L)
#define SECURITY_CAPABILITY_APP_RID                 (0x00000400L)
#define SECURITY_CAPABILITY_APP_SILO_RID            (0x00010000L)
#define SECURITY_BUILTIN_CAPABILITY_RID_COUNT       (2L)
#define SECURITY_CAPABILITY_RID_COUNT               (5L)
#define SECURITY_PARENT_PACKAGE_RID_COUNT           (SECURITY_APP_PACKAGE_RID_COUNT)
#define SECURITY_CHILD_PACKAGE_RID_COUNT            (12L)

#define SE_GROUP_MANDATORY                 (0x00000001L)
#define SE_GROUP_ENABLED_BY_DEFAULT        (0x00000002L)
#define SE_GROUP_ENABLED                   (0x00000004L)
#define SE_GROUP_OWNER                     (0x00000008L)
#define SE_GROUP_USE_FOR_DENY_ONLY         (0x00000010L)
#define SE_GROUP_INTEGRITY                 (0x00000020L)
#define SE_GROUP_INTEGRITY_ENABLED         (0x00000040L)
#define SE_GROUP_LOGON_ID                  (0xC0000000L)
#define SE_GROUP_RESOURCE                  (0x20000000L)

#define E_ACCESSDENIED                   HRESULT(0x80070005L)
#define E_INVALIDARG                     HRESULT(0x80070057L)
// clang-format on

typedef enum _SE_OBJECT_TYPE {
	SE_UNKNOWN_OBJECT_TYPE = 0,
	SE_FILE_OBJECT,
	SE_SERVICE,
	SE_PRINTER,
	SE_REGISTRY_KEY,
	SE_LMSHARE,
	SE_KERNEL_OBJECT,
	SE_WINDOW_OBJECT,
	SE_DS_OBJECT,
	SE_DS_OBJECT_ALL,
	SE_PROVIDER_DEFINED_OBJECT,
	SE_WMIGUID_OBJECT,
	SE_REGISTRY_WOW64_32KEY,
	SE_REGISTRY_WOW64_64KEY,
} SE_OBJECT_TYPE;

typedef struct _ACL {
	UCHAR AclRevision;
	UCHAR Sbz1;
	USHORT AclSize;
	USHORT AceCount;
	USHORT Sbz2;
} ACL;

typedef ACL *PACL;

/* ============================================================ */
/* Security Identifier (SID) Types (winnt.h)                    */
/* ============================================================ */

typedef enum _ACCESS_MODE {
	NOT_USED_ACCESS = 0,
	GRANT_ACCESS,
	SET_ACCESS,
	DENY_ACCESS,
	REVOKE_ACCESS,
	SET_AUDIT_SUCCESS,
	SET_AUDIT_FAILURE
} ACCESS_MODE;

typedef enum _MULTIPLE_TRUSTEE_OPERATION {
	NO_MULTIPLE_TRUSTEE,
	TRUSTEE_IS_IMPERSONATE,
} MULTIPLE_TRUSTEE_OPERATION;

typedef enum _TRUSTEE_FORM {
	TRUSTEE_IS_SID,
	TRUSTEE_IS_NAME,
	TRUSTEE_BAD_FORM,
	TRUSTEE_IS_OBJECTS_AND_SID,
	TRUSTEE_IS_OBJECTS_AND_NAME
} TRUSTEE_FORM;

typedef enum _TRUSTEE_TYPE {
	TRUSTEE_IS_UNKNOWN,
	TRUSTEE_IS_USER,
	TRUSTEE_IS_GROUP,
	TRUSTEE_IS_DOMAIN,
	TRUSTEE_IS_ALIAS,
	TRUSTEE_IS_WELL_KNOWN_GROUP,
	TRUSTEE_IS_DELETED,
	TRUSTEE_IS_INVALID,
	TRUSTEE_IS_COMPUTER
} TRUSTEE_TYPE;

/**
 * SID_IDENTIFIER_AUTHORITY - 6-byte identifier authority value for SIDs.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-sid_identifier_authority
 */
typedef struct _SID_IDENTIFIER_AUTHORITY {
	BYTE Value[6];
} SID_IDENTIFIER_AUTHORITY, *PSID_IDENTIFIER_AUTHORITY, *LPSID_IDENTIFIER_AUTHORITY;

/**
 * SID - Security Identifier structure.
 * A security identifier (SID) uniquely identifies a user or group account.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-sid
 */
typedef struct _SID {
	BYTE Revision;
	BYTE SubAuthorityCount;
	SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
	DWORD SubAuthority[1];
} SID, *PSID, *LPSID;

typedef struct _TOKEN_PRIMARY_GROUP {
	PSID PrimaryGroup;
} TOKEN_PRIMARY_GROUP, *PTOKEN_PRIMARY_GROUP;

typedef struct _TRUSTEE_W {
	struct _TRUSTEE_W *pMultipleTrustee;
	MULTIPLE_TRUSTEE_OPERATION MultipleTrusteeOperation;
	TRUSTEE_FORM TrusteeForm;
	TRUSTEE_TYPE TrusteeType;
	LPWSTR ptstrName;
} TRUSTEE_W, *PTRUSTEE_W, TRUSTEEW, *PTRUSTEEW;

typedef struct _EXPLICIT_ACCESS_W {
	DWORD grfAccessPermissions;
	ACCESS_MODE grfAccessMode;
	DWORD grfInheritance;
	TRUSTEE_W Trustee;
} EXPLICIT_ACCESS_W, *PEXPLICIT_ACCESS_W, EXPLICIT_ACCESSW, *PEXPLICIT_ACCESSW;

typedef struct _SID_AND_ATTRIBUTES {
	PSID Sid;
	DWORD Attributes;
} SID_AND_ATTRIBUTES, *PSID_AND_ATTRIBUTES;

typedef struct _TOKEN_USER {
	SID_AND_ATTRIBUTES User;
} TOKEN_USER, *PTOKEN_USER;

typedef struct _TOKEN_GROUPS {
	DWORD GroupCount;
	SID_AND_ATTRIBUTES Groups[1];
} TOKEN_GROUPS, *PTOKEN_GROUPS;

typedef struct _SECURITY_CAPABILITIES {
	PSID AppContainerSid;
	PSID_AND_ATTRIBUTES Capabilities;
	DWORD CapabilityCount;
	DWORD Reserved;
} SECURITY_CAPABILITIES, *PSECURITY_CAPABILITIES, *LPSECURITY_CAPABILITIES;

typedef struct _TOKEN_APPCONTAINER_INFORMATION {
	PSID TokenAppContainer;
} TOKEN_APPCONTAINER_INFORMATION, *PTOKEN_APPCONTAINER_INFORMATION;

__SPRT_BEGIN_DECL

/* ---- Security Functions (securitybaseapi.h) ---- */

WINAPI BOOL AllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
		BYTE nSubAuthorityCount, DWORD nSubAuthority0, DWORD nSubAuthority1, DWORD nSubAuthority2,
		DWORD nSubAuthority3, DWORD nSubAuthority4, DWORD nSubAuthority5, DWORD nSubAuthority6,
		DWORD nSubAuthority7, PSID *pSid);

/**
 * Retrieves a pointer to the count of subauthorities in a security identifier (SID).
 * @param Sid Pointer to the SID structure.
 * @return Pointer to the count of subauthorities in the SID.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-getsidsubauthoritycount
 */
WINAPI LPBYTE GetSidSubAuthorityCount(LPSID Sid);

/**
 * Retrieves a pointer to the specified subauthority value in a security identifier (SID).
 * @param Sid Pointer to the SID structure.
 * @param nSubAuthority Index of the subauthority value to retrieve (zero-based).
 * @return Pointer to the requested subauthority value.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-getsidsubauthority
 */
WINAPI LPDWORD GetSidSubAuthority(LPSID Sid, DWORD nSubAuthority);

WINAPI PVOID FreeSid(PSID pSid);

WINAPI DWORD GetSecurityInfo(HANDLE handle, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl,
		PACL *ppSacl, PSECURITY_DESCRIPTOR *ppSecurityDescriptor);

WINAPI HRESULT CreateAppContainerProfile(PCWSTR pszAppContainerName, PCWSTR pszDisplayName,
		PCWSTR pszDescription, PSID_AND_ATTRIBUTES pCapabilities, DWORD dwCapabilityCount,
		PSID *ppSidAppContainerSid);

WINAPI HRESULT DeriveAppContainerSidFromAppContainerName(PCWSTR pszAppContainerName,
		PSID *ppsidAppContainerSid);

WINAPI HRESULT GetAppContainerFolderPath(PCWSTR pszAppContainerSid, PWSTR *ppszPath);

WINAPI HRESULT DeleteAppContainerProfile(PCWSTR pszAppContainerName);

WINAPI DWORD GetNamedSecurityInfoW(LPCWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID *ppsidOwner, PSID *ppsidGroup, PACL *ppDacl,
		PACL *ppSacl, PSECURITY_DESCRIPTOR *ppSecurityDescriptor);

WINAPI DWORD SetEntriesInAclW(ULONG cCountOfExplicitEntries,
		PEXPLICIT_ACCESS_W pListOfExplicitEntries, PACL OldAcl, PACL *NewAcl);

WINAPI DWORD SetNamedSecurityInfoW(LPWSTR pObjectName, SE_OBJECT_TYPE ObjectType,
		SECURITY_INFORMATION SecurityInfo, PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl);

WINAPI BOOL ConvertSidToStringSidW(PSID Sid, LPWSTR *StringSid);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_SECURITY_API_H_
