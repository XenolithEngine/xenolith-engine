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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

template <typename _ForwardIterator, typename _Tp>
inline constexpr void replace(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__old_value, const _Tp &__new_value) {
	for (; __first != __last; ++__first) {
		if (*__first == __old_value) {
			*__first = __new_value;
		}
	}
}

template <typename _ForwardIterator, typename _Predicate, typename _Tp>
inline constexpr void replace_if(_ForwardIterator __first, _ForwardIterator __last,
		_Predicate __pred, const _Tp &__new_value) {
	for (; __first != __last; ++__first) {
		if (__pred(*__first)) {
			*__first = __new_value;
		}
	}
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REPLACE_H_
