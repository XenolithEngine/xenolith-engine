/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_CTYPES_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_CTYPES_H_

#include <sprt/c/__sprt_stddef.h>
#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_float.h>

#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/bits/__sprt_time_t.h>

#include <sprt/cxx/__type_traits/types.h>

namespace sprt {

using uint8_t = __SPRT_ID(uint8_t);
using uint16_t = __SPRT_ID(uint16_t);
using uint32_t = __SPRT_ID(uint32_t);
using uint64_t = __SPRT_ID(uint64_t);

using int8_t = __SPRT_ID(int8_t);
using int16_t = __SPRT_ID(int16_t);
using int32_t = __SPRT_ID(int32_t);
using int64_t = __SPRT_ID(int64_t);

using int_least8_t = __SPRT_ID(int_least8_t);
using int_least16_t = __SPRT_ID(int_least16_t);
using int_least32_t = __SPRT_ID(int_least32_t);
using int_least64_t = __SPRT_ID(int_least64_t);

using int_fast8_t = __SPRT_ID(int_fast8_t);
using int_fast16_t = __SPRT_ID(int_fast16_t);
using int_fast32_t = __SPRT_ID(int_fast32_t);
using int_fast64_t = __SPRT_ID(int_fast64_t);

using uint_least8_t = __SPRT_ID(uint_least8_t);
using uint_least16_t = __SPRT_ID(uint_least16_t);
using uint_least32_t = __SPRT_ID(uint_least32_t);
using uint_least64_t = __SPRT_ID(uint_least64_t);

using uint_fast8_t = __SPRT_ID(uint_fast8_t);
using uint_fast16_t = __SPRT_ID(uint_fast16_t);
using uint_fast32_t = __SPRT_ID(uint_fast32_t);
using uint_fast64_t = __SPRT_ID(uint_fast64_t);


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

template <typename T>
constexpr inline const int Digits = 0;

template <signed_or_unsigned_integer T>
constexpr inline const int Digits<T> =
		static_cast<int>(sizeof(T) * __CHAR_BIT__ - (is_signed_v<T> ? 1 : 0));

template <>
constexpr inline const int Digits<float> = __FLT_MANT_DIG__;

template <>
constexpr inline const int Digits<double> = __DBL_MANT_DIG__;

template <>
constexpr inline const int Digits<long double> = __LDBL_MANT_DIG__;


template <typename T>
constexpr inline const int Digits10 = 0;

template <signed_or_unsigned_integer T>
constexpr inline const int Digits10<T> = Digits<T> * 3 / 10;

template <>
constexpr inline const int Digits10<float> = __FLT_DIG__;

template <>
constexpr inline const int Digits10<double> = __DBL_DIG__;

template <>
constexpr inline const int Digits10<long double> = __LDBL_DIG__;


template <typename T>
constexpr inline const int MaxDigits10 = Digits10<T>;

template <>
constexpr inline const int MaxDigits10<float> = 2 + (Digits<float> * 30'103l) / 100'000l;

template <>
constexpr inline const int MaxDigits10<double> = 2 + (Digits<double> * 30'103l) / 100'000l;

template <>
constexpr inline const int MaxDigits10<long double> =
		2 + (Digits<long double> * 30'103l) / 100'000l;
;


template <typename T>
constexpr inline T MaxExp;

template <>
constexpr inline long double MaxExp<long double> = __SPRT_LDBL_MAX_EXP;

template <>
constexpr inline double MaxExp<double> = __SPRT_DBL_MAX_EXP;

template <>
constexpr inline float MaxExp<float> = __SPRT_FLT_MAX_EXP;


template <typename T>
constexpr inline T Infinity;

template <>
constexpr inline long double Infinity<long double> = __builtin_infl();

template <>
constexpr inline double Infinity<double> = __builtin_inf();

template <>
constexpr inline float Infinity<float> = __builtin_inff();


template <typename T>
constexpr inline T NaN;

template <>
constexpr inline long double NaN<long double> = __builtin_nanl("");

template <>
constexpr inline double NaN<double> = __builtin_nan("");

template <>
constexpr inline float NaN<float> = __builtin_nanf("");


template <typename T>
constexpr inline T Epsilon;

template <>
constexpr inline long double Epsilon<long double> = __SPRT_LDBL_EPSILON;

template <>
constexpr inline double Epsilon<double> = __SPRT_DBL_EPSILON;

template <>
constexpr inline float Epsilon<float> = __SPRT_FLT_EPSILON;

template <typename T>
constexpr inline T Smallest;

template <>
constexpr inline long double Smallest<long double> = __SPRT_LDBL_MIN;

template <>
constexpr inline double Smallest<double> = __SPRT_DBL_MIN;

template <>
constexpr inline float Smallest<float> = __SPRT_FLT_MIN;


template <typename T>
constexpr inline T Min;

template <>
constexpr inline long double Min<long double> = -__SPRT_LDBL_MAX;

template <>
constexpr inline double Min<double> = -__SPRT_LDBL_MAX;

template <>
constexpr inline const float Min<float> = -__SPRT_LDBL_MAX;

template <signed_integer T>
static constexpr inline const T Min<T> = T(T(1) << Digits<T>);

template <unsigned_integer T>
static constexpr inline const T Min<T> = T(0);


template <typename T>
constexpr inline T Max;

template <>
constexpr inline long double Max<long double> = __SPRT_LDBL_MAX;

template <>
constexpr inline double Max<double> = __SPRT_DBL_MAX;

template <>
constexpr inline float Max<float> = __SPRT_FLT_MAX;

template <signed_integer T>
static constexpr inline const T Max<T> = T(T(~0) ^ Min<T>);

template <unsigned_integer T>
static constexpr inline const T Max<T> = ~T(0);

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_CTYPES_H_
