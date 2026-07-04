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

#ifndef RUNTIME_INCLUDE_LIBC_STL_UNORDERED_CHAIN_H_
#define RUNTIME_INCLUDE_LIBC_STL_UNORDERED_CHAIN_H_

// A separate-chaining hash table (bucket array of singly-linked heap-node lists) that backs
// std::unordered_multimap / unordered_multiset. This lives ENTIRELY in the STL layer (namespace
// std::__detail) — sprt itself has no multi machinery. Unlike the open-addressed
// sprt/cxx/detail/hash_memory (which the UNIQUE unordered_map/set use), this layout satisfies the two
// properties the standard requires of the equivalent-key ("multi") containers and that open
// addressing cannot give:
//   * elements with equivalent keys are ADJACENT in iteration order (they share a bucket list and are
//     kept consecutive there), so equal_range() is a valid contiguous range; and
//   * references/pointers to elements are STABLE (each element lives in its own heap node that is
//     never moved — only bucket-array pointers move on rehash).
// The value lives inline in the heap node (sprt::detail::aligned_storage), reusing
// sprt::detail::aligned_storage_kv_traits for key extraction / construction just like hash_memory.

#include <__sprt_stl_config.h>

#include <sprt/runtime/mem/pool.h>
#include <sprt/cxx/__algorithm/minmax.h>
#include <sprt/cxx/__utility/pair.h>
#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/detail/aligned_storage.h>
#include <sprt/cxx/iterator>
#include <sprt/c/__sprt_assert.h>

namespace std {
namespace __detail {

// The machinery is std-owned but implemented over sprt primitives (aligned_storage, pair, the type
// traits, move/forward/swap, Max/max). Pull them in locally so the body reads like the sprt original.
using namespace sprt;
using namespace sprt::detail;

template <typename Value, typename HashType>
struct chain_node {
	aligned_storage<Value> value; // inline in the heap node → stable element address
	chain_node *next = nullptr;
	HashType hash = 0;
};

// Forward iterator: walk each bucket's singly-linked list, buckets in index order. `Node` is the
// (possibly const) node type; the bucket cursor is always over the non-const node pointer array.
template <typename Node, typename ValueType>
class chain_iterator {
public:
	using bucket_type = remove_const_t<Node> *;

	using iterator_category = forward_iterator_tag;
	using value_type = ValueType;
	using reference = value_type &;
	using pointer = value_type *;
	using difference_type = ptrdiff_t;

	using non_const_iterator = chain_iterator<remove_const_t<Node>, remove_const_t<ValueType>>;

	Node *_node = nullptr; // current element (nullptr == end)
	bucket_type *_bucket = nullptr;
	bucket_type *_bucketEnd = nullptr;

	chain_iterator() noexcept { }
	chain_iterator(Node *n, bucket_type *b, bucket_type *e) noexcept
	: _node(n), _bucket(b), _bucketEnd(e) { }

	// const_iterator is constructible from iterator
	chain_iterator(const non_const_iterator &o) noexcept
	: _node(o._node), _bucket(o._bucket), _bucketEnd(o._bucketEnd) { }

	reference operator*() const { return _node->value.ref(); }
	pointer operator->() const { return _node->value.ptr(); }

	chain_iterator &operator++() noexcept {
		_node = _node->next;
		while (!_node && _bucket != _bucketEnd) {
			++_bucket;
			if (_bucket != _bucketEnd) {
				_node = *_bucket;
			}
		}
		return *this;
	}
	chain_iterator operator++(int) noexcept {
		auto tmp = *this;
		++(*this);
		return tmp;
	}

	constexpr bool operator==(const chain_iterator &o) const noexcept { return _node == o._node; }
	constexpr bool operator!=(const chain_iterator &o) const noexcept { return _node != o._node; }
};

template <typename Key, typename Value, typename HashFn, typename EqualFn, typename Allocator>
class chain_memory {
public:
	using size_type = size_t;
	using hash_type = invoke_result_t<HashFn, Key>;
	using node_type = chain_node<Value, hash_type>;

	using allocator_type = Allocator;
	using node_allocator_type = typename allocator_type::template rebind<node_type>::other;
	using bucket_allocator_type = typename allocator_type::template rebind<node_type *>::other;

