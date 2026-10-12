// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Shared prologue for the per-file api <-> Windows SDK function-signature parity checks.
//
// Usage (see check-api-time_api.cpp):
//     #define SPRT_API_HEADER <sprt/gen/wrappers/windows/api/<name>.h>
//     #include "api_check.h"
//     #include <windows.h>          // + any domain-specific SDK headers
//     SPRT_FN(GetTickCount);
//
// The api header is pulled in FIRST inside `namespace sprt_api`, with its function
// declarations stripped of C linkage: `__sprt_def.h` is pre-included and its
// __SPRT_BEGIN_DECL/__SPRT_END_DECL redefined to nothing before the api include (the
// api header's own include of __sprt_def.h is guarded, so the empty spelling survives).
// Without that, the wrapper's `extern "C"` declarations and the SDK's would be merged
// as the *same* entity across the namespace, and clang would reject the wrapper's
// deliberate `typedef DWORD NTSTATUS` vs the SDK's `typedef LONG NTSTATUS` (and every
// other spelling difference) as "conflicting types" before any assert below could run.
//
// Struct tags still land inside `namespace sprt_api` (same trick as abi_check.h), so
// the wrapper and SDK struct types are *nominally* distinct and __is_same alone cannot
// compare them; the traits below spell out what "same ABI signature" means instead.
//
// The __SPRT_* value macros and the calling-convention primitives leak as global
// macros from the include; they are dropped here so the SDK's winnt.h defines its own.

#ifndef SPRT_API_HEADER
#error "define SPRT_API_HEADER (e.g. <sprt/gen/wrappers/windows/api/foo.h>) before including api_check.h"
#endif

// The api chain reaches wrappers/windows/winerror.h -> __sprt_winsock.h, which declares the
// cross socket types (SOCKET, fd_set, struct in_addr, __WSAFDIsSet) at *global* scope; the
// SDK's non-LEAN <windows.h> would then redefine them (winsock.h). LEAN keeps windows.h off
// winsock — the winsock/iphlpapi TUs include <winsock2.h> themselves, before the api header.
#define WIN32_LEAN_AND_MEAN 1

#include <sprt/c/bits/__sprt_def.h>
#undef __SPRT_BEGIN_DECL
#undef __SPRT_END_DECL
#undef __SPRT_C_FUNC
#define __SPRT_BEGIN_DECL
#define __SPRT_END_DECL
#define __SPRT_C_FUNC

// fdset.h (pulled in by every api header via constants.h -> winerror.h -> __sprt_winsock.h)
// declares `extern "C" int __WSAFDIsSet(SOCKET, SPRT fd_set *)` under a literal extern "C"
// (the neutralization above cannot reach it); rename SPRT's to a private symbol across the
// api block so it cannot collide with winsock2.h's by C linkage (their fd_set tags differ
// nominally). Undone right after the include; winsock2.h then declares the real one.
#define __WSAFDIsSet __sprt_api_private_WSAFDIsSet

