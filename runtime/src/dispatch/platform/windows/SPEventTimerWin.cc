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
	cancel();

	handle = CreateWaitableTimerExW(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (!handle) {
		return false;
	}

	subintervals = false;
	interval = info.interval;
	count = info.count;
	value = 0;

	int result = 1;
	LARGE_INTEGER dueDate;
	timeToFileTime(dueDate, info.timeout);

	if (interval.toMicros() < 1'000) {
		subintervals = true;
	}

	if (info.count == 1) {
		// oneshot timer
		result = SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
	} else if (!subintervals) {
		result = SetWaitableTimerEx(handle, &dueDate, interval.toMillis(), 0, 0, 0, 0);
	} else {
		result = SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
	}
	if (!result) {
		oslog::vperror(__SPRT_LOCATION, "dispatch::Queue",
				"Fail to create WaitableTimer: ", sprt::status::lastErrorToStatus(GetLastError()));
	}

	return result;
}

bool TimerWinSource::start() {
	if (!handle) {
		handle = CreateWaitableTimerExW(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
				TIMER_ALL_ACCESS);
		active = false;
	}

	int result = 1;
	if (!active) {
		LARGE_INTEGER dueDate;
		timeToFileTime(dueDate, interval);
		if (!subintervals) {
			result = SetWaitableTimerEx(handle, &dueDate, interval.toMillis(), 0, 0, 0, 0);
		} else {
			result = SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
		}
		if (result) {
			active = true;
		}
	}
	return result;
}

void TimerWinSource::stop() {
	active = false;

	if (handle && active) {
		CancelWaitableTimer(handle);
		active = false;
	}
}

void TimerWinSource::reset() {
	LARGE_INTEGER dueDate;
	if (subintervals && handle) {
		timeToFileTime(dueDate, interval);
		SetWaitableTimer(handle, &dueDate, 0, 0, 0, 0);
	}
}

void TimerWinSource::cancel() {
	active = false;

	if (handle) {
		CancelWaitableTimer(handle);
		CloseHandle(handle);
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
			iocp->addWaitableObject(source->handle, this);
			source->start();
		} else {
			source->reset();
		}
	}
	return status;
}

Status TimerWinHandle::disarm(IocpData *iocp, TimerWinSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		source->stop();
		iocp->removeWaitableObject(source->handle);
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
		_status = Status::Suspended;
		rearm(iocp, source);
	} else {
		cancel(Status::Done, source->value);
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

} // namespace sprt::dispatch
