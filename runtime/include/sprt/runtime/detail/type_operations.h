/**
 Copyright (c) 2026 Xenolith Team Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_OPERATIONS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_OPERATIONS_H_

#include <sprt/runtime/detail/type_modifications.h>

namespace sprt {

/*
Args-constructable
*/

template <typename Type, typename... _Args>
struct is_constructible : integral_constant<bool, __is_constructible(Type, _Args...)> { };

template <typename Type, typename... _Args>
inline constexpr bool is_constructible_v = __is_constructible(Type, _Args...);

template <typename Type, typename... _Args>
struct is_trivially_constructible
: integral_constant<bool, __is_trivially_constructible(Type, _Args...)> { };

template <typename Type, typename... _Args>
inline constexpr bool is_trivially_constructible_v = __is_trivially_constructible(Type, _Args...);

template <typename Type, class... _Args>
struct is_nothrow_constructible
: integral_constant<bool, __is_nothrow_constructible(Type, _Args...)> { };

template <typename Type, class... _Args>
inline constexpr bool is_nothrow_constructible_v = __is_nothrow_constructible(Type, _Args...);


/*
Default-constructable
*/

template <typename Type>
struct is_default_constructible : integral_constant<bool, __is_constructible(Type)> { };

template <typename Type>
inline constexpr bool is_default_constructible_v = __is_constructible(Type);

template <typename Type>
struct is_trivially_default_constructible
: integral_constant<bool, __is_trivially_constructible(Type)> { };

template <typename Type>
inline constexpr bool is_trivially_default_constructible_v = __is_trivially_constructible(Type);

template <typename Type>
struct is_nothrow_default_constructible
: integral_constant<bool, __is_nothrow_constructible(Type)> { };

template <typename Type>
inline constexpr bool is_nothrow_default_constructible_v = __is_nothrow_constructible(Type);


/*
Copy-constructable
*/

template <typename Type>
struct is_copy_constructible
: integral_constant<bool, __is_constructible(Type, add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_copy_constructible_v = is_copy_constructible<Type>::value;

template <typename Type>
struct is_trivially_copy_constructible
: integral_constant<bool, __is_trivially_constructible(Type, add_lvalue_reference_t<const Type>)> {
};

template <typename Type>
inline constexpr bool is_trivially_copy_constructible_v =
		is_trivially_copy_constructible<Type>::value;

template <typename Type>
struct is_nothrow_copy_constructible
: integral_constant<bool, __is_nothrow_constructible(Type, add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<Type>::value;


/*
Move-constructable
*/

template <typename Type>
struct is_move_constructible
: integral_constant<bool, __is_constructible(Type, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_move_constructible_v = is_move_constructible<Type>::value;

template <typename Type>
struct is_trivially_move_constructible
: integral_constant<bool, __is_trivially_constructible(Type, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_trivially_move_constructible_v =
		is_trivially_move_constructible<Type>::value;

template <typename Type>
struct is_nothrow_move_constructible
: integral_constant<bool, __is_nothrow_constructible(Type, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<Type>::value;


/*
Assignable
*/

template <typename Type, typename Arg>
struct is_assignable : bool_constant<__is_assignable(Type, Arg)> { };

template <typename Type, typename Arg>
inline constexpr bool is_assignable_v = __is_assignable(Type, Arg);

template <typename Type, typename Arg>
struct is_trivially_assignable : integral_constant<bool, __is_trivially_assignable(Type, Arg)> { };

template <typename Type, typename Arg>
inline constexpr bool is_trivially_assignable_v = __is_trivially_assignable(Type, Arg);

template <typename Type, typename Arg>
struct is_nothrow_assignable : integral_constant<bool, __is_nothrow_assignable(Type, Arg)> { };

template <typename Type, typename Arg>
inline constexpr bool is_nothrow_assignable_v = __is_nothrow_assignable(Type, Arg);


/*
Copy-assignable
*/

template <typename Type>
struct is_copy_assignable
: integral_constant<bool,
		  __is_assignable(add_lvalue_reference_t<Type>, add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_copy_assignable_v = is_copy_assignable<Type>::value;

template <typename Type>
struct is_trivially_copy_assignable
: integral_constant<bool,
		  __is_trivially_assignable(add_lvalue_reference_t<Type>,
				  add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<Type>::value;

template <typename Type>
struct is_nothrow_copy_assignable : integral_constant<bool,
											__is_nothrow_assignable(add_lvalue_reference_t<Type>,
													add_lvalue_reference_t<const Type>)> { };

template <typename Type>
inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<Type>::value;


/*
Move-assignable
*/

template <typename Type>
struct is_move_assignable
: integral_constant<bool,
		  __is_assignable(add_lvalue_reference_t<Type>, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_move_assignable_v = is_move_assignable<Type>::value;

template <typename Type>
struct is_trivially_move_assignable
: integral_constant<bool,
		  __is_trivially_assignable(add_lvalue_reference_t<Type>, add_rvalue_reference_t<Type>)> {
};

template <typename Type>
inline constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<Type>::value;

template <typename Type>
struct is_nothrow_move_assignable
: integral_constant<bool,
		  __is_nothrow_assignable(add_lvalue_reference_t<Type>, add_rvalue_reference_t<Type>)> { };

template <typename Type>
inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<Type>::value;


/*
Destructable
*/

template <typename Type>
struct is_destructible : bool_constant<__is_destructible(Type)> { };

template <typename Type>
inline constexpr bool is_destructible_v = __is_destructible(Type);

template <typename Type>
struct is_trivially_destructible : bool_constant<__is_trivially_destructible(Type)> { };

template <typename Type>
inline constexpr bool is_trivially_destructible_v = __is_trivially_destructible(Type);

template <typename Type>
struct is_nothrow_destructible : bool_constant<__is_nothrow_destructible(Type)> { };

template <typename Type>
inline constexpr bool is_nothrow_destructible_v = __is_nothrow_destructible(Type);


template <typename Type>
struct has_virtual_destructor : public integral_constant<bool, __has_virtual_destructor(Type)> { };


template <typename Type>
void __test_implicit_default_constructible(Type);

template <typename Type, class = void, class = typename is_default_constructible<Type>::type>
struct __is_implicitly_default_constructible : false_type { };

template <typename Type>
struct __is_implicitly_default_constructible<Type,
		decltype(sprt::__test_implicit_default_constructible<Type const &>({})), true_type>
: true_type { };

template <typename Type>
struct __is_implicitly_default_constructible<Type,
		decltype(sprt::__test_implicit_default_constructible<Type const &>({})), false_type>
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

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_OPERATIONS_H_
