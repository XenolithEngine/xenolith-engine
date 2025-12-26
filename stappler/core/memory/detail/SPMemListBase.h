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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMLISTBASE_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMLISTBASE_H_

#include "SPMemStorageNode.h"

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

template <size_t ArchSize>
struct SP_PUBLIC ListNodeFlag;

template <>
struct SP_PUBLIC ListNodeFlag<size_t(4)> {
	static constexpr uintptr_t MaxSize = maxOf<uintptr_t>();
	static constexpr uintptr_t MaxIndex = (1ULL << uintptr_t(sizeof(uintptr_t) * 8 - 1)) - 1;

	uintptr_t prealloc : 1;

	// Index of preallocated block
	// on 32-bit systems overflow is close, so, use tree preallocation optimization with care
	uintptr_t index	   : sizeof(uintptr_t) * 8 - 1;

	// for root node - here we store capacity
	// for preallocated head node - block size in bytes
	// for other preallocaed nodes - should be 0
	// for single node - block size in bytes
	uintptr_t size;
};

template <>
struct SP_PUBLIC ListNodeFlag<size_t(8)> {
	static constexpr uintptr_t MaxSize = (1ULL << uintptr_t((sizeof(uintptr_t) / 2) * 8)) - 1;
	static constexpr uintptr_t MaxIndex = (1ULL << uintptr_t((sizeof(uintptr_t) / 2) * 8 - 1)) - 1;

	uintptr_t prealloc : 1;

	// Index of preallocated block
	// on 32-bit systems overflow is close, so, use tree preallocation optimization with care
	uintptr_t index	   : (sizeof(uintptr_t) / 2) * 8 - 1;

	// for root node - here we store capacity
	// for preallocated head node - block size in bytes
	// for other preallocaed nodes - should be 0
	// for single node - block size in bytes
	uintptr_t size	   : (sizeof(uintptr_t) / 2) * 8;
};

template <typename T>
struct ForwardListNode : AllocPool {
	using Flag = detail::ListNodeFlag<sizeof(uintptr_t)>;

	static constexpr uintptr_t MaxSize = Flag::MaxSize;
	static constexpr uintptr_t MaxIndex = Flag::MaxIndex;

	static ForwardListNode *insert(ForwardListNode **pos, ForwardListNode *node) {
		node->next = *pos;
		*pos = node;
		return node;
	}

	static ForwardListNode *erase(ForwardListNode **pos) {
		auto node = *pos;
		*pos = node->next;
		return node;
	}

	static ForwardListNode *pop(ForwardListNode **pos) {
		auto node = *pos;
		*pos = node->next;
		node->next = nullptr;
		return node;
	}

	static ForwardListNode *copyValue(const Allocator<T> &alloc, ForwardListNode *dest,
			ForwardListNode *target) {
		alloc.construct(dest->value.addr(), target->value.ref());
		return dest;
	}

	static ForwardListNode *destroyValue(const Allocator<T> &alloc, ForwardListNode *node) {
		alloc.destroy(node->value.ptr());
		return node;
	}

	ForwardListNode *next = nullptr;
	Flag flag;
	detail::Storage<T> value;

	ForwardListNode() noexcept : flag(Flag{0, 0, 0}) { }

	inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	inline bool isPrealloc() const { return flag.prealloc != 0; }

	inline void setSize(uintptr_t s) { flag.size = s; }
	inline uintptr_t getSize() const { return flag.size; }

	inline void setIndex(uintptr_t s) { flag.index = s; }
	inline uintptr_t getIndex() const { return flag.index; }

	// Where to store next node when this node is in preserved list
	auto getNextStorage() const { return static_cast<ForwardListNode *>(next); }
	auto getNextStoragePtr() { return reinterpret_cast<ForwardListNode **>(&next); }
};

template <typename T>
struct ForwardListIterator {
	using iterator_category = std::forward_iterator_tag;

	using node_type = ForwardListNode<T>;
	using reference = T &;
	using pointer = T *;

	node_type *__target = nullptr;
	node_type **__next = nullptr;

	ForwardListIterator() noexcept = default;

