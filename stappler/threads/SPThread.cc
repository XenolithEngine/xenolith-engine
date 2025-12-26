/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPThread.h"
#include "SPLog.h"

#ifdef MODULE_STAPPLER_ABI
#include "SPSharedModule.h"
#include "SPAbi.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::thread {

struct ThreadCallbacks {
	void (*init)(Thread *);
	void (*dispose)(Thread *);
	bool (*worker)(Thread *);
};

static void _workerThread(const ThreadCallbacks &tm, Thread *);
static void _setThreadName(StringView name);

static std::atomic<uint32_t> s_threadId(1);

thread_local ThreadInfo tl_threadInfo;
thread_local const Thread *tl_owner = nullptr;

static void ThreadCallbacks_init(const ThreadCallbacks &cb, Thread *tm) {
	memory::pool::initialize();

	tl_threadInfo.threadAlloc = memory::allocator::create();
	tl_threadInfo.threadPool = memory::pool::create(tl_threadInfo.threadAlloc);

	tl_threadInfo.workerPool = memory::pool::create(tl_threadInfo.threadPool);

	memory::perform([&] {
		tm->retain();
		cb.init(tm);
	}, tl_threadInfo.threadPool);
}

static bool ThreadCallbacks_worker(const ThreadCallbacks &cb, Thread *tm) {
	sprt_passert(tl_threadInfo.workerPool, "Thread pool should be initialized");
	bool ret = false;

	memory::perform_clear([&] { ret = cb.worker(tm); }, tl_threadInfo.workerPool);

	return ret;
}

static void ThreadCallbacks_dispose(const ThreadCallbacks &cb, Thread *tm) {
	memory::perform([&] {
		cb.dispose(tm);
		tm->release(0);
	}, tl_threadInfo.threadPool);

	memory::pool::destroy(tl_threadInfo.workerPool);

	memory::pool::destroy(tl_threadInfo.threadPool);
	memory::allocator::destroy(tl_threadInfo.threadAlloc);

	memory::pool::terminate();
}

const ThreadInfo *ThreadInfo::getThreadInfo() {
	if (!tl_threadInfo.managed) {
		return nullptr;
	}

	return &tl_threadInfo;
}

void ThreadInfo::setThreadInfo(StringView n, uint32_t w, bool m) {
	_setThreadName(n);

	tl_threadInfo.workerId = w;
	tl_threadInfo.name = n;
	tl_threadInfo.managed = m;
}

bool ThreadInfo::setThreadPool(const NotNull<memory::pool_t> &pool) {
	if (tl_threadInfo.threadPool) {
		return false;
	}

	tl_threadInfo.threadPool = pool;
	memory::pool::cleanup_register(pool, nullptr, [](void *ptr) -> Status {
		tl_threadInfo.threadPool = nullptr;
		return Status::Ok;
	});
	return true;
}

void Thread::workerThread(Thread *tm) {
	tl_owner = tm;

	ThreadCallbacks cb;
	cb.init = [](Thread *obj) { obj->threadInit(); };

	cb.dispose = [](Thread *obj) { obj->threadDispose(); };

	cb.worker = [](Thread *obj) -> bool { return obj->worker(); };

	memory::pool::initialize();
	_workerThread(cb, tm);
	memory::pool::terminate();
}

const Thread *Thread::getCurrentThread() { return tl_owner; }

#if STAPPLER_STATIC_TOOLCHAIN

void Thread::Type::detach() {
	auto detachThread = SharedModule::acquireTypedSymbol<decltype(&abi::detachThread)>(
			buildconfig::MODULE_STAPPLER_ABI_NAME, "detachThread");
	if (detachThread) {
		detachThread(threadPtr);
	}
}

void Thread::Type::join() {
	auto joinThread = SharedModule::acquireTypedSymbol<decltype(&abi::joinThread)>(
			buildconfig::MODULE_STAPPLER_ABI_NAME, "joinThread");
	if (joinThread) {
		joinThread(threadPtr);
	}
}

Thread::Id Thread::getCurrentThreadId() { return Id{pthread_self()}; }

#else

Thread::Id Thread::getCurrentThreadId() { return std::this_thread::get_id(); }

#endif

Thread::~Thread() {
	if (getCurrentThreadId() == _thisThreadId) {
		_thisThread.detach();
		return;
	}

	if ((_flags & ThreadFlags::Joinable) != ThreadFlags::None) {
		_continueExecution.clear();
		_thisThread.join();
	}
}

bool Thread::run(ThreadFlags flags) {
	if (_type != nullptr) {
		log::source().error("Thread", "Thread already started");
		return false;
	}

	_flags = flags;
	_type = &(typeid(*this));
	_continueExecution.test_and_set();
	_parentThread = getCurrentThread();
#if STAPPLER_STATIC_TOOLCHAIN
	auto createThread = SharedModule::acquireTypedSymbol<decltype(&abi::createThread)>(
			buildconfig::MODULE_STAPPLER_ABI_NAME, "createThread");
	if (createThread) {
		_thisThread = Type{createThread(Thread::workerThread, this, toInt(flags))};
		if (!_thisThread.threadPtr) {
			return false;
		}
	}
#else
	_thisThread = std::thread(Thread::workerThread, this);
#endif
	if ((flags & ThreadFlags::Joinable) == ThreadFlags::None) {
		_thisThread.detach();
	}
	return true;
}

void Thread::stop() { _continueExecution.clear(); }

void Thread::waitRunning() {
	if (_running.load()) {
		return;
	}

	std::unique_lock lock(_runningMutex);
	if (_running.load()) {
		return;
	}

	_runningVar.wait(lock, [&] { return _running.load(); });
}

void Thread::waitStopped() {
	_thisThread.join();
	_flags &= ~ThreadFlags::Joinable;
}

void Thread::threadInit() {
	_thisThreadId = getCurrentThreadId();

	std::unique_lock lock(_runningMutex);
	_running.store(true);
	_runningVar.notify_all();
}

void Thread::threadDispose() { }

bool Thread::worker() { return performWorkload() && _continueExecution.test_and_set(); }

bool Thread::isOnThisThread() const { return _thisThreadId == getCurrentThreadId(); }

} // namespace stappler::thread