	using iterator = chain_iterator<node_type, Value>;
	using const_iterator = chain_iterator<const node_type, add_const_t<Value>>;

	static constexpr float DefaultMaxLoadFactor = 1.0f;
	static constexpr size_type MinBuckets = 8;

	using kv = aligned_storage_kv_traits<Key, Value>;

	~chain_memory() noexcept { clear_deallocate(); }

	chain_memory(size_type n, const HashFn &h, const EqualFn &eq, const allocator_type &a) noexcept
	: _hasher(h), _equal(eq), _allocator(a) {
		if (n != 0) {
			rehash(n);
		}
	}

	// --- node lifetime ---
	template <typename... Args>
	node_type *make_node(Args &&...args) noexcept {
		node_allocator_type na(_allocator);
		size_type n = 1;
		node_type *node = na.__allocate(n);
		na.construct(node); // next=nullptr, hash=0, empty value storage
		kv::construct(_allocator, node->value, sprt::forward<Args>(args)...);
		node->next = nullptr;
		return node;
	}
	void free_node(node_type *node) noexcept {
		node->value.destroy(_allocator);
		node_allocator_type na(_allocator);
		na.destroy(node);
		na.__deallocate(node, 1, sizeof(node_type));
	}

	// --- buckets ---
	size_type bucket_count() const noexcept { return _bucketCount; }

	void free_buckets() noexcept {
		if (_buckets) {
			bucket_allocator_type ba(_allocator);
			ba.__deallocate(_buckets, _bucketCount, _bucketCount * sizeof(node_type *));
			_buckets = nullptr;
			_bucketCount = 0;
		}
	}

	void clear_free() noexcept {
		if (_buckets) {
			for (size_type i = 0; i < _bucketCount; ++i) {
				node_type *node = _buckets[i];
				while (node) {
					node_type *nx = node->next;
					free_node(node);
					node = nx;
				}
				_buckets[i] = nullptr;
			}
		}
		_size = 0;
	}
	void clear_deallocate() noexcept {
		clear_free();
		free_buckets();
	}

	// Re-thread every existing node into a fresh bucket array of `n` buckets. All elements with a
	// given key live in ONE old bucket with equal keys grouped, and prepending processes that run
	// consecutively, so equal keys stay grouped (just reversed) in the new bucket list.
	bool rehash(size_type n) noexcept {
		n = sprt::max(n, MinBuckets);
		if (n <= _bucketCount && _size == 0) {
			return true;
		}
		bucket_allocator_type ba(_allocator);
		size_type req = n;
		node_type **nb = ba.__allocate(req);
		for (size_type i = 0; i < n; ++i) { nb[i] = nullptr; }

		if (_buckets) {
			for (size_type i = 0; i < _bucketCount; ++i) {
				node_type *node = _buckets[i];
				while (node) {
					node_type *nx = node->next;
					size_type b = node->hash % n;
					node->next = nb[b];
					nb[b] = node;
					node = nx;
				}
			}
			free_buckets();
		}
		_buckets = nb;
		_bucketCount = n;
		return true;
	}

	void maybe_grow() noexcept {
		if (_bucketCount == 0) {
			rehash(MinBuckets);
		} else if (float(_size + 1) > float(_bucketCount) * _maxLoadFactor) {
			rehash(_bucketCount * 2);
		}
	}

	// --- iterators ---
	iterator make_iter(node_type *node, size_type bucket) noexcept {
		return iterator(node, _buckets + bucket, _buckets + _bucketCount);
	}
	// const_iterator → iterator (the buckets are the same non-const array; only the node is re-cast)
	iterator iter_cast(const_iterator ci) noexcept {
		return iterator(const_cast<node_type *>(ci._node), ci._bucket, ci._bucketEnd);
	}
	iterator begin() noexcept {
		for (size_type i = 0; i < _bucketCount; ++i) {
			if (_buckets[i]) {
				return iterator(_buckets[i], _buckets + i, _buckets + _bucketCount);
			}
		}
		return end();
	}
	const_iterator begin() const noexcept {
		for (size_type i = 0; i < _bucketCount; ++i) {
			if (_buckets[i]) {
				return const_iterator(_buckets[i], _buckets + i, _buckets + _bucketCount);
			}
		}
		return end();
	}
	iterator end() noexcept {
		return iterator(nullptr, _buckets + _bucketCount, _buckets + _bucketCount);
	}
	const_iterator end() const noexcept {
		return const_iterator(nullptr, _buckets + _bucketCount, _buckets + _bucketCount);
	}

