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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_ITERATOR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_ITERATOR_H_

#include <sprt/runtime/int.h>

#if __SPRT_USE_STL_ITERATOR_TAGS
#include <iterator>
#endif

namespace sprt {

#if __SPRT_USE_STL_ITERATOR_TAGS
using input_iterator_tag = std::input_iterator_tag;
using output_iterator_tag = std::output_iterator_tag;
using forward_iterator_tag = std::forward_iterator_tag;
using bidirectional_iterator_tag = std::bidirectional_iterator_tag;
using random_access_iterator_tag = std::random_access_iterator_tag;
using contiguous_iterator_tag = std::contiguous_iterator_tag;
#else
struct input_iterator_tag { };
struct output_iterator_tag { };
struct forward_iterator_tag : public input_iterator_tag { };
struct bidirectional_iterator_tag : public forward_iterator_tag { };
struct random_access_iterator_tag : public bidirectional_iterator_tag { };
struct contiguous_iterator_tag : public random_access_iterator_tag { };
#endif

template <typename Iter>
struct _IteratorTraits {
	using iterator_category = typename Iter::iterator_category;
	using value_type = typename Iter::value_type;
};

template <typename Ptr>
struct _IteratorTraits<Ptr *> {
	using iterator_category = random_access_iterator_tag;
	using value_type = Ptr;
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
			typename _IteratorTraits<_InputIter>::iterator_category());
}


template <class _InputIter>
void __advance(_InputIter &__i, ptrdiff_t __n, input_iterator_tag) {
	for (; __n > 0; --__n) { ++__i; }
}

template <class _BiDirIter>
void __advance(_BiDirIter &__i, ptrdiff_t __n, bidirectional_iterator_tag) {
	if (__n >= 0) {
		for (; __n > 0; --__n) { ++__i; }
	} else {
		for (; __n < 0; ++__n) { --__i; }
	}
}

template <class _RandIter>
void __advance(_RandIter &__i, ptrdiff_t __n, random_access_iterator_tag) {
	__i += __n;
}

template < class _InputIter>
void advance(_InputIter &__i, ptrdiff_t __n) {
	sprt::__advance(__i, __n, typename _IteratorTraits<_InputIter>::iterator_category());
}

template <typename Cat1, typename Cat2>
using __has_iterator_category_convertible_to = is_convertible<Cat1, Cat2>;

template <typename Iter>
using __has_input_iterator_category =
		__has_iterator_category_convertible_to<typename Iter::iterator_category,
				input_iterator_tag>;

template <typename Iter>
using __has_forward_iterator_category =
		__has_iterator_category_convertible_to<typename Iter::iterator_category,
				forward_iterator_tag>;

template <typename Iter>
using __has_bidirectional_iterator_category =
		__has_iterator_category_convertible_to<typename Iter::iterator_category,
				bidirectional_iterator_tag>;

template <typename Iter>
using __has_random_access_iterator_category =
		__has_iterator_category_convertible_to<typename Iter::iterator_category,
				random_access_iterator_tag>;

template <typename _ForwardIterator1, typename _ForwardIterator2>
inline constexpr void iter_swap(_ForwardIterator1 __a, _ForwardIterator2 __b) noexcept {
	sprt::swap(*__a, *__b);
}

template <typename Type>
using iterator_traits = _IteratorTraits<Type>;

template < typename C >
constexpr auto begin(C &c) noexcept -> decltype(c.begin()) {
	return c.begin();
}

template < typename C >
constexpr auto begin(const C &c) noexcept -> decltype(c.begin()) {
	return c.begin();
}

template < typename T, size_t N >
T *begin(T (&array)[N]) {
	return &array[0];
}

template < typename C >
constexpr auto end(C &c) noexcept -> decltype(c.end()) {
	return c.end();
}

template < typename C >
constexpr auto end(const C &c) noexcept -> decltype(c.end()) {
	return c.end();
}

template < typename T, size_t N >
T *end(T (&array)[N]) {
	return (&array[N - 1]) + 1;
}

} // namespace sprt

#endif
