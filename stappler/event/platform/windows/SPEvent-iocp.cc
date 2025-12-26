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

#include <errhandlingapi.h>
#include <handleapi.h>
#include <minwinbase.h>
#include <winerror.h>
#include <winternl.h>
#include <ntstatus.h>

extern "C" {

WINBASEAPI NTSTATUS WINAPI NtCreateWaitCompletionPacket(_Out_ PHANDLE WaitCompletionPacketHandle,
		_In_ ACCESS_MASK DesiredAccess, _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes);

WINBASEAPI NTSTATUS WINAPI NtAssociateWaitCompletionPacket(_In_ HANDLE WaitCompletionPacketHandle,
		_In_ HANDLE IoCompletionHandle, _In_ HANDLE TargetObjectHandle, _In_opt_ PVOID KeyContext,
		_In_opt_ PVOID ApcContext, _In_ NTSTATUS IoStatus, _In_ ULONG_PTR IoStatusInformation,
		_Out_opt_ PBOOLEAN AlreadySignaled);

WINBASEAPI NTSTATUS WINAPI NtCancelWaitCompletionPacket(_In_ HANDLE WaitCompletionPacketHandle,
		_In_ BOOLEAN RemoveSignaledPacket);
}

namespace STAPPLER_VERSIONIZED stappler::event {

// based on https://github.com/tringi/win32-iocp-events

HANDLE ReportEventAsCompletion(HANDLE hIOCP, HANDLE hEvent, DWORD dwNumberOfBytesTransferred,
		ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {

	HANDLE hPacket = NULL;
	HRESULT hr = NtCreateWaitCompletionPacket(&hPacket, GENERIC_ALL, NULL);

	if (SUCCEEDED(hr)) {
		OVERLAPPED_ENTRY completion{};
		completion.dwNumberOfBytesTransferred = dwNumberOfBytesTransferred;
		completion.lpCompletionKey = dwCompletionKey;
		completion.lpOverlapped = lpOverlapped;

		if (!RestartEventCompletion(hPacket, hIOCP, hEvent, &completion)) {
			NtClose(hPacket);
			hPacket = NULL;
		}
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		default: SetLastError(hr);
		}
	}
	return hPacket;
}

BOOL RestartEventCompletion(HANDLE hPacket, HANDLE hIOCP, HANDLE hEvent,
		const OVERLAPPED_ENTRY *completion) {
	if (!completion) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return RestartEventCompletion(hPacket, hIOCP, hEvent, completion->dwNumberOfBytesTransferred,
			completion->lpCompletionKey, completion->lpOverlapped);
}

BOOL RestartEventCompletion(HANDLE hPacket, HANDLE hIOCP, HANDLE hEvent,
		DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {
	HRESULT hr = NtAssociateWaitCompletionPacket(hPacket, hIOCP, hEvent, (PVOID)dwCompletionKey,
			(PVOID)lpOverlapped, 0, dwNumberOfBytesTransferred, NULL);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		case STATUS_INVALID_HANDLE: // not valid handle passed for hIOCP
		case STATUS_OBJECT_TYPE_MISMATCH: // incorrect handle passed for hIOCP
		case STATUS_INVALID_PARAMETER_1:
		case STATUS_INVALID_PARAMETER_2: SetLastError(ERROR_INVALID_PARAMETER); break;
		case STATUS_INVALID_PARAMETER_3:
			if (hEvent) {
				SetLastError(ERROR_INVALID_HANDLE);
			} else {
				SetLastError(ERROR_INVALID_PARAMETER);
			}
			break;
		default: SetLastError(hr);
		}
		return FALSE;
	}
}

BOOL CancelEventCompletion(HANDLE hWait, BOOL cancel) {
	HRESULT hr = NtCancelWaitCompletionPacket(hWait, cancel);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		SetLastError(hr);
		return FALSE;
	}
}

void IocpData::pollMessages() {
	++_data->_performEnabled;

	auto tmpPool = memory::pool::create(_data->_tmpPool);

	MSG msg = {};
	BOOL hasMessage = 1;
	while (hasMessage) {
		mem_pool::perform_clear([&] {
			hasMessage = PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);
			if (hasMessage) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
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

	ULONG nevents = 0;

	MsgWaitForMultipleObjectsEx(1, &_port, infinite ? INFINITE : ival.toMillis(), QS_ALLINPUT,
			MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);

	// Prevent from recursive message polling
	if (_data->_performEnabled == 0) {
		pollMessages();
	}

	if (!GetQueuedCompletionStatusEx(_port, _events.data(), _events.size(), &nevents, 0, 1)) {
		auto err = GetLastError();
		if (err == WAIT_TIMEOUT) {
			_processedEvents = 0;
			_receivedEvents = 0;
			return Status::Ok;
		}
		return status::lastErrorToStatus(err);
	}

	_processedEvents = 0;
	_receivedEvents = nevents;

	if (nevents >= 0) {
		return Status::Ok;
	} else {
		return status::lastErrorToStatus(GetLastError());
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
				PostQueuedCompletionStatus(_port, toInt(wakeupFlags) | InternalFlag,
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
	PostQueuedCompletionStatus(_port, toInt(flags), reinterpret_cast<uintptr_t>(this), nullptr);
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
	PostQueuedCompletionStatus(_port, toInt(WakeupFlags::ContextDefault) | CancelFlag,
			reinterpret_cast<uintptr_t>(this), nullptr);
}

IocpData::IocpData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) {
	_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (!_port) {
		log::source().error("event::Queue",
				"Fail to create IOCP: ", status::lastErrorToStatus(GetLastError()));
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
		CloseHandle(_port);
		_port = nullptr;
	}
}

} // namespace stappler::event
