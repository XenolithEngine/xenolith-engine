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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_FUNCTIONAL_H_
#define RUNTIME_INCLUDE_SPRT_CXX_FUNCTIONAL_H_

#include <sprt/runtime/init.h>

namespace sprt {

template <typename Type, typename _Up, typename = void>
struct __is_equality_comparable : false_type { };

template <typename Type, typename _Up>
struct __is_equality_comparable<Type, _Up, void_t<decltype(declval<Type>() == declval<_Up>())> >
: true_type { };

template <typename Type, typename _Up, typename = void>
struct is_trivially_equality_comparable_impl : false_type { };

template <typename Type>
struct is_trivially_equality_comparable_impl<Type, Type>
#if __has_builtin(__is_trivially_equality_comparable)
: integral_constant<bool,
		  __is_trivially_equality_comparable(Type) && __is_equality_comparable<Type, Type>::value> {
};
#else
: is_integral<Type> {
};
#endif // __has_builtin(__is_trivially_equality_comparable)

template <typename Type, typename _Up>
struct is_trivially_equality_comparable_impl< Type, _Up,
		enable_if_t<is_integral<Type>::value && is_integral<_Up>::value
				&& !is_same<Type, _Up>::value && is_signed<Type>::value == is_signed<_Up>::value
				&& sizeof(Type) == sizeof(_Up)> > : true_type { };

template <typename Type>
struct is_trivially_equality_comparable_impl<Type *, Type *> : true_type { };

template <typename Type, typename _Up>
struct is_trivially_equality_comparable_impl<Type *, _Up *>
: integral_constant< bool,
		  __is_equality_comparable<Type *, _Up *>::value
				  && (is_same<remove_cv_t<Type>, remove_cv_t<_Up> >::value || is_void<Type>::value
						  || is_void<_Up>::value)> { };

template <typename Type, typename _Up>
using is_trivially_equality_comparable =
		is_trivially_equality_comparable_impl<remove_cv_t<Type>, remove_cv_t<_Up> >;


// Comparison operations

template <typename Type = void>
struct equal_to {
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x == __y; }
};

template <>
struct equal_to<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) == sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) == sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

template <typename Type = void>
struct not_equal_to {
	typedef bool __result_type; // used by valarray
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x != __y; }
};

template <>
struct not_equal_to<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) != sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) != sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

template <typename Type>
struct less {
	typedef bool __result_type; // used by valarray
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x < __y; }
};

template <>
struct less<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) < sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) < sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

template <typename Type = void>
struct less_equal {
	typedef bool __result_type; // used by valarray
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x <= __y; }
};

template <>
struct less_equal<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) <= sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) <= sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

template <typename Type = void>
struct greater_equal {
	typedef bool __result_type; // used by valarray
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x >= __y; }
};

template <>
struct greater_equal<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) >= sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) >= sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

template <typename Type = void>
struct greater {
	typedef bool __result_type; // used by valarray
	constexpr bool operator()(const Type &__x, const Type &__y) const { return __x > __y; }
};

template <>
struct greater<void> {
	template <typename Type1, typename Type2>
	constexpr auto operator()(Type1 &&__t, Type2 &&__u) const noexcept
			-> decltype(sprt::forward<Type1>(__t) > sprt::forward<Type2>(__u)) {
		return sprt::forward<Type1>(__t) > sprt::forward<Type2>(__u);
	}
	using is_transparent = void;
};

} // namespace sprt

#endif
