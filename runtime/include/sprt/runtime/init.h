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

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/runtime/config.h>
#include <sprt/runtime/detail/type_categories.h>
#include <sprt/runtime/detail/type_operations.h>
#include <sprt/runtime/detail/type_queries.h>

#include <sprt/c/__sprt_stddef.h>
#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/__sprt_time_t.h>

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

using uint8_t = __SPRT_ID(uint8_t);
using uint16_t = __SPRT_ID(uint16_t);
using uint32_t = __SPRT_ID(uint32_t);
using uint64_t = __SPRT_ID(uint64_t);

using int8_t = __SPRT_ID(int8_t);
using int16_t = __SPRT_ID(int16_t);
using int32_t = __SPRT_ID(int32_t);
using int64_t = __SPRT_ID(int64_t);

using uint_least32_t = __SPRT_ID(uint_least32_t);
using uint_fast32_t = __SPRT_ID(uint_fast32_t);

using size_t = __SPRT_ID(size_t);
using rsize_t = __SPRT_ID(rsize_t);
using ssize_t = __SPRT_ID(ssize_t);
using off_t = __SPRT_ID(off_t);
using time_t = __SPRT_ID(time_t);
using clock_t = __SPRT_ID(clock_t);

// Always 64-bit in SPRT
using off64_t = __SPRT_ID(off_t);
using time64_t = __SPRT_ID(time_t);

using ptrdiff_t = __SPRT_ID(ptrdiff_t);
using uintptr_t = __SPRT_ID(uintptr_t);
using intptr_t = __SPRT_ID(intptr_t);

using max_align_t = __SPRT_ID(max_align_t);

static_assert(sizeof(uint8_t) == 1, "Invalid int length");
static_assert(sizeof(int8_t) == 1, "Invalid int length");
static_assert(sizeof(uint16_t) == 2, "Invalid int length");
static_assert(sizeof(int16_t) == 2, "Invalid int length");
static_assert(sizeof(uint32_t) == 4, "Invalid int length");
static_assert(sizeof(int32_t) == 4, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");


/*
	forward
*/

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &__t) noexcept {
	return static_cast<Type &&>(__t);
}

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &&__t) noexcept {
	static_assert(!is_lvalue_reference<Type>::value, "cannot forward an rvalue as an lvalue");
	return static_cast<Type &&>(__t);
}

/*
	move
*/

// Use sprt::move instead of std::move to enable additional diagnostics
template <typename Type,
		enable_if_t<
				// Attempt to move pointer is most likely an reference count error
				// Use move_unsafe if it's not an error (like, in template context)
				!is_pointer_v<remove_reference_t<Type>>, bool> = true>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}

// Behaves like std::move
template <typename Type>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move_unsafe(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}


template <typename Type>
constexpr inline Type *addressof(Type &__x) noexcept {
	return __builtin_addressof(__x);
}

template <typename _Tp>
struct is_identity : false_type { };

struct identity {
	template <typename _Tp>
	[[nodiscard]]
	constexpr _Tp &&operator()(_Tp &&__t) const noexcept {
		return sprt::forward<_Tp>(__t);
	}

	using is_transparent = void;
};

template <>
struct is_identity<identity> : true_type { };


template <typename Type>
Type &&__declval(int);

template <typename Type>
Type __declval(long);

template <typename Type>
decltype(sprt::__declval<Type>(0)) declval() noexcept {
	static_assert(!__is_same(Type, Type),
                "std::declval can only be used in an unevaluated context. "
                "It's likely that your current usage is trying to extract a value from the function.");
}

inline constexpr bool is_constant_evaluated() noexcept { return __builtin_is_constant_evaluated(); }

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
