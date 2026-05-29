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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_LEXICOGRAPHICAL_COMPARE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_LEXICOGRAPHICAL_COMPARE_H_

#include <sprt/cxx/detail/constexpr.h>
#include <sprt/cxx/__algorithm/find.h>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/cxx/functional>
#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/compare>

namespace sprt {

struct lexicographical_type_compare {
	template <typename Type1, typename Type2>
	constexpr auto operator()(const Type1 &__t, const Type2 &__u) const noexcept {
		if (__t == __u) {
			return strong_ordering::equal;
		} else if (__t < __u) {
			return strong_ordering::less;
		} else {
			return strong_ordering::greater;
		}
	}

	using is_transparent = void;
};

struct lexicographical_type_compare_three_way {
	template <typename Type1, typename Type2>
	constexpr auto operator()(const Type1 &__t, const Type2 &__u) const noexcept {
		return __t <=> __u;
	}

	using is_transparent = void;
};

template <typename PtrType1, typename PtrType2, typename _Cmp>
constexpr auto lexicographical_compare_pointer(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2, _Cmp &&__comp)
		-> decltype(__comp(*__first1, *__first2)) {

	auto __len1 = __last1 - __first1;
	auto __len2 = __last2 - __first2;
	auto __min_len = min(__len1, __len2);

	for (decltype(__min_len) __i = 0; __i < __min_len; ++__i) {
		auto __c = __comp(*__first1, *__first2);
		if (__c != 0) {
			return __c;
		}
		++__first1;
		++__first2;
	}

	return lexicographical_type_compare()(__len1, __len2);
}

template <typename PtrType1, typename PtrType2>
constexpr auto lexicographical_compare_pointer(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2) {
	return lexicographical_compare_pointer(__first1, __last1, __first2, __last2,
			lexicographical_type_compare());
}

template <typename PtrType1, typename PtrType2>
constexpr auto lexicographical_compare_three_way(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2) {
	return lexicographical_compare_pointer(__first1, __last1, __first2, __last2,
			lexicographical_type_compare_three_way());
}

template <typename _Iter1, typename _Sent1, typename _Iter2, typename _Sent2, typename _Comp>
constexpr bool __lexicographical_compare(_Iter1 __first1, _Sent1 __last1, _Iter2 __first2,
		_Sent2 __last2, _Comp &__comp) {
	while (__first2 != __last2) {
		if (__first1 == __last1 || sprt::__invoke(__comp, *__first1, *__first2)) {
			return true;
		}
		if (sprt::__invoke(__comp, *__first2, *__first1)) {
			return false;
		}
		++__first1;
		++__first2;
	}
	return false;
}

template <typename Type, typename _Comp,
		enable_if_t<is_same_v<_Comp, sprt::less<void>> && !is_volatile<Type>::value
						&& is_trivially_equality_comparable<Type, Type>::value,
				int> = 0>
constexpr bool __lexicographical_compare(Type *__first1, Type *__last1, Type *__first2,
		Type *__last2, _Comp &) {
	if constexpr (__is_trivially_lexicographically_comparable_v<Type, Type>) {
		auto __res = sprt::__constexpr_memcmp(__first1, __first2,
				sprt::min(__last1 - __first1, __last2 - __first2));
		if (__res == 0) {
			return __last1 - __first1 < __last2 - __first2;
		}
		return __res < 0;
	} else {
		auto __res = sprt::mismatch(__first1, __last1, __first2, __last2);
		if (__res.second == __last2) {
			return false;
		}
		if (__res.first == __last1) {
			return true;
		}
		return *__res.first < *__res.second;
	}
}

template <typename _InputIterator1, typename _InputIterator2>
[[nodiscard]]
inline constexpr bool lexicographical_compare(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2) {
	identity __proj;
	return sprt::__lexicographical_compare(__first1, __last1, __first2, __last2, sprt::less<void>(),
			__proj, __proj);
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_LEXICOGRAPHICAL_COMPARE_H_
