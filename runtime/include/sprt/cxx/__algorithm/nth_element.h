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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_NTH_ELEMENT_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_NTH_ELEMENT_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/iterator> // iter_swap

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.nth.element] partial ordering: after the call *nth is the element that would be
// there in a fully sorted range, everything before it compares <= and everything after
// compares >=. Quickselect with Lomuto partition (average O(n)); `nth == last` is a no-op.
template <typename _RandomIt, typename _Compare>
constexpr void nth_element(_RandomIt __first, _RandomIt __nth, _RandomIt __last, _Compare __comp) {
	if (__nth == __last) {
		return;
	}
	while (__last - __first > 1) {
		_RandomIt __pivot = __first + (__last - __first) / 2;
		sprt::iter_swap(__pivot, __last - 1);
		_RandomIt __store = __first;
		for (_RandomIt __it = __first; __it != __last - 1; ++__it) {
			if (__comp(*__it, *(__last - 1))) {
				sprt::iter_swap(__it, __store);
				++__store;
			}
		}
		sprt::iter_swap(__store, __last - 1);
		if (__store == __nth) {
			return;
		}
		if (__nth < __store) {
			__last = __store;
		} else {
			__first = __store + 1;
		}
	}
}

template <typename _RandomIt>
constexpr void nth_element(_RandomIt __first, _RandomIt __nth, _RandomIt __last) {
	sprt::nth_element(__first, __nth, __last, less<void>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_NTH_ELEMENT_H_
