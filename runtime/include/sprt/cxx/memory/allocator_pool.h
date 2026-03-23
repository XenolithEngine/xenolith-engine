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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALLOCATOR_POOL_H_
#define RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALLOCATOR_POOL_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/runtime/mem/context.h>
#include <sprt/c/__sprt_assert.h>

#include <sprt/cxx/new.h>

namespace sprt::memory {

// Root class for pool allocated objects
// Use with care
struct SPRT_GLOBAL AllocPool {
	static void *operator new(size_t size, const nothrow_t &tag) noexcept;
	static void *operator new(size_t size, align_val_t al, const nothrow_t &tag) noexcept;
	static void *operator new(size_t size, void *ptr) noexcept;
	static void *operator new(size_t size, void *ptr, const nothrow_t &tag) noexcept;
	static void *operator new(size_t size, memory::pool_t *ptr) noexcept;

	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, align_val_t al) noexcept;

	static pool_t *getCurrentPool();

	static bool isStapplerPool(pool_t *);

	template <typename T>
	static Status cleanupObjectFromPool(void *data);

	template <typename T>
	static void registerCleanupDestructor(T *obj, pool_t *pool);
};

template <typename Type>
struct Allocator_protect_construct {
	static constexpr bool value = false; // !std::is_scalar<Type>::value;
};

template <typename T>
class AllocatorPool {
public:
	using base_class = AllocPool;

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
		using other = AllocatorPool<U>;
	};

	// default alignment for pool_t is 8-bit, so, we can store up to 3 flags in pool pointer

public:
	// Default allocator uses pool from top of thread's AllocStack
	AllocatorPool() noexcept;
	AllocatorPool(pool_t *p) noexcept;

	template <typename B>
	AllocatorPool(const AllocatorPool<B> &a) noexcept;
	template <typename B>
	AllocatorPool(AllocatorPool<B> &&a) noexcept;

	template <typename B>
	AllocatorPool<T> &operator=(const AllocatorPool<B> &a) noexcept;
	template <typename B>
	AllocatorPool<T> &operator=(AllocatorPool<B> &&a) noexcept;

	T *allocate(size_t n) const noexcept;
	T *__allocate(size_t &n) const noexcept;
	T *__allocate(size_t n, size_t &bytes) const noexcept;
	void deallocate(T *t, size_t n) const noexcept;
	void __deallocate(T *t, size_t n, size_t bytes) const noexcept;

	template <typename B>
	inline bool operator==(const AllocatorPool<B> &p) const noexcept;
	template <typename B>
	inline bool operator!=(const AllocatorPool<B> &p) const noexcept;

	inline pointer address(reference r) const noexcept;
	inline const_pointer address(const_reference r) const noexcept;

	size_type max_size() const noexcept;

	template <typename... Args>
	void construct(pointer p, Args &&...args) const noexcept;

	void destroy(pointer p) const noexcept;

	void destroy(pointer p, size_t size) const noexcept;

	explicit operator bool() const noexcept;

	operator pool_t *() const noexcept;
	pool_t *getPool() const noexcept;

	void copy(T *dest, const T *source, size_t count) noexcept;

	void copy_rewrite(T *dest, size_t dcount, const T *source, size_t count) noexcept;

	void move(T *dest, T *source, size_t count) noexcept;
	void move_rewrite(T *dest, size_t dcount, T *source, size_t count) noexcept;

private:
	static pool_t *pool_ptr(pool_t *p) noexcept { return p; }

	pool_t *pool = nullptr;
};

template <typename Value>
struct Storage {
	struct Image {
		Value _value;
	};

	alignas(__alignof__(Image::_value)) uint8_t _storage[sizeof(Value)];

	Storage() noexcept { }
	Storage(nullptr_t) noexcept { }

	void *addr() noexcept { return static_cast<void *>(&_storage); }
	const void *addr() const noexcept { return static_cast<const void *>(&_storage); }

	Value *ptr() noexcept { return static_cast<Value *>(addr()); }
	const Value *ptr() const noexcept { return static_cast<const Value *>(addr()); }

	Value &ref() noexcept { return *ptr(); }
	const Value &ref() const noexcept { return *ptr(); }
};

//
// Implementation details
//

// Default allocator uses pool from top of thread's AllocStack
template <typename T>
inline AllocatorPool<T>::AllocatorPool() noexcept : pool(pool::acquire()) { }

template <typename T>
inline AllocatorPool<T>::AllocatorPool(pool_t *p) noexcept : pool(p) { }

template <typename T>
template <typename B>
inline AllocatorPool<T>::AllocatorPool(const AllocatorPool<B> &a) noexcept : pool(a.getPool()) { }

template <typename T>
template <typename B>
inline AllocatorPool<T>::AllocatorPool(AllocatorPool<B> &&a) noexcept : pool(a.getPool()) { }

template <typename T>
template <typename B>
inline auto AllocatorPool<T>::operator=(const AllocatorPool<B> &a) noexcept -> AllocatorPool<T> & {
	pool = pool_ptr(a.pool);
	return *this;
}

template <typename T>
template <typename B>
inline auto AllocatorPool<T>::operator=(AllocatorPool<B> &&a) noexcept -> AllocatorPool<T> & {
	pool = pool_ptr(a.pool);
	return *this;
}

