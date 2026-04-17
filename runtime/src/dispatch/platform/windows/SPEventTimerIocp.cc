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

#include "SPEventTimerIocp.h"
#include "SPStatus.h"
#include "platform/windows/SPEvent-iocp.h"

namespace sprt::dispatch {

static void timeToFileTime(int64_t &ftime, TimeInterval ival) {
	// ticks in 100ns
	auto ticks = ival.toMicros() * 10;
	ftime = -ticks;
}

bool TimerIocpSource::init(const TimerInfo &info) {
	cancel();

	handle = _CreateWaitableTimerEx(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (!handle) {
		return false;
	}

	interval = info.interval;
	count = info.count;

	int result = 1;
	int64_t dueDate;
	timeToFileTime(dueDate, info.timeout);

	if (interval.toMicros() < 1'000) {
		subintervals = true;
	}

	if (info.count == 1) {
		// oneshot timer
		result = _SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
	} else if (!subintervals) {
		result = _SetWaitableTimerEx(handle, &dueDate, interval.toMillis(), 0, 0, 0, 0);
	} else {
		result = _SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
	}
	if (!result) {
		log::source().error("dispatch::Queue",
				"Fail to create WaitableTimer: ", sprt::status::lastErrorToStatus(_GetLastError()));
	}

	active = true;
	return result;
}

bool TimerIocpSource::start() {
	if (!handle) {
		handle = _CreateWaitableTimerEx(0, 0, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
				TIMER_ALL_ACCESS);
		active = false;
	}

	int result = 1;
	if (!active) {
		int64_t dueDate;
		timeToFileTime(dueDate, interval);
		if (!subintervals) {
			result = _SetWaitableTimerEx(handle, &dueDate, interval.toMillis(), 0, 0, 0, 0);
		} else {
			result = _SetWaitableTimerEx(handle, &dueDate, 0, 0, 0, 0, 0);
		}
		if (result) {
			active = true;
		}
	}
	return result;
}

void TimerIocpSource::stop() {
	active = false;

	if (event) {
		_CancelEventCompletion(event, true);
		event = nullptr;
	}

	if (handle && active) {
		_CancelWaitableTimer(handle);
		active = false;
	}
}

void TimerIocpSource::reset() {
	int64_t dueDate;
	if (subintervals && handle) {
		timeToFileTime(dueDate, interval);
		_SetWaitableTimer(handle, &dueDate, 0, 0, 0, 0);
	}
}

void TimerIocpSource::cancel() {
	active = false;

	if (event) {
		_CancelEventCompletion(event, true);
		event = nullptr;
	}

	if (handle) {
		_CancelWaitableTimer(handle);
		_CloseHandle(handle);
		handle = nullptr;
	}
}

bool TimerIocpHandle::init(HandleClass *cl, TimerInfo &&info) {
	static_assert(sizeof(TimerIocpSource) <= DataSize
			&& sprt::is_standard_layout<TimerIocpSource>::value);

	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	} else if (!info.timeout) {
		info.timeout = info.interval;
	}

	auto source = new (_data) TimerIocpSource();
	return source->init(info);
}

bool TimerIocpHandle::reset(TimerInfo &&info) {
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto source = reinterpret_cast<TimerIocpSource *>(_data);
	return source->init(info) && Handle::reset();
}

Status TimerIocpHandle::rearm(IocpData *iocp, TimerIocpSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!source->active) {
			source->start();
		} else {
			source->reset();
		}
		if (!source->event) {
			source->event = _ReportEventAsCompletion(iocp->_port, source->handle, _timeline,
					reinterpret_cast<uintptr_t>(this), nullptr);
			if (!source->event) {
				return sprt::status::lastErrorToStatus(_GetLastError());
			}
		} else {
			if (!_RestartEventCompletion2(source->event, iocp->_port, source->handle, _timeline,
						reinterpret_cast<uintptr_t>(this), nullptr)) {
				return sprt::status::lastErrorToStatus(_GetLastError());
			}
		}
	}
	return status;
}

Status TimerIocpHandle::disarm(IocpData *iocp, TimerIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		source->stop();
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void TimerIocpHandle::notify(IocpData *iocp, TimerIocpSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// event handling is suspended when we receive notification
	_status = Status::Suspended;

	auto count = source->count;
	auto current = source->value;

	++current;
	source->value = current;

	if (count == TimerInfo::Infinite || current < count) {
		rearm(iocp, source);
	} else {
		cancel(Status::Done, source->value);
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

} // namespace sprt::dispatch
