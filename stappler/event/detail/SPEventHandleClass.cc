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

#include "SPEventHandleClass.h"
#include "detail/SPEventQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::event {

Status HandleClass::create(HandleClass *cl, Handle *, uint8_t[Handle::DataSize]) {
	++cl->registredHandles;
	++cl->info->registredHandles;
	return Status::Ok;
}

Status HandleClass::destroy(HandleClass *cl, Handle *handle, uint8_t[Handle::DataSize]) {
	--cl->registredHandles;
	--cl->info->registredHandles;
	return Status::Ok;
}

Status HandleClass::run(HandleClass *cl, Handle *handle, uint8_t[Handle::DataSize]) {
	++cl->runningHandles;
	++cl->info->runningHandles;
	cl->info->queue->retain(reinterpret_cast<uintptr_t>(handle));

	if (handle->isResumable()) {
		cl->info->data->_suspendableHandles.emplace(handle);
	}

	return Status::Ok;
}

Status HandleClass::cancel(HandleClass *cl, Handle *handle, uint8_t[Handle::DataSize], Status st) {
	mem_pool::perform([&] {
		if (st == Status::Declined) {
			// remove from suspended counter if it were paused
			--cl->info->suspendedHandles;
			--cl->suspendedHandles;
		}

		if (handle->getStatus() == Status::Done) {
			auto it = cl->pendingHandles.find(handle);
			if (it != cl->pendingHandles.end()) {
				for (auto &iit : it->second) { iit->run(); }
				cl->pendingHandles.erase(it);
			}
		} else {
			auto it = cl->pendingHandles.find(handle);
			if (it != cl->pendingHandles.end()) {
				for (auto &iit : it->second) { iit->cancel(Status::ErrorCancelled); }
				cl->pendingHandles.erase(it);
			}
		}
	}, cl->info->pool);

	cl->info->data->cancel(handle);

	--cl->runningHandles;
	--cl->info->runningHandles;

	cl->info->queue->release(reinterpret_cast<uintptr_t>(handle));
	return Status::Ok;
}

Status HandleClass::suspend(HandleClass *cl, Handle *handle, uint8_t[Handle::DataSize]) {
	++cl->suspendedHandles;
	++cl->info->suspendedHandles;

	//std::cout << "Suspend: " << cl->info << " " << handle << " " << handle->getStatus() << " "
	//		  << cl->info->suspendedHandles << " " << cl->info->runningHandles << "\n";

	if (cl->info->suspendedHandles == cl->info->runningHandles) {
		cl->info->data->notifySuspendedAll();
	}

	return Status::Ok;
}

Status HandleClass::resume(HandleClass *cl, Handle *handle, uint8_t[Handle::DataSize]) {
	if (handle->isResumable()) {
		auto it = cl->info->data->_suspendableHandles.find(handle);
		if (it == cl->info->data->_suspendableHandles.end()) {
			// not started
			return Status::ErrorNoSuchProcess;
		}
	}

	--cl->suspendedHandles;
	--cl->info->suspendedHandles;
	return Status::Ok;
}

void HandleClass::addPending(Handle *origin, Handle *pending) {
	mem_pool::perform([&] {
		auto it = pendingHandles.find(origin);
		if (it == pendingHandles.end()) {
			it = pendingHandles.emplace(origin).first;
			it->second.reserve_block_optimal();
		}
		it->second.emplace_back(pending);
	}, info->pool);
}

} // namespace stappler::event
