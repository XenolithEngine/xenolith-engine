/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMENOTNULL_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMENOTNULL_H_

#include "SPRuntimeInt.h"

// see: https://stackoverflow.com/questions/63493968/reproducing-clangs-builtin-assume-for-gcc

// preferred option: C++ standard attribute
#ifdef __has_cpp_attribute
#if __has_cpp_attribute(assume) >= 202'207L
#define SPRT_ASSUME(...) [[assume(__VA_ARGS__)]]
#endif
#endif
// first fallback: compiler intrinsics/attributes for assumptions
#ifndef SPRT_ASSUME
#if defined(__clang__)
#define SPRT_ASSUME(...) do { __builtin_assume(__VA_ARGS__); } while(0)
#elif defined(_MSC_VER)
#define SPRT_ASSUME(...) do { __assume(__VA_ARGS__); } while(0)
#elif defined(__GNUC__)
#if __GNUC__ >= 13
#define SPRT_ASSUME(...) __attribute__((__assume__(__VA_ARGS__)))
#endif
#endif
#endif
// second fallback: possibly evaluating uses of unreachable()
#if !defined(SPRT_ASSUME)
#if defined(__GNUC__)
#define SPRT_ASSUME(...) do { if (!bool(__VA_ARGS__)) __builtin_unreachable(); } while(0)
#elif __cpp_lib_unreachable >= 202'202L
#include <utility>
#define SPRT_ASSUME(...) do { if (!bool(__VA_ARGS__)) ::std::unreachable(); ) while(0)
#endif
#endif
// last fallback: define macro as doing nothing
#ifndef SPRT_ASSUME
#define SPRT_ASSUME(...)
#endif

namespace sprt {

// based on https://github.com/microsoft/GSL/blob/main/include/gsl/pointers

template <typename T>
class NotNull final {
public:
	using element_type = T *;

	constexpr NotNull(element_type u) noexcept SPRT_NONNULL : _ptr(u) {
		SPRT_ASSUME(_ptr != nullptr);
	}

	template <typename U, typename = enable_if_t<is_convertible<U *, element_type>::value>>
	constexpr NotNull(const NotNull<U> &other) noexcept : NotNull(other.get()) { }

	NotNull(const NotNull &other) noexcept = default;
	NotNull &operator=(const NotNull &other) noexcept = default;

	constexpr auto get() const noexcept { return _ptr; }

	constexpr operator element_type() const noexcept { return get(); }
	constexpr decltype(auto) operator->() const noexcept { return get(); }
	constexpr decltype(auto) operator*() const noexcept { return *get(); }

	// prevents compilation when someone attempts to assign a null pointer constant
	NotNull(nullptr_t) noexcept = delete;
	NotNull &operator=(nullptr_t) noexcept = delete;

	NotNull &operator++() = delete;
	NotNull &operator--() = delete;
	NotNull operator++(int) = delete;
	NotNull operator--(int) = delete;
	NotNull &operator+=(ptrdiff_t) = delete;
	NotNull &operator-=(ptrdiff_t) = delete;
	void operator[](ptrdiff_t) const = delete;

	void swap(NotNull<T> &other) noexcept { sprt::swap(_ptr, other._ptr); }

private:
	element_type _ptr;
};

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMENOTNULL_H_
