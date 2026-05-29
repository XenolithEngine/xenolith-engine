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

#ifndef RUNTIME_INCLUDE_CXX___UTILITY_SWAP_H_
#define RUNTIME_INCLUDE_CXX___UTILITY_SWAP_H_

#include <sprt/cxx/__type_traits/operations.h>
#include <sprt/cxx/__type_traits/queries.h>
#include <sprt/cxx/__utility/common.h>

#if __SPRT_USE_STL
#include <utility>
#endif

namespace sprt {

/*
	swap
*/

#if __SPRT_USE_STL

using std::swap;

#else

template <typename Type>
using swap_result_t =
		enable_if_t<is_move_constructible<Type>::value && is_move_assignable<Type>::value>;

template <typename Type>
inline constexpr swap_result_t<Type> swap(Type &left, Type &right) noexcept {
	Type tmp(sprt::move_unsafe(left));
	left = sprt::move_unsafe(right);
	right = sprt::move_unsafe(tmp);
}

template <typename _Tp, typename _Up, typename = void>
inline const bool __is_swappable_with_v = false;

template <typename _Tp>
inline const bool __is_swappable_v = __is_swappable_with_v<_Tp &, _Tp &>;

template <typename _Tp, typename _Up, bool = __is_swappable_with_v<_Tp, _Up> >
inline const bool __is_nothrow_swappable_with_v = false;

template <typename _Tp>
inline const bool __is_nothrow_swappable_v = __is_nothrow_swappable_with_v<_Tp &, _Tp &>;

template <typename _Tp, typename _Up>
inline const bool __is_swappable_with_v<_Tp, _Up,
		void_t<decltype(swap(sprt::declval<_Tp>(), sprt::declval<_Up>())),
				decltype(swap(sprt::declval<_Up>(), sprt::declval<_Tp>()))> > = true;

template <typename _Tp, typename _Up>
inline const bool __is_nothrow_swappable_with_v<_Tp, _Up, true> =
		noexcept(swap(sprt::declval<_Tp>(), sprt::declval<_Up>()))
		&& noexcept(swap(sprt::declval<_Up>(), sprt::declval<_Tp>()));

template <typename _Tp, typename _Up>
inline constexpr bool is_swappable_with_v = __is_swappable_with_v<_Tp, _Up>;

template <typename _Tp, typename _Up>
struct is_swappable_with : bool_constant<is_swappable_with_v<_Tp, _Up>> { };

template <typename _Tp>
inline constexpr bool is_swappable_v =
		is_swappable_with_v<add_lvalue_reference_t<_Tp>, add_lvalue_reference_t<_Tp>>;

template <typename _Tp>
struct is_swappable : bool_constant<is_swappable_v<_Tp>> { };

template <typename _Tp, typename _Up>
inline constexpr bool is_nothrow_swappable_with_v = __is_nothrow_swappable_with_v<_Tp, _Up>;

template <typename _Tp, typename _Up>
struct is_nothrow_swappable_with : bool_constant<is_nothrow_swappable_with_v<_Tp, _Up>> { };

template <typename _Tp>
inline constexpr bool is_nothrow_swappable_v =
		is_nothrow_swappable_with_v<add_lvalue_reference_t<_Tp>, add_lvalue_reference_t<_Tp>>;

template <typename _Tp>
struct is_nothrow_swappable : bool_constant<is_nothrow_swappable_v<_Tp>> { };

#endif

} // namespace sprt

#endif // RUNTIME_INCLUDE_CXX___UTILITY_SWAP_H_
