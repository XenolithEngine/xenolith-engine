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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SET_OPERATIONS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SET_OPERATIONS_H_

#include <sprt/cxx/iterator>

namespace sprt {
inline namespace __cxx_algorithm {

// Merge-style set operations over two sorted ranges. Default comparator is operator<;
// each has a _Compare overload.
template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator>
inline constexpr _OutputIterator set_difference(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (*__first1 < *__first2) {
			*__result = *__first1;
			++__result;
			++__first1;
		} else if (*__first2 < *__first1) {
			++__first2;
		} else {
			++__first1;
			++__first2;
		}
	}
	for (; __first1 != __last1; ++__first1, (void)++__result) { *__result = *__first1; }
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator,
		typename _Compare>
inline constexpr _OutputIterator set_difference(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result,
		_Compare __comp) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (__comp(*__first1, *__first2)) {
			*__result = *__first1;
			++__result;
			++__first1;
		} else if (__comp(*__first2, *__first1)) {
			++__first2;
		} else {
			++__first1;
			++__first2;
		}
	}
	for (; __first1 != __last1; ++__first1, (void)++__result) { *__result = *__first1; }
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2>
inline constexpr bool includes(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2) {
	for (; __first2 != __last2; ++__first1) {
		if (__first1 == __last1 || *__first2 < *__first1) {
			return false;
		}
		if (!(*__first1 < *__first2)) {
			++__first2;
		}
	}
	return true;
}

template <typename _InputIterator1, typename _InputIterator2, typename _Compare>
inline constexpr bool includes(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _Compare __comp) {
	for (; __first2 != __last2; ++__first1) {
		if (__first1 == __last1 || __comp(*__first2, *__first1)) {
			return false;
		}
		if (!__comp(*__first1, *__first2)) {
			++__first2;
		}
	}
	return true;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator>
inline constexpr _OutputIterator set_intersection(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (*__first1 < *__first2) {
			++__first1;
		} else if (*__first2 < *__first1) {
			++__first2;
		} else {
			*__result = *__first1;
			++__result;
			++__first1;
			++__first2;
		}
	}
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator,
		typename _Compare>
inline constexpr _OutputIterator set_intersection(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result,
		_Compare __comp) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (__comp(*__first1, *__first2)) {
			++__first1;
		} else if (__comp(*__first2, *__first1)) {
			++__first2;
		} else {
			*__result = *__first1;
			++__result;
			++__first1;
			++__first2;
		}
	}
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator>
inline constexpr _OutputIterator set_union(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (*__first1 < *__first2) {
			*__result = *__first1;
			++__first1;
		} else if (*__first2 < *__first1) {
			*__result = *__first2;
			++__first2;
		} else {
			*__result = *__first1;
			++__first1;
			++__first2;
		}
		++__result;
	}
	for (; __first1 != __last1; ++__first1, (void)++__result) { *__result = *__first1; }
	for (; __first2 != __last2; ++__first2, (void)++__result) { *__result = *__first2; }
	return __result;
}

template <typename _InputIterator1, typename _InputIterator2, typename _OutputIterator,
		typename _Compare>
inline constexpr _OutputIterator set_union(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2, _OutputIterator __result,
		_Compare __comp) {
	while (__first1 != __last1 && __first2 != __last2) {
		if (__comp(*__first1, *__first2)) {
			*__result = *__first1;
			++__first1;
		} else if (__comp(*__first2, *__first1)) {
			*__result = *__first2;
			++__first2;
		} else {
			*__result = *__first1;
			++__first1;
			++__first2;
		}
		++__result;
	}
	for (; __first1 != __last1; ++__first1, (void)++__result) { *__result = *__first1; }
	for (; __first2 != __last2; ++__first2, (void)++__result) { *__result = *__first2; }
	return __result;
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_SET_OPERATIONS_H_
