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

#ifndef RUNTIME_INCLUDE_SPRT_CXX_MEMORY_FORWARD_LIST_BASE_H_
#define RUNTIME_INCLUDE_SPRT_CXX_MEMORY_FORWARD_LIST_BASE_H_

#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/memory/aligned_storage.h>
#include <sprt/cxx/memory/nodebase.h>
#include <sprt/cxx/iterator>

namespace sprt::memory {

template <typename Allocator>
struct ForwardListNodeBase : Allocator::base_class {
	using Flag = ListNodeFlag<sizeof(uintptr_t)>;

	static constexpr uintptr_t MaxSize = Flag::MaxSize;
	static constexpr uintptr_t MaxIndex = Flag::MaxIndex;

	ForwardListNodeBase *next = nullptr;
	Flag flag;

	ForwardListNodeBase() noexcept : flag(Flag{0, 0, 0}) { }

	inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	inline bool isPrealloc() const { return flag.prealloc != 0; }

	inline void setSize(uintptr_t s) { flag.size = s; }
	inline uintptr_t getSize() const { return flag.size; }

	inline void setIndex(uintptr_t s) { flag.index = s; }
	inline uintptr_t getIndex() const { return flag.index; }
};

template <typename T, typename Allocator>
struct ForwardListNode : ForwardListNodeBase<Allocator> {
	static ForwardListNode *insert(ForwardListNodeBase<Allocator> *pos, ForwardListNode *node) {
		node->next = pos->next;
		pos->next = node;
		return node;
	}

	static ForwardListNodeBase<Allocator> *erase_after(ForwardListNodeBase<Allocator> *pos) {
		auto node = pos->next;
		pos->next = node->next;
		return node;
	}

	template <template <typename U> typename NodeAllocator>
	static ForwardListNode *copyValue(const NodeAllocator<ForwardListNode> &alloc,
			ForwardListNode *dest, ForwardListNode *target) {
		using value_allocator = typename NodeAllocator<ForwardListNode>::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	template <template <typename U> typename NodeAllocator>
	static ForwardListNode *destroyValue(const NodeAllocator<ForwardListNode> &alloc,
			ForwardListNode *node) {
		using value_allocator = typename NodeAllocator<ForwardListNode>::template rebind<T>::other;

		node->value.destroy(value_allocator(alloc));
		return node;
	}

	aligned_storage<T> value;

	// Where to store next node when this node is in preserved list
	auto getNextStorage() const { return static_cast<ForwardListNode *>(this->next); }
	auto getNextStoragePtr() { return reinterpret_cast<ForwardListNode **>(&this->next); }
};

template <typename T, typename Allocator>
struct ForwardListIterator {
	using iterator_category = forward_iterator_tag;

	using node_type = ForwardListNodeBase<Allocator>;
	using list_node_type = ForwardListNode<T, Allocator>;
	using reference = T &;
	using pointer = T *;

	node_type *__target = nullptr;

	ForwardListIterator() noexcept = default;

	explicit ForwardListIterator(node_type *target) : __target(target) { }

	ForwardListIterator(const ForwardListIterator &other) noexcept = default;

	ForwardListIterator &operator=(const ForwardListIterator &other) noexcept = default;

	bool operator==(const ForwardListIterator &other) const = default;
	bool operator!=(const ForwardListIterator &other) const = default;

	ForwardListIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	ForwardListIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
		return tmp;
	}

	reference operator*() const { return static_cast<list_node_type *>(__target)->value.ref(); }
	pointer operator->() const { return static_cast<list_node_type *>(__target)->value.ptr(); }
};

template <typename T, typename Allocator>
struct ForwardListConstIterator {
	using iterator_category = forward_iterator_tag;

	using node_type = ForwardListNodeBase<Allocator>;
	using list_node_type = ForwardListNode<T, Allocator>;
	using reference = const T &;
	using pointer = const T *;

	const node_type *__target = nullptr;

	ForwardListConstIterator() noexcept = default;

	explicit ForwardListConstIterator(const node_type *target) : __target(target) { }

	ForwardListConstIterator(const ForwardListIterator<T, Allocator> &other)
	: __target(other.__target) { }

	ForwardListConstIterator(const ForwardListConstIterator &other) noexcept = default;

	ForwardListConstIterator &operator=(const ForwardListConstIterator &other) noexcept = default;

	bool operator==(const ForwardListConstIterator &other) const = default;
	bool operator!=(const ForwardListConstIterator &other) const = default;

	ForwardListConstIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	ForwardListConstIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
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
class SPRT_API forward_list_base : Allocator::base_class {
public:
	using basic_node_type = ForwardListNodeBase<Allocator>;
	using node_type = ForwardListNode<T, Allocator>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using node_allocator_type = typename Allocator::template rebind<node_type>::other;
	using allocator_helper = NodeBlockAllocatorHelper<node_type, node_allocator_type>;

