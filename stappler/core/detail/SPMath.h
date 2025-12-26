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

#ifndef CORE_CORE_DETAIL_SPMATH_H_
#define CORE_CORE_DETAIL_SPMATH_H_

#include "SPPlatformInit.h"
#include <assert.h>
#include <stdlib.h>
#include <functional>
#include <limits>

// A part of SPCore.h, DO NOT include this directly

/*
 * 		Extra math functions
 */

namespace STAPPLER_VERSIONIZED stappler {

// numbers::pi replacement from std
namespace numbers {
template <typename T>
inline constexpr T pi_v =
		std::enable_if_t<std::is_floating_point_v<T>, T>(3.141592653589793238462643383279502884L);

inline constexpr double pi = pi_v<double>;

} // namespace numbers

constexpr long double operator""_to_rad(long double val) { return val * numbers::pi / 180.0; }
constexpr long double operator""_to_rad(unsigned long long int val) {
	return val * numbers::pi / 180.0;
}

template <typename T>
using NumericLimits = std::numeric_limits<T>;

template <typename T = float>
inline constexpr auto nan() -> T {
	return NumericLimits<T>::quiet_NaN();
}

template <typename T = float>
inline constexpr auto epsilon() -> T {
	return NumericLimits<T>::epsilon();
}

template <typename T>
inline auto isnan(T &&t) -> bool {
	return std::isnan(std::forward<T>(t));
}

template <class T>
inline constexpr T maxOf() {
	if constexpr (std::is_enum_v<T>) {
		return T(NumericLimits<std::underlying_type_t<T>>::max());
	} else {
		return NumericLimits<T>::max();
	}
}

template <class T>
inline constexpr T minOf() {
	if constexpr (std::is_enum_v<T>) {
		return T(NumericLimits<std::underlying_type_t<T>>::min());
	} else {
		return NumericLimits<T>::min();
	}
}

// For floats - use lowest: https://en.cppreference.com/w/cpp/types/numeric_limits/lowest
template <>
inline constexpr double minOf<double>() {
	return NumericLimits<double>::lowest();
}

template <>
inline constexpr float minOf<float>() {
	return NumericLimits<float>::lowest();
}

template <>
inline constexpr long double minOf<long double>() {
	return NumericLimits<long double>::lowest();
}

template <typename T, typename V>
struct HasMultiplication {
	template <class A, class B>
	static auto test(A *, B *) -> decltype(std::declval<A>() * std::declval<B>());

	template <typename, typename>
	static auto test(...) -> std::false_type;

	using type = typename std::is_same<T, decltype(test<T, V>(0, 0))>::type;
};

template <class T>
constexpr inline T progress(const T &a, const T &b, float p) {
	return (a * (1.0f - p) + b * p);
}

template <typename T>
auto StringToNumber(const char *ptr, char **tail, int base) -> T;

template <>
inline auto StringToNumber<unsigned int>(const char *ptr, char **tail, int base) -> unsigned int {
	if (ptr) {
		return (unsigned int)::strtoul(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<unsigned long>(const char *ptr, char **tail, int base) -> unsigned long {
	if (ptr) {
		return ::strtoul(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<unsigned long long>(const char *ptr, char **tail, int base)
		-> unsigned long long {
	if (ptr) {
		return ::strtoull(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<int>(const char *ptr, char **tail, int base) -> int {
	if (ptr) {
		return (int)::strtol(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<long>(const char *ptr, char **tail, int base) -> long {
	if (ptr) {
		return ::strtol(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<long long>(const char *ptr, char **tail, int base) -> long long {
	if (ptr) {
		return ::strtoll(ptr, tail, base);
	}
	return 0;
}

template <>
inline auto StringToNumber<float>(const char *ptr, char **tail, int base) -> float {
	if (ptr) {
		return ::strtof(ptr, tail);
	}
	return 0.0f;
}

template <>
inline auto StringToNumber<double>(const char *ptr, char **tail, int base) -> double {
	if (ptr) {
		return ::strtod(ptr, tail);
	}
	return 0.0;
}

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::math {

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
	return math::clamp(v, lo, hi, std::less<T>());
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
	return clamp_distance(v, lo, hi, std::less<T>(), z);
}

template <typename T>
constexpr inline T clamp_distance(const T &v, const T &lo, const T &hi) {
	return clamp_distance(v, lo, hi, std::less<T>(), T(0));
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

// convert degrees to radians
template <typename T>
constexpr auto to_rad(T val) -> T {
	return T(val) * numbers::pi_v<T> / T(180);
}

// convert radians to degrees
template <typename T>
constexpr auto to_deg(T val) -> T {
	return T(val) * T(180) / numbers::pi_v<T>;
}

} // namespace stappler::math

#endif /* CORE_CORE_DETAIL_SPMATH_H_ */
