/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_MALLOC_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_MALLOC_H_

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_assert.h>

#include <sprt/cxx/new>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/cxx/__memory/common.h>
#include <sprt/cxx/detail/constexpr.h>

namespace sprt::detail {

template <typename T>
class AllocatorMalloc {
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
		using other = AllocatorMalloc<U>;
	};

public:
	// Default allocator uses pool from top of thread's AllocStack
	constexpr AllocatorMalloc() noexcept;

	template <typename B>
	constexpr AllocatorMalloc(const AllocatorMalloc<B> &a) noexcept;
	template <typename B>
	constexpr AllocatorMalloc(AllocatorMalloc<B> &&a) noexcept;

	template <typename B>
	constexpr AllocatorMalloc<T> &operator=(const AllocatorMalloc<B> &a) noexcept;
	template <typename B>
	constexpr AllocatorMalloc<T> &operator=(AllocatorMalloc<B> &&a) noexcept;

	constexpr T *allocate(size_t n) const noexcept;
	constexpr T *__allocate(size_t &n) const noexcept;
	constexpr T *__allocate(size_t n, size_t &bytes) const noexcept;
	constexpr void deallocate(T *t, size_t n) const noexcept;
	constexpr void __deallocate(T *t, size_t n, size_t bytes) const noexcept;

	template <typename B>
	constexpr inline bool operator==(const AllocatorMalloc<B> &p) const noexcept;
	template <typename B>
	constexpr inline bool operator!=(const AllocatorMalloc<B> &p) const noexcept;

	constexpr inline pointer address(reference r) const noexcept;
	constexpr inline const_pointer address(const_reference r) const noexcept;

	constexpr size_type max_size() const noexcept;

	template <typename... Args>
	constexpr void construct(pointer p, Args &&...args) const noexcept;

	constexpr void destroy(pointer p) const noexcept;

	constexpr void destroy(pointer p, size_t size) const noexcept;

	constexpr explicit operator bool() const noexcept;

	constexpr void copy(T *dest, const T *source, size_t count) noexcept;

	constexpr void copy_rewrite(T *dest, size_t dcount, const T *source, size_t count) noexcept;

	constexpr void move(T *dest, T *source, size_t count) noexcept;
	constexpr void move_rewrite(T *dest, size_t dcount, T *source, size_t count) noexcept;
};

//
// Implementation details
//

// Default allocator uses pool from top of thread's AllocStack
template <typename T>
constexpr inline AllocatorMalloc<T>::AllocatorMalloc() noexcept { }

template <typename T>
template <typename B>
constexpr inline AllocatorMalloc<T>::AllocatorMalloc(const AllocatorMalloc<B> &a) noexcept { }

template <typename T>
template <typename B>
constexpr inline AllocatorMalloc<T>::AllocatorMalloc(AllocatorMalloc<B> &&a) noexcept { }

template <typename T>
template <typename B>
constexpr inline auto AllocatorMalloc<T>::operator=(const AllocatorMalloc<B> &a) noexcept
		-> AllocatorMalloc<T> & {
	return *this;
}

