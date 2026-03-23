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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_REVERSE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_REVERSE_H_

#include <sprt/cxx/iterator.h>

namespace sprt {

template <typename _BidirectionalIterator>
inline constexpr void __reverse_impl(_BidirectionalIterator __first, _BidirectionalIterator __last,
		bidirectional_iterator_tag) {
	while (__first != __last) {
		if (__first == --__last) {
			break;
		}
		_IterOps::iter_swap(__first, __last);
		++__first;
	}
}

template <typename _RandomAccessIterator>
inline constexpr void __reverse_impl(_RandomAccessIterator __first, _RandomAccessIterator __last,
		random_access_iterator_tag) {
	if (__first != __last) {
		for (; __first < --__last; ++__first) { _IterOps::iter_swap(__first, __last); }
	}
}

template <typename _BidirectionalIterator, typename _Sentinel>
constexpr void __reverse(_BidirectionalIterator __first, _Sentinel __last) {
	sprt::__reverse_impl(sprt::move_unsafe(__first), sprt::move_unsafe(__last),
			typename _IteratorTraits<_BidirectionalIterator>::iterator_category());
}

template <typename _BidirectionalIterator>
inline constexpr void reverse(_BidirectionalIterator __first, _BidirectionalIterator __last) {
	sprt::__reverse(sprt::move_unsafe(__first), sprt::move_unsafe(__last));
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_REVERSE_H_
