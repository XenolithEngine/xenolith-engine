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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_CATEGORIES_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_CATEGORIES_H_

#include <sprt/runtime/detail/type_defaults.h>

namespace sprt {

template <typename Type>
struct is_void {
	static constexpr bool value = __is_same(__remove_cv(Type), void);
};

template <typename Type>
inline constexpr bool is_void_v = __is_same(__remove_cv(Type), void);


template <typename Type>
struct is_null_pointer {
	static constexpr bool value = __is_same(__remove_cv(Type), nullptr_t);
};

template <typename Type>
inline constexpr bool is_null_pointer_v = is_null_pointer<Type>::value;

__SPRT_STL_BOOL_BUILTIN(is_array)
__SPRT_STL_BOOL_BUILTIN(is_pointer)
__SPRT_STL_BOOL_BUILTIN(is_enum)
__SPRT_STL_BOOL_BUILTIN(is_union)
__SPRT_STL_BOOL_BUILTIN(is_class)
__SPRT_STL_BOOL_BUILTIN(is_function)
__SPRT_STL_BOOL_BUILTIN(is_reference)
__SPRT_STL_BOOL_BUILTIN(is_object)
__SPRT_STL_BOOL_BUILTIN(is_scalar)
__SPRT_STL_BOOL_BUILTIN(is_compound)
__SPRT_STL_BOOL_BUILTIN(is_integral)
__SPRT_STL_BOOL_BUILTIN(is_floating_point)
__SPRT_STL_BOOL_BUILTIN(is_fundamental)
__SPRT_STL_BOOL_BUILTIN(is_arithmetic)


#if __has_builtin(__is_lvalue_reference) && __has_builtin(__is_rvalue_reference)

__SPRT_STL_BOOL_BUILTIN(is_lvalue_reference)
__SPRT_STL_BOOL_BUILTIN(is_rvalue_reference)

#else // __has_builtin(__is_lvalue_reference)

template <typename T>
struct is_lvalue_reference {
	static constexpr auto value = false;
};
template <typename T>
struct is_lvalue_reference<T &> {
	static constexpr auto value = true;
};

template <typename T>
struct is_rvalue_reference {
	static constexpr auto value = false;
};
template <typename T>
struct is_rvalue_reference<T &&> {
	static constexpr auto value = true;
};

template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

#endif // __has_builtin(__is_lvalue_reference)

__SPRT_STL_BOOL_BUILTIN(is_member_pointer)
__SPRT_STL_BOOL_BUILTIN(is_member_object_pointer)
__SPRT_STL_BOOL_BUILTIN(is_member_function_pointer)
__SPRT_STL_BOOL_BUILTIN(is_const)
__SPRT_STL_BOOL_BUILTIN(is_volatile)
__SPRT_STL_BOOL_BUILTIN(is_empty)
__SPRT_STL_BOOL_BUILTIN(is_polymorphic)
__SPRT_STL_BOOL_BUILTIN(is_final)
__SPRT_STL_BOOL_BUILTIN(is_abstract)
__SPRT_STL_BOOL_BUILTIN(is_aggregate)
__SPRT_STL_BOOL_BUILTIN(is_trivial)
__SPRT_STL_BOOL_BUILTIN(is_trivially_copyable)
__SPRT_STL_BOOL_BUILTIN(is_standard_layout)
__SPRT_STL_BOOL_BUILTIN(is_literal_type)
__SPRT_STL_BOOL_BUILTIN(is_signed)
__SPRT_STL_BOOL_BUILTIN(is_unsigned)

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_CATEGORIES_H_
