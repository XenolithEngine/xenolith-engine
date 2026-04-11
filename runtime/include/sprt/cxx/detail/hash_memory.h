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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_HASH_MEMORY_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_HASH_MEMORY_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/detail/aligned_storage.h>
#include <sprt/cxx/detail/hash.h>
#include <sprt/c/__sprt_assert.h>

namespace sprt::detail {

template <typename NodeType, typename ValueType>
class hash_iterator {
public:
	using iterator_category = bidirectional_iterator_tag;

	using size_type = size_t;
	using iterator = hash_iterator<NodeType, ValueType>;
	using non_const_iterator = hash_iterator<remove_const_t<NodeType> *, remove_const_t<ValueType>>;
	using const_iterator = hash_iterator<add_const_t<NodeType> *, add_const_t<ValueType>>;
	using difference_type = ptrdiff_t;
	using value_type = ValueType;
	using reference = value_type &;
	using pointer = value_type *;

	hash_iterator() noexcept { }

	hash_iterator(const non_const_iterator &other) noexcept
	: begin(other.init_node()), current(other.node()), end(other.final_node()) { }

	hash_iterator(const const_iterator &other) noexcept
	: begin(other.init_node()), current(other.node()), end(other.final_node()) { }

	explicit hash_iterator(NodeType *b, NodeType *p, NodeType *e) noexcept
	: begin(b), current(p), end(e) { }

	iterator &operator=(const iterator &other) noexcept {
		begin = other.begin;
		current = other.current;
		end = other.end;
		return *this;
	}
	constexpr bool operator==(const iterator &other) const { return current == other.current; }
	constexpr bool operator!=(const iterator &other) const { return current != other.current; }
	constexpr bool operator<(const iterator &other) const { return current < other.current; }
	constexpr bool operator>(const iterator &other) const { return current > other.current; }
	constexpr bool operator<=(const iterator &other) const { return current <= other.current; }
	constexpr bool operator>=(const iterator &other) const { return current >= other.current; }

	iterator &operator++() {
		if (current != end) {
			++current;
			while (!current->active && current != end) { ++current; }
		}
		return *this;
	}
	iterator operator++(int) {
		auto tmp = *this;
		if (current != end) {
			++current;
			while (!current->active && current != end) { ++current; }
		}
		return tmp;
	}
	iterator &operator--() {
		if (current != begin) {
			--current;
			while (!current->active && current != begin) { --current; }
		}
		return *this;
	}
	iterator operator--(int) {
		auto tmp = *this;
		if (current != begin) {
			--current;
			while (!current->active && current != begin) { --current; }
		}
		return tmp;
	}

	reference operator*() const { return current->value.ref(); }
	pointer operator->() const { return current->value.ptr(); }

	// const_iterator cast
	operator hash_iterator<const NodeType, const ValueType>() const {
		return hash_iterator<const NodeType, const ValueType>(begin, current, end);
	}

	NodeType *init_node() const { return begin; }
	NodeType *node() const { return current; }
	NodeType *final_node() const { return end; }

protected:
	NodeType *begin = nullptr;
	NodeType *current = nullptr;
	NodeType *end = nullptr;
};

template <typename NodeType>
struct hash_local_iterator {
	NodeType *target;

	hash_local_iterator(NodeType *ptr) : target(ptr) { }
};

template <typename Value, typename HashType, typename SizeType>
struct hash_node {
	using value_type = Value;

	aligned_storage<Value> value;
	HashType hash;
	SizeType next	: sizeof(SizeType) *__CHAR_BIT__ - 1 = 0;
	SizeType active : 1 = 0;
};

template <typename Key, typename Value, typename HashFn, typename EqualFn, typename Allocator,
		bool ForCxxStl = true>
class hash_memory {
public:
	using size_type = size_t;
	using hash_type = invoke_result_t<HashFn, Key>;
	using node_type = hash_node<Value, hash_type, size_type>;

	using allocator_type = Allocator;
	using node_allocator_type = allocator_type::template rebind<node_type>::other;

