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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_COMPARE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_COMPARE_H_

#include <sprt/cxx/__utility/common.h>

namespace sprt {

struct identity {
	template <typename _Tp>
	[[nodiscard]]
	constexpr _Tp &&operator()(_Tp &&__t) const noexcept {
		return sprt::forward<_Tp>(__t);
	}

	using is_transparent = void;
};

template <typename _Tp>
struct is_identity : false_type { };

template <>
struct is_identity<identity> : true_type { };

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

template <typename _Tp, typename _Key = void, typename = void>
inline const bool __is_transparent_v = false;

template <typename _Tp, typename _Key>
inline const bool __is_transparent_v<_Tp, _Key, void_t<typename _Tp::is_transparent> > = true;

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___FUNCTIONAL_COMPARE_H_
