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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INT_HASH_MEMORY_H_
#define RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INT_HASH_MEMORY_H_

#include <sprt/runtime/mem/pool.h>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/c/__sprt_assert.h>

namespace sprt::detail {

// Truncated hash_memory (see hash_memory.h) for unsigned integers, where a value IS its own hash:
// the node's `hash` field is the stored value itself, so there is no separate value storage, no
// HashFn and no EqualFn — equality is hash equality. The collision-chain algorithm (offset-linked
// chains, hash-miss accounting, backward compaction on erase) is the same as in hash_memory.

template <typename NodeType, typename ValueType>
class int_hash_iterator {
public:
	using iterator_category = bidirectional_iterator_tag;

	using size_type = size_t;
	using iterator = int_hash_iterator<NodeType, ValueType>;
	using non_const_iterator = int_hash_iterator<remove_const_t<NodeType>, ValueType>;
	using difference_type = ptrdiff_t;
	using value_type = ValueType;

	// keys of a set are immutable — both iterator flavors expose const values
	using reference = const value_type &;
	using pointer = const value_type *;

	int_hash_iterator() noexcept { }

	int_hash_iterator(const non_const_iterator &other) noexcept
	: begin(other.init_node()), current(other.node()), end(other.final_node()) { }

	explicit int_hash_iterator(NodeType *b, NodeType *p, NodeType *e) noexcept
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
			while (current != end && !current->active) { ++current; }
		}
		return *this;
	}
	iterator operator++(int) {
		auto tmp = *this;
		++(*this);
		return tmp;
	}
	iterator &operator--() {
		if (current != begin) {
			--current;
			while (current != begin && !current->active) { --current; }
		}
		return *this;
	}
	iterator operator--(int) {
		auto tmp = *this;
		--(*this);
		return tmp;
	}

	reference operator*() const { return current->hash; }
	pointer operator->() const { return &current->hash; }

	operator int_hash_iterator<const NodeType, ValueType>() const {
		return int_hash_iterator<const NodeType, ValueType>(begin, current, end);
	}

	NodeType *init_node() const { return begin; }
	NodeType *node() const { return current; }
	NodeType *final_node() const { return end; }

protected:
	NodeType *begin = nullptr;
	NodeType *current = nullptr;
	NodeType *end = nullptr;
};

template <typename T>
struct int_hash_node {
	using value_type = T;

	// The stored value doubles as its hash
	T hash = 0;
	// `next` is a collision-chain offset taken modulo _capacity (see hash_memory.h). It is two
	// bits narrower than T (it shares the word with is_first/active), so table capacity must stay
	// below 2^(bits(T) - 2) — for uint32_t that is ~2^30 buckets; enforced in rehash().
	T next	   : sizeof(T) * __CHAR_BIT__ - 2 = 0;
	T is_first : 1 = 0;
	T active   : 1 = 0;
};

template <typename T, typename Allocator>
class int_hash_memory {
public:
	static_assert(is_unsigned_v<T> && (sizeof(T) == 4 || sizeof(T) == 8),
			"int_hash_memory supports only 32/64-bit unsigned integers (uint32_t/uint64_t)");

	using size_type = size_t;
	using value_type = T;
	using hash_type = T;
	using node_type = int_hash_node<T>;

	using allocator_type = Allocator;
	using node_allocator_type = allocator_type::template rebind<node_type>::other;

	using iterator = int_hash_iterator<node_type, T>;
	using const_iterator = int_hash_iterator<const node_type, T>;

	static constexpr float CoverageLoadFactor = 1.5f;
	static constexpr float DefaultMaxLoadFactor = 1.0f;

	// capacity bound of the `next` chain-offset field (computed in 64 bits so the shift is valid
	// even when bits(T) - 2 >= bits(size_type))
	static constexpr size_type MaxCapacity =
			((unsigned long long)(1) << (sizeof(T) * __CHAR_BIT__ - 2)) - 1
					> (unsigned long long)(Max<size_type> >> 1)
			? (Max<size_type> >> 1)
			: size_type(((unsigned long long)(1) << (sizeof(T) * __CHAR_BIT__ - 2)) - 1);