	using iterator = hash_iterator<node_type, Value>;
	using const_iterator = hash_iterator<add_const_t<node_type>, add_const_t<Value>>;
	using local_iterator = hash_local_iterator<node_type>;
	using const_local_iterator = hash_local_iterator<const node_type>;

	static constexpr float CoverageLoadFactor = 1.5f;
	static constexpr float DefaultMaxLoadFactor = 1.0f;

	// Find first node in chain with hashValue, or empty node to insert with hashValue
	static node_type *lookup_bucket_chain(node_type *storage, size_type capacity,
			hash_type hashValue) noexcept {
		auto end = storage + capacity;
		auto targetPos = hashValue % capacity;
		node_type *node = storage + targetPos;

		// Node at insert pos can store value with hash != hashValue;
		// In this case node->hash != insertPos
		// Skip this nodes and use first non-active or node, where node->hash == insertPos
		while (node->active && (node->hash % capacity) != targetPos) {
			++node;
			if (node >= end) {
				node -= capacity;
			}
		}
		return node;
	}

	~hash_memory() noexcept { clear_deallocate(); }

	hash_memory(size_type capacity, const HashFn &h, const EqualFn &eq,
			const allocator_type &a) noexcept
	: _hasher(h), _equal(eq), _allocator(a) {
		if (capacity != 0) {
			rehash(capacity);
		}
	}

	void clear_free() noexcept {
		if (_storage) {
			auto ptr = _storage;
			auto end = _storage + _capacity;

			while (ptr != end) {
				if (ptr->active) {
					ptr->value.destroy(_allocator);
					ptr->active = 0;
				}
				++ptr;
			}
			_size = 0;
			_hashMisses = 0;
		}
	}

	void clear_deallocate() noexcept {
		clear_free();
		if (_storage) {
			auto nodeAllocator = node_allocator_type(_allocator);
			nodeAllocator.destroy(_storage, _capacity);
			nodeAllocator.__deallocate(_storage, _capacity, _allocated);

			_storage = nullptr;
			_capacity = 0;
			_allocated = 0;
		}
	}

	template <typename... Args>
	pair<node_type *, bool> __try_emplace(node_type *storage, size_type capacity,
			hash_type hashValue, node_type *chain, const Key &key, Args &&...args) noexcept {
		// trace offset from original insert position
		// to speed-up worth case iteration
		bool equalNodeFound = false;
		size_type offset = 0;
		node_type *prev = chain;

		if (chain->active) {
			// If chain is active, we can use chain->hash to early detect equality,
			// before using _equal.
			// Note that ->next node is always also active
			while (chain->next != 0 && chain->hash != hashValue
					&& !(equalNodeFound = _equal(key,
								 aligned_storage_kv_traits<Key, Value>::extract_key(
										 chain->value)))) {
				chain += chain->next;
				if (chain >= storage + capacity) {
					chain -= capacity;
				}
			}
		}

		if (equalNodeFound) {
			// equality found - return it
			return pair<node_type *, bool>{chain, false};
		} else if (chain->active && chain->next == 0) {
			// we found the end of chain, all chain nodes has different values
			// we should now find a new node for emplace
			prev = chain;

			// If size < capacity, there are always some place for a node, but some sanity control will not harm.
			// Limit iteration with capacity to exit, when control does full circle
			while (offset < capacity && chain->active) {
				// just look for an inactive node
				++offset;
				++chain;
				if (chain >= storage + capacity) {
					chain -= capacity;
				}
			}

			if (chain->active) {
				// Fail to find a space for node, should not happen normally
				return pair<node_type *, bool>{nullptr, false};
			}
		}

		aligned_storage_kv_traits<Key, Value>::construct(_allocator, chain->value,
				sprt::forward<Args>(args)...);

		chain->active = 1;
		chain->hash = hashValue;
		if (prev) {
			prev->next = offset;
		}
		return pair<node_type *, bool>{chain, true};
	}

