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

#ifndef STAPPLER_CORE_MEMORY_SPMEMFORWARDLIST_H_
#define STAPPLER_CORE_MEMORY_SPMEMFORWARDLIST_H_

#include "detail/SPMemListBase.h"

namespace STAPPLER_VERSIONIZED stappler::memory {

template <typename T>
class forward_list : public AllocPool {
public:
	using value_type = T;
	using allocator_type = detail::Allocator<T>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = typename allocator_type::pointer;
	using const_pointer = typename allocator_type::const_pointer;

	using node_type = detail::ForwardListNode<value_type>;
	using base_type = detail::list_base<node_type>;

	using iterator = detail::ForwardListIterator<value_type>;
	using const_iterator = detail::ForwardListConstIterator<value_type>;

	forward_list() noexcept : forward_list(allocator_type()) { }

	explicit forward_list(const allocator_type &alloc) noexcept : _base(alloc) { }

	explicit forward_list(size_type count, const allocator_type &alloc = allocator_type()) noexcept;

	forward_list(size_type count, const value_type &value,
			const allocator_type &alloc = allocator_type()) noexcept;

	template <typename InputIt>
	forward_list(InputIt first, InputIt last,
			const allocator_type &alloc = allocator_type()) noexcept;

	forward_list(const forward_list &other) noexcept;
	forward_list(forward_list &&other) noexcept;

	forward_list(const forward_list &other, const allocator_type &alloc) noexcept;
	forward_list(forward_list &&other, const allocator_type &alloc) noexcept;

	forward_list(std::initializer_list<T> init,
			const allocator_type &alloc = allocator_type()) noexcept;

	~forward_list() noexcept;

	forward_list &operator=(const forward_list &other) noexcept;
	forward_list &operator=(forward_list &&other) noexcept;
	forward_list &operator=(std::initializer_list<value_type> ilist) noexcept;

	void assign(size_type count, const T &value) noexcept;

	template <typename InputIt>
	void assign(InputIt first, InputIt last) noexcept;
	void assign(std::initializer_list<T> ilist) noexcept;

	allocator_type get_allocator() const noexcept { return _base.get_allocator(); }

	reference front() noexcept { return _base.front()->value.ref(); }
	const_reference front() const noexcept { return _base.front()->value.ref(); }

	reference back() noexcept { return _base.back()->value.ref(); }
	const_reference back() const noexcept { return _base.back()->value.ref(); }

	iterator before_begin() noexcept { return iterator(_base.front_location()); }
	const_iterator before_begin() const noexcept { return const_iterator(_base.front_location()); }
	const_iterator cbefore_begin() const noexcept { return const_iterator(_base.front_location()); }

	iterator begin() noexcept { return iterator(_base.front()); }
	const_iterator begin() const noexcept { return const_iterator(_base.front()); }
	const_iterator cbegin() const noexcept { return const_iterator(_base.front()); }

	iterator end() noexcept { return iterator(); }
	const_iterator end() const noexcept { return const_iterator(); }
	const_iterator cend() const noexcept { return const_iterator(); }

	bool empty() const noexcept { return _base.front() != nullptr; }
	size_type max_size() const noexcept { return maxOf<size_type>() / sizeof(node_type); }
	size_type size() const noexcept { return _base.size(); }

	void clear() noexcept { _base.clear(); }

	template < class... Args >
	reference emplace_front(Args &&...args) noexcept;

	reference push_front(const T &value) noexcept;
	reference push_front(T &&value) noexcept;

	template < class... Args >
	reference emplace_back(Args &&...args) noexcept;

	reference push_back(const T &value) noexcept;
	reference push_back(T &&value) noexcept;

	iterator insert_after(const_iterator pos, const T &value) noexcept;
	iterator insert_after(const_iterator pos, T &&value) noexcept;
	iterator insert_after(const_iterator pos, size_type count, const T &value) noexcept;

	template <typename InputIt>
	iterator insert_after(const_iterator pos, InputIt first, InputIt last) noexcept;
	iterator insert_after(const_iterator pos, std::initializer_list<T> ilist) noexcept;

	template < class... Args >
	iterator emplace_after(const_iterator pos, Args &&...args) noexcept;

	iterator erase_after(const_iterator pos) noexcept;
	iterator erase_after(const_iterator first, const_iterator last) noexcept;

	void pop_front() noexcept;

	void resize(size_type count) noexcept;
	void resize(size_type count, const value_type &value) noexcept;

