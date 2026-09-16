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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_PARTITION_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_PARTITION_H_

#include <sprt/cxx/iterator>
#include <sprt/cxx/new> // placement new for stable_partition scratch buffer

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _InputIterator, typename _Predicate>
inline constexpr bool is_partitioned(_InputIterator __first, _InputIterator __last,
		_Predicate __pred) {
	for (; __first != __last; ++__first) {
		if (!__pred(*__first)) {
			break;
		}
	}
	for (; __first != __last; ++__first) {
		if (__pred(*__first)) {
			return false;
		}
	}
	return true;
}

template <typename _ForwardIterator, typename _Predicate>
inline constexpr _ForwardIterator partition(_ForwardIterator __first, _ForwardIterator __last,
		_Predicate __pred) {
	while (__first != __last && __pred(*__first)) { ++__first; }
	if (__first == __last) {
		return __first;
	}
	for (_ForwardIterator __i = __first; ++__i != __last;) {
		if (__pred(*__i)) {
			sprt::iter_swap(__i, __first);
			++__first;
		}
	}
	return __first;
}

template <typename _ForwardIterator, typename _Predicate>
inline _ForwardIterator stable_partition(_ForwardIterator __first, _ForwardIterator __last,
		_Predicate __pred) {
	using _Vp = typename iterator_traits<_ForwardIterator>::value_type;
	ptrdiff_t __n = sprt::distance(__first, __last);
	if (__n == 0) {
		return __first;
	}
	// Raw scratch storage (placement-constructed / explicitly destroyed below);
	// __builtin_malloc avoids the freestanding-deprecated ::operator new.
	_Vp *__buf = static_cast<_Vp *>(__builtin_malloc(sizeof(_Vp) * static_cast<size_t>(__n)));
	_Vp *__b = __buf;
	_ForwardIterator __out = __first;
	for (_ForwardIterator __it = __first; __it != __last; ++__it) {
		if (__pred(*__it)) {
			*__out = sprt::move_unsafe(*__it);
			++__out;
		} else {
			::new (static_cast<void *>(__b)) _Vp(sprt::move_unsafe(*__it));
			++__b;
		}
	}
	_ForwardIterator __ret = __out;
	for (_Vp *__p = __buf; __p != __b; ++__p) {
		*__out = sprt::move_unsafe(*__p);
		++__out;
		__p->~_Vp();
	}
	__builtin_free(__buf);
	return __ret;
}

template <typename _ForwardIterator, typename _Predicate>
inline constexpr _ForwardIterator partition_point(_ForwardIterator __first, _ForwardIterator __last,
		_Predicate __pred) {
	ptrdiff_t __n = sprt::distance(__first, __last);
	while (__n > 0) {
		ptrdiff_t __half = __n / 2;
		_ForwardIterator __mid = __first;
		sprt::advance(__mid, __half);
		if (__pred(*__mid)) {
			__first = ++__mid;
			__n -= __half + 1;
		} else {
			__n = __half;
		}
	}
	return __first;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_PARTITION_H_
