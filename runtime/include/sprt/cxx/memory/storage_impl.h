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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_MEMORY_STORAGE_IMPL_H_
#define RUNTIME_INCLUDE_SPRT_CXX_MEMORY_STORAGE_IMPL_H_

#include <sprt/cxx/memory/allocator_pool.h>
#include <sprt/runtime/endian.h>

namespace sprt::memory {

// small object optimization type (based on SSO-23: https://github.com/elliotgoodrich/SSO-23)
template <typename Type, size_t ByteCount, typename Allocator>
struct linear_memory_small {
	using self = linear_memory_small<Type, ByteCount, Allocator>;
	using pointer = Type *;
	using const_pointer = const Type *;
	using size_type = size_t;
	using allocator = Allocator;

	using strage_type = array<uint8_t, ByteCount>;

	// For both little and big endians
	static constexpr uint8_t CommonSmallMask = 0b1000'0001;

	static constexpr size_type max_capacity() {
		return (sizeof(Type) < ByteCount) ? ((ByteCount - 1) / sizeof(Type)) : 0;
	}

	static void assign(strage_type &storage, allocator &a, const_pointer ptr, size_type s) {
		const auto current = size(storage);
		a.copy_rewrite(data(storage), size(storage), ptr, s);
		if (current > s) {
			a.destroy(data(storage) + s, current - s);
		}
		set_size(storage, s);
	}

	static void move_assign(strage_type &storage, allocator &a, pointer source, size_type count) {
		const auto current = size(storage);
		a.move_rewrite(data(storage), current, source, count);
		if (current > count) {
			a.destroy(data(storage) + count, current - count);
		}
		set_size(storage, count);
	}

	static void clear(strage_type &storage, allocator &a) {
		a.destroy(data(storage), size(storage));
		set_size(storage, 0);
	}

	static void force_clear(strage_type &storage) { set_size(storage, 0); }

	static void drop_unused(strage_type &storage) {
		const auto unused = ((storage.back() & ~CommonSmallMask) >> 1);
		if (unused < max_capacity()) {
			const auto s = max_capacity() - unused;
			// data is already garbage, bypass -Wclass-memaccess
			__builtin_memset((void *)(storage.data() + s * sizeof(Type)), 0, unused * sizeof(Type));
		}
	}

	static void set_size(strage_type &storage, size_t s) {
		storage.back() = uint8_t((max_capacity() - s) << 1) & ~CommonSmallMask;
		drop_unused(storage);
	}

	static size_t modify_size(strage_type &storage, intptr_t diff) {
		auto newSize = size(storage) + diff;
		storage.back() = uint8_t((max_capacity() - newSize) << 1) & ~CommonSmallMask;
		return newSize;
	}

	static size_t size(const strage_type &storage) {
		return max_capacity() - (((storage.back() & ~CommonSmallMask) >> 1));
	}

	static size_t capacity() { return max_capacity(); }

	static pointer data(strage_type &storage) { return (pointer)storage.data(); }
	static const_pointer data(const strage_type &storage) { return (const_pointer)storage.data(); }
};

template <typename Type, size_t Extra, typename Allocator>
class linear_memory_large {
public:
	using self = linear_memory_large<Type, Extra, Allocator>;
	using pointer = Type *;
	using const_pointer = const Type *;
	using size_type = size_t;
	using allocator = Allocator;

	static constexpr size_type wrap_allocated(size_type value) {
		if constexpr (endian::native == endian::little) {
			return (value & (Max<size_type> >> 1)) | size_type(1) << (sizeof(size_type) * 8 - 1);
		} else {
			return (value << 1) | 1;
		}
	}

	static constexpr size_type unwrap_allocated(size_type value) {
		if constexpr (endian::native == endian::little) {
			return (value & (Max<size_type> >> 1));
		} else {
			return (value >> 1 & (Max<size_type> >> 1));
		}
	}

	linear_memory_large() = default;
	linear_memory_large(const self &) = delete;
	self &operator=(const self &other) = delete;

