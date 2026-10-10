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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_QUERIES_H_
#define RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_QUERIES_H_

#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/cxx/__type_traits/constants.h>

#if !__has_builtin(__is_same) || !__has_builtin(__is_convertible) \
		|| !__has_builtin(__is_nothrow_convertible) || !__has_builtin(__array_rank) \
		|| !__has_builtin(__array_extent) || !__has_builtin(__decay) \
		|| !__has_builtin(__underlying_type) \
		|| !__has_builtin(__has_unique_object_representations) \
		|| !__has_builtin(__builtin_common_type)
#include <sprt/cxx/__type_traits/__builtin_fallbacks.h>
#endif

namespace sprt {
inline namespace __cxx_type_traits {

// [conv.general]/3 says "E is convertible to T" whenever "T t=E;" is well-formed.
// We can't test for that, but we can test implicit convertibility by passing it
// to a function. Notice that __is_core_convertible<void,void> is false,
// and __is_core_convertible<immovable-type,immovable-type> is true in C++17 and later.
// Defined before is_convertible: its #else fallback branch is built on it.

template <typename _Tp, typename _Up, typename = void>
inline const bool __is_core_convertible_v = false;

template <typename _Tp, typename _Up>
inline const bool __is_core_convertible_v<_Tp, _Up,
		decltype(static_cast<void (*)(_Up)>(0)(static_cast<_Tp (*)()>(0)()))> = true;

template <typename _Tp, typename _Up>
using __is_core_convertible = integral_constant<bool, __is_core_convertible_v<_Tp, _Up> >;

template <typename _Tp, typename _Up, bool = __is_core_convertible_v<_Tp, _Up> >
inline const bool __is_nothrow_core_convertible_v = false;

template <typename _Tp, typename _Up>
inline const bool __is_nothrow_core_convertible_v<_Tp, _Up, true> =
		noexcept(static_cast<void (*)(_Up) noexcept>(0)(static_cast<_Tp (*)() noexcept>(0)()));


/*
is_same
*/

#if __has_builtin(__is_same)

template <typename TypeA, typename TypeB>
struct is_same : bool_constant<__is_same(TypeA, TypeB)> { };

template <typename TypeA, typename TypeB>
inline constexpr bool is_same_v = __is_same(TypeA, TypeB);

#else // __has_builtin(__is_same)

template <typename TypeA, typename TypeB>
struct is_same : bool_constant<__fb_is_same<TypeA, TypeB>::value> { };

template <typename TypeA, typename TypeB>
inline constexpr bool is_same_v = is_same<TypeA, TypeB>::value;

#endif // __has_builtin(__is_same)


/*
is_convertable
*/

// Fallback note: __is_core_convertible treats <void, void> as NOT convertible
// (the standard says it is); the fallback patches that case over it.
#if __has_builtin(__is_convertible)

template <typename From, typename To>
struct is_convertible : bool_constant<__is_convertible(From, To)> { };

template <typename From, typename To>
inline constexpr bool is_convertible_v = __is_convertible(From, To);

#else // __has_builtin(__is_convertible)

template <typename From, typename To>
struct is_convertible : bool_constant<__is_core_convertible_v<From, To>
								|| (is_same_v<From, To> && is_same_v<From, void>)> { };

template <typename From, typename To>
inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

#endif // __has_builtin(__is_convertible)

#if __has_builtin(__is_nothrow_convertible)

template <typename From, typename To>
struct is_nothrow_convertible : bool_constant<__is_nothrow_convertible(From, To)> { };

template <typename From, typename To>
inline constexpr bool is_nothrow_convertible_v = __is_nothrow_convertible(From, To);

#else // __has_builtin(__is_nothrow_convertible)

template <typename From, typename To>
struct is_nothrow_convertible : bool_constant<__is_nothrow_core_convertible_v<From, To>> { };

template <typename From, typename To>
inline constexpr bool is_nothrow_convertible_v = is_nothrow_convertible<From, To>::value;

#endif // __has_builtin(__is_nothrow_convertible)


/*
is_base_of
*/

template <typename Base, typename Derived>
struct is_base_of : bool_constant<__is_base_of(Base, Derived)> { };

template <typename Base, typename Derived>
inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);


template <typename Type>
struct alignment_of : public integral_constant<__sprt_size_t, alignof(Type)> { };

template <typename Type>
inline constexpr __sprt_size_t alignment_of_v = alignof(Type);


#if __has_builtin(__array_rank)

template <typename Type>
struct rank : integral_constant<__sprt_size_t, __array_rank(Type)> { };

template <typename Type>
inline constexpr __sprt_size_t rank_v = rank<Type>::value;

#else // __has_builtin(__array_rank)

template <typename Type>
struct rank : integral_constant<__sprt_size_t, __fb_array_rank<Type>::value> { };

template <typename Type>
inline constexpr __sprt_size_t rank_v = rank<Type>::value;

#endif // __has_builtin(__array_rank)

#if __has_builtin(__array_extent)

template <typename Type, __sprt_size_t _Dim = 0>
struct extent : integral_constant<__sprt_size_t, __array_extent(Type, _Dim)> { };

template <typename Type, __sprt_size_t _Dim = 0>
inline constexpr __sprt_size_t extent_v = __array_extent(Type, _Dim);

#else // __has_builtin(__array_extent)

template <typename Type, __sprt_size_t _Dim = 0>
struct extent : integral_constant<__sprt_size_t, __fb_array_extent<Type, _Dim>::value> { };

template <typename Type, __sprt_size_t _Dim = 0>
inline constexpr __sprt_size_t extent_v = extent<Type, _Dim>::value;

#endif // __has_builtin(__array_extent)


#if __has_builtin(__decay)

template <typename Type>
struct decay {
	using type = __decay(Type);
};

template <typename Type>
using decay_t = __decay(Type);

#else // __has_builtin(__decay)

template <typename Type>
struct decay {
	using type = typename __fb_decay<Type>::type;
};

template <typename Type>
using decay_t = typename decay<Type>::type;

#endif // __has_builtin(__decay)


/*
Conditional
*/

template <bool>
struct _IfImpl;

template <>
struct _IfImpl<true> {
	template <typename _IfRes, typename _ElseRes>
	using _Select = _IfRes;
};

template <>
struct _IfImpl<false> {
	template <typename _IfRes, typename _ElseRes>
	using _Select = _ElseRes;
};

template <bool _Cond, typename _IfRes, typename _ElseRes>
using _If = typename _IfImpl<_Cond>::template _Select<_IfRes, _ElseRes>;

template <bool _Bp, typename _If, typename _Then>
struct conditional {
	using type = _If;
};

template <typename _If, typename _Then>
struct conditional<false, _If, _Then> {
	using type = _Then;
};

template <bool _Bp, typename _IfRes, typename _ElseRes>
using conditional_t = typename conditional<_Bp, _IfRes, _ElseRes>::type;


/*
common_type
*/

#if __has_builtin(__builtin_common_type)

template <typename... _Args>
struct common_type;

template <typename... _Args>
using common_type_t = typename common_type<_Args...>::type;

template <typename... _Args>
struct common_type : __builtin_common_type<common_type_t, type_identity, __empty, _Args...> { };

#else // __has_builtin(__builtin_common_type)

// Classic pre-builtin common_type, the shape of libc++'s own fallback.
template <typename T>
typename __fb_add_rvalue_reference<T>::type __fb_declval(int);
template <typename T>
__nat __fb_declval(long);

template <typename T, typename U>
using __fb_cond_type = decltype(false ? __fb_declval<T>() : __fb_declval<U>());

template <typename T, typename U, typename = void>
struct __fb_common_type3 { };

// sub-bullet 4 - "if COND_RES(CREF(D1), CREF(D2)) denotes a type..."
template <typename T, typename U>
struct __fb_common_type3<T, U, void_t<__fb_cond_type<const T &, const U &>>> {
	using type = typename decay<__fb_cond_type<const T &, const U &>>::type;
};

template <typename T, typename U, typename = void>
struct __fb_common_type2_imp : __fb_common_type3<T, U> { };

// sub-bullet 3 - "if decay_t<decltype(false ? declval<D1>() : declval<D2>())> ..."
template <typename T, typename U>
struct __fb_common_type2_imp<T, U, void_t<decltype(true ? __fb_declval<T>() : __fb_declval<U>())>> {
	using type = typename decay<decltype(true ? __fb_declval<T>() : __fb_declval<U>())>::type;
};

template <typename, typename = void>
struct __fb_common_type_impl { };

template <typename... _Tp>
struct __fb_common_types;

template <typename... _Args>
struct common_type;

template <typename... _Args>
using common_type_t = typename common_type<_Args...>::type;

template <typename T, typename U>
struct __fb_common_type_impl<__fb_common_types<T, U>, void_t<typename common_type<T, U>::type>> {
	using type = typename common_type<T, U>::type;
};

template <typename T, typename U, typename V, typename... Rest>
struct __fb_common_type_impl<__fb_common_types<T, U, V, Rest...>,
		void_t<typename common_type<T, U>::type>>
: __fb_common_type_impl<__fb_common_types<typename common_type<T, U>::type, V, Rest...>> { };

// bullet 1 - sizeof...(Tp) == 0
template <>
struct common_type<> { };

// bullet 2 - sizeof...(Tp) == 1
template <typename T>
struct common_type<T> : common_type<T, T> { };

// bullet 3 - sizeof...(Tp) == 2
template <typename T, typename U>
struct common_type<T, U>
: __fb_conditional_t<is_same_v<T, decay_t<T>> && is_same_v<U, decay_t<U>>,
		  __fb_common_type2_imp<T, U>, common_type<decay_t<T>, decay_t<U>>> { };

// bullet 4 - sizeof...(Tp) > 2
template <typename T, typename U, typename V, typename... Rest>
struct common_type<T, U, V, Rest...> : __fb_common_type_impl<__fb_common_types<T, U, V, Rest...>> {
};

#endif // __has_builtin(__builtin_common_type)


/*
Enable_if
*/

template <bool, typename Type = void>
struct enable_if { };

template <typename Type>
struct enable_if<true, Type> {
	using type = Type;
};

template <bool Bool, typename Type = void>
using enable_if_t = typename enable_if<Bool, Type>::type;


/*
underlying_type
*/

// lcc does not provide __underlying_type; the fallback recovers it from the
// ABI contract (sizeof(enum) == sizeof(underlying), signedness from
// T(-1) < T(0)) — see __fb_underlying_type.
#if __has_builtin(__underlying_type)

template <typename T>
struct underlying_type {
	using type = __underlying_type(T);
};

template <typename T>
using underlying_type_t = __underlying_type(T);

#else // __has_builtin(__underlying_type)

template <typename T>
struct underlying_type {
	using type = typename __fb_underlying_type<T>::type;
};

template <typename T>
using underlying_type_t = typename underlying_type<T>::type;

#endif // __has_builtin(__underlying_type)

/*
has_unique_object_representations
*/

// Fallback is conservative (false): without compiler help padding bits cannot
// be ruled out, and consumers that skip hashing on true must not skip it
// wrongly.
#if __has_builtin(__has_unique_object_representations)

template <typename T>
struct has_unique_object_representations
: integral_constant<bool, __has_unique_object_representations(T)> { };

template <typename T>
inline constexpr bool has_unique_object_representations_v = __has_unique_object_representations(T);

#else // __has_builtin(__has_unique_object_representations)

template <typename T>
struct has_unique_object_representations
: integral_constant<bool, __fb_has_unique_object_representations<T>::value> { };

template <typename T>
inline constexpr bool has_unique_object_representations_v =
		has_unique_object_representations<T>::value;

#endif // __has_builtin(__has_unique_object_representations)


SPRT_LOCAL inline constexpr bool is_constant_evaluated() noexcept {
	return __builtin_is_constant_evaluated();
}

} // namespace __cxx_type_traits
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___TYPE_TRAITS_QUERIES_H_
