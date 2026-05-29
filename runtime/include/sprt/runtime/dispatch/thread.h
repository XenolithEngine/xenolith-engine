/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_H_

#include <sprt/cxx/thread>
#include <sprt/cxx/mutex>
#include <sprt/cxx/condition_variable>
#include <sprt/cxx/typeinfo>

#include <sprt/runtime/ref.h>

namespace sprt::dispatch {

enum class ThreadFlags : uint32_t {
	None = 0,
	Joinable = 1 << 0,
};

SPRT_DEFINE_ENUM_AS_MASK(ThreadFlags)

/* Interface for thread workers or handlers */
class SPRT_API Thread : public Ref {
public:
	using Id = sprt::thread::id;

	static void *workerThread(void *tm);

	static const Thread *getCurrentThread();
	static Id getCurrentThreadId();

	template <typename T>
	static const T *findSpecificThread();

	virtual ~Thread();

	virtual bool run(ThreadFlags = ThreadFlags::Joinable);
	virtual void stop();

	virtual void waitRunning();
	virtual void waitStopped();

	virtual void threadInit();
	virtual void threadDispose();
	virtual bool worker();

	// workload overload
	virtual bool performWorkload() { return false; }

	virtual bool isRunning() const { return _running.load(); }

	Id getThreadId() const { return _thisThreadId; }

	bool isOnThisThread() const;

	const Thread *getParentThread() const { return _parentThread; }

protected:
	ThreadFlags _flags = ThreadFlags::None;

	const Thread *_parentThread = nullptr;

	const sprt::type_info *_type = nullptr;

	sprt::thread _thisThread;
	sprt::thread::id _thisThreadId;

	sprt::atomic<bool> _running = false;
	sprt::mutex _runningMutex;
	sprt::condition_variable _runningVar;

	mutable sprt::atomic_flag _continueExecution;
};

template <typename T>
auto Thread::findSpecificThread() -> const T * {
	auto thread = getCurrentThread();

	while (thread) {
		auto d = dynamic_cast<const T *>(thread);
		if (d) {
			return d;
		}
		thread = thread->getParentThread();
	}

	return nullptr;
}

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_H_ */
