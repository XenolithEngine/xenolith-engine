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

/*
 * Fallback implementations for the compiler type-trait builtins that a
 * compiler with a classic GCC builtin subset (MCST lcc: __is_class, __is_base_of,
 * the constructible/assignable/destructible family, __has_trivial_*) does not
 * provide: __is_same, __is_convertible, __remove_cv, __remove_reference,
 * __is_enum/__is_union/__is_integral/... , __decay, __array_rank/extent,
 * __underlying_type, __make_signed/unsigned.
 *
 * This header is included ONLY from the `#else` branches of
 * `#if __has_builtin(...)` guards in types.h / queries.h / modifications.h /
 * operations.h. Compilers that provide the builtins (clang, GCC, the SDK
 * clang) never parse it. Everything here is classic pre-builtin SFINAE, in
 * the __fb_ namespace so it can never collide with the public traits.
 *
 * Two results are approximations chosen for safety; as of this writing none
 * of them is consulted outside the traits layer itself:
 *   __fb_is_final, __fb_has_virtual_destructor,
 *   __fb_has_unique_object_representations  → false (the conservative
 *   direction for the noexcept/EBO decisions that usually gate on them).
 * __fb_underlying_type recovers the underlying type of an enum without
 * compiler help: sizeof(enum) equals sizeof(underlying) and the signedness
 * follows from comparing T(-1) with T(0).
 */

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS___BUILTIN_FALLBACKS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS___BUILTIN_FALLBACKS_H_

#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/cxx/__type_traits/constants.h>

