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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_

#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/__iterator/iterator_tags.h>
#include <sprt/cxx/__iterator/iterator_ops.h>

namespace sprt {
inline namespace __cxx_iterator {

// reverse_iterator: adapts a bidirectional/random-access iterator to traverse in
// the opposite direction. *it yields the element before base() (standard offset).
template <class _Iter>
class reverse_iterator {
protected:
	_Iter current;

public:
	typedef _Iter iterator_type;
	typedef typename iterator_traits<_Iter>::iterator_category iterator_category;
	typedef typename iterator_traits<_Iter>::value_type value_type;
	typedef typename iterator_traits<_Iter>::difference_type difference_type;
	typedef typename iterator_traits<_Iter>::pointer pointer;
	typedef typename iterator_traits<_Iter>::reference reference;

	constexpr reverse_iterator() : current() { }
	constexpr explicit reverse_iterator(_Iter __x) : current(__x) { }
	template <class _Up>
	constexpr reverse_iterator(const reverse_iterator<_Up> &__u) : current(__u.base()) { }
	template <class _Up>
	constexpr reverse_iterator &operator=(const reverse_iterator<_Up> &__u) {
		current = __u.base();
		return *this;
	}

	constexpr _Iter base() const { return current; }
	constexpr reference operator*() const {
		_Iter __tmp = current;
		return *--__tmp;
	}
	constexpr pointer operator->() const { return sprt::addressof(operator*()); }

	constexpr reverse_iterator &operator++() {
		--current;
		return *this;
	}
	constexpr reverse_iterator operator++(int) {
		reverse_iterator __t(*this);
		--current;
		return __t;
	}
	constexpr reverse_iterator &operator--() {
		++current;
		return *this;
	}
	constexpr reverse_iterator operator--(int) {
		reverse_iterator __t(*this);
		++current;
		return __t;
	}

	constexpr reverse_iterator operator+(difference_type __n) const {
		return reverse_iterator(current - __n);
	}
	constexpr reverse_iterator &operator+=(difference_type __n) {
		current -= __n;
		return *this;
	}
	constexpr reverse_iterator operator-(difference_type __n) const {
		return reverse_iterator(current + __n);
	}
	constexpr reverse_iterator &operator-=(difference_type __n) {
		current += __n;
		return *this;
	}
	constexpr reference operator[](difference_type __n) const { return *(*this + __n); }
};

template <class _I1, class _I2>
constexpr bool operator==(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() == __b.base();
}
template <class _I1, class _I2>
constexpr bool operator!=(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() != __b.base();
}
template <class _I1, class _I2>
constexpr bool operator<(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() > __b.base();
}
template <class _I1, class _I2>
constexpr bool operator>(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() < __b.base();
}
template <class _I1, class _I2>
constexpr bool operator<=(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() >= __b.base();
}
template <class _I1, class _I2>
constexpr bool operator>=(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b) {
	return __a.base() <= __b.base();
}
template <class _I1, class _I2>
constexpr auto operator-(const reverse_iterator<_I1> &__a, const reverse_iterator<_I2> &__b)
		-> decltype(__b.base() - __a.base()) {
	return __b.base() - __a.base();
}
template <class _Iter>
constexpr reverse_iterator<_Iter> operator+(typename reverse_iterator<_Iter>::difference_type __n,
		const reverse_iterator<_Iter> &__it) {
	return reverse_iterator<_Iter>(__it.base() - __n);
}
template <class _Iter>
constexpr reverse_iterator<_Iter> make_reverse_iterator(_Iter __i) {
	return reverse_iterator<_Iter>(__i);
}

// move_iterator: dereferences the underlying iterator as an rvalue so range
// algorithms move rather than copy the elements.
template <class _Iter>
class move_iterator {
private:
	_Iter current;

public:
	typedef _Iter iterator_type;
	typedef typename iterator_traits<_Iter>::iterator_category iterator_category;
	typedef typename iterator_traits<_Iter>::value_type value_type;
	typedef typename iterator_traits<_Iter>::difference_type difference_type;
	typedef _Iter pointer;
	typedef value_type &&reference;

	constexpr move_iterator() : current() { }
	constexpr explicit move_iterator(_Iter __i) : current(sprt::move_unsafe(__i)) { }
	template <class _Up>
	constexpr move_iterator(const move_iterator<_Up> &__u) : current(__u.base()) { }
	template <class _Up>
	constexpr move_iterator &operator=(const move_iterator<_Up> &__u) {
		current = __u.base();
		return *this;
	}

	constexpr _Iter base() const { return current; }
	constexpr reference operator*() const { return static_cast<reference>(*current); }
	constexpr pointer operator->() const { return current; }

	constexpr move_iterator &operator++() {
		++current;
		return *this;
	}
	constexpr move_iterator operator++(int) {
		move_iterator __t(*this);
		++current;
		return __t;
	}
	constexpr move_iterator &operator--() {
		--current;
		return *this;
	}
	constexpr move_iterator operator--(int) {
		move_iterator __t(*this);
		--current;
		return __t;
	}
	constexpr move_iterator operator+(difference_type __n) const {
		return move_iterator(current + __n);
	}
	constexpr move_iterator &operator+=(difference_type __n) {
		current += __n;
		return *this;
	}
	constexpr move_iterator operator-(difference_type __n) const {
		return move_iterator(current - __n);
	}
	constexpr move_iterator &operator-=(difference_type __n) {
		current -= __n;
		return *this;
	}
	constexpr reference operator[](difference_type __n) const {
		return static_cast<reference>(current[__n]);
	}
};

template <class _I1, class _I2>
constexpr bool operator==(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() == __b.base();
}
template <class _I1, class _I2>
constexpr bool operator!=(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() != __b.base();
}
template <class _I1, class _I2>
constexpr bool operator<(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() < __b.base();
}
template <class _I1, class _I2>
constexpr bool operator>(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() > __b.base();
}
template <class _I1, class _I2>
constexpr bool operator<=(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() <= __b.base();
}
template <class _I1, class _I2>
constexpr bool operator>=(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b) {
	return __a.base() >= __b.base();
}
template <class _I1, class _I2>
constexpr auto operator-(const move_iterator<_I1> &__a, const move_iterator<_I2> &__b)
		-> decltype(__a.base() - __b.base()) {
	return __a.base() - __b.base();
}
template <class _Iter>
constexpr move_iterator<_Iter> operator+(typename move_iterator<_Iter>::difference_type __n,
		const move_iterator<_Iter> &__it) {
	return __it + __n;
}
template <class _Iter>
constexpr move_iterator<_Iter> make_move_iterator(_Iter __i) {
	return move_iterator<_Iter>(sprt::move_unsafe(__i));
}

template <class _Container>
class back_insert_iterator {
protected:
	_Container *container;

public:
	typedef output_iterator_tag iterator_category;
	typedef void value_type;
	typedef ptrdiff_t difference_type;
	typedef void pointer;
	typedef void reference;
	typedef _Container container_type;

	constexpr explicit back_insert_iterator(_Container &__x) : container(sprt::addressof(__x)) { }
	constexpr back_insert_iterator &operator=(const typename _Container::value_type &__value) {
		container->push_back(__value);
		return *this;
	}
	constexpr back_insert_iterator &operator=(typename _Container::value_type &&__value) {
		container->push_back(sprt::move_unsafe(__value));
		return *this;
	}
	constexpr back_insert_iterator &operator*() { return *this; }
	constexpr back_insert_iterator &operator++() { return *this; }
	constexpr back_insert_iterator operator++(int) { return *this; }
	constexpr _Container *__get_container() const { return container; }
};

template <class _Container>
inline constexpr back_insert_iterator<_Container> back_inserter(_Container &__x) {
	return back_insert_iterator<_Container>(__x);
}

template <class _Container>
class front_insert_iterator {
protected:
	_Container *container;

public:
	typedef output_iterator_tag iterator_category;
	typedef void value_type;
	typedef ptrdiff_t difference_type;
	typedef void pointer;
	typedef void reference;
	typedef _Container container_type;

	constexpr explicit front_insert_iterator(_Container &__x) : container(sprt::addressof(__x)) { }
	constexpr front_insert_iterator &operator=(const typename _Container::value_type &__value) {
		container->push_front(__value);
		return *this;
	}
	constexpr front_insert_iterator &operator=(typename _Container::value_type &&__value) {
		container->push_front(sprt::move_unsafe(__value));
		return *this;
	}
	constexpr front_insert_iterator &operator*() { return *this; }
	constexpr front_insert_iterator &operator++() { return *this; }
	constexpr front_insert_iterator operator++(int) { return *this; }
};

template <class _Container>
inline constexpr front_insert_iterator<_Container> front_inserter(_Container &__x) {
	return front_insert_iterator<_Container>(__x);
}

// insert_iterator: inserts into the container at a fixed position, advancing past
// each inserted element (so a range is inserted in order).
template <class _Container>
class insert_iterator {
protected:
	_Container *container;
	typename _Container::iterator iter;

public:
	typedef output_iterator_tag iterator_category;
	typedef void value_type;
	typedef ptrdiff_t difference_type;
	typedef void pointer;
	typedef void reference;
	typedef _Container container_type;

	constexpr insert_iterator(_Container &__x, typename _Container::iterator __i)
	: container(sprt::addressof(__x)), iter(__i) { }
	constexpr insert_iterator &operator=(const typename _Container::value_type &__value) {
		iter = container->insert(iter, __value);
		++iter;
		return *this;
	}
	constexpr insert_iterator &operator=(typename _Container::value_type &&__value) {
		iter = container->insert(iter, sprt::move_unsafe(__value));
		++iter;
		return *this;
	}
	constexpr insert_iterator &operator*() { return *this; }
	constexpr insert_iterator &operator++() { return *this; }
	constexpr insert_iterator &operator++(int) { return *this; }
};

template <class _Container>
inline constexpr insert_iterator<_Container> inserter(_Container &__x,
		typename _Container::iterator __i) {
	return insert_iterator<_Container>(__x, __i);
}

} // namespace __cxx_iterator
} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_BACK_INSERT_H_