	// Find first node in chain with hashValue, or empty node to insert with hashValue
	static node_type *lookup_bucket_chain(node_type *storage, size_type capacity,
			hash_type hashValue, node_type *erased = nullptr) noexcept {
		auto end = storage + capacity;
		auto targetPos = size_type(hashValue % capacity);
		node_type *node = storage + targetPos;

		// Node at insert pos can store value with hash != hashValue;
		// Skip such nodes and use first non-active node or node, where node->hash % capacity == targetPos
		while (node == erased || (node->active && size_type(node->hash % capacity) != targetPos)) {
			++node;
			if (node >= end) {
				node -= capacity;
			}
		}
		return node;
	}

	~int_hash_memory() noexcept { clear_deallocate(); }

	int_hash_memory(size_type capacity, const allocator_type &a) noexcept : _allocator(a) {
		if (capacity != 0) {
			rehash(capacity);
		}
	}

	void clear_free() noexcept {
		if (_storage) {
			// nodes are trivial and the empty state is all-zeroes (hash of an inactive
			// node is never read), so clearing degenerates into zeroing the block
			__builtin_memset((void *)_storage, 0, _capacity * sizeof(node_type));
			_size = 0;
			_hashMisses = 0;
		}
	}

	void clear_deallocate() noexcept {
		clear_free();
		if (_storage) {
			auto nodeAllocator = node_allocator_type(_allocator);
			nodeAllocator.__deallocate(_storage, _capacity, _allocated);

			_storage = nullptr;
			_capacity = 0;
			_allocated = 0;
		}
	}

	pair<node_type *, bool> __try_emplace(node_type *storage, size_type capacity,
			hash_type hashValue, node_type *chain) noexcept {
		// trace offset from original insert position
		// to speed-up worth case iteration
		bool equalNodeFound = false;
		size_type offset = 0;
		node_type *prev = chain;
		auto hashPosition = size_type(hashValue % capacity);

		if (chain->active) {
			// The value is the hash, so equality is plain hash comparison.
			// Note that ->next node is always also active
			while (chain->next != 0 && size_type(chain->hash % capacity) == hashPosition
					&& !(equalNodeFound = (chain->hash == hashValue))) {
				chain += chain->next;
				if (chain >= storage + capacity) {
					chain -= capacity;
				}
			}
		}

		// The while loop only compares nodes with next != 0, so the LAST node in the chain
		// (which includes a single-node chain) is never checked. Compare it explicitly, mirroring
		// find_node's tail check, so insert on an existing tail key returns that node
		// instead of silently inserting a duplicate.
		if (!equalNodeFound && chain->active && chain->next == 0 && chain->hash == hashValue) {
			equalNodeFound = true;
		}

		if (equalNodeFound) {
			// equality found - return it
			return pair<node_type *, bool>{chain, false};
		} else if (chain->active) {
			if (chain->next != 0) {
				// Fail to find a space for node, should not happen normally
				return pair<node_type *, bool>{nullptr, false};
			}

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
			chain->is_first = 0;
		} else {
			// Node will be first in chain
			chain->is_first = 1;
		}

		chain->hash = hashValue;
		chain->active = 1;
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

			auto result = __try_emplace(newStorage, newCapacity, source->hash, chain);
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

		sprt_passert(allocSize <= MaxCapacity,
				"int_hash_memory: capacity exceeds `next` chain-offset field range");

		// same degenerate case as clear_free: default node state is all-zeroes
		__builtin_memset((void *)ptr, 0, allocated);

		size_type hashMisses = 0;

		auto size = _size;

		if (rehashTo(ptr, allocSize, hashMisses)) {

			clear_deallocate();

			_size = size;
			_capacity = allocSize;
			_allocated = allocated;
			_hashMisses = hashMisses;
			_storage = ptr;

			return true;
		} else {
			nodeAllocator.__deallocate(ptr, allocSize, allocated);
		}
		return false;
	}

	node_type *find_bucket_or_grow(hash_type hashValue) noexcept {
		auto newCapacity = sprt::max(size_t(4), _capacity * 2);

		auto chain = lookup_bucket_chain(_storage, _capacity, hashValue);
		if (!chain) {
			rehash(newCapacity);

			chain = lookup_bucket_chain(_storage, _capacity, hashValue);
		} else if (chain->active) {
			float newLoadFactor = float(_size + 1) / (float(_size + 1) - float(_hashMisses + 1));
			if (newLoadFactor > _maxLoadFactor || _size + 1 >= _capacity) {
				rehash(newCapacity);

				chain = lookup_bucket_chain(_storage, _capacity, hashValue);
			}
		}
		return chain;
	}

