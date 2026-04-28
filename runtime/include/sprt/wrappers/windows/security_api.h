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

/* ============================================================ */
/* Security Identifier (SID) Types (winnt.h)                    */
/* ============================================================ */

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

__SPRT_BEGIN_DECL

/* ---- Security Functions (securitybaseapi.h) ---- */

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

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_SECURITY_API_H_
