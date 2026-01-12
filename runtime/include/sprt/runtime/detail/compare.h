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

struct lexicographical_type_compare_three_way {
	template <typename Type1, typename Type2>
	constexpr auto operator()(const Type1 &__t, const Type2 &__u) const noexcept {
		return __t <=> __u;
	}

	using is_transparent = void;
};

template <typename InputIt1, typename InputIt2>
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
	__identity __proj;
	return sprt::__lexicographical_compare(__first1, __last1, __first2, __last2, sprt::less<void>(),
			__proj, __proj);
}

template <typename InputIt1, typename InputIt2, typename BinaryPred>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, BinaryPred p) {
	for (; first1 != last1; ++first1, ++first2) {
		if (!p(*first1, *first2)) {
			return false;
		}
	}

	return true;
}

template <typename InputIt1, typename InputIt2, typename BinaryPred = equal_to<void>>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
	return equal(first1, last1, first2, BinaryPred());
}

template <typename InputIt, typename Value>
constexpr InputIt find(InputIt first, InputIt last, const Value &value) {
	for (; first != last; ++first) {
		if (*first == value) {
			return first;
		}
	}

	return last;
}

template <typename _Tp>
using __make_unsigned_t = __make_unsigned(_Tp);

template <typename _Integral, enable_if_t<is_integral<_Integral>::value, int> = 0>
constexpr _Integral __half_positive(_Integral __value) {
	return static_cast<_Integral>(static_cast<__make_unsigned_t<_Integral> >(__value) / 2);
}

template <typename _Tp, enable_if_t<!is_integral<_Tp>::value, int> = 0>
constexpr _Tp __half_positive(_Tp __value) {
	return __value / 2;
}

struct _IterOps {
	// advance
	template <typename _Iter, typename _Distance>
	constexpr static void advance(_Iter &__iter, _Distance __count) {
		sprt::advance(__iter, __count);
	}

	// distance
	template <typename _Iter>
	constexpr static ptrdiff_t distance(_Iter __first, _Iter __last) {
		return sprt::distance(__first, __last);
	}

	template <typename _Iter>
	static constexpr void __advance_to(_Iter &__first, _Iter __last) {
		__first = __last;
	}

	// iter_swap
	template <typename _Iter1, typename _Iter2>
	static void iter_swap(_Iter1 &&__a, _Iter2 &&__b) {
		sprt::iter_swap(sprt::forward<_Iter1>(__a), sprt::forward<_Iter2>(__b));
	}

	// advance with sentinel, a la std::ranges::advance
	template <typename _Iter>
	constexpr static ptrdiff_t __advance_to(_Iter &__iter, ptrdiff_t __count,
			const _Iter &__sentinel) {
		return _IterOps::__advance_to(__iter, __count, __sentinel,
				typename _IteratorTraits<_Iter>::iterator_category());
	}

private:
	// advance with sentinel, a la std::ranges::advance -- InputIterator specialization
	template <typename _InputIter>
	constexpr static ptrdiff_t __advance_to(_InputIter &__iter, ptrdiff_t __count,
			const _InputIter &__sentinel, input_iterator_tag) {
		ptrdiff_t __dist = 0;
		for (; __dist < __count && __iter != __sentinel; ++__dist) { ++__iter; }
		return __count - __dist;
	}

	// advance with sentinel, a la std::ranges::advance -- BidirectionalIterator specialization
	template <typename _BiDirIter>
	constexpr static ptrdiff_t __advance_to(_BiDirIter &__iter, ptrdiff_t __count,
			const _BiDirIter &__sentinel, bidirectional_iterator_tag) {
		ptrdiff_t __dist = 0;
		if (__count >= 0) {
			for (; __dist < __count && __iter != __sentinel; ++__dist) { ++__iter; }
		} else {
			for (__count = -__count; __dist < __count && __iter != __sentinel; ++__dist) {
				--__iter;
			}
		}
		return __count - __dist;
	}

	// advance with sentinel, a la std::ranges::advance -- RandomIterator specialization
	template <typename _RandIter>
	constexpr static ptrdiff_t __advance_to(_RandIter &__iter, ptrdiff_t __count,
			const _RandIter &__sentinel, random_access_iterator_tag) {
		auto __dist = _IterOps::distance(__iter, __sentinel);
		if (__count < 0) {
			__dist = __dist > __count ? __dist : __count;
		} else {
			__dist = __dist < __count ? __dist : __count;
		}
		__iter += __dist;
		return __count - __dist;
	}
};

