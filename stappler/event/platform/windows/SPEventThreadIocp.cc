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

#include "SPEventThreadIocp.h"
#include "SPStatus.h"
#include "detail/SPEventHandleClass.h"
#include "platform/windows/SPEvent-iocp.h"
#include <processthreadsapi.h>

namespace STAPPLER_VERSIONIZED stappler::event {

bool ThreadIocpSource::init() {
	currentThread = GetCurrentThread();
	return true;
}

void ThreadIocpSource::cancel() {
	currentThread = nullptr;
	port = nullptr;
}

bool ThreadIocpHandle::init(HandleClass *cl) {
	static_assert(sizeof(ThreadIocpSource) <= DataSize && std::is_standard_layout<ThreadIocpSource>::value);

	if (!ThreadHandle::init(cl)) {
		return false;
	}

	auto source = new (_data) ThreadIocpSource();
	return source->init();
}

Status ThreadIocpHandle::rearm(IocpData *iocp, ThreadIocpSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->currentThread = GetCurrentThread();
		source->port = iocp->_port;
	}
	return status;
}

Status ThreadIocpHandle::disarm(IocpData *iocp, ThreadIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		
	}
	return status;
}

void ThreadIocpHandle::notify(IocpData *iocp, ThreadIocpSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return; // just exit
	}

	auto performUnlock = [&] {
		performAll([&] (uint32_t count) {
			_mutex.unlock();
		});
	};

	if (data.result > 0) {
		if constexpr (IOCP_THREAD_NONBLOCK) {
			if (_mutex.try_lock()) {
				performUnlock();
			}
		} else {
			_mutex.lock();
			performUnlock();
		}
	} else {
		cancel(Status(data.result));
	}
}

Status ThreadIocpHandle::perform(Rc<thread::Task> &&task) {
	auto source = reinterpret_cast<ThreadIocpSource *>(_data);
	
	std::unique_lock lock(_mutex);
	_outputQueue.emplace_back(move(task));

	PostQueuedCompletionStatus(source->port, 1, reinterpret_cast<uintptr_t>(this), nullptr);

	return Status::Ok;
}

Status ThreadIocpHandle::perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) {
	auto source = reinterpret_cast<ThreadIocpSource *>(_data);

	std::unique_lock lock(_mutex);
	_outputCallbacks.emplace_back(CallbackInfo{sp::move(func), target, tag});

	PostQueuedCompletionStatus(source->port, 1, reinterpret_cast<uintptr_t>(this), nullptr);

	return Status::Ok;
}

}
