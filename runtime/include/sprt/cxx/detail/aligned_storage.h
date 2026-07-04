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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALIGNED_STORAGE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALIGNED_STORAGE_H_

#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/tuple> // pair's piecewise_construct ctor + forward_as_tuple

namespace sprt::detail {

template <typename Value>
struct aligned_storage {
	struct Image {
		Value _value;
	};

	struct Empty { };

	union Storage {
		Value value;
		Empty empty;

		constexpr Storage() : empty() { }
		constexpr ~Storage() { }
	};

	//alignas(__alignof__(Image::_value)) uint8_t _storage[sizeof(Value)];
	Storage _storage;

	constexpr aligned_storage() noexcept { }
	constexpr aligned_storage(nullptr_t) noexcept { }

	constexpr void *addr() noexcept { return static_cast<void *>(&_storage.value); }
	constexpr const void *addr() const noexcept {
		return static_cast<const void *>(&_storage.value);
	}

	constexpr Value *ptr() noexcept { return static_cast<Value *>(&_storage.value); }
	constexpr const Value *ptr() const noexcept {
		return static_cast<const Value *>(&_storage.value);
	}

	constexpr Value &ref() noexcept { return *ptr(); }
	constexpr const Value &ref() const noexcept { return *ptr(); }

	template <typename Allocator, typename... Args>
	constexpr Value *construct(const Allocator &alloc, Args &&...args) noexcept {
		auto pointer = ptr();
		alloc.construct(pointer, sprt::forward<Args>(args)...);
		return pointer;
	}

	template <typename Allocator>
	constexpr void destroy(const Allocator &alloc) noexcept {
		alloc.destroy(ptr());
	}
};

// Node "box" storage: keeps the Value in an individually heap-allocated node so that the
// element object has a STABLE address. hash_memory relocates nodes during rehash and erase
// compaction by stealing the pointer (adopt()) instead of moving the element; that pointer
// steal is what gives std::unordered_* the reference/pointer stability the standard requires
// (references and pointers to elements survive rehash; only iterators are invalidated). This
// mirrors aligned_storage's public surface (addr/ptr/ref/construct/destroy) so the hash_memory
// node and iterator machinery stays storage-agnostic — ref()/ptr() transparently dereference
// the heap node, so iterators still yield value_type& / value_type*.
template <typename Value>
struct indirect_storage {
	Value *_ptr = nullptr;

	constexpr indirect_storage() noexcept { }
	constexpr indirect_storage(nullptr_t) noexcept { }

	constexpr void *addr() noexcept { return static_cast<void *>(_ptr); }
	constexpr const void *addr() const noexcept { return static_cast<const void *>(_ptr); }

	constexpr Value *ptr() noexcept { return _ptr; }
	constexpr const Value *ptr() const noexcept { return _ptr; }

	constexpr Value &ref() noexcept { return *_ptr; }
	constexpr const Value &ref() const noexcept { return *_ptr; }

	// Allocate a single-element node via the (const) extended allocator surface and construct
	// the value in place. The passed allocator's value_type is Value, so no rebind is needed.
	template <typename Allocator, typename... Args>
	Value *construct(const Allocator &alloc, Args &&...args) noexcept {
		size_t __n = 1;
		_ptr = alloc.__allocate(__n);
		alloc.construct(_ptr, sprt::forward<Args>(args)...);
		return _ptr;
	}

	template <typename Allocator>
	void destroy(const Allocator &alloc) noexcept {
		if (_ptr) {
			alloc.destroy(_ptr);
			alloc.__deallocate(_ptr, 1, sizeof(Value));
			_ptr = nullptr;
		}
	}

	// Steal the heap node from `other` (pointer move). Used by hash_memory to relocate a node
	// during rehash / erase compaction: the element keeps its address, only the owning slot moves.
	constexpr void adopt(indirect_storage &other) noexcept {
		_ptr = other._ptr;
		other._ptr = nullptr;
	}
};

// The storage-object overloads of extract_key / extract_value / construct below use a constrained
// `auto` placeholder so a single set of overloads works for BOTH aligned_storage<Value> and
// indirect_storage<Value>. The constraint is essential: it stops the storage overload of
// extract_key from also matching a raw value_type argument (which a heterogeneous insert(P&&)
// passes), which would otherwise out-rank the by-conversion `const value_type&` overload.
template <typename S>
concept __kv_storage = requires(const S &s) { s.ref(); };

template <typename Key, typename Value>
struct aligned_storage_kv_traits;

template <typename Key>
struct aligned_storage_kv_traits<Key, Key> {
	using value_type = Key;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k; }

	static inline const Key &extract_key(const __kv_storage auto &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline const Key &extract_value(const value_type &k) noexcept { return k; }

	static inline const Key &extract_value(const __kv_storage auto &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Key &extract_value(value_type &k) noexcept { return k; }

	static inline Key &extract_value(__kv_storage auto &storage) noexcept {
		return extract_key(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, const Key &key,
			Args &&...args) noexcept {
		storage.construct(alloc, key);
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, Key &&key,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::move_unsafe(key));
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

template <typename Key, typename Value>
struct aligned_storage_kv_traits<Key, pair<Key, Value>> {
	using value_type = pair<Key, Value>;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k.first; }

	static inline const Key &extract_key(const __kv_storage auto &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline Value &extract_value(const value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(const __kv_storage auto &storage) noexcept {
		return extract_value(storage.ref());
	}

	static inline Value &extract_value(value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(__kv_storage auto &storage) noexcept {
		return extract_value(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, const Key &k,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::piecewise_construct, sprt::forward_as_tuple(k),
				sprt::forward_as_tuple(sprt::forward<Args>(args)...));
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, Key &&k,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::piecewise_construct,
				sprt::forward_as_tuple(sprt::move_unsafe(k)),
				sprt::forward_as_tuple(sprt::forward<Args>(args)...));
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

template <typename Key, typename Value>
struct aligned_storage_kv_traits<Key, pair<const Key, Value>> {
	using value_type = pair<const Key, Value>;
	using storage_type = aligned_storage<value_type>;

	static inline const Key &extract_key(const value_type &k) noexcept { return k.first; }

	static inline const Key &extract_key(const __kv_storage auto &storage) noexcept {
		return extract_key(storage.ref());
	}

	static inline const Value &extract_value(const value_type &k) noexcept { return k.second; }

	static inline const Value &extract_value(const __kv_storage auto &storage) noexcept {
		return extract_value(storage.ref());
	}

	static inline Value &extract_value(value_type &k) noexcept { return k.second; }

	static inline Value &extract_value(__kv_storage auto &storage) noexcept {
		return extract_value(storage.ref());
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, const Key &k,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::piecewise_construct, sprt::forward_as_tuple(k),
				sprt::forward_as_tuple(sprt::forward<Args>(args)...));
	}

	template <typename A, typename... Args>
	static inline void construct(const A &alloc, __kv_storage auto &storage, Key &&k,
			Args &&...args) noexcept {
		storage.construct(alloc, sprt::piecewise_construct,
				sprt::forward_as_tuple(sprt::move_unsafe(k)),
				sprt::forward_as_tuple(sprt::forward<Args>(args)...));
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			const value_type &value) noexcept {
		storage.construct(alloc, value);
	}

	template <typename A>
	static inline void construct(const A &alloc, __kv_storage auto &storage,
			value_type &&value) noexcept {
		storage.construct(alloc, sprt::move_unsafe(value));
	}
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_ALIGNED_STORAGE_H_