	template <typename Iterator = iterator>
	auto insert(T value) noexcept -> pair<Iterator, bool> {
		// A lookup must never grow the table. find_bucket_or_grow rehashes preemptively on an
		// occupied bucket, so if the key already exists, return it directly instead — otherwise
		// repeated inserts of an existing key would grow the table unboundedly without inserting.
		if (auto existing = find_node(value)) {
			return pair<Iterator, bool>(
					Iterator(_storage, const_cast<node_type *>(existing), _storage + _capacity),
					false);
		}

		if (_size == _capacity) {
			rehash(_capacity * 2);
		}

		auto chain = find_bucket_or_grow(value);
		if (!chain) {
			return pair<Iterator, bool>(Iterator(nullptr, nullptr, nullptr), false);
		}
		// A hash miss (a node not at its home bucket) is only created when we ACTUALLY insert into
		// an already-occupied bucket. Defer the increment to a real insertion (see hash_memory.h).
		const bool __collision = chain->active;

		auto result = __try_emplace(_storage, _capacity, value, chain);
		if (result.second) {
			++_size;
			if (__collision) {
				++_hashMisses;
			}
		}
		return pair<Iterator, bool>(Iterator(_storage, result.first, _storage + _capacity),
				result.second);
	}

	void copy_from(const int_hash_memory &other) noexcept {
		// reuse our storage if it is big enough to hold other's elements but not
		// excessively oversized
		if (_capacity >= other._capacity && _capacity < other._capacity * 2) {
			// reuse memory
			clear_free();
		} else {
			// reallocate memory
			clear_deallocate();
			rehash(other._capacity);
		}
		for (auto &it : other) { insert(it); }
	}

	void move_from(int_hash_memory &&other) noexcept {
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
			make_consistent_after_erase(n);
			do {
				++n; //
			} while (n < (_storage + _capacity) && !n->active);
			return n;
		};

		--_size;

