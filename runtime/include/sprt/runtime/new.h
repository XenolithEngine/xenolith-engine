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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/detail/new.h>
#include <sprt/c/__sprt_stdlib.h>

[[nodiscard]]
inline constexpr void *operator new(sprt::size_t size, sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

[[nodiscard]]
inline constexpr void *operator new[](sprt::size_t size, sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

[[nodiscard]]
inline constexpr void *operator new(sprt::size_t size, void *ptr, sprt::nothrow_t) noexcept {
	return ptr;
}

[[nodiscard]]
inline constexpr void *operator new[](sprt::size_t size, void *ptr, sprt::nothrow_t) noexcept {
	return ptr;
}

[[nodiscard]]
inline constexpr void *operator new(sprt::size_t size, sprt::align_val_t al,
		sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

[[nodiscard]]
inline constexpr void *operator new[](sprt::size_t size, sprt::align_val_t al,
		sprt::nothrow_t) noexcept {
	if (size == 0) {
		size = 1;
	}
	return __sprt_malloc(size);
}

inline void operator delete(void *ptr, const sprt::nothrow_t &) noexcept { __sprt_free(ptr); }
inline void operator delete[](void *ptr, const sprt::nothrow_t &) noexcept { __sprt_free(ptr); }

inline void operator delete(void *ptr, sprt::align_val_t al) noexcept { __sprt_free(ptr); }
inline void operator delete[](void *ptr, sprt::align_val_t al) noexcept { __sprt_free(ptr); }

inline void operator delete(void *ptr, sprt::align_val_t al, const sprt::nothrow_t &) noexcept {
	__sprt_free(ptr);
}
inline void operator delete[](void *ptr, sprt::align_val_t al, const sprt::nothrow_t &) noexcept {
	__sprt_free(ptr);
}


namespace sprt {

struct SPRT_API AllocPlacement {
	static void *operator new(size_t size, void *ptr) noexcept { return ptr; }
	static void *operator new[](size_t size, void *ptr) noexcept { return ptr; }

	inline void operator delete(void *, void *) noexcept { }
	inline void operator delete[](void *, void *) noexcept { }
};

struct SPRT_API AllocBase : public AllocPlacement {
	static void *operator new(size_t size) = delete;
	static void *operator new[](size_t size) = delete;

	static void *operator new(size_t size, const sprt::nothrow_t &tag) noexcept {
		return ::operator new(size, tag);
	}
	static void *operator new(size_t size, sprt::align_val_t al,
			const sprt::nothrow_t &tag) noexcept {
		return ::operator new(size, al, tag);
	}

	// Allocation from pool is forbidden for common classes
	static void *operator new(size_t size, memory::pool_t *ptr) noexcept = delete;

	static void operator delete(void *ptr) noexcept { return ::operator delete(ptr); }
	static void operator delete(void *ptr, sprt::align_val_t al) noexcept {
		return ::operator delete(ptr, al);
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_NEW_NOTHROW_H_
