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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_MEMORY_DOUBLE_LINKED_LIST_BASE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_MEMORY_DOUBLE_LINKED_LIST_BASE_H_

#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/memory/aligned_storage.h>
#include <sprt/cxx/memory/nodebase.h>
#include <sprt/cxx/iterator>

namespace sprt::memory {

template <typename Allocator>
struct ListNodeBase : Allocator::base_class {
	using Flag = ListNodeFlag<sizeof(uintptr_t)>;

	static constexpr uintptr_t MaxSize = Flag::MaxSize;
	static constexpr uintptr_t MaxIndex = Flag::MaxIndex;

	ListNodeBase *prev = nullptr;
	ListNodeBase *next = nullptr;
	Flag flag;

	ListNodeBase() noexcept : flag(Flag{0, 0, 0}) { }

	inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	inline bool isPrealloc() const { return flag.prealloc != 0; }

	inline void setSize(uintptr_t s) { flag.size = s; }
	inline uintptr_t getSize() const { return flag.size; }

	inline void setIndex(uintptr_t s) { flag.index = s; }
	inline uintptr_t getIndex() const { return flag.index; }
};

template <typename T, typename Allocator>
struct DoubleLinkedListNode : ListNodeBase<Allocator> {
	static DoubleLinkedListNode *insert_before(ListNodeBase<Allocator> *target,
			DoubleLinkedListNode *node) {
		node->next = target;
		node->prev = target->prev;

		target->prev->next = node;
		target->prev = node;
		return node;
	}

	static ListNodeBase<Allocator> *erase(ListNodeBase<Allocator> *target) {
		target->next->prev = target->prev;
		target->prev->next = target->next;
		return target;
	}

	template <template <typename U> typename NodeAllocator>
	static DoubleLinkedListNode *copyValue(const NodeAllocator<DoubleLinkedListNode> &alloc,
			DoubleLinkedListNode *dest, DoubleLinkedListNode *target) {
		using value_allocator =
				typename NodeAllocator<DoubleLinkedListNode>::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	template <template <typename U> typename NodeAllocator>
	static DoubleLinkedListNode *destroyValue(const NodeAllocator<DoubleLinkedListNode> &alloc,
			DoubleLinkedListNode *node) {
		using value_allocator =
				typename NodeAllocator<DoubleLinkedListNode>::template rebind<T>::other;

		node->value.destroy(value_allocator(alloc));
		return node;
	}

	aligned_storage<T> value;

	// Where to store next node when this node is in preserved list
	auto getNextStorage() const { return static_cast<DoubleLinkedListNode *>(this->next); }
	auto getNextStoragePtr() { return reinterpret_cast<DoubleLinkedListNode **>(&this->next); }
};

template <typename T, typename Allocator>
struct DoubleLinkedListIterator {
	using iterator_category = bidirectional_iterator_tag;

	using value_type = typename remove_cv<T>::type;
	using node_type = ListNodeBase<Allocator>;
	using list_node_type = DoubleLinkedListNode<T, Allocator>;
	using reference = T &;
	using pointer = T *;
	using difference_type = ptrdiff_t;

	node_type *__target = nullptr;

	DoubleLinkedListIterator() noexcept = default;

	explicit DoubleLinkedListIterator(node_type *target) : __target(target) { }

	DoubleLinkedListIterator(const DoubleLinkedListIterator &other) noexcept = default;

	DoubleLinkedListIterator &operator=(const DoubleLinkedListIterator &other) noexcept = default;

	bool operator==(const DoubleLinkedListIterator &other) const = default;
	bool operator!=(const DoubleLinkedListIterator &other) const = default;

	DoubleLinkedListIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	DoubleLinkedListIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
		return tmp;
	}

	DoubleLinkedListIterator &operator--() {
		__target = __target->prev;
		return *this;
	}
	DoubleLinkedListIterator operator--(int) {
		auto tmp = *this;
		__target = __target->prev;
		return tmp;
	}
	reference operator*() const { return static_cast<list_node_type *>(__target)->value.ref(); }
	pointer operator->() const { return static_cast<list_node_type *>(__target)->value.ptr(); }
};

template <typename T, typename Allocator>
struct DoubleLinkedListConstIterator {
	using iterator_category = bidirectional_iterator_tag;

	using value_type = typename remove_cv<T>::type;
	using node_type = ListNodeBase<Allocator>;
	using list_node_type = DoubleLinkedListNode<T, Allocator>;
	using reference = const T &;
	using pointer = const T *;
	using difference_type = ptrdiff_t;

	const node_type *__target = nullptr;

	DoubleLinkedListConstIterator() noexcept = default;

	explicit DoubleLinkedListConstIterator(const node_type *target) : __target(target) { }

