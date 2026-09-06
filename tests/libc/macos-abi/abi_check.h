// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// ---------------------------------------------------------------------------
// Shared prologue for the per-file sprt <-> Darwin parity checks.
//
// Usage (see check-errno.cpp):
//     #include <errno.h>                                  // SYSTEM headers first
//     #define SPRT_ABI_HEADER <sprt/c/cross/__sprt_errno.h>
//     #include "abi_check.h"
//     SPRT_CONST(EAGAIN);
//     SPRT_SIZE(__sprt_sockaddr, sockaddr);
//
// ORDER MATTERS, and it is the opposite of the Windows harness.
//
// The system headers must be included FIRST, before this file. sprt's tables
// carry an unprefixed alias block (`#ifndef EPERM` / `#define EPERM
// __SPRT_EPERM` / ...) so that they can stand in for the platform header on a
// freestanding target. If sprt were pulled in first, that block would define the
// bare names itself, and every assert would degrade into a tautology comparing
// __SPRT_X against __SPRT_X -- silently passing even for a value Darwin spells
// differently, and silently passing for a name Darwin does not have AT ALL.
// (Caught for real while writing this: __SPRT_ENOTCAPABLE is 107, but Darwin
// stops at ELAST 106 and has no ENOTCAPABLE; with sprt first the assert passed.)
//
// With the system headers first, sprt's `#ifndef` guards see the name already
// taken and define nothing, so a bare name in an assert is ALWAYS the system's.
// That restores the property the whole harness rests on:
//
//     __SPRT_X and X both exist  -> the values are compared
//     __SPRT_X exists, X doesn't -> error: use of undeclared identifier 'X'
//     X exists, __SPRT_X doesn't -> error: use of undeclared identifier '__SPRT_X'
//
// i.e. the table must carry Darwin's surface *exactly*, and a name sprt invents
// is as much an error as a value it gets wrong. check.sh additionally compiles
// with -Werror=macro-redefined, so an sprt header whose alias block is missing
// its guard cannot quietly take a bare name back.
//
// SPRT_ABI_HEADER is pulled in inside `namespace sprt_abi` with __SPRT_BUILD on,
// so sprt's struct tags become sprt_abi::__sprt_* and never collide with the
// system ones, while the __SPRT_* value macros stay global. A TU needing more
// than one sprt header adds them via SPRT_ABI_HEADER_2..4.
//
// Everything here is a static_assert. Nothing is emitted, nothing is linked.
// ---------------------------------------------------------------------------

#ifndef SPRT_ABI_CHECK_H_
#define SPRT_ABI_CHECK_H_

#ifndef SPRT_ABI_HEADER
#error "define SPRT_ABI_HEADER (e.g. <sprt/c/cross/__sprt_errno.h>) before including abi_check.h"
#endif

namespace sprt_abi {
#define __SPRT_BUILD 1
#include SPRT_ABI_HEADER
#ifdef SPRT_ABI_HEADER_2
#include SPRT_ABI_HEADER_2
#endif
#ifdef SPRT_ABI_HEADER_3
#include SPRT_ABI_HEADER_3
#endif
#ifdef SPRT_ABI_HEADER_4
#include SPRT_ABI_HEADER_4
#endif
#undef __SPRT_BUILD
} // namespace sprt_abi