	bool rehashTo(node_type *newStorage, size_type newCapacity, size_type &hashMisses) noexcept {
		auto source = _storage;
		auto end = _storage + _capacity;

		while (source != end) {
			if (!source->active) {
				++source;
				continue;
			}

			auto chain = lookup_bucket_chain(newStorage, newCapacity, source->hash);
			if (!chain) {
				return false;
			} else if (chain->active) {
				++hashMisses;
			}

			pair<node_type *, bool> result{nullptr, false};
			if constexpr (is_move_constructible_v<Value>) {
				result = __try_emplace(newStorage, newCapacity, source->hash, chain,
						aligned_storage_kv_traits<Key, Value>::extract_key(source->value),
						sprt::move_unsafe(source->value.ref()));
			} else {
				result = __try_emplace(newStorage, newCapacity, source->hash, chain,
						aligned_storage_kv_traits<Key, Value>::extract_key(source->value),
						source->value.ref());
			}

			if (result.first == nullptr) {
				// fail to rehash - chain overflow. Normally - should not happen,
				// because original storage should not contain chains this long
				return false;
			}

			++source;
		}
		return true;
	}

	bool rehash(size_type newsize) noexcept {
		if (newsize == 0) {
			newsize = sprt::memory::config::BlockThreshold / sizeof(node_type);
		}
		newsize = __builtin_floorf(sprt::max(float(newsize), _size / _maxLoadFactor));

		auto nodeAllocator = node_allocator_type(_allocator);

		size_type allocSize = newsize;

		// use extra memory if provided by allocator
		size_t allocated = allocSize * sizeof(node_type); // real memory block size returned
		node_type *ptr = nodeAllocator.__allocate(allocSize, allocated);
		allocSize = allocated / sizeof(node_type);

		node_type *target = ptr;
		node_type *end = ptr + allocSize;

		while (target != end) { nodeAllocator.construct(target++); }

		size_type hashMisses = 0;

		if (rehashTo(ptr, allocSize, hashMisses)) {
			clear_deallocate();

			_storage = ptr;
			_capacity = allocSize;
			_allocated = allocated;
			_hashMisses = hashMisses;

			return true;
		} else {
			target = ptr;
			while (target != end) { nodeAllocator.destroy(target++); }
			nodeAllocator.__deallocate(ptr, allocSize, allocated);
		}
		return false;
	}

	node_type *find_bucket_or_grow(size_type hashValue) noexcept {
		auto chain = lookup_bucket_chain(_storage, _capacity, hashValue);
		if (!chain) {
			// failed to
			rehash(_capacity * 2);

			chain = lookup_bucket_chain(_storage, _capacity, hashValue);
		} else if (chain->active) {
			float newLoadFactor = float(_size) / (float(_size) - float(_hashMisses + 1));
			if (newLoadFactor > _maxLoadFactor) {
				rehash(_capacity * 2);

				chain = lookup_bucket_chain(_storage, _capacity, hashValue);
			}
		}
		return chain;
	}

	template <typename Arg, typename Iterator = iterator>
	auto insert(Arg &&arg) noexcept -> pair<Iterator, bool> {
		if (_size == _capacity) {
			rehash(_capacity * 2);
		}

		auto &key = aligned_storage_kv_traits<Key, Value>::extract_key(arg);
		auto hashValue = _hasher(key);

		auto chain = find_bucket_or_grow(hashValue);
		if (!chain) {
			return pair<Iterator, bool>(Iterator(nullptr, nullptr, nullptr), false);
		} else if (chain->active) {
			++_hashMisses;
		}

		auto result =
				__try_emplace(_storage, _capacity, hashValue, chain, key, sprt::forward<Arg>(arg));
		if (result.second) {
			++_size;
		}
		return pair<Iterator, bool>(Iterator(_storage, result.first, _storage + _capacity),
				result.second);
	}

	template <typename K, typename... Args>
	pair<iterator, bool> try_emplace(K &&key, Args &&...args) noexcept {
		if (_size == _capacity) {
			rehash(_capacity * 2);
		}

		auto hashValue = _hasher(key);

		auto chain = find_bucket_or_grow(hashValue);
		if (!chain) {
			return pair<iterator, bool>(iterator(nullptr, nullptr, nullptr), false);
		} else if (chain->active) {
			++_hashMisses;
		}

		pair<node_type *, bool> result = __try_emplace(_storage, _capacity, hashValue, chain, key,
				sprt::forward<K>(key), sprt::forward<Args>(args)...);
		if (result.second) {
			++_size;
		}
		return pair<iterator, bool>(iterator(_storage, result.first, _storage + _capacity),
				result.second);
	}

