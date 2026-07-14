// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/structures.h <-> Windows SDK parity. Compile-time only; see check.sh.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/structures.h>
#include "abi_check.h"

#include <windows.h>   // LARGE_INTEGER, FILETIME, GUID, POINT, RECT, COORD, SMALL_RECT, OVERLAPPED, ...
#include <oaidl.h>     // DECIMAL, SAFEARRAY, SAFEARRAYBOUND
#include <objidl.h>    // SOLE_AUTHENTICATION_SERVICE
#include <uxtheme.h>   // MARGINS

// === LARGE_INTEGER =========================================================
// Anonymous union: check sizeof + the named members (u sub-struct, QuadPart).
SPRT_SIZE(LARGE_INTEGER);
SPRT_OFFSET(LARGE_INTEGER, u);
SPRT_OFFSET(LARGE_INTEGER, QuadPart);
SPRT_OFFSET(LARGE_INTEGER, u.LowPart);
SPRT_OFFSET(LARGE_INTEGER, u.HighPart);

// === ULARGE_INTEGER ========================================================
SPRT_SIZE(ULARGE_INTEGER);
SPRT_OFFSET(ULARGE_INTEGER, u);
SPRT_OFFSET(ULARGE_INTEGER, QuadPart);
SPRT_OFFSET(ULARGE_INTEGER, u.LowPart);
SPRT_OFFSET(ULARGE_INTEGER, u.HighPart);

// === FILETIME ==============================================================
SPRT_SIZE(FILETIME);
SPRT_OFFSET(FILETIME, dwLowDateTime);
SPRT_OFFSET(FILETIME, dwHighDateTime);

// === SECURITY_ATTRIBUTES ===================================================
SPRT_SIZE(SECURITY_ATTRIBUTES);
SPRT_OFFSET(SECURITY_ATTRIBUTES, nLength);
SPRT_OFFSET(SECURITY_ATTRIBUTES, lpSecurityDescriptor);
SPRT_OFFSET(SECURITY_ATTRIBUTES, bInheritHandle);

// === OVERLAPPED ============================================================
// Offset/OffsetHigh/Pointer live in an anonymous union; take their offsets too.
SPRT_SIZE(OVERLAPPED);
SPRT_OFFSET(OVERLAPPED, Internal);
SPRT_OFFSET(OVERLAPPED, InternalHigh);
SPRT_OFFSET(OVERLAPPED, Offset);
SPRT_OFFSET(OVERLAPPED, OffsetHigh);
SPRT_OFFSET(OVERLAPPED, Pointer);
SPRT_OFFSET(OVERLAPPED, hEvent);

// === OVERLAPPED_ENTRY ======================================================
SPRT_SIZE(OVERLAPPED_ENTRY);
SPRT_OFFSET(OVERLAPPED_ENTRY, lpCompletionKey);
SPRT_OFFSET(OVERLAPPED_ENTRY, lpOverlapped);
SPRT_OFFSET(OVERLAPPED_ENTRY, Internal);
SPRT_OFFSET(OVERLAPPED_ENTRY, dwNumberOfBytesTransferred);

// === POINT =================================================================
SPRT_SIZE(POINT);
SPRT_OFFSET(POINT, x);
SPRT_OFFSET(POINT, y);

// === POINTL ================================================================
SPRT_SIZE(POINTL);
SPRT_OFFSET(POINTL, x);
SPRT_OFFSET(POINTL, y);

// === RECT ==================================================================
SPRT_SIZE(RECT);
SPRT_OFFSET(RECT, left);
SPRT_OFFSET(RECT, top);
SPRT_OFFSET(RECT, right);
SPRT_OFFSET(RECT, bottom);

// === RECTL =================================================================
SPRT_SIZE(RECTL);
SPRT_OFFSET(RECTL, left);
SPRT_OFFSET(RECTL, top);
SPRT_OFFSET(RECTL, right);
SPRT_OFFSET(RECTL, bottom);

// === GUID ==================================================================
SPRT_SIZE(GUID);
SPRT_OFFSET(GUID, Data1);
SPRT_OFFSET(GUID, Data2);
SPRT_OFFSET(GUID, Data3);
SPRT_OFFSET(GUID, Data4);

// === LIST_ENTRY ============================================================
SPRT_SIZE(LIST_ENTRY);
SPRT_OFFSET(LIST_ENTRY, Flink);
SPRT_OFFSET(LIST_ENTRY, Blink);

