/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DEF_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DEF_H_

#include <sprt/c/bits/__sprt_config.h>

// The platforms and the architectures: SPRT_<PLATFORM>, __SPRT_PLATFORM_NAME/_ID,
// __SPRT_ARCH_NAME/_ID and their lists, generated from runtime/kdl/abi/bits/__sprt_targets.kdl.
#include <sprt/gen/c/bits/__sprt_targets.h>

#if !defined(DEBUG) && !defined(NDEBUG)
#define DEBUG 1
#endif

// clang-format off
#if __STDC_VERSION__ >= 202311L
#define __SPRT_STDC_23__ __STDC_VERSION__
#elif __STDC_VERSION__ >= 201710L
#define __SPRT_STDC_17__ __STDC_VERSION__
#elif __STDC_VERSION__ >= 201112L
#define __SPRT_STDC_11__ __STDC_VERSION__
#else
#define __SPRT_STDC_NONE__
#endif
// clang-format on

#if defined(__has_attribute)
#define __SPRT_HAS_ATTR(Attr) __has_attribute(Attr)
#else
#define __SPRT_HAS_ATTR(Attr) 0
#endif

#define __SPRT_FALLBACK_ATTR(...) __attribute__((__VA_ARGS__))

#if defined(__has_builtin)
#define __SPRT_HAS_BUILTIN(Attr) __has_builtin(Attr)
#else
#define __SPRT_HAS_BUILTIN(Attr) 0
#endif

#ifdef __cplusplus

#define __SPRT_C_FUNC extern "C"
#define __SPRT_BEGIN_DECL	extern "C" {
#define __SPRT_END_DECL	}
// lcc does not accept the [[gnu::...]] standard-attribute spelling (and the
// plain [[...]] form is unevenly supported) — route it through __attribute__.
#if defined(__LCC__) && !defined(__clang__)
#define __SPRT_ATTR(Attr) __attribute__((Attr))
#define __SPRT_ATTR_GNU(Attr) __attribute__((Attr))
#else
#define __SPRT_ATTR(Attr) [[Attr]]
#define __SPRT_ATTR_GNU(Attr) [[gnu::Attr]]
#endif
#define __SPRT_NOEXCEPT noexcept(true)
// #define __SPRT_NORETURN // Will be defined by fallback
#define __SPRT_RESTRICT __restrict

#else // __cplusplus

#define __SPRT_C_FUNC
#define __SPRT_BEGIN_DECL
#define __SPRT_END_DECL
#define __SPRT_ATTR(Attr) __attribute__((Attr))
#define __SPRT_ATTR_GNU(Attr) __attribute__((Attr))
#define __SPRT_NOEXCEPT
#define __SPRT_RESTRICT restrict

#if defined(__STDC_VERSION__)

// clang-format off
#if __STDC_VERSION__ >= 202311L
#define __SPRT_NORETURN [[noreturn]]
#elif __STDC_VERSION__ >= 201710L
#define __SPRT_NORETURN _Noreturn
#elif __STDC_VERSION__ >= 201112L
#define __SPRT_NORETURN _Noreturn
#else
#endif
// clang-format on

#else // defined(__STDC_VERSION__)

#endif // defined(__STDC_VERSION__)

#endif // __cplusplus

#define SPRT_NOEXCEPT __SPRT_NOEXCEPT

#ifndef __SPRT_NORETURN
#if __SPRT_HAS_ATTR(noreturn)
#define __SPRT_NORETURN __SPRT_FALLBACK_ATTR(noreturn)
#else
#define __SPRT_NORETURN
#endif
#endif // #ifndef __SPRT_NORETURN


#if defined(__PRETTY_FUNCTION__)
#define __SPRT_FUNCTION__ __PRETTY_FUNCTION__
#else
#define __SPRT_FUNCTION__ __func__
#endif


/*
	Visibility settings
*/
#if SPRT_WINDOWS
#ifdef __GNUC__
#define __SPRT_DLLEXPORT __SPRT_ATTR_GNU(dllexport)
#define __SPRT_DLLIMPORT __SPRT_ATTR_GNU(dllimport)
#else
#define __SPRT_DLLEXPORT __declspec(dllexport)
#define __SPRT_DLLIMPORT __declspec(dllimport)
#endif

#define __SPRT_VISIBLE
#define __SPRT_HIDDEN

#else // SPRT_WINDOWS

#define __SPRT_DLLEXPORT
#define __SPRT_DLLIMPORT

#define __SPRT_VISIBLE __SPRT_ATTR_GNU(visibility("default"))
#define __SPRT_HIDDEN __SPRT_ATTR_GNU(visibility("hidden"))

#endif // SPRT_WINDOWS

/*
	Empty-base optimization opt-in. The Microsoft ABI needs __declspec(empty_bases)
	for multiple empty base classes to occupy zero space; the Itanium ABI does it
	automatically. Used by types that mix in empty bases (e.g. sprt::optional, which
	derives from a storage base plus empty SFINAE mixins) so their size stays compact
	and identical across targets. Expands to nothing where it does not apply.
*/
#if SPRT_WINDOWS && defined(__has_declspec_attribute)
#if __has_declspec_attribute(empty_bases)
#define __SPRT_EMPTY_BASES __declspec(empty_bases)
#endif
#endif
#ifndef __SPRT_EMPTY_BASES
#define __SPRT_EMPTY_BASES
#endif

// Defined if we actually build runtime, not using it
#if SPRT_BUILD_RUNTIME

