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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MOVE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MOVE_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

// Range move / move_backward. These are the 3-argument std::move / std::move_backward
// algorithms (distinct by arity from the 1-argument sprt::move cast in <utility>).
template <typename _InputIterator, typename _OutputIterator>
inline constexpr _OutputIterator move(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result) {
	for (; __first != __last; ++__first, (void)++__result) {
		*__result = sprt::move_unsafe(*__first);
	}
	return __result;
}

template <typename _BidirectionalIterator1, typename _BidirectionalIterator2>
inline constexpr _BidirectionalIterator2 move_backward(_BidirectionalIterator1 __first,
		_BidirectionalIterator1 __last, _BidirectionalIterator2 __result) {
	while (__first != __last) { *--__result = sprt::move_unsafe(*--__last); }
	return __result;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_MOVE_H_
