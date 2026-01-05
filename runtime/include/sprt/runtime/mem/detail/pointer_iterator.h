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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_POINTER_ITERATOR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_POINTER_ITERATOR_H_

#include <sprt/runtime/iterator.h>
#include <sprt/runtime/detail/constructable.h>

namespace sprt::memory::detail {

template <class Type, class Pointer, class Reference>
class pointer_iterator {
public:
	using iterator_category = random_access_iterator_tag;

	using size_type = size_t;
	using pointer = Pointer;
	using reference = Reference;
	using iterator = pointer_iterator<Type, Pointer, Reference>;
	using difference_type = ptrdiff_t;
	using value_type = typename remove_cv<Type>::type;

	pointer_iterator() noexcept : current(nullptr) { }
	pointer_iterator(const iterator &other) noexcept : current(other.current) { }
	explicit pointer_iterator(pointer p) noexcept : current(p) { }

	iterator &operator=(const iterator &other) noexcept {
		current = other.current;
		return *this;
	}
	bool operator==(const iterator &other) const { return current == other.current; }
	bool operator!=(const iterator &other) const { return current != other.current; }
	bool operator<(const iterator &other) const { return current < other.current; }
	bool operator>(const iterator &other) const { return current > other.current; }
	bool operator<=(const iterator &other) const { return current <= other.current; }
	bool operator>=(const iterator &other) const { return current >= other.current; }

	iterator &operator++() {
		++current;
		return *this;
	}
	iterator operator++(int) {
		auto tmp = *this;
		++current;
		return tmp;
	}
	iterator &operator--() {
		--current;
		return *this;
	}
	iterator operator--(int) {
		auto tmp = *this;
		--current;
		return tmp;
	}
	iterator &operator+=(size_type n) {
		current += n;
		return *this;
	}
	iterator &operator-=(size_type n) {
		current -= n;
		return *this;
	}
	difference_type operator-(const iterator &other) const { return current - other.current; }

	reference operator*() const { return *current; }
	pointer operator->() const { return current; }
	reference operator[](size_type n) const { return *(current + n); }

	size_type operator-(pointer p) const { return current - p; }

	// const_iterator cast
	operator pointer_iterator<value_type, const value_type *, const value_type &>() const {
		return pointer_iterator<value_type, const value_type *, const value_type &>(current);
	}

	//operator pointer () const { return current; }

	friend auto operator+(size_type n, const iterator &it) -> iterator {
		return iterator(it.current + n);
	}

	friend auto operator+(const iterator &it, size_type n) -> iterator {
		return iterator(it.current + n);
	}

	friend auto operator-(const iterator &it, size_type n) -> iterator {
		return iterator(it.current - n);
	}

protected:
	pointer current;
};

template <typename _Iter>
class pointer_reverse_iterator {
public:
	using iterator_type = _Iter;
	using iterator_category = _Iter::iterator_category;

	using pointer = typename _Iter::pointer;
	using value_type = typename _Iter::value_type;
	using difference_type = typename _Iter::difference_type;
	using reference = typename _Iter::reference;

	constexpr pointer_reverse_iterator() : current() { }

	constexpr explicit pointer_reverse_iterator(_Iter __x) : current(__x) { }

	template <class _Up,
			enable_if_t<!is_same<_Up, _Iter>::value && is_convertible<_Up const &, _Iter>::value,
					int> = 0>
	constexpr pointer_reverse_iterator(const pointer_reverse_iterator<_Up> &__u)
	: current(__u.base()) { }

	template <class _Up,
			enable_if_t<!is_same<_Up, _Iter>::value && is_convertible<_Up const &, _Iter>::value
							&& is_assignable<_Iter &, _Up const &>::value,
					int> = 0>
	constexpr pointer_reverse_iterator &operator=(const pointer_reverse_iterator<_Up> &__u) {
		return *this;
	}

	constexpr _Iter base() const { return current; }
	constexpr reference operator*() const {
		_Iter __tmp = current;
		return *--__tmp;
	}

	constexpr pointer operator->() const
			requires is_pointer_v<_Iter> || requires(const _Iter __i) { __i.operator->(); }
	{
		_Iter __tmp = current;
		--__tmp;
		if constexpr (is_pointer_v<_Iter>) {
			return __tmp;
		} else {
			return __tmp.operator->();
		}
	}

	constexpr pointer_reverse_iterator &operator++() {
		--current;
		return *this;
	}
	constexpr pointer_reverse_iterator operator++(int) {
		pointer_reverse_iterator __tmp(*this);
		--current;
		return __tmp;
	}
	constexpr pointer_reverse_iterator &operator--() {
		++current;
		return *this;
	}
	constexpr pointer_reverse_iterator operator--(int) {
		pointer_reverse_iterator __tmp(*this);
		++current;
		return __tmp;
	}
	constexpr pointer_reverse_iterator operator+(difference_type __n) const {
		return pointer_reverse_iterator(current - __n);
	}
	constexpr pointer_reverse_iterator &operator+=(difference_type __n) {
		current -= __n;
		return *this;
	}
	constexpr pointer_reverse_iterator operator-(difference_type __n) const {
		return pointer_reverse_iterator(current + __n);
	}
	constexpr pointer_reverse_iterator &operator-=(difference_type __n) {
		current += __n;
		return *this;
	}
	constexpr reference operator[](difference_type __n) const { return *(*this + __n); }

protected:
	_Iter current;
};

template <typename _Iter>
class common_reverse_iterator {
public:
	using iterator_type = _Iter;
	using iterator_category = _Iter::iterator_category;

	using pointer = typename _Iter::pointer;
	using value_type = typename _Iter::value_type;
	using difference_type = typename _Iter::difference_type;
	using reference = typename _Iter::reference;

	constexpr common_reverse_iterator() : current() { }

	constexpr explicit common_reverse_iterator(_Iter __x) : current(__x) { }

	template <class _Up,
			enable_if_t<!is_same<_Up, _Iter>::value && is_convertible<_Up const &, _Iter>::value,
					int> = 0>
	constexpr common_reverse_iterator(const common_reverse_iterator<_Up> &__u)
	: current(__u.base()) { }

	template <class _Up,
			enable_if_t<!is_same<_Up, _Iter>::value && is_convertible<_Up const &, _Iter>::value
							&& is_assignable<_Iter &, _Up const &>::value,
					int> = 0>
	constexpr common_reverse_iterator &operator=(const common_reverse_iterator<_Up> &__u) {
		return *this;
	}

	constexpr _Iter base() const { return current; }
	constexpr reference operator*() const {
		_Iter __tmp = current;
		return *--__tmp;
	}

	constexpr pointer operator->() const
			requires is_pointer_v<_Iter> || requires(const _Iter __i) { __i.operator->(); }
	{
		_Iter __tmp = current;
		--__tmp;
		if constexpr (is_pointer_v<_Iter>) {
			return __tmp;
		} else {
			return __tmp.operator->();
		}
	}

	constexpr common_reverse_iterator &operator++() {
		--current;
		return *this;
	}
	constexpr common_reverse_iterator operator++(int) {
		common_reverse_iterator __tmp(*this);
		--current;
		return __tmp;
	}
	constexpr common_reverse_iterator &operator--() {
		++current;
		return *this;
	}
	constexpr common_reverse_iterator operator--(int) {
		common_reverse_iterator __tmp(*this);
		++current;
		return __tmp;
	}

protected:
	_Iter current;
};

} // namespace sprt::memory::detail

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_POINTER_ITERATOR_H_
