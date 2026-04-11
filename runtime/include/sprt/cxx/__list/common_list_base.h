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

#ifndef RUNTIME_INCLUDE_SPRT_CXX___LIST_COMMONLIST_BASE_H_
#define RUNTIME_INCLUDE_SPRT_CXX___LIST_COMMONLIST_BASE_H_

#include <sprt/cxx/__memory/allocator_traits.h>
#include <sprt/cxx/detail/allocator_pool.h>
#include <sprt/cxx/detail/nodebase.h>

namespace sprt::detail {

template <typename BasicNodeType, typename NodeType, typename Allocator>
class SPRT_API common_list_base : Allocator::base_class {
public:
	using basic_node_type = BasicNodeType;
	using node_type = NodeType;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using node_allocator_type = typename Allocator::template rebind<node_type>::other;
	using allocator_helper = NodeBlockAllocatorHelper<node_type, node_allocator_type>;

	constexpr common_list_base(const node_allocator_type &alloc = node_allocator_type())
	: _alloc(alloc) {
		_root.reset();
	}

	constexpr ~common_list_base() noexcept { clear_deallocate(); }

	constexpr auto get_allocator() const { return _alloc; }

	constexpr basic_node_type *front() { return _root.next; }
	constexpr basic_node_type *end() { return &_root; }

	constexpr const basic_node_type *front() const { return _root.next; }
	constexpr const basic_node_type *end() const { return &_root; }

	constexpr void clear() {
		auto node = _root.next;
		while (node != &_root) {
			auto next = node->next;
			destroyNode(static_cast<node_type *>(node));
			node = next;
		}

		_size = 0;

		_root.reset();

		if (!_storage || !_storage->flag.prealloc) {
			// clear preallocation index if there is no preserved nodes
			_root.flag.index = 0;
		}
	}

	constexpr void shrink_to_fit() noexcept {
		auto nFreed = allocator_helper::template release_blocks<true>(get_allocator(), &_storage,
				_root.flag.index);
		_root.flag.size -= nFreed;

		while (_storage && !_storage->flag.prealloc) {
			auto node = _storage;
			_storage = _storage->getNextStorage();
			auto size = node->getSize();
			_alloc.destroy(node);
			_alloc.__deallocate(node, 1, size);
			--_root.flag.size;
		}
	}

	constexpr void clear_deallocate() {
		auto tmp = memory_persistent();
		memory_persistent(false);
		clear();
		shrink_to_fit();

		if (!_storage) {
			// clear preallocation index if there is no preserved nodes
			_root.flag.index = 0;
		}

		memory_persistent(tmp);
	}

	constexpr void set_allocator(const Allocator &a) noexcept {
		if (a != _alloc) {
			clear_deallocate();
			_alloc = a;
		}
	}

	constexpr void set_allocator(Allocator &&a) noexcept {
		if (a != _alloc) {
			clear_deallocate();
			_alloc = sprt::move_unsafe(a);
		}
	}

	constexpr void reserve(size_type count) {
		if (count <= _size + _root.flag.size) {
			return;
		}

		if (count > 1 && _root.flag.index < node_type::MaxIndex
				&& count * sizeof(node_type) > memory::config::BlockThreshold) {
			size_type n = count;
			auto preallocIdx = ++_root.flag.index;
			allocator_helper::allocate_block([&](node_type *node, size_t idx) -> bool {
				++_root.flag.size;
				node->setNextStorage(_storage);
				_storage = node;
				return true;
			}, _alloc, n, preallocIdx, nullptr);
		} else if (count == 1) {
			auto node = allocate_node();
			++_root.flag.size;
			node->setNextStorage(_storage);
			_storage = node;
		} else {
			if constexpr (is_same_v<node_allocator_type, detail::AllocatorPool<node_type>>) {
				size_type n = count;
				// but we still can do batch allocation
				allocator_helper::allocate_batch([&](node_type *node, size_t idx) -> bool {
					++_root.flag.size;
					node->setNextStorage(_storage);
					_storage = node;
					return true;
				}, _alloc, n, nullptr);
			} else {
				// For others - we should allocate nodes one by one
				while (count-- > 0) {
					auto node = allocate_node();
					++_root.flag.size;
					node->setNextStorage(_storage);
					_storage = node;
				}
			}
		}
	}

	constexpr size_t capacity() const noexcept { return _size + _root.flag.size; }
	constexpr size_type max_size() const noexcept { return node_type::MaxSize; }
	constexpr size_t size() const noexcept { return _size; }

	constexpr void memory_persistent(bool value) noexcept { _root.flag.prealloc = value ? 1 : 0; }

	constexpr bool memory_persistent() const noexcept { return _root.flag.prealloc != 0; }

	constexpr node_type *allocate_node() {
		size_t s = 0;
		auto node = _alloc.__allocate(1, s);
		_alloc.construct(node);
		node->setPrealloc(false);
		node->setSize(s);
		return node;
	}

protected:
	constexpr void destroyNode(node_type *n) {
		node_type::destroyValue(_alloc, n);

		// we always preserve a single node
		if (!_storage || n->isPrealloc() || memory_persistent()) {
			n->setNextStorage(_storage);
			_storage = n;
			++_root.flag.size;
		} else {
			// deallocate node
			auto size = n->getSize();
			_alloc.destroy(n);
			_alloc.__deallocate(n, 1, size);
		}
	}

	[[no_unique_address]]
	node_allocator_type _alloc;
	size_type _size = 0;
	basic_node_type _root;
	node_type *_storage = nullptr;
};

} // namespace sprt::detail

#endif // RUNTIME_INCLUDE_SPRT_CXX___LIST_COMMONLIST_BASE_H_
