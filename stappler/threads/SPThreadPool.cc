/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPThreadPool.h"
#include "SPThread.h"

namespace STAPPLER_VERSIONIZED stappler::thread {

class ThreadPool::Worker : public Thread {
public:
	Worker(ThreadPool::WorkerContext *queue, StringView name, uint32_t workerId);
	virtual ~Worker();

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

protected:
	uint64_t _queueRefId = 0;
	ThreadPool::WorkerContext *_queue = nullptr;

	uint32_t _workerId;
	StringView _name;
};

bool ThreadPool::init(ThreadPoolInfo &&info) { return _context.init(move(info), this); }

Status ThreadPool::perform(Rc<Task> &&task, bool first) {
	if (!task) {
		return Status::ErrorInvalidArguemnt;
	}

	return _context.perform(move(task), first);
}

Status ThreadPool::perform(mem_std::Function<void()> &&cb, Ref *ref, bool first, StringView tag) {
	return perform(Rc<Task>::create(
						   [fn = sp::move(cb)](const Task &) -> bool {
		fn();
		return true;
	}, nullptr, ref, nullptr, tag),
			first);
}

Status ThreadPool::performCompleted(Rc<Task> &&task) {
	return _context.info.complete->perform(move(task));
}

Status ThreadPool::performCompleted(mem_std::Function<void()> &&func, Ref *target) {
	return _context.info.complete->perform(sp::move(func), target);
}

void ThreadPool::cancel() { _context.cancel(); }

bool ThreadPool::isRunning() const {
	return !_context.finalized.load()
			&& (hasFlag(_context.info.flags, ThreadPoolFlags::LazyInit)
					|| !_context.workers.empty());
}

const ThreadPoolInfo &ThreadPool::getInfo() const { return _context.info; }

StringView ThreadPool::getName() const { return _context.info.name; }

ThreadPool::Worker::Worker(ThreadPool::WorkerContext *queue, StringView name, uint32_t workerId)
: _queue(queue), _workerId(workerId), _name(name) {
	_queueRefId = _queue->threadPool->retain();
}

ThreadPool::Worker::~Worker() { _queue->threadPool->release(_queueRefId); }

void ThreadPool::Worker::threadInit() {
	ThreadInfo::setThreadInfo(_name, _workerId, true);
	Thread::threadInit();
}

void ThreadPool::Worker::threadDispose() { Thread::threadDispose(); }

bool ThreadPool::Worker::worker() {
	if (!_continueExecution.test_and_set()) {
		return false;
	}

	Rc<Task> task;

	if (!task) {
		task = _queue->popTask();
	}

	if (!task) {
		std::unique_lock<std::mutex> lock(_queue->inputMutexQueue);
		if (_queue->tasksCounter.load() > 0) {
			// some task received after locking
			return true;
		}
		_queue->wait(lock);
		return true;
	}

	task->execute();

	_queue->onMainThreadWorker(sp::move(task));

	return true;
}

ThreadPool::WorkerContext::WorkerContext() { }

ThreadPool::WorkerContext::~WorkerContext() { cancel(); }

bool ThreadPool::WorkerContext::init(ThreadPoolInfo &&i, ThreadPool *p) {
	info = move(i);
	threadPool = p;
	finalized = false;

	inputQueue.setQueueLocking(inputMutexQueue);
	inputQueue.setFreeLocking(inputMutexFree);

	if (!hasFlag(info.flags, ThreadPoolFlags::LazyInit)) {
		spawn();
	}
	return true;
}

void ThreadPool::WorkerContext::wait(std::unique_lock<std::mutex> &lock) {
	if (finalized.load() != true) {
		inputCondition.wait(lock);
	}
}

void ThreadPool::WorkerContext::spawn() {
	std::unique_lock lock(inputMutexQueue);
	if (workers.empty()) {
		for (uint32_t i = 0; i < info.threadCount; i++) {
			auto worker = new (std::nothrow) Worker(this, info.name, i);
			workers.push_back(worker);
			worker->run();
		}

		// remove lazy-init flag to prevent run-after-cancel
		info.flags &= ~ThreadPoolFlags::LazyInit;
	}
}

void ThreadPool::WorkerContext::cancel() {
	finalized = true;

	if (!workers.empty()) {
		for (auto &it : workers) { it->stop(); }

		inputCondition.notify_all();

		for (auto &it : workers) {
			it->waitStopped();
			delete it;
		}
		workers.clear();
	}

	inputQueue.foreach ([&](memory::PriorityQueue<Rc<Task>>::PriorityType p, const Rc<Task> &t) {
		if (t) {
			t->cancel();
		}
	});
	inputQueue.clear();

	info.complete = nullptr;
	info.ref = nullptr;
}

Status ThreadPool::WorkerContext::perform(Rc<Task> &&task, bool first) {
	if (finalized) {
		return Status::ErrorInvalidArguemnt;
	}

	if (hasFlag(info.flags, ThreadPoolFlags::LazyInit) && workers.empty()) {
		spawn();
	}

	if (workers.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!task->prepare()) {
		info.complete->perform(move(task));
		return Status::Declined;
	}

	task->addRef(threadPool);

	++tasksCounter;
	inputQueue.push(task->getPriority().get(), first, sp::move(task));
	inputCondition.notify_one();
	return Status::Ok;
}

void ThreadPool::WorkerContext::onMainThreadWorker(Rc<Task> &&task) {
	if (!task) {
		return;
	}

	info.complete->perform(move(task));
	--tasksCounter;
}

Rc<Task> ThreadPool::WorkerContext::popTask() {
	Rc<Task> ret;
	inputQueue.pop_direct([&](memory::PriorityQueue<Rc<Task>>::PriorityType, Rc<Task> &&task) {
		ret = move(task);
	});
	return ret;
}

} // namespace stappler::thread