	void copy_from(const hash_memory &other) noexcept {
		if (_capacity > other._capacity && _capacity * 2 < other._capacity) {
			// reuse memory
			clear_free();
		} else {
			// reallocate memory
			clear_deallocate();
			rehash(other._capacity);
		}
		for (auto &it : other) { insert(it); }
	}

	void move_from(hash_memory &&other) noexcept {
		if (_allocator == other._allocator) {
			clear_deallocate();
			_storage = other._storage;
			_size = other._size;
			_capacity = other._capacity;
			_allocated = other._allocated;
			_hashMisses = other._hashMisses;
			_maxLoadFactor = other._maxLoadFactor;

			other._storage = nullptr;
			other._size = 0;
			other._capacity = 0;
			other._allocated = 0;
			other._hashMisses = 0;
			other._maxLoadFactor = DefaultMaxLoadFactor;
		} else {
			copy_from(other);
		}
	}

	node_type *erase_node(node_type *node) noexcept {
		auto returnNext = [&](node_type *n) {
			do { ++n; } while (!n->active && n < (_storage + _capacity));
			return node;
		};

		node->value.destroy(_allocator);

		--_size;

		// Check if there are next node in chain
		if (node->next == 0) {
			// No need to care about next nodes

			node->active = 0;

			// If we are last node in chain - check, if we need to update previous node

			// On right-placed node we have no previous node, just disable self
			if (node->hash % _capacity != node - _storage) {
				return returnNext(node);
			}

			--_hashMisses;

			// Otherwise - find chain's root
			auto chain = lookup_bucket_chain(_storage, _capacity, node->hash);

			// if we are head of chain - it's also simple case
			if (node == chain) {
				return returnNext(node);
			}

			// Find prev node by iterating through chain
			auto prev = chain;
			while (chain != node && chain->active && chain->next) {
				prev = chain;
				chain += chain->next;
				if (chain >= _storage + _capacity) {
					chain -= _capacity;
				}
			}

			sprt_passert(chain == node, "hash_memory: corrupted container: invalid hash chain");

			// mark prev node as new last node in chain
			prev->next = 0;
			return returnNext(node);
		} else {

			--_hashMisses;

			// There are more effective algorithm, if we should not follow c++ STL limitations.
			if constexpr (ForCxxStl) {
				// We need to maintain container consistency to match c++ standard:
				// no interators or reference except for this one should be invalidated

				// We should find previous node in chain, and set it to point the next one from current
				auto chain = lookup_bucket_chain(_storage, _capacity, node->hash);
				auto prev = chain;
				while (chain != node && chain->active && chain->next) {
					prev = chain;
					chain += chain->next;
					if (chain >= _storage + _capacity) {
						chain -= _capacity;
					}
				}

				prev->next += node->next;
				if (prev->next > _capacity) {
					// handle potential chain overflow by remove extra cycle
					prev->next -= _capacity;
				}
				node->active = 0;
				return returnNext(node);
			} else {
				// Without C++ STL, we can invalidate iterators. With this algorithm, we move-assign last
				// node in chain to this node. It provides more optimal layout, reducing search and insertion penalty.

				// We have some next nodes it chain
				// In this case, we replace self with the last node in chain to maintain optimal layout
				// No need to care about previous nodes, chain before this node remains consistent
				// Current node also remains active

				// We need to know the last node in chain and node before it to mark it as last
				auto chain = node;
				auto prev = chain;
				while (chain->next != 0) {
					prev = chain;
					chain += chain->next;
					if (chain >= _storage + _capacity) {
						chain -= _capacity;
					}
				}

				// mark prev node as last
				// if prev == node (initial node is the one before end) - it's also valid
				prev->next = 0;

				// Move value from the last node using move constructor
				node->value.construct(_allocator, sprt::move_unsafe(chain->value.ref()));

				// Clear last node
				chain->value.destroy(_allocator);
				chain->active = 0;
				chain->next = 0;

				// return self as a next iterator
				return node;
			}
		}
	}

