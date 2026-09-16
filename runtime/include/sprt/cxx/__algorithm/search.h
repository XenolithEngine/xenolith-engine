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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SEARCH_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SEARCH_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__functional/invoke.h>

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.search] first occurrence of the subsequence [s_first,s_last) within
// [first,last), comparing with pred; last if none.
template <typename _ForwardIt1, typename _ForwardIt2, typename _BinaryPred>
[[nodiscard]]
constexpr _ForwardIt1 search(_ForwardIt1 __first, _ForwardIt1 __last, _ForwardIt2 __s_first,
		_ForwardIt2 __s_last, _BinaryPred __pred) {
	for (;; ++__first) {
		_ForwardIt1 __it = __first;
		for (_ForwardIt2 __s_it = __s_first;; ++__it, (void)++__s_it) {
			if (__s_it == __s_last) {
				return __first;
			}
			if (__it == __last) {
				return __last;
			}
			if (!sprt::__invoke(__pred, *__it, *__s_it)) {
				break;
			}
		}
	}
}

template <typename _ForwardIt1, typename _ForwardIt2>
[[nodiscard]]
constexpr _ForwardIt1 search(_ForwardIt1 __first, _ForwardIt1 __last, _ForwardIt2 __s_first,
		_ForwardIt2 __s_last) {
	return sprt::search(__first, __last, __s_first, __s_last, equal_to<void>());
}

// [alg.search] first run of `count` consecutive elements equal to `value`.
template <typename _ForwardIt, typename _Size, typename _Tp, typename _BinaryPred>
[[nodiscard]]
constexpr _ForwardIt search_n(_ForwardIt __first, _ForwardIt __last, _Size __count,
		const _Tp &__value, _BinaryPred __pred) {
	if (__count <= 0) {
		return __first;
	}
	for (; __first != __last; ++__first) {
		if (!sprt::__invoke(__pred, *__first, __value)) {
			continue;
		}
		_ForwardIt __candidate = __first;
		_Size __cur = 0;
		while (true) {
			++__cur;
			if (__cur >= __count) {
				return __candidate;
			}
			++__first;
			if (__first == __last) {
				return __last;
			}
			if (!sprt::__invoke(__pred, *__first, __value)) {
				break;
			}
		}
	}
	return __last;
}

template <typename _ForwardIt, typename _Size, typename _Tp>
[[nodiscard]]
constexpr _ForwardIt search_n(_ForwardIt __first, _ForwardIt __last, _Size __count,
		const _Tp &__value) {
	return sprt::search_n(__first, __last, __count, __value, equal_to<void>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SEARCH_H_