	auto size() const noexcept { return _size; }
	auto max_size() const noexcept { return Max<size_type> >> 1; }

	// --- insertion (always inserts a new element; duplicates allowed) ---
	// Construct the element first, then hash its key — so this works for both a ready value
	// (insert) and arbitrary constructor arguments (emplace, incl. piecewise pairs). The new node
	// is linked right after the run of equal keys (or at the bucket head), keeping equals adjacent.
	template <typename... Args>
	iterator emplace_multi(Args &&...args) noexcept {
		node_type *node = make_node(sprt::forward<Args>(args)...);
		auto hv = _hasher(kv::extract_key(node->value));
		node->hash = hv;
		maybe_grow(); // node is not linked yet, so rehash won't touch it
		size_type b = hv % _bucketCount;
		node_type *tail = equal_run_tail(b, hv, kv::extract_key(node->value));
		if (tail) {
			node->next = tail->next;
			tail->next = node;
		} else {
			node->next = _buckets[b];
			_buckets[b] = node;
		}
		++_size;
		return make_iter(node, b);
	}

	template <typename Arg>
	iterator insert_multi(Arg &&arg) noexcept {
		return emplace_multi(sprt::forward<Arg>(arg));
	}

	// Last node of the equal run for `k` in bucket b, or nullptr if none present.
	template <typename K>
	node_type *equal_run_tail(size_type b, hash_type hv, const K &k) noexcept {
		node_type *last = nullptr;
		for (node_type *n = _buckets[b]; n; n = n->next) {
			if (n->hash == hv && _equal(k, kv::extract_key(n->value))) {
				last = n;
			} else if (last) {
				break;
			}
		}
		return last;
	}

	// --- lookup ---
	template <typename K>
	node_type *find_node(const K &k) const noexcept {
		if (!_buckets || _bucketCount == 0) {
			return nullptr;
		}
		auto hv = _hasher(k);
		for (node_type *n = _buckets[hv % _bucketCount]; n; n = n->next) {
			if (n->hash == hv && _equal(k, kv::extract_key(n->value))) {
				return n;
			}
		}
		return nullptr;
	}

	template <typename K>
	iterator find(const K &k) noexcept {
		auto n = find_node(k);
		if (!n) {
			return end();
		}
		return make_iter(n, n->hash % _bucketCount);
	}
	template <typename K>
	const_iterator find(const K &k) const noexcept {
		auto n = find_node(k);
		if (!n) {
			return end();
		}
		return const_iterator(n, _buckets + (n->hash % _bucketCount), _buckets + _bucketCount);
	}

	template <typename K>
	size_type count(const K &k) const noexcept {
		if (!_buckets || _bucketCount == 0) {
			return 0;
		}
		auto hv = _hasher(k);
		size_type c = 0;
		bool seen = false;
		for (node_type *n = _buckets[hv % _bucketCount]; n; n = n->next) {
			if (n->hash == hv && _equal(k, kv::extract_key(n->value))) {
				++c;
				seen = true;
			} else if (seen) {
				break; // equal keys are contiguous
			}
		}
		return c;
	}

	template <typename K>
	pair<iterator, iterator> equal_range(const K &k) noexcept {
		auto first = find(k);
		if (first == end()) {
			return pair<iterator, iterator>(end(), end());
		}
		auto last = first;
		while (last != end() && last._node->hash == first._node->hash
				&& _equal(k, kv::extract_key(last._node->value))) {
			++last;
		}
		return pair<iterator, iterator>(first, last);
	}
	template <typename K>
	pair<const_iterator, const_iterator> equal_range(const K &k) const noexcept {
		auto first = find(k);
		if (first == end()) {
			return pair<const_iterator, const_iterator>(end(), end());
		}
		auto last = first;
		while (last != end() && last._node->hash == first._node->hash
				&& _equal(k, kv::extract_key(last._node->value))) {
			++last;
		}
		return pair<const_iterator, const_iterator>(first, last);
	}