// value parity for an integer constant: __SPRT_<name> vs the system's <name>.
#define SPRT_CONST(name) \
	static_assert((long long)(__SPRT_##name) == (long long)(name), \
			"__SPRT_" #name " != Darwin " #name)

// value parity against a differently-spelled system constant (e.g. the _SC_*
// table, which sprt spells __SPRT_SC_* without the leading underscore).
#define SPRT_CONST_MAP(sprt, native) \
	static_assert((long long)(__SPRT_##sprt) == (long long)(native), \
			"__SPRT_" #sprt " != Darwin " #native)

// 32-bit bit-pattern parity, for values one side spells signed and the other
// unsigned. The bits are what crosses the ABI.
#define SPRT_BITS32(name) \
	static_assert((unsigned)(__SPRT_##name) == (unsigned)(name), \
			"__SPRT_" #name " != Darwin " #name " (32-bit)")

// sizeof / alignof / field-offset parity. sprt tags are __sprt_-prefixed under
// __SPRT_BUILD, so both sides are always spelled differently -- hence the
// explicit native-type argument (the Windows harness can elide it).
#define SPRT_SIZE(t, native) \
	static_assert(sizeof(sprt_abi::t) == sizeof(::native), \
			"sizeof(" #t ") != Darwin sizeof(" #native ")")
#define SPRT_ALIGN(t, native) \
	static_assert(alignof(sprt_abi::t) == alignof(::native), \
			"alignof(" #t ") != Darwin alignof(" #native ")")
#define SPRT_OFFSET(t, native, field) \
	static_assert(__builtin_offsetof(sprt_abi::t, field) \
					== __builtin_offsetof(::native, field), \
			#t "." #field " offset != Darwin " #native "." #field)
// size + offset in one line, for the common "same field name both sides" case.
#define SPRT_FIELD(t, native, field) \
	SPRT_OFFSET(t, native, field); \
	static_assert(sizeof(((sprt_abi::t *)0)->field) == sizeof(((::native *)0)->field), \
			"width of " #t "." #field " != Darwin " #native "." #field)

// width / signedness parity for a typedef.
#define SPRT_TYPE_SIZE(t, native) \
	static_assert(sizeof(sprt_abi::t) == sizeof(native), \
			"sizeof(" #t ") != Darwin sizeof(" #native ")")
#define SPRT_TYPE_SIGN(t, native) \
	static_assert(((sprt_abi::t) - 1 < (sprt_abi::t)0) == ((native) - 1 < (native)0), \
			"signedness of " #t " != Darwin " #native)

// enum-member parity: sprt spells some enumerators as plain names inside the
// namespace rather than as __SPRT_ macros.
#define SPRT_ENUM(name) \
	static_assert((long long)(sprt_abi::name) == (long long)(name), \
			"sprt_abi::" #name " != Darwin " #name)

// --------------------------------------------------------------------------
// Whole-signature parity.
//
// Comparing decltype(sprt_fn) against decltype(native_fn) directly is too
// strict: sprt deliberately spells Darwin's enum parameter types as the
// underlying integer (os_sync_wait_on_address_flags_t -> unsigned int) and its
// opaque handles as void * (CFRunLoopRef -> void *), because sprt's headers
// must stand alone without the vendor's. Neither difference changes how an
// argument is passed.
//
// So both sides are normalised to their ABI shape first -- an enum becomes its
// underlying type, any object pointer becomes void * -- and the normalised
// function types are compared. What survives is exactly what matters: arity,
// and the size/class of every parameter and of the return value. A parameter
// widened from uint32_t to size_t, or a return type changed from int to long,
// still fails.
namespace sprt_abi_check {

template <class T, bool IsEnum = __is_enum(T)>
struct norm {
	using type = T;
};
template <class T>
struct norm<T, true> {
	using type = __underlying_type(T);
};
template <class T, bool IsEnum>
struct norm<T *, IsEnum> {
	using type = void *;
};

template <class F>
struct signature;
template <class R, class... A>
struct signature<R(A...)> {
	using type = typename norm<R>::type(typename norm<A>::type...);
};
template <class R, class... A>
struct signature<R(A...) noexcept> {
	using type = typename norm<R>::type(typename norm<A>::type...);
};

} // namespace sprt_abi_check

#define SPRT_SIGNATURE(sprt_fn, native_fn) \
	static_assert(__is_same(typename sprt_abi_check::signature<decltype(sprt_abi::sprt_fn)>::type, \
						  typename sprt_abi_check::signature<decltype(::native_fn)>::type), \
			"signature of " #sprt_fn " is not ABI-compatible with Darwin " #native_fn)

#endif // SPRT_ABI_CHECK_H_
