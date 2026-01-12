/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MATH_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MATH_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/float.h>
#include <sprt/runtime/detail/operations.h>
#include <sprt/c/__sprt_stdlib.h>

/*
 * 		Extra math functions
 */

namespace sprt {

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
constexpr inline T Max;

template <>
constexpr inline long double Max<long double> = __SPRT_LDBL_MAX;

template <>
constexpr inline double Max<double> = __SPRT_DBL_MAX;

template <>
constexpr inline float Max<float> = __SPRT_FLT_MAX;

template <>
inline constexpr uint8_t Max<uint8_t> = __SPRT_UINT8_MAX;

template <>
inline constexpr uint16_t Max<uint16_t> = __SPRT_UINT16_MAX;

template <>
inline constexpr uint32_t Max<uint32_t> = __SPRT_UINT32_MAX;

template <>
inline constexpr uint64_t Max<uint64_t> = __SPRT_UINT64_MAX;

template <>
inline constexpr uint8_t Max<int8_t> = __SPRT_INT8_MAX;

template <>
inline constexpr uint16_t Max<int16_t> = __SPRT_INT16_MAX;

template <>
inline constexpr uint32_t Max<int32_t> = __SPRT_INT32_MAX;

template <>
inline constexpr uint64_t Max<int64_t> = __SPRT_INT64_MAX;


template <typename T>
constexpr inline T Min;

template <>
constexpr inline long double Min<long double> = -__SPRT_LDBL_MAX;

template <>
constexpr inline double Min<double> = -__SPRT_DBL_MAX;

template <>
constexpr inline float Min<float> = -__SPRT_FLT_MAX;

template <>
inline constexpr uint8_t Min<uint8_t> = 0;

template <>
inline constexpr uint16_t Min<uint16_t> = 0;

template <>
inline constexpr uint32_t Min<uint32_t> = 0;

template <>
inline constexpr uint64_t Min<uint64_t> = 0;

template <>
inline constexpr uint8_t Min<int8_t> = __SPRT_INT8_C(-1) - __SPRT_INT8_C(__SPRT_INT8_MAX);

template <>
inline constexpr uint16_t Min<int16_t> = __SPRT_INT16_C(-1) - __SPRT_INT16_C(__SPRT_INT16_MAX);

template <>
inline constexpr uint32_t Min<int32_t> = __SPRT_INT32_C(-1) - __SPRT_INT32_C(__SPRT_INT32_MAX);

template <>
inline constexpr uint64_t Min<int64_t> = __SPRT_INT64_C(-1) - __SPRT_INT64_C(__SPRT_INT64_MAX);

template <signed_or_unsigned_integer T>
static constexpr const int Digits =
		static_cast<int>(sizeof(T) * __CHAR_BIT__ - (is_signed_integer_v<T> ? 1 : 0));

template <signed_or_unsigned_integer T>
static constexpr const int Digits10 = Digits<T> * 3 / 10;

template <typename T, typename V>
struct HasMultiplication {
	template <typename A, typename B>
	static auto test(A *, B *) -> decltype(declval<A>() * declval<B>());

	template <typename, typename>
	static auto test(...) -> false_type;

