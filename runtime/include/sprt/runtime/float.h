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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_FLOAT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_FLOAT_H_

#include <sprt/runtime/init.h>
#include <sprt/c/__sprt_float.h>

namespace sprt {

template <typename T>
struct _is_floating_point : false_type { };

template <>
struct _is_floating_point<float> : true_type { };

template <>
struct _is_floating_point<double> : true_type { };

template <>
struct _is_floating_point<long double> : true_type { };

template <typename T>
struct is_floating_point : _is_floating_point<remove_cv_t<T> > { };

template <class _Tp>
inline constexpr bool is_floating_point_v = is_floating_point<_Tp>::value;

constexpr inline bool isnan(float value) {
#if __has_builtin(__builtin_isnan)
	return __builtin_isnan(value);
#else
	return value != value;
#endif
}

constexpr inline bool isnan(double value) {
#if __has_builtin(__builtin_isnan)
	return __builtin_isnan(value);
#else
	return value != value;
#endif
}

constexpr inline bool isnan(long double value) {
#if __has_builtin(__builtin_isnan)
	return __builtin_isnan(value);
#else
	return value != value;
#endif
}

constexpr inline bool isinf(double value) { return __builtin_isinf(value); }

} // namespace sprt

// numbers::pi replacement from std
namespace sprt::numbers {

template <typename T = float>
inline constexpr T Pi =
		enable_if_t<is_floating_point_v<T>, T>(3.141592653589793238462643383279502884L);

} // namespace sprt::numbers

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_FLOAT_H_
