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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_LARGE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_LARGE_H_

#include <sprt/cxx/bit>
#include <sprt/cxx/detail/ctypes.h>

namespace sprt::detail {

template <typename Type, size_t Extra, typename Allocator, bool UseSoo>
class linear_memory_large {
public:
	using self = linear_memory_large<Type, Extra, Allocator, UseSoo>;
	using pointer = Type *;
	using const_pointer = const Type *;
	using size_type = size_t;
	using allocator = Allocator;

	static constexpr size_type wrap_allocated(size_type value) {
		if constexpr (UseSoo) {
			if constexpr (endian::native == endian::little) {
				return (value & (Max<size_type> >> 1))
						| size_type(1) << (sizeof(size_type) * 8 - 1);
			} else {
				return (value << 1) | 1;
			}
		} else {
			return value;
		}
	}

	static constexpr size_type unwrap_allocated(size_type value) {
		if constexpr (UseSoo) {
			if constexpr (endian::native == endian::little) {
				return (value & (Max<size_type> >> 1));
			} else {
				return (value >> 1 & (Max<size_type> >> 1));
			}
		} else {
			return value;
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

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_LARGE_H_
