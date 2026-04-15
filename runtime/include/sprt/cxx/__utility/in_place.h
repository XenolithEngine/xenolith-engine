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

#ifndef RUNTIME_INCLUDE_CXX___UTILITY_IN_PLACE_H_
#define RUNTIME_INCLUDE_CXX___UTILITY_IN_PLACE_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/__type_traits/constants.h>
#include <sprt/cxx/__type_traits/modifications.h>

namespace sprt {

struct in_place_t {
	explicit in_place_t() = default;
};
inline constexpr in_place_t in_place{};

template <typename _Tp>
struct in_place_type_t {
	explicit in_place_type_t() = default;
};
template <typename _Tp>
inline constexpr in_place_type_t<_Tp> in_place_type{};

template <size_t _Idx>
struct in_place_index_t {
	explicit in_place_index_t() = default;
};
template <size_t _Idx>
inline constexpr in_place_index_t<_Idx> in_place_index{};

template <typename _Tp>
struct is_inplace_type_imp : false_type { };
template <typename _Tp>
struct is_inplace_type_imp<in_place_type_t<_Tp>> : true_type { };

template <typename _Tp>
using is_inplace_type = is_inplace_type_imp<remove_cvref_t<_Tp>>;

template <typename _Tp>
struct is_inplace_index_imp : false_type { };
template <size_t _Idx>
struct is_inplace_index_imp<in_place_index_t<_Idx>> : true_type { };

template <typename _Tp>
using is_inplace_index = is_inplace_index_imp<remove_cvref_t<_Tp>>;

} // namespace sprt

#endif // RUNTIME_INCLUDE_CXX___UTILITY_IN_PLACE_H_
