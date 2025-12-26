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

#include "SPEventQueue.h"
#include "SPEventTimerHandle.h"
#include "SPEventPollHandle.h"

namespace STAPPLER_VERSIONIZED stappler::event {

Rc<SharedRef<Queue>> Queue::create(QueueInfo &&info) {
	return Rc<QueueRef>::create(SharedRefMode::Allocator, move(info));
}

Queue::~Queue() {
	if (_data) {
		delete _data;
		_data = nullptr;
	}
}

bool Queue::init(const QueueInfo &info) {
	mem_pool::perform([&] {
		_data = new (getPool()) Data(static_cast<QueueRef *>(getRef()), info);
	}, getPool());
	return _data != nullptr && _data->isValid();
}

Rc<TimerHandle> Queue::scheduleTimer(TimerInfo &&info, Ref *ref) {
	if (info.count == 0 || (!info.interval && !info.timeout)) {
		log::source().error("event::Queue", "Invalid parameters for timer");
		return nullptr;
	}

	auto h = _data->scheduleTimer(move(info));
	h->setUserdata(ref);
	_data->runHandle(h);
	return h;
}

Rc<Handle> Queue::schedule(TimeInterval timeout, mem_std::Function<void(Handle *, bool)> &&fn,
		Ref *ref) {
	struct ScheduleData : Ref {
		mem_std::Function<void(Handle *, bool)> fn;
		Rc<Ref> ref;
	};

	auto data = Rc<ScheduleData>::alloc();
	data->fn = sp::move(fn);
	data->ref = ref;

	return scheduleTimer(
			TimerInfo{
				.completion = TimerInfo::Completion::create<ScheduleData>(data,
						[](ScheduleData *data, TimerHandle *handle, uint32_t value, Status status) {
		if (data->fn) {
			if (status == Status::Done) {
				data->fn(handle, true);
			} else if (!isSuccessful(status)) {
				data->fn(handle, false);
			}
		}
	}),
				.timeout = timeout,
				.interval = TimeInterval(),
				.count = 1,
			},
			data);
}

Rc<PollHandle> Queue::listenPollableHandle(NativeHandle handle, PollFlags flags,
		CompletionHandle<PollHandle> &&cb, Ref *ref) {
	Rc<PollHandle> h = _data->listenHandle(handle, flags, move(cb));
	if (h) {
		h->setUserdata(ref);
	}
	_data->runHandle(h);
	return h;
}

// Uses Handle userdata slot for a private data
Rc<PollHandle> Queue::listenPollableHandle(NativeHandle handle, PollFlags flags,
		mem_std::Function<Status(NativeHandle fd, PollFlags)> &&cb, Ref *ref) {
	struct PollData : public Ref {
		NativeHandle handle;
		mem_std::Function<Status(NativeHandle fd, PollFlags)> cb;
		Rc<Ref> ref;
	};

	auto data = Rc<PollData>::alloc();
	data->handle = handle;
	data->cb = sp::move(cb);
	data->ref = ref;

	auto h = listenPollableHandle(handle, flags,
			CompletionHandle<Handle>::create<PollData>(data,
					[](PollData *data, Handle *handle, uint32_t value, Status st) {
		if (st == Status::Ok) {
			if (data->cb(data->handle, PollFlags(value)) != Status::Ok) {
				handle->cancel();
			}
		}
	}));
	if (h) {
		h->setUserdata(data);
	}
	return h;
}

Rc<ThreadHandle> Queue::addThreadHandle() {
	auto h = _data->addThreadHandle();
	_data->runHandle(h);
	return h;
}

/*Rc<DirHandle> Queue::openDir(OpenDirInfo &&info) {
	Rc<DirHandle> h = _data->openDir(move(info));
	if (!info.file.root || info.file.root->getStatus() == Status::Done) {
		_data->runHandle(h);
	} else {
		if (!isSuccessful(info.file.root->addPending(h))) {
			h = nullptr;
		}
	}
	return h;
}

Rc<StatHandle> Queue::stat(StatOpInfo &&info) {
	Rc<StatHandle> h = _data->stat(move(info));
	if (!info.file.root || info.file.root->getStatus() == Status::Done) {
		_data->runHandle(h);
	} else {
		if (!isSuccessful(info.file.root->addPending(h))) {
			h = nullptr;
		}
	}
	return h;
}

Rc<OpHandle> Queue::read(CompletionHandle<void> &&, InputOutputHandle *, BufferChain *, uint32_t, uint32_t) {
	return nullptr;
}
Rc<OpHandle> Queue::write(CompletionHandle<void> &&, InputOutputHandle *, BufferChain *, uint32_t, uint32_t) {
	return nullptr;
}*/

Status Queue::runHandle(Handle *h) {
	if (h->getStatus() != Status::Declined) {
		return Status::ErrorAlreadyPerformed;
	}

	return _data->runHandle(h);
}

Status Queue::submitPending() {
	_data->resumeAll();
	return _data->submit();
}

// non-blocking poll
uint32_t Queue::poll() {
	return mem_pool::perform([&] {
		_data->resumeAll();
		_data->submit();
		return _data->poll();
	}, getPool());
}

// wait until next event or timeout
uint32_t Queue::wait(TimeInterval ival) {
	return mem_pool::perform([&] {
		_data->resumeAll();
		_data->submit();
		return _data->wait(ival);
	}, getPool());
}

// run for some time
Status Queue::run(TimeInterval ival, QueueWakeupInfo &&info) {
	return mem_pool::perform([&] {
		_data->resumeAll();
		_data->submit();
		return _data->run(ival, move(info));
	}, getPool());
}

Status Queue::wakeup(WakeupFlags flags) { return _data->wakeup(flags); }

void Queue::cancel() {
	_data->cancel();
	delete _data;
	_data = nullptr;
}

QueueFlags Queue::getFlags() const { return _data->_flags; }

QueueEngine Queue::getEngine() const { return _data->_engine; }

NativeHandle Queue::getHandle() const { return _data->_handle; }

Status Queue::performNext(Rc<thread::Task> &&task) { return _data->perform(move(task)); }

Status Queue::performNext(mem_std::Function<void()> &&fn, Ref *ref, StringView tag) {
	return _data->perform(sp::move(fn), ref, tag);
}

} // namespace stappler::event