	forward_list_base(const node_allocator_type &alloc = node_allocator_type()) noexcept
	: _alloc(alloc) {
		_root.next = &_root;
	}

	forward_list_base(const forward_list_base &other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: forward_list_base(alloc) {
		__clone(other);
	}

	forward_list_base(forward_list_base &&other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: forward_list_base(alloc) {
		__move(sprt::move(other));
	}

	forward_list_base &operator=(const forward_list_base &other) noexcept {
		__clone(other);
		return *this;
	}

	forward_list_base &operator=(forward_list_base &&other) noexcept {
		__move(sprt::move(other));
		return *this;
	}

	~forward_list_base() noexcept { clear_deallocate(); }

	void swap(forward_list_base &other) noexcept {
		auto doSwap = [&] {
			sprt::swap(_alloc, other._alloc);
			sprt::swap(_size, other._size);
			sprt::swap(_root, other._root);

			// fix ring pointers
			auto n = &_root;
			while (n->next != &other._root) { n = n->next; }
			n->next = &_root;

			n = &other._root;
			while (n->next != &_root) { n = n->next; }
			n->next = &other._root;

			sprt::swap(_storage, other._storage);
		};

		if constexpr (sprt::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			doSwap();
		} else if (other._alloc == _alloc) {
			doSwap();
		}
	}

	auto get_allocator() const { return _alloc; }

	basic_node_type *front() { return _root.next; }
	basic_node_type *end() { return &_root; }

	const basic_node_type *front() const { return _root.next; }
	const basic_node_type *end() const { return &_root; }

	void clear() {
		auto node = _root.next;
		while (node != &_root) {
			auto next = node->next;
			destroyNode(static_cast<node_type *>(node));
			node = next;
		}

		_size = 0;
		_root.next = &_root;
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
		memory_persistent(tmp);
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

	size_t capacity() const noexcept { return _size + _root.flag.size; }
	size_type max_size() const noexcept { return node_type::MaxSize; }
	size_t size() const noexcept { return _size; }

	void memory_persistent(bool value) noexcept { _root.flag.prealloc = value ? 1 : 0; }

	bool memory_persistent() const noexcept { return _root.flag.prealloc != 0; }

	void insert(basic_node_type *target, node_type *node) {
		node_type::insert(target, node);
		++_size;
	}

	void insert_front(node_type *node) { insert(&_root, node); }

	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	basic_node_type *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(&_root, count, cb);
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

		while (_storage && count > 0) {
			auto node = _storage;
			_storage = node->getNextStorage();

			cb(_alloc, node);

			tail = node_type::insert(tail, node);

			++_size;
			--count;
		}

		// Allocate block only if required size is larger then BlockThreshold
		// In other case, block allocation is ineffective
		if (count > 1 && _root.flag.index < node_type::MaxIndex
				&& count * sizeof(node_type) > config::BlockThreshold) {
			size_type n = count;

			auto preallocIdx = ++_root.flag.index;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(_alloc, node);
					++_size;
					tail = node_type::insert(tail, node);
				} else {
					++_root.flag.size;
					*node->getNextStoragePtr() = _storage;
					_storage = node;
				}
				return true;
			}, _alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			auto node = allocate_node();
			cb(_alloc, node);
			++_size;
			tail = node_type::insert(tail, node);
		} else {
			if constexpr (is_same_v<Allocator, memory::AllocatorPool<T>>) {
				size_type n = count;
				// but we still can do batch allocation
				allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
					if (idx < count) {
						cb(_alloc, node);
						++_size;
						tail = node_type::insert(tail, node);
					} else {
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
					++_size;
					tail = node_type::insert(tail, node);
				}
			}
		}
		return tail;
	}

	basic_node_type *erase_after(basic_node_type *target) {
		auto node = node_type::erase_after(target);
		auto ret = node->next;
		node_type::destroyValue(_alloc, static_cast<node_type *>(node));
		--_size;
		destroyNode(static_cast<node_type *>(node));
		return ret;
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

	void __clone(const forward_list_base &other) {
		auto preallocTmp = memory_persistent();
		memory_persistent(true);
		clear();
		memory_persistent(preallocTmp);

		basic_node_type *source = other._root.next;

		expand_front(other.size(), [&](auto alloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::copyValue(_alloc, dest, static_cast<node_type *>(source));
			source = source->next;
		});
	}

	void __move(forward_list_base &&other) {
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
	ForwardListNodeBase<Allocator> _root;
	node_type *_storage = nullptr;
};

} // namespace sprt::memory

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_FORWARD_LIST_BASE_H_
