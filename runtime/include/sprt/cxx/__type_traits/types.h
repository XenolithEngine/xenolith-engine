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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_TYPES_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_TYPES_H_

#include <sprt/cxx/__type_traits/queries.h>
#include <sprt/cxx/__type_traits/constants.h>

#if !__has_builtin(__is_same) || !__has_builtin(__remove_cv) || !__has_builtin(__is_array) \
		|| !__has_builtin(__is_pointer) || !__has_builtin(__is_enum) || !__has_builtin(__is_union) \
		|| !__has_builtin(__is_function) || !__has_builtin(__is_reference) \
		|| !__has_builtin(__is_object) || !__has_builtin(__is_scalar) \
		|| !__has_builtin(__is_compound) || !__has_builtin(__is_integral) \
		|| !__has_builtin(__is_floating_point) || !__has_builtin(__is_fundamental) \
		|| !__has_builtin(__is_arithmetic) || !__has_builtin(__is_member_pointer) \
		|| !__has_builtin(__is_member_object_pointer) \
		|| !__has_builtin(__is_member_function_pointer) || !__has_builtin(__is_const) \
		|| !__has_builtin(__is_volatile) || !__has_builtin(__is_polymorphic) \
		|| !__has_builtin(__is_final) || !__has_builtin(__is_abstract) \
		|| !__has_builtin(__is_signed) || !__has_builtin(__is_unsigned)
#include <sprt/cxx/__type_traits/__builtin_fallbacks.h>
#endif

#define __SPRT_STL_BOOL_BUILTIN(Name) \
	template <typename Type> \
	struct Name : bool_constant<__##Name(Type)> { }; \
	template <typename Type> \
	inline constexpr bool Name##_v = __##Name(Type);

