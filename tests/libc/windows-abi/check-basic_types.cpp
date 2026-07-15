// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/basic_types.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// basic_types.h is almost entirely primitive typedefs (DWORD, HANDLE, ULONG_PTR,
// ...) and function-like helper macros (MAKEWORD/LOWORD/HIWORD/...) that carry no
// SDK struct layout. There are no integer object-like #define __SPRT_* constants
// and no SDK structs unique to this header, so the meaningful check is that the
// scalar typedefs still have the same width/representation as the SDK's, which is
// what every other abi/*.h layout check silently depends on.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/basic_types.h>
#include "abi_check.h"

#include <windows.h>   // DWORD, HANDLE, ULONG_PTR, ... (windef.h / winnt.h)

// scalar typedef width parity (sizeof only; signedness/rank pinned by usage)
#define SPRT_TYPE_SIZE(t) \
	static_assert(sizeof(sprt_abi::t) == sizeof(::t), "sizeof(" #t ") != SDK")

// === integer / pointer-width scalar typedefs ===============================
SPRT_TYPE_SIZE(BYTE);
SPRT_TYPE_SIZE(WORD);
SPRT_TYPE_SIZE(DWORD);
SPRT_TYPE_SIZE(DWORDLONG);
SPRT_TYPE_SIZE(DWORD_PTR);
SPRT_TYPE_SIZE(DWORD64);
SPRT_TYPE_SIZE(UCHAR);
SPRT_TYPE_SIZE(USHORT);
SPRT_TYPE_SIZE(UINT);
SPRT_TYPE_SIZE(ULONG);
SPRT_TYPE_SIZE(ULONGLONG);
SPRT_TYPE_SIZE(ULONG64);
SPRT_TYPE_SIZE(UINT64);
SPRT_TYPE_SIZE(ULONG_PTR);
SPRT_TYPE_SIZE(UINT_PTR);
SPRT_TYPE_SIZE(SHORT);
SPRT_TYPE_SIZE(INT);
SPRT_TYPE_SIZE(LONG);
SPRT_TYPE_SIZE(LONGLONG);
SPRT_TYPE_SIZE(LONG64);
SPRT_TYPE_SIZE(INT64);
SPRT_TYPE_SIZE(LONG_PTR);
SPRT_TYPE_SIZE(INT_PTR);
SPRT_TYPE_SIZE(FLOAT);
SPRT_TYPE_SIZE(DOUBLE);
SPRT_TYPE_SIZE(BOOL);
SPRT_TYPE_SIZE(BOOLEAN);
SPRT_TYPE_SIZE(CHAR);
SPRT_TYPE_SIZE(WCHAR);
SPRT_TYPE_SIZE(SIZE_T);

// === handle / pointer typedefs =============================================
SPRT_TYPE_SIZE(HANDLE);
SPRT_TYPE_SIZE(HMODULE);
SPRT_TYPE_SIZE(HINSTANCE);
SPRT_TYPE_SIZE(HKEY);
SPRT_TYPE_SIZE(PVOID);
SPRT_TYPE_SIZE(LPVOID);

// === derived scalar typedefs ===============================================
SPRT_TYPE_SIZE(WPARAM);
SPRT_TYPE_SIZE(LPARAM);
SPRT_TYPE_SIZE(LRESULT);
SPRT_TYPE_SIZE(ATOM);
SPRT_TYPE_SIZE(VARIANT_BOOL);
SPRT_TYPE_SIZE(VARTYPE);
SPRT_TYPE_SIZE(ACCESS_MASK);
SPRT_TYPE_SIZE(REGSAM);
SPRT_TYPE_SIZE(KAFFINITY);

// HRESULT/NTSTATUS/LSTATUS are `DWORD` in abi/basic_types.h but `LONG`/`LONG`
// in the SDK (winnt.h); both are 32-bit so ABI-compatible by width, but they are
// signed vs unsigned, so they are intentionally not asserted for identity here.
SPRT_TYPE_SIZE(HRESULT);
SPRT_TYPE_SIZE(NTSTATUS);
SPRT_TYPE_SIZE(LSTATUS);

#define SPRT_TYPE_SAME(t) \
	static_assert(__is_same(sprt_abi::t, ::t), "type " #t " != SDK (exact type)")
SPRT_TYPE_SAME(LPCCH);
SPRT_TYPE_SAME(PCCH);
SPRT_TYPE_SAME(LPCWCH);
SPRT_TYPE_SAME(PCWCH);
