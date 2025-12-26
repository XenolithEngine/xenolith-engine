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

#ifndef CORE_EVENT_DETAIL_SPEVENTHANDLECLASS_H_
#define CORE_EVENT_DETAIL_SPEVENTHANDLECLASS_H_

#include "SPEventHandle.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct QueueData;

struct SP_PUBLIC NotifyData {
	intptr_t result = 0;
	uint32_t queueFlags = 0;
	uint32_t userFlags = 0;
};

struct SP_PUBLIC QueueHandleClassInfo {
	QueueRef *queue = nullptr;
	QueueData *data = nullptr;
	memory::pool_t *pool = nullptr;

	size_t runningHandles = 0;
	size_t suspendedHandles = 0;
	size_t registredHandles = 0;
};

struct SP_PUBLIC HandleClass {
	static Status create(HandleClass *, Handle *, uint8_t[Handle::DataSize]);
	static Status destroy(HandleClass *, Handle *, uint8_t[Handle::DataSize]);

	static Status run(HandleClass *, Handle *, uint8_t[Handle::DataSize]);
	static Status cancel(HandleClass *, Handle *, uint8_t[Handle::DataSize], Status);

	static Status suspend(HandleClass *, Handle *, uint8_t[Handle::DataSize]);
	static Status resume(HandleClass *, Handle *, uint8_t[Handle::DataSize]);

	void addPending(Handle *origin, Handle *pending);

	QueueHandleClassInfo *info = nullptr;

	Status (*createFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize]) = &create;
	Status (*destroyFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize]) = &destroy;

	// initial launcher
	Status (*runFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize]) = &run;

	// cancellation (with Done or error), should launch pending handles
	Status (*cancelFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize], Status) = &cancel;

	// suspend execution, if supported
	Status (*suspendFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize]) = nullptr;

	// resume execution, if supported
	Status (*resumeFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize]) = nullptr;

	void (*notifyFn) (HandleClass *, Handle *, uint8_t[Handle::DataSize], const NotifyData &) = nullptr;

	mem_pool::Map<Handle *, mem_pool::Vector<Rc<Handle>>> pendingHandles;

	size_t runningHandles = 0;
	size_t suspendedHandles = 0;
	size_t registredHandles = 0;
};

}

#endif /* CORE_EVENT_DETAIL_SPEVENTHANDLECLASS_H_ */
