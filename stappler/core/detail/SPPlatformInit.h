/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

// Do not use autoformatter for this file - it can break things
// clang-format off

#ifndef CORE_CORE_DETAIL_SPPLATFORMDETECTION_H_
#define CORE_CORE_DETAIL_SPPLATFORMDETECTION_H_

/*
 * General section
 *
 * Common macro flags, visibility rules and other non-specific definitions
 */

// We want _Float16 and _Float64
#define __STDC_WANT_IEC_60559_TYPES_EXT__ 1

// For MinGW - inform about threads
#ifdef __MINGW32__
#define _POSIX_THREAD_SAFE_FUNCTIONS 1
#endif

// Visibility rules (assume -fvisibility=hidden -fvisibility-inlines-hidden)

// SP_LOCAL should be always hidden
// SP_PUBLIC is the stappler interface, it should be hidden in the final library, but public for the libraries of the SDK itself
// SP_API this is an external library interface that should always be public

#if WIN32

#ifdef __GNUC__
#define SP_API [[gnu::dllexport]]
#else
#define SP_API __declspec(dllexport)
#endif

#ifdef SP_BUILD_APPLICATION
#define SP_PUBLIC
#elif SP_BUILD_SHARED_LIBRARY
#ifdef __GNUC__
#define SP_PUBLIC [[gnu::dllexport]]
#else
#define SP_PUBLIC __declspec(dllexport)
#endif
#else // SP_BUILD_APPLICATION
#ifdef __GNUC__
#define SP_PUBLIC [[gnu::dllimport]]
#else
#define SP_PUBLIC __declspec(dllimport)
#endif
#endif
#define SP_LOCAL

#elif ANDROID

#ifdef SP_BUILD_SHARED_LIBRARY
#define SP_API [[gnu::visibility("default")]]
#define SP_PUBLIC [[gnu::visibility("hidden")]]
#define SP_LOCAL  [[gnu::visibility("hidden")]]
#else
#define SP_API [[gnu::visibility("default")]]
#define SP_PUBLIC [[gnu::visibility("hidden")]]
#define SP_LOCAL  [[gnu::visibility("hidden")]]
#endif

#else

#if __GNUC__ >= 4
#ifdef SP_BUILD_EXTERNAL_LIBRARY
#define SP_API [[gnu::visibility("default")]]
#define SP_PUBLIC [[gnu::visibility("hidden")]]
#define SP_LOCAL  [[gnu::visibility("hidden")]]
#else
#define SP_API [[gnu::visibility("default")]]
#define SP_PUBLIC [[gnu::visibility("default")]]
#define SP_LOCAL  [[gnu::visibility("hidden")]]
#endif
#else
#define SP_API
#define SP_PUBLIC
#define SP_LOCAL
#endif

#endif


// SP_USED - prevent symbols from being removed on linkage
#if defined __has_attribute
#if __has_attribute (used) && __has_attribute (retain)
#define SP_USED [[gnu::used, gnu::retain]]
#elif __has_attribute (used)
#define SP_USED [[gnu::used]]
#else
#define SP_USED
#endif
#else
#define SP_USED
#endif

#define SPUNUSED [[maybe_unused]]
#define SPINLINE __attribute__((always_inline))

// Add debug flag if none is specified
#ifndef DEBUG
#ifndef NDEBUG
#define DEBUG 1
#endif
#endif

// versionized prefix for stappler library
#ifndef STAPPLER_VERSION_PREFIX
#define STAPPLER_VERSIONIZED
#define STAPPLER_VERSIONIZED_NAMESPACE ::stappler
#else
#define STAPPLER_VERSIONIZED STAPPLER_VERSION_PREFIX
#define STAPPLER_VERSIONIZED_NAMESPACE ::STAPPLER_VERSION_PREFIX::stappler
#endif


#ifdef __cplusplus

#define SP_EXTERN_C			extern "C"

// SP_SOURCE_DEBUG controls source info output in log and async tasks tags
#if DEBUG
#define SP_SOURCE_DEBUG 1
#else
#define SP_SOURCE_DEBUG 0
#endif

// Macro to print current active function context name
// Available only from C++20
// Widely used as 'tag' for async tasks
//
// Note that, not like `__func__`, this returns name by constructing argument
// in place of a call, instead of macro substitution in place of occurrence
#if SP_SOURCE_DEBUG
#if __cplusplus >= 202002L
#define __STAPPLER_LOCATION (std::source_location::current().function_name())
#define __STAPPLER_LOCATION_FULL (STAPPLER_VERSIONIZED_NAMESPACE::SourceLocation(std::source_location::current()))
#else
#define __STAPPLER_LOCATION ("")
#define __STAPPLER_LOCATION_FULL (STAPPLER_VERSIONIZED_NAMESPACE::SourceLocation())
#endif
#else
#define __STAPPLER_LOCATION ("")
#define __STAPPLER_LOCATION_FULL (STAPPLER_VERSIONIZED_NAMESPACE::SourceLocation())
#endif // SP_SOURCE_DEBUG

#define SP_FUNC __STAPPLER_LOCATION
#define SP_LOCATION __STAPPLER_LOCATION_FULL

#else
#define SP_EXTERN_C
#define __STAPPLER_LOCATION ("")
#endif // __cplusplus

/*
 * Platform-specific section
 */

/* SP_HAVE_DEDICATED_SIZE_T
 * - Defined as 1 if platform's size_t defined as dedicated integral type
 * (not uint/int)
 */

#if MACOS
#define SP_HAVE_DEDICATED_SIZE_T 1
#else
#define SP_HAVE_DEDICATED_SIZE_T 0
#endif


// Suppress windows MIN/MAX macro
// Actually, windows-specific includes should be only in SPPlatformUnistd.h with proper filters for macro leaking
#if WIN32

#define SP_POSIX_FD 0

#if XWIN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#define NOMINMAX 1
#define _USE_MATH_DEFINES 1

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#else

#define SP_POSIX_FD 1

#endif

#if DEBUG
#if defined(__clang__)
#define SP_BREAKPOINT() __builtin_debugtrap()
#elif defined(__GNUC__)
#define SP_BREAKPOINT() raise(SIGTRAP)
#else
#define SP_BREAKPOINT()
#endif
#else
#define SP_BREAKPOINT()
#endif

#endif /* CORE_CORE_DETAIL_SPPLATFORMDETECTION_H_ */
