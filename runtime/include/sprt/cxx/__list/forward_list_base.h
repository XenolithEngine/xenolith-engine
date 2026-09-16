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

	// The forward list is LINEAR (nullptr-terminated), unlike the circular double
	// list: end() must compare different from before_begin() ([forwardlist.iter]),
	// which a shared ring sentinel cannot provide.
	constexpr void reset() { next = nullptr; }

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

	// NodeAllocator is any allocator following the sprt rebind protocol (including
	// detail::AllocatorStd, whose template argument is the wrapped allocator rather
	// than the node type, so a template-template parameter cannot express it).
	template <typename NodeAllocator>
	constexpr static ForwardListNode *copyValue(const NodeAllocator &alloc, ForwardListNode *dest,
			ForwardListNode *target) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), target->value.ref());
		return dest;
	}

	// Move-relocating variant of copyValue for move-only element types (the
	// unequal-allocator move path cannot steal nodes and cannot copy either).
	template <typename NodeAllocator>
	constexpr static ForwardListNode *moveValue(const NodeAllocator &alloc, ForwardListNode *dest,
			ForwardListNode *target) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

		dest->value.construct(value_allocator(alloc), sprt::move_unsafe(target->value.ref()));
		return dest;
	}

	template <typename NodeAllocator>
	constexpr static ForwardListNode *destroyValue(const NodeAllocator &alloc,
			ForwardListNode *node) {
		using value_allocator = typename NodeAllocator::template rebind<T>::other;

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

	using value_type = typename remove_cv<T>::type;
	using difference_type = ptrdiff_t;
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

	using value_type = typename remove_cv<T>::type;
	using difference_type = ptrdiff_t;
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
			// linear (nullptr-terminated) list: swapping the heads is complete,
			// the tails already point at nullptr on both sides

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

	constexpr void insert_front(node_type *node) { insert(sprt::addressof(this->_root), node); }

	// add count nodes with ConstructCallback to fill it
	template <typename ConstructCallback>
	constexpr basic_node_type *expand_front(size_t count, const ConstructCallback &cb) {
		return expand(sprt::addressof(this->_root), count, cb);
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

	// LWG 526-safe remove_if: matching nodes are UNLINKED during the pass and
	// destroyed only afterwards, so a predicate (or value) referring into the
	// list stays valid while the scan runs.
	template <typename NodePred>
	constexpr size_t remove_if_nodes(NodePred pred) {
		basic_node_type *sent = sprt::addressof(this->_root);
		basic_node_type *prev = sent;
		node_type *deferred = nullptr; // stack of unlinked nodes, chained via next
		size_t removed = 0;
		while (prev->next != nullptr) {
			auto *cur = static_cast<node_type *>(prev->next);
			if (pred(cur)) {
				prev->next = cur->next;
				cur->next = deferred;
				deferred = cur;
				++removed;
				--this->_size;
			} else {
				prev = cur;
			}
		}
		while (deferred) {
			auto *next = static_cast<node_type *>(deferred->next);
			node_type::destroyValue(this->_alloc, deferred);
			this->destroyNode(deferred);
			deferred = next;
		}
		return removed;
	}

	// Relink-based stable merge for [forwardlist.ops]: nodes are transferred
	// between the (circular, sentinel-terminated) lists, so iterators and
	// references into `other` stay valid and end up pointing into *this.
	// Precondition (standard): get_allocator() == other.get_allocator().
	// `comp` orders two node pointers; on ties elements of *this precede
	// elements of `other` (stability).
	template <typename NodeCompare>
	constexpr void merge_nodes(forward_list_base &other, NodeCompare comp) {
		if (this == sprt::addressof(other)) {
			return;
		}
		basic_node_type *osent = sprt::addressof(other._root);
		basic_node_type *prev = sprt::addressof(this->_root);
		basic_node_type *onode = osent->next;
		while (onode != nullptr) {
			auto *cur = prev->next;
			// advance past every element of *this that is not strictly greater
			while (cur != nullptr
					&& !comp(static_cast<node_type *>(onode), static_cast<node_type *>(cur))) {
				prev = cur;
				cur = cur->next;
			}
			auto *onext = onode->next;
			onode->next = cur;
			prev->next = onode;
			prev = onode;
			onode = onext;
		}
		this->_size += other._size;
		other._size = 0;
		osent->next = nullptr;
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
		// Same allocator (always true for stateless/always-equal allocators such as
		// std::allocator): steal the storage outright.
		if (other.get_allocator() == this->get_allocator()) {
			this->memory_persistent(false);
			this->clear();
			this->shrink_to_fit();

			this->memory_persistent(other.memory_persistent());
			this->_size = other._size;
			this->_root = other._root;
			this->_storage = other._storage;
			// linear list: the stolen chain already terminates at nullptr

			other._size = 0;
			other._root.reset();
			other._root.flag.index = 0;
			other._root.flag.size = 0;
			other._root.flag.prealloc = 0;
			other._storage = nullptr;
		} else if constexpr (!sprt::is_empty_v<node_allocator_type>) {
			// Different (stateful) allocators: cannot steal. Copyable elements keep
			// the historical copy; move-only ones are move-relocated element-wise.
			if constexpr (sprt::is_copy_constructible_v<T>) {
				__clone(other);
			} else {
				__clone_move(other);
			}
		}
	}

	constexpr void __clone_move(forward_list_base &other) {
		auto preallocTmp = this->memory_persistent();
		this->memory_persistent(true);
		this->clear();
		this->memory_persistent(preallocTmp);

		basic_node_type *source = other._root.next;

		expand_front(other.size(), [&](auto alloc, node_type *dest) SPRT_LAMBDAINLINE {
			node_type::moveValue(this->_alloc, dest, static_cast<node_type *>(source));
			source = source->next;
		});
	}
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX_MEMORY_FORWARD_LIST_BASE_H_
