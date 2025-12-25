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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMEINIT_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMEINIT_H_

/*
	Stappler Runtime - this is a minimal runtime library to support stappler

	This library implements the necessary functions based on the platform SDK,
	but should not depend on its headers (including the libc and libc++ headers)
*/

#include <c/bits/__sprt_def.h>

#ifdef SPRT_WINDOWS
#define SPRT_REF_SAFE_INSTANIATION 1
#else
#define SPRT_REF_SAFE_INSTANIATION 0
#endif

#define SPRT_UNUSED [[maybe_unused]]
#define SPRT_INLINE [[gnu::always_inline]]
#define SPRT_INLINE_LAMBDA __attribute__((always_inline))


#if defined(__GNUC__) && (__GNUC__ >= 4)
#define SPRT_NONNULL __attribute__((nonnull))
#elif defined(__has_attribute)
#if __has_attribute(nonnull)
#define SPRT_NONNULL __attribute__((nonnull))
#endif // __has_attribute(nonnull)
#else
#define SPRT_NONNULL
#endif


#if defined(__GNUC__) && (__GNUC__ >= 4)
#define SPRT_COVERAGE_TRIVIAL __attribute__ ((no_profile_instrument_function))
#elif defined(__has_attribute)
#define SPRT_COVERAGE_TRIVIAL
#else
#define SPRT_COVERAGE_TRIVIAL
#endif

namespace sprt {

using nullptr_t = decltype(nullptr);


template <typename _Tp, _Tp __v>
struct integral_constant {
	static inline constexpr const _Tp value = __v;
	typedef _Tp value_type;
	typedef integral_constant type;
	constexpr operator value_type() const noexcept { return value; }
	constexpr value_type operator()() const noexcept { return value; }
};

typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template <bool __b>
using bool_constant = integral_constant<bool, __b>;

template <typename _Tp, typename _Up>
using _IsSame = bool_constant<__is_same(_Tp, _Up)>;

template <typename _Tp, typename _Up>
using _IsNotSame = bool_constant<!__is_same(_Tp, _Up)>;


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
	remove_reference / remove_reference_t
*/

#if __has_builtin(__remove_reference_t)
template <typename T>
struct remove_reference {
	using type = __remove_reference_t(T);
};

template <typename T>
using remove_reference_t = __remove_reference_t(T);
#elif __has_builtin(__remove_reference)
template <typename T>
struct remove_reference {
	using type = __remove_reference(T);
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;
#else
#error "remove_reference not implemented!"
#endif // __has_builtin(__remove_reference_t)


/*
	remove_pointer / remove_pointer_t
*/

template <typename Type>
struct remove_pointer {
	using type = __remove_pointer(Type);
};

template <typename Type>
using remove_pointer_t = typename remove_pointer<Type>::type;


/*
	remove_const / remove_const_t
*/

#if __has_builtin(__remove_const)
template <typename Type>
struct remove_const {
	using type = __remove_const(Type);
};

template <typename Type>
using remove_const_t = __remove_const(Type);
#else
template <typename Type>
struct remove_const {
	using type = Type;
};
template <typename Type>
struct remove_const<const Type> {
	using type = Type;
};

template <typename Type>
using remove_const_t = typename remove_const<Type>::type;
#endif // __has_builtin(__remove_const)


/*
	is_reference / is_reference_v
*/

template <typename T>
struct is_reference {
	static constexpr auto value = __is_reference(T);
};

template <typename T>
inline constexpr bool is_reference_v = __is_reference(T);


/*
	is_lvalue_reference / is_rvalue_reference
*/

#if __has_builtin(__is_lvalue_reference) && __has_builtin(__is_rvalue_reference)

template <typename T>
struct is_lvalue_reference {
	static constexpr auto value = __is_lvalue_reference(T);
};

template <typename T>
struct is_rvalue_reference {
	static constexpr auto value = __is_rvalue_reference(T);
};

template <typename T>
inline constexpr bool is_lvalue_reference_v = __is_lvalue_reference(T);
template <typename T>
inline constexpr bool is_rvalue_reference_v = __is_rvalue_reference(T);

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


/*
	remove_cv / remove_cv_t
*/

template <typename Type>
struct remove_cv {
	using type = __remove_cv(Type);
};

template <typename Type>
using remove_cv_t = typename remove_cv<Type>::type;


/*
	enable_if / enable_if_t
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
	is_convertible / is_convertible_v
*/

template <typename From, typename To>
struct is_convertible {
	static constexpr auto value = __is_convertible(From, To);
};

template <typename From, typename To>
inline constexpr bool is_convertible_v = __is_convertible(From, To);


/*
	is_base_of / is_base_of_v
*/

template <typename Base, typename Derived>
struct is_base_of {
	static constexpr auto value = __is_base_of(Base, Derived);
};

template <typename Base, typename Derived>
inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);


/*
	less
*/

template <typename Type>
struct less {
	constexpr bool operator()(const Type &l, const Type &r) const noexcept { return l < r; }
};

template <>
struct less<void> {
	template <typename Type1, typename Type2>
	constexpr bool operator()(const Type1 &l, const Type2 &r) const noexcept {
		return l < r;
	}
};


/*
	is_same / is_same_v
*/

template <typename TypeA, typename TypeB>
struct is_same {
	static constexpr bool value = __is_same(TypeA, TypeB);
};

template <typename TypeA, typename TypeB>
inline constexpr bool is_same_v = __is_same(TypeA, TypeB);


/*
	is_pointer / is_pointer_v
*/

template <typename Type>
struct is_pointer {
	static constexpr bool value = __is_pointer(Type);
};

template <typename Type>
inline constexpr bool is_pointer_v = __is_pointer(Type);


/*
	forward
*/

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &__t) noexcept {
	return static_cast<Type &&>(__t);
}