	// --- erase ---
	// Unlink `node` from bucket b, free it, return the node that followed it (or nullptr).
	node_type *unlink(size_type b, node_type *node) noexcept {
		node_type *nx = node->next;
		if (_buckets[b] == node) {
			_buckets[b] = nx;
		} else {
			node_type *p = _buckets[b];
			while (p && p->next != node) { p = p->next; }
			if (p) {
				p->next = nx;
			}
		}
		free_node(node);
		--_size;
		return nx;
	}

	iterator erase(const_iterator pos) noexcept {
		node_type *node = const_cast<node_type *>(pos._node);
		size_type b = node->hash % _bucketCount;
		node_type *nx = unlink(b, node);
		if (nx) {
			return make_iter(nx, b);
		}
		// advance to the next non-empty bucket
		for (size_type i = b + 1; i < _bucketCount; ++i) {
			if (_buckets[i]) {
				return iterator(_buckets[i], _buckets + i, _buckets + _bucketCount);
			}
		}
		return end();
	}

	template <typename K>
	size_type erase_all(const K &k) noexcept {
		if (!_buckets || _bucketCount == 0) {
			return 0;
		}
		auto hv = _hasher(k);
		size_type b = hv % _bucketCount;
		size_type removed = 0;
		node_type *prev = nullptr;
		node_type *n = _buckets[b];
		while (n) {
			if (n->hash == hv && _equal(k, kv::extract_key(n->value))) {
				node_type *nx = n->next;
				if (prev) {
					prev->next = nx;
				} else {
					_buckets[b] = nx;
				}
				free_node(n);
				--_size;
				++removed;
				n = nx;
			} else {
				prev = n;
				n = n->next;
			}
		}
		return removed;
	}

	// --- bulk ops ---
	void copy_from(const chain_memory &other) noexcept {
		clear_deallocate();
		rehash(sprt::max(other._bucketCount, MinBuckets));
		_maxLoadFactor = other._maxLoadFactor;
		// preserve iteration (and equal-run) order of `other`
		for (auto it = other.begin(); it != other.end(); ++it) { insert_multi(*it); }
	}
	void move_from(chain_memory &&other) noexcept {
		if (_allocator == other._allocator) {
			clear_deallocate();
			_buckets = other._buckets;
			_bucketCount = other._bucketCount;
			_size = other._size;
			_maxLoadFactor = other._maxLoadFactor;
			other._buckets = nullptr;
			other._bucketCount = 0;
			other._size = 0;
			other._maxLoadFactor = DefaultMaxLoadFactor;
		} else {
			copy_from(other);
		}
	}
	void swap(chain_memory &other) noexcept {
		sprt::swap(_hasher, other._hasher);
		sprt::swap(_equal, other._equal);
		sprt::swap(_allocator, other._allocator);
		sprt::swap(_buckets, other._buckets);
		sprt::swap(_bucketCount, other._bucketCount);
		sprt::swap(_size, other._size);
		sprt::swap(_maxLoadFactor, other._maxLoadFactor);
	}

	allocator_type get_allocator() const noexcept { return _allocator; }
	void set_allocator(const Allocator &a) noexcept {
		if (a != _allocator) {
			clear_deallocate();
			_allocator = a;
		}
	}
	HashFn hash_function() const noexcept { return _hasher; }
	EqualFn key_eq() const noexcept { return _equal; }

	float load_factor() const noexcept {
		return _bucketCount ? float(_size) / float(_bucketCount) : 0.0f;
	}
	float max_load_factor() const noexcept { return _maxLoadFactor; }
	void max_load_factor(float z) noexcept { _maxLoadFactor = sprt::max(z, 0.1f); }

protected:
	SPRT_NO_UNIQUE_ADDRESS HashFn _hasher;
	SPRT_NO_UNIQUE_ADDRESS EqualFn _equal;
	SPRT_NO_UNIQUE_ADDRESS allocator_type _allocator;

	node_type **_buckets = nullptr;
	size_type _bucketCount = 0;
	size_type _size = 0;
	float _maxLoadFactor = DefaultMaxLoadFactor;
};

} // namespace __detail
} // namespace std

#endif // RUNTIME_INCLUDE_LIBC_STL_UNORDERED_CHAIN_H_
