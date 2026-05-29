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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_ITERATOR_OPS_H_
#define RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_ITERATOR_OPS_H_

#include <sprt/cxx/__utility/swap.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__iterator/iterator_tags.h>
#include <sprt/cxx/__memory/pointer_traits.h>

namespace sprt {


template <typename Iter>
struct iterator_traits;

template <class _Ip>
concept __has_member_iterator_category = requires { typename _Ip::iterator_category; };

template <class _Ip>
concept __specifies_members = requires {
	typename _Ip::value_type;
	requires __has_member_iterator_category<_Ip>;
};

template <__specifies_members _Ip>
struct iterator_traits<_Ip> {
	using iterator_category = typename _Ip::iterator_category;
	using value_type = typename _Ip::value_type;
	using difference_type = typename _Ip::difference_type;
};

template <typename Ptr>
struct iterator_traits<Ptr *> {
	using iterator_category = random_access_iterator_tag;
	using value_type = Ptr;
	using difference_type = sprt::ptrdiff_t;
};

template <typename _InputIter>
inline constexpr ptrdiff_t __distance(_InputIter __first, _InputIter __last, input_iterator_tag) {
	ptrdiff_t __r(0);
	for (; __first != __last; ++__first) { ++__r; }
	return __r;
}

template <typename _RandIter>
inline constexpr ptrdiff_t __distance(_RandIter __first, _RandIter __last,
		random_access_iterator_tag) {
	return __last - __first;
}

template <typename _InputIter>
inline constexpr ptrdiff_t distance(_InputIter __first, _InputIter __last) {
	return sprt::__distance(__first, __last,
			typename iterator_traits<_InputIter>::iterator_category());
}


template <typename _InputIter>
void __advance(_InputIter &__i, ptrdiff_t __n, input_iterator_tag) {
	for (; __n > 0; --__n) { ++__i; }
}

template <typename _BiDirIter>
void __advance(_BiDirIter &__i, ptrdiff_t __n, bidirectional_iterator_tag) {
	if (__n >= 0) {
		for (; __n > 0; --__n) { ++__i; }
	} else {
		for (; __n < 0; ++__n) { --__i; }
	}
}

template <typename _RandIter>
void __advance(_RandIter &__i, ptrdiff_t __n, random_access_iterator_tag) {
	__i += __n;
}

template < typename _InputIter>
void advance(_InputIter &__i, ptrdiff_t __n) {
	sprt::__advance(__i, __n, typename iterator_traits<_InputIter>::iterator_category());
}

template <typename _ForwardIterator1, typename _ForwardIterator2>
inline constexpr void iter_swap(_ForwardIterator1 __a, _ForwardIterator2 __b) noexcept {
	sprt::swap(*__a, *__b);
}

template <typename _Tp>
using __iterator_category = typename _Tp::iterator_category;

template <typename _Tp>
using __iterator_concept = typename _Tp::iterator_concept;

template <typename _Tp, typename _Up>
using __has_iterator_category_convertible_to =
		is_convertible<__detected_or_t<__nat, __iterator_category, iterator_traits<_Tp> >, _Up>;

template <typename _Tp, typename _Up>
using __has_iterator_concept_convertible_to =
		is_convertible<__detected_or_t<__nat, __iterator_concept, _Tp>, _Up>;

template <typename _Tp>
using __has_input_iterator_category =
		__has_iterator_category_convertible_to<_Tp, input_iterator_tag>;

template <typename _Tp>
using __has_forward_iterator_category =
		__has_iterator_category_convertible_to<_Tp, forward_iterator_tag>;

template <typename _Tp>
using __has_bidirectional_iterator_category =
		__has_iterator_category_convertible_to<_Tp, bidirectional_iterator_tag>;

template <typename _Tp>
using __has_random_access_iterator_category =
		__has_iterator_category_convertible_to<_Tp, random_access_iterator_tag>;

template <typename _InputIter,
		enable_if_t<__has_input_iterator_category<_InputIter>::value, int> = 0>
[[nodiscard]]
inline constexpr _InputIter next(_InputIter __x,
		typename iterator_traits<_InputIter>::difference_type __n = 1) {
	sprt::advance(__x, __n);
	return __x;
}

template <typename _InputIter,
		enable_if_t<__has_input_iterator_category<_InputIter>::value, int> = 0>
[[nodiscard]]
inline constexpr _InputIter prev(_InputIter __x,
		typename iterator_traits<_InputIter>::difference_type __n) {
	sprt::advance(__x, -__n);
	return __x;
}

template <typename _InputIter,
		enable_if_t<__has_input_iterator_category<_InputIter>::value, int> = 0>
[[nodiscard]]
constexpr _InputIter prev(_InputIter __it) {
	static_assert(__has_bidirectional_iterator_category<_InputIter>::value,
			"Attempt to prev(it) with a non-bidirectional iterator");
	return sprt::prev(sprt::move(__it), 1);
}

struct _IterOps {
	// advance
	template <typename _Iter, typename _Distance>
	constexpr static void advance(_Iter &__iter, _Distance __count) {
		sprt::advance(__iter, __count);
	}