template <typename T>
template <typename B>
constexpr inline auto AllocatorMalloc<T>::operator=(AllocatorMalloc<B> &&a) noexcept
		-> AllocatorMalloc<T> & {
	return *this;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::allocate(size_t n) const noexcept -> T * {
	T *ptr = n == 1 ? sprt::memory::allocate<T>() : sprt::memory::allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::__allocate(size_t &n) const noexcept -> T * {
	T *ptr = sprt::memory::allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::__allocate(size_t n, size_t &bytes) const noexcept
		-> T * {
	T *ptr = sprt::memory::allocate<T>(n);
	if (!ptr) {
		__sprt_perror("allocation error");
	}
	sprt_passert(ptr, "allocation should always be successful");
	bytes = n * sizeof(T);
	return ptr;
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::deallocate(T *t, size_t n) const noexcept {
	if (n == 1) {
		sprt::memory::deallocate<T>(t);
	} else {
		sprt::memory::deallocate<T>(t, n);
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::__deallocate(T *t, size_t n,
		size_t bytes) const noexcept {
	sprt::memory::deallocate<T>(t, n);
}

template <typename T>
template <typename B>
constexpr inline bool AllocatorMalloc<T>::operator==(const AllocatorMalloc<B> &p) const noexcept {
	return true;
}

template <typename T>
template <typename B>
constexpr inline bool AllocatorMalloc<T>::operator!=(const AllocatorMalloc<B> &p) const noexcept {
	return false;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::address(reference r) const noexcept -> pointer {
	return &r;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::address(const_reference r) const noexcept
		-> const_pointer {
	return &r;
}

template <typename T>
constexpr inline auto AllocatorMalloc<T>::max_size() const noexcept -> size_type {
	return Max<size_type>;
}

template <typename T>
template <typename... Args>
constexpr inline void AllocatorMalloc<T>::construct(pointer p, Args &&...args) const noexcept {
	static_assert(is_constructible<T, Args...>::value, "Invalid arguments for constructor");
	if constexpr (is_constructible<T, Args...>::value) {
		/*if constexpr (sizeof...(Args) == 1) {
			if constexpr (is_trivially_copyable<T>::value && is_copy_assignable_v<T>
					&& is_convertible_v<typename Allocator_SelectFirst<Args...>::type, const T &>) {
				auto construct_memcpy = [](pointer p, const T &source) {
					__constexpr_memcpy(p, &source, 1);
				};

				construct_memcpy(p, sprt::forward<Args>(args)...);
				return;
			}
		}*/

		sprt::construct_at(p, sprt::forward<Args>(args)...);
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::destroy(pointer p) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		sprt::destroy_at(p);
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::destroy(pointer p, size_t size) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		for (size_t i = 0; i < size; ++i) { sprt::destroy_at(p + 1); }
	}
}

template <typename T>
constexpr inline AllocatorMalloc<T>::operator bool() const noexcept {
	return true;
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::copy(T *dest, const T *source, size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			for (size_t i = count; i > 0; i--) { construct(dest + i - 1, *(source + i - 1)); }
		} else {
			for (size_t i = 0; i < count; i++) { construct(dest + i, *(source + i)); }
		}
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::copy_rewrite(T *dest, size_t dcount, const T *source,
		size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = min(count, dcount);
			for (; i > m; i--) { construct(dest + i - 1, *(source + i - 1)); }
			for (; i > 0; i--) {
				destroy(dest + i - 1);
				construct(dest + i - 1, *(source + i - 1));
			}
		} else {
			size_t i = 0;
			size_t m = min(count, dcount);
			for (; i < m; ++i) {
				destroy(dest + i);
				construct(dest + i, *(source + i));
			}
			for (; i < count; ++i) { construct(dest + i, *(source + i)); }
		}
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::move(T *dest, T *source, size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else if constexpr (is_trivially_move_constructible<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			for (size_t i = count; i > 0; i--) {
				construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
		} else {
			for (size_t i = 0; i < count; i++) {
				construct(dest + i, sprt::move_unsafe(*(source + i)));
				destroy(source + i);
			}
		}
	}
}

template <typename T>
constexpr inline void AllocatorMalloc<T>::move_rewrite(T *dest, size_t dcount, T *source,
		size_t count) noexcept {
	if constexpr (is_trivially_copyable<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else if constexpr (is_trivially_move_constructible<T>::value) {
		__constexpr_memmove(dest, source, count);
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = min(count, dcount);
			for (; i > m; i--) {
				construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
			for (; i > 0; i--) {
				destroy(dest + i - 1);
				construct(dest + i - 1, sprt::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
		} else {
			size_t i = 0;
			size_t m = min(count, dcount);
			for (; i < m; ++i) {
				destroy(dest + i);
				construct(dest + i, sprt::move_unsafe(*(source + i)));
				destroy(source + i);
			}
			for (; i < count; ++i) {
				construct(dest + i, sprt::move_unsafe(*(source + i)));
				destroy(source + i);
			}
		}
	}
}

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALLOCATOR_MALLOC_H_
