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

#ifndef STAPPLER_CORE_MEMORY_DETAIL_SPMEMSTORAGENODE_H_
#define STAPPLER_CORE_MEMORY_DETAIL_SPMEMSTORAGENODE_H_

#include "SPMemAlloc.h"

namespace STAPPLER_VERSIONIZED stappler::memory::detail {

template <size_t ArchSize>
struct SP_PUBLIC RbTreeNodeFlag;

template <>
struct SP_PUBLIC RbTreeNodeFlag<size_t(4)> {
	static constexpr uintptr_t MaxSize = maxOf<uintptr_t>();
	static constexpr uintptr_t MaxIndex = (1ULL << uintptr_t(sizeof(uintptr_t) * 8 - 2)) - 1;

	uintptr_t color	   : 1;
	uintptr_t prealloc : 1;

	// Index of preallocated block
	// on 32-bit systems overflow is close, so, use tree preallocation optimization with care
	uintptr_t index	   : sizeof(uintptr_t) * 8 - 2;

	// for root node - here we store capacity
	// for preallocated head node - block size in bytes
	// for other preallocaed nodes - should be 0
	// for single node - block size in bytes
	uintptr_t size;
};

template <>
struct SP_PUBLIC RbTreeNodeFlag<size_t(8)> {
	static constexpr uintptr_t MaxSize = (1ULL << uintptr_t((sizeof(uintptr_t) / 2) * 8)) - 1;
	static constexpr uintptr_t MaxIndex = (1ULL << uintptr_t((sizeof(uintptr_t) / 2) * 8 - 2)) - 1;

	uintptr_t color	   : 1;
	uintptr_t prealloc : 1;

	// Index of preallocated block
	// on 32-bit systems overflow is close, so, use tree preallocation optimization with care
	uintptr_t index	   : (sizeof(uintptr_t) / 2) * 8 - 2;

	// for root node - here we store capacity
	// for preallocated head node - block size in bytes
	// for other preallocaed nodes - should be 0
	// for single node - block size in bytes
	uintptr_t size	   : (sizeof(uintptr_t) / 2) * 8;
};

template <typename Node>
struct NodeBlockAllocatorHelper {
	using allocator_type = Allocator<Node>;

	// Callback in form [] (Node *, size_t) ->  bool {}
	// return true if node should not be placed in list by allocator
	// return false if allocator should make chain of nodes

	template <typename Callback>
	static Node *allocate_block(const Callback &cb, const allocator_type &alloc, size_t &count,
			uintptr_t preallocIdx, Node **tail) {
#if DEBUG
		if (count * sizeof(Node) > Node::Flag::MaxSize) {
			::perror("Fail to allocate extra memory block: too large");
			::abort();
		}
#endif

		size_t s = 0;
		auto ret = alloc.__allocate(count, s);
		auto n = ret;

		count = s / sizeof(Node);

		Node *tmpN = nullptr;
		for (size_t i = 0; i < count; ++i) {
			tmpN = n;
			alloc.construct(tmpN);
			if (!cb(n, i)) {
				*tmpN->getNextStoragePtr() = n + 1;
			}
			tmpN->setPrealloc(true);
			tmpN->setIndex(preallocIdx);
			++n;
		}

		if (tail) {
			*tail = tmpN;
		}

		// Set size for first node only
		// To find out if the list contains all nodes from a block, the list must have the
		// first block with a non-zero size value, from this size you can get the number
		// of nodes in the block, and compare this number with the number of free nodes
		ret->setSize(s);
		return ret;
	}

	// Batch allocation is just like block, but can not be effectively deallocated in batch
	// In general, it can lead to segmentation of returned memory blocks
	template <typename Callback>
	static Node *allocate_batch(const Callback &cb, const allocator_type &alloc, size_t &count,
			Node **tail) {
#if DEBUG
		if (count * sizeof(Node) > Node::Flag::MaxSize) {
			::perror("Fail to allocate extra memory block: too large");
			::abort();
		}
#endif

		size_t s = 0;
		auto ret = alloc.__allocate(count, s);
		auto n = ret;

		count = s / sizeof(Node);

		Node *tmpN = nullptr;
		for (size_t i = 0; i < count; ++i) {
			tmpN = n;
			alloc.construct(tmpN);
			if (!cb(n, i)) {
				*tmpN->getNextStoragePtr() = n + 1;
			}
			tmpN->setSize(sizeof(Node));
			s -= sizeof(Node);
			++n;
		}

		if (s > 0) {
			// append uncosumed size to last node
			tmpN->setSize(sizeof(Node) + s);
		}

		if (tail) {
			*tail = tmpN;
		}

		return ret;
	}

