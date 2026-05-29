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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_

#include <sprt/cxx/__functional/compare.h>

namespace sprt {

/*
	min
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr const Type &min(const Type &l, const Type &r, Compare comp) {
	return comp(r, l) ? r : l;
}

template <typename Type>
[[nodiscard]]
inline constexpr const Type &min(const Type &l, const Type &r) {
	return min(l, r, less<void>());
}


/*
	max
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr const Type &max(const Type &l, const Type &r, Compare comp) {
	return comp(l, r) ? r : l;
}

template <typename Type>
[[nodiscard]]
inline constexpr const Type &max(const Type &l, const Type &r) {
	return max(l, r, less<void>());
}

template <typename T, typename... Args>
constexpr const T &__vmax(const T &first, const Args &...args) {
	auto result = &first;
	((result = &sprt::max(*result, args)), ...); // Unary right fold over ','
	return *result;
}

template <typename T, typename... Args>
constexpr const T &__vmin(const T &first, const Args &...args) {
	auto result = &first;
	((result = &sprt::min(*result, args)), ...); // Unary right fold over ','
	return *result;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_