	linear_memory_large(self &&other) = delete;

	self &operator=(self &&other) = delete;

	void assign(allocator &a, const_pointer ptr, size_type count) {
		if (capacity() < count) {
			reserve(a, count);
			if (!_ptr) {
				reserve(a, count);
			}
		}
		a.copy_rewrite(data(), size(), ptr, count);
		if (_used > count) {
			a.destroy(data() + count, _used - count);
		}
		_used = count;
		drop_unused();
	}

	void move_assign(allocator &a, pointer ptr, size_type count) {
		if (capacity() < count) {
			reserve(a, count);
		}
		a.move_rewrite(_ptr, _used, ptr, count);
		if (_used > count) {
			a.destroy(data() + count, _used - count);
		}
		_used = count;
		drop_unused();
	}

	void assign_mem(allocator &a, pointer ptr, size_type s, size_type nalloc) {
		clear_dealloc(a);
		_ptr = ptr;
		_used = s;
		_allocated_with_soo_bit = wrap_allocated(nalloc - Extra);
	}

	void reserve(allocator &a, size_type s) {
		grow_alloc(a, s);
		drop_unused();
	}

	void replace_content(allocator &a, linear_memory_large &other) {
		clear_dealloc(a);
		_ptr = other._ptr;
		other._ptr = nullptr;

		_used = other._used;
		other._used = 0;

		_allocated_with_soo_bit = other._allocated_with_soo_bit;
		other._allocated_with_soo_bit = wrap_allocated(0);
	}

	void clear_dealloc(allocator &a) {
		if (_ptr) {
			if (_used) {
				a.destroy(_ptr, _used);
			}

			auto _allocated = capacity();
			if (_allocated) {
				a.deallocate(_ptr, _allocated + Extra);
			}
		}
		_ptr = nullptr;
		_used = 0;
		_allocated_with_soo_bit = wrap_allocated(0);
	}

	void force_clear() {
		_ptr = nullptr;
		_used = 0;
		_allocated_with_soo_bit = wrap_allocated(0);
	}

	pointer extract() {
		auto ret = _ptr;
		force_clear();
		return ret;
	}

	void drop_unused() {
		auto _allocated = capacity();
		if (_allocated > 0 && _allocated >= _used && _ptr) {
			// data is already garbage, bypass -Wclass-memaccess
			__builtin_memset((void *)(_ptr + _used), 0,
					(_allocated - _used + Extra) * sizeof(Type));
		}
	}

	void grow_alloc(allocator &a, size_type newsize) {
		size_t alloc_size = newsize + Extra;

		// use extra memory if provided by allocator
		size_t allocated = alloc_size * sizeof(Type); // real memory block size returned
		auto ptr = a.__allocate(alloc_size, allocated);

		alloc_size = allocated / sizeof(Type);

		if (_used > 0 && _ptr) {
			a.move(ptr, _ptr, _used);
		}

		auto _allocated = capacity();
		if (_ptr && _allocated > 0) {
			a.deallocate(_ptr, _allocated);
		}

		_ptr = ptr;
		_allocated_with_soo_bit = wrap_allocated(alloc_size - Extra);
	}

	size_t modify_size(intptr_t diff) {
		_used += diff;
		return _used;
	}

	void set_size(size_t s) {
		if (s < _used) {
			_used = s;
			drop_unused();
		} else {
			_used = s;
		}
	}

	size_t size() const noexcept { return _used; }
	size_t capacity() const noexcept { return unwrap_allocated(_allocated_with_soo_bit); }

	pointer data() noexcept { return _ptr; }
	const_pointer data() const noexcept { return _ptr; }

