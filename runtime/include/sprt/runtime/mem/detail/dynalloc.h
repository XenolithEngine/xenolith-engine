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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_DYNALLOC_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_DYNALLOC_H_

#include <sprt/runtime/mem/detail/alloc.h>

namespace sprt::memory::detail {

template <typename T>
class DynamicAllocator {
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

	template <class U>
	struct rebind {
		using other = DynamicAllocator<U>;
	};

	// default alignment for pool_t is 8-bit, so, we can store up to 3 flags in pool pointer

	enum AllocFlag : uintptr_t {
		FirstFlag = 1,
		SecondFlag = 2,
		ThirdFlag = 4,
		BitMask = 7,
	};

public:
	// Default allocator uses pool from top of thread's AllocStack
	DynamicAllocator() noexcept;

	template <typename B>
	DynamicAllocator(const DynamicAllocator<B> &a) noexcept;
	template <typename B>
	DynamicAllocator(DynamicAllocator<B> &&a) noexcept;

	template <typename B>
	DynamicAllocator<T> &operator=(const DynamicAllocator<B> &a) noexcept;
	template <typename B>
	DynamicAllocator<T> &operator=(DynamicAllocator<B> &&a) noexcept;

	T *allocate(size_t n) const noexcept;
	T *__allocate(size_t &n) const noexcept;
	T *__allocate(size_t n, size_t &bytes) const noexcept;
	void deallocate(T *t, size_t n) const noexcept;
	void __deallocate(T *t, size_t n, size_t bytes) const noexcept;

	template <typename B>
	inline bool operator==(const DynamicAllocator<B> &p) const noexcept;
	template <typename B>
	inline bool operator!=(const DynamicAllocator<B> &p) const noexcept;

	inline pointer address(reference r) const noexcept;
	inline const_pointer address(const_reference r) const noexcept;

	size_type max_size() const noexcept;

	template <typename... Args>
	void construct(pointer p, Args &&...args) const noexcept;

	void destroy(pointer p) const noexcept;

	void destroy(pointer p, size_t size) const noexcept;

	explicit operator bool() const noexcept;

	void copy(T *dest, const T *source, size_t count) noexcept;

	void copy_rewrite(T *dest, size_t dcount, const T *source, size_t count) noexcept;

	void move(T *dest, T *source, size_t count) noexcept;
	void move_rewrite(T *dest, size_t dcount, T *source, size_t count) noexcept;

