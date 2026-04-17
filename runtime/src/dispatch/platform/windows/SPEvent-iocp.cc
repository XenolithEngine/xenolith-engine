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

#include "SPEvent-iocp.h"
#include "SPEvent-windows.h"
#include "SPStatus.h"
#include "SPTime.h"

namespace sprt::dispatch {

// based on https://github.com/tringi/win32-iocp-events

void IocpData::pollMessages() {
	++_data->_performEnabled;

	auto tmpPool = memory::pool::create(_data->_tmpPool);

	winmsg msg = {};
	int hasMessage = 1;
	while (hasMessage) {
		mem_pool::perform_clear([&] {
			hasMessage = _PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
			if (hasMessage) {
				_TranslateMessage(&msg);
				_DispatchMessage(&msg);
			}
		}, tmpPool);

		_data->runAllTasks(tmpPool);
	}

	memory::pool::destroy(tmpPool);

	--_data->_performEnabled;
}

Status IocpData::runPoll(TimeInterval ival, bool infinite) {
	if (_processedEvents < _receivedEvents) {
		return Status::Ok;
	}

	unsigned long nevents = 0;

	_MsgWaitForMultipleObjectsEx(1, &_port, infinite ? _WINAPI_INFINITE : ival.toMillis(),
			QS_ALLINPUT, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);

	// Prevent from recursive message polling
	if (_data->_performEnabled == 0) {
		pollMessages();
	}

	if (!_GetQueuedCompletionStatusEx(_port, _events.data(), _events.size(), &nevents, 0, 1)) {
		auto err = _GetLastError();
		if (err == WAIT_TIMEOUT) {
			_processedEvents = 0;
			_receivedEvents = 0;
			return Status::Ok;
		}
		return sprt::status::lastErrorToStatus(err);
	}

	_processedEvents = 0;
	_receivedEvents = nevents;

	if (nevents >= 0) {
		return Status::Ok;
	} else {
		return sprt::status::lastErrorToStatus(_GetLastError());
	}
}

uint32_t IocpData::processEvents(RunContext *ctx) {
	uint32_t count = 0;
	NotifyData data;

	while (_processedEvents < _receivedEvents) {
		auto &ev = _events.at(_processedEvents++);

		auto ptr = reinterpret_cast<void *>(ev.lpCompletionKey);
		if (ptr == this) {
			auto data = ev.dwNumberOfBytesTransferred;
			auto flags = WakeupFlags(data & toInt(WakeupFlags::All));

			if (hasFlag(data, CancelFlag)) {
				stopRootContext(flags, !hasFlag(data, InternalFlag));
			} else {
				stopContext(ctx, flags, !hasFlag(data, InternalFlag));
			}
		} else {
			auto h = (Handle *)ev.lpCompletionKey;

			auto refId = h->retain();

			data.result = ev.dwNumberOfBytesTransferred;
			data.queueFlags = 0;
			data.userFlags = 0;

			_data->notify(h, data);

			h->release(refId);
			++count;
		}
	}
	_receivedEvents = _processedEvents = 0;
	return count;
}

Status IocpData::submit() { return Status::Ok; }

uint32_t IocpData::poll() {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	auto status = runPoll(TimeInterval());
	if (toInt(status) > 0) {
		result = processEvents(&ctx);
	}

	popContext(&ctx);
	return result;
}

uint32_t IocpData::wait(TimeInterval ival) {
	uint32_t result = 0;

	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	auto status = runPoll(ival);
	if (status == Status::Ok) {
		result = processEvents(&ctx);
	}

	popContext(&ctx);
	return result;
}

Status IocpData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;
	ctx.wakeupStatus = Status::Suspended;
	ctx.runWakeupFlags = wakeupFlags;

	Rc<Handle> timerHandle;
	if (ival && ival != TimeInterval::Infinite) {
		// set timeout
		timerHandle =
				_queue->get()->schedule(ival, [this, wakeupFlags](Handle *handle, bool success) {
			if (success) {
				_PostQueuedCompletionStatus(_port, toInt(wakeupFlags) | InternalFlag,
						reinterpret_cast<uintptr_t>(this), nullptr);
			}
		});
	}

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		auto status = runPoll(ival, true);
		if (status == Status::Ok) {
			processEvents(&ctx);
		} else {
			log::source().error("event::IOCP", "GetQueuedCompletionStatusEx error: ", status);
			ctx.wakeupStatus = status;
			break;
		}
	}

	if (ival && ival != TimeInterval::Infinite && timerHandle) {
		// remove timeout if set
		timerHandle->cancel();
	}

	popContext(&ctx);

	return ctx.wakeupStatus;
}

Status IocpData::wakeup(WakeupFlags flags) {
	_PostQueuedCompletionStatus(_port, toInt(flags), reinterpret_cast<uintptr_t>(this), nullptr);
	return Status::Ok;
}

Status IocpData::suspendHandles() {
	if (!_runContext) {
		return Status::ErrorInvalidArguemnt;
	}

	_runContext->wakeupStatus = Status::Suspended;

	auto nhandles = _data->suspendAll();
	_runContext->wakeupCounter = nhandles;

	return Status::Done;
}

void IocpData::cancel() {
	_PostQueuedCompletionStatus(_port, toInt(WakeupFlags::ContextDefault) | CancelFlag,
			reinterpret_cast<uintptr_t>(this), nullptr);
}

IocpData::IocpData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) {
	_port = _CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (!_port) {
		log::source().error("dispatch::Queue",
				"Fail to create IOCP: ", sprt::status::lastErrorToStatus(_GetLastError()));
		return;
	}

	auto size = info.completeQueueSize;

	if (size == 0) {
		size = info.submitQueueSize;
	}
	_events.resize(size);

	_data->_handle = _port;
}

IocpData::~IocpData() {
	if (_port) {
		_CloseHandle(_port);
		_port = nullptr;
	}
}

} // namespace sprt::dispatch