	bool empty() const noexcept { return _ptr == nullptr || _used == 0; }

protected:
	pointer _ptr = nullptr;
	size_type _used = 0; // in elements
	size_type _allocated_with_soo_bit = 0; // in elements
};

// This class routes linear_memory to SOO or non-SOO implementation
// Non-SOO uses linear_memory_large directly, potentially reducing routing overhead
template <typename Type, size_t Extra, bool UseSoo, typename Allocator>
class mem_soo_iface;

template <typename Type, size_t Extra, typename Allocator>
class mem_soo_iface<Type, Extra, false, Allocator>
: public linear_memory_large<Type, Extra, Allocator> {
public:
	using base = linear_memory_large<Type, Extra, Allocator>;
	using pointer = typename base::pointer;
	using const_pointer = typename base::const_pointer;
	using size_type = typename base::size_type;
	using allocator = typename base::allocator;

	static constexpr size_type get_soo_size() { return 0; }

	mem_soo_iface(const allocator &alloc) : _allocator(alloc) {
		sprt_passert(_allocator, "Allocator should be defined");
	}

	~mem_soo_iface() noexcept { clear_dealloc(_allocator); }

	void assign(const_pointer ptr, size_type size) {
		reserve(size, false);
		_allocator.copy_rewrite(_ptr, _used, ptr, size);
		if (_used > size) {
			_allocator.destroy(data() + size, _used - size);
		}
		_used = size;
		drop_unused();
	}

	using base::assign_mem;

	using base::data;
	using base::size;
	using base::capacity;

	// reserve memory block, optimal for realloc/free
	// useful for small temporary buffers
	// this memory block can be reused by next temporary buffer of same size
	// so, no pool memory will be leaked
	pointer reserve_block_optimal() {
		auto target = sprt::memory::config::BlockThreshold / sizeof(Type) + 1;
		return reserve(target);
	}

	pointer reserve(size_type s, bool grow = false) {
		auto _allocated = capacity();
		if (s > 0 && s > _allocated) {
			auto newmem = (grow ? max(s, _allocated * 2) : s);
			base::reserve(_allocator, newmem);
		}
		return _ptr;
	}

	void clear() {
		auto _allocated = capacity();
		if (_used > 0 && _allocated > 0) {
			if (_ptr) {
				_allocator.destroy(_ptr, _used);
			}
		} else {
			if (_allocated == 0) {
				_ptr = nullptr;
			}
		}
		_used = 0;
	}

	using base::force_clear;
	using base::extract;

protected:
	void perform_move(mem_soo_iface &&other) { this->replace_content(_allocator, other); }

	using base::clear_dealloc;
	using base::modify_size;
	using base::set_size;
	using base::drop_unused;

	using base::_ptr;
	using base::_used;

	[[no_unique_address]]
	allocator _allocator;
};

template <typename Type, size_t Extra, typename Allocator>
class mem_soo_iface<Type, Extra, true, Allocator> {
public:
	using pointer = Type *;
	using const_pointer = const Type *;

	using size_type = size_t;
	using allocator = Allocator;

	using large_mem = linear_memory_large<Type, Extra, allocator>;

	static constexpr auto getStorageSize() {
		struct layout_t {
			[[no_unique_address]]
			Allocator alloc;

			large_mem large;
		};

		auto objectSize = sizeof(layout_t);
		auto allocationSlots = (objectSize + alignof(max_align_t) - 1) / alignof(max_align_t);

		return sizeof(large_mem) + (allocationSlots * alignof(max_align_t) - sizeof(layout_t));
	}

	using small_mem = linear_memory_small<Type, getStorageSize(), allocator>;

	static constexpr size_type get_soo_size() { return small_mem::max_capacity(); }

	mem_soo_iface(const allocator &alloc) : _allocator(alloc) {
		sprt_passert(_allocator, "Allocator should be defined");
		new (_storage.data(), sprt::nothrow) large_mem();
		_storage[sizeof(large_mem) - 1] |= getSmallMask();
	}

	~mem_soo_iface() noexcept {
		if (is_large()) {
			auto large = get_large_mem();
			large->clear_dealloc(_allocator);
			large->~large_mem();
			_storage[sizeof(large_mem) - 1] &= ~getSmallMask();
		}
	}

