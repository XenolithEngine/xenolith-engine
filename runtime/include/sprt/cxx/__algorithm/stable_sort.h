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

// stable_sort via top-down merge sort with a heap scratch buffer (random-access
// iterators only, matching the standard). Preserves the relative order of equal
// elements, unlike the introsort used by sprt::sort.

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_STABLE_SORT_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_STABLE_SORT_H_

#include <sprt/cxx/iterator>
#include <sprt/cxx/new> // placement new
#include <sprt/cxx/__functional/compare.h> // less<>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _RandomAccessIterator, typename _Compare, typename _Vp>
inline void __stable_sort_merge(_RandomAccessIterator __first, _RandomAccessIterator __mid,
		_RandomAccessIterator __last, _Compare __comp, _Vp *__buf) {
	_Vp *__b = __buf;
	_RandomAccessIterator __i = __first;
	_RandomAccessIterator __j = __mid;
	// Stable merge: take from the left half unless the right element is strictly less.
	while (__i != __mid && __j != __last) {
		if (__comp(*__j, *__i)) {
			::new (static_cast<void *>(__b)) _Vp(sprt::move_unsafe(*__j));
			++__j;
		} else {
			::new (static_cast<void *>(__b)) _Vp(sprt::move_unsafe(*__i));
			++__i;
		}
		++__b;
	}
	for (; __i != __mid; ++__i, (void)++__b) {
		::new (static_cast<void *>(__b)) _Vp(sprt::move_unsafe(*__i));
	}
	for (; __j != __last; ++__j, (void)++__b) {
		::new (static_cast<void *>(__b)) _Vp(sprt::move_unsafe(*__j));
	}
	__b = __buf;
	for (_RandomAccessIterator __k = __first; __k != __last; ++__k, (void)++__b) {
		*__k = sprt::move_unsafe(*__b);
		__b->~_Vp();
	}
}

template <typename _RandomAccessIterator, typename _Compare, typename _Vp>
inline void __stable_sort_impl(_RandomAccessIterator __first, _RandomAccessIterator __last,
		_Compare __comp, _Vp *__buf) {
	ptrdiff_t __n = __last - __first;
	if (__n < 2) {
		return;
	}
	_RandomAccessIterator __mid = __first + __n / 2;
	sprt::__stable_sort_impl(__first, __mid, __comp, __buf);
	sprt::__stable_sort_impl(__mid, __last, __comp, __buf);
	sprt::__stable_sort_merge(__first, __mid, __last, __comp, __buf);
}

template <typename _RandomAccessIterator, typename _Compare>
inline void stable_sort(_RandomAccessIterator __first, _RandomAccessIterator __last,
		_Compare __comp) {
	using _Vp = typename iterator_traits<_RandomAccessIterator>::value_type;
	ptrdiff_t __n = __last - __first;
	if (__n < 2) {
		return;
	}
	// Raw scratch storage (elements are placement-constructed / explicitly destroyed
	// by the merge). __builtin_malloc avoids the freestanding-deprecated ::operator new.
	_Vp *__buf = static_cast<_Vp *>(__builtin_malloc(sizeof(_Vp) * static_cast<size_t>(__n)));
	sprt::__stable_sort_impl(__first, __last, __comp, __buf);
	__builtin_free(__buf);
}

template <typename _RandomAccessIterator>
inline void stable_sort(_RandomAccessIterator __first, _RandomAccessIterator __last) {
	sprt::stable_sort(__first, __last, sprt::less<>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_STABLE_SORT_H_
