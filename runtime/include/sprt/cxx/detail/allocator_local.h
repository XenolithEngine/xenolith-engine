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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_LOCAL_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_LOCAL_H_

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_assert.h>

#include <sprt/cxx/new>
#include <sprt/cxx/__memory/common.h>
#include <sprt/cxx/detail/allocator_utils.h>

namespace sprt::detail {

/*
	AllocatorLocal is a special variant of AllocatorMalloc to use within
	freestanding context before actual malloc is initialized.
*/
template <typename T>
class AllocatorLocal {
public:
	using base_class = AllocBase;

	using pointer = T *;
	using const_pointer = const T *;

	using void_pointer = void *;
	using const_void_pointer = const void *;

	using reference = T &;
	using const_reference = const T &;

	using value_type = T;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	template <typename U>
	struct rebind {
		using other = AllocatorLocal<U>;
	};

public:
	// Default allocator uses pool from top of thread's AllocStack
	constexpr AllocatorLocal() noexcept { }

	template <typename B>
	constexpr AllocatorLocal(const AllocatorLocal<B> &a) noexcept { }
	template <typename B>
	constexpr AllocatorLocal(AllocatorLocal<B> &&a) noexcept { }

	template <typename B>
	constexpr AllocatorLocal<T> &operator=(const AllocatorLocal<B> &a) noexcept {
		return *this;
	}
	template <typename B>
	constexpr AllocatorLocal<T> &operator=(AllocatorLocal<B> &&a) noexcept {
		return *this;
	}

	constexpr T *allocate(size_t n) const noexcept;
	constexpr T *__allocate(size_t &n) const noexcept;
	constexpr T *__allocate(size_t n, size_t &bytes) const noexcept;
	constexpr void deallocate(T *t, size_t n) const noexcept;
	constexpr void __deallocate(T *t, size_t n, size_t bytes) const noexcept;

	template <typename B>
	constexpr inline bool operator==(const AllocatorLocal<B> &p) const noexcept {
		return true;
	}

	template <typename B>
	constexpr inline bool operator!=(const AllocatorLocal<B> &p) const noexcept {
		return false;
	}

	constexpr inline pointer address(reference r) const noexcept { return &r; }
	constexpr inline const_pointer address(const_reference r) const noexcept { return &r; }

	constexpr size_type max_size() const noexcept { return Max<size_type>; }

	template <typename... Args>
	constexpr void construct(pointer p, Args &&...args) const noexcept;

	constexpr void destroy(pointer p) const noexcept;

	constexpr void destroy(pointer p, size_t size) const noexcept;

	constexpr explicit operator bool() const noexcept { return true; }
};

//
// Implementation details
//

template <typename T>
constexpr inline auto AllocatorLocal<T>::allocate(size_t n) const noexcept -> T * {
	T *ptr = n == 1 ? sprt::memory::local_allocate<T>() : sprt::memory::allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
constexpr inline auto AllocatorLocal<T>::__allocate(size_t &n) const noexcept -> T * {
	T *ptr = sprt::memory::local_allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
constexpr inline auto AllocatorLocal<T>::__allocate(size_t n, size_t &bytes) const noexcept -> T * {
	T *ptr = sprt::memory::local_allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	bytes = n * sizeof(T);
	return ptr;
}

template <typename T>
constexpr inline void AllocatorLocal<T>::deallocate(T *t, size_t n) const noexcept {
	sprt::memory::local_deallocate<T>(t, n, n * sizeof(T));
}

template <typename T>
constexpr inline void AllocatorLocal<T>::__deallocate(T *t, size_t n, size_t bytes) const noexcept {
	sprt::memory::local_deallocate<T>(t, n, bytes);
}

template <typename T>
template <typename... Args>
constexpr inline void AllocatorLocal<T>::construct(pointer p, Args &&...args) const noexcept {
	static_assert(is_constructible_v<T, Args...>, "Invalid arguments for constructor");
	if constexpr (is_constructible_v<T, Args...>) {
		sprt::construct_at(p, sprt::forward<Args>(args)...);
	}
}

template <typename T>
constexpr inline void AllocatorLocal<T>::destroy(pointer p) const noexcept {
	if constexpr (!is_destructible_v<T> || is_trivially_destructible_v<T>) {
		// do nothing
	} else {
		sprt::destroy_at(p);
	}
}

template <typename T>
constexpr inline void AllocatorLocal<T>::destroy(pointer p, size_t size) const noexcept {
	if constexpr (!is_destructible_v<T> || is_trivially_destructible_v<T>) {
		// do nothing
	} else {
		for (size_t i = 0; i < size; ++i) { sprt::destroy_at(p + i); }
	}
}

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_LOCAL_H_