	/*void swap(forward_list &other) noexcept;

	void merge(forward_list &other) noexcept;
	void merge(forward_list &&other) noexcept;
	template < class Compare >
	void merge(forward_list &other, Compare comp) noexcept;
	template < class Compare >
	void merge(forward_list &&other, Compare comp) noexcept;

	void splice_after(const_iterator pos, forward_list &other) noexcept;
	void splice_after(const_iterator pos, forward_list &&other) noexcept;
	void splice_after(const_iterator pos, forward_list &other, const_iterator it) noexcept;
	void splice_after(const_iterator pos, forward_list &&other, const_iterator it) noexcept;
	void splice_after(const_iterator pos, forward_list &other, const_iterator first,
			const_iterator last) noexcept;
	void splice_after(const_iterator pos, forward_list &&other, const_iterator first,
			const_iterator last) noexcept;

	size_type remove(const T &value) noexcept;

	template <class UnaryPred>
	void remove_if(UnaryPred p) noexcept;

	template <class UnaryPred>
	size_type remove_if(UnaryPred p) noexcept;

	void reverse() noexcept;

	size_type unique() noexcept;

	template < class BinaryPred >
	size_type unique(BinaryPred p) noexcept;

	void sort();

	template < class Compare >
	void sort(Compare comp);*/

protected:
	base_type _base;
};

template <class T>
inline bool operator==(const forward_list<T> &lhs, const forward_list<T> &rhs) {
	auto first1 = lhs.begin();
	auto last1 = lhs.end();
	auto first2 = rhs.begin();
	auto last2 = rhs.end();

	for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
		if (!(*first1 == *first2)) {
			return false;
		}
	}

	return first1 == last1 && first2 == last2;
}

template <class T>
inline auto operator<=>(const forward_list<T> &lhs, const forward_list<T> &rhs) {
	return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T>
inline forward_list<T>::forward_list(size_type count, const allocator_type &alloc) noexcept
: forward_list(alloc) {
	_base.expand_front(count,
			[](const base_type::node_allocator_type &nalloc, node_type *node) SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr());
	});
}

template <class T>
inline forward_list<T>::forward_list(size_type count, const value_type &value,
		const allocator_type &alloc) noexcept
: forward_list(alloc) {
	_base.expand_front(count,
			[value = &value](const base_type::node_allocator_type &nalloc, node_type *node)
					SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr(), *value);
	});
}

template <class T>
template <typename InputIt>
inline forward_list<T>::forward_list(InputIt first, InputIt last,
		const allocator_type &alloc) noexcept
: forward_list(alloc) {
	// TODO: we can optimize insertion with _base.expand when std::distance for iterators is constant-time
	auto iter = before_begin();
	while (first != last) {
		iter = emplace_after(iter, *first);
		++first;
	}
}


template <class T>
inline forward_list<T>::forward_list(const forward_list &other) noexcept
: forward_list(other, allocator_type()) { }

template <class T>
inline forward_list<T>::forward_list(forward_list &&other) noexcept
: forward_list(sp::move(other), allocator_type()) { }

template <class T>
inline forward_list<T>::forward_list(const forward_list &other,
		const allocator_type &alloc) noexcept
: _base(other._base, alloc) { }

template <class T>
inline forward_list<T>::forward_list(forward_list &&other, const allocator_type &alloc) noexcept
: _base(sp::move(other), alloc) { }

template <class T>
inline forward_list<T>::forward_list(std::initializer_list<T> init,
		const allocator_type &alloc) noexcept
: forward_list(alloc) {
	auto source = init.begin();
	_base.expand_front(init.size(),
			[&](const base_type::node_allocator_type &nalloc, node_type *node) SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr(), *source++);
	});
}

template <class T>
inline forward_list<T>::~forward_list() noexcept { }

template <class T>
inline auto forward_list<T>::operator=(const forward_list &other) noexcept -> forward_list & {
	_base = other._base;
	return *this;
}

template <class T>
inline auto forward_list<T>::operator=(forward_list &&other) noexcept -> forward_list & {
	_base = sp::move(other._base);
	return *this;
}


template <class T>
inline auto forward_list<T>::operator=(std::initializer_list<value_type> init) noexcept
		-> forward_list & {
	assign(init);
	return *this;
}

template <class T>
inline void forward_list<T>::assign(size_type count, const T &value) noexcept {
	auto preallocTmp = _base.memory_persistent();
	_base.set_memory_persistent(true);
	_base.clear();
	_base.set_memory_persistent(preallocTmp);

	_base.expand_front(count,
			[](const base_type::node_allocator_type &nalloc, node_type *node) SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr());
	});
}

template <class T>
template <typename InputIt>
inline void forward_list<T>::assign(InputIt first, InputIt last) noexcept {
	auto preallocTmp = _base.memory_persistent();
	_base.set_memory_persistent(true);
	_base.clear();
	_base.set_memory_persistent(preallocTmp);

	auto iter = before_begin();
	while (first != last) {
		iter = emplace_after(iter, *first);
		++first;
	}
}

template <class T>
inline void forward_list<T>::assign(std::initializer_list<T> init) noexcept {
	auto preallocTmp = _base.memory_persistent();
	_base.set_memory_persistent(true);
	_base.clear();
	_base.set_memory_persistent(preallocTmp);

	auto source = init.begin();
	_base.expand_front(init.size(),
			[&](const base_type::node_allocator_type &nalloc, node_type *node) SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr(), *source++);
	});
}