	void assign(const_pointer ptr, size_type size) {
		auto issmall = is_small();
		if (!ptr || size == 0) {
			clear();
		} else if (size <= small_mem::max_capacity()) {
			if (!issmall) {
				auto large = get_large_mem();
				large_mem old_large;
				old_large.replace_content(_allocator, *large);
				switch_to_small_mem();
				small_mem::assign(_storage, _allocator, ptr, size);
				old_large.clear_dealloc(_allocator);
			} else {
				small_mem::assign(_storage, _allocator, ptr, size);
			}
		} else if (size > small_mem::max_capacity()) {
			if (issmall) {
				// we need to switch to large_mem, preserving small_mem until assignment is complete
				large_mem new_large;
				new_large.assign(_allocator, ptr, size);

				// now we can clear small_mem storage and switch to large
				auto large = switch_to_large_mem();
				large->replace_content(_allocator, new_large);
			} else {
				auto large = get_large_mem();
				large->assign(_allocator, ptr, size);
			}
		}
	}

	void assign_mem(pointer ptr, size_type s, size_type nalloc) {
		auto large = switch_to_large_mem();
		large->assign_mem(_allocator, ptr, s, nalloc);
	}

	// reserve memory block, optimal for realloc/free
	// useful for small temporary buffers
	// this memory block can be reused by next temporary buffer of same size
	// so, no pool memory will be leaked
	pointer reserve_block_optimal() {
		auto target = sprt::memory::config::BlockThreshold / sizeof(Type) + 1;
		return reserve(target);
	}

	pointer reserve(size_type s, bool grow = false) {
		const auto _allocated = capacity();
		const auto _used = size();
		if (s > _allocated) {
			const auto smallMaxCap = small_mem::max_capacity();
			if (s <= smallMaxCap && _used <= smallMaxCap) {
				if (_allocated == 0) {
					// Large block with (virtual) CoW flag, rewrite as SSO
					auto large = get_large_mem();
					large_mem old_large;
					old_large.replace_content(_allocator, *large);
					switch_to_small_mem();
					if (old_large.data()) {
						small_mem::move_assign(_storage, _allocator, old_large.data(),
								old_large.size());
					}
					// _allocated == 0, so, large_mem not owns it's data, no cleanup needed
				}
				return small_mem::data(_storage);
			} else if (s > 0) {
				auto newmemsize = (grow ? max(s, _allocated * 2) : s);
				if (is_small()) {
					if (newmemsize > small_mem::max_capacity()) {
						large_mem new_large;
						new_large.reserve(_allocator, newmemsize);
						new_large.move_assign(_allocator, small_mem::data(_storage),
								small_mem::size(_storage));
						auto large = switch_to_large_mem();
						large->replace_content(_allocator, new_large);
						return large->data();
					}
					return small_mem::data(_storage);
				} else {
					auto large = get_large_mem();
					large->reserve(_allocator, newmemsize);
					return large->data();
				}
			}
		}
		return data();
	}

	void clear() {
		if (is_small()) {
			// this will destroy all small_mem contents
			switch_to_large_mem();
		} else {
			auto large = get_large_mem();
			const auto _used = large->size();
			const auto _allocated = large->capacity();
			auto _ptr = large->data();
			if (_used > 0 && _allocated > 0 && _ptr) {
				_allocator.destroy(_ptr, _used);
			} else {
				if (_allocated == 0 && is_large()) {
					// CoW-marked content
					large->force_clear();
				}
			}
			large->set_size(0);
		}
	}

	void force_clear() {
		if (is_small()) {
			switch_to_large_mem();
		}
		get_large_mem()->force_clear();
	}

	pointer extract() {
		if (is_large()) {
			return get_large_mem()->extract();
		} else {
			auto s = small_mem::size(_storage);
			auto ptr = _allocator.allocate(s + Extra);
			_allocator.move(ptr, small_mem::data(_storage), s);
			if constexpr (Extra) {
				// zero-terminated, bypass -Wclass-memaccess
				__builtin_memset((void *)(ptr + s), 0, Extra * sizeof(Type));
			}
			force_clear();
			return ptr;
		}
	}