namespace sprt {
inline namespace __cxx_type_traits {

template <bool __c, typename _If, typename _Then>
struct __fb_conditional { using type = _If; };

template <typename _If, typename _Then>
struct __fb_conditional<false, _If, _Then> { using type = _Then; };

template <bool __c, typename _If, typename _Then>
using __fb_conditional_t = typename __fb_conditional<__c, _If, _Then>::type;

/*
 * Primitive modifications first: the predicates below consult them.
 */

template <typename T>
struct __fb_remove_reference { using type = T; };
template <typename T>
struct __fb_remove_reference<T &> { using type = T; };
template <typename T>
struct __fb_remove_reference<T &&> { using type = T; };

template <typename T>
struct __fb_remove_const { using type = T; };
template <typename T>
struct __fb_remove_const<const T> { using type = T; };

template <typename T>
struct __fb_remove_volatile { using type = T; };
template <typename T>
struct __fb_remove_volatile<volatile T> { using type = T; };

template <typename T>
struct __fb_remove_cv {
	using type = typename __fb_remove_volatile<typename __fb_remove_const<T>::type>::type;
};

template <typename T>
struct __fb_remove_cvref {
	using type = typename __fb_remove_cv<typename __fb_remove_reference<T>::type>::type;
};

template <typename A, typename B>
struct __fb_is_same { static constexpr bool value = false; };
template <typename T>
struct __fb_is_same<T, T> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_const { static constexpr bool value = false; };
template <typename T>
struct __fb_is_const<const T> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_volatile { static constexpr bool value = false; };
template <typename T>
struct __fb_is_volatile<volatile T> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_lvalue_reference { static constexpr bool value = false; };
template <typename T>
struct __fb_is_lvalue_reference<T &> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_rvalue_reference { static constexpr bool value = false; };
template <typename T>
struct __fb_is_rvalue_reference<T &&> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_reference {
	static constexpr bool value =
			__fb_is_lvalue_reference<T>::value || __fb_is_rvalue_reference<T>::value;
};

template <typename T>
struct __fb_is_array { static constexpr bool value = false; };
template <typename T>
struct __fb_is_array<T[]> { static constexpr bool value = true; };
template <typename T, __sprt_size_t N>
struct __fb_is_array<T[N]> { static constexpr bool value = true; };

template <typename T>
struct __fb_is_pointer { static constexpr bool value = false; };
template <typename T>
struct __fb_is_pointer<T *> { static constexpr bool value = true; };

// A function type is exactly the type that cannot be made const.
template <typename T>
struct __fb_is_function {
	static constexpr bool value = !__fb_is_const<const T>::value;
};

template <typename T>
struct __fb_is_void {
	static constexpr bool value = __fb_is_same<typename __fb_remove_cv<T>::type, void>::value;
};

template <typename T>
struct __fb_is_null_pointer {
	static constexpr bool value = __fb_is_same<typename __fb_remove_cv<T>::type, nullptr_t>::value;
};

/*
 * Integral / floating point / fundamental classification
 */

template <typename T>
struct __fb_is_integral {
	using U = typename __fb_remove_cv<T>::type;
	static constexpr bool value =
			__fb_is_same<U, bool>::value || __fb_is_same<U, char>::value
			|| __fb_is_same<U, signed char>::value || __fb_is_same<U, unsigned char>::value
			|| __fb_is_same<U, wchar_t>::value || __fb_is_same<U, short>::value
			|| __fb_is_same<U, unsigned short>::value || __fb_is_same<U, int>::value
			|| __fb_is_same<U, unsigned int>::value || __fb_is_same<U, long>::value
			|| __fb_is_same<U, unsigned long>::value || __fb_is_same<U, long long>::value
			|| __fb_is_same<U, unsigned long long>::value
#ifdef __SIZEOF_INT128__
			|| __fb_is_same<U, __int128>::value || __fb_is_same<U, unsigned __int128>::value
#endif
#if defined(__cpp_char8_t)
			|| __fb_is_same<U, char8_t>::value
#endif
			|| __fb_is_same<U, char16_t>::value || __fb_is_same<U, char32_t>::value;
};

template <typename T>
struct __fb_is_floating_point {
	using U = typename __fb_remove_cv<T>::type;
	static constexpr bool value = __fb_is_same<U, float>::value
			|| __fb_is_same<U, double>::value || __fb_is_same<U, long double>::value;
};

template <typename T>
struct __fb_is_arithmetic {
	static constexpr bool value =
			__fb_is_integral<T>::value || __fb_is_floating_point<T>::value;
};

template <typename T>
struct __fb_is_fundamental {
	static constexpr bool value = __fb_is_arithmetic<T>::value || __fb_is_void<T>::value
			|| __fb_is_null_pointer<T>::value;
};

template <typename T>
struct __fb_is_compound {
	static constexpr bool value = !__fb_is_fundamental<T>::value;
};

/*
 * signedness (guarded: the comparison must only instantiate on arithmetic)
 */

template <typename T, bool = __fb_is_arithmetic<T>::value>
struct __fb_is_signed { static constexpr bool value = false; };
template <typename T>
struct __fb_is_signed<T, true> {
	static constexpr bool value = static_cast<T>(-1) < static_cast<T>(0);
};

template <typename T, bool = __fb_is_arithmetic<T>::value>
struct __fb_is_unsigned { static constexpr bool value = false; };
template <typename T>
struct __fb_is_unsigned<T, true> {
	static constexpr bool value = !__fb_is_signed<T, true>::value;
};

/*
 * class / union / enum / member pointers
 */

// `int T::*` is well-formed exactly for class and union types.
template <typename T>
struct __fb_is_class_or_union {
	template <typename U>
	static char __test(int U::*);
	template <typename>
	static long __test(...);
	static constexpr bool value = sizeof(__test<T>(0)) == 1;
};

// Requires the classic __is_class builtin (lcc provides it; the #else branch
// that reaches this header is taken on compilers in its subset).
template <typename T>
struct __fb_is_class {
	static constexpr bool value = __is_class(T);
};

template <typename T>
struct __fb_is_union {
	static constexpr bool value =
			__fb_is_class_or_union<T>::value && !__is_class(T);
};

// Member pointer classification via overload-resolution SFINAE: lcc's
// partial-specialization matching is not consistent for member-pointer
// patterns, but the function-overload form works. NB: cv-qualified member
// pointers (int A::* const) do not match; no STL consumer consults the
// traits for such types.
template <typename T>
T &&__fb_declv(int);
template <typename T>
__nat __fb_declv(long);

template <typename T>
struct __fb_is_member_function_pointer {
// Overload-resolution SFINAE in the full cv/ref/noexcept matrix: lcc's
// partial-specialization matching silently misses cv/ref-qualified member
// function pointer patterns, the function-overload form does not.
// clang-format off
#define __SPRT_FB_MFN_TEST(__cv, __ref, __noex) \
	template <typename R, typename C, typename... A> \
	static char __test(R (C::*)(A...) __cv __ref __noex);
__SPRT_FB_MFN_TEST(, , )
__SPRT_FB_MFN_TEST(const, , )
__SPRT_FB_MFN_TEST(volatile, , )
__SPRT_FB_MFN_TEST(const volatile, , )
__SPRT_FB_MFN_TEST(, &, )
__SPRT_FB_MFN_TEST(const, &, )
__SPRT_FB_MFN_TEST(volatile, &, )
__SPRT_FB_MFN_TEST(const volatile, &, )
__SPRT_FB_MFN_TEST(, &&, )
__SPRT_FB_MFN_TEST(const, &&, )
__SPRT_FB_MFN_TEST(volatile, &&, )
__SPRT_FB_MFN_TEST(const volatile, &&, )
__SPRT_FB_MFN_TEST(, , noexcept)
__SPRT_FB_MFN_TEST(const, , noexcept)
__SPRT_FB_MFN_TEST(volatile, , noexcept)
__SPRT_FB_MFN_TEST(const volatile, , noexcept)
__SPRT_FB_MFN_TEST(, &, noexcept)
__SPRT_FB_MFN_TEST(const, &, noexcept)
__SPRT_FB_MFN_TEST(volatile, &, noexcept)
__SPRT_FB_MFN_TEST(const volatile, &, noexcept)
__SPRT_FB_MFN_TEST(, &&, noexcept)
__SPRT_FB_MFN_TEST(const, &&, noexcept)
__SPRT_FB_MFN_TEST(volatile, &&, noexcept)
__SPRT_FB_MFN_TEST(const volatile, &&, noexcept)
#undef __SPRT_FB_MFN_TEST
// clang-format on
	static long __test(...);
	static constexpr bool value = sizeof(__test(__fb_declv<T>(0))) == 1;
};

template <typename T>
struct __fb_is_member_object_pointer {
	template <typename T2, typename C>
	static char __test(T2 C::*);
	static long __test(...);
	static constexpr bool value = sizeof(__test(__fb_declv<T>(0))) == 1
			&& !__fb_is_member_function_pointer<T>::value;
};

template <typename T>
struct __fb_is_member_pointer {
	static constexpr bool value =
			__fb_is_member_object_pointer<T>::value || __fb_is_member_function_pointer<T>::value;
};

template <typename T>
struct __fb_is_enum {
	static constexpr bool value = !__fb_is_class_or_union<T>::value && !__is_class(T)
			&& !__fb_is_fundamental<T>::value && !__fb_is_pointer<T>::value
			&& !__fb_is_member_pointer<T>::value && !__fb_is_reference<T>::value
			&& !__fb_is_function<T>::value && !__fb_is_array<T>::value;
};

/*
 * object / scalar
 */

template <typename T>
struct __fb_is_scalar {
	static constexpr bool value = __fb_is_arithmetic<T>::value || __fb_is_enum<T>::value
			|| __fb_is_pointer<T>::value || __fb_is_member_pointer<T>::value;
};

template <typename T>
struct __fb_is_object {
	static constexpr bool value = __fb_is_scalar<T>::value || __fb_is_array<T>::value
			|| __fb_is_union<T>::value || __is_class(T);
};

/*
 * polymorphic / abstract; conservative approximations
 */

template <typename T>
struct __fb_is_polymorphic {
	template <typename U>
	static true_type __test(decltype(dynamic_cast<const volatile void *>(
			static_cast<const volatile U *>(nullptr)))*);
	template <typename>
	static false_type __test(...);
	static constexpr bool value = decltype(__test<T>(nullptr))::value;
};

// An abstract type cannot form an array.
template <typename T>
struct __fb_is_abstract {
	template <typename U>
	static long __test(U (*)[1]);
	template <typename>
	static char __test(...);
	static constexpr bool value = sizeof(__test<T>(0)) == 1;
};

// No portable fallback exists and no consumer reads these today; false is the
// conservative direction for the noexcept / EBO decisions that gate on them.
template <typename T>
struct __fb_is_final { static constexpr bool value = false; };

template <typename T>
struct __fb_has_virtual_destructor { static constexpr bool value = false; };

template <typename T>
struct __fb_has_unique_object_representations { static constexpr bool value = false; };

/*
 * referenceable / add_reference / add_pointer / remove_pointer
 */

// Referenceable = a reference to T can be formed: everything but void and
// the already-reference types. Function types ARE referenceable.
template <typename T>
struct __fb_is_referenceable {
	static constexpr bool value = !__fb_is_void<T>::value && !__fb_is_reference<T>::value;
};

template <typename T, bool = __fb_is_referenceable<T>::value>
struct __fb_add_lvalue_reference { using type = T &; };
template <typename T>
struct __fb_add_lvalue_reference<T, false> { using type = T; };

template <typename T, bool = __fb_is_referenceable<T>::value>
struct __fb_add_rvalue_reference { using type = T &&; };
template <typename T>
struct __fb_add_rvalue_reference<T, false> { using type = T; };

template <typename T, bool = __fb_is_referenceable<T>::value>
struct __fb_add_pointer { using type = typename __fb_remove_reference<T>::type *; };
template <typename T>
struct __fb_add_pointer<T, false> { using type = T; };

template <typename T>
struct __fb_remove_pointer { using type = T; };
template <typename T>
struct __fb_remove_pointer<T *> { using type = T; };
template <typename T>
struct __fb_remove_pointer<T * const> { using type = T; };
template <typename T>
struct __fb_remove_pointer<T * volatile> { using type = T; };
template <typename T>
struct __fb_remove_pointer<T * const volatile> { using type = T; };
template <typename T, typename C>
struct __fb_remove_pointer<T C::*> { using type = T; };

/*
 * extents, rank, decay
 */

template <typename T>
struct __fb_remove_extent { using type = T; };
template <typename T>
struct __fb_remove_extent<T[]> { using type = T; };
template <typename T, __sprt_size_t N>
struct __fb_remove_extent<T[N]> { using type = T; };

template <typename T>
struct __fb_remove_all_extents { using type = typename __fb_remove_cv<T>::type; };
template <typename T>
struct __fb_remove_all_extents<T[]> { using type = typename __fb_remove_all_extents<T>::type; };
template <typename T, __sprt_size_t N>
struct __fb_remove_all_extents<T[N]> { using type = typename __fb_remove_all_extents<T>::type; };

template <typename T>
struct __fb_array_rank : integral_constant<__sprt_size_t, 0> { };
template <typename T>
struct __fb_array_rank<T[]> : integral_constant<__sprt_size_t, __fb_array_rank<T>::value + 1> { };
template <typename T, __sprt_size_t N>
struct __fb_array_rank<T[N]> : integral_constant<__sprt_size_t, __fb_array_rank<T>::value + 1> { };

template <typename T, __sprt_size_t D = 0>
struct __fb_array_extent : integral_constant<__sprt_size_t, 0> { };
template <typename T, __sprt_size_t D>
struct __fb_array_extent<T[], D> : integral_constant<__sprt_size_t, 0> { };

// The bool parameter gates the recursion: the D - 1 step instantiates only
// when D > 0 (a conditional-by-type would evaluate both branches eagerly and
// recurse past zero).
template <typename T, __sprt_size_t N, __sprt_size_t D, bool = (D == 0)>
struct __fb_array_extent_dim;
template <typename T, __sprt_size_t N, __sprt_size_t D>
struct __fb_array_extent_dim<T, N, D, true> : integral_constant<__sprt_size_t, N> { };
template <typename T, __sprt_size_t N, __sprt_size_t D>
struct __fb_array_extent_dim<T, N, D, false> : __fb_array_extent<T, D - 1> { };

template <typename T, __sprt_size_t N, __sprt_size_t D>
struct __fb_array_extent<T[N], D> : __fb_array_extent_dim<T, N, D, D == 0> { };

template <typename T>
struct __fb_decay {
	using U = typename __fb_remove_reference<T>::type;
	using type = typename __fb_conditional<__fb_is_function<U>::value,
			typename __fb_add_pointer<U>::type,
			typename __fb_conditional<__fb_is_array<U>::value,
					typename __fb_add_pointer<typename __fb_remove_extent<U>::type>::type,
					typename __fb_remove_cv<U>::type>::type>::type;
};

/*
 * make_signed / make_unsigned / underlying_type — sized-sign tables
 */

template <bool __s, unsigned __w>
struct __fb_pick_signed;
template <> struct __fb_pick_signed<true, 1> { using type = signed char; };
template <> struct __fb_pick_signed<false, 1> { using type = unsigned char; };
template <> struct __fb_pick_signed<true, 2> { using type = short; };
template <> struct __fb_pick_signed<false, 2> { using type = unsigned short; };
template <> struct __fb_pick_signed<true, 4> { using type = int; };
template <> struct __fb_pick_signed<false, 4> { using type = unsigned int; };
template <> struct __fb_pick_signed<true, 8> { using type = long long; };
template <> struct __fb_pick_signed<false, 8> { using type = unsigned long long; };

template <typename T, bool = __fb_is_integral<T>::value>
struct __fb_make_signed { using type = T; };
template <typename T>
struct __fb_make_signed<T, true> {
	using U = typename __fb_remove_cv<T>::type;
	static_assert(sizeof(U) <= 8, "unsupported integer size");
	using type = typename __fb_pick_signed<true, sizeof(U)>::type;
};

template <typename T, bool = __fb_is_integral<T>::value>
struct __fb_make_unsigned { using type = T; };
template <typename T>
struct __fb_make_unsigned<T, true> {
	using type = typename __fb_pick_signed<false, sizeof(typename __fb_remove_cv<T>::type)>::type;
};

// The underlying type of an enum recovered from the ABI contract: the enum's
// size equals the size of its underlying type, and the signedness follows
// from T(-1) < T(0). Covers every integral type the ABI can pick.
template <typename T>
struct __fb_underlying_type {
	static constexpr bool __s = static_cast<T>(-1) < static_cast<T>(0);
	using type = typename __fb_pick_signed<__s, sizeof(T)>::type;
};

} // namespace __cxx_type_traits
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS___BUILTIN_FALLBACKS_H_