	explicit ForwardListIterator(node_type **next) : __target(nullptr), __next(next) { }
	explicit ForwardListIterator(node_type *target) : __target(target), __next(&__target->next) { }

	ForwardListIterator(const ForwardListIterator &other) noexcept = default;

	ForwardListIterator &operator=(const ForwardListIterator &other) noexcept = default;

	bool operator==(const ForwardListIterator &other) const = default;
	bool operator!=(const ForwardListIterator &other) const = default;

	ForwardListIterator &operator++() {
		__target = *__next;
		if (__target) {
			__next = &__target->next;
		} else {
			__next = nullptr;
		}
		return *this;
	}
	ForwardListIterator operator++(int) {
		auto tmp = *this;
		__target = *__next;
		if (__target) {
			__next = &__target->next;
		} else {
			__next = nullptr;
		}
		return tmp;
	}

	reference operator*() const { return __target->value.ref(); }
	pointer operator->() const { return __target->value.ptr(); }
};

template <typename T>
struct ForwardListConstIterator {
	using iterator_category = std::forward_iterator_tag;

	using node_type = ForwardListNode<T>;
	using reference = const T &;
	using pointer = const T *;

	const node_type *__target = nullptr;
	node_type *const *__next = nullptr;

	ForwardListConstIterator() noexcept = default;

	explicit ForwardListConstIterator(node_type **next) : __target(nullptr), __next(next) { }
	explicit ForwardListConstIterator(node_type *target)
	: __target(target), __next(&__target->next) { }

	ForwardListConstIterator(const ForwardListIterator<T> &other)
	: __target(other.__target), __next(other.__next) { }

	ForwardListConstIterator(const ForwardListConstIterator &other) noexcept = default;

	ForwardListConstIterator &operator=(const ForwardListConstIterator &other) noexcept = default;

	bool operator==(const ForwardListConstIterator &other) const = default;
	bool operator!=(const ForwardListConstIterator &other) const = default;

	ForwardListConstIterator &operator++() {
		__target = *__next;
		if (__target) {
			__next = &__target->next;
		} else {
			__next = nullptr;
		}
		return *this;
	}
	ForwardListConstIterator operator++(int) {
		auto tmp = *this;
		__target = *__next;
		if (__target) {
			__next = &__target->next;
		} else {
			__next = nullptr;
		}
		return tmp;
	}

	reference operator*() const { return __target->value.ref(); }
	pointer operator->() const { return __target->value.ptr(); }
};

template <typename Node>
class SP_PUBLIC list_base : AllocPool {
public:
	using node_type = Node;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using node_allocator_type = detail::Allocator<Node>;
	using allocator_helper = detail::NodeBlockAllocatorHelper<node_type>;

	list_base(const node_allocator_type &alloc = node_allocator_type()) noexcept : _alloc(alloc) { }

	list_base(const list_base &other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: _alloc(alloc) {
		__clone(other);
	}

	list_base(list_base &&other, const node_allocator_type &alloc = node_allocator_type()) noexcept
	: _alloc(alloc) {
		__move(sp::move(other));
	}

	list_base &operator=(const list_base &other) noexcept {
		__clone(other);
		return *this;
	}

	list_base &operator=(list_base &&other) noexcept {
		__move(sp::move(other));
		return *this;
	}

	~list_base() noexcept {
		clear();
		allocator_helper::template release_blocks<false>(get_allocator(), &_storage, _blockIndex);
	}

	auto get_allocator() const { return _alloc; }

	Node **front_location() { return &_front; }
	Node **back_location() { return _tail ? &(_tail->next) : &_front; }
	Node *front() const { return _front; }
	Node *back() const { return _tail; }

	void clear() {
		auto node = _front;
		while (node) {
			auto next = node->next;
			destroyNode(node);
			node = next;
		}

		_size = 0;
		_front = nullptr;
		_tail = nullptr;
	}

	void shrink_to_fit() noexcept {
		auto nFreed = allocator_helper::template release_blocks<true>(get_allocator(), &_storage,
				_blockIndex);
		_extraCapacity -= nFreed;
	}

	size_t capacity() const noexcept { return _size + _extraCapacity; }

	size_t size() const noexcept { return _size; }

	void set_memory_persistent(bool value) noexcept { _alloc.set(node_allocator_type::FirstFlag); }

	bool memory_persistent() const noexcept { return _alloc.test(node_allocator_type::FirstFlag); }

	void insert(Node **target, Node *node) {
		node->next = *target;
		*target = node;

		if (!_tail || target == &_tail->next) {
			_tail = node;
		}

		++_size;
	}

	void insert_front(Node *node) { insert(&_front, node); }

	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	Node *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(&_front, count, cb);
	}

