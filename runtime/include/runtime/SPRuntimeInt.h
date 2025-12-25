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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEINT_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEINT_H_

#include <c/__sprt_stddef.h>
#include <c/__sprt_stdint.h>
#include <c/bits/__sprt_ssize_t.h>
#include <c/bits/__sprt_time_t.h>

#include "SPRuntimeInit.h" // IWYU pragma: keep

namespace sprt {

using uint8_t = __SPRT_ID(uint8_t);
using int8_t = __SPRT_ID(int8_t);

using uint16_t = __SPRT_ID(uint16_t);
using int16_t = __SPRT_ID(int16_t);

using uint32_t = __SPRT_ID(uint32_t);
using int32_t = __SPRT_ID(int32_t);

using uint64_t = __SPRT_ID(uint64_t);
using int64_t = __SPRT_ID(int64_t);

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

static_assert(sizeof(uint8_t) == 1, "Invalid int length");
static_assert(sizeof(int8_t) == 1, "Invalid int length");
static_assert(sizeof(uint16_t) == 2, "Invalid int length");
static_assert(sizeof(int16_t) == 2, "Invalid int length");
static_assert(sizeof(uint32_t) == 4, "Invalid int length");
static_assert(sizeof(int32_t) == 4, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");

template <typename T>
struct is_integral {
	static constexpr bool value = __is_integral(T);
};

template <typename T>
inline constexpr bool is_integral_v = __is_integral(T);

template <typename T>
struct is_enum {
	static constexpr bool value = __is_enum(T);
};

template <typename T>
inline constexpr bool is_enum_v = __is_enum(T);

template <typename T>
struct is_unsigned {
	static constexpr bool value = __is_unsigned(T);
};

template <typename T>
inline constexpr bool is_unsigned_v = __is_unsigned(T);

template <typename T, bool>
struct __underlying_type_impl;

template <typename T>
struct __underlying_type_impl<T, false> { };

template <typename T>
struct __underlying_type_impl<T, true> {
	using type = __underlying_type(T);
};

template <typename T>
struct underlying_type : __underlying_type_impl<T, is_enum<T>::value> { };

template <typename T>
using underlying_type_t = typename underlying_type<T>::type;

template <typename T>
struct ToIntWrapperType {
	static_assert(is_integral_v<T> or is_enum_v<T>);

	using type = decltype([]() {
		if constexpr (is_enum_v<T>) {
			return underlying_type_t<T>();
		} else {
			return T{};
		}
	}());
};

template <typename E>
constexpr typename ToIntWrapperType<E>::type toInt(const E &e) {
	return static_cast<typename ToIntWrapperType<E>::type>(e);
}

template <typename T>
constexpr T Max;

template <>
inline constexpr uint8_t Max<uint8_t> = 255;

template <>
inline constexpr uint16_t Max<uint16_t> = 0xFFFF;

template <>
inline constexpr uint32_t Max<uint32_t> = 0xFFFF'FFFF;

template <>
inline constexpr uint64_t Max<uint64_t> = 0xFFFF'FFFF'FFFF'FFFF;

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

template <class _Tp, _Tp... _Ip>
struct integer_sequence {
	typedef _Tp value_type;
	static_assert(is_integral<_Tp>::value,
			"std::integer_sequence can only be instantiated with an integral type");
	static constexpr size_t size() noexcept { return sizeof...(_Ip); }
};

template <size_t... _Ip>
using index_sequence = integer_sequence<size_t, _Ip...>;

#if __has_builtin(__make_integer_seq)

template <class _Tp, _Tp _Ep>
using make_integer_sequence = __make_integer_seq<integer_sequence, _Tp, _Ep>;

#elif __has_builtin(__integer_pack)

template <class _Tp, _Tp _SequenceSize>
using make_integer_sequence = integer_sequence<_Tp, __integer_pack(_SequenceSize)...>;

#else
#error "No known way to get an integer pack from the compiler"
#endif

template <size_t _Np>
using make_index_sequence = make_integer_sequence<size_t, _Np>;

template <typename T>
inline bool hasFlag(T mask, T flag) {
	return (mask & flag) != T(0);
}

template <typename T>
inline bool hasFlagAll(T mask, T flag) {
	return (mask & flag) == T(flag);
}

} // namespace sprt

/** SPRT_DEFINE_ENUM_AS_MASK is utility to make a bitwise-mask from typed enum
 * It defines a set of overloaded operators, that allow some bitwise operations
 * on this enum class
 *
 * Type should be unsigned, and SDK code style suggests to make it sized (uint32_t, uint64_t)
 */
#define SPRT_DEFINE_ENUM_AS_MASK(Type) \
	static_assert(::sprt::is_unsigned_v<::sprt::underlying_type_t<Type>>, #Type " should be unsigned");\
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator | (const Type &l, const Type &r) { return Type(::sprt::toInt(l) | ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator & (const Type &l, const Type &r) { return Type(::sprt::toInt(l) & ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator ^ (const Type &l, const Type &r) { return Type(::sprt::toInt(l) ^ ::sprt::toInt(r)); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator |= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) | ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator &= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) & ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type & operator ^= (Type &l, const Type &r) { l = Type(::sprt::toInt(l) ^ ::sprt::toInt(r)); return l; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator == (const Type &l, const ::sprt::underlying_type<Type>::type &r) { return ::sprt::toInt(l) == r; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator == (const ::sprt::underlying_type<Type>::type &l, const Type &r) { return l == ::sprt::toInt(r); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator != (const Type &l, const ::sprt::underlying_type<Type>::type &r) { return ::sprt::toInt(l) != r; } \
	SPRT_COVERAGE_TRIVIAL constexpr inline bool operator != (const ::sprt::underlying_type<Type>::type &l, const Type &r) { return l != ::sprt::toInt(r); } \
	SPRT_COVERAGE_TRIVIAL constexpr inline Type operator~(const Type &t) { return Type(~::sprt::toInt(t)); }

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEINT_H_
