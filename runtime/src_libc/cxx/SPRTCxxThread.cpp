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

#include <sprt/cxx/thread>

#include <sprt/c/__sprt_unistd.h>
#include <sprt/runtime/log.h>

#include "private/SPRTThread.h"

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
	if (result < 0) {
		return 0;
	}
	return static_cast<unsigned>(result);
}

struct __ThreadData {
	__malloc_function<void()> fn;
	thread *th = nullptr;
};

static void *__threadWrapper(void *ptr) {
	auto d = reinterpret_cast<__ThreadData *>(ptr);

	if (d->fn) {
		d->fn();
	}

	__delete(d);
	return nullptr;
}

int thread::__makeThread(__malloc_function<void()> &&fn) {
	auto d = new (sprt::nothrow) __ThreadData{
		sprt::move(fn),
		this,
	};

	return __sprt_pthread_create(&__native, nullptr, __threadWrapper, d);
}

} // namespace sprt

namespace sprt::this_thread {

thread::id get_id() noexcept { return {_thread::thread_t::self()->threadId}; }

void yield() noexcept { __sprt_sched_yield(); }

void sleep_for(const timeout_t &rel_time) {
	struct timespec ts = {
		static_cast<__sprt_time_t>(rel_time / 1'000'000'000),
		static_cast<__sprt_int64_t>(rel_time % 1'000'000'000),
	};

	__sprt_nanosleep(&ts, nullptr);
}

} // namespace sprt::this_thread
