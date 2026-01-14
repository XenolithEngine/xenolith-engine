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

#include "SPEventHandle.h"
#include "SPEventTimerHandle.h"
#include "SPEventFileHandle.h"
#include "SPEventThreadHandle.h"
#include "detail/SPEventQueueData.h"

namespace STAPPLER_VERSIONIZED stappler::event {

Handle::~Handle() {
	if (_class) {
		_class->destroyFn(_class, this, _data);
	}
}

Handle::Handle() { ::memset(_data, 0, DataSize); }

bool Handle::init(HandleClass *cl, CompletionHandle<void> &&c) {
	_class = cl;
	_completion = move(c);

	_class->createFn(_class, this, _data);
	return true;
}

bool Handle::isResumable() const { return _class->suspendFn && _class->resumeFn; }

Status Handle::pause() {
	if (!isResumable()) {
		return Status::ErrorNotSupported;
	}

	if (_status != Status::Ok) {
		if (_status == Status::Suspended) {
			// temporary suspended by system, mark as externally suspended
			_status = Status::Declined;

			// infrm class that handle was suspended
			_class->suspendFn(_class, this, _data);
			return Status::Ok;
		}
		// not running
		return Status::ErrorNoSuchProcess;
	}

	auto status = _class->suspendFn(_class, this, _data);
	if (status != Status::Ok && status != Status::Done) {
		log::source().error("event::Handle", "Fail to pause handle: ", _status);
	} else {
		_status = Status::Declined;
		return Status::Ok;
	}
	return status;
}

Status Handle::resume() {
	if (!isResumable()) {
		return Status::ErrorNotSupported;
	}

	if (_status != Status::Suspended && _status != Status::Declined) {
		// not running
		return Status::ErrorNoSuchProcess;
	}

	auto status = _class->resumeFn(_class, this, _data);
	if (status != Status::Ok && status != Status::Done) {
		log::source().error("event::Handle", "Fail to resume handle: ", status);
	} else {
		status = _status = Status::Ok;
	}
	return status;
}

Status Handle::cancel(Status st, uint32_t value) {
	if (!isValidCancelStatus(st)) {
		log::
				warn("event::Handle",
						"Handle::cancel should be called with Status::Done or one of the error "
						"statuses." " It's undefined behavior otherwise");
		return Status::ErrorInvalidArguemnt;
	}

	if (isValidCancelStatus(_status)) {
		return Status::ErrorAlreadyPerformed;
	}

	// try to suspend running handle, stop if failed
	if (_status == Status::Ok) {
		if (_class->suspendFn) {
			auto status = _class->suspendFn(_class, this, _data);
			if (status != Status::Ok) {
				return Status::ErrorNotPermitted;
			}
			_status = Status::Declined;
		} else {
			return Status::ErrorNotPermitted;
		}
	}

	if (_status == Status::Suspended || _status == Status::Declined) {
		auto prevStatus = _status;
		_status = st;

		if (finalize(value, _status)) {
			return _class->cancelFn(_class, this, _data, prevStatus);
		} else {
			_status = prevStatus;
			if (resume() == Status::Ok) {
				return Status::ErrorCancelled;
			} else {
				log::
						warn("event::Handle",
								"Handle::cancel was interrupted by manual reset, but handle can "
								"not be " "resumed");
				_status = st;
				return Status::ErrorInvalidArguemnt;
			}
		}
	} else {
		return Status::ErrorAlreadyPerformed;
	}
}

/*Status Handle::addPending(Rc<Handle> &&handle) {
	if (_status == Status::Done) {
		return _queueData->runHandle(handle);
	} else if (isSuccessful(_status)) {
		_pendingHandles.emplace_back(move(handle));
		return Status::Suspended;
	} else {
		return Status::ErrorInvalidArguemnt;
	}
}*/

Status Handle::run() {
	if (_class->runFn) {
		if (_status == Status::Declined) {
			// handle is paused - resume
			return resume();
		} else if (_status == Status::Pending) {
			// initial run
			auto status = _class->runFn(_class, this, _data);
			if (status != Status::Ok && status != Status::Done) {
				log::source().error("event::Handle", "Fail to run handle: ", _status);
			} else {
				_status = Status::Ok;
				if (status == Status::Done) {
					// Handle was executed in-place, cancel it
					cancel(status);
				}
			}
			return _status;
		}
	}
	return Status::ErrorNotSupported;
}

Status Handle::suspend() {
	if (_class->suspendFn) {
		auto status = _class->suspendFn(_class, this, _data);
		if (status != Status::Ok && status != Status::Done) {
			log::source().error("event::Handle", "Fail to suspend handle: ", _status);
		} else {
			_status = Status::Suspended;
			return Status::Ok;
		}
		return status;
	}
	return Status::ErrorNotSupported;
}

Status Handle::prepareRearm() {
	if (_status == Status::Ok) {
		log::source().error("event::Handle", "Fail to prepareRearm handle: ErrorAlreadyPerformed");
		return Status::ErrorAlreadyPerformed;
	}

	if (_status != Status::Declined && _status != Status::Pending && _status != Status::Suspended) {
		log::source().error("event::Handle", "Fail to prepareRearm handle: : ErrorNotPermitted");
		return Status::ErrorNotPermitted;
	}

	if (!_class->info->data->isRunning()) {
		_status = Status::Suspended;
	} else {
		_status = Status::Ok;
	}
	return _status;
}

Status Handle::prepareDisarm() {
	if (_status != Status::Ok) {
		return Status::ErrorAlreadyPerformed;
	}

	_status = Status::Suspended;
	return Status::Ok;
}

void Handle::sendCompletion(uint32_t value, Status status) {
	if (_completion.fn) {
		_completion.fn(_completion.userdata, this, value, status);
	}
}

bool Handle::finalize(uint32_t value, Status status) {
	auto tmp = _status;
	sendCompletion(value, status);

	// handle can be reset on sendCompletion, check that status remains the same

	if (tmp == _status) {
		_completion.fn = nullptr;
		_completion.userdata = nullptr;
		return true;
	}
	return false;
}

bool Handle::reset() {
	if (_status == Status::Done) {
		// we are not done, we just suspended
		_status = Status::Declined;
		return true;
	} else if (_status == Status::Ok) {
		// we are active, suspend and resume to update queue's data on handle
		if (suspend() == Status::Ok) {
			return resume() == Status::Ok;
		}
	} else if (_status == Status::Declined) {
		// we are suspended, do nothing, queue will be updated on resume call
		return true;
	} else if (_status == Status::Pending) {
		// we are in initial state - do nothing
		return true;
	}
	return false;
}


/*bool FileOpHandle::init(QueueRef *q, QueueData *d, FileOpInfo &&info) {
	if (!Handle::init(q, d)) {
		return false;
	}

	_root = info.root;
	_pathname = info.path.str<memory::StandartInterface>();
	return true;
}

bool StatHandle::init(QueueRef *q, QueueData *d, StatOpInfo &&info) {
	if (!FileOpHandle::init(q, d, move(info.file))) {
		return false;
	}

	_completion = move(info.completion);
	return true;
}

bool DirHandle::init(QueueRef *q, QueueData *d, OpenDirInfo &&info) {
	if (!FileOpHandle::init(q, d, move(info.file))) {
		return false;
	}

	_completion = move(info.completion);
	return true;
}*/

ThreadHandle::~ThreadHandle() {
	_outputQueue.clear();
	_outputCallbacks.clear();
	_unsafeQueue.clear();
	_unsafeCallbacks.clear();
	_engine->cleanup();
}

bool ThreadHandle::init(HandleClass *cl) {
	if (!Handle::init(cl, CompletionHandle<void>())) {
		return false;
	}

	_pool = Rc<PoolRef>::alloc();

	_pool->perform([&](memory::pool_t *pool) {
		_engine = new (_pool->getPool()) PerformEngine(_pool->getPool());

		// tasks can be performed at any time on this engine
		_engine->_performEnabled = 1;
	});

	_outputQueue.reserve(2);
	_outputCallbacks.reserve(2);

	return true;
}

void ThreadHandle::wakeup() {
	auto p = memory::pool::create(_engine->_tmpPool);

	_engine->runAllTasks(p);

	memory::pool::destroy(p);
}

uint32_t ThreadHandle::performAll(const Callback<void(uint32_t)> &unlockCallback) {
	auto stack = sp::move(_outputQueue);
	auto callbacks = sp::move(_outputCallbacks);

	_outputQueue.clear();
	_outputCallbacks.clear();

	unlockCallback(static_cast<uint32_t>(stack.size() + callbacks.size()));

	for (auto &it : stack) { _engine->perform(move(it)); }
	for (auto &it : callbacks) { _engine->perform(sp::move(it.fn), move(it.ref), it.tag); }
	for (auto &it : _unsafeQueue) { _engine->perform(move(it)); }
	for (auto &it : _unsafeCallbacks) { _engine->perform(sp::move(it.fn), move(it.ref), it.tag); }

	stack.clear();
	callbacks.clear();
	_unsafeQueue.clear();
	_unsafeCallbacks.clear();

	auto p = memory::pool::create(_engine->_tmpPool);

	auto ret = _engine->runAllTasks(p);

	memory::pool::destroy(p);

	return ret;
}

} // namespace stappler::event
