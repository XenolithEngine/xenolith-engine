/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include "XLLooperAdapter.h"
#include "SPEventTimerHandle.h"
#include "SPEventPollHandle.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool HandleAdapter::init(Type t, void *p, Fn fn) {
	_type = t;
	_ptr = p;
	_fn = fn;
	return true;
}

bool HandleAdapter::init(Type t, Rc<event::Handle> &&h) {
	_type = t;
	_handle = sp::move(h);
	return true;
}

void HandleAdapter::setHandle(Rc<event::Handle> &&h) { _handle = sp::move(h); }

void HandleAdapter::forward(uint32_t flags, Status status) {
	if (_ptr && _fn) {
		_fn(_ptr, this, flags, status);
	}
}

Status HandleAdapter::getStatus() const {
	if (_handle) {
		return _handle->getStatus();
	}
	return Status::ErrorNotFound;
}

Status HandleAdapter::resume() {
	if (_handle) {
		return _handle->resume();
	}
	return Status::ErrorNotFound;
}
Status HandleAdapter::pause() {
	if (_handle) {
		return _handle->pause();
	}
	return Status::ErrorNotFound;
}

Status HandleAdapter::cancel(Status st, uint32_t value) {
	if (_handle) {
		return _handle->cancel(st, value);
	}
	return Status::ErrorNotFound;
}

void HandleAdapter::setUserdata(Ref *ref) {
	if (_handle) {
		_handle->setUserdata(ref);
	}
}
Ref *HandleAdapter::getUserdata() const {
	if (_handle) {
		return _handle->getUserdata();
	}
	return nullptr;
}

bool HandleAdapter::resetPoll(filesystem::PollFlags flags) {
	if (_handle && _type == Poll) {
		return _handle.get_cast<event::PollHandle>()->reset(flags);
	}
	return false;
}

bool HandleAdapter::resetTimer(time_t timeout, time_t interval, uint32_t count) {
	if (_handle && _type == Timer) {
		return _handle.get_cast<event::TimerHandle>()->reset(event::TimerInfo{
			.timeout = TimeInterval::microseconds(timeout),
			.interval = TimeInterval::microseconds(interval),
			.count = count,
		});
	}
	return false;
}

bool LooperAdapter::init(NotNull<event::Looper> l) {
	_looper = l;

	setForThread(this);

	return true;
}

void LooperAdapter::poll() { _looper->poll(); }

Status LooperAdapter::wakeup(bool graceful) {
	return _looper->wakeup(graceful ? event::WakeupFlags::Graceful : event::WakeupFlags::None);
}

Status LooperAdapter::run() { return _looper->run(); }

Rc<sprt::window::HandleAdapter> LooperAdapter::scheduleTimer(time_t timeout, time_t interval,
		uint32_t count, void *ptr,
		void (*fn)(void *, sprt::window::HandleAdapter *, uint32_t flags, Status status)) {

	auto ha = Rc<HandleAdapter>::create(HandleAdapter::Timer, ptr, fn);

	ha->setHandle(_looper->scheduleTimer(event::TimerInfo{
		.completion =
				event::TimerInfo::Completion{
					[](void *ptr, event::TimerHandle *h, uint32_t flags, Status st) {
		auto ha = static_cast<HandleAdapter *>(ptr);
		if (ha) {
			ha->forward(flags, st);
		}
	}, ha.get()},
		.timeout = TimeInterval::microseconds(timeout),
		.interval = TimeInterval::microseconds(interval),
		.count = count,
	}));
	return ha;
}

Rc<sprt::window::HandleAdapter> LooperAdapter::listenPollableHandle(int fd,
		filesystem::PollFlags flags, void *ptr,
		void (*fn)(void *, sprt::window::HandleAdapter *, uint32_t flags, Status status)) {
	auto ha = Rc<HandleAdapter>::create(HandleAdapter::Poll, ptr, fn);

	ha->setHandle(_looper->listenPollableHandle(fd, flags,
			event::CompletionHandle<event::PollHandle>{
				[](void *ptr, event::PollHandle *h, uint32_t flags, Status st) {
		auto ha = static_cast<HandleAdapter *>(ptr);
		if (ha) {
			ha->forward(flags, st);
		}
	}, ha.get()}));
	return ha;
}

Rc<sprt::window::HandleAdapter> LooperAdapter::listenPollableHandle(int fd,
		filesystem::PollFlags flags,
		sprt::memory::dynfunction<Status(int fd, filesystem::PollFlags flags)> &&cb, Ref *ref) {
	return Rc<HandleAdapter>::create(HandleAdapter::Poll,
			_looper->listenPollableHandle(fd, flags, sp::move(cb), ref));
}

Status LooperAdapter::performOnThread(sprt::memory::dynfunction<void()> &&func, Ref *target,
		bool immediate, StringView tag) const {
	return _looper->performOnThread(sp::move(func), target, immediate, tag);
}

} // namespace stappler::xenolith
