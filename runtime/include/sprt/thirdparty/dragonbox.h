// Copyright 2020-2025 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

// We want to achieve the followings regarding the macro leakage of this header file:
//   - Normally, no macro other than the header guard JKJ_HEADER_DRAGONBOX is leaked.
//   - For internal library use, macros defined below are leaked if explicitly requested.
//   - This header file does not include any other header file in this library.
//   - The order of inclusions and repeated inclusions of header files in this library must not matter.
// To achieve all of these at once, we do some preprocessing hack:
//   - Macros are populated if JKJ_HEADER_DRAGONBOX is not defined (which means the header file is being
//   included for the first time), or JKJ_DRAGONBOX_LEAK_MACROS is defined and
//   JKJ_DRAGONBOX_MACROS_DEFINED is not defined at the same time.
//   - JKJ_DRAGONBOX_MACROS_DEFINED is defined at the end of this header file if (and only if)
//   JKJ_DRAGONBOX_LEAK_MACROS is defined.
//   - The actual content (excluding the macros) of this header file is guarded by JKJ_HEADER_DRAGONBOX
//   as usual.
//   - JKJ_DRAGONBOX_MACROS_DEFINED and all macros defined below except for JKJ_HEADER_DRAGONBOX are
//   #undef-ed at the end of this header if JKJ_DRAGONBOX_LEAK_MACROS is not defined.

#ifndef JKJ_HEADER_DRAGONBOX
// User-provided header file that contains declarations of custom implementations and config macros.
#ifdef JKJ_DRAGONBOX_CONFIG_HEADER_NAMES
#include JKJ_DRAGONBOX_CONFIG_HEADER_NAME
#endif
#endif

#if !defined(JKJ_HEADER_DRAGONBOX) \
		|| (defined(JKJ_DRAGONBOX_LEAK_MACROS) && !defined(JKJ_DRAGONBOX_MACROS_DEFINED))
// Users vendoring this library are advised to define the macro JKJ_NAMESPACE to avoid potential
// clash with other libraries vendoring this library. Every (non-macro) entity in this library will
// live inside the namespace JKJ_NAMESPACE, whose default is "jkj".
#ifndef JKJ_NAMESPACE
#define JKJ_NAMESPACE jkj
#else
#define JKJ_NAMESPACE_DEFINED 1
#endif

// Attribute for storing static data into a dedicated place, e.g. flash memory. Every ODR-used
// static data declaration will be decorated with this macro. The users may define this macro,
// before including the library headers, into whatever they want.
#ifndef JKJ_STATIC_DATA_SECTION
#define JKJ_STATIC_DATA_SECTION
#else
#define JKJ_STATIC_DATA_SECTION_DEFINED 1
#endif

// To use the library with toolchains without standard C++ headers, the users may define this macro
// into their custom namespace which contains the definitions of all the standard C++ library
// features used in this header. (The list can be found below.)
#ifndef JKJ_STD_REPLACEMENT_NAMESPACE
#define JKJ_STD_REPLACEMENT_NAMESPACE sprt
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_assert.h>
#include <sprt/cxx/cstring>
#include <sprt/cxx/bit>
#include <sprt/cxx/__type_traits/modifications.h>
#else
#define JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED 1
#endif

// The compressed cache handling for IEEE-754 binary32 performs 64-bit x 32-bit -> 96-bit
// multiplication. The library chooses between 64-bit + 32-bit and 32-bit + 64-bit splittings of the
// 96-bit result depending on availability of fast 64-bit x 64-bit -> 64-bit multiplication of
// unsigned integers. By default, the latter is used if and only if one of the macros listed below
// is defined, but the user can override this behavior by defining JKJ_FAST_MUL64. This should have
// no impact on the codegen if compact cache for IEEE-754 binary32 is not used.
#ifndef JKJ_FAST_MUL64
#if defined(__x86_64__) || defined(__aarch64__) || defined(__ppc64__) || defined(_M_X64) \
		|| defined(_M_ARM64)
#define JKJ_FAST_MUL64 1
#else
#define JKJ_FAST_MUL64 0
#endif
#else
#define JKJ_FAST_MUL64_DEFINED 1
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Language feature detections.
////////////////////////////////////////////////////////////////////////////////////////

// C++14 variable templates
#if defined(__cpp_variable_templates) && __cpp_variable_templates >= 201'304L
#define JKJ_HAS_VARIABLE_TEMPLATES 1
#elif __cplusplus >= 201'402L
#define JKJ_HAS_VARIABLE_TEMPLATES 1
#elif defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 190'023'918L && _MSVC_LANG >= 201'402L
#define JKJ_HAS_VARIABLE_TEMPLATES 1
#else
#define JKJ_HAS_VARIABLE_TEMPLATES 0
#endif

// C++14 constexpr
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201'304L
#define JKJ_HAS_CONSTEXPR14 1
#elif __cplusplus >= 201'402L
#define JKJ_HAS_CONSTEXPR14 1
#elif defined(_MSC_VER) && _MSC_VER >= 1'910 && _MSVC_LANG >= 201'402L
#define JKJ_HAS_CONSTEXPR14 1
#else
#define JKJ_HAS_CONSTEXPR14 0
#endif

#if JKJ_HAS_CONSTEXPR14
#define JKJ_CONSTEXPR14 constexpr
#else
#define JKJ_CONSTEXPR14
#endif

// C++17 constexpr lambdas
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201'603L
#define JKJ_HAS_CONSTEXPR17 1
#elif __cplusplus >= 201'703L
#define JKJ_HAS_CONSTEXPR17 1
#elif defined(_MSC_VER) && _MSC_VER >= 1'911 && _MSVC_LANG >= 201'703L
#define JKJ_HAS_CONSTEXPR17 1
#else
#define JKJ_HAS_CONSTEXPR17 0
#endif

// C++17 inline variables
#if defined(__cpp_inline_variables) && __cpp_inline_variables >= 201'606L
#define JKJ_HAS_INLINE_VARIABLE 1
#elif __cplusplus >= 201'703L
#define JKJ_HAS_INLINE_VARIABLE 1
#elif defined(_MSC_VER) && _MSC_VER >= 1'912 && _MSVC_LANG >= 201'703L
#define JKJ_HAS_INLINE_VARIABLE 1
#else
#define JKJ_HAS_INLINE_VARIABLE 0
#endif

#if JKJ_HAS_INLINE_VARIABLE
#define JKJ_INLINE_VARIABLE inline constexpr
#else
#define JKJ_INLINE_VARIABLE static constexpr
#endif

// C++17 if constexpr
#if defined(__cpp_if_constexpr) && __cpp_if_constexpr >= 201'606L
#define JKJ_HAS_IF_CONSTEXPR 1
#elif __cplusplus >= 201'703L
#define JKJ_HAS_IF_CONSTEXPR 1
#elif defined(_MSC_VER) && _MSC_VER >= 1'911 && _MSVC_LANG >= 201'703L
#define JKJ_HAS_IF_CONSTEXPR 1
#else
#define JKJ_HAS_IF_CONSTEXPR 0
#endif

#if JKJ_HAS_IF_CONSTEXPR
#define JKJ_IF_CONSTEXPR if constexpr
#else
#define JKJ_IF_CONSTEXPR if
#endif

#define JKJ_HAS_BIT_CAST 1

// C++23 if consteval or C++20 std::is_constant_evaluated
#if defined(__cpp_if_consteval) && __cpp_is_consteval >= 202'106L
#define JKJ_IF_CONSTEVAL if consteval
#define JKJ_IF_NOT_CONSTEVAL if !consteval
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
#define JKJ_USE_IS_CONSTANT_EVALUATED 0
#elif JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
#if JKJ_STD_REPLACEMENT_HAS_IS_CONSTANT_EVALUATED
#define JKJ_IF_CONSTEVAL if (stdr::is_constant_evaluated())
#define JKJ_IF_NOT_CONSTEVAL if (!stdr::is_constant_evaluated())
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
#define JKJ_USE_IS_CONSTANT_EVALUATED 1
#elif JKJ_HAS_IF_CONSTEXPR
#define JKJ_IF_CONSTEVAL if constexpr (false)
#define JKJ_IF_NOT_CONSTEVAL if constexpr (true)
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
#define JKJ_USE_IS_CONSTANT_EVALUATED 0
#else
#define JKJ_IF_CONSTEVAL if (false)
#define JKJ_IF_NOT_CONSTEVAL if (true)
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
#define JKJ_USE_IS_CONSTANT_EVALUATED 0
#endif
#else
#if defined(__cpp_lib_is_constant_evaluated) && __cpp_lib_is_constant_evaluated >= 201'811L
#define JKJ_IF_CONSTEVAL if (stdr::is_constant_evaluated())
#define JKJ_IF_NOT_CONSTEVAL if (!stdr::is_constant_evaluated())
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
#define JKJ_USE_IS_CONSTANT_EVALUATED 1
#elif JKJ_HAS_IF_CONSTEXPR
#define JKJ_IF_CONSTEVAL if constexpr (false)
#define JKJ_IF_NOT_CONSTEVAL if constexpr (true)
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
#define JKJ_USE_IS_CONSTANT_EVALUATED 0
#else
#define JKJ_IF_CONSTEVAL if (false)
#define JKJ_IF_NOT_CONSTEVAL if (true)
#define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
#define JKJ_USE_IS_CONSTANT_EVALUATED 0
#endif
#endif

#if JKJ_CAN_BRANCH_ON_CONSTEVAL && JKJ_HAS_BIT_CAST
#define JKJ_CONSTEXPR20 constexpr
#else
#define JKJ_CONSTEXPR20
#endif

// Suppress additional buffer overrun check.
// I have no idea why MSVC thinks some functions here are vulnerable to the buffer overrun
// attacks. No, they aren't.
#if defined(__GNUC__) || defined(__clang__)
#define JKJ_SAFEBUFFERS
#define JKJ_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define JKJ_SAFEBUFFERS __declspec(safebuffers)
#define JKJ_FORCEINLINE __forceinline
#else
#define JKJ_SAFEBUFFERS
#define JKJ_FORCEINLINE inline
#endif

#if defined(__has_builtin)
#define JKJ_HAS_BUILTIN(x) __has_builtin(x)
#else
#define JKJ_HAS_BUILTIN(x) false
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__INTEL_COMPILER)
#include <immintrin.h>
#endif
#endif

#ifndef JKJ_HEADER_DRAGONBOX
#define JKJ_HEADER_DRAGONBOX

namespace JKJ_NAMESPACE {
namespace dragonbox {
////////////////////////////////////////////////////////////////////////////////////////
// The Compatibility layer for toolchains without standard C++ headers.
////////////////////////////////////////////////////////////////////////////////////////
namespace detail {
namespace stdr {
// <bit>
#if JKJ_HAS_BIT_CAST
using JKJ_STD_REPLACEMENT_NAMESPACE::bit_cast;
#endif

// <cassert>
// We need assert() macro, but it is not namespaced anyway, so nothing to do here.

// <cstdint>
using JKJ_STD_REPLACEMENT_NAMESPACE::int_least8_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::int_least16_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::int_least32_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast8_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast16_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast32_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least8_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least16_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least32_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least64_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast8_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast16_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast32_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast64_t;
// We need INT32_C, UINT32_C and UINT64_C macros too, but again there is nothing to do
// here.

// <cstring>
using JKJ_STD_REPLACEMENT_NAMESPACE::size_t;
using JKJ_STD_REPLACEMENT_NAMESPACE::__constexpr_memcpy;

// <limits>

// <type_traits>
template <bool cond, class T = void>
using enable_if = JKJ_STD_REPLACEMENT_NAMESPACE::enable_if<cond, T>;
template <class T>
using add_rvalue_reference = JKJ_STD_REPLACEMENT_NAMESPACE::add_rvalue_reference<T>;
template <bool cond, class T_true, class T_false>
using conditional = JKJ_STD_REPLACEMENT_NAMESPACE::conditional<cond, T_true, T_false>;
#if JKJ_USE_IS_CONSTANT_EVALUATED
using JKJ_STD_REPLACEMENT_NAMESPACE::is_constant_evaluated;
#endif
template <class T1, class T2>
using is_same = JKJ_STD_REPLACEMENT_NAMESPACE::is_same<T1, T2>;
#if !JKJ_HAS_BIT_CAST
template <class T>
using is_trivially_copyable = JKJ_STD_REPLACEMENT_NAMESPACE::is_trivially_copyable<T>;
#endif
template <class T>
using is_integral = JKJ_STD_REPLACEMENT_NAMESPACE::is_integral<T>;
template <class T>
using is_signed = JKJ_STD_REPLACEMENT_NAMESPACE::is_signed<T>;
template <class T>
using is_unsigned = JKJ_STD_REPLACEMENT_NAMESPACE::is_unsigned<T>;

template <class T>
using make_unsigned = JKJ_STD_REPLACEMENT_NAMESPACE::make_unsigned<T>;
} // namespace stdr
} // namespace detail


////////////////////////////////////////////////////////////////////////////////////////
// Some general utilities for C++11-compatibility.
////////////////////////////////////////////////////////////////////////////////////////
namespace detail {
#if !JKJ_HAS_CONSTEXPR17
template <stdr::size_t... indices>
struct index_sequence { };

template <stdr::size_t current, stdr::size_t total, class Dummy, stdr::size_t... indices>
struct make_index_sequence_impl {
	using type =
			typename make_index_sequence_impl<current + 1, total, Dummy, indices..., current>::type;
};

template <stdr::size_t total, class Dummy, stdr::size_t... indices>
struct make_index_sequence_impl<total, total, Dummy, indices...> {
	using type = index_sequence<indices...>;
};

template <stdr::size_t N>
using make_index_sequence = typename make_index_sequence_impl<0, N, void>::type;
#endif

// Available since C++11, but including <utility> just for this is an overkill.
template <class T>
typename stdr::add_rvalue_reference<T>::type declval() noexcept;
} // namespace detail


////////////////////////////////////////////////////////////////////////////////////////
// Utilities for static data.
////////////////////////////////////////////////////////////////////////////////////////
namespace detail {
// Including <array> is an overkill.
template <class T, stdr::size_t N>
struct array {
	T data_[N];
	constexpr T const *data() const noexcept { return data_; }
	constexpr T operator[](stdr::size_t idx) const noexcept { return data_[idx]; }
	JKJ_CONSTEXPR14 T &operator[](stdr::size_t idx) noexcept { return data_[idx]; }
};

// Platforms may require special instructions for reading static data declared with
// JKJ_STATIC_DATA_SECTION. Users can specify such instructions by defining the macro
// JKJ_READ_STATIC_DATA.
template <class T>
JKJ_CONSTEXPR20 T read_static_data(T const *ptr) noexcept {
	JKJ_IF_CONSTEVAL { return *ptr; }
	else {
#ifdef JKJ_READ_STATIC_DATA
		return JKJ_READ_STATIC_DATA(ptr);
#else
		return *ptr;
#endif
	}
}
} // namespace detail


////////////////////////////////////////////////////////////////////////////////////////
// Some basic features for encoding/decoding IEEE-754 formats.
////////////////////////////////////////////////////////////////////////////////////////
namespace detail {
template <class T>
struct physical_bits {
	static constexpr stdr::size_t value = sizeof(T) * sprt::Digits<unsigned char>;
};
template <class T>
struct value_bits {
	static constexpr stdr::size_t value =
			sprt::Digits< typename stdr::enable_if<stdr::is_integral<T>::value, T>::type>;
};

template <typename To, typename From>
JKJ_CONSTEXPR20 To bit_cast(const From &from) {
#if JKJ_HAS_BIT_CAST
	return stdr::bit_cast<To>(from);
#else
	static_assert(sizeof(From) == sizeof(To), "");
	static_assert(stdr::is_trivially_copyable<To>::value, "");
	static_assert(stdr::is_trivially_copyable<From>::value, "");
	To to;
	stdr::__constexpr_memcpy(&to, &from, sizeof(To));
	return to;
#endif
}
} // namespace detail

// These classes expose encoding specs of IEEE-754-like floating-point formats.
// Currently available formats are IEEE-754 binary32 & IEEE-754 binary64.

struct ieee754_binary32 {
	static constexpr int total_bits = 32;
	static constexpr int significand_bits = 23;
	static constexpr int exponent_bits = 8;
	static constexpr int min_exponent = -126;
	static constexpr int max_exponent = 127;
	static constexpr int exponent_bias = -127;
	static constexpr int decimal_significand_digits = 9;
	static constexpr int decimal_exponent_digits = 2;
	static constexpr int max_abs_decimal_exponent = 45;
};
struct ieee754_binary64 {
	static constexpr int total_bits = 64;
	static constexpr int significand_bits = 52;
	static constexpr int exponent_bits = 11;
	static constexpr int min_exponent = -1'022;
	static constexpr int max_exponent = 1'023;
	static constexpr int exponent_bias = -1'023;
	static constexpr int decimal_significand_digits = 17;
	static constexpr int decimal_exponent_digits = 3;
	static constexpr int max_abs_decimal_exponent = 324;
};

// A floating-point format traits class defines ways to interpret a bit pattern of given size as
// an encoding of floating-point number. This is an implementation of such a traits class,
// supporting ways to interpret IEEE-754 binary floating-point numbers.
template <class Format, class CarrierUInt, class ExponentInt = int>
struct ieee754_binary_traits {
	// CarrierUInt needs to have enough size to hold the entire contents of floating-point
	// numbers. The actual bits are assumed to be aligned to the LSB, and every other bits are
	// assumed to be zeroed.
	static_assert(detail::value_bits<CarrierUInt>::value >= Format::total_bits,
			"jkj::dragonbox: insufficient number of bits");
	static_assert(detail::stdr::is_unsigned<CarrierUInt>::value, "");

	// ExponentUInt needs to be large enough to hold (unsigned) exponent bits as well as the
	// (signed) actual exponent.
	// TODO: static overflow guard against intermediate computations.
	static_assert(detail::value_bits<ExponentInt>::value >= Format::exponent_bits + 1,
			"jkj::dragonbox: insufficient number of bits");
	static_assert(detail::stdr::is_signed<ExponentInt>::value, "");

	using format = Format;
	using carrier_uint = CarrierUInt;
	static constexpr int carrier_bits = int(detail::value_bits<carrier_uint>::value);
	using exponent_int = ExponentInt;

	// Extract exponent bits from a bit pattern.
	// The result must be aligned to the LSB so that there is no additional zero paddings
	// on the right. This function does not do bias adjustment.
	static constexpr exponent_int extract_exponent_bits(carrier_uint u) noexcept {
		return exponent_int(
				(u >> format::significand_bits) & ((exponent_int(1) << format::exponent_bits) - 1));
	}

	// Extract significand bits from a bit pattern.
	// The result must be aligned to the LSB so that there is no additional zero paddings
	// on the right. The result does not contain the implicit bit.
	static constexpr carrier_uint extract_significand_bits(carrier_uint u) noexcept {
		return carrier_uint(u & ((carrier_uint(1) << format::significand_bits) - 1u));
	}

	// Remove the exponent bits and extract significand bits together with the sign bit.
	static constexpr carrier_uint remove_exponent_bits(carrier_uint u) noexcept {
		return carrier_uint(u
				& ~(((carrier_uint(1) << format::exponent_bits) - 1u) << format::significand_bits));
	}

	// Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
	static constexpr carrier_uint remove_sign_bit_and_shift(carrier_uint u) noexcept {
		return carrier_uint((carrier_uint(u) << 1)
				& ((((carrier_uint(1) << (Format::total_bits - 1)) - 1u) << 1) | 1u));
	}

	// Obtain the actual value of the binary exponent from the extracted exponent bits.
	static constexpr exponent_int binary_exponent(exponent_int exponent_bits) noexcept {
		return exponent_int(
				exponent_bits == 0 ? format::min_exponent : exponent_bits + format::exponent_bias);
	}

	// Obtain the actual value of the binary significand from the extracted significand bits
	// and exponent bits.
	static constexpr carrier_uint binary_significand(carrier_uint significand_bits,
			exponent_int exponent_bits) noexcept {
		return carrier_uint(exponent_bits == 0
						? significand_bits
						: (significand_bits | (carrier_uint(1) << format::significand_bits)));
	}

	/* Various boolean observer functions */

	static constexpr bool is_nonzero(carrier_uint u) noexcept {
		return (u & ((carrier_uint(1) << (format::significand_bits + format::exponent_bits)) - 1u))
				!= 0;
	}
	static constexpr bool is_positive(carrier_uint u) noexcept {
		return u < (carrier_uint(1) << (format::significand_bits + format::exponent_bits));
	}
	static constexpr bool is_negative(carrier_uint u) noexcept { return !is_positive(u); }
	static constexpr bool is_finite(exponent_int exponent_bits) noexcept {
		return exponent_bits != ((exponent_int(1) << format::exponent_bits) - 1);
	}
	static constexpr bool has_all_zero_significand_bits(carrier_uint u) noexcept {
		return ((u << 1) & ((((carrier_uint(1) << (Format::total_bits - 1)) - 1u) << 1) | 1u)) == 0;
	}
	static constexpr bool has_even_significand_bits(carrier_uint u) noexcept { return u % 2 == 0; }
};

// Convert between bit patterns stored in carrier_uint and instances of an actual
// floating-point type. Depending on format and carrier_uint, this operation might not
// be possible for some specific bit patterns. However, the contract is that u always
// denotes a valid bit pattern, so the functions here are assumed to be noexcept.
// Users might specialize this class to change the behavior for certain types.
// The default provided by the library is to treat the given floating-point type Float as either
// IEEE-754 binary32 or IEEE-754 binary64, depending on the bitwise size of Float.
template <class Float>
struct default_float_bit_carrier_conversion_traits {
	// Guards against types that have different internal representations than IEEE-754
	// binary32/64. I don't know if there is a truly reliable way of detecting IEEE-754 binary
	// formats. I just did my best here. Note that in some cases
	// numeric_limits<Float>::is_iec559 may report false even if the internal representation is
	// IEEE-754 compatible. In such a case, the user can specialize this traits template and
	// remove this static sanity check in order to make Dragonbox work for Float.
	/*static_assert(detail::stdr::numeric_limits<Float>::is_iec559
					&& detail::stdr::numeric_limits<Float>::radix == 2
					&& (detail::physical_bits<Float>::value == 32
							|| detail::physical_bits<Float>::value == 64),
			"jkj::dragonbox: Float may not be of IEEE-754 binary32/binary64");*/

	// Specifies the unsigned integer type to hold bitwise value of Float.
	using carrier_uint =
			typename detail::stdr::conditional<detail::physical_bits<Float>::value == 32,
					detail::stdr::uint_least32_t, detail::stdr::uint_least64_t>::type;

	// Specifies the floating-point format.
	using format = typename detail::stdr::conditional<detail::physical_bits<Float>::value == 32,
			ieee754_binary32, ieee754_binary64>::type;

	// Converts the floating-point type into the bit-carrier unsigned integer type.
	static JKJ_CONSTEXPR20 carrier_uint float_to_carrier(Float x) noexcept {
		return detail::bit_cast<carrier_uint>(x);
	}

	// Converts the bit-carrier unsigned integer type into the floating-point type.
	static JKJ_CONSTEXPR20 Float carrier_to_float(carrier_uint x) noexcept {
		return detail::bit_cast<Float>(x);
	}
};

// Convenient wrappers for floating-point traits classes.
// In order to reduce the argument passing overhead, these classes should be as simple as
// possible (e.g., no inheritance, no private non-static data member, etc.; this is an
// unfortunate fact about common ABI convention).

template <class FormatTraits>
struct signed_significand_bits {
	using format_traits = FormatTraits;
	using carrier_uint = typename format_traits::carrier_uint;

	carrier_uint u;

	signed_significand_bits() = default;
	constexpr explicit signed_significand_bits(carrier_uint bit_pattern) noexcept
	: u{bit_pattern} { }

	// Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
	constexpr carrier_uint remove_sign_bit_and_shift() const noexcept {
		return format_traits::remove_sign_bit_and_shift(u);
	}

	constexpr bool is_positive() const noexcept { return format_traits::is_positive(u); }
	constexpr bool is_negative() const noexcept { return format_traits::is_negative(u); }
	constexpr bool has_all_zero_significand_bits() const noexcept {
		return format_traits::has_all_zero_significand_bits(u);
	}
	constexpr bool has_even_significand_bits() const noexcept {
		return format_traits::has_even_significand_bits(u);
	}
};

template <class FormatTraits>
struct float_bits {
	using format_traits = FormatTraits;
	using carrier_uint = typename format_traits::carrier_uint;
	using exponent_int = typename format_traits::exponent_int;

	carrier_uint u;

	float_bits() = default;
	constexpr explicit float_bits(carrier_uint bit_pattern) noexcept : u{bit_pattern} { }

	// Extract exponent bits from a bit pattern.
	// The result must be aligned to the LSB so that there is no additional zero paddings
	// on the right. This function does not do bias adjustment.
	constexpr exponent_int extract_exponent_bits() const noexcept {
		return format_traits::extract_exponent_bits(u);
	}

	// Extract significand bits from a bit pattern.
	// The result must be aligned to the LSB so that there is no additional zero paddings
	// on the right. The result does not contain the implicit bit.
	constexpr carrier_uint extract_significand_bits() const noexcept {
		return format_traits::extract_significand_bits(u);
	}

	// Remove the exponent bits and extract significand bits together with the sign bit.
	constexpr signed_significand_bits<format_traits> remove_exponent_bits() const noexcept {
		return signed_significand_bits<format_traits>(format_traits::remove_exponent_bits(u));
	}

	// Obtain the actual value of the binary exponent from the extracted exponent bits.
	static constexpr exponent_int binary_exponent(exponent_int exponent_bits) noexcept {
		return format_traits::binary_exponent(exponent_bits);
	}
	constexpr exponent_int binary_exponent() const noexcept {
		return binary_exponent(extract_exponent_bits());
	}

	// Obtain the actual value of the binary exponent from the extracted significand bits
	// and exponent bits.
	static constexpr carrier_uint binary_significand(carrier_uint significand_bits,
			exponent_int exponent_bits) noexcept {
		return format_traits::binary_significand(significand_bits, exponent_bits);
	}
	constexpr carrier_uint binary_significand() const noexcept {
		return binary_significand(extract_significand_bits(), extract_exponent_bits());
	}

	constexpr bool is_nonzero() const noexcept { return format_traits::is_nonzero(u); }
	constexpr bool is_positive() const noexcept { return format_traits::is_positive(u); }
	constexpr bool is_negative() const noexcept { return format_traits::is_negative(u); }
	constexpr bool is_finite(exponent_int exponent_bits) const noexcept {
		return format_traits::is_finite(exponent_bits);
	}
	constexpr bool is_finite() const noexcept {
		return format_traits::is_finite(extract_exponent_bits());
	}
	constexpr bool has_even_significand_bits() const noexcept {
		return format_traits::has_even_significand_bits(u);
	}
};

template <class Float, class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
		class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
				typename ConversionTraits::carrier_uint>>
JKJ_CONSTEXPR20 float_bits<FormatTraits> make_float_bits(Float x) noexcept {
	return float_bits<FormatTraits>(ConversionTraits::float_to_carrier(x));
}

namespace detail {
////////////////////////////////////////////////////////////////////////////////////////
// Bit operation intrinsics.
////////////////////////////////////////////////////////////////////////////////////////

namespace bits {
// Most compilers should be able to optimize this into the ROR instruction.
// n is assumed to be at most of bit_width bits.
template <stdr::size_t bit_width, class UInt>
JKJ_CONSTEXPR14 UInt rotr(UInt n, unsigned int r) noexcept {
	static_assert(bit_width > 0, "jkj::dragonbox: rotation bit-width must be positive");
	static_assert(bit_width <= value_bits<UInt>::value,
			"jkj::dragonbox: rotation bit-width is too large");
	r &= (bit_width - 1);
	return (n >> r) | (n << ((bit_width - r) & (bit_width - 1)));
}
} // namespace bits

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for wide unsigned integer arithmetic.
////////////////////////////////////////////////////////////////////////////////////////

namespace wuint {
// Compilers might support built-in 128-bit integer types. However, it seems that
// emulating them with a pair of 64-bit integers actually produces a better code,
// so we avoid using those built-ins. That said, they are still useful for
// implementing 64-bit x 64-bit -> 128-bit multiplication.

// clang-format off
#if defined(__SIZEOF_INT128__)
		        // To silence "error: ISO C++ does not support '__int128' for 'type name'
		        // [-Wpedantic]"
#if defined(__GNUC__)
			    __extension__
#endif
			    using builtin_uint128_t = unsigned __int128;
#endif
// clang-format on

struct uint128 {
	uint128() = default;

	stdr::uint_least64_t high_;
	stdr::uint_least64_t low_;

	constexpr uint128(stdr::uint_least64_t high, stdr::uint_least64_t low) noexcept
	: high_{high}, low_{low} { }

	constexpr stdr::uint_least64_t high() const noexcept { return high_; }
	constexpr stdr::uint_least64_t low() const noexcept { return low_; }

