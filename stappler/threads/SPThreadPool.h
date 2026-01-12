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

#ifndef CORE_THREADS_SPTHREADPOOL_H_
#define CORE_THREADS_SPTHREADPOOL_H_

#include "SPThreadTask.h"
#include "SPStatus.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

class SP_PUBLIC PerformInterface {
public:
	virtual ~PerformInterface() = default;

	// Perform Task's complete functions on this event queue
	virtual Status perform(Rc<thread::Task> &&task) { return Status::ErrorNotImplemented; }

	// Perform function on this event queue
	virtual Status perform(mem_std::Function<void()> &&func, Ref * = nullptr,
			StringView tag = SP_FUNC) {
		return Status::ErrorNotImplemented;
	}
};

enum class ThreadPoolFlags : uint32_t {
	None,
	LazyInit = 1 << 0, // do not spawn threads unless some task is performed
};

SP_DEFINE_ENUM_AS_MASK(ThreadPoolFlags);

struct SP_PUBLIC ThreadPoolInfo {
	ThreadPoolFlags flags = ThreadPoolFlags::None;
	StringView name;
	uint16_t threadCount = std::thread::hardware_concurrency();
	PerformInterface *complete = nullptr;
	Rc<Ref> ref; // reference to store interface
};

class SP_PUBLIC ThreadPool : public Ref {
public:
	virtual ~ThreadPool() = default;

	bool init(ThreadPoolInfo &&);

	Status perform(Rc<Task> &&task, bool first = false);
	Status perform(mem_std::Function<void()> &&, Ref * = nullptr, bool first = false,
			StringView tag = StringView());

	Status performCompleted(Rc<Task> &&task);
	Status performCompleted(mem_std::Function<void()> &&func, Ref * = nullptr);

	// stop all workers
	void cancel();

	// check if workers are running (return true when lazy-init is waiting)
	bool isRunning() const;

	const ThreadPoolInfo &getInfo() const;

	StringView getName() const;

protected:
	class Worker;

	struct SP_PUBLIC WorkerContext {
		ThreadPoolInfo info;
		ThreadPool *threadPool = nullptr;

		std::atomic<bool> finalized;
		std::atomic<size_t> tasksInExecution = 0;
		std::atomic<size_t> tasksInQueue = 0;

		mem_std::Vector<Worker *> workers;

		std::mutex inputMutexQueue;
		std::mutex inputMutexFree;
		memory::PriorityQueue<Rc<Task>> inputQueue;
		std::condition_variable inputCondition;

		WorkerContext();
		~WorkerContext();

		bool init(ThreadPoolInfo &&i, ThreadPool *p);

		void wait(std::unique_lock<std::mutex> &lock);
		void finalize();
		void spawn();
		void cancel();

		Status perform(Rc<Task> &&task, bool first);
		void onMainThreadWorker(Rc<Task> &&task);

		Rc<Task> popTask();
	};

	WorkerContext _context;
};

} // namespace stappler::thread

#endif /* CORE_THREADS_SPTHREADPOOL_H_ */
