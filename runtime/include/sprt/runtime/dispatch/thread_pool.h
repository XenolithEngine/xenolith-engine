/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_POOL_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_POOL_H_

#include <sprt/runtime/dispatch/task.h>
#include <sprt/cxx/condition_variable>

namespace sprt::dispatch {

class SPRT_API PerformInterface {
public:
	virtual ~PerformInterface() = default;

	// Perform Task's complete functions on this event queue
	virtual Status perform(Rc<Task> &&task) { return Status::ErrorNotImplemented; }

	// Perform function on this event queue
	virtual Status perform(Function<void()> &&func, Ref * = nullptr, StringView tag = __SPRT_FUNC) {
		return Status::ErrorNotImplemented;
	}
};

enum class ThreadPoolFlags : uint32_t {
	None,
	LazyInit = 1 << 0, // do not spawn threads unless some task is performed
};

SPRT_DEFINE_ENUM_AS_MASK(ThreadPoolFlags);

struct SPRT_API ThreadPoolInfo {
	ThreadPoolFlags flags = ThreadPoolFlags::None;
	StringView name;
	uint16_t threadCount = sprt::thread::hardware_concurrency();
	PerformInterface *complete = nullptr;
	Rc<Ref> ref; // reference to store interface
};

class SPRT_API ThreadPool : public Ref {
public:
	virtual ~ThreadPool() = default;

	bool init(ThreadPoolInfo &&);

	Status perform(Rc<Task> &&task, bool first = false);
	Status perform(Function<void()> &&, Ref * = nullptr, bool first = false,
			StringView tag = StringView());

	Status performCompleted(Rc<Task> &&task);
	Status performCompleted(Function<void()> &&func, Ref * = nullptr);

	// stop all workers
	void cancel();

	// check if workers are running (return true when lazy-init is waiting)
	bool isRunning() const;

	const ThreadPoolInfo &getInfo() const;

	StringView getName() const;

protected:
	class Worker;

	struct SPRT_API WorkerContext {
		ThreadPoolInfo info;
		ThreadPool *threadPool = nullptr;

		sprt::atomic<bool> finalized;
		sprt::atomic<size_t> tasksInExecution = 0;
		sprt::atomic<size_t> tasksInQueue = 0;

		Vector<Worker *> workers;

		sprt::qmutex inputMutexQueue;
		sprt::qmutex inputMutexFree;
		PriorityQueue<Rc<Task>> inputQueue;
		sprt::condition_variable inputCondition;

		WorkerContext();
		~WorkerContext();

		bool init(ThreadPoolInfo &&i, ThreadPool *p);

		void wait(sprt::unique_lock<sprt::qmutex> &lock);
		void finalize();
		void spawn();
		void cancel();

		Status perform(Rc<Task> &&task, bool first);
		void onMainThreadWorker(Rc<Task> &&task);

		Rc<Task> popTask();
	};

	WorkerContext _context;
};

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_THREAD_POOL_H_ */
