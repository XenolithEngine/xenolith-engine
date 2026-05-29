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

#include <sprt/cxx/__list/common_list_base.h>
#include <sprt/cxx/detail/aligned_storage.h>
#include <sprt/cxx/iterator>

namespace sprt::detail {

template <typename Allocator>
struct ForwardListNodeBase {
	using Flag = ListNodeFlag<sizeof(uintptr_t)>;

	static constexpr uintptr_t MaxSize = Flag::MaxSize;
	static constexpr uintptr_t MaxIndex = Flag::MaxIndex;

	ForwardListNodeBase *next = nullptr;
	Flag flag;

	constexpr ForwardListNodeBase() noexcept : flag(Flag{0, 0, 0}) { }

	// make default zero-ring
	constexpr void reset() { next = this; }

	constexpr inline void setPrealloc(bool v) { flag.prealloc = v ? 1 : 0; }
	constexpr inline bool isPrealloc() const { return flag.prealloc != 0; }

	constexpr inline void setSize(uintptr_t s) { flag.size = s; }
	constexpr inline uintptr_t getSize() const { return flag.size; }

	constexpr inline void setIndex(uintptr_t s) { flag.index = s; }
	constexpr inline uintptr_t getIndex() const { return flag.index; }
};

template <typename T, typename Allocator>
struct ForwardListNode : ForwardListNodeBase<Allocator> {
	constexpr static ForwardListNode *insert(ForwardListNodeBase<Allocator> *pos,
			ForwardListNode *node) {
		node->next = pos->next;
		pos->next = node;
		return node;
	}

	constexpr static ForwardListNodeBase<Allocator> *erase_after(
			ForwardListNodeBase<Allocator> *pos) {
		auto node = pos->next;
		pos->next = node->next;
		return node;
	}

	template <template <typename U> typename NodeAllocator>
	constexpr static ForwardListNode *copyValue(const NodeAllocator<ForwardListNode> &alloc,
			ForwardListNode *dest, ForwardListNode *target) {
		using value_allocator = typename NodeAllocator<ForwardListNode>::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	template <template <typename U> typename NodeAllocator>
	constexpr static ForwardListNode *destroyValue(const NodeAllocator<ForwardListNode> &alloc,
			ForwardListNode *node) {
		using value_allocator = typename NodeAllocator<ForwardListNode>::template rebind<T>::other;

		node->value.destroy(value_allocator(alloc));
		return node;
	}

	// Where to store next node when this node is in preserved list
	constexpr ForwardListNode *getNextStorage() const {
		return static_cast<ForwardListNode *>(this->next);
	}
	constexpr void setNextStorage(ForwardListNode *ptr) { this->next = ptr; }

	aligned_storage<T> value;
};

template <typename T, typename Allocator>
struct ForwardListIterator {
	using iterator_category = forward_iterator_tag;

	using node_type = ForwardListNodeBase<Allocator>;
	using list_node_type = ForwardListNode<T, Allocator>;
	using reference = T &;
	using pointer = T *;

	node_type *__target = nullptr;

	constexpr ForwardListIterator() noexcept = default;

	constexpr explicit ForwardListIterator(node_type *target) : __target(target) { }

	constexpr ForwardListIterator(const ForwardListIterator &other) noexcept = default;

	constexpr ForwardListIterator &operator=(const ForwardListIterator &other) noexcept = default;

	constexpr bool operator==(const ForwardListIterator &other) const = default;
	constexpr bool operator!=(const ForwardListIterator &other) const = default;

	constexpr ForwardListIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	constexpr ForwardListIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
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
struct ForwardListConstIterator {
	using iterator_category = forward_iterator_tag;

	using node_type = ForwardListNodeBase<Allocator>;
	using list_node_type = ForwardListNode<T, Allocator>;
	using reference = const T &;
	using pointer = const T *;

	const node_type *__target = nullptr;

	constexpr ForwardListConstIterator() noexcept = default;

	constexpr explicit ForwardListConstIterator(const node_type *target) : __target(target) { }

	constexpr ForwardListConstIterator(const ForwardListIterator<T, Allocator> &other)
	: __target(other.__target) { }

	constexpr ForwardListConstIterator(const ForwardListConstIterator &other) noexcept = default;

	constexpr ForwardListConstIterator &operator=(
			const ForwardListConstIterator &other) noexcept = default;

	constexpr bool operator==(const ForwardListConstIterator &other) const = default;
	constexpr bool operator!=(const ForwardListConstIterator &other) const = default;

