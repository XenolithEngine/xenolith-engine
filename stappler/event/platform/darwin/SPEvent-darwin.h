/**
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

#ifndef CORE_EVENT_PLATFORM_DARWIN_SPEVENT_DARWIN_H_
#define CORE_EVENT_PLATFORM_DARWIN_SPEVENT_DARWIN_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "detail/SPEventQueueData.h"
#include "detail/SPEventHandleClass.h"

#if MACOS

namespace STAPPLER_VERSIONIZED stappler::event {

struct RunLoopData;
struct KQueueData;

struct SP_PUBLIC Queue::Data : public QueueData {
	HandleClass _kqueueThreadClass;
	HandleClass _kqueueTimerClass;

	HandleClass _runloopThreadClass;
	HandleClass _runloopTimerClass;

	Data(QueueRef *q, const QueueInfo &info);
};

template <typename HandleType, typename SourceType>
void setupKQueueHandleClass(QueueHandleClassInfo *info, HandleClass *cl, bool suspendable) {
	cl->info = info;

	cl->createFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_assert(sizeof(SourceType) <= Handle::DataSize
					  && std::is_standard_layout<SourceType>::value);
		new (data) SourceType;
		return HandleClass::create(cl, handle, data);
	};
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		auto status = static_cast<HandleType *>(handle)->rearm(reinterpret_cast<KQueueData *>(platformData->_platformQueue), source);
		if (status == Status::Ok || status == Status::Done) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto source = reinterpret_cast<SourceType *>(data);

		source->cancel();
		source->~SourceType();

		return HandleClass::cancel(cl, handle, data, st);
	};

	if (suspendable) {
		cl->suspendFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = static_cast<HandleType *>(handle)->disarm(reinterpret_cast<KQueueData *>(platformData->_platformQueue), source);
			if (status == Status::Ok || status == Status::Done) {
				return HandleClass::suspend(cl, handle, data);
			}
			return status;
		};

		cl->resumeFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = HandleClass::resume(cl, handle, data);
			if (status == Status::Ok || status == Status::Done) {
				status = static_cast<HandleType *>(handle)->rearm(reinterpret_cast<KQueueData *>(platformData->_platformQueue), source);
			}
			return status;
		};
	}

	cl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
					  const NotifyData &n) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		static_cast<HandleType *>(handle)->notify(reinterpret_cast<KQueueData *>(platformData->_platformQueue), source, n);
	};
}

template <typename HandleType, typename SourceType>
void setupRunLoopHandleClass(QueueHandleClassInfo *info, HandleClass *cl, bool suspendable) {
	cl->info = info;

	cl->createFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_assert(sizeof(SourceType) <= Handle::DataSize
					  && std::is_standard_layout<SourceType>::value);
		new (data) SourceType;
		return HandleClass::create(cl, handle, data);
	};
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		auto status = static_cast<HandleType *>(handle)->rearm(reinterpret_cast<RunLoopData *>(platformData->_platformQueue), source);
		if (status == Status::Ok || status == Status::Done) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto source = reinterpret_cast<SourceType *>(data);

		source->cancel();
		source->~SourceType();

		return HandleClass::cancel(cl, handle, data, st);
	};

	if (suspendable) {
		cl->suspendFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = static_cast<HandleType *>(handle)->disarm(reinterpret_cast<RunLoopData *>(platformData->_platformQueue), source);
			if (status == Status::Ok || status == Status::Done) {
				return HandleClass::suspend(cl, handle, data);
			}
			return status;
		};

		cl->resumeFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = HandleClass::resume(cl, handle, data);
			if (status == Status::Ok || status == Status::Done) {
				status = static_cast<HandleType *>(handle)->rearm(reinterpret_cast<RunLoopData *>(platformData->_platformQueue), source);
			}
			return status;
		};
	}

	cl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
					  const NotifyData &n) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		static_cast<HandleType *>(handle)->notify(reinterpret_cast<RunLoopData *>(platformData->_platformQueue), source, n);
	};
}

}

#endif

#endif /* CORE_EVENT_PLATFORM_DARWIN_SPEVENT_DARWIN_H_ */