#if SPRT_WINDOWS
// SPRT_BUILD_SHARED_RUNTIME is set by <root>/runtime/Makefile when SPRT_SHARED=1. Only
// then does the runtime need an export surface; the default (static sprt.lib) leaves the
// macros empty so nothing lands in an export directory.
#if defined(SPRT_BUILD_SHARED_RUNTIME)
#define SPRT_GLOBAL __SPRT_DLLEXPORT
#define SPRT_API __SPRT_DLLEXPORT
#define SPRT_LOCAL
#else
#define SPRT_GLOBAL
#define SPRT_API
#define SPRT_LOCAL
#endif
#else
#define SPRT_GLOBAL __SPRT_VISIBLE
#define SPRT_API __SPRT_VISIBLE
#define SPRT_LOCAL __SPRT_HIDDEN
#endif

// If we use runtime as an external library
#elif defined(SPRT_SHARED_RUNTIME)

#if SPRT_WINDOWS
#define SPRT_GLOBAL __SPRT_DLLIMPORT
#define SPRT_API __SPRT_DLLIMPORT
#define SPRT_LOCAL
#else // SPRT_WINDOWS
#define SPRT_GLOBAL __SPRT_VISIBLE
#define SPRT_API __SPRT_VISIBLE
#define SPRT_LOCAL __SPRT_HIDDEN
#endif // SPRT_WINDOWS

// If we build with integrated runtime
#else

// SPRT_BUILD_RUNTIME or SPRT_SHARED_RUNTIME is not defined, assume static application

#if SPRT_WINDOWS
#define SPRT_GLOBAL
#define SPRT_API
#define SPRT_LOCAL
#else
#define SPRT_GLOBAL __SPRT_VISIBLE
#define SPRT_API __SPRT_HIDDEN
#define SPRT_LOCAL __SPRT_HIDDEN
#endif

#endif

// For class templates and their partial specializations. dllexport/dllimport has no
// meaning there - only instantiations have linkage on Windows, and clang warns the
// attribute away on partial specializations - while ELF/Mach-O visibility does apply.
#if SPRT_WINDOWS
#define SPRT_TEMPLATE_API
#else
#define SPRT_TEMPLATE_API SPRT_API
#endif

// clang-format off
#if defined(_WIN32) || defined(_WIN64) || defined(__SPRT_WINDOWS)
#define SPRT_ALIGNAS(N) __declspec(align(N))
#elif defined(__GNUC__) || defined(__clang__)
// GNU/clang: use the attribute form. It is valid where SPRT_ALIGNAS is used
// (`typedef struct SPRT_ALIGNAS(N) { ... }`), whereas the C11 `_Alignas(N)` keyword
// is a syntax error between `struct` and `{`. Matters for plain-C freestanding
// builds (e.g. the wasm third-party deps) where __STDC_VERSION__ >= C11 but the
// struct-tag position rules out _Alignas.
#define SPRT_ALIGNAS(N) __SPRT_FALLBACK_ATTR(aligned(N))
#elif __STDC_VERSION__ >= 201112L
#define SPRT_ALIGNAS(N) _Alignas(N)
#else
#define SPRT_ALIGNAS(N) __SPRT_FALLBACK_ATTR(aligned(N))
#endif
// clang-format on

#ifdef __cplusplus

/*
`SPRT_LAMBDAINLINE` is used to mark a lambda function as predominantly inline
`SPRT_ALWAYSINLINE` is applied for the same purposes but for regular functions
*/
#if __SPRT_HAS_ATTR(always_inline)
#define SPRT_LAMBDAINLINE __SPRT_FALLBACK_ATTR(always_inline)
#define SPRT_ALWAYSINLINE __SPRT_FALLBACK_ATTR(always_inline)
#else
#define SPRT_LAMBDAINLINE
#define SPRT_ALWAYSINLINE __forceinline
#endif

#if __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_INTERNAL __SPRT_FALLBACK_ATTR(internal_linkage)
#else
#define SPRT_INTERNAL static
#endif

/*
`SPRT_FORCEINLINE` – marks a function to be used **only** as an inline function.
Unlike `SPRT_ALWAYSINLINE`, such a function should not be visible in the ABI.
*/
#if __SPRT_HAS_ATTR(always_inline) && __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_FORCEINLINE __SPRT_FALLBACK_ATTR(always_inline,internal_linkage) inline
#elif __SPRT_HAS_ATTR(always_inline)
#define SPRT_FORCEINLINE __SPRT_FALLBACK_ATTR(always_inline) inline
#else
#define SPRT_FORCEINLINE __forceinline inline
#endif

#if SPRT_WINDOWS
#define SPRT_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define SPRT_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#else // __cplusplus

#if __SPRT_HAS_ATTR(always_inline) && __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_FORCEINLINE static __SPRT_FALLBACK_ATTR(always_inline,internal_linkage) inline
#elif __SPRT_HAS_ATTR(always_inline)
#define SPRT_FORCEINLINE static __SPRT_FALLBACK_ATTR(always_inline) inline
#else
#define SPRT_FORCEINLINE static __forceinline inline
#endif

#endif // __cplusplus


// If we have any working libc (internal or from OS) - we use ubrella inlines;
// When we have no libc - we define prototypes for libc functions
#ifndef SPRT_UMBRELLA_REQUIRED
#define SPRT_UMBRELLA_REQUIRED (__STDC_HOSTED__ == 1)
#endif

#if SPRT_UMBRELLA_REQUIRED
#define SPRT_UMBRELLA_FUNC SPRT_FORCEINLINE
#define SPRT_UMBRELLA_END __SPRT_NOEXCEPT
#define SPRT_UMBRELLA_END_EXCEPT
#else
#define SPRT_UMBRELLA_FUNC
#define SPRT_UMBRELLA_END __SPRT_NOEXCEPT;
#define SPRT_UMBRELLA_END_EXCEPT ;
#endif


#define __SPRT_ID(Def) __sprt_ ## Def

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DEF_H_