	JKJ_CONSTEXPR20 uint128 &operator+=(stdr::uint_least64_t n) & noexcept {
		auto const generic_impl = [&] {
			auto const sum = (low_ + n) & __SPRT_UINT64_C(0xffff'ffff'ffff'ffff);
			high_ += (sum < low_ ? 1 : 0);
			low_ = sum;
		};
		// To suppress warning.
		static_cast<void>(generic_impl);

		JKJ_IF_CONSTEXPR(value_bits<stdr::uint_least64_t>::value > 64) {
			generic_impl();
			return *this;
		}

		JKJ_IF_CONSTEVAL {
			generic_impl();
			return *this;
		}

		// See https://github.com/fmtlib/fmt/pull/2985.
#if JKJ_HAS_BUILTIN(__builtin_addcll) && !defined(__ibmxl__)
		JKJ_IF_CONSTEXPR(stdr::is_same<stdr::uint_least64_t, unsigned long long>::value) {
			unsigned long long carry{};
			low_ = stdr::uint_least64_t(__builtin_addcll(low_, n, 0, &carry));
			high_ = stdr::uint_least64_t(__builtin_addcll(high_, 0, carry, &carry));
			return *this;
		}
#endif
#if JKJ_HAS_BUILTIN(__builtin_addcl) && !defined(__ibmxl__)
		JKJ_IF_CONSTEXPR(stdr::is_same<stdr::uint_least64_t, unsigned long>::value) {
			unsigned long carry{};
			low_ = stdr::uint_least64_t(__builtin_addcl(static_cast<unsigned long>(low_),
					static_cast<unsigned long>(n), 0, &carry));
			high_ = stdr::uint_least64_t(
					__builtin_addcl(static_cast<unsigned long>(high_), 0, carry, &carry));
			return *this;
		}
#endif
#if JKJ_HAS_BUILTIN(__builtin_addc) && !defined(__ibmxl__)
		JKJ_IF_CONSTEXPR(stdr::is_same<stdr::uint_least64_t, unsigned int>::value) {
			unsigned int carry{};
			low_ = stdr::uint_least64_t(__builtin_addc(static_cast<unsigned int>(low_),
					static_cast<unsigned int>(n), 0, &carry));
			high_ = stdr::uint_least64_t(
					__builtin_addc(static_cast<unsigned int>(high_), 0, carry, &carry));
			return *this;
		}
#endif

#if JKJ_HAS_BUILTIN(__builtin_ia32_addcarry_u64)
		// __builtin_ia32_addcarry_u64 is not documented, but it seems it takes unsigned
		// long long arguments.
		unsigned long long result{};
		auto const carry = __builtin_ia32_addcarry_u64(0, low_, n, &result);
		low_ = stdr::uint_least64_t(result);
		__builtin_ia32_addcarry_u64(carry, high_, 0, &result);
		high_ = stdr::uint_least64_t(result);
#elif defined(_MSC_VER) && defined(_M_X64)
		// On MSVC, uint_least64_t and __int64 must be unsigned long long; see
		// https://learn.microsoft.com/en-us/cpp/c-runtime-library/standard-types
		// and https://learn.microsoft.com/en-us/cpp/cpp/int8-int16-int32-int64.
		static_assert(stdr::is_same<unsigned long long, stdr::uint_least64_t>::value, "");
		auto const carry = _addcarry_u64(0, low_, n, &low_);
		_addcarry_u64(carry, high_, 0, &high_);
#elif defined(__INTEL_COMPILER) && (defined(_M_X64) || defined(__x86_64))
		// Cannot find any documentation on how things are defined, but hopefully this
		// is always true...
		static_assert(stdr::is_same<unsigned __int64, stdr::uint_least64_t>::value, "");
		auto const carry = _addcarry_u64(0, low_, n, &low_);
		_addcarry_u64(carry, high_, 0, &high_);
#else
		generic_impl();
#endif
		return *this;
	}
};

struct uint32_64 {
	stdr::uint_least64_t low64_;
	stdr::uint_least32_t high32_;

	constexpr uint32_64(stdr::uint_least32_t high32, stdr::uint_least64_t low64) noexcept
	: low64_{low64}, high32_{high32} { }

	constexpr stdr::uint_least32_t high32() const noexcept { return high32_; }
	constexpr stdr::uint_least64_t low64() const noexcept { return low64_; }
};

struct uint64_32 {
	stdr::uint_least64_t high64_;
	stdr::uint_least32_t low32_;

	constexpr uint64_32(stdr::uint_least64_t high64, stdr::uint_least32_t low32) noexcept
	: high64_{high64}, low32_{low32} { }