namespace sprt_api {
#define __SPRT_BUILD 1
#define __SPRT_WIN_USE_IMPORT_LIB 0
#define __SPRT_WIN_USE_IMPORT_STRING_LIB 0
#ifdef SPRT_API_PRELUDE
#include SPRT_API_PRELUDE
#endif
#include SPRT_API_HEADER
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

// The wrapper spells HRESULT_FROM_WIN32 as a function-like macro; the SDK's winerror.h
// spells it as a FORCEINLINE *function*, which the leftover macro would rewrite into
// garbage. Every other wrapper function-like macro (SUCCEEDED, MAKEWORD, ...) is also a
// macro in the SDK and just gets redefined over.
#undef HRESULT_FROM_WIN32

// The cross socket headers (in every api chain, via constants.h -> winerror.h) also leak
// the in{,6}_addr field aliases as global macros; the SDK gates IN_ADDR/IN6_ADDR behind
// `#ifndef s_addr` / `#ifndef s6_addr`, so undo them here for clean SDK parses.
#undef __WSAFDIsSet
#undef s_addr
#undef s_host
#undef s_net
#undef s_imp
#undef s_impno
#undef s6_addr

// === signature comparison ==================================================
// What "the generated declaration matches the SDK's" means for a function:
//   - presence: both decltype operands must resolve — a name the wrapper declares
//     that the SDK headers do not (or vice versa) fails to compile, naming it;
//   - calling convention: WINAPI/__stdcall collapses into the plain function type on
//     x86_64/aarch64-pc-windows-msvc, so it participates in the comparison for free;
//   - return type: identical, or both integers of the same width — the wrapper spells
//     the NTSTATUS/HRESULT/LSTATUS family as DWORD (unsigned long) where the SDK spells
//     LONG (long); the bits are what crosses the ABI, and SPRT_STATUS pins the constants
//     the same way;
//   - parameters: identical types (width, signedness, pointee type and cv), except
//     pointers to struct/class types, whose *shape* is pinned struct-by-struct by the
//     SPRT_SIZE/SPRT_OFFSET asserts in the per-module abi checks — here the two sides'
//     tags are nominally distinct (the namespace trick), so pointee size is what is
//     compared. Callback parameters recurse into the same rules.

template <typename A, typename B> struct sprt_abi_param;
template <typename A, typename B> struct sprt_abi_ret;

// SFINAE-safe sizeof: 0 for void and incomplete types (forward-declared opaque tags)
template <typename T> struct sprt_make_void { using type = void; };
template <typename T, typename = void> struct sprt_type_size {
	static constexpr unsigned long long value = 0;
};
template <typename T> struct sprt_type_size<T, typename sprt_make_void<decltype(sizeof(T))>::type> {
	static constexpr unsigned long long value = sizeof(T);
};

template <typename T> struct sprt_is_void { static constexpr bool value = false; };
template <> struct sprt_is_void<void> { static constexpr bool value = true; };
template <> struct sprt_is_void<const void> { static constexpr bool value = true; };
template <> struct sprt_is_void<volatile void> { static constexpr bool value = true; };
template <> struct sprt_is_void<const volatile void> { static constexpr bool value = true; };

template <typename A, typename B> struct sprt_params_eq;
template <typename...> struct sprt_params { };
template <typename LA, typename LB> struct sprt_params_eq {
	static constexpr bool value = false; // arity mismatch (or non-list arguments)
};
template <> struct sprt_params_eq<sprt_params<>, sprt_params<>> {
	static constexpr bool value = true;
};
template <typename A, typename... As, typename B, typename... Bs>
struct sprt_params_eq<sprt_params<A, As...>, sprt_params<B, Bs...>> {
	static constexpr bool value = sprt_abi_param<A, B>::value
			&& sprt_params_eq<sprt_params<As...>, sprt_params<Bs...>>::value;
};

template <typename A, typename B>
struct sprt_abi_ret {
	// integer returns of equal width carry the same bits, which is what the
	// NTSTATUS/HRESULT/LSTATUS family needs (wrapper: DWORD, SDK: LONG); SPRT_STATUS
	// pins their constants the same way. Everything else delegates to the parameter
	// rules (struct-pointer returns, opaque handle returns, enums, ...).
	static constexpr bool value = __is_same(A, B)
			|| ((__is_integral(A) || __is_enum(A)) && (__is_integral(B) || __is_enum(B))
					&& sprt_type_size<A>::value == sprt_type_size<B>::value)
			|| sprt_abi_param<A, B>::value;
};

template <typename A, typename B>
struct sprt_abi_param {
	// scalars: identical spelling (signedness of a *parameter* is API surface, and a
	// wrapper/SDK sign difference stays a failure), or enum-vs-integer of equal width
	// (MINIDUMP_TYPE, FILE_INFO_BY_HANDLE_CLASS, TOKEN_INFORMATION_CLASS, ... — the
	// generator spells SDK enums as fixed-width integers)
	static constexpr bool value = __is_same(A, B)
			|| ((__is_enum(A) ? (__is_integral(B) || __is_enum(B)) : false)
					|| (__is_enum(B) ? (__is_integral(A) || __is_enum(A)) : false))
					&& sprt_type_size<A>::value == sprt_type_size<B>::value
			// by-value struct/union params (COORD, LARGE_INTEGER): equal size; either
			// side incomplete means that side keeps the type opaque
			|| ((__is_class(A) || __is_union(A)) && (__is_class(B) || __is_union(B))
					&& (sprt_type_size<A>::value == sprt_type_size<B>::value
							|| sprt_type_size<A>::value == 0
							|| sprt_type_size<B>::value == 0));
};

// by-value struct/union parameters take the __is_class branch of the pointer rule below;
// the tags differ nominally (the namespace trick) and the field layout is pinned
// struct-by-struct by the SPRT_SIZE/SPRT_OFFSET abi asserts, so size is what remains
// callback parameters: compare the pointed-to function types by the same rules
template <typename RA, typename... AA, typename RB, typename... AB>
struct sprt_abi_param<RA(AA...), RB(AB...)> {
	static constexpr bool value = sprt_abi_ret<RA, RB>::value
			&& sprt_params_eq<sprt_params<AA...>, sprt_params<AB...>>::value;
};
template <typename RA, typename... AA, typename RB, typename... AB>
struct sprt_abi_param<RA(AA..., ...), RB(AB..., ...)> {
	static constexpr bool value = sprt_abi_ret<RA, RB>::value
			&& sprt_params_eq<sprt_params<AA...>, sprt_params<AB...>>::value;
};

// pointer parameters: one body shared by every cv-qualification of the pointers
// themselves — `A *, B *` partial specs do not match a top-level-cv-qualified pointer
// argument, and a `PCUITEMID_CHILD const *` reaches the comparison as `const T *const`
template <typename A, typename B> struct sprt_abi_ptr {
	static constexpr bool value = __is_same(A, B)
			// struct pointers: same rationale as by-value struct params — equal size,
			// with either side size 0 meaning that side keeps the type opaque
			// (IUnknown, MINIDUMP_USER_STREAM_INFORMATION, forward-declared tags)
			|| ((__is_class(A) || __is_union(A)) && (__is_class(B) || __is_union(B))
					&& (sprt_type_size<A>::value == sprt_type_size<B>::value
							|| sprt_type_size<A>::value == 0
							|| sprt_type_size<B>::value == 0))
			// erased object pointer: the SDK voids out what the wrapper types (PSID =
			// void * in the SDK vs struct _SID * here) or vice versa (HANDLE vs the
			// SDK's struct HINSTANCE__ *) — an opaque object pointer either way
			|| (sprt_is_void<A>::value && (__is_class(B) || __is_union(B)))
			|| (sprt_is_void<B>::value && (__is_class(A) || __is_union(A)))
			// pointer-to-pointer and function pointees (callback parameters) recurse
			// via the partial specs on sprt_abi_param below
			|| sprt_abi_param<A, B>::value;
};

template <typename A, typename B>
struct sprt_abi_param<A *, B *> : sprt_abi_ptr<A, B> { };
template <typename A, typename B>
struct sprt_abi_param<A *const, B *> : sprt_abi_ptr<A, B> { };
template <typename A, typename B>
struct sprt_abi_param<A *, B *const> : sprt_abi_ptr<A, B> { };
template <typename A, typename B>
struct sprt_abi_param<A *const, B *const> : sprt_abi_ptr<A, B> { };

template <typename A, typename B>
struct sprt_abi_param<A &, B &> {
	// reference params (REFCLSID/REFIID spell `const GUID &`): same rules as pointers
	static constexpr bool value = __is_same(A, B)
			|| ((__is_class(A) || __is_union(A)) && (__is_class(B) || __is_union(B))
					&& (sprt_type_size<A>::value == sprt_type_size<B>::value
							|| sprt_type_size<A>::value == 0
							|| sprt_type_size<B>::value == 0));
};

template <typename P1, typename P2> struct sprt_abi_signature {
	static constexpr bool value = false; // not both function pointers with a prototype
};
template <typename R1, typename... A1, typename R2, typename... A2>
struct sprt_abi_signature<R1 (*)(A1...), R2 (*)(A2...)> {
	static constexpr bool value = sprt_abi_ret<R1, R2>::value
			&& sprt_params_eq<sprt_params<A1...>, sprt_params<A2...>>::value;
};
template <typename R1, typename... A1, typename R2, typename... A2>
struct sprt_abi_signature<R1 (*)(A1..., ...), R2 (*)(A2..., ...)> {
	static constexpr bool value = sprt_abi_ret<R1, R2>::value
			&& sprt_params_eq<sprt_params<A1...>, sprt_params<A2...>>::value;
};

// Signature parity for one wrapper function against the identically named SDK one.
// The __is_same term is redundant with the trait but keeps clang's diagnostic useful:
// a failed static_assert prints the substituted pointer types of both sides inline.
#define SPRT_FN(name) \
	static_assert(__is_same(decltype(&sprt_api::name), decltype(&::name)) \
			|| sprt_abi_signature<decltype(&sprt_api::name), decltype(&::name)>::value \
			/* __declspec(noreturn) (ExitThread/ExitProcess) models as a function-type */ \
			/* attribute: a distinct type no partial spec matches, but it drops implicitly */ \
			|| __is_constructible(decltype(&sprt_api::name), decltype(&::name)) \
			|| __is_constructible(decltype(&::name), decltype(&sprt_api::name)), \
			#name ": wrapper signature != Windows SDK")

// Signature parity against a differently-spelled SDK function (A/W sets, macro-renamed
// dbghelp entry points, ...): SPRT_FN_MAP(SymInitialize, SymInitializeW);
#define SPRT_FN_MAP(sprt, native) \
	static_assert(__is_same(decltype(&sprt_api::sprt), decltype(&::native)) \
			|| sprt_abi_signature<decltype(&sprt_api::sprt), decltype(&::native)>::value \
			|| __is_constructible(decltype(&sprt_api::sprt), decltype(&::native)) \
			|| __is_constructible(decltype(&::native), decltype(&sprt_api::sprt)), \
			#sprt ": wrapper signature != Windows SDK " #native)