template <typename T>
inline auto AllocatorPool<T>::allocate(size_t n) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	return ptr;
}

template <typename T>
inline auto AllocatorPool<T>::__allocate(size_t &n) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	n = size / sizeof(T);
	return ptr;
}

template <typename T>
inline auto AllocatorPool<T>::__allocate(size_t n, size_t &bytes) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	bytes = size;
	return ptr;
}

template <typename T>
inline void AllocatorPool<T>::deallocate(T *t, size_t n) const noexcept {
	pool::free(pool_ptr(pool), t, n * sizeof(T));
}

template <typename T>
inline void AllocatorPool<T>::__deallocate(T *t, size_t n, size_t bytes) const noexcept {
	pool::free(pool_ptr(pool), t, bytes);
}

template <typename T>
template <typename B>
inline bool AllocatorPool<T>::operator==(const AllocatorPool<B> &p) const noexcept {
	return pool_ptr(p.pool) == pool_ptr(pool);
}

template <typename T>
template <typename B>
inline bool AllocatorPool<T>::operator!=(const AllocatorPool<B> &p) const noexcept {
	return pool_ptr(p.pool) != pool_ptr(pool);
}

template <typename T>
inline auto AllocatorPool<T>::address(reference r) const noexcept -> pointer {
	return &r;
}

template <typename T>
inline auto AllocatorPool<T>::address(const_reference r) const noexcept -> const_pointer {
	return &r;
}

template <typename T>
inline auto AllocatorPool<T>::max_size() const noexcept -> size_type {
	return Max<size_type>;
}

template <typename T>
template <typename... Args>
inline void AllocatorPool<T>::construct(pointer p, Args &&...args) const noexcept {
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

		if constexpr (Allocator_protect_construct<T>::value) {
			perform_conditional([&] {
				new ((T *)p, sprt::nothrow) T(sprt::forward<Args>(args)...);
			}, pool_ptr(pool));
			return;
		}
		new ((T *)p, sprt::nothrow) T(sprt::forward<Args>(args)...);
	}
}

template <typename T>
inline void AllocatorPool<T>::destroy(pointer p) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		if constexpr (Allocator_protect_construct<T>::value) {
			perform_conditional([&] { p->~T(); }, pool_ptr(pool));
			return;
		}

		do { p->~T(); } while (0);
	}
}

template <typename T>
inline void AllocatorPool<T>::destroy(pointer p, size_t size) const noexcept {
	if constexpr (!is_destructible<T>::value || is_scalar<T>::value) {
		// do nothing
	} else {
		if constexpr (Allocator_protect_construct<T>::value) {
			perform_conditional([&] {
				for (size_t i = 0; i < size; ++i) { (p + i)->~T(); }
			}, pool_ptr(pool));
			return;
		}

		for (size_t i = 0; i < size; ++i) { (p + i)->~T(); }
	}
}

template <typename T>
inline AllocatorPool<T>::operator bool() const noexcept {
	return pool_ptr(pool) != nullptr;
}

template <typename T>
inline AllocatorPool<T>::operator pool_t *() const noexcept {
	return pool_ptr(pool);
}

template <typename T>
inline pool_t *AllocatorPool<T>::getPool() const noexcept {
	return pool_ptr(pool);
}

template <typename T>
inline void AllocatorPool<T>::copy(T *dest, const T *source, size_t count) noexcept {
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
inline void AllocatorPool<T>::copy_rewrite(T *dest, size_t dcount, const T *source,
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
inline void AllocatorPool<T>::move(T *dest, T *source, size_t count) noexcept {
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
inline void AllocatorPool<T>::move_rewrite(T *dest, size_t dcount, T *source,
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

inline void *AllocPool::operator new(size_t size, const nothrow_t &tag) noexcept {
	return pool::alloc(pool::acquire(), size);
}

inline void *AllocPool::operator new(size_t size, align_val_t al, const nothrow_t &tag) noexcept {
	return pool::alloc(pool::acquire(), size, static_cast<uint32_t>(toInt(al)));
}

inline void *AllocPool::operator new(size_t size, pool_t *pool) noexcept {
	return pool::alloc(pool, size);
}

inline void *AllocPool::operator new(size_t size, void *mem) noexcept { return mem; }

inline void *AllocPool::operator new(size_t size, void *mem, const nothrow_t &tag) noexcept {
	return mem;
}

inline void AllocPool::operator delete(void *) noexcept {
	// APR doesn't require to free object's memory
	// It can be optimized if we'd know true allocation size
}

inline void AllocPool::operator delete(void *, align_val_t al) noexcept {
	// APR doesn't require to free object's memory,
	// It can be optimized if we'd know true allocation size
}

inline pool_t *AllocPool::getCurrentPool() { return pool::acquire(); }

template <typename T>
inline Status AllocPool::cleanupObjectFromPool(void *data) {
	delete ((T *)data);
	return Status::Ok;
}

template <typename T>
inline void AllocPool::registerCleanupDestructor(T *obj, pool_t *pool) {
	pool::pre_cleanup_register(pool, (void *)obj, &(cleanupObjectFromPool<T>));
}

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALLOCATOR_POOL_H_