	// distance
	template <typename _Iter>
	constexpr static ptrdiff_t distance(_Iter __first, _Iter __last) {
		return sprt::distance(__first, __last);
	}

	template <typename _Iter>
	static constexpr void __advance_to(_Iter &__first, _Iter __last) {
		__first = __last;
	}

	// iter_swap
	template <typename _Iter1, typename _Iter2>
	static void iter_swap(_Iter1 &&__a, _Iter2 &&__b) {
		sprt::iter_swap(sprt::forward<_Iter1>(__a), sprt::forward<_Iter2>(__b));
	}

	// advance with sentinel, a la std::ranges::advance
	template <typename _Iter>
	constexpr static ptrdiff_t __advance_to(_Iter &__iter, ptrdiff_t __count,
			const _Iter &__sentinel) {
		return _IterOps::__advance_to(__iter, __count, __sentinel,
				typename iterator_traits<_Iter>::iterator_category());
	}

	// next
	template <typename _Iterator>
	static constexpr _Iterator next(_Iterator, _Iterator __last) {
		return __last;
	}

	template <typename _Iter>
	static constexpr remove_cvref_t<_Iter> next(_Iter &&__it,
			typename iterator_traits<remove_cvref_t<_Iter> >::difference_type __n = 1) {
		return sprt::next(sprt::forward<_Iter>(__it), __n);
	}

	// prev
	template <typename _Iter>
	static constexpr remove_cvref_t<_Iter> prev(_Iter &&__iter,
			typename iterator_traits<remove_cvref_t<_Iter> >::difference_type __n = 1) {
		return sprt::prev(sprt::forward<_Iter>(__iter), __n);
	}

private:
	// advance with sentinel, a la std::ranges::advance -- InputIterator specialization
	template <typename _InputIter>
	constexpr static ptrdiff_t __advance_to(_InputIter &__iter, ptrdiff_t __count,
			const _InputIter &__sentinel, input_iterator_tag) {
		ptrdiff_t __dist = 0;
		for (; __dist < __count && __iter != __sentinel; ++__dist) { ++__iter; }
		return __count - __dist;
	}

	// advance with sentinel, a la std::ranges::advance -- BidirectionalIterator specialization
	template <typename _BiDirIter>
	constexpr static ptrdiff_t __advance_to(_BiDirIter &__iter, ptrdiff_t __count,
			const _BiDirIter &__sentinel, bidirectional_iterator_tag) {
		ptrdiff_t __dist = 0;
		if (__count >= 0) {
			for (; __dist < __count && __iter != __sentinel; ++__dist) { ++__iter; }
		} else {
			for (__count = -__count; __dist < __count && __iter != __sentinel; ++__dist) {
				--__iter;
			}
		}
		return __count - __dist;
	}

	// advance with sentinel, a la std::ranges::advance -- RandomIterator specialization
	template <typename _RandIter>
	constexpr static ptrdiff_t __advance_to(_RandIter &__iter, ptrdiff_t __count,
			const _RandIter &__sentinel, random_access_iterator_tag) {
		auto __dist = _IterOps::distance(__iter, __sentinel);
		if (__count < 0) {
			__dist = __dist > __count ? __dist : __count;
		} else {
			__dist = __dist < __count ? __dist : __count;
		}
		__iter += __dist;
		return __count - __dist;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX___ITERATOR_ITERATOR_OPS_H_
