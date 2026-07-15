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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ROTATE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ROTATE_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

// Gries-Mills forward rotate: works for any ForwardIterator, returns the iterator
// to the element originally at __first (i.e. __first + (__last - __middle)).
template <typename _ForwardIterator>
inline constexpr _ForwardIterator rotate(_ForwardIterator __first, _ForwardIterator __middle,
		_ForwardIterator __last) {
	if (__first == __middle) {
		return __last;
	}
	if (__middle == __last) {
		return __first;
	}

	_ForwardIterator __next = __middle;
	do {
		sprt::iter_swap(__first, __next);
		++__first;
		++__next;
		if (__first == __middle) {
			__middle = __next;
		}
	} while (__next != __last);

	_ForwardIterator __ret = __first;
	__next = __middle;
	while (__next != __last) {
		sprt::iter_swap(__first, __next);
		++__first;
		++__next;
		if (__first == __middle) {
			__middle = __next;
		} else if (__next == __last) {
			__next = __middle;
		}
	}
	return __ret;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ROTATE_H_
