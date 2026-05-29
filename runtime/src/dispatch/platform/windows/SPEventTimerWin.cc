/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "SPEventTimerWin.h"

#include <sprt/runtime/log.h>
#include <sprt/wrappers/windows/windows.h>

namespace sprt::dispatch {

bool TimerWinSource::init(const TimerInfo &info) {
	if (!info.timeout) {
		timeout = sprt::max(info.interval.toMillis(), uint64_t(1));
	} else {
		timeout = sprt::max(info.timeout.toMillis(), uint64_t(1));
	}
	interval = sprt::max(info.interval.toMillis(), uint64_t(1));
	count = info.count;
	value = 0;
	return true;
}

bool TimerWinSource::start(HANDLE q, TimerWinHandle *h) {
	queue = q;

	int result = 1;
	if (!active) {
		result = CreateTimerQueueTimer(&handle, queue, [](void *ptr, BOOLEAN) {
			auto h = reinterpret_cast< const TimerWinHandle *>(ptr);
			auto iocp = static_cast<IocpData *>(h->getClass()->info->data->_platformQueue);
			PostQueuedCompletionStatus(iocp->_port, 1, reinterpret_cast<uintptr_t>(h), nullptr);
		}, (void *)h, timeout, interval, WT_EXECUTEDEFAULT);

		if (result && handle) {
			sprt::retain(h, reinterpret_cast<uintptr_t>(this));
			// next start after first will use interval instead of timeout
			timeout = interval;
			active = true;
		}
	}
	return result;
}

void TimerWinSource::stop(TimerWinHandle *h) {
	if (handle && active) {
		DeleteTimerQueueTimer(queue, handle, nullptr);
		sprt::release(h, reinterpret_cast<uintptr_t>(this));
		handle = nullptr;
		active = false;
	}
}

void TimerWinSource::reset(TimerWinHandle *h) {
	if (handle) {
		ChangeTimerQueueTimer(handle, queue, timeout, interval);
	}
}

void TimerWinSource::cancel(Handle *h) {
	active = false;

	if (handle) {
		DeleteTimerQueueTimer(queue, handle, nullptr);
		sprt::release(h, reinterpret_cast<uintptr_t>(this));
		handle = nullptr;
	}
}

bool TimerWinHandle::init(HandleClass *cl, TimerInfo &&info) {
	static_assert(
			sizeof(TimerWinSource) <= DataSize && sprt::is_standard_layout<TimerWinSource>::value);

	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	} else if (!info.timeout) {
		info.timeout = info.interval;
	}

	auto source = new (_data) TimerWinSource();
	return source->init(info);
}

bool TimerWinHandle::reset(TimerInfo &&info) {
	bool shouldResume = false;
	Status st = Status::Ok;
	if (_status == Status::Ok) {
		st = suspend();
		if (st != Status::Ok) {
			return false;
		}
		shouldResume = true;
	}
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto source = reinterpret_cast<TimerWinSource *>(_data);
	if (source->init(info)) {
		if (source->active) {
			source->reset(nullptr);
		}
		if (shouldResume) {
			resume();
		}
		return true;
	}
	return false;
}

Status TimerWinHandle::rearm(IocpData *iocp, TimerWinSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!source->active) {
			//iocp->addWaitableObject(source->handle, this);
			source->start(iocp->_timerQueue, this);
		} else {
			source->reset(this);
		}
	}
	return status;
}

Status TimerWinHandle::disarm(IocpData *iocp, TimerWinSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		source->stop(this);
		//iocp->removeWaitableObject(source->handle);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void TimerWinHandle::notify(IocpData *iocp, TimerWinSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// event handling is suspended when we receive notification

	auto count = source->count;
	auto current = source->value;

	++current;
	source->value = current;

	if (count == TimerInfo::Infinite || current < count) {
		// do nothing - timer is auto-rearmed
	} else {
		cancel(Status::Done, source->value);
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

} // namespace sprt::dispatch