template <typename Type>
[[nodiscard]]
SPRT_LOCAL inline constexpr Type &&forward(remove_reference_t<Type> &&__t) noexcept {
	static_assert(!is_lvalue_reference<Type>::value, "cannot forward an rvalue as an lvalue");
	return static_cast<Type &&>(__t);
}

/*
	move
*/

// Use sprt::move instead of std::move to enable additional diagnostics
template <typename Type,
		enable_if_t<
				// Attempt to move pointer is most likely an reference count error
				// Use move_unsafe if it's not an error (like, in template context)
				!is_pointer_v<remove_reference_t<Type>>, bool> = true>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}

// Behaves like std::move
template <typename Type>
[[nodiscard]]
constexpr typename remove_reference<Type>::type &&move_unsafe(Type &&value) noexcept {
	return static_cast<typename remove_reference<Type>::type &&>(value);
}


/*
	swap
*/

template <typename Type>
inline constexpr void swap(Type &left, Type &right) noexcept {
	Type tmp(move(left));
	left = move(right);
	right = move(tmp);
}


/*
	min
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr const Type &min(const Type &l, const Type &r, Compare comp) {
	return comp(r, l) ? r : l;
}

template <typename Type>
[[nodiscard]]
inline constexpr const Type &min(const Type &l, const Type &r) {
	return min(l, r, less<void>());
}


/*
	max
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr const Type &max(const Type &l, const Type &r, Compare comp) {
	return comp(l, r) ? r : l;
}

template <typename Type>
[[nodiscard]]
inline constexpr const Type &max(const Type &l, const Type &r) {
	return max(l, r, less<void>());
}


#ifndef __BYTE_ORDER__
#error "__BYTE_ORDER__ is not defined"
#endif


/*
	endian
*/

enum class endian {
	little = 0xDEAD,
	big = 0xFACE,
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	native = little
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	native = big
#else
	native = 0xCAFE
#endif
};

} // namespace sprt

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMEINIT_H_