template <class T>
template < class... Args >
forward_list<T>::reference forward_list<T>::emplace_front(Args &&...args) noexcept {
	auto node = _base.allocate_node();
	get_allocator().construct(node->value.ptr(), std::forward<Args>(args)...);
	_base.insert_front(node);
	return node->value.ref();
}

template <class T>
forward_list<T>::reference forward_list<T>::push_front(const T &value) noexcept {
	return emplace_front(value);
}

template <class T>
forward_list<T>::reference forward_list<T>::push_front(T &&value) noexcept {
	return emplace_front(sp::move_unsafe(value));
}

template <class T>
template < class... Args >
forward_list<T>::reference forward_list<T>::emplace_back(Args &&...args) noexcept {
	auto node = _base.allocate_node();
	get_allocator().construct(node->value.ptr(), std::forward<Args>(args)...);
	_base.insert(_base.back_location(), node);
	return node->value.ref();
}

template <class T>
forward_list<T>::reference forward_list<T>::push_back(const T &value) noexcept {
	return emplace_back(value);
}

template <class T>
forward_list<T>::reference forward_list<T>::push_back(T &&value) noexcept {
	return emplace_back(sp::move_unsafe(value));
}

template <class T>
forward_list<T>::iterator forward_list<T>::insert_after(const_iterator pos,
		const T &value) noexcept {
	auto node = _base.allocate_node();
	get_allocator().construct(node->value.ptr(), value);
	_base.insert(pos.__next);
	return iterator(node);
}

template <class T>
forward_list<T>::iterator forward_list<T>::insert_after(const_iterator pos, T &&value) noexcept {
	auto node = _base.allocate_node();
	get_allocator().construct(node->value.ptr(), sp::move_unsafe(value));
	_base.insert(pos.__next);
	return iterator(node);
}

template <class T>
forward_list<T>::iterator forward_list<T>::insert_after(const_iterator pos, size_type count,
		const T &value) noexcept {
	auto node = _base.expand(pos.__next, count,
			[value = &value](const base_type::node_allocator_type &nalloc, node_type *node)
					SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr(), *value);
	});
	return iterator(node);
}

template <class T>
template <typename InputIt>
forward_list<T>::iterator forward_list<T>::insert_after(const_iterator iter, InputIt first,
		InputIt last) noexcept {
	while (first != last) {
		iter = emplace_after(iter, *first);
		++first;
	}
	return iter;
}

template <class T>
forward_list<T>::iterator forward_list<T>::insert_after(const_iterator pos,
		std::initializer_list<T> init) noexcept {
	auto source = init.begin();
	auto node = _base.expand_front(init.size(),
			[&](const base_type::node_allocator_type &nalloc, node_type *node) SPINLINE {
		auto alloc = allocator_type(nalloc);
		alloc.construct(node->value.ptr(), *source++);
	});
	return iterator(node);
}

template <class T>
template < class... Args >
forward_list<T>::iterator forward_list<T>::emplace_after(const_iterator pos,
		Args &&...args) noexcept {
	auto node = _base.allocate_node();
	get_allocator().construct(node->value.ptr(), std::forward<Args>(args)...);
	_base.insert(const_cast<node_type **>(pos.__next), node);
	return iterator(node);
}

template <class T>
forward_list<T>::iterator forward_list<T>::erase_after(const_iterator pos) noexcept {
	auto node = _base.erase_after(pos.__next);
	return iterator(node);
}

template <class T>
forward_list<T>::iterator forward_list<T>::erase_after(const_iterator first,
		const_iterator last) noexcept {
	while (first != last) { first = (_base.erase_after(first.__next)); }
	return first;
}

template <class T>
void forward_list<T>::pop_front() noexcept {
	_base.erase_after(before_begin().__next);
}

template <class T>
void forward_list<T>::resize(size_type count) noexcept {
	if (_base.size() > count) {
		auto target = before_begin();
		while (count > 0) {
			++target;
			--count;
		}

		while (*target.__next) { _base.erase_after(target.__next); }
	} else if (_base.size() < count) {
		_base.expand(&_base.back()->next, count - _base.size(),
				[](const base_type::node_allocator_type &nalloc, node_type *node) {
			auto alloc = allocator_type(nalloc);
			alloc.construct(node->value.ptr());
		});
	}
}

template <class T>
void forward_list<T>::resize(size_type count, const value_type &value) noexcept {
	if (_base.size() > count) {
		auto target = before_begin();
		while (count > 0) {
			++target;
			--count;
		}

		while (*target.__next) { _base.erase_after(target.__next); }
	} else if (_base.size() < count) {
		_base.expand(&_base.back()->next, count - _base.size(),
				[value = &value](const base_type::node_allocator_type &nalloc, node_type *node) {
			auto alloc = allocator_type(nalloc);
			alloc.construct(node->value.ptr(), *value);
		});
	}
}

} // namespace stappler::memory

#endif // STAPPLER_CORE_MEMORY_SPMEMFORWARDLIST_H_
