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

/*
	Known platforms
*/
#define __SPRT_PLATFORM_NAME_MACOS macos_sprt
#define __SPRT_PLATFORM_ID_MACOS 1

#define __SPRT_PLATFORM_NAME_IOS ios_sprt
#define __SPRT_PLATFORM_ID_IOS 2

#define __SPRT_PLATFORM_NAME_DARWIN_UNKNOWN darwin_sprt
#define __SPRT_PLATFORM_ID_DARWIN_UNKNOWN 3

#define __SPRT_PLATFORM_NAME_WINDOWS windows_sprt
#define __SPRT_PLATFORM_ID_WINDOWS 4

#define __SPRT_PLATFORM_NAME_ANDROID android_sprt
#define __SPRT_PLATFORM_ID_ANDROID 5

#define __SPRT_PLATFORM_NAME_LINUX linux_sprt
#define __SPRT_PLATFORM_ID_LINUX 6


/*
	Defines one of:

	SPRT_MACOS
	SPRT_IOS
	SPRT_DARWIN_UNKNOWN
	SPRT_WINDOWS
	SPRT_ANDROID
	SPRT_LINUX

	for platform detection
*/
#ifdef __APPLE__
#warning TODO: remove this library dependency
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_MACOS
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_MACOS
#define SPRT_MACOS __SPRT_PLATFORM_ID_MACOS
#elif TARGET_OS_IPHONE
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_IOS
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_IOS
#define SPRT_IOS __SPRT_PLATFORM_ID_IOS
#else
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_DARWIN_UNKNOWN
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_DARWIN_UNKNOWN
#define SPRT_DARWIN_UNKNOWN __SPRT_PLATFORM_ID_DARWIN_UNKNOWN
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_WINDOWS
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_WINDOWS
#define SPRT_WINDOWS __SPRT_PLATFORM_ID_WINDOWS
#elif defined(__ANDROID__)
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_ANDROID
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_ANDROID
#define SPRT_ANDROID __SPRT_PLATFORM_ID_ANDROID
#elif defined(__linux__)
#define __SPRT_PLATFORM_NAME __SPRT_PLATFORM_NAME_LINUX
#define __SPRT_PLATFORM_ID __SPRT_PLATFORM_ID_LINUX
#define SPRT_LINUX __SPRT_PLATFORM_ID_LINUX
#else
#error "Unknown platform"
#endif


/*
	Known arches
*/
#define __SPRT_ARCH_NAME_AARCH64 aarch64_sprt
#define __SPRT_ARCH_ID_AARCH64 1

#define __SPRT_ARCH_NAME_ARM arm_sprt
#define __SPRT_ARCH_ID_ARM 2

#define __SPRT_ARCH_NAME_X86 x86_sprt
#define __SPRT_ARCH_ID_X86 3

#define __SPRT_ARCH_NAME_X86_64 x86_64_sprt
#define __SPRT_ARCH_ID_X86_64 4

#define __SPRT_ARCH_NAME_E2K e2k_sprt
#define __SPRT_ARCH_ID_E2K 5

#define __SPRT_ARCH_NAME_WASM64 wasm64_sprt
#define __SPRT_ARCH_ID_WASM64 6

#define __SPRT_ARCH_NAME_WASM32 wasm32_sprt
#define __SPRT_ARCH_ID_WASM32 7

#define __SPRT_ARCH_NAME_RISCV64 riscv64_sprt
#define __SPRT_ARCH_ID_RISCV64 8

#define __SPRT_ARCH_NAME_RISCV32 riscv32_sprt
#define __SPRT_ARCH_ID_RISCV32 9

#define __SPRT_ARCH_NAME_LOONGARCH64 loongarch64_sprt
#define __SPRT_ARCH_ID_LOONGARCH64 10

#define __SPRT_ARCH_NAME_LOONGARCH32 loongarch32_sprt
#define __SPRT_ARCH_ID_LOONGARCH32 11


#if defined(__aarch64__) || defined(_M_ARM64)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_AARCH64
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_AARCH64
#elif defined(__arm__) || defined(_M_ARM)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_ARM
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_ARM
#elif defined(__x86_64__) || defined(_M_X64)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_X86_64
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_X86_64
#elif defined(i386) || defined(__i386__) || defined(_M_IX86)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_X86
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_X86
#elif defined(__wasm64__)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_WASM64
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_WASM64
#elif defined(__wasm32__)
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_WASM32
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_WASM32
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_RISCV64
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_RISCV64
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 32
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_RISCV32
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_RISCV32
#elif defined(__loongarch__) && defined(__loongarch_grlen) && __loongarch_grlen == 64
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_RISCV64
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_RISCV64
#elif defined(__loongarch__) && defined(__loongarch_grlen) && __loongarch_grlen == 32
#define __SPRT_ARCH_NAME __SPRT_ARCH_NAME_RISCV32
#define __SPRT_ARCH_ID __SPRT_ARCH_ID_RISCV32
#else
#error "Unknown arch"
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
#define __SPRT_ATTR(Attr) [[Attr]]
#define __SPRT_ATTR_GNU(Attr) [[gnu::Attr]]
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


// Defined if we actually build runtime, not using it
#if SPRT_BUILD_RUNTIME

#if SPRT_WINDOWS
#define SPRT_GLOBAL __SPRT_DLLEXPORT
#define SPRT_API __SPRT_DLLEXPORT
#define SPRT_LOCAL
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
#define SPRT_API __SPRT_HIDDEN
#define SPRT_LOCAL __SPRT_HIDDEN
#endif // SPRT_WINDOWS

// If we build with integerated runtime
#else

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

// clang-format off
#if __STDC_VERSION__ >= 201112L
#define SPRT_ALIGNAS(N) _Alignas(N)
#else
#define SPRT_ALIGNAS(N) __SPRT_FALLBACK_ATTR(aligned(N))
#endif
// clang-format on

#ifdef __cplusplus

#if __SPRT_HAS_ATTR(always_inline)
#define SPRT_LAMBDAINLINE __SPRT_FALLBACK_ATTR(always_inline)
#else
#define SPRT_LAMBDAINLINE
#endif

#if __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_INTERNAL __SPRT_FALLBACK_ATTR(internal_linkage)
#else
#define SPRT_INTERNAL static
#endif

#if __SPRT_HAS_ATTR(always_inline) && __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_FORCEINLINE __SPRT_FALLBACK_ATTR(always_inline,internal_linkage) inline
#elif __SPRT_HAS_ATTR(always_inline)
#define SPRT_FORCEINLINE __SPRT_FALLBACK_ATTR(always_inline) inline
#else
#define SPRT_FORCEINLINE __forceinline inline
#endif

#else

#if __SPRT_HAS_ATTR(always_inline) && __SPRT_HAS_ATTR(internal_linkage)
#define SPRT_FORCEINLINE static __SPRT_FALLBACK_ATTR(always_inline,internal_linkage) inline
#elif __SPRT_HAS_ATTR(always_inline)
#define SPRT_FORCEINLINE static __SPRT_FALLBACK_ATTR(always_inline) inline
#else
#define SPRT_FORCEINLINE static __forceinline inline
#endif

#endif

#define __SPRT_ID(Def) __sprt_ ## Def

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DEF_H_