	using type = typename is_same<T, decltype(test<T, V>(0, 0))>::type;
};

template <typename T>
requires requires(T x, float v) { x * v; }
constexpr inline T progress(const T &a, const T &b, float p) {
	return (a * (1.0f - p) + b * p);
}

template <typename T>
requires requires(T x, float v) { T::progress(x, x, v); } && (!requires(T x, float v) { x * v; })
constexpr inline T progress(const T &a, const T &b, float p) {
	return T::progress(a, b, p);
}

template <typename Type>
constexpr int __countl_zero(Type __t) noexcept {
	static_assert(is_unsigned_integer_v<Type>, "__countl_zero requires an unsigned integer type");
	return __builtin_clzg(__t, Digits<Type>);
}

template <unsigned_integer Type>
[[nodiscard]]
constexpr int countl_zero(Type __t) noexcept {
	return sprt::__countl_zero(__t);
}

template <unsigned_integer Type>
[[nodiscard]]
constexpr int countl_one(Type __t) noexcept {
	return __t != Max<Type> ? sprt::countl_zero(static_cast<Type>(~__t)) : Digits<Type>;
}

template <typename Type>
constexpr int __countr_zero(Type __t) noexcept {
	static_assert(is_unsigned_integer_v<Type>, "__countr_zero only works with unsigned types");
	return __builtin_ctzg(__t, Digits<Type>);
}

template <unsigned_integer Type>
[[nodiscard]]
constexpr int countr_zero(Type __t) noexcept {
	return sprt::__countr_zero(__t);
}

template <unsigned_integer Type>
[[nodiscard]]
constexpr int countr_one(Type __t) noexcept {
	return __t != Max<Type> ? sprt::countr_zero(static_cast<Type>(~__t)) : Digits<Type>;
}


template <class _Tp>
[[nodiscard]]
constexpr int __popcount(_Tp __t) noexcept {
	static_assert(is_unsigned_integer_v<_Tp>, "__popcount only works with unsigned types");
	return __builtin_popcountg(__t);
}

template <unsigned_integer _Tp>
[[nodiscard]]
constexpr int popcount(_Tp __t) noexcept {
	return sprt::__popcount(__t);
}


template <typename T>
auto StringToNumber(const char *ptr, char **tail, int base) -> T;

template <>
inline auto StringToNumber<unsigned int>(const char *ptr, char **tail, int base) -> unsigned int {
	if (ptr) {
		return (unsigned int)__sprt_strtoul(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<unsigned long>(const char *ptr, char **tail, int base) -> unsigned long {
	if (ptr) {
		return __sprt_strtoul(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<unsigned long long>(const char *ptr, char **tail, int base)
		-> unsigned long long {
	if (ptr) {
		return __sprt_strtoull(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<int>(const char *ptr, char **tail, int base) -> int {
	if (ptr) {
		return (int)__sprt_strtol(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<long>(const char *ptr, char **tail, int base) -> long {
	if (ptr) {
		return __sprt_strtol(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<long long>(const char *ptr, char **tail, int base) -> long long {
	if (ptr) {
		return __sprt_strtoll(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<float>(const char *ptr, char **tail, int base) -> float {
	if (ptr) {
		return __sprt_strtof(ptr, tail);
	}
	return 0.0f;
}

template <>
inline auto StringToNumber<double>(const char *ptr, char **tail, int base) -> double {
	if (ptr) {
		return __sprt_strtod(ptr, tail);
	}
	return 0.0;
}

} // namespace sprt

namespace sprt::math {

constexpr float MATH_FLOAT_SMALL = 1.0e-37f;
constexpr float MATH_TOLERANCE = 2e-37f;

/**
 * Updates this vector towards the given target using a smoothing function.
 * The given response time determines the amount of smoothing (lag). A longer
 * response time yields a smoother result and more lag. To force this vector to
 * follow the target closely, provide a response time that is very small relative
 * to the given elapsed time. */

// avoid constexpr to support SIMD-based implementation
template <typename T>
constexpr inline T smooth(const T &source, const T &target, float elapsed, float response) {
	if (elapsed > 0) {
		return source + (target - source) * (elapsed / (elapsed + response));
	}
	return source;
}

// avoid constexpr to support SIMD-based implementation
template <typename T, typename V = float>
constexpr inline T lerp(const T &a, const T &b, const V &alpha) {
	return (a * (-alpha + 1.0f) + b * alpha);
}

template <typename T, typename Compare>
constexpr inline const T &clamp(const T &v, const T &lo, const T &hi, Compare comp) {
	if (comp(hi, lo)) {
		return comp(v, hi) ? hi : comp(lo, v) ? lo : v;
	} else {
		return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
	}
}

template <typename T>
constexpr inline const T &clamp(const T &v, const T &lo, const T &hi) {
	return math::clamp(v, lo, hi, sprt::less<T>());
}

template <typename T, typename Compare>
constexpr inline T clamp_distance(const T &v, const T &lo, const T &hi, Compare comp, const T &z) {
	assert(!comp(hi, lo));
	return comp(v, lo) ? (lo - v) : comp(hi, v) ? (v - hi) : z;
}

template <typename T, typename Compare>
constexpr inline T clamp_distance(const T &v, const T &lo, const T &hi, Compare comp) {
	return clamp_distance(v, lo, hi, comp, T(0));
}

template <typename T>
constexpr inline T clamp_distance(const T &v, const T &lo, const T &hi, const T &z) {
	return clamp_distance(v, lo, hi, less<T>(), z);
}

template <typename T>
constexpr inline T clamp_distance(const T &v, const T &lo, const T &hi) {
	return clamp_distance(v, lo, hi, less<T>(), T(0));
}

template <typename T>
constexpr inline T smoothstep(const T &edge0, const T &edge1, const T &x) {
	auto t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

template <typename T>
constexpr inline T add_cyclic(const T &value, const T &increment, const T &lo, const T &hi) {
	auto cycle = (hi - lo + T(1));
	auto incr = increment % cycle;
	auto tmp = value + incr;
	if (tmp > hi) {
		return tmp - cycle;
	} else {
		return tmp;
	}
}

template <typename T>
constexpr inline T sub_cyclic(const T &value, const T &decrement, const T &lo, const T &hi) {
	auto cycle = (hi - lo + T(1));
	auto decr = decrement % cycle;
	if (value < lo + decr) {
		return value + cycle - decr;
	} else {
		return value - decr;
	}
}

// next power of two
inline uint32_t npot(uint32_t n) {
	--n;

	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return n + 1;
}

inline uint64_t npot(uint64_t n) {
	--n;

	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;

	return n + 1;
}

// Align on a power of 2 boundary
template <typename T = uint64_t>
static constexpr auto align(T size, T boundary) -> T {
	return (((size) + ((boundary)-1)) & ~((boundary)-1));
}

/*
	Given a pointer ptr to a buffer of size space, returns a pointer aligned by
	the specified alignment for size number of bytes and decreases space argument
	by the number of bytes used for alignment. The first aligned address is returned.

	The function modifies the pointer only if it would be possible to fit the wanted
	number of bytes aligned by the given alignment into the buffer. If the buffer is
	too small, the function does nothing and returns nullptr. 
*/
template <typename T = size_t>
static constexpr void *align(T alignment, T size, void *&ptr, T &space) {
	void *r = nullptr;
	if (size <= space) {
		char *p1 = static_cast<char *>(ptr);
		char *p2 = reinterpret_cast<char *>(
				reinterpret_cast<uintptr_t>(p1 + (alignment - 1)) & -alignment);
		T d = static_cast<T>(p2 - p1);
		if (d <= space - size) {
			r = p2;
			ptr = r;
			space -= d;
		}
	}
	return r;
}


// convert degrees to radians
template <typename T>
constexpr auto to_rad(T val) -> T {
	return T(val) * numbers::Pi<T> / T(180);
}

// convert radians to degrees
template <typename T>
constexpr auto to_deg(T val) -> T {
	return T(val) * T(180) / numbers::Pi<T>;
}

} // namespace sprt::math

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_MATH_H_ */
