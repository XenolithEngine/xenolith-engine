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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTRUCTABLE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTRUCTABLE_H_

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


template < class _Tp, class... _Args>
struct is_nothrow_constructible
: integral_constant<bool, __is_nothrow_constructible(_Tp, _Args...)> { };

template <class _Tp, class... _Args>
inline constexpr bool is_nothrow_constructible_v = is_nothrow_constructible<_Tp, _Args...>::value;

template <class _Tp>
struct is_nothrow_copy_constructible
: integral_constant<bool, __is_nothrow_constructible(_Tp, add_lvalue_reference_t<const _Tp>)> { };

template <class _Tp>
inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<_Tp>::value;

template <class _Tp>
struct is_nothrow_move_constructible
: integral_constant<bool, __is_nothrow_constructible(_Tp, add_rvalue_reference_t<_Tp>)> { };

template <class _Tp>
inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<_Tp>::value;

template <class _Tp>
struct is_nothrow_default_constructible : integral_constant<bool, __is_nothrow_constructible(_Tp)> {
};

template <class _Tp>
inline constexpr bool is_nothrow_default_constructible_v = __is_nothrow_constructible(_Tp);


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


template <class _Tp, class _Arg>
struct is_nothrow_assignable : integral_constant<bool, __is_nothrow_assignable(_Tp, _Arg)> { };

template <class _Tp, class _Arg>
inline constexpr bool is_nothrow_assignable_v = __is_nothrow_assignable(_Tp, _Arg);

template <class _Tp>
struct is_nothrow_copy_assignable
: integral_constant<bool,
		  __is_nothrow_assignable(add_lvalue_reference_t<_Tp>, add_lvalue_reference_t<const _Tp>)> {
};

template <class _Tp>
inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<_Tp>::value;

template <class _Tp>
struct is_nothrow_move_assignable
: integral_constant<bool,
		  __is_nothrow_assignable(add_lvalue_reference_t<_Tp>, add_rvalue_reference_t<_Tp>)> { };

template <class _Tp>
inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<_Tp>::value;


template <class _Tp>
void __test_implicit_default_constructible(_Tp);

template <class _Tp, class = void, class = typename is_default_constructible<_Tp>::type>
struct __is_implicitly_default_constructible : false_type { };

template <class _Tp>
struct __is_implicitly_default_constructible<_Tp,
		decltype(sprt::__test_implicit_default_constructible<_Tp const &>({})), true_type>
: true_type { };

template <class _Tp>
struct __is_implicitly_default_constructible<_Tp,
		decltype(sprt::__test_implicit_default_constructible<_Tp const &>({})), false_type>
: false_type { };

// A type is replaceable if, with `x` and `y` being different objects, `x = std::move(y)` is equivalent to:
//
//  std::destroy_at(&x)
//  std::construct_at(&x, std::move(y))
//
// This allows turning a move-assignment into a sequence of destroy + move-construct, which
// is often more efficient. This is especially relevant when the move-construct is in fact
// part of a trivial relocation from somewhere else, in which case there is a huge win.
//
// Note that this requires language support in order to be really effective, but we
// currently emulate the base template with something very conservative.
template <class _Tp, class = void>
struct __is_replaceable : is_trivially_copyable<_Tp> { };

template <class _Tp>
struct __is_replaceable<_Tp, enable_if_t<is_same<_Tp, typename _Tp::__replaceable>::value> >
: true_type { };

template <class _Tp>
inline const bool __is_replaceable_v = __is_replaceable<_Tp>::value;


} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_CONSTRUCTABLE_H_
