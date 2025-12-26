/**
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMALLOC_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMALLOC_H_

#include "SPMemPoolInterface.h"
#include "SPStatus.h"

namespace STAPPLER_VERSIONIZED stappler::memory {

// Root class for pool allocated objects
// Use with care
struct SP_PUBLIC AllocPool {
	static void *operator new(size_t size, const std::nothrow_t &tag) noexcept;
	static void *operator new(size_t size, std::align_val_t al, const std::nothrow_t &tag) noexcept;
	static void *operator new(size_t size, void *ptr) noexcept;
	static void *operator new(size_t size, memory::pool_t *ptr) noexcept;

	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, std::align_val_t al) noexcept;

	static pool_t *getCurrentPool();

	static bool isStapplerPool(pool_t *);

	template <typename T>
	static Status cleanupObjectFromPool(void *data);

	template <typename T>
	static void registerCleanupDestructor(T *obj, pool_t *pool);
};

} // namespace stappler::memory

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

namespace {
template < class... Args>
struct Allocator_SelectFirst;
template < class A, class... Args>
struct Allocator_SelectFirst<A, Args...> {
	using type = A;
};
} // namespace

template <typename Type>
struct Allocator_protect_construct {
	static constexpr bool value = false; // !std::is_scalar<Type>::value;
};

template <typename T>
class Allocator {
public:
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
		using other = Allocator<U>;
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
	Allocator() noexcept;
	Allocator(pool_t *p) noexcept;

	template <typename B>
	Allocator(const Allocator<B> &a) noexcept;
	template <typename B>
	Allocator(Allocator<B> &&a) noexcept;

	template <typename B>
	Allocator<T> &operator=(const Allocator<B> &a) noexcept;
	template <typename B>
	Allocator<T> &operator=(Allocator<B> &&a) noexcept;

	T *allocate(size_t n) const noexcept;
	T *__allocate(size_t &n) const noexcept;
	T *__allocate(size_t n, size_t &bytes) const noexcept;
	void deallocate(T *t, size_t n) const noexcept;
	void __deallocate(T *t, size_t n, size_t bytes) const noexcept;

	template <typename B>
	inline bool operator==(const Allocator<B> &p) const noexcept;
	template <typename B>
	inline bool operator!=(const Allocator<B> &p) const noexcept;

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

	bool test(AllocFlag f) const noexcept;
	void set(AllocFlag f) noexcept;
	void reset(AllocFlag f) noexcept;
	void flip(AllocFlag f) noexcept;

private:
	static pool_t *pool_ptr(pool_t *p) noexcept {
		return (pool_t *)(uintptr_t(p) & ~toInt(BitMask));
	}

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
inline Allocator<T>::Allocator() noexcept : pool(pool::acquire()) { }

template <typename T>
inline Allocator<T>::Allocator(pool_t *p) noexcept : pool(p) { }

template <typename T>
template <typename B>
inline Allocator<T>::Allocator(const Allocator<B> &a) noexcept : pool(a.getPool()) { }

template <typename T>
template <typename B>
inline Allocator<T>::Allocator(Allocator<B> &&a) noexcept : pool(a.getPool()) { }

template <typename T>
template <typename B>
inline auto Allocator<T>::operator=(const Allocator<B> &a) noexcept -> Allocator<T> & {
	pool = pool_ptr(a.pool);
	return *this;
}

template <typename T>
template <typename B>
inline auto Allocator<T>::operator=(Allocator<B> &&a) noexcept -> Allocator<T> & {
	pool = pool_ptr(a.pool);
	return *this;
}

template <typename T>
inline auto Allocator<T>::allocate(size_t n) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	return ptr;
}

template <typename T>
inline auto Allocator<T>::__allocate(size_t &n) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	n = size / sizeof(T);
	return ptr;
}

template <typename T>
inline auto Allocator<T>::__allocate(size_t n, size_t &bytes) const noexcept -> T * {
	size_t size = sizeof(T) * n;
	auto ptr = static_cast<T *>(pool::alloc(pool_ptr(pool), size, alignof(T)));

	sprt_passert(ptr, "allocation should always be successful");

	bytes = size;
	return ptr;
}

template <typename T>
inline void Allocator<T>::deallocate(T *t, size_t n) const noexcept {
	pool::free(pool_ptr(pool), t, n * sizeof(T));
}

template <typename T>
inline void Allocator<T>::__deallocate(T *t, size_t n, size_t bytes) const noexcept {
	pool::free(pool_ptr(pool), t, bytes);
}

template <typename T>
template <typename B>
inline bool Allocator<T>::operator==(const Allocator<B> &p) const noexcept {
	return pool_ptr(p.pool) == pool_ptr(pool);
}

template <typename T>
template <typename B>
inline bool Allocator<T>::operator!=(const Allocator<B> &p) const noexcept {
	return pool_ptr(p.pool) != pool_ptr(pool);
}

template <typename T>
inline auto Allocator<T>::address(reference r) const noexcept -> pointer {
	return &r;
}

template <typename T>
inline auto Allocator<T>::address(const_reference r) const noexcept -> const_pointer {
	return &r;
}

template <typename T>
inline auto Allocator<T>::max_size() const noexcept -> size_type {
	return maxOf<size_type>();
}

template <typename T>
template <typename... Args>
inline void Allocator<T>::construct(pointer p, Args &&...args) const noexcept {
	static_assert(std::is_constructible<T, Args...>::value, "Invalid arguments for constructor");
	if constexpr (std::is_constructible<T, Args...>::value) {
		if constexpr (sizeof...(Args) == 1) {
			if constexpr (std::is_trivially_copyable<T>::value
					&& std::is_convertible_v<typename Allocator_SelectFirst<Args...>::type,
							const T &>) {
				auto construct_memcpy = [](pointer p, const T &source) {
					memcpy(p, &source, sizeof(T));
				};

				construct_memcpy(p, std::forward<Args>(args)...);
				return;
			}
		}

		if constexpr (Allocator_protect_construct<T>::value) {
			perform_conditional([&] { new ((T *)p) T(std::forward<Args>(args)...); },
					pool_ptr(pool));
			return;
		}
		new ((T *)p) T(std::forward<Args>(args)...);
	}
}

template <typename T>
inline void Allocator<T>::destroy(pointer p) const noexcept {
	if constexpr (!std::is_destructible<T>::value || std::is_scalar<T>::value) {
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
inline void Allocator<T>::destroy(pointer p, size_t size) const noexcept {
	if constexpr (!std::is_destructible<T>::value || std::is_scalar<T>::value) {
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
inline Allocator<T>::operator bool() const noexcept {
	return pool_ptr(pool) != nullptr;
}

template <typename T>
inline Allocator<T>::operator pool_t *() const noexcept {
	return pool_ptr(pool);
}

template <typename T>
inline pool_t *Allocator<T>::getPool() const noexcept {
	return pool_ptr(pool);
}

template <typename T>
inline void Allocator<T>::copy(T *dest, const T *source, size_t count) noexcept {
	if constexpr (std::is_trivially_copyable<T>::value) {
		memmove(dest, source, count * sizeof(T));
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
inline void Allocator<T>::copy_rewrite(T *dest, size_t dcount, const T *source,
		size_t count) noexcept {
	if constexpr (std::is_trivially_copyable<T>::value) {
		memmove(dest, source, count * sizeof(T));
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = std::min(count, dcount);
			for (; i > m; i--) { construct(dest + i - 1, *(source + i - 1)); }
			for (; i > 0; i--) {
				destroy(dest + i - 1);
				construct(dest + i - 1, *(source + i - 1));
			}
		} else {
			size_t i = 0;
			size_t m = std::min(count, dcount);
			for (; i < m; ++i) {
				destroy(dest + i);
				construct(dest + i, *(source + i));
			}
			for (; i < count; ++i) { construct(dest + i, *(source + i)); }
		}
	}
}

template <typename T>
inline void Allocator<T>::move(T *dest, T *source, size_t count) noexcept {
	if constexpr (std::is_trivially_copyable<T>::value) {
		memmove(dest, source, count * sizeof(T));
	} else if constexpr (std::is_trivially_move_constructible<T>::value) {
		memmove((void *)dest, source, count * sizeof(T));
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			for (size_t i = count; i > 0; i--) {
				construct(dest + i - 1, sp::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
		} else {
			for (size_t i = 0; i < count; i++) {
				construct(dest + i, sp::move_unsafe(*(source + i)));
				destroy(source + i);
			}
		}
	}
}

template <typename T>
inline void Allocator<T>::move_rewrite(T *dest, size_t dcount, T *source, size_t count) noexcept {
	if constexpr (std::is_trivially_copyable<T>::value) {
		memmove(dest, source, count * sizeof(T));
	} else if constexpr (std::is_trivially_move_constructible<T>::value) {
		memmove(dest, source, count * sizeof(T));
	} else {
		if (dest == source) {
			return;
		} else if (uintptr_t(dest) > uintptr_t(source)) {
			size_t i = count;
			size_t m = std::min(count, dcount);
			for (; i > m; i--) {
				construct(dest + i - 1, sp::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
			for (; i > 0; i--) {
				destroy(dest + i - 1);
				construct(dest + i - 1, sp::move_unsafe(*(source + i - 1)));
				destroy(source + i - 1);
			}
		} else {
			size_t i = 0;
			size_t m = std::min(count, dcount);
			for (; i < m; ++i) {
				destroy(dest + i);
				construct(dest + i, sp::move_unsafe(*(source + i)));
				destroy(source + i);
			}
			for (; i < count; ++i) {
				construct(dest + i, sp::move_unsafe(*(source + i)));
				destroy(source + i);
			}
		}
	}
}

template <typename T>
inline bool Allocator<T>::test(AllocFlag f) const noexcept {
	return (reinterpret_cast<uintptr_t>(pool) & toInt(f)) != uintptr_t(0);
}

template <typename T>
inline void Allocator<T>::set(AllocFlag f) noexcept {
	pool = reinterpret_cast<pool_t *>(reinterpret_cast<uintptr_t>(pool) | toInt(f));
}

template <typename T>
inline void Allocator<T>::reset(AllocFlag f) noexcept {
	pool = reinterpret_cast<pool_t *>(reinterpret_cast<uintptr_t>(pool) & ~toInt(f));
}

template <typename T>
inline void Allocator<T>::flip(AllocFlag f) noexcept {
	pool = reinterpret_cast<pool_t *>(reinterpret_cast<uintptr_t>(pool) ^ toInt(f));
}


} // namespace stappler::memory::detail


namespace STAPPLER_VERSIONIZED stappler::memory {

inline void *AllocPool::operator new(size_t size, const std::nothrow_t &tag) noexcept {
	return pool::alloc(pool::acquire(), size);
}

inline void *AllocPool::operator new(size_t size, std::align_val_t al,
		const std::nothrow_t &tag) noexcept {
	return pool::alloc(pool::acquire(), size, static_cast<uint32_t>(toInt(al)));
}

inline void *AllocPool::operator new(size_t size, pool_t *pool) noexcept {
	return pool::alloc(pool, size);
}

inline void *AllocPool::operator new(size_t size, void *mem) noexcept { return mem; }

inline void AllocPool::operator delete(void *) noexcept {
	// APR doesn't require to free object's memory
	// It can be optimized if we'd know true allocation size
}

inline void AllocPool::operator delete(void *, std::align_val_t al) noexcept {
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

} // namespace stappler::memory

#endif /* STAPPLER_CORE_MEMORY_SPMEMALLOC_H_ */