	template <bool ReconstructChain>
	static size_t release_blocks(const allocator_type &alloc, Node **list, size_t nblocks) {
		// release any tmp nodes if possible
		// preallocated nodes released in batch as acquired

		static constexpr size_t nCells = 16;

		size_t ret = 0;

		struct PreallocatedData {
			// Head should be mimibal pointer for block intex
			// then this will be the blockab address if the first node is free
			Node *head = reinterpret_cast<Node *>(maxOf<uintptr_t>());

			// Preserve free list for block in case we can not deallocate it
			Node *list = nullptr;
			Node *tail = nullptr;
			size_t count = 0;
		};

		// on first step - we remove everything from original list
		Node **segment = list;

		// all nodes with indexes, that overflows cell array will be placed in extraList
		Node *extraList = nullptr;

		// after step, extra list will become tmpList, and extra list will be new overflow list
		// then, we remove everything from tmpList, deallocate new indexed blocks, and fill
		// extraList with overflows
		Node *tmpList = nullptr;

		// offset is first processed index, limit - 1 is last processed index
		size_t offset = 0, limit = nCells;
		PreallocatedData data[nCells];

		// iterate until all known indexes processed
		while (offset < nblocks) {
			for (auto &it : data) { it = PreallocatedData(); }

			while (*segment) {
				auto node = *segment;
				if (node->isPrealloc()) {
					auto next = static_cast<Node *>(node->getNextStorage());
					auto index = node->getIndex();
					if (index < limit) {
						// index less then limit - process this node
						auto &targetCell = data[index - offset];
						if (node < targetCell.head) {
							targetCell.head = node;
						}

						if constexpr (ReconstructChain) {
							*node->getNextStoragePtr() = targetCell.list;
							targetCell.list = node;

							if (!targetCell.tail) {
								targetCell.tail = node;
							}
						}

						++targetCell.count;
						*segment = next;
					} else {
						// index overflow - add to extra list
						*node->getNextStoragePtr() = extraList;
						extraList = node;
						*segment = next;
					}
				} else {
					// node is not indexed - just deallocate it
					auto next = static_cast<Node *>(node->getNextStorage());

					alloc.destroy(node);
					alloc.__deallocate(node, 1, node->getSize());

					*segment = next;
					++ret;
				}
			}

			// now, deallocate indexed blocks cell by cell
			for (auto &cell : data) {
				if (reinterpret_cast<uintptr_t>(cell.head) != maxOf<uintptr_t>()
						&& cell.head->getSize()) {

					// check if we have all indexed nodes in free list, then we can deallocate block
					auto nNodes = cell.head->getSize() / sizeof(Node);
					if (nNodes == cell.count) {
						ret += cell.count;

						auto node = cell.head;
						for (size_t i = 0; i < cell.count; ++i, ++node) { alloc.destroy(node); }
						alloc.__deallocate(cell.head, cell.count, cell.head->getSize());
						continue;
					}
				}


				if constexpr (ReconstructChain) {
					// we failed to deallocate block
					// restore chain in original free list
					*cell.tail->getNextStoragePtr() = *list;
					*list = cell.list;
				}
			}

			// increment cell counters
			offset += nCells;
			limit += nCells;

			// swap extra list with tmp list
			tmpList = extraList;

			// use tmp list as new segment
			segment = &tmpList;
			extraList = nullptr;
		}

		return ret;
	}
};

} // namespace stappler::memory::detail

#endif // STAPPLER_CORE_MEMORY_DETAIL_SPMEMSTORAGENODE_H_
