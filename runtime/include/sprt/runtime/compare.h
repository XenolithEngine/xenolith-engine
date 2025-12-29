/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

// Original source code from LLVM libc++

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_COMPARE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_COMPARE_H_

#include <sprt/runtime/constructable.h>
#include <sprt/runtime/iterator.h>

namespace sprt {

struct lexicographical_type_compare {
	template <typename Type1, typename Type2>
	constexpr auto operator()(const Type1 &__t, const Type2 &__u) const noexcept {
		if (__t == __u) {
			return 0;
		} else if (__t < __u) {
			return -1;
		} else {
			return 1;
		}
	}

	using is_transparent = void;
};

template <typename PtrType1, typename PtrType2, typename _Cmp>
constexpr auto lexicographical_compare_pointer(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2, _Cmp &&__comp)
		-> decltype(__comp(*__first1, *__first2)) {

	auto __len1 = __last1 - __first1;
	auto __len2 = __last2 - __first2;
	auto __min_len = min(__len1, __len2);

	for (decltype(__min_len) __i = 0; __i < __min_len; ++__i) {
		auto __c = __comp(*__first1, *__first2);
		if (__c != 0) {
			return __c;
		}
		++__first1;
		++__first2;
	}

	return lexicographical_type_compare()(__len1, __len2);
}

template <typename PtrType1, typename PtrType2, typename _Cmp>
constexpr auto lexicographical_compare_pointer(PtrType1 __first1, PtrType1 __last1,
		PtrType2 __first2, PtrType2 __last2) {
	return lexicographical_compare_pointer(__first1, __last1, __first2, __last2,
			lexicographical_type_compare());
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_COMPARE_H_
