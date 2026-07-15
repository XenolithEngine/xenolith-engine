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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__utility/pair.h> // minmax returns a pair
#include <sprt/cxx/initializer_list>

namespace sprt {
inline namespace __cxx_algorithm {

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

/*
	min / max over an initializer_list ([alg.min.max]). Unlike the two-argument
	forms these return by value (the list is a temporary) and the list must be
	non-empty.
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr Type min(initializer_list<Type> __il, Compare __comp) {
	const Type *__result = __il.begin();
	for (const Type *__it = __il.begin() + 1; __it != __il.end(); ++__it) {
		if (__comp(*__it, *__result)) {
			__result = __it;
		}
	}
	return *__result;
}

template <typename Type>
[[nodiscard]]
inline constexpr Type min(initializer_list<Type> __il) {
	return sprt::min(__il, less<void>());
}

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr Type max(initializer_list<Type> __il, Compare __comp) {
	const Type *__result = __il.begin();
	for (const Type *__it = __il.begin() + 1; __it != __il.end(); ++__it) {
		if (__comp(*__result, *__it)) {
			__result = __it;
		}
	}
	return *__result;
}

template <typename Type>
[[nodiscard]]
inline constexpr Type max(initializer_list<Type> __il) {
	return sprt::max(__il, less<void>());
}

/*
	minmax ([alg.min.max]): the (min, max) pair. The two-argument form returns
	references into its arguments; the initializer_list form returns by value.
*/

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr pair<const Type &, const Type &> minmax(const Type &l, const Type &r,
		Compare comp) {
	return comp(r, l) ? pair<const Type &, const Type &>(r, l)
					  : pair<const Type &, const Type &>(l, r);
}

template <typename Type>
[[nodiscard]]
inline constexpr pair<const Type &, const Type &> minmax(const Type &l, const Type &r) {
	return sprt::minmax(l, r, less<void>());
}

template <typename Type, typename Compare>
[[nodiscard]]
inline constexpr pair<Type, Type> minmax(initializer_list<Type> __il, Compare __comp) {
	// Pairwise scheme (as in libc++'s __minmax_element_impl): elements are taken
	// two at a time — one comparison orders the pair, one updates the minimum,
	// one updates the maximum — for at most 3·N/2 comparisons ([alg.min.max]).
	const Type *__first = __il.begin();
	const Type *__last = __il.end();
	const Type *__min = __first;
	const Type *__max = __first;
	if (__first != __last && ++__first != __last) {
		if (__comp(*__first, *__min)) {
			__min = __first;
		} else {
			__max = __first;
		}
		while (++__first != __last) {
			const Type *__i = __first;
			if (++__first == __last) {
				// odd tail: one element left
				if (__comp(*__i, *__min)) {
					__min = __i;
				} else if (!__comp(*__i, *__max)) {
					__max = __i;
				}
				break;
			}
			if (__comp(*__first, *__i)) {
				if (__comp(*__first, *__min)) {
					__min = __first;
				}
				if (!__comp(*__i, *__max)) {
					__max = __i;
				}
			} else {
				if (__comp(*__i, *__min)) {
					__min = __i;
				}
				if (!__comp(*__first, *__max)) {
					__max = __first;
				}
			}
		}
	}
	return pair<Type, Type>(*__min, *__max);
}

template <typename Type>
[[nodiscard]]
inline constexpr pair<Type, Type> minmax(initializer_list<Type> __il) {
	return sprt::minmax(__il, less<void>());
}

template <typename T, typename... Args>
constexpr const T &__vmax(const T &first, const Args &...args) {
	auto result = &first;
	((result = &sprt::max(*result, args)), ...); // Unary right fold over ','
	return *result;
}

template <typename T, typename... Args>
constexpr const T &__vmin(const T &first, const Args &...args) {
	auto result = &first;
	((result = &sprt::min(*result, args)), ...); // Unary right fold over ','
	return *result;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MINMAX_H_
