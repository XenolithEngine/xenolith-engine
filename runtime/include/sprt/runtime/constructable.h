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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_CONSTRUCTABLE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_CONSTRUCTABLE_H_

#include <sprt/runtime/init.h>

namespace sprt {

template <typename Type, typename... _Args>
struct is_constructible : integral_constant<bool, __is_constructible(Type, _Args...)> { };

template <typename Type, typename... _Args>
inline constexpr bool is_constructible_v = __is_constructible(Type, _Args...);

template <typename Type, typename... _Args>
struct is_trivially_constructible
: integral_constant<bool, __is_trivially_constructible(Type, _Args...)> { };

template <typename Type, typename... _Args>
inline constexpr bool is_trivially_constructible_v = __is_trivially_constructible(Type, _Args...);

template <typename Type>
struct is_trivially_copy_constructible
: integral_constant<bool, __is_trivially_constructible(Type, add_lvalue_reference_t<const Type>)> {
};

template <typename Type>
inline constexpr bool is_trivially_copy_constructible_v =
		is_trivially_copy_constructible<Type>::value;

template <typename Type>
struct is_trivially_move_constructible
: integral_constant<bool, __is_trivially_constructible(Type, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_trivially_move_constructible_v =
		is_trivially_move_constructible<Type>::value;

template <typename Type>
struct is_trivially_default_constructible
: integral_constant<bool, __is_trivially_constructible(Type)> { };

template <typename Type>
inline constexpr bool is_trivially_default_constructible_v = __is_trivially_constructible(Type);

template <typename Type>
struct is_destructible : bool_constant<__is_destructible(Type)> { };

template <typename Type>
inline constexpr bool is_destructible_v = __is_destructible(Type);

template <typename Type>
struct is_copy_constructible
: integral_constant<bool, __is_constructible(Type, add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<Type>::value;

template <typename Type>
struct is_move_constructible
: integral_constant<bool, __is_constructible(Type, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_move_constructible_v = is_move_constructible<Type>::value;

template <typename Type>
struct is_default_constructible : integral_constant<bool, __is_constructible(Type)> { };

template <typename Type>
inline constexpr bool is_default_constructible_v = __is_constructible(Type);

template <typename Type>
struct is_trivially_copyable : integral_constant<bool, __is_trivially_copyable(Type)> { };

template <typename Type>
inline constexpr bool is_trivially_copyable_v = __is_trivially_copyable(Type);


template <class _Tp, class _Up>
struct is_assignable : bool_constant<__is_assignable(_Tp, _Up)> { };

template <class _Tp, class _Arg>
inline constexpr bool is_assignable_v = __is_assignable(_Tp, _Arg);

template <class _Tp>
struct is_copy_assignable
: integral_constant<bool,
		  __is_assignable(add_lvalue_reference_t<_Tp>, add_lvalue_reference_t<const _Tp>)> { };

template <class _Tp>
inline constexpr bool is_copy_assignable_v = is_copy_assignable<_Tp>::value;

template <class _Tp>
struct is_move_assignable
: integral_constant<bool,
		  __is_assignable(add_lvalue_reference_t<_Tp>, add_rvalue_reference_t<_Tp>)> { };

template <class _Tp>
inline constexpr bool is_move_assignable_v = is_move_assignable<_Tp>::value;

} // namespace sprt

#endif
