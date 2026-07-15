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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_COPY_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_COPY_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _InputIterator, typename _OutputIterator, typename _Tp>
inline constexpr _OutputIterator replace_copy(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result, const _Tp &__old_value, const _Tp &__new_value) {
	for (; __first != __last; ++__first, (void)++__result) {
		*__result = (*__first == __old_value) ? __new_value : *__first;
	}
	return __result;
}

template <typename _InputIterator, typename _OutputIterator, typename _Predicate, typename _Tp>
inline constexpr _OutputIterator replace_copy_if(_InputIterator __first, _InputIterator __last,
		_OutputIterator __result, _Predicate __pred, const _Tp &__new_value) {
	for (; __first != __last; ++__first, (void)++__result) {
		*__result = __pred(*__first) ? __new_value : *__first;
	}
	return __result;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_COPY_H_
