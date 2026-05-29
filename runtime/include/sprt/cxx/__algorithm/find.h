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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_FIND_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_FIND_H_

#include <sprt/cxx/iterator>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/__functional/compare.h>

namespace sprt {

template <typename InputIt1, typename InputIt2>
constexpr auto mismatch(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
	while (first1 != last1 && first2 != last2 && *first1 == *first2) { ++first1, ++first2; }

	return pair(first1, first2);
}

template <typename InputIt, typename Value>
[[nodiscard]]
constexpr InputIt find(InputIt first, InputIt last, const Value &value) {
	for (; first != last; ++first) {
		if (*first == value) {
			return first;
		}
	}

	return last;
}

template <typename _InputIterator, typename _Predicate>
[[nodiscard]]
inline constexpr _InputIterator find_if(_InputIterator __first, _InputIterator __last,
		_Predicate __pred) {
	for (; __first != __last; ++__first) {
		if (__pred(*__first)) {
			break;
		}
	}
	return __first;
}

template <typename _InputIterator, typename _Predicate>
[[nodiscard]]
inline constexpr _InputIterator find_if_not(_InputIterator __first, _InputIterator __last,
		_Predicate __pred) {
	for (; __first != __last; ++__first) {
		if (!__pred(*__first)) {
			break;
		}
	}
	return __first;
}

template <typename _ForwardIterator1, typename _ForwardIterator2, typename _BinaryPredicate>
constexpr _ForwardIterator1 __find_first_of_ce(_ForwardIterator1 __first1,
		_ForwardIterator1 __last1, _ForwardIterator2 __first2, _ForwardIterator2 __last2,
		_BinaryPredicate &&__pred) {
	for (; __first1 != __last1; ++__first1) {
		for (_ForwardIterator2 __j = __first2; __j != __last2; ++__j) {
			if (__pred(*__first1, *__j)) {
				return __first1;
			}
		}
	}
	return __last1;
}

template <typename _ForwardIterator1, typename _ForwardIterator2, typename _BinaryPredicate>
[[nodiscard]]
inline constexpr _ForwardIterator1 find_first_of(_ForwardIterator1 __first1,
		_ForwardIterator1 __last1, _ForwardIterator2 __first2, _ForwardIterator2 __last2,
		_BinaryPredicate __pred) {
	return sprt::__find_first_of_ce(__first1, __last1, __first2, __last2, __pred);
}

template <typename _ForwardIterator1, typename _ForwardIterator2>
[[nodiscard]]
inline constexpr _ForwardIterator1 find_first_of(_ForwardIterator1 __first1,
		_ForwardIterator1 __last1, _ForwardIterator2 __first2, _ForwardIterator2 __last2) {
	return sprt::__find_first_of_ce(__first1, __last1, __first2, __last2, equal_to());
}

template < typename _Iter1, typename _Sent1, typename _Iter2, typename _Sent2, typename _Pred,
		typename _Proj1, typename _Proj2>
inline constexpr pair<_Iter1, _Iter1> __find_end_impl(_Iter1 __first1, _Sent1 __last1,
		_Iter2 __first2, _Sent2 __last2, _Pred &__pred, _Proj1 &__proj1, _Proj2 &__proj2,
		forward_iterator_tag, forward_iterator_tag) {
	// modeled after search algorithm
	_Iter1 __match_first = _IterOps::next(__first1, __last1); // __last1 is the "default" answer
	_Iter1 __match_last = __match_first;
	if (__first2 == __last2) {
		return pair<_Iter1, _Iter1>(__match_last, __match_last);
	}
	while (true) {
		while (true) {
			if (__first1
					== __last1) { // if source exhausted return last correct answer (or __last1 if never found)
				return pair<_Iter1, _Iter1>(__match_first, __match_last);
			}
			if (sprt::__invoke(__pred, sprt::__invoke(__proj1, *__first1),
						sprt::__invoke(__proj2, *__first2))) {
				break;
			}
			++__first1;
		}
		// *__first1 matches *__first2, now match elements after here
		_Iter1 __m1 = __first1;
		_Iter2 __m2 = __first2;
		while (true) {
			if (++__m2 == __last2) { // Pattern exhaused, record answer and search for another one
				__match_first = __first1;
				__match_last = ++__m1;
				++__first1;
				break;
			}
			if (++__m1 == __last1) { // Source exhausted, return last answer
				return pair<_Iter1, _Iter1>(__match_first, __match_last);
			}
			// mismatch, restart with a new __first
			if (!sprt::__invoke(__pred, sprt::__invoke(__proj1, *__m1),
						sprt::__invoke(__proj2, *__m2))) {
				++__first1;
				break;
			} // else there is a match, check next elements
		}
	}
}

template <typename _ForwardIterator1, typename _ForwardIterator2, typename _BinaryPredicate>
[[nodiscard]]
inline constexpr _ForwardIterator1 __find_end_classic(_ForwardIterator1 __first1,
		_ForwardIterator1 __last1, _ForwardIterator2 __first2, _ForwardIterator2 __last2,
		_BinaryPredicate &__pred) {
	auto __proj = identity();
	return sprt::__find_end_impl(__first1, __last1, __first2, __last2, __pred, __proj, __proj,
			typename iterator_traits<_ForwardIterator1>::iterator_category(),
			typename iterator_traits<_ForwardIterator2>::iterator_category())
			.first;
}

template <typename _ForwardIterator1, typename _ForwardIterator2, typename _BinaryPredicate>
[[nodiscard]]
inline constexpr _ForwardIterator1 find_end(_ForwardIterator1 __first1, _ForwardIterator1 __last1,
		_ForwardIterator2 __first2, _ForwardIterator2 __last2, _BinaryPredicate __pred) {
	return sprt::__find_end_classic(__first1, __last1, __first2, __last2, __pred);
}

template <typename _ForwardIterator1, typename _ForwardIterator2>
[[nodiscard]]
inline constexpr _ForwardIterator1 find_end(_ForwardIterator1 __first1, _ForwardIterator1 __last1,
		_ForwardIterator2 __first2, _ForwardIterator2 __last2) {
	return sprt::find_end(__first1, __last1, __first2, __last2, equal_to());
}


} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_FIND_H_
