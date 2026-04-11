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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_SMALL_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_LINEAR_MEMORY_SMALL_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/array>

namespace sprt::detail {

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

} // namespace sprt::detail

#endif