	DoubleLinkedListConstIterator(const DoubleLinkedListIterator<T, Allocator> &other)
	: __target(other.__target) { }

	DoubleLinkedListConstIterator(const DoubleLinkedListConstIterator &other) noexcept = default;

	DoubleLinkedListConstIterator &operator=(
			const DoubleLinkedListConstIterator &other) noexcept = default;

	bool operator==(const DoubleLinkedListConstIterator &other) const = default;
	bool operator!=(const DoubleLinkedListConstIterator &other) const = default;

	DoubleLinkedListConstIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	DoubleLinkedListConstIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
		return tmp;
	}

	DoubleLinkedListConstIterator &operator--() {
		__target = __target->prev;
		return *this;
	}
	DoubleLinkedListConstIterator operator--(int) {
		auto tmp = *this;
		__target = __target->prev;
		return tmp;
	}

	reference operator*() const {
		return static_cast<const list_node_type *>(__target)->value.ref();
	}
	pointer operator->() const {
		return static_cast<const list_node_type *>(__target)->value.ptr();
	}
};


template <typename T, typename Allocator>
class SPRT_API double_linked_list_base : Allocator::base_class {
public:
	using basic_node_type = ListNodeBase<Allocator>;
	using node_type = DoubleLinkedListNode<T, Allocator>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using node_allocator_type = typename Allocator::template rebind<node_type>::other;
	using allocator_helper = NodeBlockAllocatorHelper<node_type, node_allocator_type>;

	double_linked_list_base(const node_allocator_type &alloc = node_allocator_type()) noexcept
	: _alloc(alloc) {
		_root.next = &_root;
		_root.prev = &_root;
	}

