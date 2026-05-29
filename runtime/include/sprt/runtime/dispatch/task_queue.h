/**
Copyright (c) 2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TASK_QUEUE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TASK_QUEUE_H_

#include <sprt/runtime/dispatch/thread_pool.h>
#include <sprt/runtime/mem/pool_ref.h>

namespace sprt::dispatch {

class Worker;

struct SPRT_API TaskQueueInfo {
	ThreadPoolFlags flags = ThreadPoolFlags::None;
	StringView name;
	uint16_t threadCount;
	Function<void()> wakeup;
};

class SPRT_API TaskQueue : public ThreadPool {
public:
	virtual ~TaskQueue();

	bool init(TaskQueueInfo &&);

	Status performOnThread(Rc<Task> &&task);
	Status performOnThread(Function<void()> &&func, Ref *target);

	void update(uint32_t *count = nullptr);

	Status waitForAll(TimeInterval = TimeInterval::seconds(1));

	Status wait(uint32_t *count = nullptr);
	Status wait(TimeInterval, uint32_t *count = nullptr);

	size_t getOutputCounter() const;

	void lock();
	void unlock();

protected:
	__SPRT_PUSH_ALLOW_CXXABI_ALLOC
	struct SPRT_API OutputContext : PerformInterface {
		__SPRT_POP_ALLOW_CXXABI_ALLOC
		Rc<PoolRef> pool;
		TaskQueue *queue = nullptr;
		sprt::qmutex outputMutex;
		Vector<Rc<Task>> outputQueue;
		Vector<pair<Function<void()>, Rc<Ref>>> outputCallbacks;
		sprt::condition_variable outputCondition;
		sprt::atomic<size_t> outputCounter = 0;
		Function<void()> wakeup;

		virtual Status perform(Rc<Task> &&task) override;
		virtual Status perform(Function<void()> &&func, Ref *, StringView) override;
	};

	OutputContext _outContext;
};

} // namespace sprt::dispatch

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_DISPATCH_TASK_QUEUE_H_ */
