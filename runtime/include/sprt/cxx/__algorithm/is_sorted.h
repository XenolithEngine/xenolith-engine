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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_SORTED_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_SORTED_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _ForwardIterator, typename _Compare>
inline constexpr _ForwardIterator is_sorted_until(_ForwardIterator __first, _ForwardIterator __last,
		_Compare __comp) {
	if (__first == __last) {
		return __last;
	}
	_ForwardIterator __next = __first;
	for (++__next; __next != __last; __first = __next, (void)++__next) {
		if (__comp(*__next, *__first)) {
			return __next;
		}
	}
	return __last;
}

template <typename _ForwardIterator>
inline constexpr _ForwardIterator is_sorted_until(_ForwardIterator __first,
		_ForwardIterator __last) {
	return sprt::is_sorted_until(__first, __last,
			[](const auto &__a, const auto &__b) { return __a < __b; });
}

template <typename _ForwardIterator, typename _Compare>
inline constexpr bool is_sorted(_ForwardIterator __first, _ForwardIterator __last,
		_Compare __comp) {
	return sprt::is_sorted_until(__first, __last, __comp) == __last;
}

template <typename _ForwardIterator>
inline constexpr bool is_sorted(_ForwardIterator __first, _ForwardIterator __last) {
	return sprt::is_sorted_until(__first, __last) == __last;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_SORTED_H_