	constexpr stdr::uint_least64_t high64() const noexcept { return high64_; }
	constexpr stdr::uint_least32_t low32() const noexcept { return low32_; }
};

inline JKJ_CONSTEXPR20 stdr::uint_least64_t umul64(stdr::uint_least32_t x,
		stdr::uint_least32_t y) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
	JKJ_IF_NOT_CONSTEVAL { return __emulu(x, y); }
#endif
	return x * stdr::uint_least64_t(y);
}

// Get 128-bit result of multiplication of two 64-bit unsigned integers.
JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128 umul128(stdr::uint_least64_t x,
		stdr::uint_least64_t y) noexcept {
	auto const generic_impl = [=]() -> uint128 {
		auto const a = stdr::uint_least32_t(x >> 32);
		auto const b = stdr::uint_least32_t(x);
		auto const c = stdr::uint_least32_t(y >> 32);
		auto const d = stdr::uint_least32_t(y);

		auto const ac = umul64(a, c);
		auto const bc = umul64(b, c);
		auto const ad = umul64(a, d);
		auto const bd = umul64(b, d);

		auto const intermediate = (bd >> 32) + stdr::uint_least32_t(ad) + stdr::uint_least32_t(bc);

		return {ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32),
			(intermediate << 32) + stdr::uint_least32_t(bd)};
	};
	// To silence warning.
	static_cast<void>(generic_impl);

#if defined(__SIZEOF_INT128__)
	auto const result = builtin_uint128_t(x) * builtin_uint128_t(y);
	return {stdr::uint_least64_t(result >> 64), stdr::uint_least64_t(result)};
#elif defined(_MSC_VER) && defined(_M_X64)
	JKJ_IF_CONSTEVAL {
		// This redundant variable is to workaround MSVC's codegen bug caused by the
		// interaction of NRVO and intrinsics.
		auto const result = generic_impl();
		return result;
	}
	uint128 result;
#if defined(__AVX2__)
	result.low_ = _mulx_u64(x, y, &result.high_);
#else
	result.low_ = _umul128(x, y, &result.high_);
#endif
	return result;
#else
	return generic_impl();
#endif
}

// Get high half of the 128-bit result of multiplication of two 64-bit unsigned
// integers.
JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 stdr::uint_least64_t umul128_upper64(stdr::uint_least64_t x,
		stdr::uint_least64_t y) noexcept {
	auto const generic_impl = [=]() -> stdr::uint_least64_t {
		auto const a = stdr::uint_least32_t(x >> 32);
		auto const b = stdr::uint_least32_t(x);
		auto const c = stdr::uint_least32_t(y >> 32);
		auto const d = stdr::uint_least32_t(y);

		auto const ac = umul64(a, c);
		auto const bc = umul64(b, c);
		auto const ad = umul64(a, d);
		auto const bd = umul64(b, d);

		auto const intermediate = (bd >> 32) + stdr::uint_least32_t(ad) + stdr::uint_least32_t(bc);

		return ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
	};
	// To silence warning.
	static_cast<void>(generic_impl);

#if defined(__SIZEOF_INT128__)
	auto const result = builtin_uint128_t(x) * builtin_uint128_t(y);
	return stdr::uint_least64_t(result >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
	JKJ_IF_CONSTEVAL {
		// This redundant variable is to workaround MSVC's codegen bug caused by the
		// interaction of NRVO and intrinsics.
		auto const result = generic_impl();
		return result;
	}
	stdr::uint_least64_t result;
#if defined(__AVX2__)
	_mulx_u64(x, y, &result);
#else
	result = __umulh(x, y);
#endif
	return result;
#else
	return generic_impl();
#endif
}

// Get upper 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128 umul192_upper128(stdr::uint_least64_t x,
		uint128 y) noexcept {
	auto r = umul128(x, y.high());
	r += umul128_upper64(x, y.low());
	return r;
}

// Get 96-bit result of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20
#if JKJ_FAST_MUL64
		uint32_64
#else
		uint64_32
#endif
		umul96(stdr::uint_fast32_t x, stdr::uint_least64_t y) noexcept {
#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && defined(_M_X64))
	auto const r128 = umul128(stdr::uint_least64_t(x), y);

	auto const high = stdr::uint_least32_t(r128.high());
	auto const middle = stdr::uint_least32_t(r128.low() >> 32);
	auto const low = stdr::uint_least32_t(r128.low());
#else
	auto const yh = stdr::uint_least32_t(y >> 32);
	auto const yl = stdr::uint_least32_t(y);

	auto xyh = umul64(x, yh);
	auto const xyl = umul64(x, yl);

	xyh += (xyl >> 32);

	auto const high = stdr::uint_least32_t(xyh >> 32);
	auto const middle = stdr::uint_least32_t(xyh);
	auto const low = stdr::uint_least32_t(xyl);
#endif

#if JKJ_FAST_MUL64
	uint32_64 r{high, (stdr::uint_least64_t(middle) << 32) | low};
#else
	uint64_32 r{(stdr::uint_least64_t(high) << 32) | middle, low};
#endif
	return r;
}

// Get upper 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
inline JKJ_CONSTEXPR20 stdr::uint_least64_t umul96_upper64(stdr::uint_least32_t x,
		stdr::uint_least64_t y) noexcept {
#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && defined(_M_X64))
	return umul128_upper64(stdr::uint_least64_t(x) << 32, y);
#else
	auto const yh = stdr::uint_least32_t(y >> 32);
	auto const yl = stdr::uint_least32_t(y);

	auto const xyh = umul64(x, yh);
	auto const xyl = umul64(x, yl);

	return xyh + (xyl >> 32);
#endif
}

// Get lower 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
// unsigned integer.
JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128 umul192_lower128(stdr::uint_least64_t x,
		uint128 y) noexcept {
	auto const high = x * y.high();
	auto const high_low = umul128(x, y.low());
	return {(high + high_low.high()) & __SPRT_UINT64_C(0xffff'ffff'ffff'ffff), high_low.low()};
}

// Get lower 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
// unsigned integer.
constexpr stdr::uint_least64_t umul96_lower64(stdr::uint_least32_t x,
		stdr::uint_least64_t y) noexcept {
	return (x * y) & __SPRT_UINT64_C(0xffff'ffff'ffff'ffff);
}
} // namespace wuint

////////////////////////////////////////////////////////////////////////////////////////
// Some simple utilities for constexpr computation.
////////////////////////////////////////////////////////////////////////////////////////

template <int k, class Int>
constexpr Int compute_power(Int a) noexcept {
	static_assert(k >= 0, "");
#if JKJ_HAS_CONSTEXPR14
	Int p = 1;
	for (int i = 0; i < k; ++i) { p *= a; }
	return p;
#else
	return k == 0		 ? 1
			: k % 2 == 0 ? compute_power<k / 2, Int>(a * a)
						 : a * compute_power<k / 2, Int>(a * a);
#endif
}

template <int a, class UInt>
constexpr int count_factors(UInt n) noexcept {
	static_assert(a > 1, "");
#if JKJ_HAS_CONSTEXPR14
	int c = 0;
	while (n % a == 0) {
		n /= a;
		++c;
	}
	return c;
#else
	return n % a == 0 ? count_factors<a, UInt>(n / a) + 1 : 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for fast/constexpr log computation.
////////////////////////////////////////////////////////////////////////////////////////

namespace log {
static_assert((stdr::int_fast32_t(-1) >> 1) == stdr::int_fast32_t(-1)
				&& (stdr::int_fast16_t(-1) >> 1) == stdr::int_fast16_t(-1),
		"jkj::dragonbox: right-shift for signed integers must be arithmetic");

// Only for constexpr computation.
// Returns -1 when n = 0.
template <class UInt>
constexpr int floor_log2(UInt n) noexcept {
#if JKJ_HAS_CONSTEXPR14
	int count = -1;
	while (n != 0) {
		++count;
		n >>= 1;
	}
	return count;
#else
	return n == 0 ? -1 : floor_log2<UInt>(n / 2) + 1;
#endif
}

// Implementations of various log computations using binary approximations.
// On less powerful platforms, using smaller integer types and magic numbers for these
// computations significantly improve codegen, so depending on the range of input we try
// to choose the smallest possible magic numbers that result in the best codegen.
// To generically achieve that goal, we list multiple sets of magic numbers with
// associated ranges of inputs, called "tiers", so that we choose the smallest tier
// whose associated range covers the requested range of input.

// Check if the given tier covers the requested range.
// Info<current_tier> is supposed to hold the magic numbers needed for the computation
// and the supported range of input (i.e. the range of input where the computation with
// the given magic numbers must be valid).
template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
		stdr::int_least32_t max_exponent, stdr::size_t current_tier,
		stdr::int_least32_t supported_min_exponent = Info<current_tier>::min_exponent,
		stdr::int_least32_t supported_max_exponent = Info<current_tier>::max_exponent>
constexpr bool is_in_range(int) noexcept {
	return min_exponent >= supported_min_exponent && max_exponent <= supported_max_exponent;
}
// This overload generates a compile-error and is called only if the former overload is
// removed via SFINAE, which happens if and only if every provided tier fails to cover
// the requested range.
template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
		stdr::int_least32_t max_exponent, stdr::size_t current_tier>
constexpr bool is_in_range(...) noexcept {
	// Supposed to be always false, but formally dependent on the template parameters.
	static_assert(min_exponent > max_exponent, "jkj::dragonbox: exponent range is too wide");
	return false;
}

// Generic implementation of log computations.
template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
		stdr::int_least32_t max_exponent, stdr::size_t current_tier = 0,
		bool = is_in_range<Info, min_exponent, max_exponent, current_tier>(0)>
struct compute_impl;

// Specialization for the case when the given tier covers the requested range of input.
// In this case, do the computation with the given magic numbers.
template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
		stdr::int_least32_t max_exponent, stdr::size_t current_tier>
struct compute_impl<Info, min_exponent, max_exponent, current_tier, true> {
	using info = Info<current_tier>;
	using default_return_type = typename info::default_return_type;
	template <class ReturnType, class Int>
	static constexpr ReturnType compute(Int e) noexcept {
		// The sign is irrelevant for the mathematical validity of the formula, but
		// assuming positivity makes the overflow analysis simpler.
		static_assert(info::multiply >= 0 && info::subtract >= 0, "");
		return static_cast<ReturnType>((e * info::multiply - info::subtract) >> info::shift);
	}
};

// Specialization for the case when the given tier does not cover the requested range of
// input. In this case, delegate the computation to the next tier. If the next tier does
// not exist, then the compilation must fail.
template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
		stdr::int_least32_t max_exponent, stdr::size_t current_tier>
struct compute_impl<Info, min_exponent, max_exponent, current_tier, false> {
	using next_tier = compute_impl<Info, min_exponent, max_exponent, current_tier + 1>;
	using default_return_type = typename next_tier::default_return_type;
	template <class ReturnType, class Int>
	static constexpr ReturnType compute(Int e) noexcept {
		return next_tier::template compute<ReturnType>(e);
	}
};

template <stdr::size_t tier>
struct floor_log10_pow2_info;
template <>
struct floor_log10_pow2_info<0> {
	using default_return_type = stdr::int_fast8_t;
	static constexpr stdr::int_fast16_t multiply = 77;
	static constexpr stdr::int_fast16_t subtract = 0;
	static constexpr stdr::size_t shift = 8;
	static constexpr stdr::int_least32_t min_exponent = -102;
	static constexpr stdr::int_least32_t max_exponent = 102;
};
template <>
struct floor_log10_pow2_info<1> {
	using default_return_type = stdr::int_fast8_t;
	// 24-bits are enough in fact.
	static constexpr stdr::int_fast32_t multiply = 1'233;
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 12;
	// Formula itself holds on [-680,680]; [-425,425] is to ensure that the output is
	// within [-127,127].
	static constexpr stdr::int_least32_t min_exponent = -425;
	static constexpr stdr::int_least32_t max_exponent = 425;
};
template <>
struct floor_log10_pow2_info<2> {
	using default_return_type = stdr::int_fast16_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(315'653);
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 20;
	static constexpr stdr::int_least32_t min_exponent = -2'620;
	static constexpr stdr::int_least32_t max_exponent = 2'620;
};
template <stdr::int_least32_t min_exponent = -2'620, stdr::int_least32_t max_exponent = 2'620,
		class ReturnType = typename compute_impl<floor_log10_pow2_info, min_exponent,
				max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log10_pow2(Int e) noexcept {
	return compute_impl<floor_log10_pow2_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}

template <stdr::size_t tier>
struct floor_log2_pow10_info;
template <>
struct floor_log2_pow10_info<0> {
	using default_return_type = stdr::int_fast8_t;
	static constexpr stdr::int_fast16_t multiply = 53;
	static constexpr stdr::int_fast16_t subtract = 0;
	static constexpr stdr::size_t shift = 4;
	static constexpr stdr::int_least32_t min_exponent = -15;
	static constexpr stdr::int_least32_t max_exponent = 18;
};
template <>
struct floor_log2_pow10_info<1> {
	using default_return_type = stdr::int_fast16_t;
	// 24-bits are enough in fact.
	static constexpr stdr::int_fast32_t multiply = 1'701;
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 9;
	static constexpr stdr::int_least32_t min_exponent = -58;
	static constexpr stdr::int_least32_t max_exponent = 58;
};
template <>
struct floor_log2_pow10_info<2> {
	using default_return_type = stdr::int_fast16_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(1'741'647);
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 19;
	// Formula itself holds on [-4003,4003]; [-1233,1233] is to ensure no overflow.
	static constexpr stdr::int_least32_t min_exponent = -1'233;
	static constexpr stdr::int_least32_t max_exponent = 1'233;
};
template <stdr::int_least32_t min_exponent = -1'233, stdr::int_least32_t max_exponent = 1'233,
		class ReturnType = typename compute_impl<floor_log2_pow10_info, min_exponent,
				max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log2_pow10(Int e) noexcept {
	return compute_impl<floor_log2_pow10_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}

template <stdr::size_t tier>
struct floor_log10_pow2_minus_log10_4_over_3_info;
template <>
struct floor_log10_pow2_minus_log10_4_over_3_info<0> {
	using default_return_type = stdr::int_fast8_t;
	static constexpr stdr::int_fast16_t multiply = 77;
	static constexpr stdr::int_fast16_t subtract = 31;
	static constexpr stdr::size_t shift = 8;
	static constexpr stdr::int_least32_t min_exponent = -75;
	static constexpr stdr::int_least32_t max_exponent = 129;
};
template <>
struct floor_log10_pow2_minus_log10_4_over_3_info<1> {
	using default_return_type = stdr::int_fast8_t;
	// 24-bits are enough in fact.
	static constexpr stdr::int_fast32_t multiply = 19'728;
	static constexpr stdr::int_fast32_t subtract = 8'241;
	static constexpr stdr::size_t shift = 16;
	// Formula itself holds on [-849,315]; [-424,315] is to ensure that the output is
	// within [-127,127].
	static constexpr stdr::int_least32_t min_exponent = -424;
	static constexpr stdr::int_least32_t max_exponent = 315;
};
template <>
struct floor_log10_pow2_minus_log10_4_over_3_info<2> {
	using default_return_type = stdr::int_fast16_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(631'305);
	static constexpr stdr::int_fast32_t subtract = __SPRT_INT32_C(261'663);
	static constexpr stdr::size_t shift = 21;
	static constexpr stdr::int_least32_t min_exponent = -2'985;
	static constexpr stdr::int_least32_t max_exponent = 2'936;
};
template <stdr::int_least32_t min_exponent = -2'985, stdr::int_least32_t max_exponent = 2'936,
		class ReturnType = typename compute_impl<floor_log10_pow2_minus_log10_4_over_3_info,
				min_exponent, max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log10_pow2_minus_log10_4_over_3(Int e) noexcept {
	return compute_impl<floor_log10_pow2_minus_log10_4_over_3_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}

template <stdr::size_t tier>
struct floor_log2_pow5_info;
template <>
struct floor_log2_pow5_info<0> {
	using default_return_type = stdr::int_fast8_t;
	// 24-bits are enough in fact.
	static constexpr stdr::int_fast32_t multiply = 1'189;
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 9;
	// Formula itself holds on [-58,58]; [-55,55] is to ensure no overflow.
	static constexpr stdr::int_least32_t min_exponent = -55;
	static constexpr stdr::int_least32_t max_exponent = 55;
};
template <>
struct floor_log2_pow5_info<1> {
	using default_return_type = stdr::int_fast16_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(76'085);
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 15;
	static constexpr stdr::int_least32_t min_exponent = -642;
	static constexpr stdr::int_least32_t max_exponent = 642;
};
template <stdr::int_least32_t min_exponent = -642, stdr::int_least32_t max_exponent = 642,
		class ReturnType = typename compute_impl<floor_log2_pow5_info, min_exponent,
				max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log2_pow5(Int e) noexcept {
	return compute_impl<floor_log2_pow5_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}

template <stdr::size_t tier>
struct floor_log5_pow2_info;
template <>
struct floor_log5_pow2_info<0> {
	using default_return_type = stdr::int_fast32_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(225'799);
	static constexpr stdr::int_fast32_t subtract = 0;
	static constexpr stdr::size_t shift = 19;
	static constexpr stdr::int_least32_t min_exponent = -1'831;
	static constexpr stdr::int_least32_t max_exponent = 1'831;
};
template <stdr::int_least32_t min_exponent = -1'831, stdr::int_least32_t max_exponent = 1'831,
		class ReturnType = typename compute_impl<floor_log5_pow2_info, min_exponent,
				max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log5_pow2(Int e) noexcept {
	return compute_impl<floor_log5_pow2_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}

template <stdr::size_t tier>
struct floor_log5_pow2_minus_log5_3_info;
template <>
struct floor_log5_pow2_minus_log5_3_info<0> {
	using default_return_type = stdr::int_fast32_t;
	static constexpr stdr::int_fast32_t multiply = __SPRT_INT32_C(451'597);
	static constexpr stdr::int_fast32_t subtract = __SPRT_INT32_C(715'764);
	static constexpr stdr::size_t shift = 20;
	static constexpr stdr::int_least32_t min_exponent = -3'543;
	static constexpr stdr::int_least32_t max_exponent = 2'427;
};
template <stdr::int_least32_t min_exponent = -3'543, stdr::int_least32_t max_exponent = 2'427,
		class ReturnType = typename compute_impl<floor_log5_pow2_minus_log5_3_info, min_exponent,
				max_exponent>::default_return_type,
		class Int>
constexpr ReturnType floor_log5_pow2_minus_log5_3(Int e) noexcept {
	return compute_impl<floor_log5_pow2_minus_log5_3_info, min_exponent,
			max_exponent>::template compute<ReturnType>(e);
}
} // namespace log

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for fast divisibility tests.
////////////////////////////////////////////////////////////////////////////////////////

namespace div {
// Replace n by floor(n / 10^N).
// Returns true if and only if n is divisible by 10^N.
// Precondition: n <= 10^(N+1)
// !!It takes an in-out parameter!!
template <int N, class UInt>
struct divide_by_pow10_info;

template <class UInt>
struct divide_by_pow10_info<1, UInt> {
	static constexpr stdr::uint_fast32_t magic_number = 6'554;
	static constexpr int shift_amount = 16;
};

template <>
struct divide_by_pow10_info<1, stdr::uint_least8_t> {
	static constexpr stdr::uint_fast16_t magic_number = 103;
	static constexpr int shift_amount = 10;
};

template <>
struct divide_by_pow10_info<1, stdr::uint_least16_t> {
	static constexpr stdr::uint_fast16_t magic_number = 103;
	static constexpr int shift_amount = 10;
};

template <class UInt>
struct divide_by_pow10_info<2, UInt> {
	static constexpr stdr::uint_fast32_t magic_number = 656;
	static constexpr int shift_amount = 16;
};

template <>
struct divide_by_pow10_info<2, stdr::uint_least16_t> {
	static constexpr stdr::uint_fast32_t magic_number = 41;
	static constexpr int shift_amount = 12;
};

template <int N, class UInt>
JKJ_CONSTEXPR14 bool check_divisibility_and_divide_by_pow10(UInt &n) noexcept {
	// Make sure the computation for max_n does not overflow.
	static_assert(N + 1 <= log::floor_log10_pow2(int(value_bits<UInt>::value)), "");
	__sprt_assert(n <= compute_power<N + 1>(UInt(10)));

	using info = divide_by_pow10_info<N, UInt>;
	using intermediate_type = decltype(info::magic_number);
	auto const prod = intermediate_type(n * info::magic_number);

	constexpr auto mask = intermediate_type((intermediate_type(1) << info::shift_amount) - 1);
	bool const result = ((prod & mask) < info::magic_number);

	n = UInt(prod >> info::shift_amount);
	return result;
}

// Compute floor(n / 10^N) for small n and N.
// Precondition: n <= 10^(N+1)
template <int N, class UInt>
JKJ_CONSTEXPR14 UInt small_division_by_pow10(UInt n) noexcept {
	// Make sure the computation for max_n does not overflow.
	static_assert(N + 1 <= log::floor_log10_pow2(int(value_bits<UInt>::value)), "");
	assert(n <= compute_power<N + 1>(UInt(10)));

	return UInt((n * divide_by_pow10_info<N, UInt>::magic_number)
			>> divide_by_pow10_info<N, UInt>::shift_amount);
}

// Compute floor(n / 10^N) for small N.
// Precondition: n <= n_max
template <int N, class UInt, UInt n_max>
JKJ_CONSTEXPR20 UInt divide_by_pow10(UInt n) noexcept {
	static_assert(N >= 0, "");

	// Specialize for 32-bit division by 10.
	// Without the bound on n_max (which compilers these days never leverage), the
	// minimum needed amount of shift is larger than 32. Hence, this may generate better
	// code for 32-bit or smaller architectures. Even for 64-bit architectures, it seems
	// compilers tend to generate mov + mul instead of a single imul for an unknown
	// reason if we just write n / 10.
	JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least32_t>::value && N == 1
			&& n_max <= __SPRT_UINT32_C(1'073'741'828)) {
		return UInt(wuint::umul64(n, __SPRT_UINT32_C(429'496'730)) >> 32);
	}
	// Specialize for 64-bit division by 10.
	// Without the bound on n_max (which compilers these days never leverage), the
	// minimum needed amount of shift is larger than 64.
	else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least64_t>::value && N == 1
			&& n_max <= __SPRT_UINT64_C(4'611'686'018'427'387'908)) {
		return UInt(wuint::umul128_upper64(n, __SPRT_UINT64_C(1'844'674'407'370'955'162)));
	}
	// Specialize for 32-bit division by 100.
	// It seems compilers tend to generate mov + mul instead of a single imul for an
	// unknown reason if we just write n / 100.
	else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least32_t>::value && N == 2) {
		return UInt(wuint::umul64(n, __SPRT_UINT32_C(1'374'389'535)) >> 37);
	}
	// Specialize for 64-bit division by 1000.
	// Without the bound on n_max (which compilers these days never leverage), the
	// smallest magic number for this computation does not fit into 64-bits.
	else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least64_t>::value && N == 3
			&& n_max <= __SPRT_UINT64_C(15'534'100'272'597'517'998)) {
		return UInt(wuint::umul128_upper64(n, __SPRT_UINT64_C(4'722'366'482'869'645'214)) >> 8);
	}
	else {
		constexpr auto divisor = compute_power<N>(UInt(10));
		return n / divisor;
	}
}
} // namespace div
} // namespace detail

////////////////////////////////////////////////////////////////////////////////////////
// Return types for the main interface function.
////////////////////////////////////////////////////////////////////////////////////////

template <class SignificandType, class ExponentType, bool is_signed, bool trailing_zero_flag>
struct decimal_fp;

template <class SignificandType, class ExponentType>
struct decimal_fp<SignificandType, ExponentType, false, false> {
	SignificandType significand;
	ExponentType exponent;
};

template <class SignificandType, class ExponentType>
struct decimal_fp<SignificandType, ExponentType, true, false> {
	SignificandType significand;
	ExponentType exponent;
	bool is_negative;
};

template <class SignificandType, class ExponentType>
struct decimal_fp<SignificandType, ExponentType, false, true> {
	SignificandType significand;
	ExponentType exponent;
	bool may_have_trailing_zeros;
};

template <class SignificandType, class ExponentType>
struct decimal_fp<SignificandType, ExponentType, true, true> {
	SignificandType significand;
	ExponentType exponent;
	bool may_have_trailing_zeros;
	bool is_negative;
};

template <class SignificandType, class ExponentType, bool trailing_zero_flag = false>
using unsigned_decimal_fp = decimal_fp<SignificandType, ExponentType, false, trailing_zero_flag>;

template <class SignificandType, class ExponentType, bool trailing_zero_flag = false>
using signed_decimal_fp = decimal_fp<SignificandType, ExponentType, true, trailing_zero_flag>;

template <class SignificandType, class ExponentType>
constexpr signed_decimal_fp<SignificandType, ExponentType, false> add_sign_to_unsigned_decimal_fp(
		bool is_negative, unsigned_decimal_fp<SignificandType, ExponentType, false> r) noexcept {
	return {r.significand, r.exponent, is_negative};
}

template <class SignificandType, class ExponentType>
constexpr signed_decimal_fp<SignificandType, ExponentType, true> add_sign_to_unsigned_decimal_fp(
		bool is_negative, unsigned_decimal_fp<SignificandType, ExponentType, true> r) noexcept {
	return {r.significand, r.exponent, r.may_have_trailing_zeros, is_negative};
}

namespace detail {
template <class UnsignedDecimalFp>
struct unsigned_decimal_fp_to_signed;

template <class SignificandType, class ExponentType, bool trailing_zero_flag>
struct unsigned_decimal_fp_to_signed<
		unsigned_decimal_fp<SignificandType, ExponentType, trailing_zero_flag>> {
	using type = signed_decimal_fp<SignificandType, ExponentType, trailing_zero_flag>;
};

template <class UnsignedDecimalFp>
using unsigned_decimal_fp_to_signed_t =
		typename unsigned_decimal_fp_to_signed<UnsignedDecimalFp>::type;
} // namespace detail


////////////////////////////////////////////////////////////////////////////////////////
// Computed cache entries.
////////////////////////////////////////////////////////////////////////////////////////

template <class FloatFormat, class Dummy = void>
struct cache_holder;

template <class Dummy>
struct cache_holder<ieee754_binary32, Dummy> {
	using cache_entry_type = detail::stdr::uint_least64_t;
	static constexpr int cache_bits = 64;
	static constexpr int min_k = -31;
	static constexpr int max_k = 46;
	static constexpr detail::array<cache_entry_type, detail::stdr::size_t(max_k - min_k + 1)> cache
			JKJ_STATIC_DATA_SECTION = {{__SPRT_UINT64_C(0x81ce'b32c'4b43'fcf5),
				__SPRT_UINT64_C(0xa242'5ff7'5e14'fc32), __SPRT_UINT64_C(0xcad2'f7f5'359a'3b3f),
				__SPRT_UINT64_C(0xfd87'b5f2'8300'ca0e), __SPRT_UINT64_C(0x9e74'd1b7'91e0'7e49),
				__SPRT_UINT64_C(0xc612'0625'7658'9ddb), __SPRT_UINT64_C(0xf796'87ae'd3ee'c552),
				__SPRT_UINT64_C(0x9abe'14cd'4475'3b53), __SPRT_UINT64_C(0xc16d'9a00'9592'8a28),
				__SPRT_UINT64_C(0xf1c9'0080'baf7'2cb2), __SPRT_UINT64_C(0x971d'a050'74da'7bef),
				__SPRT_UINT64_C(0xbce5'0864'9211'1aeb), __SPRT_UINT64_C(0xec1e'4a7d'b695'61a6),
				__SPRT_UINT64_C(0x9392'ee8e'921d'5d08), __SPRT_UINT64_C(0xb877'aa32'36a4'b44a),
				__SPRT_UINT64_C(0xe695'94be'c44d'e15c), __SPRT_UINT64_C(0x901d'7cf7'3ab0'acda),
				__SPRT_UINT64_C(0xb424'dc35'095c'd810), __SPRT_UINT64_C(0xe12e'1342'4bb4'0e14),
				__SPRT_UINT64_C(0x8cbc'cc09'6f50'88cc), __SPRT_UINT64_C(0xafeb'ff0b'cb24'aaff),
				__SPRT_UINT64_C(0xdbe6'fece'bded'd5bf), __SPRT_UINT64_C(0x8970'5f41'36b4'a598),
				__SPRT_UINT64_C(0xabcc'7711'8461'cefd), __SPRT_UINT64_C(0xd6bf'94d5'e57a'42bd),
				__SPRT_UINT64_C(0x8637'bd05'af6c'69b6), __SPRT_UINT64_C(0xa7c5'ac47'1b47'8424),
				__SPRT_UINT64_C(0xd1b7'1758'e219'652c), __SPRT_UINT64_C(0x8312'6e97'8d4f'df3c),
				__SPRT_UINT64_C(0xa3d7'0a3d'70a3'd70b), __SPRT_UINT64_C(0xcccc'cccc'cccc'cccd),
				__SPRT_UINT64_C(0x8000'0000'0000'0000), __SPRT_UINT64_C(0xa000'0000'0000'0000),
				__SPRT_UINT64_C(0xc800'0000'0000'0000), __SPRT_UINT64_C(0xfa00'0000'0000'0000),
				__SPRT_UINT64_C(0x9c40'0000'0000'0000), __SPRT_UINT64_C(0xc350'0000'0000'0000),
				__SPRT_UINT64_C(0xf424'0000'0000'0000), __SPRT_UINT64_C(0x9896'8000'0000'0000),
				__SPRT_UINT64_C(0xbebc'2000'0000'0000), __SPRT_UINT64_C(0xee6b'2800'0000'0000),
				__SPRT_UINT64_C(0x9502'f900'0000'0000), __SPRT_UINT64_C(0xba43'b740'0000'0000),
				__SPRT_UINT64_C(0xe8d4'a510'0000'0000), __SPRT_UINT64_C(0x9184'e72a'0000'0000),
				__SPRT_UINT64_C(0xb5e6'20f4'8000'0000), __SPRT_UINT64_C(0xe35f'a931'a000'0000),
				__SPRT_UINT64_C(0x8e1b'c9bf'0400'0000), __SPRT_UINT64_C(0xb1a2'bc2e'c500'0000),
				__SPRT_UINT64_C(0xde0b'6b3a'7640'0000), __SPRT_UINT64_C(0x8ac7'2304'89e8'0000),
				__SPRT_UINT64_C(0xad78'ebc5'ac62'0000), __SPRT_UINT64_C(0xd8d7'26b7'177a'8000),
				__SPRT_UINT64_C(0x8786'7832'6eac'9000), __SPRT_UINT64_C(0xa968'163f'0a57'b400),
				__SPRT_UINT64_C(0xd3c2'1bce'cced'a100), __SPRT_UINT64_C(0x8459'5161'4014'84a0),
				__SPRT_UINT64_C(0xa56f'a5b9'9019'a5c8), __SPRT_UINT64_C(0xcecb'8f27'f420'0f3a),
				__SPRT_UINT64_C(0x813f'3978'f894'0985), __SPRT_UINT64_C(0xa18f'07d7'36b9'0be6),
				__SPRT_UINT64_C(0xc9f2'c9cd'0467'4edf), __SPRT_UINT64_C(0xfc6f'7c40'4581'2297),
				__SPRT_UINT64_C(0x9dc5'ada8'2b70'b59e), __SPRT_UINT64_C(0xc537'1912'364c'e306),
				__SPRT_UINT64_C(0xf684'df56'c3e0'1bc7), __SPRT_UINT64_C(0x9a13'0b96'3a6c'115d),
				__SPRT_UINT64_C(0xc097'ce7b'c907'15b4), __SPRT_UINT64_C(0xf0bd'c21a'bb48'db21),
				__SPRT_UINT64_C(0x9676'9950'b50d'88f5), __SPRT_UINT64_C(0xbc14'3fa4'e250'eb32),
				__SPRT_UINT64_C(0xeb19'4f8e'1ae5'25fe), __SPRT_UINT64_C(0x92ef'd1b8'd0cf'37bf),
				__SPRT_UINT64_C(0xb7ab'c627'0503'05ae), __SPRT_UINT64_C(0xe596'b7b0'c643'c71a),
				__SPRT_UINT64_C(0x8f7e'32ce'7bea'5c70), __SPRT_UINT64_C(0xb35d'bf82'1ae4'f38c),
				__SPRT_UINT64_C(0xe035'2f62'a19e'306f)}};
};
#if !JKJ_HAS_INLINE_VARIABLE
// decltype(...) should not depend on Dummy; see
// https://stackoverflow.com/questions/76438400/decltype-on-static-variable-in-template-class.
template <class Dummy>
constexpr decltype(cache_holder<ieee754_binary32>::cache)
		cache_holder<ieee754_binary32, Dummy>::cache;
#endif

template <class Dummy>
struct cache_holder<ieee754_binary64, Dummy> {
	using cache_entry_type = detail::wuint::uint128;
	static constexpr int cache_bits = 128;
	static constexpr int min_k = -292;
	static constexpr int max_k = 326;
	static constexpr detail::array<cache_entry_type, detail::stdr::size_t(max_k - min_k + 1)> cache
			JKJ_STATIC_DATA_SECTION = {{{__SPRT_UINT64_C(0xff77'b1fc'bebc'dc4f),
											__SPRT_UINT64_C(0x25e8'e89c'13bb'0f7b)},
				{__SPRT_UINT64_C(0x9faa'cf3d'f736'09b1), __SPRT_UINT64_C(0x77b1'9161'8c54'e9ad)},
				{__SPRT_UINT64_C(0xc795'830d'7503'8c1d), __SPRT_UINT64_C(0xd59d'f5b9'ef6a'2418)},
				{__SPRT_UINT64_C(0xf97a'e3d0'd244'6f25), __SPRT_UINT64_C(0x4b05'7328'6b44'ad1e)},
				{__SPRT_UINT64_C(0x9bec'ce62'836a'c577), __SPRT_UINT64_C(0x4ee3'67f9'430a'ec33)},
				{__SPRT_UINT64_C(0xc2e8'01fb'2445'76d5), __SPRT_UINT64_C(0x229c'41f7'93cd'a740)},
				{__SPRT_UINT64_C(0xf3a2'0279'ed56'd48a), __SPRT_UINT64_C(0x6b43'5275'78c1'1110)},
				{__SPRT_UINT64_C(0x9845'418c'3456'44d6), __SPRT_UINT64_C(0x830a'1389'6b78'aaaa)},
				{__SPRT_UINT64_C(0xbe56'91ef'416b'd60c), __SPRT_UINT64_C(0x23cc'986b'c656'd554)},
				{__SPRT_UINT64_C(0xedec'366b'11c6'cb8f), __SPRT_UINT64_C(0x2cbf'be86'b7ec'8aa9)},
				{__SPRT_UINT64_C(0x94b3'a202'eb1c'3f39), __SPRT_UINT64_C(0x7bf7'd714'32f3'd6aa)},
				{__SPRT_UINT64_C(0xb9e0'8a83'a5e3'4f07), __SPRT_UINT64_C(0xdaf5'ccd9'3fb0'cc54)},
				{__SPRT_UINT64_C(0xe858'ad24'8f5c'22c9), __SPRT_UINT64_C(0xd1b3'400f'8f9c'ff69)},
				{__SPRT_UINT64_C(0x9137'6c36'd999'95be), __SPRT_UINT64_C(0x2310'0809'b9c2'1fa2)},
				{__SPRT_UINT64_C(0xb585'4744'8fff'fb2d), __SPRT_UINT64_C(0xabd4'0a0c'2832'a78b)},
				{__SPRT_UINT64_C(0xe2e6'9915'b3ff'f9f9), __SPRT_UINT64_C(0x16c9'0c8f'323f'516d)},
				{__SPRT_UINT64_C(0x8dd0'1fad'907f'fc3b), __SPRT_UINT64_C(0xae3d'a7d9'7f67'92e4)},
				{__SPRT_UINT64_C(0xb144'2798'f49f'fb4a), __SPRT_UINT64_C(0x99cd'11cf'df41'779d)},
				{__SPRT_UINT64_C(0xdd95'317f'31c7'fa1d), __SPRT_UINT64_C(0x4040'5643'd711'd584)},
				{__SPRT_UINT64_C(0x8a7d'3eef'7f1c'fc52), __SPRT_UINT64_C(0x4828'35ea'666b'2573)},
				{__SPRT_UINT64_C(0xad1c'8eab'5ee4'3b66), __SPRT_UINT64_C(0xda32'4365'0005'eed0)},
				{__SPRT_UINT64_C(0xd863'b256'369d'4a40), __SPRT_UINT64_C(0x90be'd43e'4007'6a83)},
				{__SPRT_UINT64_C(0x873e'4f75'e222'4e68), __SPRT_UINT64_C(0x5a77'44a6'e804'a292)},
				{__SPRT_UINT64_C(0xa90d'e353'5aaa'e202), __SPRT_UINT64_C(0x7115'15d0'a205'cb37)},
				{__SPRT_UINT64_C(0xd351'5c28'3155'9a83), __SPRT_UINT64_C(0x0d5a'5b44'ca87'3e04)},
				{__SPRT_UINT64_C(0x8412'd999'1ed5'8091), __SPRT_UINT64_C(0xe858'790a'fe94'86c3)},
				{__SPRT_UINT64_C(0xa517'8fff'668a'e0b6), __SPRT_UINT64_C(0x626e'974d'be39'a873)},
				{__SPRT_UINT64_C(0xce5d'73ff'402d'98e3), __SPRT_UINT64_C(0xfb0a'3d21'2dc8'1290)},
				{__SPRT_UINT64_C(0x80fa'687f'881c'7f8e), __SPRT_UINT64_C(0x7ce6'6634'bc9d'0b9a)},
				{__SPRT_UINT64_C(0xa139'029f'6a23'9f72), __SPRT_UINT64_C(0x1c1f'ffc1'ebc4'4e81)},
				{__SPRT_UINT64_C(0xc987'4347'44ac'874e), __SPRT_UINT64_C(0xa327'ffb2'66b5'6221)},
				{__SPRT_UINT64_C(0xfbe9'1419'15d7'a922), __SPRT_UINT64_C(0x4bf1'ff9f'0062'baa9)},
				{__SPRT_UINT64_C(0x9d71'ac8f'ada6'c9b5), __SPRT_UINT64_C(0x6f77'3fc3'603d'b4aa)},
				{__SPRT_UINT64_C(0xc4ce'17b3'9910'7c22), __SPRT_UINT64_C(0xcb55'0fb4'384d'21d4)},
				{__SPRT_UINT64_C(0xf601'9da0'7f54'9b2b), __SPRT_UINT64_C(0x7e2a'53a1'4660'6a49)},
				{__SPRT_UINT64_C(0x99c1'0284'4f94'e0fb), __SPRT_UINT64_C(0x2eda'7444'cbfc'426e)},
				{__SPRT_UINT64_C(0xc031'4325'637a'1939), __SPRT_UINT64_C(0xfa91'1155'fefb'5309)},
				{__SPRT_UINT64_C(0xf03d'93ee'bc58'9f88), __SPRT_UINT64_C(0x7935'55ab'7eba'27cb)},
				{__SPRT_UINT64_C(0x9626'7c75'35b7'63b5), __SPRT_UINT64_C(0x4bc1'558b'2f34'58df)},
				{__SPRT_UINT64_C(0xbbb0'1b92'8325'3ca2), __SPRT_UINT64_C(0x9eb1'aaed'fb01'6f17)},
				{__SPRT_UINT64_C(0xea9c'2277'23ee'8bcb), __SPRT_UINT64_C(0x465e'15a9'79c1'cadd)},
				{__SPRT_UINT64_C(0x92a1'958a'7675'175f), __SPRT_UINT64_C(0x0bfa'cd89'ec19'1eca)},
				{__SPRT_UINT64_C(0xb749'faed'1412'5d36), __SPRT_UINT64_C(0xcef9'80ec'671f'667c)},
				{__SPRT_UINT64_C(0xe51c'79a8'5916'f484), __SPRT_UINT64_C(0x82b7'e127'80e7'401b)},
				{__SPRT_UINT64_C(0x8f31'cc09'37ae'58d2), __SPRT_UINT64_C(0xd1b2'ecb8'b090'8811)},
				{__SPRT_UINT64_C(0xb2fe'3f0b'8599'ef07), __SPRT_UINT64_C(0x861f'a7e6'dcb4'aa16)},
				{__SPRT_UINT64_C(0xdfbd'cece'6700'6ac9), __SPRT_UINT64_C(0x67a7'91e0'93e1'd49b)},
				{__SPRT_UINT64_C(0x8bd6'a141'0060'42bd), __SPRT_UINT64_C(0xe0c8'bb2c'5c6d'24e1)},
				{__SPRT_UINT64_C(0xaecc'4991'4078'536d), __SPRT_UINT64_C(0x58fa'e9f7'7388'6e19)},
				{__SPRT_UINT64_C(0xda7f'5bf5'9096'6848), __SPRT_UINT64_C(0xaf39'a475'506a'899f)},
				{__SPRT_UINT64_C(0x888f'9979'7a5e'012d), __SPRT_UINT64_C(0x6d84'06c9'5242'9604)},
				{__SPRT_UINT64_C(0xaab3'7fd7'd8f5'8178), __SPRT_UINT64_C(0xc8e5'087b'a6d3'3b84)},
				{__SPRT_UINT64_C(0xd560'5fcd'cf32'e1d6), __SPRT_UINT64_C(0xfb1e'4a9a'9088'0a65)},
				{__SPRT_UINT64_C(0x855c'3be0'a17f'cd26), __SPRT_UINT64_C(0x5cf2'eea0'9a55'0680)},
				{__SPRT_UINT64_C(0xa6b3'4ad8'c9df'c06f), __SPRT_UINT64_C(0xf42f'aa48'c0ea'481f)},
				{__SPRT_UINT64_C(0xd060'1d8e'fc57'b08b), __SPRT_UINT64_C(0xf13b'94da'f124'da27)},
				{__SPRT_UINT64_C(0x823c'1279'5db6'ce57), __SPRT_UINT64_C(0x76c5'3d08'd6b7'0859)},
				{__SPRT_UINT64_C(0xa2cb'1717'b524'81ed), __SPRT_UINT64_C(0x5476'8c4b'0c64'ca6f)},
				{__SPRT_UINT64_C(0xcb7d'dcdd'a26d'a268), __SPRT_UINT64_C(0xa994'2f5d'cf7d'fd0a)},
				{__SPRT_UINT64_C(0xfe5d'5415'0b09'0b02), __SPRT_UINT64_C(0xd3f9'3b35'435d'7c4d)},
				{__SPRT_UINT64_C(0x9efa'548d'26e5'a6e1), __SPRT_UINT64_C(0xc47b'c501'4a1a'6db0)},
				{__SPRT_UINT64_C(0xc6b8'e9b0'709f'109a), __SPRT_UINT64_C(0x359a'b641'9ca1'091c)},
				{__SPRT_UINT64_C(0xf867'241c'8cc6'd4c0), __SPRT_UINT64_C(0xc301'63d2'03c9'4b63)},
				{__SPRT_UINT64_C(0x9b40'7691'd7fc'44f8), __SPRT_UINT64_C(0x79e0'de63'425d'cf1e)},
				{__SPRT_UINT64_C(0xc210'9436'4dfb'5636), __SPRT_UINT64_C(0x9859'15fc'12f5'42e5)},
				{__SPRT_UINT64_C(0xf294'b943'e17a'2bc4), __SPRT_UINT64_C(0x3e6f'5b7b'17b2'939e)},
				{__SPRT_UINT64_C(0x979c'f3ca'6cec'5b5a), __SPRT_UINT64_C(0xa705'992c'eecf'9c43)},
				{__SPRT_UINT64_C(0xbd84'30bd'0827'7231), __SPRT_UINT64_C(0x50c6'ff78'2a83'8354)},
				{__SPRT_UINT64_C(0xece5'3cec'4a31'4ebd), __SPRT_UINT64_C(0xa4f8'bf56'3524'6429)},
				{__SPRT_UINT64_C(0x940f'4613'ae5e'd136), __SPRT_UINT64_C(0x871b'7795'e136'be9a)},
				{__SPRT_UINT64_C(0xb913'1798'99f6'8584), __SPRT_UINT64_C(0x28e2'557b'5984'6e40)},
				{__SPRT_UINT64_C(0xe757'dd7e'c074'26e5), __SPRT_UINT64_C(0x331a'eada'2fe5'89d0)},
				{__SPRT_UINT64_C(0x9096'ea6f'3848'984f), __SPRT_UINT64_C(0x3ff0'd2c8'5def'7622)},
				{__SPRT_UINT64_C(0xb4bc'a50b'065a'be63), __SPRT_UINT64_C(0x0fed'077a'756b'53aa)},
				{__SPRT_UINT64_C(0xe1eb'ce4d'c7f1'6dfb), __SPRT_UINT64_C(0xd3e8'4959'12c6'2895)},
				{__SPRT_UINT64_C(0x8d33'60f0'9cf6'e4bd), __SPRT_UINT64_C(0x6471'2dd7'abbb'd95d)},
				{__SPRT_UINT64_C(0xb080'392c'c434'9dec), __SPRT_UINT64_C(0xbd8d'794d'96aa'cfb4)},
				{__SPRT_UINT64_C(0xdca0'4777'f541'c567), __SPRT_UINT64_C(0xecf0'd7a0'fc55'83a1)},
				{__SPRT_UINT64_C(0x89e4'2caa'f949'1b60), __SPRT_UINT64_C(0xf416'86c4'9db5'7245)},
				{__SPRT_UINT64_C(0xac5d'37d5'b79b'6239), __SPRT_UINT64_C(0x311c'2875'c522'ced6)},
				{__SPRT_UINT64_C(0xd774'85cb'2582'3ac7), __SPRT_UINT64_C(0x7d63'3293'366b'828c)},
				{__SPRT_UINT64_C(0x86a8'd39e'f771'64bc), __SPRT_UINT64_C(0xae5d'ff9c'0203'3198)},
				{__SPRT_UINT64_C(0xa853'0886'b54d'bdeb), __SPRT_UINT64_C(0xd9f5'7f83'0283'fdfd)},
				{__SPRT_UINT64_C(0xd267'caa8'62a1'2d66), __SPRT_UINT64_C(0xd072'df63'c324'fd7c)},
				{__SPRT_UINT64_C(0x8380'dea9'3da4'bc60), __SPRT_UINT64_C(0x4247'cb9e'59f7'1e6e)},
				{__SPRT_UINT64_C(0xa461'1653'8d0d'eb78), __SPRT_UINT64_C(0x52d9'be85'f074'e609)},
				{__SPRT_UINT64_C(0xcd79'5be8'7051'6656), __SPRT_UINT64_C(0x6790'2e27'6c92'1f8c)},
				{__SPRT_UINT64_C(0x806b'd971'4632'dff6), __SPRT_UINT64_C(0x00ba'1cd8'a3db'53b7)},
				{__SPRT_UINT64_C(0xa086'cfcd'97bf'97f3), __SPRT_UINT64_C(0x80e8'a40e'ccd2'28a5)},
				{__SPRT_UINT64_C(0xc8a8'83c0'fdaf'7df0), __SPRT_UINT64_C(0x6122'cd12'8006'b2ce)},
				{__SPRT_UINT64_C(0xfad2'a4b1'3d1b'5d6c), __SPRT_UINT64_C(0x796b'8057'2008'5f82)},
				{__SPRT_UINT64_C(0x9cc3'a6ee'c631'1a63), __SPRT_UINT64_C(0xcbe3'3036'7405'3bb1)},
				{__SPRT_UINT64_C(0xc3f4'90aa'77bd'60fc), __SPRT_UINT64_C(0xbedb'fc44'1106'8a9d)},
				{__SPRT_UINT64_C(0xf4f1'b4d5'15ac'b93b), __SPRT_UINT64_C(0xee92'fb55'1548'2d45)},
				{__SPRT_UINT64_C(0x9917'1105'2d8b'f3c5), __SPRT_UINT64_C(0x751b'dd15'2d4d'1c4b)},
				{__SPRT_UINT64_C(0xbf5c'd546'78ee'f0b6), __SPRT_UINT64_C(0xd262'd45a'78a0'635e)},
				{__SPRT_UINT64_C(0xef34'0a98'172a'ace4), __SPRT_UINT64_C(0x86fb'8971'16c8'7c35)},
				{__SPRT_UINT64_C(0x9580'869f'0e7a'ac0e), __SPRT_UINT64_C(0xd45d'35e6'ae3d'4da1)},
				{__SPRT_UINT64_C(0xbae0'a846'd219'5712), __SPRT_UINT64_C(0x8974'8360'59cc'a10a)},
				{__SPRT_UINT64_C(0xe998'd258'869f'acd7), __SPRT_UINT64_C(0x2bd1'a438'703f'c94c)},
				{__SPRT_UINT64_C(0x91ff'8377'5423'cc06), __SPRT_UINT64_C(0x7b63'06a3'4627'ddd0)},
				{__SPRT_UINT64_C(0xb67f'6455'292c'bf08), __SPRT_UINT64_C(0x1a3b'c84c'17b1'd543)},
				{__SPRT_UINT64_C(0xe41f'3d6a'7377'eeca), __SPRT_UINT64_C(0x20ca'ba5f'1d9e'4a94)},
				{__SPRT_UINT64_C(0x8e93'8662'882a'f53e), __SPRT_UINT64_C(0x547e'b47b'7282'ee9d)},
				{__SPRT_UINT64_C(0xb238'67fb'2a35'b28d), __SPRT_UINT64_C(0xe99e'619a'4f23'aa44)},
				{__SPRT_UINT64_C(0xdec6'81f9'f4c3'1f31), __SPRT_UINT64_C(0x6405'fa00'e2ec'94d5)},
				{__SPRT_UINT64_C(0x8b3c'113c'38f9'f37e), __SPRT_UINT64_C(0xde83'bc40'8dd3'dd05)},
				{__SPRT_UINT64_C(0xae0b'158b'4738'705e), __SPRT_UINT64_C(0x9624'ab50'b148'd446)},
				{__SPRT_UINT64_C(0xd98d'daee'1906'8c76), __SPRT_UINT64_C(0x3bad'd624'dd9b'0958)},
				{__SPRT_UINT64_C(0x87f8'a8d4'cfa4'17c9), __SPRT_UINT64_C(0xe54c'a5d7'0a80'e5d7)},
				{__SPRT_UINT64_C(0xa9f6'd30a'038d'1dbc), __SPRT_UINT64_C(0x5e9f'cf4c'cd21'1f4d)},
				{__SPRT_UINT64_C(0xd474'87cc'8470'652b), __SPRT_UINT64_C(0x7647'c320'0069'6720)},
				{__SPRT_UINT64_C(0x84c8'd4df'd2c6'3f3b), __SPRT_UINT64_C(0x29ec'd9f4'0041'e074)},
				{__SPRT_UINT64_C(0xa5fb'0a17'c777'cf09), __SPRT_UINT64_C(0xf468'1071'0052'5891)},
				{__SPRT_UINT64_C(0xcf79'cc9d'b955'c2cc), __SPRT_UINT64_C(0x7182'148d'4066'eeb5)},
				{__SPRT_UINT64_C(0x81ac'1fe2'93d5'99bf), __SPRT_UINT64_C(0xc6f1'4cd8'4840'5531)},
				{__SPRT_UINT64_C(0xa217'27db'38cb'002f), __SPRT_UINT64_C(0xb8ad'a00e'5a50'6a7d)},
				{__SPRT_UINT64_C(0xca9c'f1d2'06fd'c03b), __SPRT_UINT64_C(0xa6d9'0811'f0e4'851d)},
				{__SPRT_UINT64_C(0xfd44'2e46'88bd'304a), __SPRT_UINT64_C(0x908f'4a16'6d1d'a664)},
				{__SPRT_UINT64_C(0x9e4a'9cec'1576'3e2e), __SPRT_UINT64_C(0x9a59'8e4e'0432'87ff)},
				{__SPRT_UINT64_C(0xc5dd'4427'1ad3'cdba), __SPRT_UINT64_C(0x40ef'f1e1'853f'29fe)},
				{__SPRT_UINT64_C(0xf754'9530'e188'c128), __SPRT_UINT64_C(0xd12b'ee59'e68e'f47d)},
				{__SPRT_UINT64_C(0x9a94'dd3e'8cf5'78b9), __SPRT_UINT64_C(0x82bb'74f8'3019'58cf)},
				{__SPRT_UINT64_C(0xc13a'148e'3032'd6e7), __SPRT_UINT64_C(0xe36a'5236'3c1f'af02)},
				{__SPRT_UINT64_C(0xf188'99b1'bc3f'8ca1), __SPRT_UINT64_C(0xdc44'e6c3'cb27'9ac2)},
				{__SPRT_UINT64_C(0x96f5'600f'15a7'b7e5), __SPRT_UINT64_C(0x29ab'103a'5ef8'c0ba)},
				{__SPRT_UINT64_C(0xbcb2'b812'db11'a5de), __SPRT_UINT64_C(0x7415'd448'f6b6'f0e8)},
				{__SPRT_UINT64_C(0xebdf'6617'91d6'0f56), __SPRT_UINT64_C(0x111b'495b'3464'ad22)},
				{__SPRT_UINT64_C(0x936b'9fce'bb25'c995), __SPRT_UINT64_C(0xcab1'0dd9'00be'ec35)},
				{__SPRT_UINT64_C(0xb846'87c2'69ef'3bfb), __SPRT_UINT64_C(0x3d5d'514f'40ee'a743)},
				{__SPRT_UINT64_C(0xe658'29b3'046b'0afa), __SPRT_UINT64_C(0x0cb4'a5a3'112a'5113)},
				{__SPRT_UINT64_C(0x8ff7'1a0f'e2c2'e6dc), __SPRT_UINT64_C(0x47f0'e785'eaba'72ac)},
				{__SPRT_UINT64_C(0xb3f4'e093'db73'a093), __SPRT_UINT64_C(0x59ed'2167'6569'0f57)},
				{__SPRT_UINT64_C(0xe0f2'18b8'd250'88b8), __SPRT_UINT64_C(0x3068'69c1'3ec3'532d)},
				{__SPRT_UINT64_C(0x8c97'4f73'8372'5573), __SPRT_UINT64_C(0x1e41'4218'c73a'13fc)},
				{__SPRT_UINT64_C(0xafbd'2350'644e'eacf), __SPRT_UINT64_C(0xe5d1'929e'f908'98fb)},
				{__SPRT_UINT64_C(0xdbac'6c24'7d62'a583), __SPRT_UINT64_C(0xdf45'f746'b74a'bf3a)},
				{__SPRT_UINT64_C(0x894b'c396'ce5d'a772), __SPRT_UINT64_C(0x6b8b'ba8c'328e'b784)},
				{__SPRT_UINT64_C(0xab9e'b47c'81f5'114f), __SPRT_UINT64_C(0x066e'a92f'3f32'6565)},
				{__SPRT_UINT64_C(0xd686'619b'a272'55a2), __SPRT_UINT64_C(0xc80a'537b'0efe'febe)},
				{__SPRT_UINT64_C(0x8613'fd01'4587'7585), __SPRT_UINT64_C(0xbd06'742c'e95f'5f37)},
				{__SPRT_UINT64_C(0xa798'fc41'96e9'52e7), __SPRT_UINT64_C(0x2c48'1138'23b7'3705)},
				{__SPRT_UINT64_C(0xd17f'3b51'fca3'a7a0), __SPRT_UINT64_C(0xf75a'1586'2ca5'04c6)},
				{__SPRT_UINT64_C(0x82ef'8513'3de6'48c4), __SPRT_UINT64_C(0x9a98'4d73'dbe7'22fc)},
				{__SPRT_UINT64_C(0xa3ab'6658'0d5f'daf5), __SPRT_UINT64_C(0xc13e'60d0'd2e0'ebbb)},
				{__SPRT_UINT64_C(0xcc96'3fee'10b7'd1b3), __SPRT_UINT64_C(0x318d'f905'0799'26a9)},
				{__SPRT_UINT64_C(0xffbb'cfe9'94e5'c61f), __SPRT_UINT64_C(0xfdf1'7746'497f'7053)},
				{__SPRT_UINT64_C(0x9fd5'61f1'fd0f'9bd3), __SPRT_UINT64_C(0xfeb6'ea8b'edef'a634)},
				{__SPRT_UINT64_C(0xc7ca'ba6e'7c53'82c8), __SPRT_UINT64_C(0xfe64'a52e'e96b'8fc1)},
				{__SPRT_UINT64_C(0xf9bd'690a'1b68'637b), __SPRT_UINT64_C(0x3dfd'ce7a'a3c6'73b1)},
				{__SPRT_UINT64_C(0x9c16'61a6'5121'3e2d), __SPRT_UINT64_C(0x06be'a10c'a65c'084f)},
				{__SPRT_UINT64_C(0xc31b'fa0f'e569'8db8), __SPRT_UINT64_C(0x486e'494f'cff3'0a63)},
				{__SPRT_UINT64_C(0xf3e2'f893'dec3'f126), __SPRT_UINT64_C(0x5a89'dba3'c3ef'ccfb)},
				{__SPRT_UINT64_C(0x986d'db5c'6b3a'76b7), __SPRT_UINT64_C(0xf896'2946'5a75'e01d)},
				{__SPRT_UINT64_C(0xbe89'5233'8609'1465), __SPRT_UINT64_C(0xf6bb'b397'f113'5824)},
				{__SPRT_UINT64_C(0xee2b'a6c0'678b'597f), __SPRT_UINT64_C(0x746a'a07d'ed58'2e2d)},
				{__SPRT_UINT64_C(0x94db'4838'40b7'17ef), __SPRT_UINT64_C(0xa8c2'a44e'b457'1cdd)},
				{__SPRT_UINT64_C(0xba12'1a46'50e4'ddeb), __SPRT_UINT64_C(0x92f3'4d62'616c'e414)},
				{__SPRT_UINT64_C(0xe896'a0d7'e51e'1566), __SPRT_UINT64_C(0x77b0'20ba'f9c8'1d18)},
				{__SPRT_UINT64_C(0x915e'2486'ef32'cd60), __SPRT_UINT64_C(0x0ace'1474'dc1d'122f)},
				{__SPRT_UINT64_C(0xb5b5'ada8'aaff'80b8), __SPRT_UINT64_C(0x0d81'9992'1324'56bb)},
				{__SPRT_UINT64_C(0xe323'1912'd5bf'60e6), __SPRT_UINT64_C(0x10e1'fff6'97ed'6c6a)},
				{__SPRT_UINT64_C(0x8df5'efab'c597'9c8f), __SPRT_UINT64_C(0xca8d'3ffa'1ef4'63c2)},
				{__SPRT_UINT64_C(0xb173'6b96'b6fd'83b3), __SPRT_UINT64_C(0xbd30'8ff8'a6b1'7cb3)},
				{__SPRT_UINT64_C(0xddd0'467c'64bc'e4a0), __SPRT_UINT64_C(0xac7c'b3f6'd05d'dbdf)},
				{__SPRT_UINT64_C(0x8aa2'2c0d'bef6'0ee4), __SPRT_UINT64_C(0x6bcd'f07a'423a'a96c)},
				{__SPRT_UINT64_C(0xad4a'b711'2eb3'929d), __SPRT_UINT64_C(0x86c1'6c98'd2c9'53c7)},
				{__SPRT_UINT64_C(0xd89d'64d5'7a60'7744), __SPRT_UINT64_C(0xe871'c7bf'077b'a8b8)},
				{__SPRT_UINT64_C(0x8762'5f05'6c7c'4a8b), __SPRT_UINT64_C(0x1147'1cd7'64ad'4973)},
				{__SPRT_UINT64_C(0xa93a'f6c6'c79b'5d2d), __SPRT_UINT64_C(0xd598'e40d'3dd8'9bd0)},
				{__SPRT_UINT64_C(0xd389'b478'7982'3479), __SPRT_UINT64_C(0x4aff'1d10'8d4e'c2c4)},
				{__SPRT_UINT64_C(0x8436'10cb'4bf1'60cb), __SPRT_UINT64_C(0xcedf'722a'5851'39bb)},
				{__SPRT_UINT64_C(0xa543'94fe'1eed'b8fe), __SPRT_UINT64_C(0xc297'4eb4'ee65'8829)},
				{__SPRT_UINT64_C(0xce94'7a3d'a6a9'273e), __SPRT_UINT64_C(0x733d'2262'29fe'ea33)},
				{__SPRT_UINT64_C(0x811c'cc66'8829'b887), __SPRT_UINT64_C(0x0806'357d'5a3f'5260)},
				{__SPRT_UINT64_C(0xa163'ff80'2a34'26a8), __SPRT_UINT64_C(0xca07'c2dc'b0cf'26f8)},
				{__SPRT_UINT64_C(0xc9bc'ff60'34c1'3052), __SPRT_UINT64_C(0xfc89'b393'dd02'f0b6)},
				{__SPRT_UINT64_C(0xfc2c'3f38'41f1'7c67), __SPRT_UINT64_C(0xbbac'2078'd443'ace3)},
				{__SPRT_UINT64_C(0x9d9b'a783'2936'edc0), __SPRT_UINT64_C(0xd54b'944b'84aa'4c0e)},
				{__SPRT_UINT64_C(0xc502'9163'f384'a931), __SPRT_UINT64_C(0x0a9e'795e'65d4'df12)},
				{__SPRT_UINT64_C(0xf643'35bc'f065'd37d), __SPRT_UINT64_C(0x4d46'17b5'ff4a'16d6)},
				{__SPRT_UINT64_C(0x99ea'0196'163f'a42e), __SPRT_UINT64_C(0x504b'ced1'bf8e'4e46)},
				{__SPRT_UINT64_C(0xc064'81fb'9bcf'8d39), __SPRT_UINT64_C(0xe45e'c286'2f71'e1d7)},
				{__SPRT_UINT64_C(0xf07d'a27a'82c3'7088), __SPRT_UINT64_C(0x5d76'7327'bb4e'5a4d)},
				{__SPRT_UINT64_C(0x964e'858c'91ba'2655), __SPRT_UINT64_C(0x3a6a'07f8'd510'f870)},
				{__SPRT_UINT64_C(0xbbe2'26ef'b628'afea), __SPRT_UINT64_C(0x8904'89f7'0a55'368c)},
				{__SPRT_UINT64_C(0xeada'b0ab'a3b2'dbe5), __SPRT_UINT64_C(0x2b45'ac74'ccea'842f)},
				{__SPRT_UINT64_C(0x92c8'ae6b'464f'c96f), __SPRT_UINT64_C(0x3b0b'8bc9'0012'929e)},
				{__SPRT_UINT64_C(0xb77a'da06'17e3'bbcb), __SPRT_UINT64_C(0x09ce'6ebb'4017'3745)},
				{__SPRT_UINT64_C(0xe559'9087'9ddc'aabd), __SPRT_UINT64_C(0xcc42'0a6a'101d'0516)},
				{__SPRT_UINT64_C(0x8f57'fa54'c2a9'eab6), __SPRT_UINT64_C(0x9fa9'4682'4a12'232e)},
				{__SPRT_UINT64_C(0xb32d'f8e9'f354'6564), __SPRT_UINT64_C(0x4793'9822'dc96'abfa)},
				{__SPRT_UINT64_C(0xdff9'7724'7029'7ebd), __SPRT_UINT64_C(0x5978'7e2b'93bc'56f8)},
				{__SPRT_UINT64_C(0x8bfb'ea76'c619'ef36), __SPRT_UINT64_C(0x57eb'4edb'3c55'b65b)},
				{__SPRT_UINT64_C(0xaefa'e514'77a0'6b03), __SPRT_UINT64_C(0xede6'2292'0b6b'23f2)},
				{__SPRT_UINT64_C(0xdab9'9e59'9588'85c4), __SPRT_UINT64_C(0xe95f'ab36'8e45'ecee)},
				{__SPRT_UINT64_C(0x88b4'02f7'fd75'539b), __SPRT_UINT64_C(0x11db'cb02'18eb'b415)},
				{__SPRT_UINT64_C(0xaae1'03b5'fcd2'a881), __SPRT_UINT64_C(0xd652'bdc2'9f26'a11a)},
				{__SPRT_UINT64_C(0xd599'44a3'7c07'52a2), __SPRT_UINT64_C(0x4be7'6d33'46f0'4960)},
				{__SPRT_UINT64_C(0x857f'cae6'2d84'93a5), __SPRT_UINT64_C(0x6f70'a440'0c56'2ddc)},
				{__SPRT_UINT64_C(0xa6df'bd9f'b8e5'b88e), __SPRT_UINT64_C(0xcb4c'cd50'0f6b'b953)},
				{__SPRT_UINT64_C(0xd097'ad07'a71f'26b2), __SPRT_UINT64_C(0x7e20'00a4'1346'a7a8)},
				{__SPRT_UINT64_C(0x825e'cc24'c873'782f), __SPRT_UINT64_C(0x8ed4'0066'8c0c'28c9)},
				{__SPRT_UINT64_C(0xa2f6'7f2d'fa90'563b), __SPRT_UINT64_C(0x7289'0080'2f0f'32fb)},
				{__SPRT_UINT64_C(0xcbb4'1ef9'7934'6bca), __SPRT_UINT64_C(0x4f2b'40a0'3ad2'ffba)},
				{__SPRT_UINT64_C(0xfea1'26b7'd781'86bc), __SPRT_UINT64_C(0xe2f6'10c8'4987'bfa9)},
				{__SPRT_UINT64_C(0x9f24'b832'e6b0'f436), __SPRT_UINT64_C(0x0dd9'ca7d'2df4'd7ca)},
				{__SPRT_UINT64_C(0xc6ed'e63f'a05d'3143), __SPRT_UINT64_C(0x9150'3d1c'7972'0dbc)},
				{__SPRT_UINT64_C(0xf8a9'5fcf'8874'7d94), __SPRT_UINT64_C(0x75a4'4c63'97ce'912b)},
				{__SPRT_UINT64_C(0x9b69'dbe1'b548'ce7c), __SPRT_UINT64_C(0xc986'afbe'3ee1'1abb)},
				{__SPRT_UINT64_C(0xc244'52da'229b'021b), __SPRT_UINT64_C(0xfbe8'5bad'ce99'6169)},
				{__SPRT_UINT64_C(0xf2d5'6790'ab41'c2a2), __SPRT_UINT64_C(0xfae2'7299'423f'b9c4)},
				{__SPRT_UINT64_C(0x97c5'60ba'6b09'19a5), __SPRT_UINT64_C(0xdccd'879f'c967'd41b)},
				{__SPRT_UINT64_C(0xbdb6'b8e9'05cb'600f), __SPRT_UINT64_C(0x5400'e987'bbc1'c921)},
				{__SPRT_UINT64_C(0xed24'6723'473e'3813), __SPRT_UINT64_C(0x2901'23e9'aab2'3b69)},
				{__SPRT_UINT64_C(0x9436'c076'0c86'e30b), __SPRT_UINT64_C(0xf9a0'b672'0aaf'6522)},
				{__SPRT_UINT64_C(0xb944'7093'8fa8'9bce), __SPRT_UINT64_C(0xf808'e40e'8d5b'3e6a)},
				{__SPRT_UINT64_C(0xe795'8cb8'7392'c2c2), __SPRT_UINT64_C(0xb60b'1d12'30b2'0e05)},
				{__SPRT_UINT64_C(0x90bd'77f3'483b'b9b9), __SPRT_UINT64_C(0xb1c6'f22b'5e6f'48c3)},
				{__SPRT_UINT64_C(0xb4ec'd5f0'1a4a'a828), __SPRT_UINT64_C(0x1e38'aeb6'360b'1af4)},
				{__SPRT_UINT64_C(0xe228'0b6c'20dd'5232), __SPRT_UINT64_C(0x25c6'da63'c38d'e1b1)},
				{__SPRT_UINT64_C(0x8d59'0723'948a'535f), __SPRT_UINT64_C(0x579c'487e'5a38'ad0f)},
				{__SPRT_UINT64_C(0xb0af'48ec'79ac'e837), __SPRT_UINT64_C(0x2d83'5a9d'f0c6'd852)},
				{__SPRT_UINT64_C(0xdcdb'1b27'9818'2244), __SPRT_UINT64_C(0xf8e4'3145'6cf8'8e66)},
				{__SPRT_UINT64_C(0x8a08'f0f8'bf0f'156b), __SPRT_UINT64_C(0x1b8e'9ecb'641b'5900)},
				{__SPRT_UINT64_C(0xac8b'2d36'eed2'dac5), __SPRT_UINT64_C(0xe272'467e'3d22'2f40)},
				{__SPRT_UINT64_C(0xd7ad'f884'aa87'9177), __SPRT_UINT64_C(0x5b0e'd81d'cc6a'bb10)},
				{__SPRT_UINT64_C(0x86cc'bb52'ea94'baea), __SPRT_UINT64_C(0x98e9'4712'9fc2'b4ea)},
				{__SPRT_UINT64_C(0xa87f'ea27'a539'e9a5), __SPRT_UINT64_C(0x3f23'98d7'47b3'6225)},
				{__SPRT_UINT64_C(0xd29f'e4b1'8e88'640e), __SPRT_UINT64_C(0x8eec'7f0d'19a0'3aae)},
				{__SPRT_UINT64_C(0x83a3'eeee'f915'3e89), __SPRT_UINT64_C(0x1953'cf68'3004'24ad)},
				{__SPRT_UINT64_C(0xa48c'eaaa'b75a'8e2b), __SPRT_UINT64_C(0x5fa8'c342'3c05'2dd8)},
				{__SPRT_UINT64_C(0xcdb0'2555'6531'31b6), __SPRT_UINT64_C(0x3792'f412'cb06'794e)},
				{__SPRT_UINT64_C(0x808e'1755'5f3e'bf11), __SPRT_UINT64_C(0xe2bb'd88b'bee4'0bd1)},
				{__SPRT_UINT64_C(0xa0b1'9d2a'b70e'6ed6), __SPRT_UINT64_C(0x5b6a'ceae'ae9d'0ec5)},
				{__SPRT_UINT64_C(0xc8de'0475'64d2'0a8b), __SPRT_UINT64_C(0xf245'825a'5a44'5276)},
				{__SPRT_UINT64_C(0xfb15'8592'be06'8d2e), __SPRT_UINT64_C(0xeed6'e2f0'f0d5'6713)},
				{__SPRT_UINT64_C(0x9ced'737b'b6c4'183d), __SPRT_UINT64_C(0x5546'4dd6'9685'606c)},
				{__SPRT_UINT64_C(0xc428'd05a'a475'1e4c), __SPRT_UINT64_C(0xaa97'e14c'3c26'b887)},
				{__SPRT_UINT64_C(0xf533'0471'4d92'65df), __SPRT_UINT64_C(0xd53d'd99f'4b30'66a9)},
				{__SPRT_UINT64_C(0x993f'e2c6'd07b'7fab), __SPRT_UINT64_C(0xe546'a803'8efe'402a)},
				{__SPRT_UINT64_C(0xbf8f'db78'849a'5f96), __SPRT_UINT64_C(0xde98'5204'72bd'd034)},
				{__SPRT_UINT64_C(0xef73'd256'a5c0'f77c), __SPRT_UINT64_C(0x963e'6685'8f6d'4441)},
				{__SPRT_UINT64_C(0x95a8'6376'2798'9aad), __SPRT_UINT64_C(0xdde7'0013'79a4'4aa9)},
				{__SPRT_UINT64_C(0xbb12'7c53'b17e'c159), __SPRT_UINT64_C(0x5560'c018'580d'5d53)},
				{__SPRT_UINT64_C(0xe9d7'1b68'9dde'71af), __SPRT_UINT64_C(0xaab8'f01e'6e10'b4a7)},
				{__SPRT_UINT64_C(0x9226'7121'62ab'070d), __SPRT_UINT64_C(0xcab3'9613'04ca'70e9)},
				{__SPRT_UINT64_C(0xb6b0'0d69'bb55'c8d1), __SPRT_UINT64_C(0x3d60'7b97'c5fd'0d23)},
				{__SPRT_UINT64_C(0xe45c'10c4'2a2b'3b05), __SPRT_UINT64_C(0x8cb8'9a7d'b77c'506b)},
				{__SPRT_UINT64_C(0x8eb9'8a7a'9a5b'04e3), __SPRT_UINT64_C(0x77f3'608e'92ad'b243)},
				{__SPRT_UINT64_C(0xb267'ed19'40f1'c61c), __SPRT_UINT64_C(0x55f0'38b2'3759'1ed4)},
				{__SPRT_UINT64_C(0xdf01'e85f'912e'37a3), __SPRT_UINT64_C(0x6b6c'46de'c52f'6689)},
				{__SPRT_UINT64_C(0x8b61'313b'babc'e2c6), __SPRT_UINT64_C(0x2323'ac4b'3b3d'a016)},
				{__SPRT_UINT64_C(0xae39'7d8a'a96c'1b77), __SPRT_UINT64_C(0xabec'975e'0a0d'081b)},
				{__SPRT_UINT64_C(0xd9c7'dced'53c7'2255), __SPRT_UINT64_C(0x96e7'bd35'8c90'4a22)},
				{__SPRT_UINT64_C(0x881c'ea14'545c'7575), __SPRT_UINT64_C(0x7e50'd641'77da'2e55)},
				{__SPRT_UINT64_C(0xaa24'2499'6973'92d2), __SPRT_UINT64_C(0xdde5'0bd1'd5d0'b9ea)},
				{__SPRT_UINT64_C(0xd4ad'2dbf'c3d0'7787), __SPRT_UINT64_C(0x955e'4ec6'4b44'e865)},
				{__SPRT_UINT64_C(0x84ec'3c97'da62'4ab4), __SPRT_UINT64_C(0xbd5a'f13b'ef0b'113f)},
				{__SPRT_UINT64_C(0xa627'4bbd'd0fa'dd61), __SPRT_UINT64_C(0xecb1'ad8a'eacd'd58f)},
				{__SPRT_UINT64_C(0xcfb1'1ead'4539'94ba), __SPRT_UINT64_C(0x67de'18ed'a581'4af3)},
				{__SPRT_UINT64_C(0x81ce'b32c'4b43'fcf4), __SPRT_UINT64_C(0x80ea'cf94'8770'ced8)},
				{__SPRT_UINT64_C(0xa242'5ff7'5e14'fc31), __SPRT_UINT64_C(0xa125'8379'a94d'028e)},
				{__SPRT_UINT64_C(0xcad2'f7f5'359a'3b3e), __SPRT_UINT64_C(0x096e'e458'13a0'4331)},
				{__SPRT_UINT64_C(0xfd87'b5f2'8300'ca0d), __SPRT_UINT64_C(0x8bca'9d6e'1888'53fd)},
				{__SPRT_UINT64_C(0x9e74'd1b7'91e0'7e48), __SPRT_UINT64_C(0x775e'a264'cf55'347e)},
				{__SPRT_UINT64_C(0xc612'0625'7658'9dda), __SPRT_UINT64_C(0x9536'4afe'032a'819e)},
				{__SPRT_UINT64_C(0xf796'87ae'd3ee'c551), __SPRT_UINT64_C(0x3a83'ddbd'83f5'2205)},
				{__SPRT_UINT64_C(0x9abe'14cd'4475'3b52), __SPRT_UINT64_C(0xc492'6a96'7279'3543)},
				{__SPRT_UINT64_C(0xc16d'9a00'9592'8a27), __SPRT_UINT64_C(0x75b7'053c'0f17'8294)},
				{__SPRT_UINT64_C(0xf1c9'0080'baf7'2cb1), __SPRT_UINT64_C(0x5324'c68b'12dd'6339)},
				{__SPRT_UINT64_C(0x971d'a050'74da'7bee), __SPRT_UINT64_C(0xd3f6'fc16'ebca'5e04)},
				{__SPRT_UINT64_C(0xbce5'0864'9211'1aea), __SPRT_UINT64_C(0x88f4'bb1c'a6bc'f585)},
				{__SPRT_UINT64_C(0xec1e'4a7d'b695'61a5), __SPRT_UINT64_C(0x2b31'e9e3'd06c'32e6)},
				{__SPRT_UINT64_C(0x9392'ee8e'921d'5d07), __SPRT_UINT64_C(0x3aff'322e'6243'9fd0)},
				{__SPRT_UINT64_C(0xb877'aa32'36a4'b449), __SPRT_UINT64_C(0x09be'feb9'fad4'87c3)},
				{__SPRT_UINT64_C(0xe695'94be'c44d'e15b), __SPRT_UINT64_C(0x4c2e'be68'7989'a9b4)},
				{__SPRT_UINT64_C(0x901d'7cf7'3ab0'acd9), __SPRT_UINT64_C(0x0f9d'3701'4bf6'0a11)},
				{__SPRT_UINT64_C(0xb424'dc35'095c'd80f), __SPRT_UINT64_C(0x5384'84c1'9ef3'8c95)},
				{__SPRT_UINT64_C(0xe12e'1342'4bb4'0e13), __SPRT_UINT64_C(0x2865'a5f2'06b0'6fba)},
				{__SPRT_UINT64_C(0x8cbc'cc09'6f50'88cb), __SPRT_UINT64_C(0xf93f'87b7'442e'45d4)},
				{__SPRT_UINT64_C(0xafeb'ff0b'cb24'aafe), __SPRT_UINT64_C(0xf78f'69a5'1539'd749)},
				{__SPRT_UINT64_C(0xdbe6'fece'bded'd5be), __SPRT_UINT64_C(0xb573'440e'5a88'4d1c)},
				{__SPRT_UINT64_C(0x8970'5f41'36b4'a597), __SPRT_UINT64_C(0x3168'0a88'f895'3031)},
				{__SPRT_UINT64_C(0xabcc'7711'8461'cefc), __SPRT_UINT64_C(0xfdc2'0d2b'36ba'7c3e)},
				{__SPRT_UINT64_C(0xd6bf'94d5'e57a'42bc), __SPRT_UINT64_C(0x3d32'9076'0469'1b4d)},
				{__SPRT_UINT64_C(0x8637'bd05'af6c'69b5), __SPRT_UINT64_C(0xa63f'9a49'c2c1'b110)},
				{__SPRT_UINT64_C(0xa7c5'ac47'1b47'8423), __SPRT_UINT64_C(0x0fcf'80dc'3372'1d54)},
				{__SPRT_UINT64_C(0xd1b7'1758'e219'652b), __SPRT_UINT64_C(0xd3c3'6113'404e'a4a9)},
				{__SPRT_UINT64_C(0x8312'6e97'8d4f'df3b), __SPRT_UINT64_C(0x645a'1cac'0831'26ea)},
				{__SPRT_UINT64_C(0xa3d7'0a3d'70a3'd70a), __SPRT_UINT64_C(0x3d70'a3d7'0a3d'70a4)},
				{__SPRT_UINT64_C(0xcccc'cccc'cccc'cccc), __SPRT_UINT64_C(0xcccc'cccc'cccc'cccd)},
				{__SPRT_UINT64_C(0x8000'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xa000'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xc800'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xfa00'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9c40'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xc350'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xf424'0000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9896'8000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xbebc'2000'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xee6b'2800'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9502'f900'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xba43'b740'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xe8d4'a510'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9184'e72a'0000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xb5e6'20f4'8000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xe35f'a931'a000'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x8e1b'c9bf'0400'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xb1a2'bc2e'c500'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xde0b'6b3a'7640'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x8ac7'2304'89e8'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xad78'ebc5'ac62'0000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xd8d7'26b7'177a'8000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x8786'7832'6eac'9000), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xa968'163f'0a57'b400), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xd3c2'1bce'cced'a100), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x8459'5161'4014'84a0), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xa56f'a5b9'9019'a5c8), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xcecb'8f27'f420'0f3a), __SPRT_UINT64_C(0x0000'0000'0000'0000)},
				{__SPRT_UINT64_C(0x813f'3978'f894'0984), __SPRT_UINT64_C(0x4000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xa18f'07d7'36b9'0be5), __SPRT_UINT64_C(0x5000'0000'0000'0000)},
				{__SPRT_UINT64_C(0xc9f2'c9cd'0467'4ede), __SPRT_UINT64_C(0xa400'0000'0000'0000)},
				{__SPRT_UINT64_C(0xfc6f'7c40'4581'2296), __SPRT_UINT64_C(0x4d00'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9dc5'ada8'2b70'b59d), __SPRT_UINT64_C(0xf020'0000'0000'0000)},
				{__SPRT_UINT64_C(0xc537'1912'364c'e305), __SPRT_UINT64_C(0x6c28'0000'0000'0000)},
				{__SPRT_UINT64_C(0xf684'df56'c3e0'1bc6), __SPRT_UINT64_C(0xc732'0000'0000'0000)},
				{__SPRT_UINT64_C(0x9a13'0b96'3a6c'115c), __SPRT_UINT64_C(0x3c7f'4000'0000'0000)},
				{__SPRT_UINT64_C(0xc097'ce7b'c907'15b3), __SPRT_UINT64_C(0x4b9f'1000'0000'0000)},
				{__SPRT_UINT64_C(0xf0bd'c21a'bb48'db20), __SPRT_UINT64_C(0x1e86'd400'0000'0000)},
				{__SPRT_UINT64_C(0x9676'9950'b50d'88f4), __SPRT_UINT64_C(0x1314'4480'0000'0000)},
				{__SPRT_UINT64_C(0xbc14'3fa4'e250'eb31), __SPRT_UINT64_C(0x17d9'55a0'0000'0000)},
				{__SPRT_UINT64_C(0xeb19'4f8e'1ae5'25fd), __SPRT_UINT64_C(0x5dcf'ab08'0000'0000)},
				{__SPRT_UINT64_C(0x92ef'd1b8'd0cf'37be), __SPRT_UINT64_C(0x5aa1'cae5'0000'0000)},
				{__SPRT_UINT64_C(0xb7ab'c627'0503'05ad), __SPRT_UINT64_C(0xf14a'3d9e'4000'0000)},
				{__SPRT_UINT64_C(0xe596'b7b0'c643'c719), __SPRT_UINT64_C(0x6d9c'cd05'd000'0000)},
				{__SPRT_UINT64_C(0x8f7e'32ce'7bea'5c6f), __SPRT_UINT64_C(0xe482'0023'a200'0000)},
				{__SPRT_UINT64_C(0xb35d'bf82'1ae4'f38b), __SPRT_UINT64_C(0xdda2'802c'8a80'0000)},
				{__SPRT_UINT64_C(0xe035'2f62'a19e'306e), __SPRT_UINT64_C(0xd50b'2037'ad20'0000)},
				{__SPRT_UINT64_C(0x8c21'3d9d'a502'de45), __SPRT_UINT64_C(0x4526'f422'cc34'0000)},
				{__SPRT_UINT64_C(0xaf29'8d05'0e43'95d6), __SPRT_UINT64_C(0x9670'b12b'7f41'0000)},
				{__SPRT_UINT64_C(0xdaf3'f046'51d4'7b4c), __SPRT_UINT64_C(0x3c0c'dd76'5f11'4000)},
				{__SPRT_UINT64_C(0x88d8'762b'f324'cd0f), __SPRT_UINT64_C(0xa588'0a69'fb6a'c800)},
				{__SPRT_UINT64_C(0xab0e'93b6'efee'0053), __SPRT_UINT64_C(0x8eea'0d04'7a45'7a00)},
				{__SPRT_UINT64_C(0xd5d2'38a4'abe9'8068), __SPRT_UINT64_C(0x72a4'9045'98d6'd880)},
				{__SPRT_UINT64_C(0x85a3'6366'eb71'f041), __SPRT_UINT64_C(0x47a6'da2b'7f86'4750)},
				{__SPRT_UINT64_C(0xa70c'3c40'a64e'6c51), __SPRT_UINT64_C(0x9990'90b6'5f67'd924)},
				{__SPRT_UINT64_C(0xd0cf'4b50'cfe2'0765), __SPRT_UINT64_C(0xfff4'b4e3'f741'cf6d)},
				{__SPRT_UINT64_C(0x8281'8f12'81ed'449f), __SPRT_UINT64_C(0xbff8'f10e'7a89'21a5)},
				{__SPRT_UINT64_C(0xa321'f2d7'2268'95c7), __SPRT_UINT64_C(0xaff7'2d52'192b'6a0e)},
				{__SPRT_UINT64_C(0xcbea'6f8c'eb02'bb39), __SPRT_UINT64_C(0x9bf4'f8a6'9f76'4491)},
				{__SPRT_UINT64_C(0xfee5'0b70'25c3'6a08), __SPRT_UINT64_C(0x02f2'36d0'4753'd5b5)},
				{__SPRT_UINT64_C(0x9f4f'2726'179a'2245), __SPRT_UINT64_C(0x01d7'6242'2c94'6591)},
				{__SPRT_UINT64_C(0xc722'f0ef'9d80'aad6), __SPRT_UINT64_C(0x424d'3ad2'b7b9'7ef6)},
				{__SPRT_UINT64_C(0xf8eb'ad2b'84e0'd58b), __SPRT_UINT64_C(0xd2e0'8987'65a7'deb3)},
				{__SPRT_UINT64_C(0x9b93'4c3b'330c'8577), __SPRT_UINT64_C(0x63cc'55f4'9f88'eb30)},
				{__SPRT_UINT64_C(0xc278'1f49'ffcf'a6d5), __SPRT_UINT64_C(0x3cbf'6b71'c76b'25fc)},
				{__SPRT_UINT64_C(0xf316'271c'7fc3'908a), __SPRT_UINT64_C(0x8bef'464e'3945'ef7b)},
				{__SPRT_UINT64_C(0x97ed'd871'cfda'3a56), __SPRT_UINT64_C(0x9775'8bf0'e3cb'b5ad)},
				{__SPRT_UINT64_C(0xbde9'4e8e'43d0'c8ec), __SPRT_UINT64_C(0x3d52'eeed'1cbe'a318)},
				{__SPRT_UINT64_C(0xed63'a231'd4c4'fb27), __SPRT_UINT64_C(0x4ca7'aaa8'63ee'4bde)},
				{__SPRT_UINT64_C(0x945e'455f'24fb'1cf8), __SPRT_UINT64_C(0x8fe8'caa9'3e74'ef6b)},
				{__SPRT_UINT64_C(0xb975'd6b6'ee39'e436), __SPRT_UINT64_C(0xb3e2'fd53'8e12'2b45)},
				{__SPRT_UINT64_C(0xe7d3'4c64'a9c8'5d44), __SPRT_UINT64_C(0x60db'bca8'7196'b617)},
				{__SPRT_UINT64_C(0x90e4'0fbe'ea1d'3a4a), __SPRT_UINT64_C(0xbc89'55e9'46fe'31ce)},
				{__SPRT_UINT64_C(0xb51d'13ae'a4a4'88dd), __SPRT_UINT64_C(0x6bab'ab63'98bd'be42)},
				{__SPRT_UINT64_C(0xe264'589a'4dcd'ab14), __SPRT_UINT64_C(0xc696'963c'7eed'2dd2)},
				{__SPRT_UINT64_C(0x8d7e'b760'70a0'8aec), __SPRT_UINT64_C(0xfc1e'1de5'cf54'3ca3)},
				{__SPRT_UINT64_C(0xb0de'6538'8cc8'ada8), __SPRT_UINT64_C(0x3b25'a55f'4329'4bcc)},
				{__SPRT_UINT64_C(0xdd15'fe86'affa'd912), __SPRT_UINT64_C(0x49ef'0eb7'13f3'9ebf)},
				{__SPRT_UINT64_C(0x8a2d'bf14'2dfc'c7ab), __SPRT_UINT64_C(0x6e35'6932'6c78'4338)},
				{__SPRT_UINT64_C(0xacb9'2ed9'397b'f996), __SPRT_UINT64_C(0x49c2'c37f'0796'5405)},
				{__SPRT_UINT64_C(0xd7e7'7a8f'87da'f7fb), __SPRT_UINT64_C(0xdc33'745e'c97b'e907)},
				{__SPRT_UINT64_C(0x86f0'ac99'b4e8'dafd), __SPRT_UINT64_C(0x69a0'28bb'3ded'71a4)},
				{__SPRT_UINT64_C(0xa8ac'd7c0'2223'11bc), __SPRT_UINT64_C(0xc408'32ea'0d68'ce0d)},
				{__SPRT_UINT64_C(0xd2d8'0db0'2aab'd62b), __SPRT_UINT64_C(0xf50a'3fa4'90c3'0191)},
				{__SPRT_UINT64_C(0x83c7'088e'1aab'65db), __SPRT_UINT64_C(0x7926'67c6'da79'e0fb)},
				{__SPRT_UINT64_C(0xa4b8'cab1'a156'3f52), __SPRT_UINT64_C(0x5770'01b8'9118'5939)},
				{__SPRT_UINT64_C(0xcde6'fd5e'09ab'cf26), __SPRT_UINT64_C(0xed4c'0226'b55e'6f87)},
				{__SPRT_UINT64_C(0x80b0'5e5a'c60b'6178), __SPRT_UINT64_C(0x544f'8158'315b'05b5)},
				{__SPRT_UINT64_C(0xa0dc'75f1'778e'39d6), __SPRT_UINT64_C(0x6963'61ae'3db1'c722)},
				{__SPRT_UINT64_C(0xc913'936d'd571'c84c), __SPRT_UINT64_C(0x03bc'3a19'cd1e'38ea)},
				{__SPRT_UINT64_C(0xfb58'7849'4ace'3a5f), __SPRT_UINT64_C(0x04ab'48a0'4065'c724)},
				{__SPRT_UINT64_C(0x9d17'4b2d'cec0'e47b), __SPRT_UINT64_C(0x62eb'0d64'283f'9c77)},
				{__SPRT_UINT64_C(0xc45d'1df9'4271'1d9a), __SPRT_UINT64_C(0x3ba5'd0bd'324f'8395)},
				{__SPRT_UINT64_C(0xf574'6577'930d'6500), __SPRT_UINT64_C(0xca8f'44ec'7ee3'647a)},
				{__SPRT_UINT64_C(0x9968'bf6a'bbe8'5f20), __SPRT_UINT64_C(0x7e99'8b13'cf4e'1ecc)},
				{__SPRT_UINT64_C(0xbfc2'ef45'6ae2'76e8), __SPRT_UINT64_C(0x9e3f'edd8'c321'a67f)},
				{__SPRT_UINT64_C(0xefb3'ab16'c59b'14a2), __SPRT_UINT64_C(0xc5cf'e94e'f3ea'101f)},
				{__SPRT_UINT64_C(0x95d0'4aee'3b80'ece5), __SPRT_UINT64_C(0xbba1'f1d1'5872'4a13)},
				{__SPRT_UINT64_C(0xbb44'5da9'ca61'281f), __SPRT_UINT64_C(0x2a8a'6e45'ae8e'dc98)},
				{__SPRT_UINT64_C(0xea15'7514'3cf9'7226), __SPRT_UINT64_C(0xf52d'09d7'1a32'93be)},
				{__SPRT_UINT64_C(0x924d'692c'a61b'e758), __SPRT_UINT64_C(0x593c'2626'705f'9c57)},
				{__SPRT_UINT64_C(0xb6e0'c377'cfa2'e12e), __SPRT_UINT64_C(0x6f8b'2fb0'0c77'836d)},
				{__SPRT_UINT64_C(0xe498'f455'c38b'997a), __SPRT_UINT64_C(0x0b6d'fb9c'0f95'6448)},
				{__SPRT_UINT64_C(0x8edf'98b5'9a37'3fec), __SPRT_UINT64_C(0x4724'bd41'89bd'5ead)},
				{__SPRT_UINT64_C(0xb297'7ee3'00c5'0fe7), __SPRT_UINT64_C(0x58ed'ec91'ec2c'b658)},
				{__SPRT_UINT64_C(0xdf3d'5e9b'c0f6'53e1), __SPRT_UINT64_C(0x2f29'67b6'6737'e3ee)},
				{__SPRT_UINT64_C(0x8b86'5b21'5899'f46c), __SPRT_UINT64_C(0xbd79'e0d2'0082'ee75)},
				{__SPRT_UINT64_C(0xae67'f1e9'aec0'7187), __SPRT_UINT64_C(0xecd8'5906'80a3'aa12)},
				{__SPRT_UINT64_C(0xda01'ee64'1a70'8de9), __SPRT_UINT64_C(0xe80e'6f48'20cc'9496)},
				{__SPRT_UINT64_C(0x8841'34fe'9086'58b2), __SPRT_UINT64_C(0x3109'058d'147f'dcde)},
				{__SPRT_UINT64_C(0xaa51'823e'34a7'eede), __SPRT_UINT64_C(0xbd4b'46f0'599f'd416)},
				{__SPRT_UINT64_C(0xd4e5'e2cd'c1d1'ea96), __SPRT_UINT64_C(0x6c9e'18ac'7007'c91b)},
				{__SPRT_UINT64_C(0x850f'adc0'9923'329e), __SPRT_UINT64_C(0x03e2'cf6b'c604'ddb1)},
				{__SPRT_UINT64_C(0xa653'9930'bf6b'ff45), __SPRT_UINT64_C(0x84db'8346'b786'151d)},
				{__SPRT_UINT64_C(0xcfe8'7f7c'ef46'ff16), __SPRT_UINT64_C(0xe612'6418'6567'9a64)},
				{__SPRT_UINT64_C(0x81f1'4fae'158c'5f6e), __SPRT_UINT64_C(0x4fcb'7e8f'3f60'c07f)},
				{__SPRT_UINT64_C(0xa26d'a399'9aef'7749), __SPRT_UINT64_C(0xe3be'5e33'0f38'f09e)},
				{__SPRT_UINT64_C(0xcb09'0c80'01ab'551c), __SPRT_UINT64_C(0x5cad'f5bf'd307'2cc6)},
				{__SPRT_UINT64_C(0xfdcb'4fa0'0216'2a63), __SPRT_UINT64_C(0x73d9'732f'c7c8'f7f7)},
				{__SPRT_UINT64_C(0x9e9f'11c4'014d'da7e), __SPRT_UINT64_C(0x2867'e7fd'dcdd'9afb)},
				{__SPRT_UINT64_C(0xc646'd635'01a1'511d), __SPRT_UINT64_C(0xb281'e1fd'5415'01b9)},
				{__SPRT_UINT64_C(0xf7d8'8bc2'4209'a565), __SPRT_UINT64_C(0x1f22'5a7c'a91a'4227)},
				{__SPRT_UINT64_C(0x9ae7'5759'6946'075f), __SPRT_UINT64_C(0x3375'788d'e9b0'6959)},
				{__SPRT_UINT64_C(0xc1a1'2d2f'c397'8937), __SPRT_UINT64_C(0x0052'd6b1'641c'83af)},
				{__SPRT_UINT64_C(0xf209'787b'b47d'6b84), __SPRT_UINT64_C(0xc067'8c5d'bd23'a49b)},
				{__SPRT_UINT64_C(0x9745'eb4d'50ce'6332), __SPRT_UINT64_C(0xf840'b7ba'9636'46e1)},
				{__SPRT_UINT64_C(0xbd17'6620'a501'fbff), __SPRT_UINT64_C(0xb650'e5a9'3bc3'd899)},
				{__SPRT_UINT64_C(0xec5d'3fa8'ce42'7aff), __SPRT_UINT64_C(0xa3e5'1f13'8ab4'cebf)},
				{__SPRT_UINT64_C(0x93ba'47c9'80e9'8cdf), __SPRT_UINT64_C(0xc66f'336c'36b1'0138)},
				{__SPRT_UINT64_C(0xb8a8'd9bb'e123'f017), __SPRT_UINT64_C(0xb80b'0047'445d'4185)},
				{__SPRT_UINT64_C(0xe6d3'102a'd96c'ec1d), __SPRT_UINT64_C(0xa60d'c059'1574'91e6)},
				{__SPRT_UINT64_C(0x9043'ea1a'c7e4'1392), __SPRT_UINT64_C(0x87c8'9837'ad68'db30)},
				{__SPRT_UINT64_C(0xb454'e4a1'79dd'1877), __SPRT_UINT64_C(0x29ba'be45'98c3'11fc)},
				{__SPRT_UINT64_C(0xe16a'1dc9'd854'5e94), __SPRT_UINT64_C(0xf429'6dd6'fef3'd67b)},
				{__SPRT_UINT64_C(0x8ce2'529e'2734'bb1d), __SPRT_UINT64_C(0x1899'e4a6'5f58'660d)},
				{__SPRT_UINT64_C(0xb01a'e745'b101'e9e4), __SPRT_UINT64_C(0x5ec0'5dcf'f72e'7f90)},
				{__SPRT_UINT64_C(0xdc21'a117'1d42'645d), __SPRT_UINT64_C(0x7670'7543'f4fa'1f74)},
				{__SPRT_UINT64_C(0x8995'04ae'7249'7eba), __SPRT_UINT64_C(0x6a06'494a'791c'53a9)},
				{__SPRT_UINT64_C(0xabfa'45da'0edb'de69), __SPRT_UINT64_C(0x0487'db9d'1763'6893)},
				{__SPRT_UINT64_C(0xd6f8'd750'9292'd603), __SPRT_UINT64_C(0x45a9'd284'5d3c'42b7)},
				{__SPRT_UINT64_C(0x865b'8692'5b9b'c5c2), __SPRT_UINT64_C(0x0b8a'2392'ba45'a9b3)},
				{__SPRT_UINT64_C(0xa7f2'6836'f282'b732), __SPRT_UINT64_C(0x8e6c'ac77'68d7'141f)},
				{__SPRT_UINT64_C(0xd1ef'0244'af23'64ff), __SPRT_UINT64_C(0x3207'd795'430c'd927)},
				{__SPRT_UINT64_C(0x8335'616a'ed76'1f1f), __SPRT_UINT64_C(0x7f44'e6bd'49e8'07b9)},
				{__SPRT_UINT64_C(0xa402'b9c5'a8d3'a6e7), __SPRT_UINT64_C(0x5f16'206c'9c62'09a7)},
				{__SPRT_UINT64_C(0xcd03'6837'1308'90a1), __SPRT_UINT64_C(0x36db'a887'c37a'8c10)},
				{__SPRT_UINT64_C(0x8022'2122'6be5'5a64), __SPRT_UINT64_C(0xc249'4954'da2c'978a)},
				{__SPRT_UINT64_C(0xa02a'a96b'06de'b0fd), __SPRT_UINT64_C(0xf2db'9baa'10b7'bd6d)},
				{__SPRT_UINT64_C(0xc835'53c5'c896'5d3d), __SPRT_UINT64_C(0x6f92'8294'94e5'acc8)},
				{__SPRT_UINT64_C(0xfa42'a8b7'3abb'f48c), __SPRT_UINT64_C(0xcb77'2339'ba1f'17fa)},
				{__SPRT_UINT64_C(0x9c69'a972'84b5'78d7), __SPRT_UINT64_C(0xff2a'7604'1453'6efc)},
				{__SPRT_UINT64_C(0xc384'13cf'25e2'd70d), __SPRT_UINT64_C(0xfef5'1385'1968'4abb)},
				{__SPRT_UINT64_C(0xf465'18c2'ef5b'8cd1), __SPRT_UINT64_C(0x7eb2'5866'5fc2'5d6a)},
				{__SPRT_UINT64_C(0x98bf'2f79'd599'3802), __SPRT_UINT64_C(0xef2f'773f'fbd9'7a62)},
				{__SPRT_UINT64_C(0xbeee'fb58'4aff'8603), __SPRT_UINT64_C(0xaafb'550f'facf'd8fb)},
				{__SPRT_UINT64_C(0xeeaa'ba2e'5dbf'6784), __SPRT_UINT64_C(0x95ba'2a53'f983'cf39)},
				{__SPRT_UINT64_C(0x952a'b45c'fa97'a0b2), __SPRT_UINT64_C(0xdd94'5a74'7bf2'6184)},
				{__SPRT_UINT64_C(0xba75'6174'393d'88df), __SPRT_UINT64_C(0x94f9'7111'9aee'f9e5)},
				{__SPRT_UINT64_C(0xe912'b9d1'478c'eb17), __SPRT_UINT64_C(0x7a37'cd56'01aa'b85e)},
				{__SPRT_UINT64_C(0x91ab'b422'ccb8'12ee), __SPRT_UINT64_C(0xac62'e055'c10a'b33b)},
				{__SPRT_UINT64_C(0xb616'a12b'7fe6'17aa), __SPRT_UINT64_C(0x577b'986b'314d'600a)},
				{__SPRT_UINT64_C(0xe39c'4976'5fdf'9d94), __SPRT_UINT64_C(0xed5a'7e85'fda0'b80c)},
				{__SPRT_UINT64_C(0x8e41'ade9'fbeb'c27d), __SPRT_UINT64_C(0x1458'8f13'be84'7308)},
				{__SPRT_UINT64_C(0xb1d2'1964'7ae6'b31c), __SPRT_UINT64_C(0x596e'b2d8'ae25'8fc9)},
				{__SPRT_UINT64_C(0xde46'9fbd'99a0'5fe3), __SPRT_UINT64_C(0x6fca'5f8e'd9ae'f3bc)},
				{__SPRT_UINT64_C(0x8aec'23d6'8004'3bee), __SPRT_UINT64_C(0x25de'7bb9'480d'5855)},
				{__SPRT_UINT64_C(0xada7'2ccc'2005'4ae9), __SPRT_UINT64_C(0xaf56'1aa7'9a10'ae6b)},
				{__SPRT_UINT64_C(0xd910'f7ff'2806'9da4), __SPRT_UINT64_C(0x1b2b'a151'8094'da05)},
				{__SPRT_UINT64_C(0x87aa'9aff'7904'2286), __SPRT_UINT64_C(0x90fb'44d2'f05d'0843)},
				{__SPRT_UINT64_C(0xa995'41bf'5745'2b28), __SPRT_UINT64_C(0x353a'1607'ac74'4a54)},
				{__SPRT_UINT64_C(0xd3fa'922f'2d16'75f2), __SPRT_UINT64_C(0x4288'9b89'9791'5ce9)},
				{__SPRT_UINT64_C(0x847c'9b5d'7c2e'09b7), __SPRT_UINT64_C(0x6995'6135'feba'da12)},
				{__SPRT_UINT64_C(0xa59b'c234'db39'8c25), __SPRT_UINT64_C(0x43fa'b983'7e69'9096)},
				{__SPRT_UINT64_C(0xcf02'b2c2'1207'ef2e), __SPRT_UINT64_C(0x94f9'67e4'5e03'f4bc)},
				{__SPRT_UINT64_C(0x8161'afb9'4b44'f57d), __SPRT_UINT64_C(0x1d1b'e0ee'bac2'78f6)},
				{__SPRT_UINT64_C(0xa1ba'1ba7'9e16'32dc), __SPRT_UINT64_C(0x6462'd92a'6973'1733)},
				{__SPRT_UINT64_C(0xca28'a291'859b'bf93), __SPRT_UINT64_C(0x7d7b'8f75'03cf'dcff)},
				{__SPRT_UINT64_C(0xfcb2'cb35'e702'af78), __SPRT_UINT64_C(0x5cda'7352'44c3'd43f)},
				{__SPRT_UINT64_C(0x9def'bf01'b061'adab), __SPRT_UINT64_C(0x3a08'8813'6afa'64a8)},
				{__SPRT_UINT64_C(0xc56b'aec2'1c7a'1916), __SPRT_UINT64_C(0x088a'aa18'45b8'fdd1)},
				{__SPRT_UINT64_C(0xf6c6'9a72'a398'9f5b), __SPRT_UINT64_C(0x8aad'549e'5727'3d46)},
				{__SPRT_UINT64_C(0x9a3c'2087'a63f'6399), __SPRT_UINT64_C(0x36ac'54e2'f678'864c)},
				{__SPRT_UINT64_C(0xc0cb'28a9'8fcf'3c7f), __SPRT_UINT64_C(0x8457'6a1b'b416'a7de)},
				{__SPRT_UINT64_C(0xf0fd'f2d3'f3c3'0b9f), __SPRT_UINT64_C(0x656d'44a2'a11c'51d6)},
				{__SPRT_UINT64_C(0x969e'b7c4'7859'e743), __SPRT_UINT64_C(0x9f64'4ae5'a4b1'b326)},
				{__SPRT_UINT64_C(0xbc46'65b5'9670'6114), __SPRT_UINT64_C(0x873d'5d9f'0dde'1fef)},
				{__SPRT_UINT64_C(0xeb57'ff22'fc0c'7959), __SPRT_UINT64_C(0xa90c'b506'd155'a7eb)},
				{__SPRT_UINT64_C(0x9316'ff75'dd87'cbd8), __SPRT_UINT64_C(0x09a7'f124'42d5'88f3)},
				{__SPRT_UINT64_C(0xb7dc'bf53'54e9'bece), __SPRT_UINT64_C(0x0c11'ed6d'538a'eb30)},
				{__SPRT_UINT64_C(0xe5d3'ef28'2a24'2e81), __SPRT_UINT64_C(0x8f16'68c8'a86d'a5fb)},
				{__SPRT_UINT64_C(0x8fa4'7579'1a56'9d10), __SPRT_UINT64_C(0xf96e'017d'6944'87bd)},
				{__SPRT_UINT64_C(0xb38d'92d7'60ec'4455), __SPRT_UINT64_C(0x37c9'81dc'c395'a9ad)},
				{__SPRT_UINT64_C(0xe070'f78d'3927'556a), __SPRT_UINT64_C(0x85bb'e253'f47b'1418)},
				{__SPRT_UINT64_C(0x8c46'9ab8'43b8'9562), __SPRT_UINT64_C(0x9395'6d74'78cc'ec8f)},
				{__SPRT_UINT64_C(0xaf58'4166'54a6'babb), __SPRT_UINT64_C(0x387a'c8d1'9700'27b3)},
				{__SPRT_UINT64_C(0xdb2e'51bf'e9d0'696a), __SPRT_UINT64_C(0x0699'7b05'fcc0'319f)},
				{__SPRT_UINT64_C(0x88fc'f317'f222'41e2), __SPRT_UINT64_C(0x441f'ece3'bdf8'1f04)},
				{__SPRT_UINT64_C(0xab3c'2fdd'eeaa'd25a), __SPRT_UINT64_C(0xd527'e81c'ad76'26c4)},
				{__SPRT_UINT64_C(0xd60b'3bd5'6a55'86f1), __SPRT_UINT64_C(0x8a71'e223'd8d3'b075)},
				{__SPRT_UINT64_C(0x85c7'0565'6275'7456), __SPRT_UINT64_C(0xf687'2d56'6784'4e4a)},
				{__SPRT_UINT64_C(0xa738'c6be'bb12'd16c), __SPRT_UINT64_C(0xb428'f8ac'0165'61dc)},
				{__SPRT_UINT64_C(0xd106'f86e'69d7'85c7), __SPRT_UINT64_C(0xe133'36d7'01be'ba53)},
				{__SPRT_UINT64_C(0x82a4'5b45'0226'b39c), __SPRT_UINT64_C(0xecc0'0246'6117'3474)},
				{__SPRT_UINT64_C(0xa34d'7216'42b0'6084), __SPRT_UINT64_C(0x27f0'02d7'f95d'0191)},
				{__SPRT_UINT64_C(0xcc20'ce9b'd35c'78a5), __SPRT_UINT64_C(0x31ec'038d'f7b4'41f5)},
				{__SPRT_UINT64_C(0xff29'0242'c833'96ce), __SPRT_UINT64_C(0x7e67'0471'75a1'5272)},
				{__SPRT_UINT64_C(0x9f79'a169'bd20'3e41), __SPRT_UINT64_C(0x0f00'62c6'e984'd387)},
				{__SPRT_UINT64_C(0xc758'09c4'2c68'4dd1), __SPRT_UINT64_C(0x52c0'7b78'a3e6'0869)},
				{__SPRT_UINT64_C(0xf92e'0c35'3782'6145), __SPRT_UINT64_C(0xa770'9a56'ccdf'8a83)},
				{__SPRT_UINT64_C(0x9bbc'c7a1'42b1'7ccb), __SPRT_UINT64_C(0x88a6'6076'400b'b692)},
				{__SPRT_UINT64_C(0xc2ab'f989'935d'dbfe), __SPRT_UINT64_C(0x6acf'f893'd00e'a436)},
				{__SPRT_UINT64_C(0xf356'f7eb'f835'52fe), __SPRT_UINT64_C(0x0583'f6b8'c412'4d44)},
				{__SPRT_UINT64_C(0x9816'5af3'7b21'53de), __SPRT_UINT64_C(0xc372'7a33'7a8b'704b)},
				{__SPRT_UINT64_C(0xbe1b'f1b0'59e9'a8d6), __SPRT_UINT64_C(0x744f'18c0'592e'4c5d)},
				{__SPRT_UINT64_C(0xeda2'ee1c'7064'130c), __SPRT_UINT64_C(0x1162'def0'6f79'df74)},
				{__SPRT_UINT64_C(0x9485'd4d1'c63e'8be7), __SPRT_UINT64_C(0x8add'cb56'45ac'2ba9)},
				{__SPRT_UINT64_C(0xb9a7'4a06'37ce'2ee1), __SPRT_UINT64_C(0x6d95'3e2b'd717'3693)},
				{__SPRT_UINT64_C(0xe811'1c87'c5c1'ba99), __SPRT_UINT64_C(0xc8fa'8db6'ccdd'0438)},
				{__SPRT_UINT64_C(0x910a'b1d4'db99'14a0), __SPRT_UINT64_C(0x1d9c'9892'400a'22a3)},
				{__SPRT_UINT64_C(0xb54d'5e4a'127f'59c8), __SPRT_UINT64_C(0x2503'beb6'd00c'ab4c)},
				{__SPRT_UINT64_C(0xe2a0'b5dc'971f'303a), __SPRT_UINT64_C(0x2e44'ae64'840f'd61e)},
				{__SPRT_UINT64_C(0x8da4'71a9'de73'7e24), __SPRT_UINT64_C(0x5cea'ecfe'd289'e5d3)},
				{__SPRT_UINT64_C(0xb10d'8e14'5610'5dad), __SPRT_UINT64_C(0x7425'a83e'872c'5f48)},
				{__SPRT_UINT64_C(0xdd50'f199'6b94'7518), __SPRT_UINT64_C(0xd12f'124e'28f7'771a)},
				{__SPRT_UINT64_C(0x8a52'96ff'e33c'c92f), __SPRT_UINT64_C(0x82bd'6b70'd99a'aa70)},
				{__SPRT_UINT64_C(0xace7'3cbf'dc0b'fb7b), __SPRT_UINT64_C(0x636c'c64d'1001'550c)},
				{__SPRT_UINT64_C(0xd821'0bef'd30e'fa5a), __SPRT_UINT64_C(0x3c47'f7e0'5401'aa4f)},
				{__SPRT_UINT64_C(0x8714'a775'e3e9'5c78), __SPRT_UINT64_C(0x65ac'faec'3481'0a72)},
				{__SPRT_UINT64_C(0xa8d9'd153'5ce3'b396), __SPRT_UINT64_C(0x7f18'39a7'41a1'4d0e)},
				{__SPRT_UINT64_C(0xd310'45a8'341c'a07c), __SPRT_UINT64_C(0x1ede'4811'1209'a051)},
				{__SPRT_UINT64_C(0x83ea'2b89'2091'e44d), __SPRT_UINT64_C(0x934a'ed0a'ab46'0433)},
				{__SPRT_UINT64_C(0xa4e4'b66b'68b6'5d60), __SPRT_UINT64_C(0xf81d'a84d'5617'8540)},
				{__SPRT_UINT64_C(0xce1d'e406'42e3'f4b9), __SPRT_UINT64_C(0x3625'1260'ab9d'668f)},
				{__SPRT_UINT64_C(0x80d2'ae83'e9ce'78f3), __SPRT_UINT64_C(0xc1d7'2b7c'6b42'601a)},
				{__SPRT_UINT64_C(0xa107'5a24'e442'1730), __SPRT_UINT64_C(0xb24c'f65b'8612'f820)},
				{__SPRT_UINT64_C(0xc949'30ae'1d52'9cfc), __SPRT_UINT64_C(0xdee0'33f2'6797'b628)},
				{__SPRT_UINT64_C(0xfb9b'7cd9'a4a7'443c), __SPRT_UINT64_C(0x1698'40ef'017d'a3b2)},
				{__SPRT_UINT64_C(0x9d41'2e08'06e8'8aa5), __SPRT_UINT64_C(0x8e1f'2895'60ee'864f)},
				{__SPRT_UINT64_C(0xc491'798a'08a2'ad4e), __SPRT_UINT64_C(0xf1a6'f2ba'b92a'27e3)},
				{__SPRT_UINT64_C(0xf5b5'd7ec'8acb'58a2), __SPRT_UINT64_C(0xae10'af69'6774'b1dc)},
				{__SPRT_UINT64_C(0x9991'a6f3'd6bf'1765), __SPRT_UINT64_C(0xacca'6da1'e0a8'ef2a)},
				{__SPRT_UINT64_C(0xbff6'10b0'cc6e'dd3f), __SPRT_UINT64_C(0x17fd'090a'58d3'2af4)},
				{__SPRT_UINT64_C(0xeff3'94dc'ff8a'948e), __SPRT_UINT64_C(0xddfc'4b4c'ef07'f5b1)},
				{__SPRT_UINT64_C(0x95f8'3d0a'1fb6'9cd9), __SPRT_UINT64_C(0x4abd'af10'1564'f98f)},
				{__SPRT_UINT64_C(0xbb76'4c4c'a7a4'440f), __SPRT_UINT64_C(0x9d6d'1ad4'1abe'37f2)},
				{__SPRT_UINT64_C(0xea53'df5f'd18d'5513), __SPRT_UINT64_C(0x84c8'6189'216d'c5ee)},
				{__SPRT_UINT64_C(0x9274'6b9b'e2f8'552c), __SPRT_UINT64_C(0x32fd'3cf5'b4e4'9bb5)},
				{__SPRT_UINT64_C(0xb711'8682'dbb6'6a77), __SPRT_UINT64_C(0x3fbc'8c33'221d'c2a2)},
				{__SPRT_UINT64_C(0xe4d5'e823'92a4'0515), __SPRT_UINT64_C(0x0fab'af3f'eaa5'334b)},
				{__SPRT_UINT64_C(0x8f05'b116'3ba6'832d), __SPRT_UINT64_C(0x29cb'4d87'f2a7'400f)},
				{__SPRT_UINT64_C(0xb2c7'1d5b'ca90'23f8), __SPRT_UINT64_C(0x743e'20e9'ef51'1013)},
				{__SPRT_UINT64_C(0xdf78'e4b2'bd34'2cf6), __SPRT_UINT64_C(0x914d'a924'6b25'5417)},
				{__SPRT_UINT64_C(0x8bab'8eef'b640'9c1a), __SPRT_UINT64_C(0x1ad0'89b6'c2f7'548f)},
				{__SPRT_UINT64_C(0xae96'72ab'a3d0'c320), __SPRT_UINT64_C(0xa184'ac24'73b5'29b2)},
				{__SPRT_UINT64_C(0xda3c'0f56'8cc4'f3e8), __SPRT_UINT64_C(0xc9e5'd72d'90a2'741f)},
				{__SPRT_UINT64_C(0x8865'8996'17fb'1871), __SPRT_UINT64_C(0x7e2f'a67c'7a65'8893)},
				{__SPRT_UINT64_C(0xaa7e'ebfb'9df9'de8d), __SPRT_UINT64_C(0xddbb'901b'98fe'eab8)},
				{__SPRT_UINT64_C(0xd51e'a6fa'8578'5631), __SPRT_UINT64_C(0x552a'7422'7f3e'a566)},
				{__SPRT_UINT64_C(0x8533'285c'936b'35de), __SPRT_UINT64_C(0xd53a'8895'8f87'2760)},
				{__SPRT_UINT64_C(0xa67f'f273'b846'0356), __SPRT_UINT64_C(0x8a89'2aba'f368'f138)},
				{__SPRT_UINT64_C(0xd01f'ef10'a657'842c), __SPRT_UINT64_C(0x2d2b'7569'b043'2d86)},
				{__SPRT_UINT64_C(0x8213'f56a'67f6'b29b), __SPRT_UINT64_C(0x9c3b'2962'0e29'fc74)},
				{__SPRT_UINT64_C(0xa298'f2c5'01f4'5f42), __SPRT_UINT64_C(0x8349'f3ba'91b4'7b90)},
				{__SPRT_UINT64_C(0xcb3f'2f76'4271'7713), __SPRT_UINT64_C(0x241c'70a9'3621'9a74)},
				{__SPRT_UINT64_C(0xfe0e'fb53'd30d'd4d7), __SPRT_UINT64_C(0xed23'8cd3'83aa'0111)},
				{__SPRT_UINT64_C(0x9ec9'5d14'63e8'a506), __SPRT_UINT64_C(0xf436'3804'324a'40ab)},
				{__SPRT_UINT64_C(0xc67b'b459'7ce2'ce48), __SPRT_UINT64_C(0xb143'c605'3edc'd0d6)},
				{__SPRT_UINT64_C(0xf81a'a16f'dc1b'81da), __SPRT_UINT64_C(0xdd94'b786'8e94'050b)},
				{__SPRT_UINT64_C(0x9b10'a4e5'e991'3128), __SPRT_UINT64_C(0xca7c'f2b4'191c'8327)},
				{__SPRT_UINT64_C(0xc1d4'ce1f'63f5'7d72), __SPRT_UINT64_C(0xfd1c'2f61'1f63'a3f1)},
				{__SPRT_UINT64_C(0xf24a'01a7'3cf2'dccf), __SPRT_UINT64_C(0xbc63'3b39'673c'8ced)},
				{__SPRT_UINT64_C(0x976e'4108'8617'ca01), __SPRT_UINT64_C(0xd5be'0503'e085'd814)},
				{__SPRT_UINT64_C(0xbd49'd14a'a79d'bc82), __SPRT_UINT64_C(0x4b2d'8644'd8a7'4e19)},
				{__SPRT_UINT64_C(0xec9c'459d'5185'2ba2), __SPRT_UINT64_C(0xddf8'e7d6'0ed1'219f)},
				{__SPRT_UINT64_C(0x93e1'ab82'52f3'3b45), __SPRT_UINT64_C(0xcabb'90e5'c942'b504)},
				{__SPRT_UINT64_C(0xb8da'1662'e7b0'0a17), __SPRT_UINT64_C(0x3d6a'751f'3b93'6244)},
				{__SPRT_UINT64_C(0xe710'9bfb'a19c'0c9d), __SPRT_UINT64_C(0x0cc5'1267'0a78'3ad5)},
				{__SPRT_UINT64_C(0x906a'617d'4501'87e2), __SPRT_UINT64_C(0x27fb'2b80'668b'24c6)},
				{__SPRT_UINT64_C(0xb484'f9dc'9641'e9da), __SPRT_UINT64_C(0xb1f9'f660'802d'edf7)},
				{__SPRT_UINT64_C(0xe1a6'3853'bbd2'6451), __SPRT_UINT64_C(0x5e78'73f8'a039'6974)},
				{__SPRT_UINT64_C(0x8d07'e334'5563'7eb2), __SPRT_UINT64_C(0xdb0b'487b'6423'e1e9)},
				{__SPRT_UINT64_C(0xb049'dc01'6abc'5e5f), __SPRT_UINT64_C(0x91ce'1a9a'3d2c'da63)},
				{__SPRT_UINT64_C(0xdc5c'5301'c56b'75f7), __SPRT_UINT64_C(0x7641'a140'cc78'10fc)},
				{__SPRT_UINT64_C(0x89b9'b3e1'1b63'29ba), __SPRT_UINT64_C(0xa9e9'04c8'7fcb'0a9e)},
				{__SPRT_UINT64_C(0xac28'20d9'623b'f429), __SPRT_UINT64_C(0x5463'45fa'9fbd'cd45)},
				{__SPRT_UINT64_C(0xd732'290f'baca'f133), __SPRT_UINT64_C(0xa97c'1779'47ad'4096)},
				{__SPRT_UINT64_C(0x867f'59a9'd4be'd6c0), __SPRT_UINT64_C(0x49ed'8eab'cccc'485e)},
				{__SPRT_UINT64_C(0xa81f'3014'49ee'8c70), __SPRT_UINT64_C(0x5c68'f256'bfff'5a75)},
				{__SPRT_UINT64_C(0xd226'fc19'5c6a'2f8c), __SPRT_UINT64_C(0x7383'2eec'6fff'3112)},
				{__SPRT_UINT64_C(0x8358'5d8f'd9c2'5db7), __SPRT_UINT64_C(0xc831'fd53'c5ff'7eac)},
				{__SPRT_UINT64_C(0xa42e'74f3'd032'f525), __SPRT_UINT64_C(0xba3e'7ca8'b77f'5e56)},
				{__SPRT_UINT64_C(0xcd3a'1230'c43f'b26f), __SPRT_UINT64_C(0x28ce'1bd2'e55f'35ec)},
				{__SPRT_UINT64_C(0x8044'4b5e'7aa7'cf85), __SPRT_UINT64_C(0x7980'd163'cf5b'81b4)},
				{__SPRT_UINT64_C(0xa055'5e36'1951'c366), __SPRT_UINT64_C(0xd7e1'05bc'c332'6220)},
				{__SPRT_UINT64_C(0xc86a'b5c3'9fa6'3440), __SPRT_UINT64_C(0x8dd9'472b'f3fe'faa8)},
				{__SPRT_UINT64_C(0xfa85'6334'878f'c150), __SPRT_UINT64_C(0xb14f'98f6'f0fe'b952)},
				{__SPRT_UINT64_C(0x9c93'5e00'd4b9'd8d2), __SPRT_UINT64_C(0x6ed1'bf9a'569f'33d4)},
				{__SPRT_UINT64_C(0xc3b8'3581'09e8'4f07), __SPRT_UINT64_C(0x0a86'2f80'ec47'00c9)},
				{__SPRT_UINT64_C(0xf4a6'42e1'4c62'62c8), __SPRT_UINT64_C(0xcd27'bb61'2758'c0fb)},
				{__SPRT_UINT64_C(0x98e7'e9cc'cfbd'7dbd), __SPRT_UINT64_C(0x8038'd51c'b897'789d)},
				{__SPRT_UINT64_C(0xbf21'e440'03ac'dd2c), __SPRT_UINT64_C(0xe047'0a63'e6bd'56c4)},
				{__SPRT_UINT64_C(0xeeea'5d50'0498'1478), __SPRT_UINT64_C(0x1858'ccfc'e06c'ac75)},
				{__SPRT_UINT64_C(0x9552'7a52'02df'0ccb), __SPRT_UINT64_C(0x0f37'801e'0c43'ebc9)},
				{__SPRT_UINT64_C(0xbaa7'18e6'8396'cffd), __SPRT_UINT64_C(0xd305'6025'8f54'e6bb)},
				{__SPRT_UINT64_C(0xe950'df20'247c'83fd), __SPRT_UINT64_C(0x47c6'b82e'f32a'206a)},
				{__SPRT_UINT64_C(0x91d2'8b74'16cd'd27e), __SPRT_UINT64_C(0x4cdc'331d'57fa'5442)},
				{__SPRT_UINT64_C(0xb647'2e51'1c81'471d), __SPRT_UINT64_C(0xe013'3fe4'adf8'e953)},
				{__SPRT_UINT64_C(0xe3d8'f9e5'63a1'98e5), __SPRT_UINT64_C(0x5818'0fdd'd977'23a7)},
				{__SPRT_UINT64_C(0x8e67'9c2f'5e44'ff8f), __SPRT_UINT64_C(0x570f'09ea'a7ea'7649)},
				{__SPRT_UINT64_C(0xb201'833b'35d6'3f73), __SPRT_UINT64_C(0x2cd2'cc65'51e5'13db)},
				{__SPRT_UINT64_C(0xde81'e40a'034b'cf4f), __SPRT_UINT64_C(0xf807'7f7e'a65e'58d2)},
				{__SPRT_UINT64_C(0x8b11'2e86'420f'6191), __SPRT_UINT64_C(0xfb04'afaf'27fa'f783)},
				{__SPRT_UINT64_C(0xadd5'7a27'd293'39f6), __SPRT_UINT64_C(0x79c5'db9a'f1f9'b564)},
				{__SPRT_UINT64_C(0xd94a'd8b1'c738'0874), __SPRT_UINT64_C(0x1837'5281'ae78'22bd)},
				{__SPRT_UINT64_C(0x87ce'c76f'1c83'0548), __SPRT_UINT64_C(0x8f22'9391'0d0b'15b6)},
				{__SPRT_UINT64_C(0xa9c2'794a'e3a3'c69a), __SPRT_UINT64_C(0xb2eb'3875'504d'db23)},
				{__SPRT_UINT64_C(0xd433'179d'9c8c'b841), __SPRT_UINT64_C(0x5fa6'0692'a461'51ec)},
				{__SPRT_UINT64_C(0x849f'eec2'81d7'f328), __SPRT_UINT64_C(0xdbc7'c41b'a6bc'd334)},
				{__SPRT_UINT64_C(0xa5c7'ea73'224d'eff3), __SPRT_UINT64_C(0x12b9'b522'906c'0801)},
				{__SPRT_UINT64_C(0xcf39'e50f'eae1'6bef), __SPRT_UINT64_C(0xd768'226b'3487'0a01)},
				{__SPRT_UINT64_C(0x8184'2f29'f2cc'e375), __SPRT_UINT64_C(0xe6a1'1583'00d4'6641)},
				{__SPRT_UINT64_C(0xa1e5'3af4'6f80'1c53), __SPRT_UINT64_C(0x6049'5ae3'c109'7fd1)},
				{__SPRT_UINT64_C(0xca5e'89b1'8b60'2368), __SPRT_UINT64_C(0x385b'b19c'b14b'dfc5)},
				{__SPRT_UINT64_C(0xfcf6'2c1d'ee38'2c42), __SPRT_UINT64_C(0x4672'9e03'dd9e'd7b6)},
				{__SPRT_UINT64_C(0x9e19'db92'b4e3'1ba9), __SPRT_UINT64_C(0x6c07'a2c2'6a83'46d2)},
				{__SPRT_UINT64_C(0xc5a0'5277'621b'e293), __SPRT_UINT64_C(0xc709'8b73'0524'1886)},
				{__SPRT_UINT64_C(0xf708'6715'3aa2'db38), __SPRT_UINT64_C(0xb8cb'ee4f'c66d'1ea8)}}};
};
#if !JKJ_HAS_INLINE_VARIABLE
// decltype(...) should not depend on Dummy; see
// https://stackoverflow.com/questions/76438400/decltype-on-static-variable-in-template-class.
template <class Dummy>
constexpr decltype(cache_holder<ieee754_binary64>::cache)
		cache_holder<ieee754_binary64, Dummy>::cache;
#endif

// Compressed cache.
template <class FloatFormat, class Dummy = void>
struct compressed_cache_holder {
	using cache_entry_type = typename cache_holder<FloatFormat>::cache_entry_type;
	static constexpr int cache_bits = cache_holder<FloatFormat>::cache_bits;
	static constexpr int min_k = cache_holder<FloatFormat>::min_k;
	static constexpr int max_k = cache_holder<FloatFormat>::max_k;

	template <class ShiftAmountType, class DecimalExponentType>
	static JKJ_CONSTEXPR20 cache_entry_type get_cache(DecimalExponentType k) noexcept {
		return detail::read_static_data(cache_holder<FloatFormat>::cache.data() + k - min_k);
	}
};

template <class Dummy>
struct compressed_cache_holder<ieee754_binary32, Dummy> {
	using cache_entry_type = cache_holder<ieee754_binary32>::cache_entry_type;
	static constexpr int cache_bits = cache_holder<ieee754_binary32>::cache_bits;
	static constexpr int min_k = cache_holder<ieee754_binary32>::min_k;
	static constexpr int max_k = cache_holder<ieee754_binary32>::max_k;
	static constexpr int compression_ratio = 13;
	static constexpr detail::stdr::size_t compressed_table_size =
			detail::stdr::size_t((max_k - min_k + compression_ratio) / compression_ratio);
	static constexpr detail::stdr::size_t pow5_table_size =
			detail::stdr::size_t((compression_ratio + 1) / 2);

	using cache_holder_t = detail::array<cache_entry_type, compressed_table_size>;
	using pow5_holder_t = detail::array<detail::stdr::uint_least16_t, pow5_table_size>;

#if JKJ_HAS_CONSTEXPR17
	static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION = [] {
		cache_holder_t res{};
		for (detail::stdr::size_t i = 0; i < compressed_table_size; ++i) {
			res[i] = cache_holder<ieee754_binary32>::cache[i * compression_ratio];
		}
		return res;
	}();
	static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION = [] {
		pow5_holder_t res{};
		detail::stdr::uint_least16_t p = 1;
		for (detail::stdr::size_t i = 0; i < pow5_table_size; ++i) {
			res[i] = p;
			p *= 5;
		}
		return res;
	}();
#else
	template <detail::stdr::size_t... indices>
	static constexpr cache_holder_t make_cache(detail::index_sequence<indices...>) {
		return {cache_holder<ieee754_binary32>::cache[indices * compression_ratio]...};
	}
	static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION =
			make_cache(detail::make_index_sequence<compressed_table_size>{});

	template <detail::stdr::size_t... indices>
	static constexpr pow5_holder_t make_pow5_table(detail::index_sequence<indices...>) {
		return {detail::compute_power<indices>(detail::stdr::uint_least16_t(5))...};
	}
	static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION =
			make_pow5_table(detail::make_index_sequence<pow5_table_size>{});
#endif

	template <class ShiftAmountType, class DecimalExponentType>
	static JKJ_CONSTEXPR20 cache_entry_type get_cache(DecimalExponentType k) noexcept {
		// Compute the base index.
		// Supposed to compute (k - min_k) / compression_ratio.
		// Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
		static_assert(max_k - min_k <= 89 && compression_ratio == 13, "");
		static_assert(max_k - min_k <= sprt::Max<DecimalExponentType>, "");
		using unsigned_exponent_type =
				typename detail::stdr::make_unsigned<DecimalExponentType>::type;
		auto const cache_index =
				DecimalExponentType(detail::stdr::uint_fast16_t(unsigned_exponent_type(k - min_k)
											* detail::stdr::uint_fast16_t(79))
						>> 10);
		auto const kb = DecimalExponentType(cache_index * compression_ratio + min_k);
		auto const offset = DecimalExponentType(k - kb);

		// Get the base cache.
		auto const base_cache = detail::read_static_data(cache.data() + cache_index);

		if (offset == 0) {
			return base_cache;
		} else {
			// Compute the required amount of bit-shift.
			auto const alpha = ShiftAmountType(detail::log::floor_log2_pow5<min_k, max_k>(k)
					- detail::log::floor_log2_pow5<min_k, max_k>(kb));
			__sprt_assert(alpha > 0 && alpha < 32);

			// Try to recover the real cache.
			constexpr auto pow5_6 = detail::stdr::uint_fast32_t(pow5_table[6]);
			auto const pow5 = offset >= 7
					? detail::stdr::uint_fast32_t(
							  pow5_6 * detail::read_static_data(pow5_table.data() + offset - 6))
					: detail::stdr::uint_fast32_t(
							  detail::read_static_data(pow5_table.data() + offset));
			auto mul_result = detail::wuint::umul96(pow5, base_cache);

			auto const recovered_cache =
#if JKJ_FAST_MUL64
					cache_entry_type((((detail::stdr::uint_least64_t(mul_result.high32())
											   << ShiftAmountType(64 - alpha))
											  | (mul_result.low64() >> alpha))
											 + 1)
							& __SPRT_UINT64_C(0xffff'ffff'ffff'ffff));
#else
					cache_entry_type((((mul_result.high64() << ShiftAmountType(32 - alpha))
											  | (mul_result.low32() >> alpha))
											 + 1)
							& UINT64_C(0xffff'ffff'ffff'ffff));
#endif
			__sprt_assert(recovered_cache != 0);

			return recovered_cache;
		}
	}
};
#if !JKJ_HAS_INLINE_VARIABLE
template <class Dummy>
constexpr typename compressed_cache_holder<ieee754_binary32, Dummy>::cache_holder_t
		compressed_cache_holder<ieee754_binary32, Dummy>::cache;
template <class Dummy>
constexpr typename compressed_cache_holder<ieee754_binary32, Dummy>::pow5_holder_t
		compressed_cache_holder<ieee754_binary32, Dummy>::pow5_table;
#endif

template <class Dummy>
struct compressed_cache_holder<ieee754_binary64, Dummy> {
	using cache_entry_type = cache_holder<ieee754_binary64>::cache_entry_type;
	static constexpr int cache_bits = cache_holder<ieee754_binary64>::cache_bits;
	static constexpr int min_k = cache_holder<ieee754_binary64>::min_k;
	static constexpr int max_k = cache_holder<ieee754_binary64>::max_k;
	static constexpr int compression_ratio = 27;
	static constexpr detail::stdr::size_t compressed_table_size =
			detail::stdr::size_t((max_k - min_k + compression_ratio) / compression_ratio);
	static constexpr detail::stdr::size_t pow5_table_size = detail::stdr::size_t(compression_ratio);

	using cache_holder_t = detail::array<cache_entry_type, compressed_table_size>;
	using pow5_holder_t = detail::array<detail::stdr::uint_least64_t, pow5_table_size>;

#if JKJ_HAS_CONSTEXPR17
	static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION = [] {
		cache_holder_t res{};
		for (detail::stdr::size_t i = 0; i < compressed_table_size; ++i) {
			res[i] = cache_holder<ieee754_binary64>::cache[i * compression_ratio];
		}
		return res;
	}();
	static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION = [] {
		pow5_holder_t res{};
		detail::stdr::uint_least64_t p = 1;
		for (detail::stdr::size_t i = 0; i < pow5_table_size; ++i) {
			res[i] = p;
			p *= 5;
		}
		return res;
	}();
#else
	template <detail::stdr::size_t... indices>
	static constexpr cache_holder_t make_cache(detail::index_sequence<indices...>) {
		return {cache_holder<ieee754_binary64>::cache[indices * compression_ratio]...};
	}
	static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION =
			make_cache(detail::make_index_sequence<compressed_table_size>{});

	template <detail::stdr::size_t... indices>
	static constexpr pow5_holder_t make_pow5_table(detail::index_sequence<indices...>) {
		return {detail::compute_power<indices>(detail::stdr::uint_least64_t(5))...};
	}
	static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION =
			make_pow5_table(detail::make_index_sequence<pow5_table_size>{});
#endif

	template <class ShiftAmountType, class DecimalExponentType>
	static JKJ_CONSTEXPR20 cache_entry_type get_cache(DecimalExponentType k) noexcept {
		// Compute the base index.
		// Supposed to compute (k - min_k) / compression_ratio.
		// Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
		static_assert(max_k - min_k <= 619 && compression_ratio == 27, "");
		static_assert(max_k - min_k <= sprt::Max<DecimalExponentType>, "");
		using unsigned_exponent_type =
				typename detail::stdr::make_unsigned<DecimalExponentType>::type;
		auto const cache_index =
				DecimalExponentType(detail::stdr::uint_fast32_t(unsigned_exponent_type(k - min_k)
											* detail::stdr::uint_fast32_t(607))
						>> 14);
		auto const kb = DecimalExponentType(cache_index * compression_ratio + min_k);
		auto const offset = DecimalExponentType(k - kb);

		// Get the base cache.
		auto const base_cache = detail::read_static_data(cache.data() + cache_index);

		if (offset == 0) {
			return base_cache;
		} else {
			// Compute the required amount of bit-shift.
			auto const alpha = ShiftAmountType(detail::log::floor_log2_pow5<min_k, max_k>(k)
					- detail::log::floor_log2_pow5<min_k, max_k>(kb));
			__sprt_assert(alpha > 0 && alpha < 64);

			// Try to recover the real cache.
			auto const pow5 = detail::read_static_data(pow5_table.data() + offset);
			auto recovered_cache = detail::wuint::umul128(base_cache.high(), pow5);
			auto const middle_low = detail::wuint::umul128(base_cache.low(), pow5);

			recovered_cache += middle_low.high();

			auto const high_to_middle = detail::stdr::uint_least64_t(
					(recovered_cache.high() << ShiftAmountType(64 - alpha))
					& __SPRT_UINT64_C(0xffff'ffff'ffff'ffff));
			auto const middle_to_low = detail::stdr::uint_least64_t(
					(recovered_cache.low() << ShiftAmountType(64 - alpha))
					& __SPRT_UINT64_C(0xffff'ffff'ffff'ffff));

			recovered_cache = {(recovered_cache.low() >> alpha) | high_to_middle,
				((middle_low.low() >> alpha) | middle_to_low)};

			__sprt_assert(recovered_cache.low() != __SPRT_UINT64_C(0xffff'ffff'ffff'ffff));
			recovered_cache = {recovered_cache.high(),
				detail::stdr::uint_least64_t(recovered_cache.low() + 1)};

			return recovered_cache;
		}
	}
};
#if !JKJ_HAS_INLINE_VARIABLE
template <class Dummy>
constexpr typename compressed_cache_holder<ieee754_binary64, Dummy>::cache_holder_t
		compressed_cache_holder<ieee754_binary64, Dummy>::cache;
template <class Dummy>
constexpr typename compressed_cache_holder<ieee754_binary64, Dummy>::pow5_holder_t
		compressed_cache_holder<ieee754_binary64, Dummy>::pow5_table;
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations of user-specializable templates used in the main algorithm.
////////////////////////////////////////////////////////////////////////////////////////

// Remove trailing zeros from significand and add the number of removed zeros into
// exponent.
template <class TrailingZeroPolicy, class Format, class DecimalSignificand,
		class DecimalExponentType>
struct remove_trailing_zeros_traits;

// Users can specialize this traits class to make Dragonbox work with their own formats.
// However, this requires detailed knowledge on how the algorithm works, so it is recommended to
// read through the paper.
template <class FormatTraits, class CacheEntryType, detail::stdr::size_t cache_bits_>
struct multiplication_traits;

// A collection of some common definitions to reduce boilerplate.
template <class FormatTraits, class CacheEntryType, detail::stdr::size_t cache_bits_>
struct multiplication_traits_base {
	using format = typename FormatTraits::format;
	static constexpr int significand_bits = format::significand_bits;
	static constexpr int total_bits = format::total_bits;
	using carrier_uint = typename FormatTraits::carrier_uint;
	using cache_entry_type = CacheEntryType;
	static constexpr int cache_bits = int(cache_bits_);

	struct compute_mul_result {
		carrier_uint integer_part;
		bool is_integer;
	};
	struct compute_mul_parity_result {
		bool parity;
		bool is_integer;
	};
};


////////////////////////////////////////////////////////////////////////////////////////
// Policies.
////////////////////////////////////////////////////////////////////////////////////////

namespace detail {
template <class T>
struct dummy { };
} // namespace detail

namespace policy {
namespace sign {
JKJ_INLINE_VARIABLE struct ignore_t {
	using sign_policy = ignore_t;
	static constexpr bool return_has_sign = false;

#if defined(_MSC_VER) && !defined(__clang__)
	// See
	// https://developercommunity.visualstudio.com/t/Failure-to-optimize-intrinsics/10628226
	template <class SignedSignificandBits, class DecimalSignificand, class DecimalExponentType>
	static constexpr decimal_fp<DecimalSignificand, DecimalExponentType, false, false> handle_sign(
			SignedSignificandBits,
			decimal_fp<DecimalSignificand, DecimalExponentType, false, false> r) noexcept {
		return {r.significand, r.exponent};
	}
	template <class SignedSignificandBits, class DecimalSignificand, class DecimalExponentType>
	static constexpr decimal_fp<DecimalSignificand, DecimalExponentType, false, true> handle_sign(
			SignedSignificandBits,
			decimal_fp<DecimalSignificand, DecimalExponentType, false, true> r) noexcept {
		return {r.significand, r.exponent, r.may_have_trailing_zeros};
	}
#else
	template <class SignedSignificandBits, class UnsignedDecimalFp>
	static constexpr UnsignedDecimalFp handle_sign(SignedSignificandBits,
			UnsignedDecimalFp r) noexcept {
		return r;
	}
#endif
} ignore = {};

JKJ_INLINE_VARIABLE struct return_sign_t {
	using sign_policy = return_sign_t;
	static constexpr bool return_has_sign = true;

	template <class SignedSignificandBits, class UnsignedDecimalFp>
	static constexpr detail::unsigned_decimal_fp_to_signed_t<UnsignedDecimalFp> handle_sign(
			SignedSignificandBits s, UnsignedDecimalFp r) noexcept {
		return add_sign_to_unsigned_decimal_fp(s.is_negative(), r);
	}
} return_sign = {};
} // namespace sign

namespace trailing_zero {
JKJ_INLINE_VARIABLE struct ignore_t {
	using trailing_zero_policy = ignore_t;
	static constexpr bool report_trailing_zeros = false;

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
	on_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent};
	}

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
	no_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent};
	}
} ignore = {};

JKJ_INLINE_VARIABLE struct remove_t {
	using trailing_zero_policy = remove_t;
	static constexpr bool report_trailing_zeros = false;

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14
			unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
			on_trailing_zeros(DecimalSignificand significand,
					DecimalExponentType exponent) noexcept {
		remove_trailing_zeros_traits< remove_t, Format, DecimalSignificand,
				DecimalExponentType>::remove_trailing_zeros(significand, exponent);
		return {significand, exponent};
	}

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
	no_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent};
	}
} remove = {};

JKJ_INLINE_VARIABLE struct remove_compact_t {
	using trailing_zero_policy = remove_compact_t;
	static constexpr bool report_trailing_zeros = false;

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14
			unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
			on_trailing_zeros(DecimalSignificand significand,
					DecimalExponentType exponent) noexcept {
		remove_trailing_zeros_traits< remove_compact_t, Format, DecimalSignificand,
				DecimalExponentType>::remove_trailing_zeros(significand, exponent);
		return {significand, exponent};
	}

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
	no_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent};
	}
} remove_compact = {};

JKJ_INLINE_VARIABLE struct report_t {
	using trailing_zero_policy = report_t;
	static constexpr bool report_trailing_zeros = true;

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, true>
	on_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent, true};
	}

	template <class Format, class DecimalSignificand, class DecimalExponentType>
	static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, true>
	no_trailing_zeros(DecimalSignificand significand, DecimalExponentType exponent) noexcept {
		return {significand, exponent, false};
	}
} report = {};
} // namespace trailing_zero

namespace decimal_to_binary_rounding {
enum class tag_t {
	to_nearest,
	left_closed_directed,
	right_closed_directed
};

namespace interval_type {
struct symmetric_boundary {
	static constexpr bool is_symmetric = true;
	bool is_closed;
	constexpr bool include_left_endpoint() const noexcept { return is_closed; }
	constexpr bool include_right_endpoint() const noexcept { return is_closed; }
};
struct asymmetric_boundary {
	static constexpr bool is_symmetric = false;
	bool is_left_closed;
	constexpr bool include_left_endpoint() const noexcept { return is_left_closed; }
	constexpr bool include_right_endpoint() const noexcept { return !is_left_closed; }
};
struct closed {
	static constexpr bool is_symmetric = true;
	static constexpr bool include_left_endpoint() noexcept { return true; }
	static constexpr bool include_right_endpoint() noexcept { return true; }
};
struct open {
	static constexpr bool is_symmetric = true;
	static constexpr bool include_left_endpoint() noexcept { return false; }
	static constexpr bool include_right_endpoint() noexcept { return false; }
};
struct left_closed_right_open {
	static constexpr bool is_symmetric = false;
	static constexpr bool include_left_endpoint() noexcept { return true; }
	static constexpr bool include_right_endpoint() noexcept { return false; }
};
struct right_closed_left_open {
	static constexpr bool is_symmetric = false;
	static constexpr bool include_left_endpoint() noexcept { return false; }
	static constexpr bool include_right_endpoint() noexcept { return true; }
};
} // namespace interval_type

JKJ_INLINE_VARIABLE struct nearest_to_even_t {
	using decimal_to_binary_rounding_policy = nearest_to_even_t;
	using interval_type_provider = nearest_to_even_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_to_even_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_to_even_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::symmetric_boundary normal_interval(
			SignedSignificandBits s) noexcept {
		return {s.has_even_significand_bits()};
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::closed shorter_interval(SignedSignificandBits) noexcept {
		return {};
	}
} nearest_to_even = {};

JKJ_INLINE_VARIABLE struct nearest_to_odd_t {
	using decimal_to_binary_rounding_policy = nearest_to_odd_t;
	using interval_type_provider = nearest_to_odd_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_to_odd_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_to_odd_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::symmetric_boundary normal_interval(
			SignedSignificandBits s) noexcept {
		return {!s.has_even_significand_bits()};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::open shorter_interval(SignedSignificandBits) noexcept {
		return {};
	}
} nearest_to_odd = {};

JKJ_INLINE_VARIABLE struct nearest_toward_plus_infinity_t {
	using decimal_to_binary_rounding_policy = nearest_toward_plus_infinity_t;
	using interval_type_provider = nearest_toward_plus_infinity_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_toward_plus_infinity_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_toward_plus_infinity_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::asymmetric_boundary normal_interval(
			SignedSignificandBits s) noexcept {
		return {!s.is_negative()};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::asymmetric_boundary shorter_interval(
			SignedSignificandBits s) noexcept {
		return {!s.is_negative()};
	}
} nearest_toward_plus_infinity = {};

JKJ_INLINE_VARIABLE struct nearest_toward_minus_infinity_t {
	using decimal_to_binary_rounding_policy = nearest_toward_minus_infinity_t;
	using interval_type_provider = nearest_toward_minus_infinity_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_toward_minus_infinity_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_toward_minus_infinity_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::asymmetric_boundary normal_interval(
			SignedSignificandBits s) noexcept {
		return {s.is_negative()};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::asymmetric_boundary shorter_interval(
			SignedSignificandBits s) noexcept {
		return {s.is_negative()};
	}
} nearest_toward_minus_infinity = {};

JKJ_INLINE_VARIABLE struct nearest_toward_zero_t {
	using decimal_to_binary_rounding_policy = nearest_toward_zero_t;
	using interval_type_provider = nearest_toward_zero_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_toward_zero_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_toward_zero_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::right_closed_left_open normal_interval(
			SignedSignificandBits) noexcept {
		return {};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::right_closed_left_open shorter_interval(
			SignedSignificandBits) noexcept {
		return {};
	}
} nearest_toward_zero = {};

JKJ_INLINE_VARIABLE struct nearest_away_from_zero_t {
	using decimal_to_binary_rounding_policy = nearest_away_from_zero_t;
	using interval_type_provider = nearest_away_from_zero_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			dragonbox::detail::declval<nearest_away_from_zero_t>(),
			Args{}...)) delegate(SignedSignificandBits, Func f, Args... args) noexcept {
		return f(nearest_away_from_zero_t{}, args...);
	}

	template <class SignedSignificandBits>
	static constexpr interval_type::left_closed_right_open normal_interval(
			SignedSignificandBits) noexcept {
		return {};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::left_closed_right_open shorter_interval(
			SignedSignificandBits) noexcept {
		return {};
	}
} nearest_away_from_zero = {};

namespace detail {
struct nearest_always_closed_t {
	using interval_type_provider = nearest_always_closed_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits>
	static constexpr interval_type::closed normal_interval(SignedSignificandBits) noexcept {
		return {};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::closed shorter_interval(SignedSignificandBits) noexcept {
		return {};
	}
};
struct nearest_always_open_t {
	using interval_type_provider = nearest_always_open_t;
	static constexpr auto tag = tag_t::to_nearest;

	template <class SignedSignificandBits>
	static constexpr interval_type::open normal_interval(SignedSignificandBits) noexcept {
		return {};
	}
	template <class SignedSignificandBits>
	static constexpr interval_type::open shorter_interval(SignedSignificandBits) noexcept {
		return {};
	}
};
} // namespace detail

JKJ_INLINE_VARIABLE struct nearest_to_even_static_boundary_t {
	using decimal_to_binary_rounding_policy = nearest_to_even_static_boundary_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::nearest_always_closed_t{}, Args{}...)) delegate(SignedSignificandBits s, Func f,
			Args... args) noexcept {
		return s.has_even_significand_bits() ? f(detail::nearest_always_closed_t{}, args...)
											 : f(detail::nearest_always_open_t{}, args...);
	}
} nearest_to_even_static_boundary = {};

JKJ_INLINE_VARIABLE struct nearest_to_odd_static_boundary_t {
	using decimal_to_binary_rounding_policy = nearest_to_odd_static_boundary_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::nearest_always_closed_t{}, Args{}...)) delegate(SignedSignificandBits s, Func f,
			Args... args) noexcept {
		return s.has_even_significand_bits() ? f(detail::nearest_always_open_t{}, args...)
											 : f(detail::nearest_always_closed_t{}, args...);
	}
} nearest_to_odd_static_boundary = {};

JKJ_INLINE_VARIABLE struct nearest_toward_plus_infinity_static_boundary_t {
	using decimal_to_binary_rounding_policy = nearest_toward_plus_infinity_static_boundary_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(nearest_toward_zero,
			Args{}...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
		return s.is_negative() ? f(nearest_toward_zero, args...)
							   : f(nearest_away_from_zero, args...);
	}
} nearest_toward_plus_infinity_static_boundary = {};

JKJ_INLINE_VARIABLE struct nearest_toward_minus_infinity_static_boundary_t {
	using decimal_to_binary_rounding_policy = nearest_toward_minus_infinity_static_boundary_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(nearest_toward_zero,
			Args{}...)) delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
		return s.is_negative() ? f(nearest_away_from_zero, args...)
							   : f(nearest_toward_zero, args...);
	}
} nearest_toward_minus_infinity_static_boundary = {};

namespace detail {
struct left_closed_directed_t {
	static constexpr auto tag = tag_t::left_closed_directed;
};
struct right_closed_directed_t {
	static constexpr auto tag = tag_t::right_closed_directed;
};
} // namespace detail

JKJ_INLINE_VARIABLE struct toward_plus_infinity_t {
	using decimal_to_binary_rounding_policy = toward_plus_infinity_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::left_closed_directed_t{}, Args{}...)) delegate(SignedSignificandBits s, Func f,
			Args... args) noexcept {
		return s.is_negative() ? f(detail::left_closed_directed_t{}, args...)
							   : f(detail::right_closed_directed_t{}, args...);
	}
} toward_plus_infinity = {};

JKJ_INLINE_VARIABLE struct toward_minus_infinity_t {
	using decimal_to_binary_rounding_policy = toward_minus_infinity_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::left_closed_directed_t{}, Args{}...)) delegate(SignedSignificandBits s, Func f,
			Args... args) noexcept {
		return s.is_negative() ? f(detail::right_closed_directed_t{}, args...)
							   : f(detail::left_closed_directed_t{}, args...);
	}
} toward_minus_infinity = {};

JKJ_INLINE_VARIABLE struct toward_zero_t {
	using decimal_to_binary_rounding_policy = toward_zero_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::left_closed_directed_t{}, Args{}...)) delegate(SignedSignificandBits, Func f,
			Args... args) noexcept {
		return f(detail::left_closed_directed_t{}, args...);
	}
} toward_zero = {};

JKJ_INLINE_VARIABLE struct away_from_zero_t {
	using decimal_to_binary_rounding_policy = away_from_zero_t;

	template <class SignedSignificandBits, class Func, class... Args>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
			detail::right_closed_directed_t{}, Args{}...)) delegate(SignedSignificandBits, Func f,
			Args... args) noexcept {
		return f(detail::right_closed_directed_t{}, args...);
	}
} away_from_zero = {};
} // namespace decimal_to_binary_rounding

namespace binary_to_decimal_rounding {
// (Always assumes nearest rounding modes, as there can be no tie for other rounding
// modes.)
enum class tag_t {
	do_not_care,
	to_even,
	to_odd,
	away_from_zero,
	toward_zero
};

// The parameter significand corresponds to 10\tilde{s}+t in the paper.

JKJ_INLINE_VARIABLE struct do_not_care_t {
	using binary_to_decimal_rounding_policy = do_not_care_t;
	static constexpr auto tag = tag_t::do_not_care;

	template <class CarrierUInt>
	static constexpr bool prefer_round_down(CarrierUInt) noexcept {
		return false;
	}
} do_not_care = {};

JKJ_INLINE_VARIABLE struct to_even_t {
	using binary_to_decimal_rounding_policy = to_even_t;
	static constexpr auto tag = tag_t::to_even;

	template <class CarrierUInt>
	static constexpr bool prefer_round_down(CarrierUInt significand) noexcept {
		return significand % 2 != 0;
	}
} to_even = {};

JKJ_INLINE_VARIABLE struct to_odd_t {
	using binary_to_decimal_rounding_policy = to_odd_t;
	static constexpr auto tag = tag_t::to_odd;

	template <class CarrierUInt>
	static constexpr bool prefer_round_down(CarrierUInt significand) noexcept {
		return significand % 2 == 0;
	}
} to_odd = {};

JKJ_INLINE_VARIABLE struct away_from_zero_t {
	using binary_to_decimal_rounding_policy = away_from_zero_t;
	static constexpr auto tag = tag_t::away_from_zero;

	template <class CarrierUInt>
	static constexpr bool prefer_round_down(CarrierUInt) noexcept {
		return false;
	}
} away_from_zero = {};

JKJ_INLINE_VARIABLE struct toward_zero_t {
	using binary_to_decimal_rounding_policy = toward_zero_t;
	static constexpr auto tag = tag_t::toward_zero;

	template <class CarrierUInt>
	static constexpr bool prefer_round_down(CarrierUInt) noexcept {
		return true;
	}
} toward_zero = {};
} // namespace binary_to_decimal_rounding

namespace cache {
JKJ_INLINE_VARIABLE struct full_t {
	using cache_policy = full_t;
	template <class FloatFormat>
	using cache_holder_type = cache_holder<FloatFormat>;

	template <class FloatFormat, class ShiftAmountType, class DecimalExponentType>
	static JKJ_CONSTEXPR20 typename cache_holder_type<FloatFormat>::cache_entry_type get_cache(
			DecimalExponentType k) noexcept {
		assert(k >= cache_holder_type<FloatFormat>::min_k
				&& k <= cache_holder_type<FloatFormat>::max_k);

		return detail::read_static_data(cache_holder_type<FloatFormat>::cache.data()
				+ detail::stdr::size_t(k - cache_holder_type<FloatFormat>::min_k));
	}
} full = {};

JKJ_INLINE_VARIABLE struct compact_t {
	using cache_policy = compact_t;
	template <class FloatFormat>
	using cache_holder_type = compressed_cache_holder<FloatFormat>;

	template <class FloatFormat, class ShiftAmountType, class DecimalExponentType>
	static JKJ_CONSTEXPR20 typename cache_holder<FloatFormat>::cache_entry_type get_cache(
			DecimalExponentType k) noexcept {
		__sprt_assert(
				k >= cache_holder<FloatFormat>::min_k && k <= cache_holder<FloatFormat>::max_k);

		return cache_holder_type<FloatFormat>::template get_cache<ShiftAmountType>(k);
	}
} compact = {};
} // namespace cache

namespace preferred_integer_types {
JKJ_INLINE_VARIABLE struct match_t {
	using preferred_integer_types_policy = match_t;

	template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
	using remainder_type = typename FormatTraits::carrier_uint;

	template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
			detail::stdr::uint_least32_t upper_bound>
	using decimal_exponent_type = typename FormatTraits::exponent_int;

	template <class FormatTraits>
	using shift_amount_type = typename FormatTraits::exponent_int;
} match;

JKJ_INLINE_VARIABLE struct prefer_32_t {
	using preferred_integer_types_policy = prefer_32_t;

	// Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
	template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
	using remainder_type = typename detail::stdr::conditional< upper_bound
					<= sprt::Max<detail::stdr::uint_least32_t>,
			detail::stdr::uint_least32_t, typename FormatTraits::carrier_uint>::type;

	template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
			detail::stdr::uint_least32_t upper_bound>
	using decimal_exponent_type =
			typename detail::stdr::conditional< FormatTraits::format::exponent_bits
							<= detail::value_bits<detail::stdr::int_least32_t>::value,
					detail::stdr::int_least32_t, typename FormatTraits::exponent_int>::type;

	template <class FormatTraits>
	using shift_amount_type = detail::stdr::int_least32_t;
} prefer_32;

JKJ_INLINE_VARIABLE struct minimal_t {
	using preferred_integer_types_policy = minimal_t;

	// Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
	template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
	using remainder_type = typename detail::stdr::conditional<
			upper_bound <= sprt::Max<detail::stdr::uint_least8_t>, detail::stdr::uint_least8_t,
			typename detail::stdr::conditional< upper_bound
							<= sprt::Max<detail::stdr::uint_least16_t>,
					detail::stdr::uint_least16_t,
					typename detail::stdr::conditional< upper_bound
									<= sprt::Max<detail::stdr::uint_least32_t>,
							detail::stdr::uint_least32_t,
							typename detail::stdr::conditional< upper_bound
											<= sprt::Max<detail::stdr::uint_least64_t>,
									detail::stdr::uint_least64_t,
									typename FormatTraits::carrier_uint>::type>::type>::type>::type;

	// Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
	template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
			detail::stdr::uint_least32_t upper_bound>
	using decimal_exponent_type = typename detail::stdr::conditional< lower_bound
							>= sprt::Min<detail::stdr::uint_least8_t>
					&& upper_bound <= sprt::Max<detail::stdr::uint_least8_t>,
			detail::stdr::int_least8_t,
			typename detail::stdr::conditional< lower_bound
									>= sprt::Min<detail::stdr::uint_least16_t>
							&& upper_bound <= sprt::Max<detail::stdr::uint_least16_t>,
					detail::stdr::int_least16_t,
					typename detail::stdr::conditional< lower_bound
											>= sprt::Min<detail::stdr::uint_least32_t>
									&& upper_bound <= sprt::Max<detail::stdr::uint_least32_t>,
							detail::stdr::int_least32_t,
							typename FormatTraits::exponent_int>::type>::type>::type;

	template <class FormatTraits>
	using shift_amount_type = detail::stdr::int_least8_t;
} minimal;
} // namespace preferred_integer_types
} // namespace policy

////////////////////////////////////////////////////////////////////////////////////////
// Specializations of user-specializable templates used in the main algorithm.
////////////////////////////////////////////////////////////////////////////////////////

template <class DecimalExponentType>
struct remove_trailing_zeros_traits<policy::trailing_zero::remove_t, ieee754_binary32,
		detail::stdr::uint_least32_t, DecimalExponentType> {
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void remove_trailing_zeros(
			detail::stdr::uint_least32_t &significand, DecimalExponentType &exponent) noexcept {
		// See https://github.com/jk-jeon/rtz_benchmark.
		// The idea of branchless search below is by reddit users r/pigeon768 and
		// r/TheoreticalDumbass.

		auto r = detail::bits::rotr<32>(
				detail::stdr::uint_least32_t(significand * __SPRT_UINT32_C(184'254'097)), 4);
		auto b = r < __SPRT_UINT32_C(429'497);
		auto s = DecimalExponentType(b);
		significand = b ? r : significand;

		r = detail::bits::rotr<32>(
				detail::stdr::uint_least32_t(significand * __SPRT_UINT32_C(42'949'673)), 2);
		b = r < __SPRT_UINT32_C(42'949'673);
		s = s * 2 + b;
		significand = b ? r : significand;

		r = detail::bits::rotr<32>(
				detail::stdr::uint_least32_t(significand * __SPRT_UINT32_C(1'288'490'189)), 1);
		b = r < __SPRT_UINT32_C(429'496'730);
		s = s * 2 + b;
		significand = b ? r : significand;

		__sprt_assert(sprt::Max<DecimalExponentType> - s >= exponent);
		exponent += s;
	}
};

template <class DecimalExponentType>
struct remove_trailing_zeros_traits<policy::trailing_zero::remove_t, ieee754_binary64,
		detail::stdr::uint_least64_t, DecimalExponentType> {
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void remove_trailing_zeros(
			detail::stdr::uint_least64_t &significand, DecimalExponentType &exponent) noexcept {
		// See https://github.com/jk-jeon/rtz_benchmark.
		// The idea of branchless search below is by reddit users r/pigeon768 and
		// r/TheoreticalDumbass.

		auto r = detail::bits::rotr<64>(
				detail::stdr::uint_least64_t(significand * __SPRT_UINT64_C(28'999'941'890'838'049)),
				8);
		auto b = r < __SPRT_UINT64_C(184'467'440'738);
		auto s = DecimalExponentType(b);
		significand = b ? r : significand;

		r = detail::bits::rotr<64>(detail::stdr::uint_least64_t(
										   significand * __SPRT_UINT64_C(182'622'766'329'724'561)),
				4);
		b = r < __SPRT_UINT64_C(1'844'674'407'370'956);
		s = s * 2 + b;
		significand = b ? r : significand;

		r = detail::bits::rotr<64>(detail::stdr::uint_least64_t(significand
										   * __SPRT_UINT64_C(10'330'176'681'277'348'905)),
				2);
		b = r < __SPRT_UINT64_C(184'467'440'737'095'517);
		s = s * 2 + b;
		significand = b ? r : significand;

		r = detail::bits::rotr<64>(detail::stdr::uint_least64_t(significand
										   * __SPRT_UINT64_C(14'757'395'258'967'641'293)),
				1);
		b = r < __SPRT_UINT64_C(1'844'674'407'370'955'162);
		s = s * 2 + b;
		significand = b ? r : significand;

		__sprt_assert(sprt::Max<DecimalExponentType> - s >= exponent);
		exponent += s;
	}
};

template <class DecimalExponentType>
struct remove_trailing_zeros_traits<policy::trailing_zero::remove_compact_t, ieee754_binary32,
		detail::stdr::uint_least32_t, DecimalExponentType> {
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void remove_trailing_zeros(
			detail::stdr::uint_least32_t &significand, DecimalExponentType &exponent) noexcept {
		// See https://github.com/jk-jeon/rtz_benchmark.
		while (true) {
			auto const r =
					detail::stdr::uint_least32_t(significand * __SPRT_UINT32_C(1'288'490'189));
			if (r < __SPRT_UINT32_C(429'496'731)) {
				significand = detail::stdr::uint_least32_t(r >> 1);
				exponent += 1;
			} else {
				break;
			}
		}
	}
};

template <class DecimalExponentType>
struct remove_trailing_zeros_traits<policy::trailing_zero::remove_compact_t, ieee754_binary64,
		detail::stdr::uint_least64_t, DecimalExponentType> {
	JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void remove_trailing_zeros(
			detail::stdr::uint_least64_t &significand, DecimalExponentType &exponent) noexcept {
		// See https://github.com/jk-jeon/rtz_benchmark.
		while (true) {
			auto const r = detail::stdr::uint_least64_t(
					significand * __SPRT_UINT64_C(5'534'023'222'112'865'485));
			if (r < __SPRT_UINT64_C(1'844'674'407'370'955'163)) {
				significand = detail::stdr::uint_least64_t(r >> 1);
				exponent += 1;
			} else {
				break;
			}
		}
	}
};

template <class ExponentInt>
struct multiplication_traits<
		ieee754_binary_traits<ieee754_binary32, detail::stdr::uint_least32_t, ExponentInt>,
		detail::stdr::uint_least64_t, 64>
: public multiplication_traits_base<
		  ieee754_binary_traits<ieee754_binary32, detail::stdr::uint_least32_t>,
		  detail::stdr::uint_least64_t, 64> {
	static JKJ_CONSTEXPR20 compute_mul_result compute_mul(carrier_uint u,
			cache_entry_type const &cache) noexcept {
		auto const r = detail::wuint::umul96_upper64(u, cache);
		return {carrier_uint(r >> 32), carrier_uint(r) == 0};
	}

	template <class ShiftAmountType>
	static constexpr detail::stdr::uint_least64_t compute_delta(cache_entry_type const &cache,
			ShiftAmountType beta) noexcept {
		return detail::stdr::uint_least64_t(cache >> ShiftAmountType(cache_bits - 1 - beta));
	}

	template <class ShiftAmountType>
	static JKJ_CONSTEXPR20 compute_mul_parity_result compute_mul_parity(carrier_uint two_f,
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		__sprt_assert(beta >= 1);
		__sprt_assert(beta <= 32);

		auto const r = detail::wuint::umul96_lower64(two_f, cache);
		return {((r >> ShiftAmountType(64 - beta)) & 1) != 0,
			(__SPRT_UINT32_C(0xffff'ffff) & (r >> ShiftAmountType(32 - beta))) == 0};
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_left_endpoint_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return carrier_uint((cache - (cache >> (significand_bits + 2)))
				>> ShiftAmountType(cache_bits - significand_bits - 1 - beta));
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_right_endpoint_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return carrier_uint((cache + (cache >> (significand_bits + 1)))
				>> ShiftAmountType(cache_bits - significand_bits - 1 - beta));
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_round_up_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return (carrier_uint(cache >> ShiftAmountType(cache_bits - significand_bits - 2 - beta))
					   + 1)
				/ 2;
	}
};

template <class ExponentInt>
struct multiplication_traits<
		ieee754_binary_traits<ieee754_binary64, detail::stdr::uint_least64_t, ExponentInt>,
		detail::wuint::uint128, 128>
: public multiplication_traits_base<
		  ieee754_binary_traits<ieee754_binary64, detail::stdr::uint_least64_t>,
		  detail::wuint::uint128, 128> {
	static JKJ_CONSTEXPR20 compute_mul_result compute_mul(carrier_uint u,
			cache_entry_type const &cache) noexcept {
		auto const r = detail::wuint::umul192_upper128(u, cache);
		return {r.high(), r.low() == 0};
	}

	template <class ShiftAmountType>
	static constexpr detail::stdr::uint_least64_t compute_delta(cache_entry_type const &cache,
			ShiftAmountType beta) noexcept {
		return detail::stdr::uint_least64_t(cache.high() >> ShiftAmountType(total_bits - 1 - beta));
	}

	template <class ShiftAmountType>
	static JKJ_CONSTEXPR20 compute_mul_parity_result compute_mul_parity(carrier_uint two_f,
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		__sprt_assert(beta >= 1);
		__sprt_assert(beta < 64);

		auto const r = detail::wuint::umul192_lower128(two_f, cache);
		return {((r.high() >> ShiftAmountType(64 - beta)) & 1) != 0,
			(((r.high() << beta) & __SPRT_UINT64_C(0xffff'ffff'ffff'ffff))
					| (r.low() >> ShiftAmountType(64 - beta)))
					== 0};
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_left_endpoint_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return (cache.high() - (cache.high() >> (significand_bits + 2)))
				>> ShiftAmountType(total_bits - significand_bits - 1 - beta);
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_right_endpoint_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return (cache.high() + (cache.high() >> (significand_bits + 1)))
				>> ShiftAmountType(total_bits - significand_bits - 1 - beta);
	}

	template <class ShiftAmountType>
	static constexpr carrier_uint compute_round_up_for_shorter_interval_case(
			cache_entry_type const &cache, ShiftAmountType beta) noexcept {
		return ((cache.high() >> ShiftAmountType(total_bits - significand_bits - 2 - beta)) + 1)
				/ 2;
	}
};

namespace detail {
////////////////////////////////////////////////////////////////////////////////////////
// The main algorithm.
////////////////////////////////////////////////////////////////////////////////////////

template <class FormatTraits>
struct impl : private FormatTraits::format {
	using format = typename FormatTraits::format;
	using carrier_uint = typename FormatTraits::carrier_uint;
	static constexpr int carrier_bits = FormatTraits::carrier_bits;
	using exponent_int = typename FormatTraits::exponent_int;

	using format::significand_bits;
	using format::min_exponent;
	using format::max_exponent;
	using format::exponent_bias;

	static constexpr int kappa = log::floor_log10_pow2(carrier_bits - significand_bits - 2) - 1;
	static_assert(kappa >= 1, "");
	static_assert(carrier_bits >= significand_bits + 2 + log::floor_log2_pow10(kappa + 1), "");

	// Trailing underscores are to avoid name clash with macros (e.g. Windows.h).
	static constexpr int min_(int x, int y) noexcept { return x < y ? x : y; }
	static constexpr int max_(int x, int y) noexcept { return x > y ? x : y; }

	static constexpr int min_k =
			min_(-log::floor_log10_pow2_minus_log10_4_over_3(max_exponent - significand_bits),
					-log::floor_log10_pow2(max_exponent - significand_bits) + kappa);

	// We do invoke shorter_interval_case for exponent == min_exponent case,
	// so we should not add 1 here.
	static constexpr int max_k = max_(
			-log::floor_log10_pow2_minus_log10_4_over_3(min_exponent - significand_bits /*+ 1*/),
			-log::floor_log10_pow2(min_exponent - significand_bits) + kappa);

	static constexpr int case_shorter_interval_left_endpoint_lower_threshold = 2;
	static constexpr int case_shorter_interval_left_endpoint_upper_threshold = 2
			+ log::floor_log2(
					compute_power< count_factors<5>((carrier_uint(1) << (significand_bits + 2)) - 1)
							+ 1>(10)
					/ 3);

	static constexpr int case_shorter_interval_right_endpoint_lower_threshold = 0;
	static constexpr int case_shorter_interval_right_endpoint_upper_threshold = 2
			+ log::floor_log2(
					compute_power< count_factors<5>((carrier_uint(1) << (significand_bits + 1)) + 1)
							+ 1>(10)
					/ 3);

	static constexpr int shorter_interval_tie_lower_threshold =
			-log::floor_log5_pow2_minus_log5_3(significand_bits + 4) - 2 - significand_bits;
	static constexpr int shorter_interval_tie_upper_threshold =
			-log::floor_log5_pow2(significand_bits + 2) - 2 - significand_bits;

	template <class PreferredIntegerTypesPolicy>
	using remainder_type =
			typename PreferredIntegerTypesPolicy::template remainder_type< FormatTraits,
					compute_power<kappa + 1>(detail::stdr::uint_least64_t(10))>;

	template <class PreferredIntegerTypesPolicy>
	using decimal_exponent_type =
			typename PreferredIntegerTypesPolicy::template decimal_exponent_type< FormatTraits,
					detail::stdr::int_least32_t(min_(-max_k, min_k)),
					detail::stdr::int_least32_t(max_(max_k, -min_k + kappa + 1))>;

	template <class SignPolicy, class TrailingZeroPolicy, class PreferredIntegerTypesPolicy>
	using return_type = decimal_fp<carrier_uint, decimal_exponent_type<PreferredIntegerTypesPolicy>,
			SignPolicy::return_has_sign, TrailingZeroPolicy::report_trailing_zeros>;

	//// The main algorithm assumes the input is a normal/subnormal finite number.

	template <class SignPolicy, class TrailingZeroPolicy, class IntervalTypeProvider,
			class BinaryToDecimalRoundingPolicy, class CachePolicy,
			class PreferredIntegerTypesPolicy>
	JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
			return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
			compute_nearest(signed_significand_bits<FormatTraits> s,
					exponent_int exponent_bits) noexcept {
		using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
		static_assert(min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

		using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
		using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
		using shift_amount_type =
				typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

		using multiplication_traits_ = multiplication_traits<FormatTraits,
				typename cache_holder_type::cache_entry_type, cache_holder_type::cache_bits>;

		auto two_fc = s.remove_sign_bit_and_shift();
		auto binary_exponent = exponent_bits;

		// Is the input a normal number?
		if (binary_exponent != 0) {
			binary_exponent += format::exponent_bias - format::significand_bits;

			// Shorter interval case; proceed like Schubfach.
			// One might think this condition is wrong, since when exponent_bits ==
			// 1 and two_fc == 0, the interval is actually regular. However, it
			// turns out that this seemingly wrong condition is actually fine,
			// because the end result is anyway the same.
			//
			// [binary32]
			// (fc-1/2) * 2^e = 1.175'494'28... * 10^-38
			// (fc-1/4) * 2^e = 1.175'494'31... * 10^-38
			//    fc    * 2^e = 1.175'494'35... * 10^-38
			// (fc+1/2) * 2^e = 1.175'494'42... * 10^-38
			//
			// Hence, shorter_interval_case will return 1.175'494'4 * 10^-38.
			// 1.175'494'3 * 10^-38 is also a correct shortest representation that
			// will be rejected if we assume shorter interval, but 1.175'494'4 *
			// 10^-38 is closer to the true value so it doesn't matter.
			//
			// [binary64]
			// (fc-1/2) * 2^e = 2.225'073'858'507'201'13... * 10^-308
			// (fc-1/4) * 2^e = 2.225'073'858'507'201'25... * 10^-308
			//    fc    * 2^e = 2.225'073'858'507'201'38... * 10^-308
			// (fc+1/2) * 2^e = 2.225'073'858'507'201'63... * 10^-308
			//
			// Hence, shorter_interval_case will return 2.225'073'858'507'201'4 *
			// 10^-308. This is indeed of the shortest length, and it is the unique
			// one closest to the true value among valid representations of the same
			// length.
			static_assert(stdr::is_same<format, ieee754_binary32>::value
							|| stdr::is_same<format, ieee754_binary64>::value,
					"");

			// Shorter interval case.
			if (two_fc == 0) {
				auto interval_type = IntervalTypeProvider::shorter_interval(s);

				// Compute k and beta.
				auto const minus_k = log::floor_log10_pow2_minus_log10_4_over_3< min_exponent
								- format::significand_bits,
						max_exponent - format::significand_bits, decimal_exponent_type_>(
						binary_exponent);
				auto const beta = shift_amount_type(binary_exponent
						+ log::floor_log2_pow10<min_k, max_k>(decimal_exponent_type_(-minus_k)));

				// Compute xi and zi.
				auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
						decimal_exponent_type_(-minus_k));

				auto xi = multiplication_traits_::compute_left_endpoint_for_shorter_interval_case(
						cache, beta);
				auto zi = multiplication_traits_::compute_right_endpoint_for_shorter_interval_case(
						cache, beta);

				// If we don't accept the right endpoint and
				// if the right endpoint is an integer, decrease it.
				if (!interval_type.include_right_endpoint()
						&& is_right_endpoint_integer_shorter_interval(binary_exponent)) {
					--zi;
				}
				// If we don't accept the left endpoint or
				// if the left endpoint is not an integer, increase it.
				if (!interval_type.include_left_endpoint()
						|| !is_left_endpoint_integer_shorter_interval(binary_exponent)) {
					++xi;
				}

				// Try bigger divisor.
				// zi is at most floor((f_c + 1/2) * 2^e * 10^k0).
				// Substituting f_c = 2^p and k0 = -floor(log10(3 * 2^(e-2))), we get
				// zi <= floor((2^(p+1) + 1) * 20/3) <= ceil((2^(p+1) + 1)/3) * 20.
				// This computation does not overflow for any of the formats I care about.
				carrier_uint decimal_significand = div::divide_by_pow10< 1, carrier_uint,
						carrier_uint(((((carrier_uint(1) << (significand_bits + 1)) + 1) / 3) + 1)
								* 20)>(zi);

				// If succeed, remove trailing zeros if necessary and return.
				if (decimal_significand * 10 >= xi) {
					return SignPolicy::handle_sign(s,
							TrailingZeroPolicy::template on_trailing_zeros<format>(
									decimal_significand, decimal_exponent_type_(minus_k + 1)));
				}

				// Otherwise, compute the round-up of y.
				decimal_significand =
						multiplication_traits_::compute_round_up_for_shorter_interval_case(cache,
								beta);

				// When tie occurs, choose one of them according to the rule.
				if (BinaryToDecimalRoundingPolicy::prefer_round_down(decimal_significand)
						&& binary_exponent >= shorter_interval_tie_lower_threshold
						&& binary_exponent <= shorter_interval_tie_upper_threshold) {
					--decimal_significand;
				} else if (decimal_significand < xi) {
					++decimal_significand;
				}
				return SignPolicy::handle_sign(s,
						TrailingZeroPolicy::template no_trailing_zeros<format>(decimal_significand,
								decimal_exponent_type_(minus_k)));
			}

			// Normal interval case.
			two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
		}
		// Is the input a subnormal number?
		else {
			// Normal interval case.
			binary_exponent = format::min_exponent - format::significand_bits;
		}

		//////////////////////////////////////////////////////////////////////
		// Step 1: Schubfach multiplier calculation.
		//////////////////////////////////////////////////////////////////////

		auto interval_type = IntervalTypeProvider::normal_interval(s);

		// Compute k and beta.
		auto const minus_k = decimal_exponent_type_(
				log::floor_log10_pow2<min_exponent - format::significand_bits,
						max_exponent - format::significand_bits, decimal_exponent_type_>(
						binary_exponent)
				- kappa);
		auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
				decimal_exponent_type_(-minus_k));
		auto const beta = shift_amount_type(binary_exponent
				+ log::floor_log2_pow10<min_k, max_k>(decimal_exponent_type_(-minus_k)));

		// Compute zi and deltai.
		// 10^kappa <= deltai < 10^(kappa + 1)
		auto const deltai =
				static_cast<remainder_type_>(multiplication_traits_::compute_delta(cache, beta));
		// For the case of binary32, the result of integer check is not correct for
		// 29711844 * 2^-82
		// = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
		// and 29711844 * 2^-81
		// = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17,
		// and they are the unique counterexamples. However, since 29711844 is even,
		// this does not cause any problem for the endpoints calculations; it can only
		// cause a problem when we need to perform integer check for the center.
		// Fortunately, with these inputs, that branch is never executed, so we are
		// fine.
		auto const z_result =
				multiplication_traits_::compute_mul(carrier_uint((two_fc | 1) << beta), cache);


		//////////////////////////////////////////////////////////////////////
		// Step 2: Try larger divisor; remove trailing zeros if necessary.
		//////////////////////////////////////////////////////////////////////

		constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));
		constexpr auto small_divisor = compute_power<kappa>(remainder_type_(10));

		// Using an upper bound on zi, we might be able to optimize the division
		// better than the compiler; we are computing zi / big_divisor here.
		carrier_uint decimal_significand = div::divide_by_pow10< kappa + 1, carrier_uint,
				carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(
				z_result.integer_part);
		auto r = remainder_type_(z_result.integer_part - big_divisor * decimal_significand);

		do {
			if (r < deltai) {
				// Exclude the right endpoint if necessary.
				if ((r | remainder_type_(!z_result.is_integer)
							| remainder_type_(interval_type.include_right_endpoint()))
						== 0) {
					JKJ_IF_CONSTEXPR(BinaryToDecimalRoundingPolicy::tag
							== policy::binary_to_decimal_rounding::tag_t::do_not_care) {
						decimal_significand *= 10;
						--decimal_significand;
						return SignPolicy::handle_sign(s,
								TrailingZeroPolicy::template no_trailing_zeros<format>(
										decimal_significand,
										decimal_exponent_type_(minus_k + kappa)));
					}
					else {
						--decimal_significand;
						r = big_divisor;
						break;
					}
				}
			} else if (r > deltai) {
				break;
			} else {
				// r == deltai; compare fractional parts.
				auto const x_result = multiplication_traits_::compute_mul_parity(
						carrier_uint(two_fc - 1), cache, beta);

				if (!(x_result.parity
							| (x_result.is_integer & interval_type.include_left_endpoint()))) {
					break;
				}
			}

			// We may need to remove trailing zeros.
			return SignPolicy::handle_sign(s,
					TrailingZeroPolicy::template on_trailing_zeros<format>(decimal_significand,
							decimal_exponent_type_(minus_k + kappa + 1)));
		} while (false);


		//////////////////////////////////////////////////////////////////////
		// Step 3: Find the significand with the smaller divisor.
		//////////////////////////////////////////////////////////////////////

		decimal_significand *= 10;

		JKJ_IF_CONSTEXPR(BinaryToDecimalRoundingPolicy::tag
				== policy::binary_to_decimal_rounding::tag_t::do_not_care) {
			// Normally, we want to compute
			// significand += r / small_divisor
			// and return, but we need to take care of the case that the resulting
			// value is exactly the right endpoint, while that is not included in the
			// interval.
			if (!interval_type.include_right_endpoint()) {
				// Is r divisible by 10^kappa?
				if (div::check_divisibility_and_divide_by_pow10<kappa>(r) && z_result.is_integer) {
					// This should be in the interval.
					decimal_significand += r - 1;
				} else {
					decimal_significand += r;
				}
			} else {
				decimal_significand += div::small_division_by_pow10<kappa>(r);
			}
		}
		else {
			// delta is equal to 10^(kappa + elog10(2) - floor(elog10(2))), so dist cannot
			// be larger than r.
			auto dist = remainder_type_(r - (deltai / 2) + (small_divisor / 2));
			bool const approx_y_parity = ((dist ^ (small_divisor / 2)) & 1) != 0;

			// Is dist divisible by 10^kappa?
			bool const divisible_by_small_divisor =
					div::check_divisibility_and_divide_by_pow10<kappa>(dist);

			// Add dist / 10^kappa to the significand.
			decimal_significand += dist;

			if (divisible_by_small_divisor) {
				// Check z^(f) >= epsilon^(f).
				// We have either yi == zi - epsiloni or yi == (zi - epsiloni) - 1,
				// where yi == zi - epsiloni if and only if z^(f) >= epsilon^(f).
				// Since there are only 2 possibilities, we only need to care about the
				// parity. Also, zi and r should have the same parity since the divisor
				// is an even number.
				auto const y_result =
						multiplication_traits_::compute_mul_parity(two_fc, cache, beta);
				if (y_result.parity != approx_y_parity) {
					--decimal_significand;
				} else {
					// If z^(f) >= epsilon^(f), we might have a tie
					// when z^(f) == epsilon^(f), or equivalently, when y is an integer.
					// For tie-to-up case, we can just choose the upper one.
					if (BinaryToDecimalRoundingPolicy::prefer_round_down(decimal_significand)
							& y_result.is_integer) {
						--decimal_significand;
					}
				}
			}
		}
		return SignPolicy::handle_sign(s,
				TrailingZeroPolicy::template no_trailing_zeros<format>(decimal_significand,
						decimal_exponent_type_(minus_k + kappa)));
	}

	template <class SignPolicy, class TrailingZeroPolicy, class CachePolicy,
			class PreferredIntegerTypesPolicy>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
			return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
			compute_left_closed_directed(signed_significand_bits<FormatTraits> s,
					exponent_int exponent_bits) noexcept {
		using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
		static_assert(min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

		using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
		using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
		using shift_amount_type =
				typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

		using multiplication_traits_ = multiplication_traits<FormatTraits,
				typename cache_holder_type::cache_entry_type, cache_holder_type::cache_bits>;

		auto two_fc = s.remove_sign_bit_and_shift();
		auto binary_exponent = exponent_bits;

		// Is the input a normal number?
		if (binary_exponent != 0) {
			binary_exponent += format::exponent_bias - format::significand_bits;
			two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
		}
		// Is the input a subnormal number?
		else {
			binary_exponent = format::min_exponent - format::significand_bits;
		}

		//////////////////////////////////////////////////////////////////////
		// Step 1: Schubfach multiplier calculation.
		//////////////////////////////////////////////////////////////////////

		// Compute k and beta.
		auto const minus_k = decimal_exponent_type_(
				log::floor_log10_pow2<format::min_exponent - format::significand_bits,
						format::max_exponent - format::significand_bits, decimal_exponent_type_>(
						binary_exponent)
				- kappa);
		auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
				decimal_exponent_type_(-minus_k));
		auto const beta = shift_amount_type(binary_exponent
				+ log::floor_log2_pow10<min_k, max_k>(decimal_exponent_type_(-minus_k)));

		// Compute xi and deltai.
		// 10^kappa <= deltai < 10^(kappa + 1)
		auto const deltai =
				static_cast<remainder_type_>(multiplication_traits_::compute_delta(cache, beta));
		auto x_result = multiplication_traits_::compute_mul(carrier_uint(two_fc << beta), cache);

		// Deal with the unique exceptional cases
		// 29711844 * 2^-82
		// = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
		// and 29711844 * 2^-81
		// = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17
		// for binary32.
		JKJ_IF_CONSTEXPR(stdr::is_same<format, ieee754_binary32>::value) {
			if (binary_exponent <= -80) {
				x_result.is_integer = false;
			}
		}

		if (!x_result.is_integer) {
			++x_result.integer_part;
		}

		//////////////////////////////////////////////////////////////////////
		// Step 2: Try larger divisor; remove trailing zeros if necessary.
		//////////////////////////////////////////////////////////////////////

		constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));

		// Using an upper bound on xi, we might be able to optimize the division
		// better than the compiler; we are computing xi / big_divisor here.
		carrier_uint decimal_significand = div::divide_by_pow10< kappa + 1, carrier_uint,
				carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(
				x_result.integer_part);
		auto r = remainder_type_(x_result.integer_part - big_divisor * decimal_significand);

		if (r != 0) {
			++decimal_significand;
			r = remainder_type_(big_divisor - r);
		}

		do {
			if (r > deltai) {
				break;
			} else if (r == deltai) {
				// Compare the fractional parts.
				// This branch is never taken for the exceptional cases
				// 2f_c = 29711482, e = -81
				// (6.1442649164096937243516663440523473127541365101933479309082... *
				// 10^-18) and 2f_c = 29711482, e = -80
				// (1.2288529832819387448703332688104694625508273020386695861816... *
				// 10^-17).
				// For the case of compressed cache for binary32, there is another
				// exceptional case 2f_c = 33554430, e = -10 (16383.9990234375). In this
				// case, the recovered cache is two large to make compute_mul_parity
				// mistakenly conclude that z is not an integer, but actually z = 16384 is
				// an integer.
				JKJ_IF_CONSTEXPR(stdr::is_same<cache_holder_type,
						compressed_cache_holder<ieee754_binary32>>::value) {
					if (two_fc == 33'554'430 && binary_exponent == -10) {
						break;
					}
				}
				auto const z_result = multiplication_traits_::compute_mul_parity(
						carrier_uint(two_fc + 2), cache, beta);
				if (z_result.parity || z_result.is_integer) {
					break;
				}
			}

			// The ceiling is inside, so we are done.
			return SignPolicy::handle_sign(s,
					TrailingZeroPolicy::template on_trailing_zeros<format>(decimal_significand,
							decimal_exponent_type_(minus_k + kappa + 1)));
		} while (false);


		//////////////////////////////////////////////////////////////////////
		// Step 3: Find the significand with the smaller divisor.
		//////////////////////////////////////////////////////////////////////

		decimal_significand *= 10;
		decimal_significand -= div::small_division_by_pow10<kappa>(r);
		return SignPolicy::handle_sign(s,
				TrailingZeroPolicy::template no_trailing_zeros<format>(decimal_significand,
						decimal_exponent_type_(minus_k + kappa)));
	}

	template <class SignPolicy, class TrailingZeroPolicy, class CachePolicy,
			class PreferredIntegerTypesPolicy>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
			return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
			compute_right_closed_directed(signed_significand_bits<FormatTraits> s,
					exponent_int exponent_bits) noexcept {
		using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
		static_assert(min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

		using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
		using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
		using shift_amount_type =
				typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

		using multiplication_traits_ = multiplication_traits<FormatTraits,
				typename cache_holder_type::cache_entry_type, cache_holder_type::cache_bits>;

		auto two_fc = s.remove_sign_bit_and_shift();
		auto binary_exponent = exponent_bits;
		bool shorter_interval = false;

		// Is the input a normal number?
		if (binary_exponent != 0) {
			if (two_fc == 0 && binary_exponent != 1) {
				shorter_interval = true;
			}
			binary_exponent += format::exponent_bias - format::significand_bits;
			two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
		}
		// Is the input a subnormal number?
		else {
			binary_exponent = format::min_exponent - format::significand_bits;
		}

		//////////////////////////////////////////////////////////////////////
		// Step 1: Schubfach multiplier calculation.
		//////////////////////////////////////////////////////////////////////

		// Compute k and beta.
		auto const minus_k = decimal_exponent_type_(
				log::floor_log10_pow2<format::min_exponent - format::significand_bits,
						format::max_exponent - format::significand_bits, decimal_exponent_type_>(
						exponent_int(binary_exponent - (shorter_interval ? 1 : 0)))
				- kappa);
		auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
				decimal_exponent_type_(-minus_k));
		auto const beta = shift_amount_type(
				binary_exponent + log::floor_log2_pow10(decimal_exponent_type_(-minus_k)));

		// Compute zi and deltai.
		// 10^kappa <= deltai < 10^(kappa + 1)
		auto const deltai = static_cast<remainder_type_>(multiplication_traits_::compute_delta(
				cache, shift_amount_type(beta - (shorter_interval ? 1 : 0))));
		carrier_uint const zi =
				multiplication_traits_::compute_mul(carrier_uint(two_fc << beta), cache)
						.integer_part;


		//////////////////////////////////////////////////////////////////////
		// Step 2: Try larger divisor; remove trailing zeros if necessary.
		//////////////////////////////////////////////////////////////////////

		constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));

		// Using an upper bound on zi, we might be able to optimize the division better
		// than the compiler; we are computing zi / big_divisor here.
		carrier_uint decimal_significand = div::divide_by_pow10< kappa + 1, carrier_uint,
				carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(zi);
		auto const r = remainder_type_(zi - big_divisor * decimal_significand);

		do {
			if (r > deltai) {
				break;
			} else if (r == deltai) {
				// Compare the fractional parts.
				if (!multiplication_traits_::compute_mul_parity(
							carrier_uint(two_fc - (shorter_interval ? 1 : 2)), cache, beta)
								.parity) {
					break;
				}
			}

			// The floor is inside, so we are done.
			return SignPolicy::handle_sign(s,
					TrailingZeroPolicy::template on_trailing_zeros<format>(decimal_significand,
							decimal_exponent_type_(minus_k + kappa + 1)));
		} while (false);


		//////////////////////////////////////////////////////////////////////
		// Step 3: Find the significand with the small divisor.
		//////////////////////////////////////////////////////////////////////

		decimal_significand *= 10;
		decimal_significand += div::small_division_by_pow10<kappa>(r);
		return SignPolicy::handle_sign(s,
				TrailingZeroPolicy::template no_trailing_zeros<format>(decimal_significand,
						decimal_exponent_type_(minus_k + kappa)));
	}

	static constexpr bool is_right_endpoint_integer_shorter_interval(
			exponent_int binary_exponent) noexcept {
		return binary_exponent >= case_shorter_interval_right_endpoint_lower_threshold
				&& binary_exponent <= case_shorter_interval_right_endpoint_upper_threshold;
	}

	static constexpr bool is_left_endpoint_integer_shorter_interval(
			exponent_int binary_exponent) noexcept {
		return binary_exponent >= case_shorter_interval_left_endpoint_lower_threshold
				&& binary_exponent <= case_shorter_interval_left_endpoint_upper_threshold;
	}
};


////////////////////////////////////////////////////////////////////////////////////////
// Policy holder.
////////////////////////////////////////////////////////////////////////////////////////

// The library will specify a list of accepted kinds of policies and their defaults,
// and the user will pass a list of policies parameters. The policy parameters are
// supposed to be stateless and only convey information through their types.
// The aim of the helper classes/functions given below is to do the following:
//   1. Check if the policy parameters given by the user are all valid; that means,
//      each of them should be at least of one of the kinds specified by the library.
//      If that's not the case, then the compilation fails.
//   2. Check if multiple policy parameters for the same kind is specified by the
//      user. If that's the case, then the compilation fails.
//   3. Build a class deriving from all policies the user have given, and also from
//      the default policies if the user did not specify one for some kinds.
// The library considers a certain policy parameter to belong to a specific kind if and only
// if the parameter's type has a member type with a specific name; for example, it belongs
// to "sign policy" kind if there is a member type sign_policy.

// For a given kind, find a policy belonging to that kind.
// Check if there are more than one such policies.
enum class policy_found_info {
	not_found,
	unique,
	repeated
};
template <class Policy, policy_found_info info>
struct found_policy_pair {
	// Either the policy parameter type given by the user, or the default policy.
	using policy = Policy;
	static constexpr auto found_info = info;
};

template <class KindDetector, class DefaultPolicy>
struct detector_default_pair {
	using kind_detector = KindDetector;

	// Iterate through all given policy parameter types and see if there is a policy
	// parameter type belonging to the policy kind specified by KindDetector.
	// 1. If there is none, get_found_policy_pair returns
	//    found_policy_pair<DefaultPolicy, policy_found_info::not_found>.
	// 2. If there is only one parameter type belonging to the specified kind, then
	//    get_found_policy_pair returns
	//    found_policy_pair<Policy, policy_found_info::unique>
	//    where Policy is the unique parameter type belonging to the specified kind.
	// 3. If there are multiple parameter types belonging to the specified kind, then
	//    get_found_policy_pair returns
	//    found_policy_pair<FirstPolicy, policy_found_info::repeated>
	//    where FirstPolicy is the first parameter type belonging to the specified kind.
	//    The compilation must fail if this happens.
	// This is done by first setting FoundPolicyInfo below to
	// found_policy_pair<DefaultPolicy, policy_found_info::not_found>, and then iterate
	// over Policies, replacing FoundPolicyInfo by the appropriate one if a parameter
	// type belonging to the specified kind is found.

	template <class FoundPolicyInfo, class... Policies>
	struct get_found_policy_pair_impl;

	template <class FoundPolicyInfo>
	struct get_found_policy_pair_impl<FoundPolicyInfo> {
		using type = FoundPolicyInfo;
	};

	template <class FoundPolicyInfo, class FirstPolicy, class... RemainingPolicies>
	struct get_found_policy_pair_impl<FoundPolicyInfo, FirstPolicy, RemainingPolicies...> {
		using type = typename stdr::conditional< KindDetector{}(dummy<FirstPolicy>{}),
				typename stdr::conditional< FoundPolicyInfo::found_info
								== policy_found_info::not_found,
						typename get_found_policy_pair_impl<
								found_policy_pair<FirstPolicy, policy_found_info::unique>,
								RemainingPolicies...>::type,
						typename get_found_policy_pair_impl<
								found_policy_pair<FirstPolicy, policy_found_info::repeated>,
								RemainingPolicies...>::type>::type,
				typename get_found_policy_pair_impl<FoundPolicyInfo,
						RemainingPolicies...>::type>::type;
	};

	template <class... Policies>
	using get_found_policy_pair = typename get_found_policy_pair_impl<
			found_policy_pair<DefaultPolicy, policy_found_info::not_found>, Policies...>::type;
};

// Simple typelist of detector_default_pair's.
template <class... DetectorDefaultPairs>
struct detector_default_pair_list { };

// Check if a given policy belongs to one of the kinds specified by the library.
template <class Policy>
constexpr bool check_policy_validity(dummy<Policy>, detector_default_pair_list<>) noexcept {
	return false;
}
template <class Policy, class FirstDetectorDefaultPair, class... RemainingDetectorDefaultPairs>
constexpr bool check_policy_validity(dummy<Policy>,
		detector_default_pair_list<FirstDetectorDefaultPair,
				RemainingDetectorDefaultPairs...>) noexcept {
	return typename FirstDetectorDefaultPair::kind_detector{}(dummy<Policy>{})
			|| check_policy_validity(dummy<Policy>{},
					detector_default_pair_list<RemainingDetectorDefaultPairs...>{});
}

// Check if all of policies belong to some of the kinds specified by the library.
template <class DetectorDefaultPairList>
constexpr bool check_policy_list_validity(DetectorDefaultPairList) noexcept {
	return true;
}
template <class DetectorDefaultPairList, class FirstPolicy, class... RemainingPolicies>
constexpr bool check_policy_list_validity(DetectorDefaultPairList, dummy<FirstPolicy> first_policy,
		dummy<RemainingPolicies>... remaining_policies) {
	return check_policy_validity(first_policy, DetectorDefaultPairList{})
			&& check_policy_list_validity(DetectorDefaultPairList{}, remaining_policies...);
}

// Actual policy holder class deriving from all specified policy types.
template <class... Policies>
struct policy_holder : Policies... { };

// Iterate through the library-specified list of base-default pairs, i.e., the list of
// policy kinds and their defaults. For each base-default pair, call
// base_default_pair::get_found_policy_pair on the list of user-specified list of
// policies to get found_policy_pair, and build the list of them.

template <bool repeated_, class... FoundPolicyPairs>
struct found_policy_pair_list {
	// This will be set to be true if and only if there exists at least one
	// found_policy_pair inside FoundPolicyPairs with
	// found_info == policy_found_info::repeated, in which case the compilation must
	// fail.
	static constexpr bool repeated = repeated_;
};

// Iterate through DetectorDefaultPairList and augment FoundPolicyPairList by one at each
// iteration.
template <class DetectorDefaultPairList, class FoundPolicyPairList, class... Policies>
struct make_policy_pair_list_impl;

// When there is no more detector-default pair to iterate, then the current
// found_policy_pair_list is the final result.
template <bool repeated, class... FoundPolicyPairs, class... Policies>
struct make_policy_pair_list_impl<detector_default_pair_list<>,
		found_policy_pair_list<repeated, FoundPolicyPairs...>, Policies...> {
	using type = found_policy_pair_list<repeated, FoundPolicyPairs...>;
};

// For the first detector-default pair in the remaining list, call
// detector_default_pair::get_found_policy_pair on Policies and add the returned
// found_policy_pair into the current list of found_policy_pair's, and move to the next
// detector-default pair.
template <class FirstDetectorDefaultPair, class... RemainingDetectorDefaultPairs, bool repeated,
		class... FoundPolicyPairs, class... Policies>
struct make_policy_pair_list_impl<
		detector_default_pair_list<FirstDetectorDefaultPair, RemainingDetectorDefaultPairs...>,
		found_policy_pair_list<repeated, FoundPolicyPairs...>, Policies...> {
	using new_found_policy_pair =
			typename FirstDetectorDefaultPair::template get_found_policy_pair<Policies...>;

	using type = typename make_policy_pair_list_impl<
			detector_default_pair_list<RemainingDetectorDefaultPairs...>,
			found_policy_pair_list<
					(repeated || new_found_policy_pair::found_info == policy_found_info::repeated),
					new_found_policy_pair, FoundPolicyPairs...>,
			Policies...>::type;
};

template <class DetectorDefaultPairList, class... Policies>
using policy_pair_list = typename make_policy_pair_list_impl<DetectorDefaultPairList,
		found_policy_pair_list<false>, Policies...>::type;

// Unpack FoundPolicyPairList into found_policy_pair's and build the policy_holder type
// from the corresponding typelist of found_policy_pair::policy's.
template <class FoundPolicyPairList, class... RawPolicies>
struct convert_to_policy_holder_impl;

template <bool repeated, class... RawPolicies>
struct convert_to_policy_holder_impl<found_policy_pair_list<repeated>, RawPolicies...> {
	using type = policy_holder<RawPolicies...>;
};

template <bool repeated, class FirstFoundPolicyPair, class... RemainingFoundPolicyPairs,
		class... RawPolicies>
struct convert_to_policy_holder_impl<
		found_policy_pair_list<repeated, FirstFoundPolicyPair, RemainingFoundPolicyPairs...>,
		RawPolicies...> {
	using type = typename convert_to_policy_holder_impl<
			found_policy_pair_list<repeated, RemainingFoundPolicyPairs...>,
			typename FirstFoundPolicyPair::policy, RawPolicies...>::type;
};

template <class FoundPolicyPairList>
using convert_to_policy_holder = typename convert_to_policy_holder_impl<FoundPolicyPairList>::type;

template <class DetectorDefaultPairList, class... Policies>
struct make_policy_holder_impl {
	static_assert(check_policy_list_validity(DetectorDefaultPairList{}, dummy<Policies>{}...),
			"jkj::dragonbox: an invalid policy is specified");

	static_assert(!policy_pair_list<DetectorDefaultPairList, Policies...>::repeated,
			"jkj::dragonbox: at most one policy should be specified for each policy kind");

	using type = convert_to_policy_holder<policy_pair_list<DetectorDefaultPairList, Policies...>>;
};

template <class DetectorDefaultPairList, class... Policies>
using make_policy_holder =
		typename make_policy_holder_impl<DetectorDefaultPairList, Policies...>::type;

// Policy kind detectors.
struct is_sign_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::sign_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};
struct is_trailing_zero_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::trailing_zero_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};
struct is_decimal_to_binary_rounding_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::decimal_to_binary_rounding_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};
struct is_binary_to_decimal_rounding_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::binary_to_decimal_rounding_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};
struct is_cache_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::cache_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};
struct is_preferred_integer_types_policy {
	constexpr bool operator()(...) noexcept { return false; }
	template <class Policy, class = typename Policy::preferred_integer_types_policy>
	constexpr bool operator()(dummy<Policy>) noexcept {
		return true;
	}
};

template <class... Policies>
using to_decimal_policy_holder = make_policy_holder<
		detector_default_pair_list<
				detector_default_pair<is_sign_policy, policy::sign::return_sign_t>,
				detector_default_pair<is_trailing_zero_policy, policy::trailing_zero::remove_t>,
				detector_default_pair<is_decimal_to_binary_rounding_policy,
						policy::decimal_to_binary_rounding::nearest_to_even_t>,
				detector_default_pair<is_binary_to_decimal_rounding_policy,
						policy::binary_to_decimal_rounding::to_even_t>,
				detector_default_pair<is_cache_policy, policy::cache::full_t>,
				detector_default_pair<is_preferred_integer_types_policy,
						policy::preferred_integer_types::match_t>>,
		Policies...>;

template <class FormatTraits, class... Policies>
using to_decimal_return_type = typename impl<FormatTraits>::template return_type<
		typename to_decimal_policy_holder<Policies...>::sign_policy,
		typename to_decimal_policy_holder<Policies...>::trailing_zero_policy,
		typename to_decimal_policy_holder<Policies...>::preferred_integer_types_policy>;

template <class FormatTraits, class PolicyHolder>
struct to_decimal_dispatcher {
	using sign_policy = typename PolicyHolder::sign_policy;
	using trailing_zero_policy = typename PolicyHolder::trailing_zero_policy;
	using binary_to_decimal_rounding_policy =
			typename PolicyHolder::binary_to_decimal_rounding_policy;
	using cache_policy = typename PolicyHolder::cache_policy;
	using preferred_integer_types_policy = typename PolicyHolder::preferred_integer_types_policy;
	using return_type = typename impl<FormatTraits>::template return_type<sign_policy,
			trailing_zero_policy, preferred_integer_types_policy>;

	template <class IntervalTypeProvider>
	JKJ_FORCEINLINE JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 return_type operator()(IntervalTypeProvider,
			signed_significand_bits<FormatTraits> s,
			typename FormatTraits::exponent_int exponent_bits) noexcept {
		constexpr auto tag = IntervalTypeProvider::tag;

		JKJ_IF_CONSTEXPR(tag == policy::decimal_to_binary_rounding::tag_t::to_nearest) {
			return impl<FormatTraits>::template compute_nearest< sign_policy, trailing_zero_policy,
					IntervalTypeProvider, binary_to_decimal_rounding_policy, cache_policy,
					preferred_integer_types_policy>(s, exponent_bits);
		}
		else JKJ_IF_CONSTEXPR(
				tag == policy::decimal_to_binary_rounding::tag_t::left_closed_directed) {
			return impl<FormatTraits>::template compute_left_closed_directed< sign_policy,
					trailing_zero_policy, cache_policy, preferred_integer_types_policy>(s,
					exponent_bits);
		}
		else {
#if JKJ_HAS_IF_CONSTEXPR
			static_assert(tag == policy::decimal_to_binary_rounding::tag_t::right_closed_directed,
					"");
#endif
			return impl<FormatTraits>::template compute_right_closed_directed< sign_policy,
					trailing_zero_policy, cache_policy, preferred_integer_types_policy>(s,
					exponent_bits);
		}
	}
};
} // namespace detail


////////////////////////////////////////////////////////////////////////////////////////
// The interface function.
////////////////////////////////////////////////////////////////////////////////////////

template <class FormatTraits, class ExponentBits, class... Policies>
JKJ_FORCEINLINE
		JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 detail::to_decimal_return_type<FormatTraits, Policies...>
		to_decimal_ex(signed_significand_bits<FormatTraits> s, ExponentBits exponent_bits,
				Policies...) noexcept {
	// Build policy holder type.
	using policy_holder = detail::to_decimal_policy_holder<Policies...>;

	return policy_holder::delegate(s, detail::to_decimal_dispatcher<FormatTraits, policy_holder>{},
			s, exponent_bits);
}

template <class Float, class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
		class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
				typename ConversionTraits::carrier_uint>,
		class... Policies>
JKJ_FORCEINLINE
		JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 detail::to_decimal_return_type<FormatTraits, Policies...>
		to_decimal(Float x, Policies... policies) noexcept {
	auto const br = make_float_bits<Float, ConversionTraits, FormatTraits>(x);
	auto const exponent_bits = br.extract_exponent_bits();
	auto const s = br.remove_exponent_bits();
	__sprt_assert(br.is_finite() && br.is_nonzero());

	return to_decimal_ex(s, exponent_bits, policies...);
}
} // namespace dragonbox
} // namespace JKJ_NAMESPACE

#endif

#ifdef JKJ_DRAGONBOX_LEAK_MACROS
#define JKJ_DRAGONBOX_MACROS_DEFINED
#else
#undef JKJ_HAS_BUILTIN
#undef JKJ_FORCEINLINE
#undef JKJ_SAFEBUFFERS
#undef JKJ_CONSTEXPR20
#undef JKJ_USE_IS_CONSTANT_EVALUATED
#undef JKJ_CAN_BRANCH_ON_CONSTEVAL
#undef JKJ_IF_NOT_CONSTEVAL
#undef JKJ_IF_CONSTEVAL
#undef JKJ_HAS_BIT_CAST
#undef JKJ_IF_CONSTEXPR
#undef JKJ_HAS_IF_CONSTEXPR
#undef JKJ_INLINE_VARIABLE
#undef JKJ_HAS_INLINE_VARIABLE
#undef JKJ_HAS_CONSTEXPR17
#undef JKJ_CONSTEXPR14
#undef JKJ_HAS_CONSTEXPR14
#undef JKJ_HAS_VARIABLE_TEMPLATES
#if JKJ_FAST_MUL64_DEFINED
#undef JKJ_FAST_MUL64_DEFINED
#else
#undef JKJ_FAST_MUL64
#endif
#if JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
#undef JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
#else
#undef JKJ_STD_REPLACEMENT_NAMESPACE
#endif
#if JKJ_STATIC_DATA_SECTION_DEFINED
#undef JKJ_STATIC_DATA_SECTION_DEFINED
#else
#undef JKJ_STATIC_DATA_SECTION
#endif
#if JKJ_NAMESPACE_DEFINED
#undef JKJ_NAMESPACE_DEFINED
#else
#undef JKJ_NAMESPACE
#endif
#undef JKJ_DRAGONBOX_MACROS_DEFINED
#endif
