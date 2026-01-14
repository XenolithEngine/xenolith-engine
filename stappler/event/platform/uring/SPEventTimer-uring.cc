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

#include "SPEventTimer-uring.h"
#include "detail/SPEventHandleClass.h"

namespace STAPPLER_VERSIONIZED stappler::event {

bool TimerUringSource::init(const TimerInfo &info) {
	if (info.timeout) {
		setNanoTimespec(timer.it_value, info.timeout);
	} else {
		setNanoTimespec(timer.it_value, info.interval);
	}
	setNanoTimespec(timer.it_interval, info.interval);

	value = 0;
	count = info.count;
	return true;
}

bool TimerURingHandle::init(HandleClass *cl, TimerInfo &&info) {
	if (!TimerHandle::init(cl, info.completion)) {
		return false;
	}

	if (info.count == 1) {
		info.interval = info.timeout;
	}

	auto source = new (_data) TimerUringSource;
	return source->init(info);
}

static bool timespecIsEqual(const _linux_timespec &l, const _linux_timespec &r) {
	return l.tv_nsec == r.tv_nsec && l.tv_sec == r.tv_sec;
}

bool TimerURingHandle::reset(TimerInfo &&info) {
	auto hasMultiTimer =
			hasFlag(reinterpret_cast<URingData *>(_class->info->data->_platformQueue)->_uflags,
					URingFlags::TimerMultishotSupported);

	if (info.count == 1 || (hasMultiTimer && info.count != TimerInfo::Infinite)) {
		if (info.completion) {
			_completion = move(info.completion);
			_userdata = nullptr;
		}

		auto source = reinterpret_cast<TimerUringSource *>(_data);
		return source->init(info) && Handle::reset();
	}

	log::source().info("TimerURingHandle",
			"TimerURingHandle can only be reset with 1 or TimerInfo::Infinite `count`");
	return false;
}

Status TimerURingHandle::rearm(URingData *uring, TimerUringSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (source->value == 0
				&& !timespecIsEqual(source->timer.it_value, source->timer.it_interval)) {
			status = uring->pushSqe({IORING_OP_TIMEOUT}, [&](io_uring_sqe *sqe, uint32_t n) {
				sqe->len = 1;
				sqe->addr = reinterpret_cast<uintptr_t>(&source->timer.it_value);
				sqe->off = 0;
				sqe->timeout_flags = IORING_TIMEOUT_ETIME_SUCCESS;
				sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK);
			}, URingPushFlags::Submit);
		} else {
			status = uring->pushSqe({IORING_OP_TIMEOUT}, [&](io_uring_sqe *sqe, uint32_t n) {
				sqe->fd = -1;
				sqe->len = 1;
				sqe->addr = reinterpret_cast<uintptr_t>(&source->timer.it_interval);
				sqe->off =
						(source->count == TimerInfo::Infinite ? 0 : source->count - source->value);
				sqe->timeout_flags = IORING_TIMEOUT_MULTISHOT | IORING_TIMEOUT_ETIME_SUCCESS;
				sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK);
			}, URingPushFlags::Submit);
		}
	}
	//log::source().debug("TimerURingHandle", uring->_tick, ": rearm");
	return status;
}

Status TimerURingHandle::disarm(URingData *uring, TimerUringSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	}

	//log::source().debug("TimerURingHandle", uring->_tick, ": disarm");
	return status;
}

void TimerURingHandle::notify(URingData *uring, TimerUringSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	//log::source().debug("TimerURingHandle", uring->_tick, ": notify");

	bool more = (data.queueFlags & IORING_CQE_F_MORE);
	if (!more) {
		// if more - we still running
		_status = Status::Suspended;
	}

	if (data.result != -ETIME && data.result < 0) {
		cancel(URingData::getErrnoStatus(data.result));
	}

	auto count = source->count;
	auto current = source->value;

	bool isFirst = (current == 0);

	++current;
	source->value = current;

	if ((isFirst && !timespecIsEqual(source->timer.it_value, source->timer.it_interval))
			|| (!more)) {
		if (count == TimerInfo::Infinite || current < count) {
			// rearm if spatial
			rearm(uring, source);
		} else {
			cancel(Status::Done, source->value);
			return; // cancel will emit sendCompletion
		}
	}

	sendCompletion(current, _status == Status::Suspended ? Status::Ok : _status);
}

} // namespace stappler::event
