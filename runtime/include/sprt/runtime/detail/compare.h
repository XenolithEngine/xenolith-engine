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

// Original source code from LLVM libc++

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_COMPARE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_COMPARE_H_

#include <sprt/runtime/detail/constructable.h>
#include <sprt/runtime/detail/invoke.h>
#include <sprt/runtime/iterator.h>
#include <sprt/runtime/pair.h>
#include <sprt/runtime/detail/operations.h>
#include <sprt/runtime/detail/constexpr.h>

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


struct lexicographical_type_compare {
	template <typename Type1, typename Type2>
	constexpr auto operator()(const Type1 &__t, const Type2 &__u) const noexcept {
		if (__t == __u) {
			return 0;
		} else if (__t < __u) {
			return -1;
		} else {
			return 1;
		}
	}

	using is_transparent = void;
};

template <class InputIt1, class InputIt2>
constexpr auto mismatch(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
	while (first1 != last1 && first2 != last2 && *first1 == *first2) { ++first1, ++first2; }

	return pair(first1, first2);
}

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

template <typename PtrType1, typename PtrType2, typename _Cmp>
constexpr auto lexicographical_compare_pointer(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2) {
	return lexicographical_compare_pointer(__first1, __last1, __first2, __last2,
			lexicographical_type_compare());
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
	__identity __proj;
	return sprt::__lexicographical_compare(__first1, __last1, __first2, __last2, sprt::less<void>(),
			__proj, __proj);
}

template <class InputIt1, class InputIt2, class BinaryPred>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, BinaryPred p) {
	for (; first1 != last1; ++first1, ++first2) {
		if (!p(*first1, *first2)) {
			return false;
		}
	}

	return true;
}

template <class InputIt1, class InputIt2, class BinaryPred>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
	return equal(first1, last1, first2, equal_to<void>());
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_COMPARE_H_
