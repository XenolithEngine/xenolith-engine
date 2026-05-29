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
#include <sprt/runtime/utils/backtrace.h>
#include <sprt/c/__sprt_assert.h>
#include <sprt/cxx/forward_list>
#include <sprt/cxx/list>
#include <sprt/cxx/map>
#include <sprt/runtime/thread/qmutex.h>

namespace sprt {

void RefAlloc::operator delete(RefAlloc *ptr, sprt::destroying_delete_t) noexcept {
	// should newer be called normally
	::__sprt_abort();
}

RefAlloc::~RefAlloc() { }

RefAlloc::RefAlloc() noexcept { }

} // namespace sprt


namespace sprt::memleak {

static qmutex s_mutex;
static atomic<uint64_t> s_refId = 1;

struct BackraceInfo : detail::AllocPool {
	time_t t = platform::clock(platform::ClockType::Monotonic);
	memory::pool_t *pool = nullptr;
	__pool_list<StringView> backtrace;
};

struct RefInfo : detail::AllocPool {
	qmutex refMutex;
	const Ref *ref = nullptr;
	memory::pool_t *pool = nullptr;
	__pool_map<uint64_t, BackraceInfo *> backtraces;
};

static __pool_map<const Ref *, RefInfo *> s_retainMap =
		__pool_map<const Ref *, RefInfo *>(memory::get_zero_pool());

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
		s_retainMap.memory_persistent(true);
	}

	auto it = s_retainMap.find(ptr);
	if (it == s_retainMap.end()) {
		// allocate by-reference pool
		auto newRefPool = memory::pool::create(memory::get_zero_pool());
		memory::perform([&] { info = new (newRefPool) RefInfo; }, newRefPool);

		info->pool = newRefPool;
		info->ref = ptr;
		info->backtraces.memory_persistent(true);

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

		sprt::backtrace::getBacktrace(2, [&](uintptr_t, StringView str) {
			btInfo->backtrace.emplace_back(str.pdup(btPool));
		});
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
		const callback<void(uint64_t, time_t, const __pool_list<StringView> &)> &cb) {

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
