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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_H_

#include <sprt/cxx/detail/linear_memory_soo.h>
#include <sprt/cxx/detail/pointer_iterator.h>

namespace sprt::detail {

template <typename Type>
struct mem_sso_test {
	static constexpr bool value = sprt::is_scalar<Type>::value;
};

// Zero `n` elements of (raw / just-destroyed) storage at `p`. At runtime this is a
// plain memset; in constant evaluation the void* cast + assignment-to-raw-storage are
// ill-formed, so value-initialise (construct) the elements instead, keeping them live
// so c_str()/data() sees a null terminator in the vacated tail slot.
template <typename Type>
constexpr inline void __zero_raw_tail(Type *p, size_t n) {
	if (sprt::is_constant_evaluated()) {
		for (size_t i = 0; i < n; ++i) { sprt::construct_at(p + i); }
	} else {
		__builtin_memset((void *)p, 0, n * sizeof(Type));
	}
}

template <typename Type, size_t Extra, typename Allocator, bool UseSoo = mem_sso_test<Type>::value>
class linear_memory : public linear_memory_soo<Type, Extra, UseSoo, Allocator> {
public:
	using base = linear_memory_soo<Type, Extra, UseSoo, Allocator>;
	using self = linear_memory<Type, Extra, Allocator, UseSoo>;
	using pointer = Type *;
	using const_pointer = const Type *;
	using reference = Type &;
	using const_reference = const Type &;

	using size_type = size_t;
	using allocator = Allocator;

	using iterator = pointer_iterator<Type, pointer, reference>;
	using const_iterator = pointer_iterator<Type, const_pointer, const_reference>;

	using reverse_iterator = pointer_reverse_iterator<iterator>;
	using const_reverse_iterator = pointer_reverse_iterator<const_iterator>;

	using base::get_soo_size;

	// default init with current context allocator or specified allocator
	constexpr linear_memory(const allocator &alloc = allocator()) noexcept : base(alloc) {
		sprt_passert(_allocator, "Allocator should be defined");
	}

	constexpr linear_memory(pointer p, size_type s, const allocator &alloc) noexcept : linear_memory(alloc) {
		assign(p, s);
	}

	constexpr linear_memory(const_pointer p, size_type s, const allocator &alloc = allocator()) noexcept
	: linear_memory(alloc) {
		assign(p, s);
	}

	constexpr linear_memory(const self &other, size_type pos, size_type len,
			const allocator &alloc = allocator()) noexcept
	: linear_memory(alloc) {
		if (pos < other.size()) {
			assign(other.data() + pos, min(len, other.size() - pos));
		}
	}

	// copy-construct
	constexpr linear_memory(const self &other, const allocator &alloc = allocator()) noexcept
	: linear_memory(alloc) {
		assign(other);
	}

	// move
	// we steal memory block from other, it lifetime is same, or make copy
	constexpr linear_memory(self &&other, const allocator &alloc = allocator()) noexcept
	: linear_memory(alloc) {
		if constexpr (sprt::is_empty_v<allocator>) {
			perform_move(sprt::move_unsafe(other));
		} else if (other._allocator == _allocator) {
			// lifetime is same, steal allocated memory
			perform_move(sprt::move_unsafe(other));
		} else {
			// Allocators differ, so the block can't be stolen. For copyable elements
			// keep the historical copy; for move-only elements (e.g. unique_ptr) the
			// copy is both wrong and ill-formed, so move-relocate element-wise.
			if constexpr (sprt::is_copy_constructible_v<Type>) {
				assign(other);
			} else {
				this->move_assign(_allocator, other.data(), other.size());
			}
		}
		other.clear_dealloc(_allocator);
	}

	constexpr linear_memory &operator=(const self &other) noexcept {
		assign(other);
		return *this;
	}

