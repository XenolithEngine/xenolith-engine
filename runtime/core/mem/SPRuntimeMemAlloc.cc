/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "SPRTMemStruct.h"
#include <sprt/runtime/platform.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/cxx/__mutex/unique_lock.h>

namespace sprt::memory::impl {

static atomic<size_t> s_nAllocators = 0;

#if DEBUG
static bool isValidNode(MemNode *node) {
	/*std::set<MemNode *> nodes;

	while (node) {
		auto tmp = node->next;
		if (nodes.find(node) == nodes.end()) {
			nodes.emplace(node);
		} else {
			return false;
		}
		node = tmp;
	}*/
	return true;
}
#endif

size_t Allocator::getAllocatorsCount() { return s_nAllocators.load(); }

SPRT_UNUSED static uint8_t *Allocator_mmap(size_t size) {
#if 0
	auto addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (addr != MAP_FAILED) {
		return reinterpret_cast<uint8_t *>(addr);
	}
#endif
	return nullptr;
}

static void Allocator_unmmap(uint8_t *ptr, size_t size) {
#if 0
	::munmap(ptr, size);
#endif
}

static MemNode *Allocator_malloc(size_t size, uint32_t index) {
	uint32_t mapped = 0;
	uint8_t *ptr = nullptr;
#if 0
	// NOTE: this page-alignment gate is the wrong criterion for choosing mmap.
	// Anonymous mmap only beats malloc/arena allocation for *large* blocks: both
	// glibc and macOS deliberately avoid it below ~128 KiB. Below that threshold
	// mmap loses to malloc because of first-touch page faults, mandatory kernel
	// zeroing, page-alignment waste, per-call syscall cost, and no free-list reuse
	// (a freed mmap block goes straight back to the kernel and must re-fault).
	//   - glibc: default M_MMAP_THRESHOLD is 128 KiB and ratchets dynamically up
	//     to 32 MiB; it only mmaps at/above that. A real benchmark (Quickwit)
	//     measured ~330 MB/s when forcing mmap (threshold=0) vs ~700 MB/s with a
	//     4 MiB threshold.
	//   - macOS libmalloc routes only the "large" zone (~128 KiB+) to mach_vm_alloc;
	//     smaller sizes stay in pooled tiny/small/medium magazines.
	// BOUNDARY_SIZE (8 KiB) being page-aligned says nothing about this: it would
	// start mmapping MIN_ALLOC (16 KiB) nodes, squarely in the range where mmap
	// is a net loss. If re-enabled, gate on an absolute size (>= ~128 KiB) and
	// ideally only for the large, non-recycled sink nodes (index >= MAX_INDEX).
	static bool isPageAligned = config::BOUNDARY_SIZE % platform::getMemoryPageSize() == 0;
	if (isPageAligned) {
		ptr = Allocator_mmap(size);
	}
#endif

	if (ptr) {
		mapped = 1;
	} else {
		ptr = reinterpret_cast<uint8_t *>(::__sprt_local_alloc(size));
	}

	return new (ptr) MemNode{
		{},
		nullptr,
		nullptr,
		mapped,
		index,
		0,
		ptr + SIZEOF_MEMNODE,
		ptr + size,
	};
}

static void Allocator_free(MemNode *ptr) {
	if (ptr->mapped) {
		Allocator_unmmap(reinterpret_cast<uint8_t *>(ptr),
				ptr->endp - reinterpret_cast<uint8_t *>(ptr));
	} else {
		::__sprt_local_free(ptr, 0);
	}
}

Allocator::Allocator() {
	++s_nAllocators;
	buf.fill(nullptr);
}

Allocator::~Allocator() {
	unique_lock lock(mutex);
	for (uint32_t index = 0; index < config::MAX_INDEX; index++) {
		auto node = buf[index];

#if DEBUG
		if (!isValidNode(node)) {
			__sprt_abort();
		}
#endif

		while (node) {
			auto tmp = node->next;
			allocated -= node->endp - (uint8_t *)node;
			Allocator_free(node);
			node = tmp;
		}
		buf[index] = nullptr;
	}

	--s_nAllocators;
}

void Allocator::set_max(size_t size) {
	unique_lock<Allocator> lock(*this);

	uint32_t max_free_index =
			uint32_t(math::align(size, size_t(config::BOUNDARY_SIZE)) >> config::BOUNDARY_INDEX);
	current += max_free_index;
	current -= max;
	max = max_free_index;
	if (current > max) {
		current = max;
	}
}

MemNode *Allocator::alloc(size_t in_size) {
	unique_lock<Allocator> lock;

	size_t size = uint32_t(math::align(in_size + SIZEOF_MEMNODE, size_t(config::BOUNDARY_SIZE)));
	if (size < in_size) {
		return nullptr;
	}
	if (size < config::MIN_ALLOC) {
		size = config::MIN_ALLOC;
	}

	// `size` is clamped to [MIN_ALLOC, UINT32_MAX] above, so index is in
	// [1, (0xFFFFE000 >> BOUNDARY_INDEX) - 1] and always fits MemNode::index.
	// Indices >= MAX_INDEX are valid and routed to the sink (buf[0]); only the
	// `index <= last` path below dereferences buf[index], and `last` is kept
	// < MAX_INDEX, so that access is in-bounds. (Upstream APR's
	// `index > UINT32_MAX` guard is dead here since index is uint32_t.)
	uint32_t index = uint32_t(size >> config::BOUNDARY_INDEX) - 1;

	MemNode *node = nullptr;
	MemNode **ref = nullptr;

	/* First see if there are any nodes in the area we know
	 * our node will fit into.
	 */
	lock = unique_lock<Allocator>(*this);
	if (index <= last) {
		/* Walk the free list to see if there are
		 * any nodes on it of the requested size
		 */
		uint32_t max_index = last;
		ref = &buf[index];
		uint32_t i = index;
		while (*ref == nullptr && i < max_index) {
			ref++;
			i++;
		}

		if ((node = *ref) != nullptr) {
			/* If we have found a node and it doesn't have any
			 * nodes waiting in line behind it _and_ we are on
			 * the highest available index, find the new highest
			 * available index
			 */
			if ((*ref = node->next) == nullptr && i >= max_index) {
				do {
					ref--;
					max_index--;
				} while (*ref == nullptr && max_index > 0);

				last = max_index;
			}

			current += node->index + 1;
			if (current > max) {
				current = max;
			}

			node->next = nullptr;
			node->first_avail = (uint8_t *)node + SIZEOF_MEMNODE;

			return node;
		}
	} else if (buf[0]) {
		/* If we found nothing, seek the sink (at index 0), if
		 * it is not empty.
		 */

		/* Walk the free list to see if there are
		 * any nodes on it of the requested size
		 */
		ref = &buf[0];
		while ((node = *ref) != nullptr && index > node->index) { ref = &node->next; }

		if (node) {
			*ref = node->next;

			current += node->index + 1;
			if (current > max) {
				current = max;
			}

			node->next = nullptr;
			node->first_avail = (uint8_t *)node + SIZEOF_MEMNODE;

			return node;
		}
	}

	/* If we haven't got a suitable node, malloc a new one
	 * and initialize it.
	 */
	node = nullptr;

	if (lock.owns_lock()) {
		lock.unlock();
	}

	if ((node = Allocator_malloc(size, index)) == nullptr) {
		return nullptr;
	}

	allocated += size;

	return node;
}

void Allocator::free(MemNode *node) {
	MemNode *next, *freelist = nullptr;

	unique_lock<Allocator> lock(*this);

	uint32_t max_index = last;
	uint32_t max_free_index = max;
	uint32_t current_free_index = current;

	/* Walk the list of submitted nodes and free them one by one,
	 * shoving them in the right 'size' buckets as we go.
	 */
	do {
		next = node->next;
		uint32_t index = node->index;

		if (max_free_index != config::ALLOCATOR_MAX_FREE_UNLIMITED
				&& index + 1 > current_free_index) {
			node->next = freelist;
			freelist = node;
		} else if (index < config::MAX_INDEX) {
			/* Add the node to the appropiate 'size' bucket.  Adjust
			 * the max_index when appropiate.
			 */
			if ((node->next = buf[index]) == nullptr && index > max_index) {
				max_index = index;
			}
			buf[index] = node;
			if (current_free_index >= index + 1) {
				current_free_index -= index + 1;
			} else {
				current_free_index = 0;
			}

#if DEBUG
			if (!isValidNode(buf[index])) {
				__sprt_abort();
			}
#endif
		} else {
			/* This node is too large to keep in a specific size bucket,
			 * just add it to the sink (at index 0).
			 */
			node->next = buf[0];
			buf[0] = node;
			if (current_free_index >= index + 1) {
				current_free_index -= index + 1;
			} else {
				current_free_index = 0;
			}
		}
	} while ((node = next) != nullptr);

#if DEBUG
	int i = 0;
	auto n = buf[1];
	while (n && i < 1'024 * 16) {
		n = n->next;
		++i;
	}

	if (i >= 1'024 * 16) {
		__sprt_perror("ERRER: pool double-free detected!\n");
		__sprt_abort();
	}
#endif

	last = max_index;
	current = current_free_index;

	if (lock.owns_lock()) {
		lock.unlock();
	}

	while (freelist != nullptr) {
		node = freelist;
		freelist = node->next;
		allocated -= node->endp - (uint8_t *)node;
		Allocator_free(node);
	}
}

void Allocator::lock() { mutex.lock(); }

void Allocator::unlock() { mutex.unlock(); }

} // namespace sprt::memory::impl