	double_linked_list_base(const double_linked_list_base &other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: double_linked_list_base(alloc) {
		__clone(other);
	}

	double_linked_list_base(double_linked_list_base &&other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: double_linked_list_base(alloc) {
		__move(sprt::move(other));
	}

	double_linked_list_base &operator=(const double_linked_list_base &other) noexcept {
		__clone(other);
		return *this;
	}

	double_linked_list_base &operator=(double_linked_list_base &&other) noexcept {
		__move(sprt::move(other));
		return *this;
	}

	~double_linked_list_base() noexcept {
		clear();
		allocator_helper::template release_blocks<false>(get_allocator(), &_storage,
				_root.flag.index);
	}

	void swap(double_linked_list_base &other) noexcept {
		auto doSwap = [&] {
			sprt::swap(_alloc, other._alloc);
			sprt::swap(_size, other._size);
			sprt::swap(_root, other._root);

			_root.next->prev = &_root;
			_root.prev->next = &_root;

			other._root.next->prev = &other._root;
			other._root.prev->next = &other._root;

			sprt::swap(_storage, other._storage);
		};

		if constexpr (sprt::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			doSwap();
		} else if (other._alloc == _alloc) {
			doSwap();
		}
	}

	auto get_allocator() const { return _alloc; }

	node_type **front_location() { return reinterpret_cast<node_type **>(&_root.next); }
	node_type **back_location() { return reinterpret_cast<node_type **>(&_root.prev); }
	node_type *front() const { return static_cast<node_type *>(_root.next); }
	node_type *back() const { return static_cast<node_type *>(_root.prev); }

	auto end() const { return &_root; }
	auto end() { return &_root; }

	void clear() {
		auto node = front();
		while (node != &_root) {
			auto next = static_cast<node_type *>(node->next);
			destroyNode(node);
			node = next;
		}

		_size = 0;
		_root.next = &_root;
		_root.prev = &_root;
	}

	void shrink_to_fit() noexcept {
		auto nFreed = allocator_helper::template release_blocks<true>(get_allocator(), &_storage,
				_root.flag.index);
		_root.flag.size -= nFreed;
	}

	void clear_deallocate() {
		auto tmp = memory_persistent();
		memory_persistent(false);
		clear();
		shrink_to_fit();
		set_memory_persistent(tmp);
	}

	void set_allocator(const Allocator &a) noexcept {
		if (a != _alloc) {
			clear_deallocate();
			_alloc = a;
		}
	}

	void set_allocator(Allocator &&a) noexcept {
		if (a != _alloc) {
			clear_deallocate();
			_alloc = sprt::move_unsafe(a);
		}
	}

	bool empty() const { return _root.next == &_root; }

	size_t capacity() const noexcept { return _size + _root.flag.size; }
	size_type max_size() const noexcept { return node_type::MaxSize; }
	size_t size() const noexcept { return _size; }

	void memory_persistent(bool value) noexcept { _root.flag.prealloc = value ? 1 : 0; }

	bool memory_persistent() const noexcept { return _root.flag.prealloc != 0; }

	void insert_before(basic_node_type *target, node_type *node) {
		node_type::insert_before(target, node);
		++_size;
	}

	void insert_front(node_type *node) { insert_before(_root.next, node); }
	void insert_back(node_type *node) { insert_before(&_root, node); }

	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	basic_node_type *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(_root.next, count, cb);
	}

	node_type *allocate_node() {
		size_t s = 0;
		auto node = _alloc.__allocate(1, s);
		_alloc.construct(node);
		node->setPrealloc(false);
		node->setSize(s);
		return node;
	}

	template <typename ConstructCallback>
	basic_node_type *expand(basic_node_type *insertTarget, size_t count,
			const ConstructCallback &cb) {
		basic_node_type *tail = insertTarget;

		// First - reuse nodes, that already free
		while (_storage && count > 0) {
			auto node = _storage;
			_storage = node->getNextStorage();

			cb(_alloc, node);

			tail = node_type::insert_before(insertTarget, node);

			++_size;
			--count;
		}

		// Allocate block only if required size is larger then BlockThreshold
		// In other cases, block allocation is ineffective
		if (count > 1 && _root.flag.index < node_type::MaxIndex
				&& count * sizeof(node_type) > config::BlockThreshold) {
			node_type *tail = nullptr;
			size_type n = count;

			auto preallocIdx = ++_root.flag.index;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(_alloc, node);
					++_size;
					tail = node_type::insert_before(insertTarget, node);
					insertTarget = static_cast<node_type *>(tail->next);
				} else {
					// unused nodes moved into free nodes storage
					++_root.flag.size;
					*node->getNextStoragePtr() = _storage;
					_storage = node;
				}
				return true;
			}, _alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			auto node = allocate_node();
			cb(_alloc, node);
			node_type::insert_before(insertTarget, node);
			tail = node;
			++_size;
		} else {
			if constexpr (is_same_v<Allocator, memory::AllocatorPool<T>>) {
				// For mempool-based containers, we still can do batch allocation
				size_type n = count;
				allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
					if (idx < count) {
						cb(_alloc, node);
						++_size;
						tail = node_type::insert_before(insertTarget, node);
						insertTarget = static_cast<node_type *>(tail->next);
					} else {
						// unused nodes moved into free nodes storage
						++_root.flag.size;
						*node->getNextStoragePtr() = _storage;
						_storage = node;
					}
					return true;
				}, _alloc, n, nullptr);
			} else {
				// For others - we should allocate nodes one by one
				while (count-- > 0) {
					auto node = allocate_node();
					cb(_alloc, node);
					node_type::insert_before(insertTarget, node);
					tail = node;
					++_size;
				}
			}
		}
		return tail;
	}

	node_type *erase(node_type *target) {
		auto node = static_cast<node_type *>(node_type::erase(target));
		auto ret = node->next;
		node->prev = nullptr;
		node->next = nullptr;
		node_type::destroyValue(_alloc, node);
		--_size;
		destroyNode(node);
		return static_cast<node_type *>(ret);
	}

protected:
	void destroyNode(node_type *n) {
		node_type::destroyValue(_alloc, n);
		if (!_storage || n->isPrealloc() || memory_persistent()) {
			*n->getNextStoragePtr() = _storage;
			_storage = n;
			++_root.flag.size;
		} else {
			// deallocate node
			_alloc.destroy(n);
			_alloc.__deallocate(n, 1, n->getSize());
		}
	}

	void __clone(const double_linked_list_base &other) {
		auto preallocTmp = memory_persistent();
		memory_persistent(true);
		clear();
		memory_persistent(preallocTmp);

		node_type *source = static_cast<node_type *>(other._root.next);

		expand_front(other.size(),
				[&](const node_allocator_type &nalloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::copyValue(_alloc, dest, source);
			source = static_cast<node_type *>(source->next);
		});
	}

	void __move(double_linked_list_base &&other) {
		if (other.get_allocator() != get_allocator()) {
			memory_persistent(false);
			clear();
			shrink_to_fit();

			memory_persistent(other.memory_persistent());
			_size = other._size;
			_root = other._root;
			_storage = other._storage;

			other._size = 0;
			other._root.next = &other._root;
			other._root.prev = &other._root;
			other._root.flag.index = 0;
			other._root.flag.size = 0;
			other._root.flag.prealloc = 0;
			other._storage = nullptr;
		} else {
			__clone(other);
		}
	}

	[[no_unique_address]]
	node_allocator_type _alloc;
	size_type _size = 0;

	ListNodeBase<Allocator> _root;
	node_type *_storage = nullptr;
};

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_DOUBLE_LINKED_LIST_BASE_H_
