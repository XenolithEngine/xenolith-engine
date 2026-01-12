/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_INFO_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_INFO_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/mem/function.h>
#include <sprt/c/__sprt_unistd.h>

namespace sprt::thread {

struct SPRT_API info {
	static constexpr uint32_t DetachedWorker = Max<uint32_t>;

	static uint32_t hardware_concurrency();

	static const info *get();

	static void set(StringView, uint32_t worker = DetachedWorker, bool managed = true);

	// Registers cleanup function, that will be called when thread about to exit
	template <typename Callback>
	static void add_cleanup(Callback &&);

	info();

	__SPRT_ID(pid_t) tid = 0;
	uint32_t workerId = 0;
	StringView name;
	bool managed = false;

	memory::pool_t *threadPool = nullptr;
	memory::pool_t *workerPool = nullptr;
};

template <typename Callback>
void info::add_cleanup(Callback &&cb) {
	static_assert(is_invocable_v<Callback>, "Callback should be invokable without arguments");
	auto d = get();
	if (d) {
		memory::perform_conditional([&] {
			memory::pool::cleanup_register(d->threadPool,
					memory::function<void()>(sprt::forward<Callback>(cb)));
		}, d->threadPool);
	}
}

} // namespace sprt::thread

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_INFO_H_
