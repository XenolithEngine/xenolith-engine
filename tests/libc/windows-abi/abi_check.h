// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Shared prologue for the per-file abi <-> Windows SDK parity checks.
//
// Usage (see check-time_api.cpp):
//     #define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/<name>.h>
//     #include "abi_check.h"
//     #include <windows.h>          // + any domain-specific SDK headers
//     SPRT_CONST(SOME_CONSTANT);
//     SPRT_SIZE(SOME_STRUCT); SPRT_OFFSET(SOME_STRUCT, field);
//
// The abi header is pulled in FIRST inside `namespace sprt_abi` (with __SPRT_BUILD
// and the dllimport function tables disabled), so the SPRT struct tags land in the
// namespace and never collide with the SDK ones, while the __SPRT_* value macros
// stay global (the SDK spells them unprefixed). Including the SDK first would break
// abi's `typedef void VOID` / `typedef const GUID& REFCLSID` via the SDK's
// `#define VOID` / `#define REFCLSID`, so order matters. The abi calling-convention
// primitives leak as global macros; they are dropped here so the SDK's winnt.h
// defines its own.

#ifndef SPRT_ABI_HEADER
#error "define SPRT_ABI_HEADER (e.g. <sprt/wrappers/windows/abi/foo.h>) before including abi_check.h"
#endif

namespace sprt_abi {
#define __SPRT_BUILD 1
#define __SPRT_WIN_USE_IMPORT_LIB 0
#define __SPRT_WIN_USE_IMPORT_STRING_LIB 0
#include SPRT_ABI_HEADER
#undef __SPRT_BUILD
}

#undef WINAPI
#undef CALLBACK
#undef NTAPI
#undef BASETYPES
#undef APIENTRY
#undef DECLSPEC_ALIGN
#undef MemoryBarrier
#undef DECLARE_HANDLE
#undef NULL

// sizeof / field-offset parity for a struct shared with the SDK.
#define SPRT_SIZE(t) \
	static_assert(sizeof(sprt_abi::t) == sizeof(::t), "sizeof(" #t ") != SDK")
#define SPRT_OFFSET(t, field) \
	static_assert(__builtin_offsetof(sprt_abi::t, field) == __builtin_offsetof(::t, field), \
			#t "." #field " offset != SDK")

// value parity for an integer constant: __SPRT_<name> vs the SDK's <name>.
#define SPRT_CONST(name) \
	static_assert((long long)(__SPRT_##name) == (long long)(name), \
			"__SPRT_" #name " != SDK " #name)
// value parity against a differently-spelled SDK constant.
#define SPRT_CONST_MAP(sprt, native) \
	static_assert((long long)(__SPRT_##sprt) == (long long)(native), \
			"__SPRT_" #sprt " != SDK " #native)

// enum-member parity: the abi spells enumerators as plain names (sprt_abi::NAME),
// not __SPRT_ macros, so compare the namespaced value against the SDK's global one.
#ifndef SPRT_ENUM
#define SPRT_ENUM(name) \
	static_assert((long long)(sprt_abi::name) == (long long)(name), \
			"sprt_abi::" #name " != SDK " #name)
#endif

// 32-bit *bit-pattern* parity: for NTSTATUS/HRESULT-style codes where the abi spells
// the value `(DWORD)` (unsigned) and the SDK spells it `(NTSTATUS)`/`(long)` (signed).
// The bits are what matter for the ABI; compare them modulo 2^32 to ignore the sign.
#define SPRT_STATUS(name) \
	static_assert((unsigned)(__SPRT_##name) == (unsigned)(name), \
			"__SPRT_" #name " != SDK " #name " (32-bit)")
