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

#include "SPEventThreadHandle-alooper.h"
#include "SPEvent-alooper.h"

#include <sprt/c/sys/__sprt_eventfd.h>

namespace STAPPLER_VERSIONIZED stappler::event {

bool ThreadALooperHandle::init(HandleClass *cl) {
	if (!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = reinterpret_cast<EventFdSource *>(_data);
	return source->init();
}

Status ThreadALooperHandle::read() {
	auto source = reinterpret_cast<EventFdSource *>(_data);
	auto ret = ::__sprt_eventfd_read(source->fd, &source->eventTarget);
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status ThreadALooperHandle::write(uint64_t val) {
	auto source = reinterpret_cast<EventFdSource *>(_data);
	auto ret = ::__sprt_eventfd_write(source->fd, val);
	if (ret < 0) {
		return sprt::status::errnoToStatus(errno);
	}
	return Status::Ok;
}

Status ThreadALooperHandle::rearm(ALooperData *alooper, EventFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = alooper->add(source->fd, __SPRT_ALOOPER_EVENT_INPUT, this);
	}
	return status;
}

Status ThreadALooperHandle::disarm(ALooperData *alooper, EventFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ThreadALooperHandle::notify(ALooperData *alooper, EventFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.queueFlags & __SPRT_ALOOPER_EVENT_INPUT) {
		while (read() == Status::Ok) {
			_mutex.lock();

			performAll([&](uint32_t count) { _mutex.unlock(); });
		}
	}

	if ((data.queueFlags & __SPRT_ALOOPER_EVENT_ERROR)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_INVALID)) {
		cancel();
	}
}

Status ThreadALooperHandle::perform(Rc<thread::Task> &&task) {
	std::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

Status ThreadALooperHandle::perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) {
	std::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});

	uint64_t value = 1;
	::__sprt_eventfd_write(reinterpret_cast<EventFdSource *>(_data)->fd, value);
	return Status::Ok;
}

} // namespace stappler::event