	const_iterator erase(const_iterator iter) noexcept {
		sprt_passert(iter.init_node() == _storage && iter.final_node() == _storage + _capacity,
				"Invalid hash_memory iterator: invalid value range");
		node_type *node = const_cast<node_type *>(iter.node());

		auto nextNode = erase_node(node);

		return const_iterator(_storage, nextNode, _storage + _capacity);
	}

	iterator erase(iterator iter) noexcept {
		sprt_passert(iter.init_node() == _storage && iter.final_node() == _storage + _capacity,
				"Invalid hash_memory iterator: invalid value range");
		node_type *node = iter.node();

		auto nextNode = erase_node(node);

		return iterator(_storage, nextNode, _storage + _capacity);
	}

	template <typename K>
	const node_type *find_node(const K &k) const noexcept {
		if (!_storage) {
			return nullptr;
		}

		auto hashValue = _hasher(k);

		auto chain = lookup_bucket_chain(const_cast<node_type *>(_storage), _capacity, hashValue);
		if (!chain->active) {
			return nullptr;
		}

		bool equalNodeFound = false;
		if (chain->active) {
			// If chain is active, we can use chain->hash to early detect equality,
			// before using _equal.
			// Note that ->next node is always also active
			while (chain->next != 0 && chain->hash != hashValue
					&& !(equalNodeFound = _equal(k,
								 aligned_storage_kv_traits<Key, Value>::extract_key(
										 chain->value)))) {
				chain += chain->next;
				if (chain >= _storage + _capacity) {
					chain -= _capacity;
				}
			}
		}

		// target node found
		if (equalNodeFound) {
			return chain;
		}

		// tail node was not checked for equality
		if (chain->next == 0 && chain->hash == hashValue
				&& _equal(k, aligned_storage_kv_traits<Key, Value>::extract_key(chain->value))) {
			return chain;
		}
		return nullptr;
	}

	template <typename K>
	iterator find(K &&k) noexcept {
		auto node = find_node(k);
		if (!node) {
			return end();
		}
		return iterator(_storage, const_cast<node_type *>(node), _storage + _capacity);
	}

	template <typename K>
	const_iterator find(K &&k) const noexcept {
		auto node = find_node(k);
		if (!node) {
			return end();
		}
		return const_iterator(_storage, node, _storage + _capacity);
	}

	template <typename K>
	size_type count(K &&k) const noexcept {
		auto hashValue = _hasher(k);
		auto chain = lookup_bucket_chain(const_cast<node_type *>(_storage), _capacity, hashValue);

		size_type counter = 0;
		if (chain->active) {
			while (chain->next != 0) {
				if (chain->hash == hashValue
						&& _equal(k,
								aligned_storage_kv_traits<Key, Value>::extract_key(chain->value))) {
					++counter;
				}
				chain += chain->next;
				if (chain >= _storage + _capacity) {
					chain -= _capacity;
				}
			}

			if (chain->next == 0 && chain->hash == hashValue
					&& _equal(k,
							aligned_storage_kv_traits<Key, Value>::extract_key(chain->value))) {
				++counter;
			}
		}
		return counter;
	}

	template <typename K>
	sprt::pair<const node_type *, const node_type *> equal_range_nodes(K &&k) const noexcept {
		auto node = find_node(k);
		auto endNode = _storage + _capacity;
		auto lastNode = node;

		auto isEqual = [&](const node_type *n) {
			return node->hash == n->hash
					&& _equal(k, aligned_storage_kv_traits<Key, Value>::extract_key(n->value));
		};

		do { ++lastNode; } while (lastNode < endNode && isEqual(lastNode));

		return sprt::make_pair(node, lastNode);
	}

