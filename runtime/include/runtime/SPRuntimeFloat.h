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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEFLOAT_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEFLOAT_H_

#include "SPRuntimeInit.h"

namespace sprt {

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

constexpr inline bool isinf(double value) { return __builtin_isinf(value); }

template <typename T>
constexpr inline T Infinity;

template <>
constexpr inline double Infinity<double> = __builtin_inf();

template <>
constexpr inline float Infinity<float> = __builtin_inf();

} // namespace sprt

#endif