	Node *allocate_node() {
		size_t s = 0;
		auto node = _alloc.__allocate(1, s);
		_alloc.construct(node);
		node->setPrealloc(false);
		node->setSize(s);
		return node;
	}

	template <typename ConstructCallback>
	Node *expand(Node **insertTarget, size_t count, const ConstructCallback &cb) {
		Node *tail = *insertTarget;

		while (_storage && count > 0) {
			auto node = _storage;
			_storage = node->next;

			cb(_alloc, node);

			tail = Node::insert(insertTarget, node);

			++_size;
			--count;
		}

		// Allocate block only if required size is larger then BlockThreshold
		// In other case, block allocation is ineffective
		if (count > 1 && _blockIndex < Node::MaxIndex
				&& count * sizeof(Node) > config::BlockThreshold) {
			Node *tail = nullptr;
			size_type n = count;

			auto preallocIdx = ++_blockIndex;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(_alloc, node);
					++_size;
					tail = Node::insert(insertTarget, node);
					insertTarget = &tail->next;
				} else {
					++_extraCapacity;
					Node::insert(&_storage, node);
				}
				return true;
			}, _alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			tail = allocate_node();
			Node::insert(insertTarget, tail);
		} else {
			size_type n = count;
			// but we still can do batch allocation
			allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(_alloc, node);
					++_size;
					tail = Node::insert(insertTarget, node);
					insertTarget = &tail->next;
				} else {
					++_extraCapacity;
					Node::insert(&_storage, node);
				}
				return true;
			}, _alloc, n, nullptr);
		}

		if (!_tail || insertTarget == &_tail->next) {
			_tail = tail;
		}
		return tail;
	}

	Node *erase_after(Node **target) {
		auto node = Node::erase(target);
		auto ret = node->next;
		Node::destroyValue(_alloc, node);
		--_size;
		destroyNode(node);
		return ret;
	}

protected:
	void destroyNode(Node *n) {
		Node::destroyValue(_alloc, n);
		if (!_storage || n->isPrealloc() || memory_persistent()) {
			Node::insert(&_storage, n);
			++_extraCapacity;
		} else {
			// deallocate node
			_alloc.destroy(n);
			_alloc.__deallocate(n, 1, n->getSize());
		}
	}

	void __clone(const list_base &other) {
		auto preallocTmp = memory_persistent();
		set_memory_persistent(true);
		clear();
		set_memory_persistent(preallocTmp);

		Node *source = other._front;

		__expand(other.size(), [&](Node *dest) SPINLINE {
			Node::copyValue(_alloc, dest, source);
			source = source->next;
		});
	}

	void __move(list_base &&other) {
		if (other.get_allocator() != get_allocator()) {

			set_memory_persistent(false);
			clear();
			shrink_to_fit();

			set_memory_persistent(other.memory_persistent());
			_size = other._size;
			_blockIndex = other._blockIndex;
			_extraCapacity = other._extraCapacity;
			_front = other._front;
			_tail = other._tail;
			_storage = other._storage;

			other._size = 0;
			other._blockIndex = 0;
			other._extraCapacity = 0;
			other._front = nullptr;
			other._tail = nullptr;
			other._storage = nullptr;
		} else {
			__clone(other);
		}
	}

	node_allocator_type _alloc;
	size_t _size = 0;
	uint32_t _blockIndex = 0;
	uint32_t _extraCapacity = 0;

	node_type *_front = nullptr;
	node_type *_tail = nullptr;
	node_type *_storage = nullptr;
};

} // namespace stappler::memory::detail

#endif // STAPPLER_CORE_MEMORY_DETAIL_SPMEMLISTBASE_H_
