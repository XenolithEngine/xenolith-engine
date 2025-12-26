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

#include "SPMemPoolStruct.h"
#include "SPPlatform.h"

#include <limits.h>
#include <fcntl.h>
#include <sys/mman.h>

namespace STAPPLER_VERSIONIZED stappler::memory::custom {

static std::atomic<size_t> s_nAllocators = 0;

#if DEBUG
static bool isValidNode(MemNode *node) {
	std::set<MemNode *> nodes;

	while (node) {
		auto tmp = node->next;
		if (nodes.find(node) == nodes.end()) {
			nodes.emplace(node);
		} else {
			return false;
		}
		node = tmp;
	}
	return true;
}
#endif

size_t Allocator::getAllocatorsCount() { return s_nAllocators.load(); }

static uint8_t *Allocator_mmap(size_t size) {
	auto addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (addr != MAP_FAILED) {
		return reinterpret_cast<uint8_t *>(addr);
	}
	return nullptr;
}

static void Allocator_unmmap(uint8_t *ptr, size_t size) { ::munmap(ptr, size); }

static MemNode *Allocator_malloc(size_t size, uint32_t index) {
	static bool isPageAligned = config::BOUNDARY_SIZE % platform::getMemoryPageSize() == 0;

	uint32_t mapped = 0;
	uint8_t *ptr = nullptr;
	if (isPageAligned) {
		ptr = Allocator_mmap(size);
	}

	if (ptr) {
		mapped = 1;
	} else {
		ptr = reinterpret_cast<uint8_t *>(::malloc(size));
	}

	return new (ptr) MemNode{nullptr, nullptr, mapped, index, 0, ptr + SIZEOF_MEMNODE, ptr + size};
}

static void Allocator_free(MemNode *ptr) {
	if (ptr->mapped) {
		Allocator_unmmap(reinterpret_cast<uint8_t *>(ptr),
				ptr->endp - reinterpret_cast<uint8_t *>(ptr));
	} else {
		::free(ptr);
	}
}

Allocator::Allocator() {
	++s_nAllocators;
	buf.fill(nullptr);
}

Allocator::~Allocator() {
	for (uint32_t index = 0; index < config::MAX_INDEX; index++) {
		auto node = buf[index];

#if DEBUG
		if (!isValidNode(node)) {
			abort();
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
	std::unique_lock<Allocator> lock(*this);

	uint32_t max_free_index =
			uint32_t(config::SPALIGN(size, config::BOUNDARY_SIZE) >> config::BOUNDARY_INDEX);
	current += max_free_index;
	current -= max;
	max = max_free_index;
	if (current > max) {
		current = max;
	}
}

MemNode *Allocator::alloc(size_t in_size) {
	std::unique_lock<Allocator> lock;

	size_t size = uint32_t(config::SPALIGN(in_size + SIZEOF_MEMNODE, config::BOUNDARY_SIZE));
	if (size < in_size) {
		return nullptr;
	}
	if (size < config::MIN_ALLOC) {
		size = config::MIN_ALLOC;
	}

	uint32_t index = uint32_t(size >> config::BOUNDARY_INDEX) - 1;
	if (index > maxOf<uint32_t>()) {
		return nullptr;
	}

	MemNode *node = nullptr;
	MemNode **ref = nullptr;

	/* First see if there are any nodes in the area we know
	 * our node will fit into.
	 */
	lock = std::unique_lock<Allocator>(*this);
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
				} while (*ref == NULL && max_index > 0);

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

	std::unique_lock<Allocator> lock(*this);

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
				abort();
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

	if (i >= 1'024 * 128) {
		printf("ERRER: pool double-free detected!\n");
		abort();
	}
#endif

	last = max_index;
	current = current_free_index;

	if (lock.owns_lock()) {
		lock.unlock();
	}

	while (freelist != NULL) {
		node = freelist;
		freelist = node->next;
		allocated -= node->endp - (uint8_t *)node;
		Allocator_free(node);
	}
}

void Allocator::lock() { mutex.lock(); }

void Allocator::unlock() { mutex.unlock(); }

} // namespace stappler::memory::custom
