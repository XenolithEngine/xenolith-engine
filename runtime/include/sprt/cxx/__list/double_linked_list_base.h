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

#include <sprt/cxx/__list/common_list_base.h>
#include <sprt/cxx/detail/aligned_storage.h>
#include <sprt/cxx/iterator>

namespace sprt::detail {

template <typename Allocator>
struct ListNodeBase : Allocator::base_class {
	using Flag = ListNodeFlag<sizeof(uintptr_t)>;

	static constexpr uintptr_t MaxSize = Flag::MaxSize;
	static constexpr uintptr_t MaxIndex = Flag::MaxIndex;

	ListNodeBase *prev = nullptr;
	ListNodeBase *next = nullptr;
	Flag flag;

	constexpr ListNodeBase() noexcept : flag(Flag{0, 0, 0}) { }

	constexpr void reset() {
		prev = this;
		next = this;
	}

	constexpr inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	constexpr inline bool isPrealloc() const { return flag.prealloc != 0; }

	constexpr inline void setSize(uintptr_t s) { flag.size = s; }
	constexpr inline uintptr_t getSize() const { return flag.size; }

	constexpr inline void setIndex(uintptr_t s) { flag.index = s; }
	constexpr inline uintptr_t getIndex() const { return flag.index; }
};

template <typename T, typename Allocator>
struct DoubleLinkedListNode : ListNodeBase<Allocator> {
	constexpr static DoubleLinkedListNode *insert_before(ListNodeBase<Allocator> *target,
			DoubleLinkedListNode *node) {
		node->next = target;
		node->prev = target->prev;

		target->prev->next = node;
		target->prev = node;
		return node;
	}

	constexpr static ListNodeBase<Allocator> *erase(ListNodeBase<Allocator> *target) {
		target->next->prev = target->prev;
		target->prev->next = target->next;
		return target;
	}

	// NodeAllocator is any allocator following the sprt rebind protocol (including
	// detail::AllocatorStd, whose template argument is the wrapped allocator rather
	// than the node type, so a template-template parameter cannot express it).
	template <typename NodeAllocator>
	constexpr static DoubleLinkedListNode *copyValue(const NodeAllocator &alloc,
			DoubleLinkedListNode *dest, DoubleLinkedListNode *target) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	// Move-relocating variant of copyValue for move-only element types (the
	// unequal-allocator move path cannot steal nodes and cannot copy either).
	template <typename NodeAllocator>
	constexpr static DoubleLinkedListNode *moveValue(const NodeAllocator &alloc,
			DoubleLinkedListNode *dest, DoubleLinkedListNode *target) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), sprt::move_unsafe(target->value.ref()));
		return dest;
	}

	template <typename NodeAllocator>
	constexpr static DoubleLinkedListNode *destroyValue(const NodeAllocator &alloc,
			DoubleLinkedListNode *node) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

		node->value.destroy(value_allocator(alloc));
		return node;
	}

	// Where to store next node when this node is in preserved list
	constexpr DoubleLinkedListNode *getNextStorage() const {
		return static_cast<DoubleLinkedListNode *>(this->next);
	}
	constexpr void setNextStorage(DoubleLinkedListNode *ptr) { this->next = ptr; }

	aligned_storage<T> value;
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

	constexpr DoubleLinkedListIterator() noexcept = default;

	constexpr explicit DoubleLinkedListIterator(node_type *target) : __target(target) { }

	constexpr DoubleLinkedListIterator(const DoubleLinkedListIterator &other) noexcept = default;

	constexpr DoubleLinkedListIterator &operator=(
			const DoubleLinkedListIterator &other) noexcept = default;

	constexpr bool operator==(const DoubleLinkedListIterator &other) const = default;
	constexpr bool operator!=(const DoubleLinkedListIterator &other) const = default;

	constexpr DoubleLinkedListIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	constexpr DoubleLinkedListIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
		return tmp;
	}

	constexpr DoubleLinkedListIterator &operator--() {
		__target = __target->prev;
		return *this;
	}
	constexpr DoubleLinkedListIterator operator--(int) {
		auto tmp = *this;
		__target = __target->prev;
		return tmp;
	}
	constexpr reference operator*() const {
		return static_cast<list_node_type *>(__target)->value.ref();
	}
	constexpr pointer operator->() const {
		return static_cast<list_node_type *>(__target)->value.ptr();
	}
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

	constexpr DoubleLinkedListConstIterator() noexcept = default;

	constexpr explicit DoubleLinkedListConstIterator(const node_type *target) : __target(target) { }

	constexpr DoubleLinkedListConstIterator(const DoubleLinkedListIterator<T, Allocator> &other)
	: __target(other.__target) { }

	constexpr DoubleLinkedListConstIterator(
			const DoubleLinkedListConstIterator &other) noexcept = default;

	constexpr DoubleLinkedListConstIterator &operator=(
			const DoubleLinkedListConstIterator &other) noexcept = default;

	constexpr bool operator==(const DoubleLinkedListConstIterator &other) const = default;
	constexpr bool operator!=(const DoubleLinkedListConstIterator &other) const = default;

	constexpr DoubleLinkedListConstIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	constexpr DoubleLinkedListConstIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
		return tmp;
	}

	constexpr DoubleLinkedListConstIterator &operator--() {
		__target = __target->prev;
		return *this;
	}
	constexpr DoubleLinkedListConstIterator operator--(int) {
		auto tmp = *this;
		__target = __target->prev;
		return tmp;
	}

	constexpr reference operator*() const {
		return static_cast<const list_node_type *>(__target)->value.ref();
	}
	constexpr pointer operator->() const {
		return static_cast<const list_node_type *>(__target)->value.ptr();
	}
};


