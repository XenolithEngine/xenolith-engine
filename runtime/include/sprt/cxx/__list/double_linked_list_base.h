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

	template <template <typename U> typename NodeAllocator>
	constexpr static DoubleLinkedListNode *copyValue(
			const NodeAllocator<DoubleLinkedListNode> &alloc, DoubleLinkedListNode *dest,
			DoubleLinkedListNode *target) {
		using value_allocator =
				typename NodeAllocator<DoubleLinkedListNode>::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	template <template <typename U> typename NodeAllocator>
	constexpr static DoubleLinkedListNode *destroyValue(
			const NodeAllocator<DoubleLinkedListNode> &alloc, DoubleLinkedListNode *node) {
		using value_allocator =
				typename NodeAllocator<DoubleLinkedListNode>::template rebind<T>::other;

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

			this->_root.next->prev = &this->_root;
			this->_root.prev->next = &this->_root;

			other._root.next->prev = &other._root;
			other._root.prev->next = &other._root;

			sprt::swap(this->_storage, other._storage);
		};

		if constexpr (sprt::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			doSwap();
		} else if (other._alloc == this->_alloc) {
			doSwap();
		}
	}

	constexpr node_type **front_location() {
		return reinterpret_cast<node_type **>(&this->_root.next);
	}
	constexpr node_type **back_location() {
		return reinterpret_cast<node_type **>(&this->_root.prev);
	}
	constexpr node_type *back() const { return static_cast<node_type *>(this->_root.prev); }

	constexpr void insert_before(basic_node_type *target, node_type *node) {
		node_type::insert_before(target, node);
		++this->_size;
	}

	constexpr void insert_front(node_type *node) { insert_before(this->_root.next, node); }
	constexpr void insert_back(node_type *node) { insert_before(&this->_root, node); }

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
		if (other.get_allocator() != this->get_allocator()) {
			this->clear_deallocate();

			this->memory_persistent(other.memory_persistent());
			this->_size = other._size;
			this->_root = other._root;
			this->_storage = other._storage;

			this->_root.next->prev = &this->_root;
			this->_root.prev->next = &this->_root;

			other._size = 0;
			other._root.reset();
			other._root.flag.index = 0;
			other._root.flag.size = 0;
			other._root.flag.prealloc = 0;
			other._storage = nullptr;
		} else {
			__clone(other);
		}
	}
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_DOUBLE_LINKED_LIST_BASE_H_
