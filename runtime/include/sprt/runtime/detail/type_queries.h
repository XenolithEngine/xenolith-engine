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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_QUERIES_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_QUERIES_H_

#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/runtime/detail/type_defaults.h>

namespace sprt {

template <typename TypeA, typename TypeB>
struct is_same {
	static constexpr bool value = __is_same(TypeA, TypeB);
};

template <typename TypeA, typename TypeB>
inline constexpr bool is_same_v = __is_same(TypeA, TypeB);


/*
is_convertable
*/

template <typename From, typename To>
struct is_convertible {
	static constexpr bool value = __is_convertible(From, To);
};

template <typename From, typename To>
inline constexpr bool is_convertible_v = __is_convertible(From, To);

template <typename From, typename To>
struct is_nothrow_convertible {
	static constexpr auto value = __is_nothrow_convertible(From, To);
};

template <typename From, typename To>
inline constexpr bool is_nothrow_convertible_v = __is_nothrow_convertible(From, To);


// [conv.general]/3 says "E is convertible to T" whenever "T t=E;" is well-formed.
// We can't test for that, but we can test implicit convertibility by passing it
// to a function. Notice that __is_core_convertible<void,void> is false,
// and __is_core_convertible<immovable-type,immovable-type> is true in C++17 and later.

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
is_base_of
*/

template <typename Base, typename Derived>
struct is_base_of {
	static constexpr auto value = __is_base_of(Base, Derived);
};

template <typename Base, typename Derived>
inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);


template <typename Type>
struct alignment_of : public integral_constant<__sprt_size_t, alignof(Type)> { };

template <typename Type>
inline constexpr __sprt_size_t alignment_of_v = alignof(Type);


template <typename Type>
struct rank : integral_constant<__sprt_size_t, __array_rank(Type)> { };

template <typename Type>
inline constexpr __sprt_size_t rank_v = rank<Type>::value;


template <typename Type, __sprt_size_t _Dim = 0>
struct extent : integral_constant<__sprt_size_t, __array_extent(Type, _Dim)> { };

template <typename Type, __sprt_size_t _Dim = 0>
inline constexpr __sprt_size_t extent_v = __array_extent(Type, _Dim);


template <typename Type>
struct decay {
	using type = __decay(Type);
};

template <typename Type>
using decay_t = __decay(Type);


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

template <typename... _Args>
struct common_type;

template <typename... _Args>
using common_type_t = typename common_type<_Args...>::type;

template <typename... _Args>
struct common_type : __builtin_common_type<common_type_t, type_identity, __empty, _Args...> { };


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

template <typename T>
struct underlying_type {
	using type = __underlying_type(T);
};

template <typename T>
using underlying_type_t = __underlying_type(T);

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_TYPE_QUERIES_H_