template <typename T, typename Allocator>
class SPRT_API double_linked_list_base
: public common_list_base<ListNodeBase<Allocator>, DoubleLinkedListNode<T, Allocator>, Allocator> {
public:
	using base_type = common_list_base<ListNodeBase<Allocator>, DoubleLinkedListNode<T, Allocator>,
			Allocator>;
	using node_allocator_type = base_type::node_allocator_type;
	using node_type = base_type::node_type;
	using size_type = base_type::size_type;
	using basic_node_type = base_type::basic_node_type;
	using allocator_helper = base_type::allocator_helper;

	constexpr double_linked_list_base(
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: base_type(alloc) { }

	constexpr double_linked_list_base(const double_linked_list_base &other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: double_linked_list_base(alloc) {
		__clone(other);
	}

	constexpr double_linked_list_base(double_linked_list_base &&other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: double_linked_list_base(alloc) {
		__move(sprt::move(other));
	}

	constexpr double_linked_list_base &operator=(const double_linked_list_base &other) noexcept {
		__clone(other);
		return *this;
	}

	constexpr double_linked_list_base &operator=(double_linked_list_base &&other) noexcept {
		__move(sprt::move(other));
		return *this;
	}

	constexpr void swap(double_linked_list_base &other) noexcept {
		auto doSwap = [&] {
			sprt::swap(this->_alloc, other._alloc);
			sprt::swap(this->_size, other._size);
			sprt::swap(this->_root, other._root);

			// An empty side's links still point at the OTHER object's sentinel
			// after the value swap; re-close such circles instead of chasing them.
			if (this->_root.next == sprt::addressof(other._root)) {
				this->_root.next = sprt::addressof(this->_root);
				this->_root.prev = sprt::addressof(this->_root);
			} else {
				this->_root.next->prev = sprt::addressof(this->_root);
				this->_root.prev->next = sprt::addressof(this->_root);
			}

			if (other._root.next == sprt::addressof(this->_root)) {
				other._root.next = sprt::addressof(other._root);
				other._root.prev = sprt::addressof(other._root);
			} else {
				other._root.next->prev = sprt::addressof(other._root);
				other._root.prev->next = sprt::addressof(other._root);
			}

			sprt::swap(this->_storage, other._storage);
		};

		if constexpr (sprt::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			doSwap();
		} else if (other._alloc == this->_alloc) {
			doSwap();
		}
	}

	constexpr node_type **front_location() {
		return reinterpret_cast<node_type **>(sprt::addressof(this->_root).next);
	}
	constexpr node_type **back_location() {
		return reinterpret_cast<node_type **>(sprt::addressof(this->_root).prev);
	}
	constexpr node_type *back() const { return static_cast<node_type *>(this->_root.prev); }

	constexpr void insert_before(basic_node_type *target, node_type *node) {
		node_type::insert_before(target, node);
		++this->_size;
	}

	constexpr void transfer_node(basic_node_type *target, double_linked_list_base &from,
			basic_node_type *node) {
		// Splicing a node to just before itself (pos == the node, in the same list) is a
		// no-op; relinking it would form a self-loop and corrupt the ring.
		if (node == target) {
			return;
		}
		node_type::erase(node);
		--from._size;
		node_type::insert_before(target, static_cast<node_type *>(node));
		++this->_size;
	}

	constexpr void insert_front(node_type *node) { insert_before(this->_root.next, node); }
	constexpr void insert_back(node_type *node) {
		insert_before(sprt::addressof(this->_root), node);
	}

	template <typename NodeLess>
	constexpr void sort_nodes(NodeLess __less) {
		if (this->_size < 2) {
			return;
		}
		basic_node_type *__head = this->_root.next;
		this->_root.prev->next = nullptr; // break the ring into a null-terminated chain
		__head = __merge_sort_run(__head, this->_size, __less);
		basic_node_type *__prev = sprt::addressof(this->_root);
		for (basic_node_type *__n = __head; __n; __n = __n->next) {
			__n->prev = __prev;
			__prev->next = __n;
			__prev = __n;
		}
		__prev->next = sprt::addressof(this->_root);
		this->_root.prev = __prev;
	}

private:
	template <typename NodeLess>
	static constexpr basic_node_type *__merge_runs(basic_node_type *__a, basic_node_type *__b,
			NodeLess __less) {
		basic_node_type *__head = nullptr;
		basic_node_type **__tail = &__head;
		while (__a && __b) {
			// Take from the left run unless the right strictly precedes it — keeps the
			// sort stable (equal elements retain their original relative order).
			if (__less(__b, __a)) {
				*__tail = __b;
				__b = __b->next;
			} else {
				*__tail = __a;
				__a = __a->next;
			}
			__tail = &(*__tail)->next;
		}
		*__tail = __a ? __a : __b;
		return __head;
	}

	template <typename NodeLess>
	static constexpr basic_node_type *__merge_sort_run(basic_node_type *__head, size_type __count,
			NodeLess __less) {
		if (__count < 2) {
			return __head;
		}
		size_type __half = __count / 2;
		basic_node_type *__mid = __head;
		for (size_type __i = 1; __i < __half; ++__i) { __mid = __mid->next; }
		basic_node_type *__right = __mid->next;
		__mid->next = nullptr; // split into [head, half) and [right, count-half)
		basic_node_type *__l = __merge_sort_run(__head, __half, __less);
		basic_node_type *__r = __merge_sort_run(__right, __count - __half, __less);
		return __merge_runs(__l, __r, __less);
	}

public:
	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	constexpr basic_node_type *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(this->_root.next, count, cb);
	}

	template <typename ConstructCallback>
	constexpr basic_node_type *expand(basic_node_type *insertTarget, size_t count,
			const ConstructCallback &cb) {
		basic_node_type *tail = insertTarget;

		// First - reuse nodes, that already free
		while (this->_storage && count > 0) {
			auto node = this->_storage;
			this->_storage = node->getNextStorage();

			cb(this->_alloc, node);

			tail = node_type::insert_before(insertTarget, node);

			++this->_size;
			--count;
		}

		// Allocate block only if required size is larger then BlockThreshold
		// In other cases, block allocation is ineffective
		if (count > 1 && this->_root.flag.index < node_type::MaxIndex
				&& count * sizeof(node_type) > memory::config::BlockThreshold) {
			node_type *tail = nullptr;
			size_type n = count;

			auto preallocIdx = ++this->_root.flag.index;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(this->_alloc, node);
					++this->_size;
					tail = node_type::insert_before(insertTarget, node);
					insertTarget = static_cast<node_type *>(tail->next);
				} else {
					// unused nodes moved into free nodes storage
					++this->_root.flag.size;
					node->setNextStorage(this->_storage);
					this->_storage = node;
				}
				return true;
			}, this->_alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			auto node = this->allocate_node();
			cb(this->_alloc, node);
			node_type::insert_before(insertTarget, node);
			tail = node;
			++this->_size;
		} else {
			if constexpr (is_same_v<Allocator, detail::AllocatorPool<T>>) {
				// For mempool-based containers, we still can do batch allocation
				size_type n = count;
				allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
					if (idx < count) {
						cb(this->_alloc, node);
						++this->_size;
						tail = node_type::insert_before(insertTarget, node);
						insertTarget = static_cast<node_type *>(tail->next);
					} else {
						// unused nodes moved into free nodes storage
						++this->_root.flag.size;
						node->setNextStorage(this->_storage);
						this->_storage = node;
					}
					return true;
				}, this->_alloc, n, nullptr);
			} else {
				// For others - we should allocate nodes one by one
				while (count-- > 0) {
					auto node = this->allocate_node();
					cb(this->_alloc, node);
					node_type::insert_before(insertTarget, node);
					tail = node;
					++this->_size;
				}
			}
		}
		return tail;
	}

	constexpr basic_node_type *erase(basic_node_type *target) {
		auto node = static_cast<node_type *>(node_type::erase(target));
		auto ret = node->next;
		node->prev = nullptr;
		node->next = nullptr;
		node_type::destroyValue(this->_alloc, node);
		--this->_size;
		this->destroyNode(node);
		return ret;
	}

