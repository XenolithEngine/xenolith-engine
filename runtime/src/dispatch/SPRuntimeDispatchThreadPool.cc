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

#include <sprt/runtime/dispatch/thread_pool.h>
#include <sprt/runtime/dispatch/thread_info.h>
#include <sprt/runtime/dispatch/thread.h>
#include <sprt/runtime/log.h>

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>

namespace sprt::dispatch {

// SP_DISPATCH_AFFINITY: the cores the pool workers are pinned to, as a list
// with ranges ("1-3", "0,2,3"). Worker i of every pool goes to core list[i % n].
// Unset, empty or "off": nobody is pinned, the scheduler places them (BF-44).
static constexpr uint32_t DispatchAffinityMax = 64;
static constexpr uint32_t DispatchAffinityCoreLimit = 1'024; // CPU_SETSIZE

// Whether the bad value has been reported: every worker parses the variable,
// one of them says what was wrong with it.
static int s_dispatchAffinityReported = 0;

static bool ThreadPool_readUint(const char *&p, uint32_t &out) {
	if (*p < '0' || *p > '9') {
		return false;
	}
	uint32_t v = 0;
	while (*p >= '0' && *p <= '9') {
		v = v * 10 + uint32_t(*p - '0');
		if (v >= DispatchAffinityCoreLimit) {
			return false;
		}
		++p;
	}
	out = v;
	return true;
}

// The list, or 0 when nothing is to be pinned -- including when the value is
// wrong, which is then said once, loudly: a typo must not look like "pinning
// makes no difference".
static uint32_t ThreadPool_readAffinity(uint32_t (&cores)[DispatchAffinityMax]) {
	auto value = ::getenv("SP_DISPATCH_AFFINITY");
	if (!value || !*value || ::strcmp(value, "off") == 0) {
		return 0;
	}

	uint32_t n = 0;
	const char *p = value;
	while (true) {
		uint32_t first = 0, last = 0;
		if (!ThreadPool_readUint(p, first)) {
			break;
		}
		last = first;
		if (*p == '-') {
			++p;
			if (!ThreadPool_readUint(p, last) || last < first) {
				break;
			}
		}
		for (auto c = first; c <= last && n < DispatchAffinityMax; ++c) { cores[n++] = c; }
		if (*p == 0) {
			return n;
		}
		if (*p != ',') {
			break;
		}
		++p;
	}

	if (__atomic_exchange_n(&s_dispatchAffinityReported, 1, __ATOMIC_RELAXED) == 0) {
		oslog::vperror(__SPRT_LOCATION, "dispatch::ThreadPool", "SP_DISPATCH_AFFINITY=",
				StringView(value),
				" is not a list of cores like 1-3 or 0,2,3; no worker is pinned");
	}
	return 0;
}

// Runs on the worker itself, before its first task.
static void ThreadPool_pinWorker(StringView name, uint32_t workerId) {
	uint32_t cores[DispatchAffinityMax];
	auto n = ThreadPool_readAffinity(cores);
	if (n == 0) {
		return;
	}

	auto core = cores[workerId % n];
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(core, &set);
	auto ret = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
	if (ret == 0) {
		// Read back, so the line says what the kernel keeps, not what was asked.
		CPU_ZERO(&set);
		pthread_getaffinity_np(pthread_self(), sizeof(set), &set);
		oslog::vpinfo(__SPRT_LOCATION, "dispatch::ThreadPool", name, " worker ", workerId,
				": pinned to core ", core, ", mask now ", uint64_t(set.__bits[0]));
	} else {
		oslog::vperror(__SPRT_LOCATION, "dispatch::ThreadPool", name, " worker ", workerId,
				": pinning to core ", core, " failed: ", ret);
	}
}

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

Status ThreadPool::perform(Function<void()> &&cb, Ref *ref, bool first, StringView tag) {
	return perform(Rc<Task>::create(
						   [fn = sprt::move(cb)](const Task &) -> bool {
		fn();
		return true;
	}, nullptr, ref, nullptr, tag),
			first);
}

Status ThreadPool::performCompleted(Rc<Task> &&task) {
	return _context.info.complete->perform(move(task));
}

Status ThreadPool::performCompleted(Function<void()> &&func, Ref *target) {
	return _context.info.complete->perform(sprt::move(func), target);
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
	_queueRefId = sprt::retain(_queue->threadPool);
}

ThreadPool::Worker::~Worker() { sprt::release(_queue->threadPool, _queueRefId); }

void ThreadPool::Worker::threadInit() {
	sprt::dispatch::thread_info::set(_name, _workerId, true);
	ThreadPool_pinWorker(_name, _workerId);
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
		sprt::unique_lock<sprt::mutex> lock(_queue->inputMutexQueue);
		if (_queue->tasksInQueue.load() > 0) {
			// some task received after locking
			return true;
		}
		_queue->wait(lock);
		return true;
	}

	--_queue->tasksInQueue;
	task->execute();

	_queue->onMainThreadWorker(sprt::move(task));

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

void ThreadPool::WorkerContext::wait(sprt::unique_lock<sprt::qmutex> &lock) {
	if (finalized.load() != true) {
		inputCondition.wait(lock);
	}
}

void ThreadPool::WorkerContext::spawn() {
	sprt::unique_lock lock(inputMutexQueue);
	if (workers.empty()) {
		for (uint32_t i = 0; i < info.threadCount; i++) {
			auto worker = RefAlloc::__new<Worker>(this, info.name, i);
			workers.push_back(worker);
			worker->run();
		}

		// remove lazy-init flag to prevent run-after-cancel
		info.flags &= ~ThreadPoolFlags::LazyInit;
	}
}

void ThreadPool::WorkerContext::cancel() {
	if (!workers.empty()) {
		for (auto &it : workers) { it->stop(); }

		{
			// Publish `finalized` and wake waiters under `inputMutexQueue`, per the
			// qcondvar contract (the mutex must be held while signalling, otherwise a
			// worker that has not yet published its wait state misses the wakeup and
			// blocks forever). Release the lock before joining, since the workers
			// re-acquire `inputMutexQueue` on their way out of wait().
			sprt::unique_lock lock(inputMutexQueue);
			finalized = true;
			inputCondition.notify_all();
		}

		for (auto &it : workers) {
			it->waitStopped();
			__delete(it);
		}
		workers.clear();
	} else {
		finalized = true;
	}

	inputQueue.foreach ([&](PriorityQueue<Rc<Task>>::PriorityType p, const Rc<Task> &t) {
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

	++tasksInExecution;
	++tasksInQueue;
	inputQueue.push(task->getPriority().get(), first, sprt::move(task));

	{
		sprt::unique_lock lock(inputMutexQueue);
		inputCondition.notify_one();
	}
	return Status::Ok;
}

void ThreadPool::WorkerContext::onMainThreadWorker(Rc<Task> &&task) {
	if (!task) {
		return;
	}

	info.complete->perform(move(task));
	--tasksInExecution;
}

Rc<Task> ThreadPool::WorkerContext::popTask() {
	Rc<Task> ret;
	inputQueue.pop_direct(
			[&](PriorityQueue<Rc<Task>>::PriorityType, Rc<Task> &&task) { ret = move(task); });
	return ret;
}

} // namespace sprt::dispatch
