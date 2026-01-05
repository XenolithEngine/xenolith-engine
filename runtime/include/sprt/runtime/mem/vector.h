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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_VECTOR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_VECTOR_H_

#include <sprt/runtime/mem/detail/storage.h>
#include <sprt/runtime/hash.h>
#include <sprt/runtime/string.h>
#include <sprt/runtime/detail/compare.h>
#include <sprt/runtime/initializer_list.h>

namespace sprt::memory {

template <typename Type>
class vector : public sprt::memory::AllocPool {
public:
	using allocator_type = sprt::memory::detail::Allocator<Type>;

	using pointer = Type *;
	using const_pointer = const Type *;
	using reference = Type &;
	using const_reference = const Type &;

	using size_type = size_t;
	using value_type = Type;
	using mem_type = detail::storage_mem<Type, 0>;
	using self = vector<Type>;

	using iterator = typename mem_type::iterator;
	using const_iterator = typename mem_type::const_iterator;
	using reverse_iterator = typename mem_type::reverse_iterator;
	using const_reverse_iterator = typename mem_type::const_reverse_iterator;

	vector() noexcept { }
	explicit vector(const allocator_type &alloc) noexcept : _mem(alloc) { }
	explicit vector(size_type count, const Type &value,
			const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		_mem.fill(count, value);
	}
	explicit vector(size_type count, const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		_mem.fill(count);
	}

	template < class InputIt >
	vector(InputIt first, InputIt last, const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		auto size = sprt::distance(first, last);
		_mem.reserve(size);
		for (auto it = first; it != last; it++) { _mem.emplace_back_unsafe(*it); }
	}

	vector(const vector &other) noexcept : _mem(other._mem) { }
	vector(const vector &other, const allocator_type &alloc) noexcept : _mem(other._mem, alloc) { }
	vector(vector &&other) noexcept : _mem(sprt::move_unsafe(other._mem)) { }
	vector(vector &&other, const allocator_type &alloc) noexcept
	: _mem(sprt::move_unsafe(other._mem), alloc) { }
	vector(initializer_list<Type> init, const allocator_type &alloc = allocator_type()) noexcept
	: _mem(alloc) {
		_mem.reserve(init.size());
		for (auto &it : init) { _mem.emplace_back_unsafe(it); }
	}

	vector &operator=(const vector &other) noexcept {
		_mem = other._mem;
		return *this;
	}
	vector &operator=(vector &&other) noexcept {
		_mem = sprt::move_unsafe(other._mem);
		return *this;
	}
	vector &operator=(initializer_list<Type> init) noexcept {
		_mem.clear();
		_mem.reserve(init.size());
		for (auto &it : init) { _mem.emplace_back_unsafe(it); }
		return *this;
	}

	void assign(size_type count, const Type &value) { _mem.fill(count, value); }

	template < class InputIt >
	void assign(InputIt first,
			InputIt last) { // @TODO: self-assign protection and modern assignment
		_mem.clear();
		auto size = sprt::distance(first, last);
		_mem.reserve(size);
		for (auto it = first; it != last; it++) { _mem.emplace_back_unsafe(*it); }
	}

	void assign(initializer_list<Type> init) {
		_mem.clear();
		_mem.reserve(init.size());
		for (auto &it : init) { _mem.emplace_back_unsafe(it); }
	}

	void assign_strong(Type *t, size_type s) { _mem.assign(t, s); }

	allocator_type get_allocator() const noexcept { return _mem.get_allocator(); }

	reference at(size_type pos) noexcept { return _mem.at(pos); }
	const_reference at(size_type pos) const noexcept { return _mem.at(pos); }

	reference operator[](size_type pos) noexcept { return _mem.at(pos); }
	const_reference operator[](size_type pos) const noexcept { return _mem.at(pos); }

	reference front() noexcept { return _mem.front(); }
	const_reference front() const noexcept { return _mem.front(); }

	reference back() noexcept { return _mem.back(); }
	const_reference back() const noexcept { return _mem.back(); }

	pointer data() noexcept { return _mem.data(); }
	const_pointer data() const noexcept { return _mem.data(); }

	iterator begin() noexcept { return _mem.begin(); }
	iterator end() noexcept { return _mem.end(); }

	const_iterator begin() const noexcept { return _mem.begin(); }
	const_iterator end() const noexcept { return _mem.end(); }