protected:
	constexpr void __clone(const double_linked_list_base &other) {
		auto preallocTmp = this->memory_persistent();
		this->memory_persistent(true);
		this->clear();
		this->memory_persistent(preallocTmp);

		node_type *source = static_cast<node_type *>(other._root.next);

		expand_front(other.size(),
				[&](const node_allocator_type &nalloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::copyValue(this->_alloc, dest, source);
			source = static_cast<node_type *>(source->next);
		});
	}

	constexpr void __move(double_linked_list_base &&other) {
		// Same allocator (always true for stateless/always-equal allocators): steal storage.
		if (other.get_allocator() == this->get_allocator()) {
			this->clear_deallocate();

			this->memory_persistent(other.memory_persistent());
			this->_size = other._size;
			this->_root = other._root;
			this->_storage = other._storage;

			if (this->_root.next == sprt::addressof(other._root)) {
				// Empty source: the copied links still point at the SOURCE's
				// sentinel; re-close the circle on our own root (flags stay).
				this->_root.prev = sprt::addressof(this->_root);
				this->_root.next = sprt::addressof(this->_root);
			} else {
				this->_root.next->prev = sprt::addressof(this->_root);
				this->_root.prev->next = sprt::addressof(this->_root);
			}

			other._size = 0;
			other._root.reset();
			other._root.flag.index = 0;
			other._root.flag.size = 0;
			other._root.flag.prealloc = 0;
			other._storage = nullptr;
		} else if constexpr (!sprt::is_empty_v<node_allocator_type>) {
			// Different (stateful) allocators: cannot steal; elided for always-equal
			// allocators so a moved list of an incomplete element type never
			// instantiates a clone path. Copyable elements keep the historical copy;
			// move-only ones are move-relocated element-wise.
			if constexpr (sprt::is_copy_constructible_v<T>) {
				__clone(other);
			} else {
				__clone_move(other);
			}
		}
	}

