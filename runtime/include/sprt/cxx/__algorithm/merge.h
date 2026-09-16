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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MERGE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MERGE_H_

#include <sprt/cxx/__algorithm/bounds.h> // lower_bound / upper_bound
#include <sprt/cxx/__algorithm/copy.h> // copy
#include <sprt/cxx/__algorithm/rotate.h> // rotate
#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/iterator> // iter_swap / distance
#include <sprt/cxx/new> // nothrow operator new for the buffered inplace_merge
#include <sprt/cxx/type_traits> // is_constant_evaluated

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.merge] merge two sorted ranges into __result.
template <typename _InputIt1, typename _InputIt2, typename _OutputIt, typename _Compare>
constexpr _OutputIt merge(_InputIt1 __first1, _InputIt1 __last1, _InputIt2 __first2,
		_InputIt2 __last2, _OutputIt __result, _Compare __comp) {
	for (; __first1 != __last1; ++__result) {
		if (__first2 == __last2) {
			return sprt::copy(__first1, __last1, __result);
		}
		if (__comp(*__first2, *__first1)) {
			*__result = *__first2;
			++__first2;
		} else {
			*__result = *__first1;
			++__first1;
		}
	}
	return sprt::copy(__first2, __last2, __result);
}

template <typename _InputIt1, typename _InputIt2, typename _OutputIt>
constexpr _OutputIt merge(_InputIt1 __first1, _InputIt1 __last1, _InputIt2 __first2,
		_InputIt2 __last2, _OutputIt __result) {
	return sprt::merge(__first1, __last1, __first2, __last2, __result, less<void>());
}

// [alg.merge] inplace_merge. The classic no-buffer divide-and-conquer fallback:
// split the larger half, locate the matching cut in the other by binary search,
// rotate the two inner blocks together and recurse — O(n log n) comparisons, no
// allocation. Used at compile time and when the temporary buffer is unavailable.
template <typename _BidIt, typename _Compare>
constexpr void __inplace_merge_no_buffer(_BidIt __first, _BidIt __middle, _BidIt __last,
		_Compare __comp) {
	auto __len1 = sprt::distance(__first, __middle);
	auto __len2 = sprt::distance(__middle, __last);
	if (__len1 == 0 || __len2 == 0) {
		return;
	}
	if (__len1 + __len2 == 2) {
		if (__comp(*__middle, *__first)) {
			sprt::iter_swap(__first, __middle);
		}
		return;
	}

	_BidIt __first_cut = __first;
	_BidIt __second_cut = __middle;
	if (__len1 > __len2) {
		auto __len11 = __len1 / 2;
		for (auto __d = __len11; __d > 0; --__d) { ++__first_cut; }
		__second_cut = sprt::lower_bound(__middle, __last, *__first_cut, __comp);
	} else {
		auto __len22 = __len2 / 2;
		for (auto __d = __len22; __d > 0; --__d) { ++__second_cut; }
		__first_cut = sprt::upper_bound(__first, __middle, *__second_cut, __comp);
	}

	_BidIt __new_middle = sprt::rotate(__first_cut, __middle, __second_cut);
	sprt::__inplace_merge_no_buffer(__first, __first_cut, __new_middle, __comp);
	sprt::__inplace_merge_no_buffer(__new_middle, __second_cut, __last, __comp);
}

// Buffered path (mirrors libc++'s __buffered_inplace_merge): the smaller half is
// moved into a heap buffer and merged back — at most len1 + len2 - 1 comparisons,
// as [alg.merge] requires when memory is available.
template <typename _BidIt, typename _Compare, typename _Tp>
void __inplace_merge_buffered(_BidIt __first, _BidIt __middle, _BidIt __last, _Compare __comp,
		_Tp *__buf, bool __left) {
	if (__left) {
		// stash [first, middle), merge forward with [middle, last)
		_Tp *__be = __buf;
		for (_BidIt __i = __first; __i != __middle; ++__i, (void)++__be) {
			::new (static_cast<void *>(__be)) _Tp(sprt::move_unsafe(*__i));
		}
		_Tp *__b = __buf;
		_BidIt __r = __middle;
		_BidIt __o = __first;
		while (__b != __be && __r != __last) {
			if (__comp(*__r, *__b)) {
				*__o = sprt::move_unsafe(*__r);
				++__r;
			} else {
				*__o = sprt::move_unsafe(*__b);
				++__b;
			}
			++__o;
		}
		for (; __b != __be; ++__b, (void)++__o) { *__o = sprt::move_unsafe(*__b); }
		for (_Tp *__p = __buf; __p != __be; ++__p) { __p->~_Tp(); }
	} else {
		// stash [middle, last), merge backward with [first, middle)
		_Tp *__be = __buf;
		for (_BidIt __i = __middle; __i != __last; ++__i, (void)++__be) {
			::new (static_cast<void *>(__be)) _Tp(sprt::move_unsafe(*__i));
		}
		_Tp *__b = __be;
		_BidIt __l = __middle;
		_BidIt __o = __last;
		while (__b != __buf && __l != __first) {
			--__o;
			if (__comp(*(__b - 1), *sprt::prev(__l))) {
				--__l;
				*__o = sprt::move_unsafe(*__l);
			} else {
				--__b;
				*__o = sprt::move_unsafe(*__b);
			}
		}
		while (__b != __buf) {
			--__o;
			--__b;
			*__o = sprt::move_unsafe(*__b);
		}
		for (_Tp *__p = __buf; __p != __be; ++__p) { __p->~_Tp(); }
	}
}

template <typename _BidIt, typename _Compare>
constexpr void inplace_merge(_BidIt __first, _BidIt __middle, _BidIt __last, _Compare __comp) {
	if (sprt::is_constant_evaluated()) {
		sprt::__inplace_merge_no_buffer(__first, __middle, __last, __comp);
		return;
	}
	using _Tp = typename iterator_traits<_BidIt>::value_type;
	const auto __len1 = sprt::distance(__first, __middle);
	const auto __len2 = sprt::distance(__middle, __last);
	if (__len1 == 0 || __len2 == 0) {
		return;
	}
	const bool __left = __len1 <= __len2;
	const auto __blen = __left ? __len1 : __len2;
	void *__mem = __sprt_malloca(static_cast<sprt::size_t>(__blen) * sizeof(_Tp));
	if (!__mem) {
		sprt::__inplace_merge_no_buffer(__first, __middle, __last, __comp);
		return;
	}
	sprt::__inplace_merge_buffered(__first, __middle, __last, __comp, static_cast<_Tp *>(__mem),
			__left);
	__sprt_freea(__mem);
}

template <typename _BidIt>
constexpr void inplace_merge(_BidIt __first, _BidIt __middle, _BidIt __last) {
	sprt::inplace_merge(__first, __middle, __last, less<void>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MERGE_H_
