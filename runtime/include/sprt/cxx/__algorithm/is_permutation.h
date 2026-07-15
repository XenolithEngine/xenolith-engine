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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_PERMUTATION_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_PERMUTATION_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

// [alg.is.permutation] true iff [first1,last1) is a permutation of the range of the
// same length beginning at first2, comparing with pred. O(n^2), no extra storage:
// after discarding the common prefix, each distinct element's multiplicity in the two
// ranges is compared.
template <typename _ForwardIt1, typename _ForwardIt2, typename _BinaryPred>
[[nodiscard]]
constexpr bool is_permutation(_ForwardIt1 __first1, _ForwardIt1 __last1, _ForwardIt2 __first2,
		_BinaryPred __pred) {
	// Skip the common prefix.
	for (; __first1 != __last1; ++__first1, (void)++__first2) {
		if (!sprt::__invoke(__pred, *__first1, *__first2)) {
			break;
		}
	}
	if (__first1 == __last1) {
		return true;
	}
	// last2 marks the end of the second range (same length as the remaining first).
	_ForwardIt2 __last2 = __first2;
	for (auto __d = sprt::distance(__first1, __last1); __d > 0; --__d) { ++__last2; }
	for (_ForwardIt1 __i = __first1; __i != __last1; ++__i) {
		// Only handle each distinct value once.
		bool __seen = false;
		for (_ForwardIt1 __j = __first1; __j != __i; ++__j) {
			if (sprt::__invoke(__pred, *__j, *__i)) {
				__seen = true;
				break;
			}
		}
		if (__seen) {
			continue;
		}
		// Count matches of *__i in the second range...
		ptrdiff_t __c2 = 0;
		for (_ForwardIt2 __k = __first2; __k != __last2; ++__k) {
			if (sprt::__invoke(__pred, *__i, *__k)) {
				++__c2;
			}
		}
		if (__c2 == 0) {
			return false;
		}
		// ...and in the remainder of the first range (including *__i itself).
		ptrdiff_t __c1 = 1;
		_ForwardIt1 __j = __i;
		for (++__j; __j != __last1; ++__j) {
			if (sprt::__invoke(__pred, *__i, *__j)) {
				++__c1;
			}
		}
		if (__c1 != __c2) {
			return false;
		}
	}
	return true;
}

template <typename _ForwardIt1, typename _ForwardIt2>
[[nodiscard]]
constexpr bool is_permutation(_ForwardIt1 __first1, _ForwardIt1 __last1, _ForwardIt2 __first2) {
	return sprt::is_permutation(__first1, __last1, __first2, equal_to<void>());
}

template <typename _ForwardIt1, typename _ForwardIt2, typename _BinaryPred>
[[nodiscard]]
constexpr bool is_permutation(_ForwardIt1 __first1, _ForwardIt1 __last1, _ForwardIt2 __first2,
		_ForwardIt2 __last2, _BinaryPred __pred) {
	if (sprt::distance(__first1, __last1) != sprt::distance(__first2, __last2)) {
		return false;
	}
	return sprt::is_permutation(__first1, __last1, __first2, __pred);
}

// [alg.is.permutation] the two-range form without a predicate. `__last2` binds `_ForwardIt2` a
// second time, so this overload is more specialized than the (first1, last1, first2, pred) one and
// wins by partial ordering for a four-iterator call — it does not collide with it.
template <typename _ForwardIt1, typename _ForwardIt2>
[[nodiscard]]
constexpr bool is_permutation(_ForwardIt1 __first1, _ForwardIt1 __last1, _ForwardIt2 __first2,
		_ForwardIt2 __last2) {
	if (sprt::distance(__first1, __last1) != sprt::distance(__first2, __last2)) {
		return false;
	}
	return sprt::is_permutation(__first1, __last1, __first2, equal_to<void>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_IS_PERMUTATION_H_