// === COORD =================================================================
SPRT_SIZE(COORD);
SPRT_OFFSET(COORD, X);
SPRT_OFFSET(COORD, Y);

// === SMALL_RECT ============================================================
SPRT_SIZE(SMALL_RECT);
SPRT_OFFSET(SMALL_RECT, Left);
SPRT_OFFSET(SMALL_RECT, Top);
SPRT_OFFSET(SMALL_RECT, Right);
SPRT_OFFSET(SMALL_RECT, Bottom);

// === GROUP_AFFINITY ========================================================
SPRT_SIZE(GROUP_AFFINITY);
SPRT_OFFSET(GROUP_AFFINITY, Mask);
SPRT_OFFSET(GROUP_AFFINITY, Group);
SPRT_OFFSET(GROUP_AFFINITY, Reserved);

// === still omitted =========================================================
// VARIANT (oaidl.h — big anonymous-union struct), UNICODE_STRING (winternl.h,
// Reserved-padded), GROUP_AFFINITY32 / GROUP_AFFINITY64 (SPRT explicit-width, no
// SDK tag). MARGINS / DECIMAL / SAFEARRAY / SAFEARRAYBOUND / SOLE_AUTHENTICATION_SERVICE
// are now pinned below via <uxtheme.h>/<oaidl.h>/<objidl.h>.

// === MARGINS (uxtheme.h) ===================================================
SPRT_SIZE(MARGINS);
SPRT_OFFSET(MARGINS, cxLeftWidth);
SPRT_OFFSET(MARGINS, cxRightWidth);
SPRT_OFFSET(MARGINS, cyTopHeight);
SPRT_OFFSET(MARGINS, cyBottomHeight);

// === DECIMAL (oaidl.h/wtypes.h) ============================================
SPRT_SIZE(DECIMAL);
SPRT_OFFSET(DECIMAL, wReserved);
SPRT_OFFSET(DECIMAL, scale);
SPRT_OFFSET(DECIMAL, sign);
SPRT_OFFSET(DECIMAL, signscale);
SPRT_OFFSET(DECIMAL, Hi32);
SPRT_OFFSET(DECIMAL, Lo32);
SPRT_OFFSET(DECIMAL, Mid32);
SPRT_OFFSET(DECIMAL, Lo64);

// === SAFEARRAYBOUND / SAFEARRAY (oaidl.h) ==================================
SPRT_SIZE(SAFEARRAYBOUND);
SPRT_OFFSET(SAFEARRAYBOUND, cElements);
SPRT_OFFSET(SAFEARRAYBOUND, lLbound);
SPRT_SIZE(SAFEARRAY);
SPRT_OFFSET(SAFEARRAY, cDims);
SPRT_OFFSET(SAFEARRAY, fFeatures);
SPRT_OFFSET(SAFEARRAY, cbElements);
SPRT_OFFSET(SAFEARRAY, cLocks);
SPRT_OFFSET(SAFEARRAY, pvData);
SPRT_OFFSET(SAFEARRAY, rgsabound);

// === SOLE_AUTHENTICATION_SERVICE (objidl.h) ================================
SPRT_SIZE(SOLE_AUTHENTICATION_SERVICE);
SPRT_OFFSET(SOLE_AUTHENTICATION_SERVICE, dwAuthnSvc);
SPRT_OFFSET(SOLE_AUTHENTICATION_SERVICE, dwAuthzSvc);
SPRT_OFFSET(SOLE_AUTHENTICATION_SERVICE, pPrincipalName);
SPRT_OFFSET(SOLE_AUTHENTICATION_SERVICE, hr);

// === GROUP_AFFINITY (ntdef.h) ==============================================
// (Already pinned above via <windows.h>; ntdef.h is the canonical source.)
SPRT_SIZE(GROUP_AFFINITY);
SPRT_OFFSET(GROUP_AFFINITY, Mask);
SPRT_OFFSET(GROUP_AFFINITY, Group);
SPRT_OFFSET(GROUP_AFFINITY, Reserved);
// GROUP_AFFINITY32 / GROUP_AFFINITY64 are SPRT explicit-width variants with no SDK
// tag (the SDK's single GROUP_AFFINITY == GROUP_AFFINITY64 on these 64-bit targets).
