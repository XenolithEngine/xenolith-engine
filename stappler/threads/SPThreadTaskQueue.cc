/**
Copyright (c) 2019 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "SPThreadTaskQueue.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

Status TaskQueue::OutputContext::perform(Rc<thread::Task> &&task) {
	if (!task) {
		return Status::ErrorInvalidArguemnt;
	}

	task->addRef(queue);

	outputMutex.lock();
	outputQueue.push_back(sp::move(task));
	++outputCounter;
	outputMutex.unlock();

	if (wakeup) {
		wakeup();
	}

	outputCondition.notify_one();
	return Status::Ok;
}

Status TaskQueue::OutputContext::perform(mem_std::Function<void()> &&func, Ref *target,
		StringView) {
	outputMutex.lock();
	outputCallbacks.emplace_back(sp::move(func), target);
	++outputCounter;
	outputMutex.unlock();

	if (wakeup) {
		wakeup();
	}

	outputCondition.notify_one();
	return Status::Ok;
}

TaskQueue::~TaskQueue() {
	cancel();
	update(nullptr);
}

bool TaskQueue::init(TaskQueueInfo &&info) {
	if (!ThreadPool::init(ThreadPoolInfo{.flags = info.flags,
			.name = info.name,
			.threadCount = info.threadCount,
			.complete = &_outContext})) {
		return false;
	}

	_outContext.wakeup = sp::move(info.wakeup);
	_outContext.pool = Rc<PoolRef>::alloc();
	_outContext.queue = this;

	return true;
}

Status TaskQueue::performOnThread(Rc<Task> &&task) {
	return ThreadPool::performCompleted(move(task));
}

Status TaskQueue::performOnThread(mem_std::Function<void()> &&func, Ref *target) {
	return ThreadPool::performCompleted(sp::move(func), target);
}

void TaskQueue::update(uint32_t *count) {
	_outContext.outputMutex.lock();

	auto stack = sp::move(_outContext.outputQueue);
	auto callbacks = sp::move(_outContext.outputCallbacks);

	_outContext.outputQueue.clear();
	_outContext.outputCallbacks.clear();

	_outContext.outputCounter.store(0);

	_outContext.outputMutex.unlock();

	memory::perform_clear([&] {
		for (Rc<Task> &task : stack) {
			// launch all stages with run()
			task->run();
		}

		for (Pair<std::function<void()>, Rc<Ref>> &task : callbacks) { task.first(); }
	}, _outContext.pool->getPool());

	if (count) {
		*count += stack.size() + callbacks.size();
	}

	if (_context.tasksCounter.load() > 0) {
		_context.inputCondition.notify_all();
	}
}

size_t TaskQueue::getOutputCounter() const { return _outContext.outputCounter.load(); }

void TaskQueue::lock() { _context.inputMutexQueue.lock(); }

void TaskQueue::unlock() { _context.inputMutexQueue.unlock(); }

Status TaskQueue::waitForAll(TimeInterval iv) {
	update();
	while (_context.tasksCounter.load() != 0) { wait(iv, nullptr); }
	return Status::Ok;
}

Status TaskQueue::wait(uint32_t *count) {
	std::unique_lock<std::mutex> waitLock(_outContext.outputMutex);
	_outContext.outputCondition.wait(waitLock);
	update(count);
	return Status::Ok;
}

Status TaskQueue::wait(TimeInterval iv, uint32_t *count) {
	std::unique_lock<std::mutex> waitLock(_outContext.outputMutex);
	auto c = _outContext.outputCounter.load();
	if (c > 0) {
		waitLock.unlock();
		update(count);
		return Status::Ok;
	}
	auto ret =
			_outContext.outputCondition.wait_for(waitLock, std::chrono::microseconds(iv.toMicros()),
					[&, this] { return _outContext.outputCounter.load() > 0; });
	if (!ret) {
		if (count) {
			*count = 0;
		}
		return Status::Declined;
	} else {
		waitLock.unlock();
		update(count);
		return Status::Ok;
	}
}

} // namespace stappler::thread