	const_iterator cbegin() const noexcept { return _mem.cbegin(); }
	const_iterator cend() const noexcept { return _mem.cend(); }

	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	size_type size() const noexcept { return _mem.size(); }
	size_type capacity() const noexcept { return _mem.capacity(); }
	bool empty() const noexcept { return _mem.empty(); }

	// Reserve capcity, that can be effectively reused by pool allocator.
	// If used carefully, it can provide static memory footprint in memory pool
	void reserve_block_optimal() { _mem.reserve_block_optimal(); }

	void reserve(size_type new_cap) { _mem.reserve(new_cap); }

	void shrink_to_fit() { }

	void clear() { _mem.clear(); }

	template < class... Args >
	iterator emplace(const_iterator pos, Args &&...args) {
		return _mem.emplace(pos, sprt::forward<Args>(args)...);
	}

	iterator insert(const_iterator pos, const Type &value) { return emplace(pos, value); }
	iterator insert(const_iterator pos, Type &&value) {
		return emplace(pos, sprt::move_unsafe(value));
	}
	iterator insert(const_iterator pos, size_type count, const Type &value) {
		return _mem.insert(pos, count, value);
	}

	template < class InputIt >
	iterator insert(const_iterator pos, InputIt first, InputIt last) {
		return _mem.insert(pos, first, last);
	}

	iterator insert(const_iterator pos, initializer_list<Type> init) {
		return _mem.insert(pos, init.begin(), init.end());
	}

	iterator erase(const_iterator pos) { return _mem.erase(pos); }

	iterator erase(const_iterator first, const_iterator last) { return _mem.erase(first, last); }

	template < class... Args >
	reference emplace_back(Args &&...args) {
		return _mem.emplace_back(sprt::forward<Args>(args)...);
	}

	void push_back(const Type &value) { emplace_back(value); }
	void push_back(Type &&value) { emplace_back(sprt::move_unsafe(value)); }

	void pop_back() { _mem.pop_back(); }

	void resize(size_type count) { _mem.resize(count); }
	void resize(size_type count, const value_type &value) { _mem.resize(count, value); }

public:
	static const vector make_weak(const Type *str, size_type l,
			const allocator_type &alloc = allocator_type()) {
		vector ret(alloc);
		if (str) {
			ret.assign_weak(str, l);
		}
		return ret;
	}

	vector &assign_weak(const Type *str, size_type l) {
		_mem.assign_weak(str, l);
		return *this;
	}

	bool is_weak() const noexcept { return _mem.is_weak(); }

	void force_clear() { _mem.force_clear(); }

protected:
	mem_type _mem;
};

using bytes = vector<uint8_t>;

template <typename _Tp>
inline constexpr bool operator==(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return (__x.size() == __y.size() && sprt::equal(__x.begin(), __x.end(), __y.begin()));
}

template <typename _Tp>
inline constexpr bool operator<(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return sprt::lexicographical_compare(__x.begin(), __x.end(), __y.begin(), __y.end());
}

template <>
inline constexpr bool operator==(const vector<uint8_t> &x, const vector<uint8_t> &y) {
	auto len1 = x.size();
	auto len2 = y.size();
	return len1 == len2 && __constexpr_memcmp(x.data(), y.data(), len1) == 0;
}

template <>
inline constexpr bool operator<(const vector<uint8_t> &x, const vector<uint8_t> &y) {
	auto len1 = x.size();
	auto len2 = y.size();
	int result = __constexpr_memcmp(x.data(), y.data(), min(len1, len2));
	if (result != 0) {
		return result < 0;
	}
	if (len1 < len2) {
		return true;
	}
	if (len1 > len2) {
		return false;
	}
	return false;
}

/// Based on operator==
template <typename _Tp>
inline constexpr bool operator!=(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return !(__x == __y);
}

/// Based on operator<
template <typename _Tp>
inline constexpr bool operator>(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return __y < __x;
}

/// Based on operator<
template <typename _Tp>
inline constexpr bool operator<=(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return !(__y < __x);
}

/// Based on operator<
template <typename _Tp>
inline constexpr bool operator>=(const vector<_Tp> &__x, const vector<_Tp> &__y) {
	return !(__x < __y);
}

} // namespace sprt::memory

#endif
