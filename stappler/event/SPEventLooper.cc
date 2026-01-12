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

#include "SPEventLooper.h"
#include "SPMemory.h"
#include "SPThread.h"
#include "SPEventThreadHandle.h"
#include "SPEventBus.h"

#include <sprt/runtime/thread/info.h>

namespace STAPPLER_VERSIONIZED stappler::event::platform {

Rc<QueueRef> getThreadQueue(QueueInfo &&);

}

namespace STAPPLER_VERSIONIZED stappler::event {

static thread_local Rc<Looper> tl_looper;

struct Looper::Data : public memory::AllocPool {
	thread::ThreadPoolInfo threadPoolInfo;

	Rc<QueueRef> queue;
	Rc<ThreadHandle> threadHandle;
	Rc<thread::ThreadPool> threadPool;
	const sprt::thread::info *threadInfo = nullptr;
	memory::pool_t *threadMemPool = nullptr;
	thread::Thread::Id thisThreadId;
	bool suspendThreadsOnWakeup = false;

	mem_std::Set<Bus *> buses;
	std::mutex *mutex = nullptr;

	thread::ThreadPool *getThreadPool() {
		if (!threadPool) {
			threadPool = Rc<thread::ThreadPool>::create(thread::ThreadPoolInfo(threadPoolInfo));
		}
		return threadPool;
	}

	static void cleanup(Data *d, Looper *l) {
		if (!d) {
			return;
		}

		// Looper cleanup can cause app finalization, so, prevent in with
		// initialize + terminate
		sprt::memory::pool::initialize();
		std::unique_lock lock(l->_mutex);

		d->queue->poll();

		auto tmp = sp::move(d->buses);
		d->buses.clear();

		for (auto &it : tmp) { it->invalidateLooper(l); }

		if (d->threadPool) {
			d->threadPool->cancel();
			d->threadPool = nullptr;
		}

		d->threadPoolInfo.ref = nullptr;

		if (d->threadHandle) {
			d->threadHandle->cancel();
			d->threadHandle = nullptr;
		}

		if (d->threadPool) {
			d->threadPool->cancel();
			d->threadPool = nullptr;
			log::source().debug("Looper", "Cleanup: ", sprt::thread::info::get()->name);
		}

		if (d->queue) {
			auto q = d->queue;
			d->queue = nullptr;
			q->cancel();
#if SPRT_REF_DEBUG
			if (q->getRef()->getReferenceCount() > 1) {
				auto tmp = q->getRef();
				q = nullptr;

				tmp->foreachBacktrace(
						[](uint64_t id, time_t t, const memory::forward_list<StringView> &vec) {
					mem_std::StringStream stream;
					stream << "[" << id << ":" << Time(t).toHttp<memory::StandartInterface>()
						   << "]:\n";
					for (auto &it : vec) { stream << "\t" << it << "\n"; }
					log::source().debug("event::Queue", stream.str());
				});
			} else {
				q = nullptr;
			}
#else
			q = nullptr;
#endif
		}
		lock.unlock();
		sprt::memory::pool::terminate();
	}

	void attachBus(Bus *bus) {
		std::unique_lock lock(*mutex);
		buses.emplace(bus);
	}

