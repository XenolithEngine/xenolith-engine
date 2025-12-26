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

#include "SPEventPollIocp.h"
#include "SPEvent-windows.h"

namespace STAPPLER_VERSIONIZED stappler::event {

bool PollIocpSource::init(HANDLE h, PollFlags f) {
	handle = h;
	flags = f;
	return true;
}

void PollIocpSource::cancel() { handle = nullptr; }

bool PollIocpHandle::init(HandleClass *cl, HANDLE handle, PollFlags flags,
		CompletionHandle<PollHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	auto source = new (_data) PollIocpSource;
	return source->init(handle, flags);
}

NativeHandle PollIocpHandle::getNativeHandle() const {
	return reinterpret_cast<const PollIocpSource *>(_data)->handle;
}

bool PollIocpHandle::reset(PollFlags flags) {
	reinterpret_cast<PollIocpSource *>(_data)->flags = flags;
	return Handle::reset();
}

Status PollIocpHandle::rearm(IocpData *iocp, PollIocpSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!source->event) {
			source->event = ReportEventAsCompletion(iocp->_port, source->handle, 1,
					reinterpret_cast<uintptr_t>(this), nullptr);
			if (!source->event) {
				return status::lastErrorToStatus(GetLastError());
			}
		} else {
			if (!RestartEventCompletion(source->event, iocp->_port, source->handle, 1,
						reinterpret_cast<uintptr_t>(this), nullptr)) {
				return status::lastErrorToStatus(GetLastError());
			}
		}
	}
	return status;
}

Status PollIocpHandle::disarm(IocpData *iocp, PollIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		if (source->event) {
			CancelEventCompletion(source->event, true);
			source->event = nullptr;
		}
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void PollIocpHandle::notify(IocpData *iocp, PollIocpSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// event handling is suspended when we receive notification
	_status = Status::Suspended;

	if (data.result > 0) {
		rearm(iocp, source);
	} else {
		cancel();
	}

	sendCompletion(data.result, _status == Status::Suspended ? Status::Ok : _status);
}

} // namespace stappler::event
