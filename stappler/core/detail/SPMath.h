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
#include <sprt/runtime/math.h>
#include <assert.h>
#include <stdlib.h>
#include <functional>

// A part of SPCore.h, DO NOT include this directly

/*
 * 		Extra math functions
 */

namespace STAPPLER_VERSIONIZED stappler {

constexpr long double operator""_to_rad(long double val) {
	return val * sprt::numbers::Pi<long double> / 180.0;
}
constexpr long double operator""_to_rad(unsigned long long int val) {
	return val * sprt::numbers::Pi<long double> / 180.0;
}

template <typename T = float>
inline constexpr auto nan() -> T {
	return sprt::NaN<T>;
}

template <typename T = float>
inline constexpr auto epsilon() -> T {
	return sprt::Epsilon<T>;
}

template <class T>
inline constexpr T maxOf() {
	return sprt::Max<T>;
}

template <class T>
inline constexpr T minOf() {
	return sprt::Min<T>;
}

template <typename T, typename V>
using HasMultiplication = sprt::HasMultiplication<T, V>;

using sprt::progress;

using sprt::StringToNumber;

namespace math = sprt::math;

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* CORE_CORE_DETAIL_SPMATH_H_ */
