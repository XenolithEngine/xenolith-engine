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

#ifndef SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_
#define SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_

#include <sprt/wrappers/windows/basic_types.h>

/*
 * Windows common structures (windef.h, winnt.h subset)
 */

/* LARGE_INTEGER union for file positioning */
typedef union _LARGE_INTEGER {
	struct {
		DWORD LowPart;
		LONG HighPart;
	};
	struct {
		DWORD LowPart;
		LONG HighPart;
	} u;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
	struct {
		DWORD LowPart;
		DWORD HighPart;
	};
	struct {
		DWORD LowPart;
		DWORD HighPart;
	} u;
	ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

/* FILETIME structure */
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

/* Security attributes structure */
typedef struct _SECURITY_ATTRIBUTES {
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	union {
		struct {
			DWORD Offset;
			DWORD OffsetHigh;
		};
		PVOID Pointer;
	};

	HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _OVERLAPPED_ENTRY {
	ULONG_PTR lpCompletionKey;
	LPOVERLAPPED lpOverlapped;
	ULONG_PTR Internal;
	DWORD dwNumberOfBytesTransferred;
} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;

typedef struct tagPOINT {
	LONG x;
	LONG y;
} POINT, *PPOINT, *NPPOINT, *LPPOINT;

typedef struct _POINTL {
	LONG x;
	LONG y;
} POINTL, *PPOINTL;

typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *PRECT, *NPRECT, *LPRECT;

typedef const RECT *LPCRECT;

typedef struct tagRECTL {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECTL, *PRECTL, *LPRECTL;

typedef const RECTL *LPCRECTL;

typedef struct _MARGINS {
	int cxLeftWidth; // width of left border that retains its size
	int cxRightWidth; // width of right border that retains its size
	int cyTopHeight; // height of top border that retains its size
	int cyBottomHeight; // height of bottom border that retains its size
} MARGINS, *PMARGINS;

typedef struct tagDEC {
	USHORT wReserved;
	union {
		struct {
			BYTE scale;
			BYTE sign;
		};
		USHORT signscale;
	};
	ULONG Hi32;
	union {
		struct {
			ULONG Lo32;
			ULONG Mid32;
		};
		ULONGLONG Lo64;
	};
} DECIMAL;

typedef struct tagSAFEARRAYBOUND {
	ULONG cElements;
	LONG lLbound;
} SAFEARRAYBOUND;

typedef struct tagSAFEARRAYBOUND *LPSAFEARRAYBOUND;

typedef struct tagSAFEARRAY {
	USHORT cDims;
	USHORT fFeatures;
	ULONG cbElements;
	ULONG cLocks;
	PVOID pvData;
	SAFEARRAYBOUND rgsabound[1];
} SAFEARRAY;

/* COM interface forward declarations */
typedef struct IWbemLocator IWbemLocator;
typedef struct IWbemServices IWbemServices;
typedef struct IEnumWbemClassObject IEnumWbemClassObject;
typedef struct IWbemClassObject IWbemClassObject;
typedef struct IUnknown IUnknown;
typedef struct IDispatch IDispatch;
typedef struct IRecordInfo IRecordInfo;

/* Security descriptor type for COM initialization */
typedef void *PSECURITY_DESCRIPTOR;

/* GUID type for CLSID/IID definitions - must be defined before REFCLSID/REFIID */
typedef struct _GUID {
	DWORD Data1;
	WORD Data2;
	WORD Data3;
	BYTE Data4[8];
} GUID, *PGUID, *LPGUID, IID, CLSID;

/* SOLE_AUTHENTICATION_SERVICE - Authentication service info for CoInitializeSecurity */
typedef struct _SOLE_AUTHENTICATION_SERVICE {
	LPWSTR pAuthenticatorAddress;
	DWORD dwCapabilities;
	DWORD AuthnSvc;
	DWORD AuthzSvc;
} SOLE_AUTHENTICATION_SERVICE, *PSOLE_AUTHENTICATION_SERVICE;

/* COM reference types (CLSID/IID as references) - C++ only */
#ifdef __cplusplus
typedef const GUID &REFCLSID;
typedef const GUID &REFIID;
#else
#define REFCLSID const GUID *
#define REFIID const GUID *
#endif

typedef struct tagVARIANT VARIANT;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"
struct tagVARIANT {
	union {
		struct {
			VARTYPE vt;
			WORD wReserved1;
			WORD wReserved2;
			WORD wReserved3;
			union {
				LONGLONG llVal;
				LONG lVal;
				BYTE bVal;
				SHORT iVal;
				FLOAT fltVal;
				DOUBLE dblVal;
				VARIANT_BOOL boolVal;
				VARIANT_BOOL __OBSOLETE__VARIANT_BOOL;
				BSTR bstrVal;
				IUnknown *punkVal;
				IDispatch *pdispVal;
				SAFEARRAY *parray;
				BYTE *pbVal;
				SHORT *piVal;
				LONG *plVal;
				LONGLONG *pllVal;
				FLOAT *pfltVal;
				DOUBLE *pdblVal;
				VARIANT_BOOL *pboolVal;
				VARIANT_BOOL *__OBSOLETE__VARIANT_PBOOL;
				BSTR *pbstrVal;
				IUnknown **ppunkVal;
				IDispatch **ppdispVal;
				SAFEARRAY **pparray;
				VARIANT *pvarVal;
				PVOID byref;
				CHAR cVal;
				USHORT uiVal;
				ULONG ulVal;
				ULONGLONG ullVal;
				INT intVal;
				UINT uintVal;
				DECIMAL *pdecVal;
				CHAR *pcVal;
				USHORT *puiVal;
				ULONG *pulVal;
				ULONGLONG *pullVal;
				INT *pintVal;
				UINT *puintVal;
				struct {
					PVOID pvRecord;
					IRecordInfo *pRecInfo;
				};
			};
		};
		DECIMAL decVal;
	};
};

#pragma clang diagnostic pop

typedef VARIANT *LPVARIANT;
typedef VARIANT VARIANTARG;
typedef VARIANT *LPVARIANTARG;

struct _UNICODE_STRING {
	WORD Length; //0x0
	WORD MaximumLength; //0x2
	PWCHAR Buffer; //0x8
};

typedef struct _LIST_ENTRY {
	struct _LIST_ENTRY *Flink;
	struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY, PRLIST_ENTRY;

typedef struct _COORD {
	SHORT X;
	SHORT Y;
} COORD, *PCOORD;

typedef struct _SMALL_RECT {
	SHORT Left;
	SHORT Top;
	SHORT Right;
	SHORT Bottom;
} SMALL_RECT, *PSMALL_RECT;

typedef struct _GROUP_AFFINITY {
	KAFFINITY Mask;
	USHORT Group;
	USHORT Reserved[3];
} GROUP_AFFINITY, *PGROUP_AFFINITY;

typedef struct _GROUP_AFFINITY32 {
	ULONG Mask;
	USHORT Group;
	USHORT Reserved[3];
} GROUP_AFFINITY32, *PGROUP_AFFINITY32;

typedef struct _GROUP_AFFINITY64 {
	unsigned __int64 Mask;
	USHORT Group;
	USHORT Reserved[3];
} GROUP_AFFINITY64, *PGROUP_AFFINITY64;

#endif // SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_
