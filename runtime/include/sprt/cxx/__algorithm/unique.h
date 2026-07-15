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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_UNIQUE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_UNIQUE_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _ForwardIterator, typename _BinaryPredicate>
inline constexpr _ForwardIterator unique(_ForwardIterator __first, _ForwardIterator __last,
		_BinaryPredicate __pred) {
	if (__first == __last) {
		return __last;
	}
	_ForwardIterator __result = __first;
	while (++__first != __last) {
		if (!__pred(*__result, *__first)) {
			++__result;
			if (__result != __first) {
				*__result = sprt::move_unsafe(*__first);
			}
		}
	}
	return ++__result;
}

template <typename _ForwardIterator>
inline constexpr _ForwardIterator unique(_ForwardIterator __first, _ForwardIterator __last) {
	return sprt::unique(__first, __last,
			[](const auto &__a, const auto &__b) { return __a == __b; });
}

template <typename _InputIterator, typename _OutputIterator, typename _BinaryPredicate>
inline constexpr _OutputIterator unique_copy(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result, _BinaryPredicate __pred) {
	if (__first == __last) {
		return __result;
	}
	typename iterator_traits<_InputIterator>::value_type __value = *__first;
	*__result = __value;
	++__result;
	while (++__first != __last) {
		if (!__pred(__value, *__first)) {
			__value = *__first;
			*__result = __value;
			++__result;
		}
	}
	return __result;
}

template <typename _InputIterator, typename _OutputIterator>
inline constexpr _OutputIterator unique_copy(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result) {
	return sprt::unique_copy(__first, __last, __result,
			[](const auto &__a, const auto &__b) { return __a == __b; });
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_UNIQUE_H_