public:
	// LWG 526-safe remove_if: matching nodes are UNLINKED during the pass and
	// destroyed only afterwards, so a predicate (or value) referring into the
	// list stays valid while the scan runs.
	template <typename NodePred>
	constexpr size_t remove_if_nodes(NodePred pred) {
		basic_node_type *sent = sprt::addressof(this->_root);
		node_type *deferred = nullptr; // stack of unlinked nodes, chained via next
		size_t removed = 0;
		auto *cur = this->_root.next;
		while (cur != sent) {
			auto *next = cur->next;
			if (pred(static_cast<node_type *>(cur))) {
				node_type::erase(cur); // unlink, relinking neighbours
				auto *n = static_cast<node_type *>(cur);
				n->setNextStorage(deferred);
				deferred = n;
				++removed;
				--this->_size;
			}
			cur = next;
		}
		while (deferred) {
			auto *next = deferred->getNextStorage();
			node_type::destroyValue(this->_alloc, deferred);
			this->destroyNode(deferred);
			deferred = next;
		}
		return removed;
	}

protected:
	constexpr void __clone_move(double_linked_list_base &other) {
		auto preallocTmp = this->memory_persistent();
		this->memory_persistent(true);
		this->clear();
		this->memory_persistent(preallocTmp);

		node_type *source = static_cast<node_type *>(other._root.next);

		expand_front(other.size(),
				[&](const node_allocator_type &nalloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::moveValue(this->_alloc, dest, source);
			source = static_cast<node_type *>(source->next);
		});
	}
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_DOUBLE_LINKED_LIST_BASE_H_