	pointer data() noexcept {
		return is_large() ? get_large_mem()->data() : small_mem::data(_storage);
	}
	const_pointer data() const noexcept {
		return is_large() ? get_large_mem()->data() : small_mem::data(_storage);
	}

	size_type size() const noexcept {
		return is_large() ? get_large_mem()->size() : small_mem::size(_storage);
	}
	size_type capacity() const noexcept {
		return is_large() ? get_large_mem()->capacity() : small_mem::capacity();
	}

	bool empty() const noexcept {
		return is_large() ? get_large_mem()->empty() : (small_mem::size(_storage) == 0);
	}

protected:
	void perform_move(mem_soo_iface &&other) {
		if (&other == this) {
			return;
		}

		if (other.is_small()) {
			switch_to_small_mem();
			small_mem::force_clear(_storage);
			small_mem::move_assign(_storage, this->_allocator, other.data(), other.size());
			small_mem::force_clear(other._storage);
		} else {
			auto large = switch_to_large_mem();
			large->replace_content(_allocator, *other.get_large_mem());
		}
	}

	void clear_dealloc(allocator &a) {
		if (is_large()) {
			get_large_mem()->clear_dealloc(a);
		} else {
			clear();
		}
	}

	size_type modify_size(intptr_t diff) {
		return is_large() ? get_large_mem()->modify_size(diff)
						  : small_mem::modify_size(_storage, diff);
	}

	void set_size(size_type s) {
		if (is_large()) {
			get_large_mem()->set_size(s);
		} else {
			small_mem::set_size(_storage, s);
		}
	}

	[[no_unique_address]]
	allocator _allocator;

private:
	static constexpr uint8_t getSmallMask() {
		// Bit in getSmallMask should not be used by small or large implementations
		// It should not be affected by anything
		if constexpr (endian::native == endian::little) {
			// For LE - set MSB to 1, MSB is also MSB for _capacity
			return 0b1000'0000;
		} else {
			// For BE - set LSB to 1, LSB is also LSB for _capacity
			return 0b1;
		}
	}

	static constexpr size_type getMemAlignment() {
		return sprt::max(alignof(large_mem), alignof(Type));
	}

	bool is_small() const { return (_storage[sizeof(large_mem) - 1] & getSmallMask()) == 0; }
	bool is_large() const { return (_storage[sizeof(large_mem) - 1] & getSmallMask()) != 0; }

	large_mem *get_large_mem() { return reinterpret_cast<large_mem *>(_storage.data()); }
	const large_mem *get_large_mem() const {
		return reinterpret_cast<const large_mem *>(_storage.data());
	}

	large_mem *switch_to_large_mem() {
		if (is_small()) {
			small_mem::clear(_storage, _allocator); // discard content
			new (_storage.data(), sprt::nothrow) large_mem();
		}

		_storage[sizeof(large_mem) - 1] |= getSmallMask();
		return reinterpret_cast<large_mem *>(_storage.data());
	}

	void switch_to_small_mem() {
		if (is_large()) {
			auto large = reinterpret_cast<large_mem *>(_storage.data());
			large->clear_dealloc(_allocator);
			large->~large_mem();
			small_mem::force_clear(_storage);
		}

		_storage[sizeof(large_mem) - 1] &= ~getSmallMask();
	}

	// Debug assist: allow user to read large_mem and small_mem contents
	// clang-format off
#if DEBUG
	struct small_mem_DEBUG {
		Type values[small_mem::max_capacity()];
		Type counter;
	};

	struct large_mem_DEBUG {
		pointer ptr;
		size_type used;
		size_type allocated : sizeof(size_type) * 8 - 1;
		size_type soo_bit : 1;
	};

	union {
#endif

	alignas(getMemAlignment()) array<uint8_t, getStorageSize()> _storage;

#if DEBUG
		large_mem_DEBUG large;
		small_mem_DEBUG small;
	};
#endif
	// clang-format on
};

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_STORAGE_IMPL_H_