		// Check if there are next node in chain
		if (node->next == 0) {
			// No need to care about next nodes

			node->active = 0;

			// If we are last node in chain - check, if we need to update previous node

			// On right-placed node we have no previous node, just disable self
			if (size_type(node->hash % _capacity) == size_type(node - _storage)) {
				return returnNext(node);
			}

			// Node was a hash miss, decrement hash misses for stats
			--_hashMisses;

			auto prev = find_prev_in_chain(node);
			if (prev) {
				// mark prev node as new last node in chain
				prev->next = 0;
			}
			return returnNext(node);
		} else {
			// we will remove a single hash miss
			--_hashMisses;

			// We have some next nodes in chain
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

			// Move value from the last node — a plain integer copy here
			node->hash = chain->hash;

			chain->active = 0;
			chain->next = 0;

			make_consistent_after_erase(chain);

			// return self as a next iterator
			return node;
		}
	}

	const_iterator erase(const_iterator iter) noexcept {
		sprt_passert(iter.init_node() == _storage && iter.final_node() == _storage + _capacity,
				"Invalid int_hash_memory iterator: invalid value range");
		node_type *node = const_cast<node_type *>(iter.node());

		auto nextNode = erase_node(node);

		return const_iterator(_storage, nextNode, _storage + _capacity);
	}

	iterator erase(iterator iter) noexcept {
		sprt_passert(iter.init_node() == _storage && iter.final_node() == _storage + _capacity,
				"Invalid int_hash_memory iterator: invalid value range");
		node_type *node = iter.node();

		auto nextNode = erase_node(node);

		return iterator(_storage, nextNode, _storage + _capacity);
	}

	const node_type *find_node(T value) const noexcept {
		if (!_storage) {
			return nullptr;
		}

		auto hashValue = value;
		auto hashPosition = size_type(hashValue % _capacity);

		auto chain = lookup_bucket_chain(const_cast<node_type *>(_storage), _capacity, hashValue);
		if (!chain->active) {
			return nullptr;
		}

		// The value is the hash — equality is plain hash comparison
		do {
			if (chain->hash == hashValue) {
				return chain;
			}
			chain += chain->next;
			if (chain >= _storage + _capacity) {
				chain -= _capacity;
			}
		} while (chain->next != 0 && size_type(chain->hash % _capacity) == hashPosition);

		// tail node was not checked for equality
		if (chain->next == 0 && chain->hash == hashValue) {
			return chain;
		}
		return nullptr;
	}

	iterator find(T value) noexcept {
		auto node = find_node(value);
		if (!node) {
			return end();
		}
		return iterator(_storage, const_cast<node_type *>(node), _storage + _capacity);
	}

	const_iterator find(T value) const noexcept {
		auto node = find_node(value);
		if (!node) {
			return end();
		}
		return const_iterator(_storage, node, _storage + _capacity);
	}

	size_type count(T value) const noexcept {
		// keys are unique — count is 0 or 1
		return find_node(value) ? 1 : 0;
	}

	void swap(int_hash_memory &other) noexcept {
		if constexpr (sprt::allocator_traits<allocator_type>::propagate_on_container_swap::value) {
			sprt::swap(_allocator, other._allocator);
			sprt::swap(_storage, other._storage);
			sprt::swap(_size, other._size);
			sprt::swap(_capacity, other._capacity);
			sprt::swap(_allocated, other._allocated);
			sprt::swap(_hashMisses, other._hashMisses);
			sprt::swap(_maxLoadFactor, other._maxLoadFactor);
		} else if (other._allocator == _allocator) {
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
	auto max_size() const noexcept { return MaxCapacity; }

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

	float load_factor() const noexcept {
		if (_size > 0) {
			return float(_size) / (float(_size) - float(_hashMisses));
		}
		return 0.0f;
	}

	float max_load_factor() const noexcept { return _maxLoadFactor; }
	void max_load_factor(float ml) noexcept { _maxLoadFactor = sprt::max(ml, 1.0f); }

protected:
	node_type *find_prev_in_chain(node_type *node, node_type *erased = nullptr) {
		auto chain = lookup_bucket_chain(_storage, _capacity, node->hash, erased);

		// if we are the head of chain
		if (node == chain) {
			return nullptr;
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

		sprt_passert(chain == node, "int_hash_memory: corrupted container: invalid hash chain");
		return prev;
	};

	// when node is erased, there can be some neighbor nodes, that should take it's place
	void make_consistent_after_erase(node_type *erase_pos) {
		auto moveBackward = [&](node_type *neighbor) {
			size_t offset = neighbor + _capacity - erase_pos;
			if (offset > _capacity) {
				offset -= _capacity;
			}

			auto prev = find_prev_in_chain(neighbor, erase_pos);

			erase_pos->hash = neighbor->hash;
			erase_pos->next = (neighbor->next == 0) ? 0 : (neighbor->next + offset) % _capacity;
			erase_pos->active = 1;

			if (prev) {
				if (prev->next > offset) {
					prev->next -= offset;
				} else {
					// swap prev and erase_pos in chain
					size_t prevOffset = prev + _capacity - erase_pos;
					if (prevOffset > _capacity) {
						prevOffset -= _capacity;
					}

					erase_pos->next = prevOffset;
					prev->next = (neighbor->next + (offset - prevOffset)) % _capacity;
				}
			}

			neighbor->active = 0;
			neighbor->next = 0;

			make_consistent_after_erase(neighbor);
		};

		auto hashPos = size_type(erase_pos - _storage);

		node_type *neighbor = erase_pos + 1;
		if (neighbor >= _storage + _capacity) {
			neighbor -= _capacity;
		}

		size_t offset = 1;
		size_t targetScore = Max<size_t>;
		node_type *target = nullptr;

		size_t posOff = 0;
		while (neighbor->active) {
			if (size_type(neighbor->hash % _capacity) == hashPos) {
				moveBackward(neighbor);
				return;
			}

			posOff = ((neighbor - _storage) + _capacity - size_type(neighbor->hash % _capacity))
					% _capacity;

			if (posOff > offset) {
				if (posOff - offset < targetScore) {
					targetScore = posOff - offset;
					target = neighbor;
				}
			}

			++offset;
			++neighbor;
			if (neighbor >= _storage + _capacity) {
				neighbor -= _capacity;
			}
		}

		if (target) {
			moveBackward(target);
		}
	}

	SPRT_NO_UNIQUE_ADDRESS
	allocator_type _allocator;

	node_type *_storage = nullptr;
	size_type _size = 0;
	size_type _capacity = 0;
	size_type _allocated = 0;
	size_type _hashMisses = 0;
	float _maxLoadFactor = DefaultMaxLoadFactor;
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_DETAIL_INT_HASH_MEMORY_H_