	constexpr linear_memory &operator=(self &&other) noexcept {
		if constexpr (sprt::is_empty_v<allocator>) {
			// Stateless (always-equal) allocator — always steal (see the move ctor).
			clear_dealloc(_allocator);
			perform_move(sprt::move_unsafe(other));
		} else if (other._allocator == _allocator) {
			// clear and deallocate our memory, self-move-assignment is UB
			clear_dealloc(_allocator);
			perform_move(sprt::move_unsafe(other));
		} else {
			// See the move constructor: copy for copyable elements, move-relocate for
			// move-only ones.
			if constexpr (sprt::is_copy_constructible_v<Type>) {
				assign(other);
			} else {
				this->move_assign(_allocator, other.data(), other.size());
			}
		}
		other.clear_dealloc(_allocator);
		return *this;
	}

	using base::assign;

	constexpr void assign(const self &other) { assign(other.data(), other.size()); }

	constexpr void assign(const self &other, size_type pos, size_type len) {
		if (pos < other.size()) { // guard other.size() - pos underflow / OOB data()+pos
			assign(other.data() + pos, min(len, other.size() - pos));
		} else {
			assign(other.data(), size_type(0));
		}
	}

	template <typename... Args>
	constexpr reference emplace_back(Args &&...args) {
		reserve(size() + 1, true); // reserve should switch mode if required
		return emplace_back_unsafe(sprt::forward<Args>(args)...);
	}

	constexpr void pop_back() {
		if (size() > 0) {
			const auto size = modify_size(-1);
			auto ptr = data() + size;
			_allocator.destroy(ptr);
			__zero_raw_tail(ptr, 1);
		}
	}

	template <typename... Args>
	constexpr reference emplace_back_unsafe(Args &&...args) {
		const auto s = modify_size(1);
		auto ptr = data() + s - 1;
		_allocator.construct(data() + s - 1, sprt::forward<Args>(args)...);
		return *ptr;
	}

	template <typename... Args>
	constexpr iterator emplace(const_iterator it, Args &&...args) {
		const auto _size = size();
		auto _ptr = data();
		size_type pos = it - _ptr;
		if (_size == 0 || pos == _size) {
			emplace_back(sprt::forward<Args>(args)...);
			return iterator(data() + size() - 1);
		} else {
			_ptr = reserve(_size + 1, true);
			__allocator_move_within(_allocator, _ptr + pos + 1, _ptr + pos, _size - pos);
			_allocator.construct(_ptr + pos, sprt::forward<Args>(args)...);
			modify_size(1);
			return iterator(_ptr + pos);
		}
	}

	template <typename... Args>
	constexpr iterator emplace_safe(const_iterator it, Args &&...args) {
		const auto _used = size();
		auto _ptr = data();
		size_type pos = it - _ptr;
		if (_used == 0 || pos == _used) {
			emplace_back(sprt::forward<Args>(args)...);
			return iterator(data() + size() - 1);
		} else {
			_ptr = reserve(_used + 2, true);
			_allocator.construct(_ptr + _used + 1, sprt::forward<Args>(args)...);
			__allocator_move_within(_allocator, _ptr + pos + 1, _ptr + pos, _used - pos);
			__allocator_move_within(_allocator, _ptr + pos, _ptr + _used + 1, 1);
			modify_size(1);
			return iterator(_ptr + pos);
		}
	}

	constexpr void insert_back(const_pointer ptr, size_type s) {
		const auto _used = size();
		// [string.require]: the source may alias this buffer (e.g. s.append(s)); capture
		// its offset before a growth reallocation can move/free the current storage.
		size_type __alias_off = 0;
		const bool __alias = __ptr_index_within(data(), _used, ptr, __alias_off);
		auto _ptr = reserve(_used + s, true);
		const_pointer __src = __alias ? const_pointer(_ptr + __alias_off) : ptr;
		__allocator_copy(_allocator, _ptr + _used, __src, s);
		modify_size(s);
	}

	constexpr void insert_back(const self &other) { insert_back(other.data(), other.size()); }

	constexpr void insert_back(const self &other, size_type pos, size_type len) {
		insert_back(other.data() + pos, sprt::min(other.size() - pos, len));
	}