namespace sprt {
inline namespace __cxx_type_traits {

#if __has_builtin(__is_same) && __has_builtin(__remove_cv)

template <typename Type>
struct is_void : bool_constant<__is_same(__remove_cv(Type), void)> { };

template <typename Type>
inline constexpr bool is_void_v = __is_same(__remove_cv(Type), void);

template <typename Type>
struct is_null_pointer : bool_constant<__is_same(__remove_cv(Type), nullptr_t)> { };

template <typename Type>
inline constexpr bool is_null_pointer_v = is_null_pointer<Type>::value;

#else // __has_builtin(__is_same)

template <typename Type>
struct is_void : __fb_is_void<Type> { };

template <typename Type>
inline constexpr bool is_void_v = is_void<Type>::value;

template <typename Type>
struct is_null_pointer : __fb_is_null_pointer<Type> { };

template <typename Type>
inline constexpr bool is_null_pointer_v = is_null_pointer<Type>::value;

#endif // __has_builtin(__is_same)

#if __has_builtin(__is_array)
__SPRT_STL_BOOL_BUILTIN(is_array)
#else // __has_builtin(__is_array)

template <typename Type>
struct is_array : __fb_is_array<Type> { };

template <typename Type>
inline constexpr bool is_array_v = is_array<Type>::value;

#endif // __has_builtin(__is_array)
#if __has_builtin(__is_pointer)
__SPRT_STL_BOOL_BUILTIN(is_pointer)
#else // __has_builtin(__is_pointer)

template <typename Type>
struct is_pointer : __fb_is_pointer<Type> { };

template <typename Type>
inline constexpr bool is_pointer_v = is_pointer<Type>::value;

#endif // __has_builtin(__is_pointer)
#if __has_builtin(__is_enum)
__SPRT_STL_BOOL_BUILTIN(is_enum)
#else // __has_builtin(__is_enum)

template <typename Type>
struct is_enum : __fb_is_enum<Type> { };

template <typename Type>
inline constexpr bool is_enum_v = is_enum<Type>::value;

#endif // __has_builtin(__is_enum)
#if __has_builtin(__is_union)
__SPRT_STL_BOOL_BUILTIN(is_union)
#else // __has_builtin(__is_union)

template <typename Type>
struct is_union : __fb_is_union<Type> { };

template <typename Type>
inline constexpr bool is_union_v = is_union<Type>::value;

#endif // __has_builtin(__is_union)
__SPRT_STL_BOOL_BUILTIN(is_class)
#if __has_builtin(__is_function)
__SPRT_STL_BOOL_BUILTIN(is_function)
#else // __has_builtin(__is_function)

template <typename Type>
struct is_function : __fb_is_function<Type> { };

template <typename Type>
inline constexpr bool is_function_v = is_function<Type>::value;

#endif // __has_builtin(__is_function)
#if __has_builtin(__is_reference)
__SPRT_STL_BOOL_BUILTIN(is_reference)
#else // __has_builtin(__is_reference)

template <typename Type>
struct is_reference : __fb_is_reference<Type> { };

template <typename Type>
inline constexpr bool is_reference_v = is_reference<Type>::value;

#endif // __has_builtin(__is_reference)
#if __has_builtin(__is_object)
__SPRT_STL_BOOL_BUILTIN(is_object)
#else // __has_builtin(__is_object)

template <typename Type>
struct is_object : __fb_is_object<Type> { };

template <typename Type>
inline constexpr bool is_object_v = is_object<Type>::value;

#endif // __has_builtin(__is_object)
#if __has_builtin(__is_scalar)
__SPRT_STL_BOOL_BUILTIN(is_scalar)
#else // __has_builtin(__is_scalar)

template <typename Type>
struct is_scalar : __fb_is_scalar<Type> { };

template <typename Type>
inline constexpr bool is_scalar_v = is_scalar<Type>::value;

#endif // __has_builtin(__is_scalar)
#if __has_builtin(__is_compound)
__SPRT_STL_BOOL_BUILTIN(is_compound)
#else // __has_builtin(__is_compound)

template <typename Type>
struct is_compound : __fb_is_compound<Type> { };

template <typename Type>
inline constexpr bool is_compound_v = is_compound<Type>::value;

#endif // __has_builtin(__is_compound)
#if __has_builtin(__is_integral)
__SPRT_STL_BOOL_BUILTIN(is_integral)
#else // __has_builtin(__is_integral)

template <typename Type>
struct is_integral : __fb_is_integral<Type> { };

template <typename Type>
inline constexpr bool is_integral_v = is_integral<Type>::value;

#endif // __has_builtin(__is_integral)
#if __has_builtin(__is_floating_point)
__SPRT_STL_BOOL_BUILTIN(is_floating_point)
#else // __has_builtin(__is_floating_point)

template <typename Type>
struct is_floating_point : __fb_is_floating_point<Type> { };

template <typename Type>
inline constexpr bool is_floating_point_v = is_floating_point<Type>::value;

#endif // __has_builtin(__is_floating_point)
#if __has_builtin(__is_fundamental)
__SPRT_STL_BOOL_BUILTIN(is_fundamental)
#else // __has_builtin(__is_fundamental)

template <typename Type>
struct is_fundamental : __fb_is_fundamental<Type> { };

template <typename Type>
inline constexpr bool is_fundamental_v = is_fundamental<Type>::value;

#endif // __has_builtin(__is_fundamental)
#if __has_builtin(__is_arithmetic)
__SPRT_STL_BOOL_BUILTIN(is_arithmetic)
#else // __has_builtin(__is_arithmetic)

template <typename Type>
struct is_arithmetic : __fb_is_arithmetic<Type> { };

template <typename Type>
inline constexpr bool is_arithmetic_v = is_arithmetic<Type>::value;

#endif // __has_builtin(__is_arithmetic)


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

#if __has_builtin(__is_member_pointer)
__SPRT_STL_BOOL_BUILTIN(is_member_pointer)
#else // __has_builtin(__is_member_pointer)

template <typename Type>
struct is_member_pointer : __fb_is_member_pointer<Type> { };

template <typename Type>
inline constexpr bool is_member_pointer_v = is_member_pointer<Type>::value;

#endif // __has_builtin(__is_member_pointer)
#if __has_builtin(__is_member_object_pointer)
__SPRT_STL_BOOL_BUILTIN(is_member_object_pointer)
#else // __has_builtin(__is_member_object_pointer)

template <typename Type>
struct is_member_object_pointer : __fb_is_member_object_pointer<Type> { };

template <typename Type>
inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<Type>::value;

#endif // __has_builtin(__is_member_object_pointer)
#if __has_builtin(__is_member_function_pointer)
__SPRT_STL_BOOL_BUILTIN(is_member_function_pointer)
#else // __has_builtin(__is_member_function_pointer)

template <typename Type>
struct is_member_function_pointer : __fb_is_member_function_pointer<Type> { };

template <typename Type>
inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<Type>::value;

#endif // __has_builtin(__is_member_function_pointer)
#if __has_builtin(__is_const)
__SPRT_STL_BOOL_BUILTIN(is_const)
#else // __has_builtin(__is_const)

template <typename Type>
struct is_const : __fb_is_const<Type> { };

template <typename Type>
inline constexpr bool is_const_v = is_const<Type>::value;

#endif // __has_builtin(__is_const)
#if __has_builtin(__is_volatile)
__SPRT_STL_BOOL_BUILTIN(is_volatile)
#else // __has_builtin(__is_volatile)

template <typename Type>
struct is_volatile : __fb_is_volatile<Type> { };

template <typename Type>
inline constexpr bool is_volatile_v = is_volatile<Type>::value;

#endif // __has_builtin(__is_volatile)
__SPRT_STL_BOOL_BUILTIN(is_empty)
#if __has_builtin(__is_polymorphic)
__SPRT_STL_BOOL_BUILTIN(is_polymorphic)
#else // __has_builtin(__is_polymorphic)

template <typename Type>
struct is_polymorphic : __fb_is_polymorphic<Type> { };

template <typename Type>
inline constexpr bool is_polymorphic_v = is_polymorphic<Type>::value;

#endif // __has_builtin(__is_polymorphic)
#if __has_builtin(__is_final)
__SPRT_STL_BOOL_BUILTIN(is_final)
#else // __has_builtin(__is_final)

template <typename Type>
struct is_final : __fb_is_final<Type> { };

template <typename Type>
inline constexpr bool is_final_v = is_final<Type>::value;

#endif // __has_builtin(__is_final)
#if __has_builtin(__is_abstract)
__SPRT_STL_BOOL_BUILTIN(is_abstract)
#else // __has_builtin(__is_abstract)

template <typename Type>
struct is_abstract : __fb_is_abstract<Type> { };

template <typename Type>
inline constexpr bool is_abstract_v = is_abstract<Type>::value;

#endif // __has_builtin(__is_abstract)
__SPRT_STL_BOOL_BUILTIN(is_aggregate)
__SPRT_STL_BOOL_BUILTIN(is_trivial)
__SPRT_STL_BOOL_BUILTIN(is_pod) // deprecated in C++20, still required by the tests
__SPRT_STL_BOOL_BUILTIN(is_trivially_copyable)
__SPRT_STL_BOOL_BUILTIN(is_standard_layout)
__SPRT_STL_BOOL_BUILTIN(is_literal_type)
#if __has_builtin(__is_signed)
__SPRT_STL_BOOL_BUILTIN(is_signed)
#else // __has_builtin(__is_signed)

template <typename Type>
struct is_signed : __fb_is_signed<Type> { };

template <typename Type>
inline constexpr bool is_signed_v = is_signed<Type>::value;

#endif // __has_builtin(__is_signed)
#if __has_builtin(__is_unsigned)
__SPRT_STL_BOOL_BUILTIN(is_unsigned)
#else // __has_builtin(__is_unsigned)

template <typename Type>
struct is_unsigned : __fb_is_unsigned<Type> { };

template <typename Type>
inline constexpr bool is_unsigned_v = is_unsigned<Type>::value;

#endif // __has_builtin(__is_unsigned)

template <typename _Tp>
concept signed_integer = is_signed_v<_Tp> && !is_floating_point_v<_Tp>;

template <typename _Tp>
concept unsigned_integer = is_unsigned_v<_Tp> && !is_floating_point_v<_Tp>;

template <typename _Tp>
concept floating_point = is_floating_point_v<_Tp>;

template <typename _Tp>
concept signed_or_unsigned_integer = signed_integer<_Tp> || unsigned_integer<_Tp>;

template <typename _Tp>
concept io_character = is_same_v<_Tp, char> || is_same_v<_Tp, char16_t> || is_same_v<_Tp, char32_t>;

template <typename _Tp>
concept enumeration = is_enum_v<_Tp>;

} // namespace __cxx_type_traits
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_TYPES_H_