	bool test(AllocFlag f) const noexcept;
	void set(AllocFlag f) noexcept;
	void reset(AllocFlag f) noexcept;
	void flip(AllocFlag f) noexcept;

private:
	uint8_t _flags = 0;
};

//
// Implementation details
//

// Default allocator uses pool from top of thread's AllocStack
template <typename T>
inline DynamicAllocator<T>::DynamicAllocator() noexcept { }

template <typename T>
template <typename B>
inline DynamicAllocator<T>::DynamicAllocator(const DynamicAllocator<B> &a) noexcept { }

template <typename T>
template <typename B>
inline DynamicAllocator<T>::DynamicAllocator(DynamicAllocator<B> &&a) noexcept { }

template <typename T>
template <typename B>
inline auto DynamicAllocator<T>::operator=(const DynamicAllocator<B> &a) noexcept
		-> DynamicAllocator<T> & {
	return *this;
}

template <typename T>
template <typename B>
inline auto DynamicAllocator<T>::operator=(DynamicAllocator<B> &&a) noexcept
		-> DynamicAllocator<T> & {
	return *this;
}

template <typename T>
inline auto DynamicAllocator<T>::allocate(size_t n) const noexcept -> T * {
	T *ptr = nullptr;
	__sprt_posix_memalign((void **)&ptr, alignof(T), sizeof(T) * n);
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
inline auto DynamicAllocator<T>::__allocate(size_t &n) const noexcept -> T * {
	T *ptr = nullptr;
	__sprt_posix_memalign((void **)&ptr, alignof(T), sizeof(T) * n);
	sprt_passert(ptr, "allocation should always be successful");
	return ptr;
}

template <typename T>
inline auto DynamicAllocator<T>::__allocate(size_t n, size_t &bytes) const noexcept -> T * {
	T *ptr = nullptr;
	__sprt_posix_memalign((void **)&ptr, alignof(T), sizeof(T) * n);
	sprt_passert(ptr, "allocation should always be successful");
	bytes = n * sizeof(T);
	return ptr;
}

template <typename T>
inline void DynamicAllocator<T>::deallocate(T *t, size_t n) const noexcept {
	__sprt_free(t);
}

template <typename T>
inline void DynamicAllocator<T>::__deallocate(T *t, size_t n, size_t bytes) const noexcept {
	__sprt_free(t);
}

template <typename T>
template <typename B>
inline bool DynamicAllocator<T>::operator==(const DynamicAllocator<B> &p) const noexcept {
	return true;
}

template <typename T>
template <typename B>
inline bool DynamicAllocator<T>::operator!=(const DynamicAllocator<B> &p) const noexcept {
	return false;
}

template <typename T>
inline auto DynamicAllocator<T>::address(reference r) const noexcept -> pointer {
	return &r;
}

template <typename T>
inline auto DynamicAllocator<T>::address(const_reference r) const noexcept -> const_pointer {
	return &r;
}

template <typename T>
inline auto DynamicAllocator<T>::max_size() const noexcept -> size_type {
	return Max<size_type>;
}

template <typename T>
template <typename... Args>
inline void DynamicAllocator<T>::construct(pointer p, Args &&...args) const noexcept {
	static_assert(is_constructible<T, Args...>::value, "Invalid arguments for constructor");
	if constexpr (is_constructible<T, Args...>::value) {
		if constexpr (sizeof...(Args) == 1) {
			if constexpr (is_trivially_copyable<T>::value
					&& is_convertible_v<typename Allocator_SelectFirst<Args...>::type, const T &>) {
				auto construct_memcpy = [](pointer p, const T &source) {
					__builtin_memcpy(p, &source, sizeof(T));
				};

				construct_memcpy(p, sprt::forward<Args>(args)...);
				return;
			}
		}

		new ((T *)p, sprt::nothrow) T(sprt::forward<Args>(args)...);
	}
}

template <typename T>
inline void DynamicAllocator<T>::destroy(pointer p) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		do { p->~T(); } while (0);
	}
}

template <typename T>
inline void DynamicAllocator<T>::destroy(pointer p, size_t size) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		for (size_t i = 0; i < size; ++i) { (p + i)->~T(); }
	}
}

template <typename T>
inline DynamicAllocator<T>::operator bool() const noexcept {
	return true;
}

template <typename T>
inline void DynamicAllocator<T>::copy(T *dest, const T *source, size_t count) noexcept {
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
inline void DynamicAllocator<T>::copy_rewrite(T *dest, size_t dcount, const T *source,
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
inline void DynamicAllocator<T>::move(T *dest, T *source, size_t count) noexcept {
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
inline void DynamicAllocator<T>::move_rewrite(T *dest, size_t dcount, T *source,
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

template <typename T>
inline bool DynamicAllocator<T>::test(AllocFlag f) const noexcept {
	return (_flags & uint8_t(toInt(f))) != 0;
}

template <typename T>
inline void DynamicAllocator<T>::set(AllocFlag f) noexcept {
	_flags |= uint8_t(toInt(f));
}

template <typename T>
inline void DynamicAllocator<T>::reset(AllocFlag f) noexcept {
	_flags &= ~uint8_t(toInt(f));
}

template <typename T>
inline void DynamicAllocator<T>::flip(AllocFlag f) noexcept {
	_flags ^= ~uint8_t(toInt(f));
}

} // namespace sprt::memory::detail

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MEM_DETAIL_DYNALLOC_H_
