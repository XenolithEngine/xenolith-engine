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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_BOUNDS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_BOUNDS_H_

#include <sprt/cxx/__functional/invoke.h>
#include <sprt/cxx/__functional/compare.h>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__iterator/iterator_ops.h>

namespace sprt {

template <typename _Integral, enable_if_t<is_integral<_Integral>::value, int> = 0>
constexpr _Integral __half_positive(_Integral __value) {
	return static_cast<_Integral>(static_cast<make_unsigned_t<_Integral> >(__value) / 2);
}

template <typename _Tp, enable_if_t<!is_integral<_Tp>::value, int> = 0>
constexpr _Tp __half_positive(_Tp __value) {
	return __value / 2;
}

/*
lower_bound
*/

template <typename _Iter, typename _Type, typename _Proj, typename _Comp>
[[nodiscard]]
constexpr _Iter __lower_bound_bisecting(_Iter __first, const _Type &__value, ptrdiff_t __len,
		_Comp &__comp, _Proj &__proj) {
	while (__len != 0) {
		auto __l2 = sprt::__half_positive(__len);
		_Iter __m = __first;
		_IterOps::advance(__m, __l2);
		if (sprt::__invoke(__comp, sprt::__invoke(__proj, *__m), __value)) {
			__first = ++__m;
			__len -= __l2 + 1;
		} else {
			__len = __l2;
		}
	}
	return __first;
}

template <typename _ForwardIterator, typename _Sent, typename _Type, typename _Proj, typename _Comp>
[[nodiscard]]
inline constexpr _ForwardIterator __lower_bound(_ForwardIterator __first, _Sent __last,
		const _Type &__value, _Comp &__comp, _Proj &__proj) {
	const auto __dist = sprt::distance(__first, __last);
	return sprt::__lower_bound_bisecting(__first, __value, __dist, __comp, __proj);
}

template <typename _ForwardIterator, typename _Tp, typename _Compare>
[[nodiscard]]
inline constexpr _ForwardIterator lower_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value, _Compare __comp) {
	auto __proj = sprt::identity();
	return sprt::__lower_bound(__first, __last, __value, __comp, __proj);
}

template <typename _ForwardIterator, typename _Tp>
[[nodiscard]]
inline constexpr _ForwardIterator lower_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value) {
	return sprt::lower_bound(__first, __last, __value, less<void>());
}


/*
upper_bound
*/

template <typename _Compare, typename _Iter, typename _Sent, typename _Tp, typename _Proj>
constexpr _Iter __upper_bound(_Iter __first, _Sent __last, const _Tp &__value, _Compare &&__comp,
		_Proj &&__proj) {
	auto __len = _IterOps::distance(__first, __last);
	while (__len != 0) {
		auto __half_len = sprt::__half_positive(__len);
		auto __mid = _IterOps::next(__first, __half_len);
		if (sprt::__invoke(__comp, __value, sprt::__invoke(__proj, *__mid))) {
			__len = __half_len;
		} else {
			__first = ++__mid;
			__len -= __half_len + 1;
		}
	}
	return __first;
}

template <typename _ForwardIterator, typename _Tp, typename _Compare>
[[nodiscard]]
inline constexpr _ForwardIterator upper_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value, _Compare __comp) {
	static_assert(is_copy_constructible<_ForwardIterator>::value,
			"Iterator has to be copy constructible");
	return sprt::__upper_bound(sprt::move(__first), sprt::move(__last), __value, sprt::move(__comp),
			sprt::identity());
}

template <typename _ForwardIterator, typename _Tp>
[[nodiscard]]
inline constexpr _ForwardIterator upper_bound(_ForwardIterator __first, _ForwardIterator __last,
		const _Tp &__value) {
	return sprt::upper_bound(sprt::move(__first), sprt::move(__last), __value, less<void>());
}


/*
equal_range
*/

template <typename _Compare, typename _Iter, typename _Sent, typename _Tp, typename _Proj>
constexpr pair<_Iter, _Iter> __equal_range(_Iter __first, _Sent __last, const _Tp &__value,
		_Compare &&__comp, _Proj &&__proj) {
	auto __len = _IterOps::distance(__first, __last);
	_Iter __end = _IterOps::next(__first, __last);
	while (__len != 0) {
		auto __half_len = sprt::__half_positive(__len);
		_Iter __mid = _IterOps::next(__first, __half_len);
		if (sprt::__invoke(__comp, sprt::__invoke(__proj, *__mid), __value)) {
			__first = ++__mid;
			__len -= __half_len + 1;
		} else if (sprt::__invoke(__comp, __value, sprt::__invoke(__proj, *__mid))) {
			__end = __mid;
			__len = __half_len;
		} else {
			_Iter __mp1 = __mid;
			return pair<_Iter, _Iter>(sprt::__lower_bound(__first, __mid, __value, __comp, __proj),
					sprt::__upper_bound(++__mp1, __end, __value, __comp, __proj));
		}
	}
	return pair<_Iter, _Iter>(__first, __first);
}

template <typename _ForwardIterator, typename _Tp, typename _Compare>
[[nodiscard]]
constexpr pair<_ForwardIterator, _ForwardIterator> equal_range(_ForwardIterator __first,
		_ForwardIterator __last, const _Tp &__value, _Compare __comp) {
	static_assert(is_copy_constructible<_ForwardIterator>::value,
			"Iterator has to be copy constructible");
	return sprt::__equal_range(sprt::move(__first), sprt::move(__last), __value, __comp,
			sprt::identity());
}

template <typename _ForwardIterator, typename _Tp>
[[nodiscard]]
constexpr pair<_ForwardIterator, _ForwardIterator> equal_range(_ForwardIterator __first,
		_ForwardIterator __last, const _Tp &__value) {
	return sprt::equal_range(sprt::move(__first), sprt::move(__last), __value, less<void>());
}

template <typename InputIt1, typename InputIt2, typename BinaryPred>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, BinaryPred p) {
	for (; first1 != last1; ++first1, ++first2) {
		if (!p(*first1, *first2)) {
			return false;
		}
	}

	return true;
}

template <typename InputIt1, typename InputIt2, typename BinaryPred = equal_to<void>>
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
	return equal(first1, last1, first2, BinaryPred());
}

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ALGORITHM_BOUNDS_H_