	constexpr ForwardListConstIterator &operator++() {
		__target = __target->next;
		return *this;
	}
	constexpr ForwardListConstIterator operator++(int) {
		auto tmp = *this;
		__target = __target->next;
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
class SPRT_API forward_list_base : public common_list_base<ForwardListNodeBase<Allocator>,
										   ForwardListNode<T, Allocator>, Allocator> {
public:
	using base_type = common_list_base<ForwardListNodeBase<Allocator>,
			ForwardListNode<T, Allocator>, Allocator>;
	using node_allocator_type = base_type::node_allocator_type;
	using node_type = base_type::node_type;
	using size_type = base_type::size_type;
	using basic_node_type = base_type::basic_node_type;
	using allocator_helper = base_type::allocator_helper;

	constexpr forward_list_base(const node_allocator_type &alloc = node_allocator_type()) noexcept
	: base_type(alloc) { }

	constexpr forward_list_base(const forward_list_base &other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: forward_list_base(alloc) {
		__clone(other);
	}

	constexpr forward_list_base(forward_list_base &&other,
			const node_allocator_type &alloc = node_allocator_type()) noexcept
	: forward_list_base(alloc) {
		__move(sprt::move(other));
	}

	constexpr forward_list_base &operator=(const forward_list_base &other) noexcept {
		__clone(other);
		return *this;
	}

	constexpr forward_list_base &operator=(forward_list_base &&other) noexcept {
		__move(sprt::move(other));
		return *this;
	}

	constexpr void swap(forward_list_base &other) noexcept {
		auto doSwap = [&] {
			sprt::swap(this->_alloc, other._alloc);
			sprt::swap(this->_size, other._size);
			sprt::swap(this->_root, other._root);

			// fix ring pointers
			auto n = &this->_root;
			while (n->next != &other._root) { n = n->next; }
			n->next = &this->_root;

			n = &other._root;
			while (n->next != &this->_root) { n = n->next; }
			n->next = &other._root;

			sprt::swap(this->_storage, other._storage);
		};

		if constexpr (sprt::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			doSwap();
		} else if (other._alloc == this->_alloc) {
			doSwap();
		}
	}

	constexpr void insert(basic_node_type *target, node_type *node) {
		node_type::insert(target, node);
		++this->_size;
	}

	constexpr void insert_front(node_type *node) { insert(&this->_root, node); }

	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	constexpr basic_node_type *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(&this->_root, count, cb);
	}

	template <typename ConstructCallback>
	constexpr basic_node_type *expand(basic_node_type *insertTarget, size_t count,
			const ConstructCallback &cb) {
		basic_node_type *tail = insertTarget;

		while (this->_storage && count > 0) {
			auto node = this->_storage;
			this->_storage = node->getNextStorage();

			cb(this->_alloc, node);

			tail = node_type::insert(tail, node);

			++this->_size;
			--count;
		}

		// Allocate block only if required size is larger then BlockThreshold
		// In other case, block allocation is ineffective
		if (count > 1 && this->_root.flag.index < node_type::MaxIndex
				&& count * sizeof(node_type) > memory::config::BlockThreshold) {
			size_type n = count;

			auto preallocIdx = ++this->_root.flag.index;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				if (idx < count) {
					cb(this->_alloc, node);
					++this->_size;
					tail = node_type::insert(tail, node);
				} else {
					++this->_root.flag.size;
					node->setNextStorage(this->_storage);
					this->_storage = node;
				}
				return true;
			}, this->_alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			auto node = this->allocate_node();
			cb(this->_alloc, node);
			++this->_size;
			tail = node_type::insert(tail, node);
		} else {
			if constexpr (is_same_v<Allocator, detail::AllocatorPool<T>>) {
				size_type n = count;
				// but we still can do batch allocation
				allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
					if (idx < count) {
						cb(this->_alloc, node);
						++this->_size;
						tail = node_type::insert(tail, node);
					} else {
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
					++this->_size;
					tail = node_type::insert(tail, node);
				}
			}
		}
		return tail;
	}

	constexpr basic_node_type *erase_after(basic_node_type *target) {
		auto node = node_type::erase_after(target);
		auto ret = node->next;
		node_type::destroyValue(this->_alloc, static_cast<node_type *>(node));
		--this->_size;
		this->destroyNode(static_cast<node_type *>(node));
		return ret;
	}

protected:
	constexpr void __clone(const forward_list_base &other) {
		auto preallocTmp = this->memory_persistent();
		this->memory_persistent(true);
		this->clear();
		this->memory_persistent(preallocTmp);

		basic_node_type *source = other._root.next;

		expand_front(other.size(), [&](auto alloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::copyValue(this->_alloc, dest, static_cast<node_type *>(source));
			source = source->next;
		});
	}

	constexpr void __move(forward_list_base &&other) {
		if (other.get_allocator() != this->get_allocator()) {
			this->memory_persistent(false);
			this->clear();
			this->shrink_to_fit();

			memory_persistent(other.memory_persistent());
			this->_size = other._size;
			this->_root = other._root;
			this->_storage = other._storage;

			auto n = &this->_root;
			while (n->next != &other._root) { n = n->next; }
			n->next = &this->_root;

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

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_FORWARD_LIST_BASE_H_
