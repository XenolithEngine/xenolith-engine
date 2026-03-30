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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALIGNED_STORAGE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALIGNED_STORAGE_H_

#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/detail/ctypes.h>

namespace sprt::memory {

template <typename Value>
struct aligned_storage {
	struct Image {
		Value _value;
	};

	alignas(__alignof__(Image::_value)) uint8_t _storage[sizeof(Value)];

	aligned_storage() noexcept { }
	aligned_storage(nullptr_t) noexcept { }

	void *addr() noexcept { return static_cast<void *>(&_storage); }
	const void *addr() const noexcept { return static_cast<const void *>(&_storage); }

	Value *ptr() noexcept { return static_cast<Value *>(addr()); }
	const Value *ptr() const noexcept { return static_cast<const Value *>(addr()); }

	Value &ref() noexcept { return *ptr(); }
	const Value &ref() const noexcept { return *ptr(); }

	template <typename Allocator, typename... Args>
	Value *construct(const Allocator &alloc, Args &&...args) {
		auto pointer = ptr();
		alloc.construct(pointer, sprt::forward<Args>(args)...);
		return pointer;
	}

	template <typename Allocator>
	void destroy(const Allocator &alloc) {
		alloc.destroy(ptr());
	}
};

template <typename Key, typename Value>
struct aligned_storage_kv_traits;

template <typename Key>
struct aligned_storage_kv_traits<Key, Key> {
	using value_type = Key;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k; }

	static inline const Key &extract_key(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline const Key &extract_value(const value_type &k) noexcept { return k; }

	static inline const Key &extract_value(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Key &extract_value(value_type &k) noexcept { return k; }

	static inline Key &extract_value(storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, const Key &key,
			Args &&...args) noexcept {
		storage.construct(alloc, key);
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, Key &&key,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::move_unsafe(key));
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

template <typename Key, typename Value>
struct aligned_storage_kv_traits<Key, pair<Key, Value>> {
	using value_type = pair<Key, Value>;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k.first; }

	static inline const Key &extract_key(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Value &extract_value(const value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Value &extract_value(value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, const Key &k,
			Args &&...args) noexcept {
		storage.construct(alloc, pair_emplace_construct_t(), sprt::forward<Key>(k),
				sprt::forward<Args>(args)...);
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, Key &&k,
			Args &&...args) noexcept {
		storage.construct(alloc, pair_emplace_construct_t(), sprt::move_unsafe(k),
				sprt::forward<Args>(args)...);
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

template <typename Key, typename Value>
struct aligned_storage_kv_traits<Key, pair<const Key, Value>> {
	using value_type = pair<const Key, Value>;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k.first; }

	static inline const Key &extract_key(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline const Value &extract_value(const value_type &k) noexcept { return k.second; }

	static inline const Value &extract_value(const storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Value &extract_value(value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(storage_type &storage) noexcept {
		return extract_key(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, const Key &k,
			Args &&...args) noexcept {
		storage.construct(alloc, pair_emplace_construct_t(), k, sprt::forward<Args>(args)...);
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, storage_type &storage, Key &&k,
			Args &&...args) noexcept {
		storage.construct(alloc, pair_emplace_construct_t(), sprt::move_unsafe(k),
				sprt::forward<Args>(args)...);
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, storage_type &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_ALIGNED_STORAGE_H_
