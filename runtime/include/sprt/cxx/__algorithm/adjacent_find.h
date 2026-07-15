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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ADJACENT_FIND_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ADJACENT_FIND_H_

#include <sprt/cxx/__functional/invoke.h>

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.adjacent.find] first iterator i in [first, last) with pred(*i, *(i+1)); last
// if none. The default predicate is equality.
template <typename ForwardIt, typename BinaryPred>
[[nodiscard]]
constexpr ForwardIt adjacent_find(ForwardIt first, ForwardIt last, BinaryPred pred) {
	if (first == last) {
		return last;
	}
	ForwardIt next = first;
	++next;
	for (; next != last; ++next, ++first) {
		if (sprt::__invoke(pred, *first, *next)) {
			return first;
		}
	}
	return last;
}

template <typename ForwardIt>
[[nodiscard]]
constexpr ForwardIt adjacent_find(ForwardIt first, ForwardIt last) {
	if (first == last) {
		return last;
	}
	ForwardIt next = first;
	++next;
	for (; next != last; ++next, ++first) {
		if (*first == *next) {
			return first;
		}
	}
	return last;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_ADJACENT_FIND_H_
