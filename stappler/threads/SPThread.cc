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

#include <sprt/runtime/thread/entry.h>

namespace STAPPLER_VERSIONIZED stappler::thread {

static std::atomic<uint32_t> s_threadId(1);

thread_local const Thread *tl_owner = nullptr;

void Thread::workerThread(NotNull<Thread> tm) {
	tl_owner = tm;

	sprt::thread::callbacks cb;
	cb.init = [](NotNull<Ref> obj) {
		static_cast<Thread *>(obj.get())->threadInit(); //
	};

	cb.dispose = [](NotNull<Ref> obj) {
		static_cast<Thread *>(obj.get())->threadDispose(); //
	};

	cb.worker = [](NotNull<Ref> obj) -> bool {
		return static_cast<Thread *>(obj.get())->worker(); //
	};

	sprt::thread::_entry(cb, tm);
}

const Thread *Thread::getCurrentThread() { return tl_owner; }

Thread::Id Thread::getCurrentThreadId() { return std::this_thread::get_id(); }

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

	_thisThread = std::thread(Thread::workerThread, this);

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