	constexpr void insert(size_type pos, const_pointer ptr, size_type s) {
		const auto _used = size();
		// Capture a self-aliasing source offset before the growth reallocation (see
		// insert_back). After the shift the [pos, pos+s) hole is filled from the rebased
		// source; a source that overlaps the shifted tail is not supported (matches the
		// prior behaviour) but self-references into the head stay valid.
		size_type __alias_off = 0;
		const bool __alias = __ptr_index_within(data(), _used, ptr, __alias_off);
		auto _ptr = reserve(_used + s, true);
		// The tail [pos, _used) is relocated by +s, so a source that lived there now
		// lives at __alias_off + s; a source in the head [0, pos) is untouched.
		const_pointer __src = __alias
				? const_pointer(_ptr + (__alias_off >= pos ? __alias_off + s : __alias_off))
				: ptr;
		__allocator_move_within(_allocator, _ptr + pos + s, _ptr + pos, _used - pos);
		__allocator_copy(_allocator, _ptr + pos, __src, s);
		modify_size(s);
	}

	constexpr void insert(size_type pos, const self &other) { insert(pos, other.data(), other.size()); }

	constexpr void insert(size_type spos, const self &other, size_type pos, size_type len) {
		insert(spos, other.data() + pos, min(other.size() - pos, len));
	}

	template < typename... Args >
	constexpr void insert(size_type pos, size_type s, Args &&...args) {
		const auto _used = size();
		auto _ptr = reserve(_used + s, true);
		__allocator_move_within(_allocator, _ptr + pos + s, _ptr + pos, _used - pos);
		for (size_type i = pos; i < pos + s; i++) {
			_allocator.construct(_ptr + i, sprt::forward<Args>(args)...);
		}
		modify_size(s);
	}

	template < typename... Args >
	constexpr iterator insert(const_iterator it, size_type len, Args &&...args) {
		size_type pos = it - data();
		insert(pos, len, sprt::forward<Args>(args)...);
		return iterator(data() + pos);
	}

	template < typename InputIt >
	constexpr iterator insert(const_iterator it, InputIt first, InputIt last) {
		auto _ptr = data();
		const auto _used = size();
		auto pos = it - _ptr;
		auto size = sprt::distance(first, last);
		_ptr = reserve(_used + size, true);
		if (size_t(pos) < _used) { // shift the trailing tail (pos/_used are unsigned)
			__allocator_move_within(_allocator, _ptr + pos + size, _ptr + pos, _used - pos);
		}
		auto i = pos;
		for (auto it = first; it != last; ++it, ++i) { _allocator.construct(_ptr + i, *it); }
		modify_size(size);
		return iterator(_ptr + pos);
	}

	constexpr void erase(size_type pos, size_type len) {
		auto _ptr = data();
		const auto _used = size();
		if (pos >= _used) { // nothing to erase; avoids the _used - pos underflow
			return;
		}
		len = min(len, _used - pos);
		_allocator.destroy(_ptr + pos, len); // удаляем указанный блок
		if (pos + len < _used) { // смещаем остаток
			__allocator_move_within(_allocator, _ptr + pos, _ptr + pos + len, _used - pos - len);
		}
		auto s = modify_size(-len);
		__zero_raw_tail(_ptr + s, len);
	}

	constexpr iterator erase(const_iterator it) {
		auto _ptr = data();
		const auto _used = size();
		auto pos = it - _ptr;
		_allocator.destroy(const_cast<pointer>(&(*it)));
		if (pos < _used - 1) {
			__allocator_move_within(_allocator, _ptr + pos, _ptr + pos + 1, _used - pos - 1);
		}
		auto s = modify_size(-1);
		__zero_raw_tail(_ptr + s, 1);
		return iterator(_ptr + pos);
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		auto _ptr = data();
		auto pos = first - _ptr;
		auto len = last - first;
		erase(pos, len);
		return iterator(_ptr + pos);
	}

	constexpr pointer prepare_replace(size_type pos, size_type len, size_type nlen) {
		const auto _used = size();
		auto _ptr = reserve(_used - len + nlen, true);
		_allocator.destroy(_ptr + pos, len);
		if (pos + len < _used) {
			__allocator_move_within(_allocator, _ptr + pos + nlen, _ptr + pos + len,
					_used - pos - len); // смещаем данные
		}
		return _ptr + pos;
	}