	void detachBus(Bus *bus) {
		std::unique_lock lock(*mutex);
		buses.erase(bus);
	}
};

Looper *Looper::acquire(LooperInfo &&info) {
	return acquire(move(info),
			QueueInfo{
				.flags = QueueFlags::SubmitImmediate | QueueFlags::ThreadNative,
				.engineMask = info.engineMask,
				.osIdleInterval = TimeInterval::milliseconds(100),
			});
}

Looper *Looper::acquire(LooperInfo &&info, QueueInfo &&qinfo) {
	if (tl_looper) {
		return tl_looper;
	}

	auto q = platform::getThreadQueue(move(qinfo));
	if (!q) {
		return nullptr;
	}

	tl_looper = Rc<Looper>::alloc(move(info), move(q));

	return tl_looper;
}

Looper *Looper::getIfExists() { return tl_looper; }

Looper::~Looper() {
	if (_data) {
		Data::cleanup(_data, this);
		_data = nullptr;
	}
}

Rc<TimerHandle> Looper::scheduleTimer(TimerInfo &&info, Ref *ref) {
	return _data->queue->scheduleTimer(move(info), ref);
}

Rc<Handle> Looper::schedule(TimeInterval timeout, mem_std::Function<void(Handle *, bool)> &&fn,
		Ref *ref) {
	return _data->queue->schedule(timeout, sp::move(fn), ref);
}

Rc<PollHandle> Looper::listenPollableHandle(NativeHandle fd, PollFlags flags,
		CompletionHandle<PollHandle> &&cb, Ref *ref) {
	return _data->queue->listenPollableHandle(fd, flags, move(cb), ref);
}

Rc<PollHandle> Looper::listenPollableHandle(NativeHandle fd, PollFlags flags,
		mem_std::Function<Status(NativeHandle, PollFlags)> &&cb, Ref *ref) {
	return _data->queue->listenPollableHandle(fd, flags, sp::move(cb), ref);
}

Status Looper::performOnThread(Rc<thread::Task> &&task, bool immediate) {
	bool isOnThread = isOnThisThread();
	if (immediate && isOnThread) {
		task->run();
		return Status::Ok;
	}
	if (!isOnThread || _data->queue->performNext(move(task)) == Status::Declined) {
		return _data->threadHandle->perform(move(task));
	} else {
		return Status::Ok;
	}
}

Status Looper::performOnThread(mem_std::Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) {
	bool isOnThread = isOnThisThread();
	if (immediate && isOnThread) {
		func();
		return Status::Ok;
	}
	if (!isOnThread || _data->queue->performNext(sp::move(func), target, tag) == Status::Declined) {
		return _data->threadHandle->perform(sp::move(func), target, tag);
	} else {
		return Status::Ok;
	}
}

Status Looper::performAsync(Rc<thread::Task> &&task, bool first) {
	return _data->getThreadPool()->perform(move(task), first);
}

Status Looper::performAsync(mem_std::Function<void()> &&func, Ref *target, bool first,
		StringView tag) {
	return _data->getThreadPool()->perform(sp::move(func), target, first, tag);
}

Status Looper::performHandle(Handle *h) { return _data->queue->runHandle(h); }

uint32_t Looper::poll() { return _data->queue->poll(); }

uint32_t Looper::wait(TimeInterval ival) { return _data->queue->wait(ival); }

Status Looper::run(TimeInterval ival, QueueWakeupInfo &&info) {
	_data->threadHandle->wakeup();

	auto ret = _data->queue->run(ival, move(info));

	if (_data->suspendThreadsOnWakeup) {
		_data->suspendThreadsOnWakeup = false;

		if (_data->threadPool) {
			_data->threadPool->cancel();
			_data->threadPool = nullptr;
		}
	}

	return ret;
}

Status Looper::wakeup(WakeupFlags flags) {
	if (hasFlag(flags, WakeupFlags::SuspendThreads)) {
		_data->suspendThreadsOnWakeup = true;
	}
	return _data->queue->wakeup(flags);
}

uint16_t Looper::getWorkersCount() const { return _data->threadPool->getInfo().threadCount; }

memory::pool_t *Looper::getThreadMemPool() const { return _data->threadMemPool; }

const event::Queue *Looper::getQueue() const { return _data->queue; }

thread::ThreadPool *Looper::getThreadPool() const { return _data->getThreadPool(); }

bool Looper::isOnThisThread() const {
	return _data->thisThreadId == thread::Thread::getCurrentThreadId();
}

Looper::Looper(LooperInfo &&info, Rc<QueueRef> &&q) {
	auto pool = q->getPool();
	mem_pool::perform([&] {
		_data = new (pool) Data;
		_data->mutex = &_mutex;
		_data->queue = move(q);
		_data->threadHandle = _data->queue->addThreadHandle();
		_data->threadPoolInfo = thread::ThreadPoolInfo{
			.flags = info.workersFlags,
			.name = info.name,
			.threadCount = info.workersCount,
			.complete = _data->threadHandle.get(),
			.ref = _data->threadHandle,
		};

		_data->threadPoolInfo.name = StringView(mem_pool::toString(info.name, ":Worker")).pdup();

		sprt::thread::info::set(info.name);

		_data->threadInfo = sprt::thread::info::get();
		if (!_data->threadInfo || !_data->threadInfo->threadPool) {
			_data->threadMemPool = pool;
		} else {
			_data->threadMemPool = _data->threadInfo->threadPool;
			memory::pool::cleanup_register(_data->threadMemPool, this, [](void *d) -> Status {
				auto l = (Looper *)d;
				l->_active = false;
				Data::cleanup(l->_data, l);
				l->_data = nullptr;
				tl_looper = nullptr;
				return Status::Ok;
			});
		}

		_data->thisThreadId = thread::Thread::getCurrentThreadId();
		_active = true;
	}, pool);
}

void Looper::attachBus(Bus *bus) {
	if (_data) {
		_data->attachBus(bus);
	}
}

void Looper::detachBus(Bus *bus) {
	if (_data) {
		_data->detachBus(bus);
	}
}

} // namespace stappler::event
