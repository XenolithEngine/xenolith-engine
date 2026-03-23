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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_NEXT_PERMUTATION_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_NEXT_PERMUTATION_H_

#include <sprt/cxx/iterator.h>
#include <sprt/cxx/pair.h>
#include <sprt/cxx/detail/reverse.h>
#include <sprt/cxx/functional.h>

namespace sprt {

template <typename _Compare, typename _BidirectionalIterator, typename _Sentinel>
constexpr pair<_BidirectionalIterator, bool> __next_permutation(_BidirectionalIterator __first,
		_Sentinel __last, _Compare &&__comp) {
	using _Result = pair<_BidirectionalIterator, bool>;

	_BidirectionalIterator __last_iter = _IterOps::next(__first, __last);
	_BidirectionalIterator __i = __last_iter;
	if (__first == __last || __first == --__i) {
		return _Result(sprt::move(__last_iter), false);
	}

	while (true) {
		_BidirectionalIterator __ip1 = __i;
		if (__comp(*--__i, *__ip1)) {
			_BidirectionalIterator __j = __last_iter;
			while (!__comp(*__i, *--__j));
			_IterOps::iter_swap(__i, __j);
			sprt::__reverse(__ip1, __last_iter);
			return _Result(sprt::move(__last_iter), true);
		}
		if (__i == __first) {
			sprt::__reverse(__first, __last_iter);
			return _Result(sprt::move(__last_iter), false);
		}
	}
}

template <typename _BidirectionalIterator, typename _Compare>
inline constexpr bool next_permutation(_BidirectionalIterator __first,
		_BidirectionalIterator __last, _Compare __comp) {
	return sprt::__next_permutation(sprt::move(__first), sprt::move(__last), __comp).second;
}

template <typename _BidirectionalIterator>
inline constexpr bool next_permutation(_BidirectionalIterator __first,
		_BidirectionalIterator __last) {
	return sprt::next_permutation(__first, __last, less<void>());
}

} // namespace sprt

#endif
