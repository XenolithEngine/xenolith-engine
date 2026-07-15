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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_TRANSFORM_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_TRANSFORM_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _InputIterator, typename _OutputIterator, typename _UnaryOperation>
inline constexpr _OutputIterator transform(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result, _UnaryOperation __op) {
	for (; __first != __last; ++__first, (void)++__result) { *__result = __op(*__first); }
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator,
		typename _BinaryOperation>
inline constexpr _OutputIterator transform(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _OutputIterator __result, _BinaryOperation __op) {
	for (; __first1 != __last1; ++__first1, (void)++__first2, (void)++__result) {
		*__result = __op(*__first1, *__first2);
	}
	return __result;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_TRANSFORM_H_
