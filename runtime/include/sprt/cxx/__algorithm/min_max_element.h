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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MIN_MAX_ELEMENT_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MIN_MAX_ELEMENT_H_

#include <sprt/cxx/iterator>
#include <sprt/cxx/__utility/pair.h>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _ForwardIterator, typename _Compare>
inline constexpr _ForwardIterator min_element(_ForwardIterator __first, _ForwardIterator __last,
		_Compare __comp) {
	if (__first == __last) {
		return __last;
	}
	_ForwardIterator __smallest = __first;
	for (++__first; __first != __last; ++__first) {
		if (__comp(*__first, *__smallest)) {
			__smallest = __first;
		}
	}
	return __smallest;
}

template <typename _ForwardIterator>
inline constexpr _ForwardIterator min_element(_ForwardIterator __first, _ForwardIterator __last) {
	return sprt::min_element(__first, __last,
			[](const auto &__a, const auto &__b) { return __a < __b; });
}

template <typename _ForwardIterator, typename _Compare>
inline constexpr _ForwardIterator max_element(_ForwardIterator __first, _ForwardIterator __last,
		_Compare __comp) {
	if (__first == __last) {
		return __last;
	}
	_ForwardIterator __largest = __first;
	for (++__first; __first != __last; ++__first) {
		if (__comp(*__largest, *__first)) {
			__largest = __first;
		}
	}
	return __largest;
}

template <typename _ForwardIterator>
inline constexpr _ForwardIterator max_element(_ForwardIterator __first, _ForwardIterator __last) {
	return sprt::max_element(__first, __last,
			[](const auto &__a, const auto &__b) { return __a < __b; });
}

template <typename _ForwardIterator, typename _Compare>
inline constexpr sprt::pair<_ForwardIterator, _ForwardIterator> minmax_element(
		_ForwardIterator __first, _ForwardIterator __last, _Compare __comp) {
	_ForwardIterator __min = __first;
	_ForwardIterator __max = __first;
	if (__first == __last) {
		return {__min, __max};
	}
	for (++__first; __first != __last; ++__first) {
		if (__comp(*__first, *__min)) {
			__min = __first;
		}
		if (!__comp(*__first, *__max)) {
			__max = __first;
		}
	}
	return {__min, __max};
}

template <typename _ForwardIterator>
inline constexpr sprt::pair<_ForwardIterator, _ForwardIterator> minmax_element(
		_ForwardIterator __first, _ForwardIterator __last) {
	return sprt::minmax_element(__first, __last,
			[](const auto &__a, const auto &__b) { return __a < __b; });
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MIN_MAX_ELEMENT_H_
