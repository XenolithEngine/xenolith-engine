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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_INIT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_INIT_H_

/*
	Stappler Runtime - this is a minimal runtime library to support stappler

	This library implements the necessary functions based on the platform SDK,
	but should not depend on its headers (including the libc and libc++ headers)
*/

#include <sprt/cxx/detail/ctypes.h>

#include <sprt/runtime/config.h>

#define SPRT_UNUSED [[maybe_unused]]
#define SPRT_INLINE [[gnu::always_inline]]
#define SPRT_INLINE_LAMBDA __attribute__((always_inline))

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define SPRT_NONNULL __attribute__((nonnull))
#elif defined(__has_attribute)
#if __has_attribute(nonnull)
#define SPRT_NONNULL __attribute__((nonnull))
#endif // __has_attribute(nonnull)
#else
#define SPRT_NONNULL
#endif


#if defined(__GNUC__) && (__GNUC__ >= 4)
#define SPRT_COVERAGE_TRIVIAL __attribute__ ((no_profile_instrument_function))
#elif defined(__has_attribute)
#define SPRT_COVERAGE_TRIVIAL
#else
#define SPRT_COVERAGE_TRIVIAL
#endif

namespace sprt {

constexpr size_t operator""_length(const char *str, size_t len) { return len; }
constexpr size_t operator""_length(const char16_t *str, size_t len) { return len; }
constexpr size_t operator""_len(const char *str, size_t len) { return len; }
constexpr size_t operator""_len(const char16_t *str, size_t len) { return len; }

constexpr unsigned long long int operator""_GiB(unsigned long long int val) {
	return val * 1'024 * 1'024 * 1'024;
}
constexpr unsigned long long int operator""_MiB(unsigned long long int val) {
	return val * 1'024 * 1'024;
}
constexpr unsigned long long int operator""_KiB(unsigned long long int val) { return val * 1'024; }

constexpr char16_t operator""_c16(unsigned long long int val) { return (char16_t)val; }
constexpr char operator""_c8(unsigned long long int val) { return (char)val; }


} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_INIT_H_
