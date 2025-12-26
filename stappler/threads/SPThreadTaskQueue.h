/**
Copyright (c) 2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_THREADS_SPTHREADTASKQUEUE_H_
#define STAPPLER_THREADS_SPTHREADTASKQUEUE_H_

#include "SPThreadTask.h"
#include "SPThreadPool.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

class Worker;

struct SP_PUBLIC TaskQueueInfo {
	ThreadPoolFlags flags = ThreadPoolFlags::None;
	StringView name;
	uint16_t threadCount;
	mem_std::Function<void()> wakeup;
};

class SP_PUBLIC TaskQueue : public ThreadPool {
public:
	virtual ~TaskQueue();

	bool init(TaskQueueInfo &&);

	Status performOnThread(Rc<Task> &&task);
	Status performOnThread(mem_std::Function<void()> &&func, Ref *target);

	void update(uint32_t *count = nullptr);

	Status waitForAll(TimeInterval = TimeInterval::seconds(1));

	Status wait(uint32_t *count = nullptr);
	Status wait(TimeInterval, uint32_t *count = nullptr);

	size_t getOutputCounter() const;

	void lock();
	void unlock();

protected:
	struct SP_PUBLIC OutputContext : PerformInterface {
		Rc<PoolRef> pool;
		TaskQueue *queue = nullptr;
		std::mutex outputMutex;
		mem_std::Vector<Rc<Task>> outputQueue;
		mem_std::Vector<Pair<std::function<void()>, Rc<Ref>>> outputCallbacks;
		std::condition_variable outputCondition;
		std::atomic<size_t> outputCounter = 0;
		mem_std::Function<void()> wakeup;

		virtual Status perform(Rc<thread::Task> &&task) override;
		virtual Status perform(mem_std::Function<void()> &&func, Ref *, StringView) override;
	};

	OutputContext _outContext;
};

} // namespace stappler::thread

#endif /* STAPPLER_THREADS_SPTHREADTASKQUEUE_H_ */
