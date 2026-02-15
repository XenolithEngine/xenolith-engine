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

#include <sprt/runtime/platform.h>
#include <sprt/runtime/ref.h>
#include <sprt/runtime/backtrace.h>
#include <sprt/c/__sprt_assert.h>
#include <sprt/runtime/mem/forward_list.h>
#include <sprt/runtime/mem/map.h>
#include <sprt/runtime/mutex.h>

namespace sprt {

struct RefAllocData {
	void *lastPtr = nullptr;

	memory::forward_list<memory::pool_t *> *delayedPools = nullptr;
	memory::forward_list<memory::allocator_t *> *delayedAllocs = nullptr;

	static RefAllocData *get() {
		static thread_local RefAllocData tl_RefAllocData;

		return &tl_RefAllocData;
	}

	~RefAllocData() { clear(); }

	void clear() {
		if (delayedPools) {
			while (!delayedPools->empty()) {
				sprt::memory::pool::destroy(delayedPools->front());
				delayedPools->pop_front();
			}

			delayedPools->clear();
		}

		if (delayedAllocs) {
			while (!delayedAllocs->empty()) {
				sprt::memory::allocator::destroy(delayedAllocs->front());
				delayedAllocs->pop_front();
			}

			delayedAllocs->clear();
		}
	}
};

void *RefAlloc::operator new(size_t size, memory::pool_t *pool) noexcept {
	sprt_passert(pool, "Context pool should be defined for allocation");

	auto ptr = sprt::memory::pool::palloc(pool, size);
	RefAllocData::get()->lastPtr = ptr;
	return ptr;
}


void RefAlloc::__abi_operator_delete(void *ptr) noexcept {
	auto d = RefAllocData::get();
	if (ptr != d->lastPtr) {
		__sprt_free(ptr);
	}
	d->lastPtr = nullptr;
	d->clear();
}

void RefAlloc::__abi_operator_delete(void *ptr, size_t al) noexcept {
	auto d = RefAllocData::get();
	if (ptr != d->lastPtr) {
		if (toInt(al) <= alignof(__sprt_max_align_t)) {
			return __sprt_free(ptr);
		} else {
			return __sprt_aligned_free(ptr);
		}
	}
	d->lastPtr = nullptr;
	d->clear();
}

RefAlloc::~RefAlloc() {
	if ((_referenceCount.load() & PoolAllocBit) != 0) {
		RefAllocData::get()->lastPtr = this;
	}
}

RefAlloc::RefAlloc() noexcept {
	auto d = RefAllocData::get();
	if (d->lastPtr == this) {
		_referenceCount.fetch_or(PoolAllocBit);
	}
	d->lastPtr = nullptr;
}

void RefAlloc::destroySelfContained(memory::pool_t *pool) {
	auto d = RefAllocData::get();
	if (!d->delayedPools) {
		auto pool = memory::get_thread_support_pool();
		d->delayedPools = new (pool) memory::forward_list<memory::pool_t *>(pool);
		d->delayedPools->set_memory_persistent(true);
	}

	d->delayedPools->emplace_front(pool);
}

void RefAlloc::destroySelfContained(memory::allocator_t *alloc) {
	auto d = RefAllocData::get();
	if (!d->delayedAllocs) {
		auto pool = memory::get_thread_support_pool();
		d->delayedAllocs = new (pool) memory::forward_list<memory::allocator_t *>(pool);
		d->delayedAllocs->set_memory_persistent(true);
	}

	d->delayedAllocs->emplace_front(alloc);
}

void getBacktrace(size_t offset, const callback<void(StringView)> &cb) {
	sprt::backtrace::getBacktrace(offset + 1, cb);
}

} // namespace sprt


namespace sprt::memleak {

static qmutex s_mutex;
static atomic<uint64_t> s_refId = 1;

struct BackraceInfo : memory::AllocPool {
	time_t t = platform::clock(platform::ClockType::Monotonic);
	memory::pool_t *pool = nullptr;
	memory::forward_list<StringView> backtrace;
};

struct RefInfo : memory::AllocPool {
	qmutex refMutex;
	const Ref *ref = nullptr;
	memory::pool_t *pool = nullptr;
	memory::map<uint64_t, BackraceInfo *> backtraces;
};

static memory::map<const Ref *, RefInfo *> s_retainMap =
		memory::map<const Ref *, RefInfo *>(memory::get_zero_pool());

uint64_t getNextRefId() { return s_refId.fetch_add(1); }

uint64_t retainBacktrace(const Ref *ptr, uint64_t id) {
	if (!id) {
		return 0;
	}

	if (id == Max<uint64_t>) {
		id = getNextRefId();
	}

	RefInfo *info = nullptr;

	// acquire global info for reference
	s_mutex.lock();
	// on initial call, set persistent memory for map
	if (s_retainMap.empty()) {
		s_retainMap.set_memory_persistent(true);
	}

	auto it = s_retainMap.find(ptr);
	if (it == s_retainMap.end()) {
		// allocate by-reference pool
		auto newRefPool = memory::pool::create(memory::get_zero_pool());
		memory::perform([&] { info = new (newRefPool) RefInfo; }, newRefPool);

		info->pool = newRefPool;
		info->ref = ptr;
		info->backtraces.set_memory_persistent(true);

		s_retainMap.emplace(ptr, info);
	} else {
		info = it->second;
	}
	s_mutex.unlock();

	// collect backtrace with new pool
	BackraceInfo *btInfo = nullptr;

	// pool creation is thread-safe
	auto btPool = memory::pool::create(info->pool);
	memory::perform([&] {
		btInfo = new (btPool) BackraceInfo;
		btInfo->pool = btPool;

		getBacktrace(2, [&](StringView str) { btInfo->backtrace.emplace_back(str.pdup(btPool)); });
	}, btPool);

	// write new backtrace to RefInfo, we need lock
	info->refMutex.lock();
	info->backtraces.emplace(id, btInfo);
	info->refMutex.unlock();
	return id;
}

void releaseBacktrace(const Ref *ptr, uint64_t id) {
	if (!id) {
		return;
	}

	// first - acquire ref info from global data
	RefInfo *info = nullptr;

	s_mutex.lock();
	auto it = s_retainMap.find(ptr);
	if (it != s_retainMap.end()) {
		info = it->second;
	}
	s_mutex.unlock();

	if (info) {
		info->refMutex.lock();
		memory::perform([&] {
			auto btInfoIt = info->backtraces.find(id);
			if (btInfoIt != info->backtraces.end()) {
				memory::pool::destroy(btInfoIt->second->pool);
				info->backtraces.erase(btInfoIt);
			}
		}, info->pool);
		info->refMutex.unlock();
	}
}

void foreachBacktrace(const Ref *ptr,
		const callback<void(uint64_t, time_t, const memory::forward_list<StringView> &)> &cb) {

	// first - acquire ref info from global data
	RefInfo *info = nullptr;

	s_mutex.lock();
	auto it = s_retainMap.find(ptr);
	if (it != s_retainMap.end()) {
		info = it->second;
	}
	s_mutex.unlock();

	if (!info) {
		return;
	}

	info->refMutex.lock();

	for (auto &it : info->backtraces) { cb(it.first, it.second->t, it.second->backtrace); }

	info->refMutex.unlock();
}

void releaseRef(const Ref *ptr) {
	s_mutex.lock();
	auto it = s_retainMap.find(ptr);
	if (it != s_retainMap.end()) {
		// this will destroy all the pool tree with backtrace data
		memory::pool::destroy(it->second->pool);
		s_retainMap.erase(it);
	}
	s_mutex.unlock();
}

} // namespace sprt::memleak
