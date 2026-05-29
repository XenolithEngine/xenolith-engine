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

#include "SPRuntimeDispatch.h"

#include <pthread.h>

#if SPRT_MACOS

typedef struct objc_object *id;
typedef struct objc_class *Class;
typedef struct objc_selector *SEL;

extern "C" void objc_msgSend(void);
extern "C" Class objc_getClass(const char *name);
extern "C" SEL sel_getUid(const char *str);

namespace sprt::dispatch {

using AutoreleasePool_new_type = id (*)(Class, SEL);
using AutoreleasePool_drain_type = void (*)(id, SEL);

template <typename Callback>
static void ThreadCallbacks_performInAutorelease(Callback &&cb) {
	id pool = ((AutoreleasePool_new_type)&objc_msgSend)(objc_getClass("NSAutoreleasePool"),
			sel_getUid("new"));
	cb();
	((AutoreleasePool_drain_type)&objc_msgSend)(pool, sel_getUid("drain"));
}

void _entry_platform(const callbacks &cb, NotNull<Ref> tm) {
	ThreadCallbacks_performInAutorelease([&] { _init(cb, tm); });

	bool ret = true;
	while (ret) {
		ThreadCallbacks_performInAutorelease([&] { ret = _worker(cb, tm); });
	}

	ThreadCallbacks_performInAutorelease([&] { _dispose(cb, tm); });
}

} // namespace sprt::dispatch

#else

namespace sprt::dispatch {

void _entry_platform(const callbacks &cb, NotNull<Ref> tm) {
	_init(cb, tm);
	while (_worker(cb, tm)) { }
	_dispose(cb, tm);
}

} // namespace sprt::dispatch


#endif

namespace sprt::dispatch {

thread_local thread_info tl_threadInfo;

void _init(const callbacks &cb, Ref *tm) {
	tl_threadInfo.threadPool = memory::get_thread_support_pool();
	tl_threadInfo.workerPool = memory::pool::create(tl_threadInfo.threadPool);

	memory::perform([&] {
		sprt::retain(tm);
		cb.init(tm);
	}, tl_threadInfo.threadPool);
}

bool _worker(const callbacks &cb, Ref *tm) {
	sprt_passert(tl_threadInfo.workerPool, "Thread pool should be initialized");
	bool ret = false;

	memory::perform_clear([&] { ret = cb.worker(tm); }, tl_threadInfo.workerPool);

	return ret;
}

void _dispose(const callbacks &cb, Ref *tm) {
	memory::perform([&] {
		cb.dispose(tm);
		sprt::release(tm, 0);
	}, tl_threadInfo.threadPool);

	memory::pool::destroy(tl_threadInfo.workerPool);
	memory::pool::clear(tl_threadInfo.threadPool);

	tl_threadInfo.workerPool = nullptr;
}

uint32_t thread_info::hardware_concurrency() {
	long result = __sprt_sysconf(__SPRT_SC_NPROCESSORS_ONLN);
	if (result < 0) {
		return 0;
	}
	return static_cast<unsigned>(result);
}

const thread_info *thread_info::get() {
	if (!tl_threadInfo.managed) {
		return nullptr;
	}

	return &tl_threadInfo;
}

void thread_info::set(StringView n, uint32_t w, bool m) {
	n.pdup(tl_threadInfo.threadPool).performWithTerminated([](const char *tname, size_t) {
		pthread_setname_np(pthread_self(), tname); //
	});

	tl_threadInfo.workerId = w;
	tl_threadInfo.name = n;
	tl_threadInfo.managed = m;
}

thread_info::thread_info() : tid(__sprt_gettid()) {
	threadPool = memory::get_thread_support_pool(); //
}

void _entry(const callbacks &cb, NotNull<Ref> tm) {
	memory::pool::initialize();

	_entry_platform(cb, tm);

	memory::pool::terminate();
}

} // namespace sprt::dispatch