template <typename _Iter, typename _Type, typename _Proj, typename _Comp>
[[nodiscard]]
constexpr _Iter __lower_bound_bisecting(_Iter __first, const _Type &__value, ptrdiff_t __len,
		_Comp &__comp, _Proj &__proj) {
	while (__len != 0) {
		auto __l2 = sprt::__half_positive(__len);
		_Iter __m = __first;
		_IterOps::advance(__m, __l2);
		if (sprt::__invoke(__comp, sprt::__invoke(__proj, *__m), __value)) {
			__first = ++__m;
			__len -= __l2 + 1;
		} else {
			__len = __l2;
		}
	}
	return __first;
}

template <typename _ForwardIterator, typename _Sent, typename _Type, typename _Proj, typename _Comp>
[[__nodiscard__]]
constexpr _ForwardIterator __lower_bound_onesided(_ForwardIterator __first, _Sent __last,
		const _Type &__value, _Comp &__comp, _Proj &__proj) {
	// step = 0, ensuring we can always short-circuit when distance is 1 later on
	if (__first == __last || !sprt::__invoke(__comp, sprt::__invoke(__proj, *__first), __value)) {
		return __first;
	}

	using _Distance = ptrdiff_t;
	for (_Distance __step = 1; __first != __last; __step <<= 1) {
		auto __it = __first;
		auto __dist = __step - _IterOps::__advance_to(__it, __step, __last);
		// once we reach the last range where needle can be we must start
		// looking inwards, bisecting that range
		if (__it == __last || !sprt::__invoke(__comp, sprt::__invoke(__proj, *__it), __value)) {
			// we've already checked the previous value and it was less, we can save
			// one comparison by skipping bisection
			if (__dist == 1) {
				return __it;
			}
			return sprt::__lower_bound_bisecting(__first, __value, __dist, __comp, __proj);
		}
		// range not found, move forward!
		__first = __it;
	}
	return __first;
}

template <typename _ForwardIterator, typename _Sent, typename _Type, typename _Proj, typename _Comp>
[[nodiscard]]
inline constexpr _ForwardIterator __lower_bound(_ForwardIterator __first, _Sent __last,
		const _Type &__value, _Comp &__comp, _Proj &__proj) {
	const auto __dist = sprt::distance(__first, __last);
	return sprt::__lower_bound_bisecting(__first, __value, __dist, __comp, __proj);
}

template <typename _ForwardIterator, typename _Tp, typename _Compare>
[[nodiscard]]
inline constexpr _ForwardIterator lower_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value, _Compare __comp) {
	auto __proj = sprt::__identity();
	return sprt::__lower_bound(__first, __last, __value, __comp, __proj);
}

template <typename _ForwardIterator, typename _Tp>
[[nodiscard]]
inline constexpr _ForwardIterator lower_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value) {
	return sprt::lower_bound(__first, __last, __value, less<void>());
}

template <typename _BidirectionalIterator>
inline constexpr void __reverse_impl(_BidirectionalIterator __first, _BidirectionalIterator __last,
		bidirectional_iterator_tag) {
	while (__first != __last) {
		if (__first == --__last) {
			break;
		}
		_IterOps::iter_swap(__first, __last);
		++__first;
	}
}

template <typename _RandomAccessIterator>
inline constexpr void __reverse_impl(_RandomAccessIterator __first, _RandomAccessIterator __last,
		random_access_iterator_tag) {
	if (__first != __last) {
		for (; __first < --__last; ++__first) { _IterOps::iter_swap(__first, __last); }
	}
}

template <typename _BidirectionalIterator, typename _Sentinel>
constexpr void __reverse(_BidirectionalIterator __first, _Sentinel __last) {
	sprt::__reverse_impl(sprt::move_unsafe(__first), sprt::move_unsafe(__last),
			typename _IteratorTraits<_BidirectionalIterator>::iterator_category());
}

template <typename _BidirectionalIterator>
inline constexpr void reverse(_BidirectionalIterator __first, _BidirectionalIterator __last) {
	sprt::__reverse(sprt::move_unsafe(__first), sprt::move_unsafe(__last));
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_DETAIL_COMPARE_H_
