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

#define __SPRT_BUILD 1

#include <sprt/cxx/thread>

#include <sprt/c/__sprt_unistd.h>
#include <sprt/runtime/log.h>

#include "../../core/pthread/pthread_impl.h"

namespace sprt {

thread::~thread() { detach(); }

thread::thread(thread &&other) noexcept {
	__native = other.__native;
	other.__native = nullptr;
}
thread &thread::operator=(thread &&other) noexcept {
	if (&other == this) {
		return *this;
	}

	detach();

	__native = other.__native;
	other.__native = nullptr;
	return *this;
}

bool thread::joinable() const noexcept {
	return __native
			&& !hasFlag(reinterpret_cast<_thread::thread_t *>(__native)->attr.attr,
					_thread::ThreadAttrFlags::Detached);
}

bool thread::try_join() {
	if (__native) {
		auto ret = __sprt_pthread_tryjoin_np(__native, nullptr);
		if (ret == 0) {
			__native = nullptr;
			return true;
		}
	}
	return false;
}

void thread::join() {
	if (__native) {
		auto ret = __sprt_pthread_join(__native, nullptr);
		if (ret != 0) {
			oslog::vpfatal(__SPRT_LOCATION, "sprt::thread", "Fail to pthread_join: errno(", ret,
					")");
		}
		__native = nullptr;
	}
}

void thread::detach() {
	if (__native) {
		auto handle = reinterpret_cast<_thread::thread_t *>(__native);
		if (!hasFlag(handle->attr.attr, _thread::ThreadAttrFlags::Detached)) {
			__sprt_pthread_detach(__native);
		}
		__native = nullptr;
	}
}

thread::id thread::get_id() const noexcept {
	if (__native) {
		return {reinterpret_cast<_thread::thread_t *>(__native)->threadId};
	}
	return {0};
}

uint32_t thread::hardware_concurrency() noexcept {
	long result = __sprt_sysconf(__SPRT_SC_NPROCESSORS_ONLN);
	// > 1, not >= 0: callers size worker pools with integer `hardware_concurrency() / 2`, so a
	// report of 1 (what the wasm sysconf stub returns) collapses to 0 workers and every
	// performAsync task (font glyph rasterization, deferred work, ...) is queued but never runs.
	// Only trust a genuine multi-core report; otherwise default to a small pool.
	// TODO(wasm): expose navigator.hardwareConcurrency via a host import for an accurate count.
	if (result > 1) {
		return static_cast<unsigned>(result);
	}

#if SPRT_WASM
	return 4;
#else
	return 1;
#endif
}

int thread::__makeThread(void *(*cb)(uint8_t *st, size_t stSize),
		const Callback<void(uint8_t *st, size_t stSize)> &wcb) {
	_thread::thread_t *__t = nullptr;

	// sprt::thread should not be affected by default pthread args
	_thread::attr_t def;

	auto ret = _thread::thread_t::create(&__t, &def, [](_thread::thread_base_t *t) -> void * {
		auto tcb = reinterpret_cast<decltype(cb)>(t->arg);
		tcb(t->storage, THREAD_STORAGE_BLOCK_SIZE);
		return (void *)0;
	}, (void *)cb, wcb);
	if (ret == 0) {
		__native = __t;
	}
	return ret;
}

} // namespace sprt

// this_thread is declared inside the inline namespace __cxx_thread in <sprt/cxx/thread>,
// so its out-of-line definitions must go through that inline namespace to match
// (the qualified form `namespace sprt::this_thread` would define a different namespace).
namespace sprt {
inline namespace __cxx_thread {
namespace this_thread {

thread::id get_id() noexcept {
#if SPRT_HOSTED_RTOS
	return {__sprt_gettid()};
#else
	return {_thread::thread_t::self()->threadId};
#endif
}

void yield() noexcept { __sprt_sched_yield(); }

void sleep_for(const timeout_t &rel_time) {
	struct __SPRT_TIMESPEC_NAME ts = {
		static_cast<__sprt_time_t>(rel_time / 1'000'000'000),
		static_cast<__sprt_int64_t>(rel_time % 1'000'000'000),
	};

	__sprt_nanosleep(&ts, nullptr);
}

} // namespace this_thread
} // namespace __cxx_thread
} // namespace sprt
