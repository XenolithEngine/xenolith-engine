/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_OPERATIONS_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_OPERATIONS_H_

#include <sprt/runtime/init.h>

namespace sprt {

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


} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_OPERATIONS_H_