	constexpr void replace(size_type pos, size_type len, const_pointer ptr, size_type nlen) {
		const auto _used = size();
		if (pos > _used) { // clamp; avoids the _used - pos underflow
			pos = _used;
		}
		len = min(len, _used - pos);
		__allocator_copy(_allocator, prepare_replace(pos, len, nlen), ptr, nlen);

		const auto s = modify_size(nlen - len);

		if (nlen < len) {
			__zero_raw_tail(data() + s, len - nlen);
		}
	}

	constexpr void replace(size_type pos, size_type len, const self &other) {
		replace(pos, len, other.data(), other.size());
	}
	constexpr void replace(size_type pos, size_type len, const self &other, size_type npos, size_type nlen) {
		replace(pos, len, other.data() + npos, min(other.size() - npos, nlen));
	}

	constexpr void replace(size_type pos, size_type len, size_type nlen, Type t) {
		const auto _used = size();
		if (pos > _used) { // clamp; avoids the _used - pos underflow
			pos = _used;
		}
		len = min(len, _used - pos);
		prepare_replace(pos, len, nlen);
		auto _ptr = data();
		for (size_type i = pos; i < pos + nlen; i++) { _allocator.construct(_ptr + i, t); }

		const auto s = modify_size(nlen - len);

		if (nlen < len) {
			__zero_raw_tail(data() + s, len - nlen);
		}
	}

	template < typename InputIt >
	constexpr iterator replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2) {
		auto pos = size_t(first - data());
		auto len = size_t(last - first);
		auto nlen = sprt::distance(first2, last2);

		prepare_replace(pos, len, nlen);
		auto i = pos;
		auto _ptr = data();
		for (auto it = first2; it != last2; it++, i++) { _allocator.construct(_ptr + i, *it); }

		const auto s = modify_size(nlen - len);

		if (nlen < len) {
			__zero_raw_tail(data() + s, len - nlen);
		}
		return iterator(pos);
	}

	template <typename... Args>
	constexpr void fill(size_type s, Args &&...args) {
		clear();
		auto _ptr = reserve(s, true);
		for (size_type i = 0; i < s; i++) {
			_allocator.construct(_ptr + i, sprt::forward<Args>(args)...);
		}

		set_size(s);
	}

	template <typename... Args>
	constexpr void resize(size_type n, Args &&...args) {
		auto _ptr = reserve(n, true);

		const auto _used = size();
		if (n < _used) {
			if (_ptr) {
				_allocator.destroy(_ptr + n, _used - n);
			}
		} else if (n > _used) {
			for (size_type i = _used; i < n; i++) {
				_allocator.construct(_ptr + i, sprt::forward<Args>(args)...);
			}
		}

		set_size(n);
	}

	using base::reserve;
	using base::clear;
	using base::extract;

	using base::empty;
	using base::data;
	using base::size;
	using base::capacity;

	constexpr reference at(size_type s) noexcept { return data()[s]; }

	constexpr const_reference at(size_type s) const noexcept { return data()[s]; }

	constexpr reference back() noexcept { return *(data() + (size() - 1)); }
	constexpr const_reference back() const noexcept { return *(data() + (size() - 1)); }

	constexpr reference front() noexcept { return *data(); }
	constexpr const_reference front() const noexcept { return *data(); }

	constexpr iterator begin() noexcept { return iterator(data()); }
	constexpr iterator end() noexcept { return iterator(data() + size()); }

	constexpr const_iterator begin() const noexcept { return const_iterator(data()); }
	constexpr const_iterator end() const noexcept { return const_iterator(data() + size()); }

	constexpr const_iterator cbegin() const noexcept { return const_iterator(data()); }
	constexpr const_iterator cend() const noexcept { return const_iterator(data() + size()); }

	constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

	constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

	constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
	constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

	constexpr void shrink_to_fit() noexcept {
		if (size() == 0) {
			clear_dealloc(_allocator);
		}
	}

	constexpr const allocator &get_allocator() const noexcept { return _allocator; }

private:
	using base::perform_move;
	using base::clear_dealloc;
	using base::modify_size;
	using base::set_size;

	using base::_allocator;
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_H_
