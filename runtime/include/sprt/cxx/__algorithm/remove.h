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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REMOVE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REMOVE_H_

#include <sprt/cxx/__algorithm/find.h>
#include <sprt/cxx/__iterator/iterator_ops.h>

namespace sprt {

template <class ForwardIt, class T = typename sprt::iterator_traits<ForwardIt>::value_type>
ForwardIt remove(ForwardIt first, ForwardIt last, const T &value) {
	first = sprt::find(first, last, value);
	if (first != last) {
		for (ForwardIt i = first; ++i != last;) {
			if (!(*i == value)) {
				*first++ = sprt::move_unsafe(*i);
			}
		}
	}
	return first;
}

template <class ForwardIt, class UnaryPred>
ForwardIt remove_if(ForwardIt first, ForwardIt last, UnaryPred p) {
	first = sprt::find_if(first, last, p);
	if (first != last) {
		for (ForwardIt i = first; ++i != last;) {
			if (!p(*i)) {
				*first++ = sprt::move_unsafe(*i);
			}
		}
	}
	return first;
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_REMOVE_H_