	template <typename K>
	sprt::pair<const_iterator, const_iterator> equal_range(K &&k) const noexcept {
		auto nodes = equal_range_nodes(k);

		return sprt::make_pair(const_iterator(_storage, nodes.first, _storage + _capacity),
				const_iterator(_storage, nodes.second, _storage + _capacity));
	}

	template <typename K>
	sprt::pair<iterator, iterator> equal_range(K &&k) noexcept {
		auto nodes = equal_range_nodes(k);

		return sprt::make_pair(
				iterator(_storage, const_cast<node_type *>(nodes.first), _storage + _capacity),
				iterator(_storage, const_cast<node_type *>(nodes.second), _storage + _capacity));
	}

	void swap(hash_memory &other) noexcept {
		if constexpr (sprt::allocator_traits<allocator_type>::propagate_on_container_swap::value) {
			sprt::swap(_allocator, other._allocator);
			sprt::swap(_hasher, other._hasher);
			sprt::swap(_equal, other._equal);
			sprt::swap(_storage, other._storage);
			sprt::swap(_size, other._size);
			sprt::swap(_capacity, other._capacity);
			sprt::swap(_allocated, other._allocated);
			sprt::swap(_hashMisses, other._hashMisses);
			sprt::swap(_maxLoadFactor, other._maxLoadFactor);
		} else if (other._allocator == _allocator) {
			sprt::swap(_hasher, other._hasher);
			sprt::swap(_equal, other._equal);
			sprt::swap(_storage, other._storage);
			sprt::swap(_size, other._size);
			sprt::swap(_capacity, other._capacity);
			sprt::swap(_allocated, other._allocated);
			sprt::swap(_hashMisses, other._hashMisses);
			sprt::swap(_maxLoadFactor, other._maxLoadFactor);
		}
	}

	iterator begin() noexcept {
		if (!_storage) {
			return iterator(nullptr, nullptr, nullptr);
		}

		auto it = _storage;
		auto end = _storage + _capacity;

		while (it != end && !it->active) { ++it; }
		return iterator(_storage, it, end);
	}
	const_iterator begin() const noexcept {
		if (!_storage) {
			return const_iterator(nullptr, nullptr, nullptr);
		}

		auto it = _storage;
		auto end = _storage + _capacity;

		while (it != end && !it->active) { ++it; }
		return const_iterator(_storage, it, end);
	}
	iterator end() noexcept {
		if (!_storage) {
			return iterator(nullptr, nullptr, nullptr);
		}

		return iterator(_storage, _storage + _capacity, _storage + _capacity);
	}
	const_iterator end() const noexcept {
		if (!_storage) {
			return const_iterator(nullptr, nullptr, nullptr);
		}

		return const_iterator(_storage, _storage + _capacity, _storage + _capacity);
	}

	auto size() const noexcept { return _size; }
	auto max_size() const noexcept { return Max<size_type> >> 1; }

	auto get_allocator() const noexcept { return _allocator; }
	void set_allocator(const Allocator &a) noexcept {
		if (a != _allocator) {
			clear_deallocate();
			_allocator = a;
		}
	}
	void set_allocator(Allocator &&a) noexcept {
		if (a != _allocator) {
			clear_deallocate();
			_allocator = sprt::move_unsafe(a);
		}
	}

	auto hash_function() const noexcept { return _hasher; }
	auto key_eq() const noexcept { return _equal; }

	float load_factor() const noexcept {
		if (_size > 0) {
			return float(_size) / (float(_size) - float(_hashMisses));
		}
		return 0.0f;
	}

	float max_load_factor() const noexcept { return _maxLoadFactor; }
	void max_load_factor(float ml) noexcept { _maxLoadFactor = sprt::max(ml, 1.0f); }

protected:
	[[no_unique_address]]
	HashFn _hasher;

	[[no_unique_address]]
	EqualFn _equal;

	[[no_unique_address]]
	allocator_type _allocator;

	node_type *_storage = nullptr;
	size_type _size = 0;
	size_type _capacity = 0;
	size_type _allocated = 0;
	size_type _hashMisses = 0;
	float _maxLoadFactor = DefaultMaxLoadFactor;
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_HASH_MEMORY_H_
