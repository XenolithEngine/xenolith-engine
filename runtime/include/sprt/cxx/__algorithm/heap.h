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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_HEAP_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_HEAP_H_

#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__utility/common.h> // move_unsafe (element moves; elements may be pointers)

namespace sprt {
inline namespace __cxx_algorithm {

// Binary max-heap over a random-access range, comparator `comp` (a<b means a is
// "smaller", so the largest element is the root). Element moves use move_unsafe:
// sprt::move is SFINAE-disabled for pointer arguments and heap elements are often
// pointers.

// Percolate a hole at `hole` up towards the root until `value` fits (used by push_heap).
template <typename _RandIt, typename _Dist, typename _Tp, typename _Compare>
constexpr void __sift_up(_RandIt __first, _Dist __hole, _Tp __value, _Compare __comp) {
	_Dist __parent = (__hole - 1) / 2;
	while (__hole > 0 && __comp(*(__first + __parent), __value)) {
		*(__first + __hole) = sprt::move_unsafe(*(__first + __parent));
		__hole = __parent;
		__parent = (__hole - 1) / 2;
	}
	*(__first + __hole) = sprt::move_unsafe(__value);
}

// Percolate a hole at `start` down over [0,len), then place `value` (used by
// pop_heap / make_heap).
template <typename _RandIt, typename _Dist, typename _Tp, typename _Compare>
constexpr void __sift_down(_RandIt __first, _Dist __len, _Dist __start, _Tp __value,
		_Compare __comp) {
	_Dist __root = __start;
	_Dist __child;
	while ((__child = 2 * __root + 1) < __len) {
		if (__child + 1 < __len && __comp(*(__first + __child), *(__first + __child + 1))) {
			++__child;
		}
		if (!__comp(__value, *(__first + __child))) {
			break;
		}
		*(__first + __root) = sprt::move_unsafe(*(__first + __child));
		__root = __child;
	}
	*(__first + __root) = sprt::move_unsafe(__value);
}

template <typename _RandIt, typename _Compare>
constexpr void push_heap(_RandIt __first, _RandIt __last, _Compare __comp) {
	auto __len = __last - __first;
	if (__len > 1) {
		auto __hole = __len - 1;
		sprt::__sift_up(__first, __hole, sprt::move_unsafe(*(__first + __hole)), __comp);
	}
}

template <typename _RandIt>
constexpr void push_heap(_RandIt __first, _RandIt __last) {
	sprt::push_heap(__first, __last, less<void>());
}

template <typename _RandIt, typename _Compare>
constexpr void pop_heap(_RandIt __first, _RandIt __last, _Compare __comp) {
	auto __len = __last - __first;
	if (__len > 1) {
		--__last;
		auto __value = sprt::move_unsafe(*__last);
		*__last = sprt::move_unsafe(*__first);
		sprt::__sift_down(__first, __len - 1, decltype(__len)(0), sprt::move_unsafe(__value),
				__comp);
	}
}

template <typename _RandIt>
constexpr void pop_heap(_RandIt __first, _RandIt __last) {
	sprt::pop_heap(__first, __last, less<void>());
}

template <typename _RandIt, typename _Compare>
constexpr void make_heap(_RandIt __first, _RandIt __last, _Compare __comp) {
	auto __len = __last - __first;
	for (auto __start = __len / 2; __start > 0;) {
		--__start;
		sprt::__sift_down(__first, __len, __start, sprt::move_unsafe(*(__first + __start)), __comp);
	}
}

template <typename _RandIt>
constexpr void make_heap(_RandIt __first, _RandIt __last) {
	sprt::make_heap(__first, __last, less<void>());
}

template <typename _RandIt, typename _Compare>
constexpr void sort_heap(_RandIt __first, _RandIt __last, _Compare __comp) {
	for (; __last - __first > 1; --__last) { sprt::pop_heap(__first, __last, __comp); }
}

template <typename _RandIt>
constexpr void sort_heap(_RandIt __first, _RandIt __last) {
	sprt::sort_heap(__first, __last, less<void>());
}

template <typename _RandIt, typename _Compare>
[[nodiscard]]
constexpr _RandIt is_heap_until(_RandIt __first, _RandIt __last, _Compare __comp) {
	auto __len = __last - __first;
	for (decltype(__len) __child = 1; __child < __len; ++__child) {
		auto __parent = (__child - 1) / 2;
		if (__comp(*(__first + __parent), *(__first + __child))) {
			return __first + __child;
		}
	}
	return __last;
}

template <typename _RandIt>
[[nodiscard]]
constexpr _RandIt is_heap_until(_RandIt __first, _RandIt __last) {
	return sprt::is_heap_until(__first, __last, less<void>());
}

template <typename _RandIt, typename _Compare>
[[nodiscard]]
constexpr bool is_heap(_RandIt __first, _RandIt __last, _Compare __comp) {
	return sprt::is_heap_until(__first, __last, __comp) == __last;
}

template <typename _RandIt>
[[nodiscard]]
constexpr bool is_heap(_RandIt __first, _RandIt __last) {
	return sprt::is_heap(__first, __last, less<void>());
}

} // namespace __cxx_algorithm
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_HEAP_H_
