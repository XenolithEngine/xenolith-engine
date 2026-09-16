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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_SECURITY_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_SECURITY_API_H_

#include <sprt/wrappers/windows/abi/basic_types.h>
#include <sprt/wrappers/windows/abi/constants.h>
#include <sprt/wrappers/windows/abi/structures.h>

// clang-format off
#define __SPRT_SECURITY_CAPABILITY_INTERNET_CLIENT                     (0x00000001L)
#define __SPRT_SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER              (0x00000002L)
#define __SPRT_SECURITY_CAPABILITY_PRIVATE_NETWORK_CLIENT_SERVER       (0x00000003L)
#define __SPRT_SECURITY_CAPABILITY_PICTURES_LIBRARY                    (0x00000004L)
#define __SPRT_SECURITY_CAPABILITY_VIDEOS_LIBRARY                      (0x00000005L)
#define __SPRT_SECURITY_CAPABILITY_MUSIC_LIBRARY                       (0x00000006L)
#define __SPRT_SECURITY_CAPABILITY_DOCUMENTS_LIBRARY                   (0x00000007L)
#define __SPRT_SECURITY_CAPABILITY_ENTERPRISE_AUTHENTICATION           (0x00000008L)
#define __SPRT_SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES            (0x00000009L)
#define __SPRT_SECURITY_CAPABILITY_REMOVABLE_STORAGE                   (0x0000000AL)
#define __SPRT_SECURITY_CAPABILITY_APPOINTMENTS                        (0x0000000BL)
#define __SPRT_SECURITY_CAPABILITY_CONTACTS                            (0x0000000CL)
#define __SPRT_SECURITY_CAPABILITY_INTERNET_EXPLORER                   (0x00001000L)

#define __SPRT_OWNER_SECURITY_INFORMATION                  (0x00000001L)
#define __SPRT_GROUP_SECURITY_INFORMATION                  (0x00000002L)
#define __SPRT_DACL_SECURITY_INFORMATION                   (0x00000004L)
#define __SPRT_SACL_SECURITY_INFORMATION                   (0x00000008L)
#define __SPRT_LABEL_SECURITY_INFORMATION                  (0x00000010L)
#define __SPRT_ATTRIBUTE_SECURITY_INFORMATION              (0x00000020L)
#define __SPRT_SCOPE_SECURITY_INFORMATION                  (0x00000040L)
#define __SPRT_PROCESS_TRUST_LABEL_SECURITY_INFORMATION    (0x00000080L)
#define __SPRT_ACCESS_FILTER_SECURITY_INFORMATION          (0x00000100L)
#define __SPRT_BACKUP_SECURITY_INFORMATION                 (0x00010000L)

#define __SPRT_PROTECTED_DACL_SECURITY_INFORMATION         (0x80000000L)
#define __SPRT_PROTECTED_SACL_SECURITY_INFORMATION         (0x40000000L)
#define __SPRT_UNPROTECTED_DACL_SECURITY_INFORMATION       (0x20000000L)
#define __SPRT_UNPROTECTED_SACL_SECURITY_INFORMATION       (0x10000000L)

#define __SPRT_OBJECT_INHERIT_ACE                (0x1)
#define __SPRT_CONTAINER_INHERIT_ACE             (0x2)
#define __SPRT_NO_PROPAGATE_INHERIT_ACE          (0x4)
#define __SPRT_INHERIT_ONLY_ACE                  (0x8)
#define __SPRT_INHERITED_ACE                     (0x10)
#define __SPRT_VALID_INHERIT_FLAGS               (0x1F)

#define __SPRT_SECURITY_APP_PACKAGE_AUTHORITY              {0,0,0,0,0,15}

#define __SPRT_SECURITY_APP_PACKAGE_BASE_RID               (0x00000002L)
#define __SPRT_SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT      (2L)
#define __SPRT_SECURITY_APP_PACKAGE_RID_COUNT              (8L)
#define __SPRT_SECURITY_CAPABILITY_BASE_RID                (0x00000003L)
#define __SPRT_SECURITY_CAPABILITY_APP_RID                 (0x00000400L)
#define __SPRT_SECURITY_CAPABILITY_APP_SILO_RID            (0x00010000L)
#define __SPRT_SECURITY_BUILTIN_CAPABILITY_RID_COUNT       (2L)
#define __SPRT_SECURITY_CAPABILITY_RID_COUNT               (5L)
#define __SPRT_SECURITY_PARENT_PACKAGE_RID_COUNT           (__SPRT_SECURITY_APP_PACKAGE_RID_COUNT)
#define __SPRT_SECURITY_CHILD_PACKAGE_RID_COUNT            (12L)

#define __SPRT_SE_GROUP_MANDATORY                 (0x00000001L)
#define __SPRT_SE_GROUP_ENABLED_BY_DEFAULT        (0x00000002L)
#define __SPRT_SE_GROUP_ENABLED                   (0x00000004L)
#define __SPRT_SE_GROUP_OWNER                     (0x00000008L)
#define __SPRT_SE_GROUP_USE_FOR_DENY_ONLY         (0x00000010L)
#define __SPRT_SE_GROUP_INTEGRITY                 (0x00000020L)
#define __SPRT_SE_GROUP_INTEGRITY_ENABLED         (0x00000040L)
#define __SPRT_SE_GROUP_LOGON_ID                  (0xC0000000L)
#define __SPRT_SE_GROUP_RESOURCE                  (0x20000000L)
 
#define __SPRT_SE_PRIVILEGE_ENABLED_BY_DEFAULT (0x00000001L)
#define __SPRT_SE_PRIVILEGE_ENABLED            (0x00000002L)
#define __SPRT_SE_PRIVILEGE_REMOVED            (0X00000004L)
#define __SPRT_SE_PRIVILEGE_USED_FOR_ACCESS    (0x80000000L)

#define __SPRT_SE_PRIVILEGE_VALID_ATTRIBUTES \
	(__SPRT_SE_PRIVILEGE_ENABLED_BY_DEFAULT | __SPRT_SE_PRIVILEGE_ENABLED | __SPRT_SE_PRIVILEGE_REMOVED  | __SPRT_SE_PRIVILEGE_USED_FOR_ACCESS)

#define __SPRT_E_ACCESSDENIED                   HRESULT(0x80070005L)
#define __SPRT_E_INVALIDARG                     HRESULT(0x80070057L)

#define __SPRT_EVENTLOG_SUCCESS                0x0000
#define __SPRT_EVENTLOG_ERROR_TYPE             0x0001
#define __SPRT_EVENTLOG_WARNING_TYPE           0x0002
#define __SPRT_EVENTLOG_INFORMATION_TYPE       0x0004
#define __SPRT_EVENTLOG_AUDIT_SUCCESS          0x0008
#define __SPRT_EVENTLOG_AUDIT_FAILURE          0x0010

// clang-format on

__SPRT_DECLARE_HANDLE(HWINSTA);

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

typedef struct _LUID {
	ULONG LowPart;
	LONG HighPart;
} LUID, *PLUID;

typedef struct _LUID_AND_ATTRIBUTES {
	LUID Luid;
	DWORD Attributes;
} LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;
typedef LUID_AND_ATTRIBUTES LUID_AND_ATTRIBUTES_ARRAY[__SPRT_ANYSIZE_ARRAY];
typedef LUID_AND_ATTRIBUTES_ARRAY *PLUID_AND_ATTRIBUTES_ARRAY;

typedef struct _TOKEN_PRIVILEGES {
	DWORD PrivilegeCount;
	LUID_AND_ATTRIBUTES Privileges[__SPRT_ANYSIZE_ARRAY];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_SECURITY_API_H_
