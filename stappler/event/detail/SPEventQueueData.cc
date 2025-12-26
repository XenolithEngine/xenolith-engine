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

#include "SPEventQueueData.h"
#include "SPEventHandle.h"

namespace STAPPLER_VERSIONIZED stappler::event {

Status PerformEngine::perform(Rc<thread::Task> &&task) {
	if (_performEnabled == 0) {
		return Status::Declined;
	}

	mem_pool::perform([&] {
		Block *next = nullptr;
		if (_emptyBlocks) {
			next = _emptyBlocks;
			_emptyBlocks = next->next;
			--_blocksFree;
		} else {
			++_blocksAllocated;
			next = new (_pool) Block;
		}

		next->task = move(task);
		next->fn = nullptr;
		next->ref = nullptr;
		next->next = nullptr;
		next->tag = next->task->getTag();

		if (_pendingBlocksTail) {
			_pendingBlocksTail->next = next;
			_pendingBlocksTail = next;
		} else {
			_pendingBlocksFront = _pendingBlocksTail = next;
		}
		++_blocksWaiting;
		//log::source().debug("PerformEngine", this, " perform ", _blocksWaiting, " ", _blocksFree, " ", next->tag);
	}, _pool);

	return Status::Ok;
}

Status PerformEngine::perform(mem_std::Function<void()> &&fn, Ref *ref, StringView tag) {
	if (_performEnabled == 0) {
		return Status::Declined;
	}

	mem_pool::perform([&] {
		Block *next = nullptr;
		if (_emptyBlocks) {
			next = _emptyBlocks;
			_emptyBlocks = next->next;
			--_blocksFree;
		} else {
			++_blocksAllocated;
			next = new (_pool) Block;
		}

		next->task = nullptr;
		next->fn = sp::move(fn);
		next->ref = ref;
		next->next = nullptr;
		next->tag = tag;

		if (_pendingBlocksTail) {
			_pendingBlocksTail->next = next;
			_pendingBlocksTail = next;
		} else {
			_pendingBlocksFront = _pendingBlocksTail = next;
		}
		++_blocksWaiting;
		//log::source().debug("PerformEngine", this, " perform ", _blocksWaiting, " ", _blocksFree, " ", next->tag);
	}, _pool);

	return Status::Ok;
}

uint32_t PerformEngine::runAllTasks(memory::pool_t *tmpPool) {
	uint32_t nevents = 0;

	while (_pendingBlocksFront) {
		auto next = _pendingBlocksFront;

		if (next == _pendingBlocksTail) {
			_pendingBlocksTail = nullptr;
		}
		_pendingBlocksFront = next->next;

		--_blocksWaiting;

		mem_pool::perform_clear([&] {
			if (next->fn) {
				next->fn();
			}

			if (next->task) {
				next->task->run();
			}

			++nevents;

			next->fn = nullptr;
			next->task = nullptr;
			next->ref = nullptr;
			next->next = nullptr;
			next->tag = StringView();
		}, tmpPool);

		next->next = _emptyBlocks;
		_emptyBlocks = next;
		++_blocksFree;
	}

	//if (nevents > 0) {
	//	log::source().debug("PerformEngine", this, " end runAllTasks ", nevents, " ", _blocksWaiting, " ", _blocksFree);
	//}
	return nevents;
}

void PerformEngine::cleanup() {
	while (_pendingBlocksFront) {
		auto next = _pendingBlocksFront;

		if (next == _pendingBlocksTail) {
			_pendingBlocksTail = nullptr;
		}
		_pendingBlocksFront = next->next;

		--_blocksWaiting;

		next->fn = nullptr;
		next->task = nullptr;
		next->ref = nullptr;
		next->next = nullptr;
		next->tag = StringView();

		next->next = _emptyBlocks;
		_emptyBlocks = next;
		++_blocksFree;
	}
}

PerformEngine::PerformEngine(memory::pool_t *pool)
: _pool(pool), _tmpPool(memory::pool::create(pool)) { }

uint32_t QueueData::suspendAll() {
	uint32_t ret = 0;
	_running = false;
	for (auto &it : _suspendableHandles) {
		if (it->getStatus() == Status::Ok || it->getStatus() == Status::Suspended) {
			if (isSuccessful(it->suspend())) {
				++ret;
			}
		} else if (it->getStatus() != Status::Declined) {
			log::source().error("event::QueueData",
					"suspendAll: Invalid status for a resumable handle: ", it->getStatus());
		}
	}
	return ret;
}

uint32_t QueueData::resumeAll() {
	if (_running) {
		return 0;
	}

	_running = true;

	uint32_t ret = 0;
	for (auto &it : _suspendableHandles) {
		if (it->getStatus() == Status::Suspended) {
			if (it->resume() == Status::Ok) {
				++ret;
			}
		} else if (it->getStatus() != Status::Declined) {
			log::source().error("event::QueueData",
					"resumeAll: Invalid status for a resumable handle: ", it->getStatus());
		}
	}
	for (auto &it : _pendingHandles) {
		auto status = it->run();
		if (!isSuccessful(status)) {
			it->cancel(status);
		} else {
			++ret;
		}
	}
	_pendingHandles.clear();
	return ret;
}

Status QueueData::runHandle(Handle *h) {
	if (_running) {
		auto status = h->run();
		if (!isSuccessful(status)) {
			h->cancel(status);
		}
		return status;
	} else {
		_pendingHandles.emplace(h);
		return Status::Suspended;
	}
}

void QueueData::cancel(Handle *h) { _suspendableHandles.erase(h); }

void QueueData::cleanup() {
	mem_pool::perform([&] {
		mem_pool::Set<Rc<Handle>> tmpHandles;
		for (auto &it : _suspendableHandles) { tmpHandles.emplace(it); }

		for (auto &it : _pendingHandles) { tmpHandles.emplace(it); }

		for (auto &it : tmpHandles) { it->cancel(); }
	}, _tmpPool);

	_suspendableHandles.clear();
	_pendingHandles.clear();

	PerformEngine::cleanup();
}

void QueueData::notify(Handle *handle, const NotifyData &data) {
	auto cl = handle->_class;

	++_performEnabled;

	auto tmpPool = memory::pool::create(_tmpPool);

	mem_pool::perform_clear([&] {
		if (cl && cl->notifyFn) {
			cl->notifyFn(cl, handle, handle->_data, data);
		}
	}, tmpPool);

	runAllTasks(tmpPool);

	memory::pool::destroy(tmpPool);

	--_performEnabled;
}

void QueueData::notifySuspendedAll() { _platformQueue->handleSuspendedAll(); }

Status QueueData::submit() {
	if (_submit) {
		return _submit(_platformQueue);
	}
	return Status::ErrorNotImplemented;
}

uint32_t QueueData::poll() {
	if (_poll) {
		return _poll(_platformQueue);
	}
	return 0;
}

uint32_t QueueData::wait(TimeInterval ival) {
	if (_wait) {
		return _wait(_platformQueue, ival);
	}
	return 0;
}

Status QueueData::run(TimeInterval ival, QueueWakeupInfo &&info) {
	if (_run) {
		return _run(_platformQueue, ival, move(info));
	}
	return Status::ErrorNotImplemented;
}

Status QueueData::wakeup(WakeupFlags flags) {
	if (_wakeup) {
		return _wakeup(_platformQueue, flags);
	}
	return Status::ErrorNotImplemented;
}

void QueueData::cancel() {
	if (_cancel) {
		_cancel(_platformQueue);
	}
	cleanup();
}

Rc<TimerHandle> QueueData::scheduleTimer(TimerInfo &&info) {
	if (_timer) {
		return _timer(this, _platformQueue, move(info));
	}
	return nullptr;
}

Rc<PollHandle> QueueData::listenHandle(NativeHandle handle, PollFlags flags,
		CompletionHandle<PollHandle> &&cb) {
	if (_listenHandle) {
		return _listenHandle(this, _platformQueue, handle, flags, move(cb));
	}
	return nullptr;
}

Rc<ThreadHandle> QueueData::addThreadHandle() {
	if (_thread) {
		return _thread(this, _platformQueue);
	}
	return nullptr;
}

QueueData::~QueueData() {
	if (_platformQueue && _destroy) {
		_destroy(_platformQueue);
	}
	_platformQueue = nullptr;
}

QueueData::QueueData(QueueRef *ref, QueueFlags flags)
: PerformEngine(ref->getPool())
, _info(QueueHandleClassInfo{ref, this, ref->getPool()})
, _flags(flags)
, _threadId(thread::Thread::getCurrentThreadId()) {
	_pendingHandles.set_memory_persistent(true);
	_suspendableHandles.set_memory_persistent(true);
}

Status PlatformQueueData::suspendHandles(RunContext *ctx) {
	if (!ctx) {
		return Status::ErrorInvalidArguemnt;
	}

	ctx->wakeupStatus = Status::Suspended;

	auto nhandles = _data->suspendAll();
	ctx->wakeupCounter = nhandles;

	if (_suspend) {
		return _suspend(ctx);
	} else {
		return Status::Done;
	}
}

Status PlatformQueueData::stopContext(RunContext *contextToStop, WakeupFlags flags,
		bool externalCall) {
	if (!_runContext) {
		return Status::ErrorInvalidArguemnt;
	}

	Status status = Status::Ok;

	if (contextToStop && contextToStop != _runContext) {
		auto ctx = _runContext;
		while (ctx && ctx != contextToStop) { ctx = ctx->prev; }

		if (ctx && ctx == contextToStop) {
			ctx = _runContext;
			while (ctx && ctx != contextToStop) {
				if (ctx->state == RunContext::Running) {
					ctx->state = RunContext::Signaled;
				}
				ctx = ctx->prev;
			}

			if (contextToStop->state == RunContext::Running) {
				contextToStop->state = RunContext::Signaled;
			}
		} else {
			status = Status::ErrorNotFound;
		}
	} else if (_runContext->state == RunContext::Running) {
		_runContext->state = RunContext::Signaled;
	}

	if (_runContext->state == RunContext::Signaled) {
		if (hasFlag(flags, WakeupFlags::ContextDefault)) {
			flags = _runContext->runWakeupFlags;
		}

		RunContext::State nextState = RunContext::Stopped;

		// stop top-level context
		if (hasFlag(flags, WakeupFlags::Graceful)) {
			auto suspensionStatus = suspendHandles(_runContext);
			if (suspensionStatus == Status::Done) {
				_runContext->wakeupStatus = Status::Ok; // graceful wakeup
			} else if (suspensionStatus == Status::Ok) {
				// suspend was not completed
				nextState = RunContext::Stopping;
			}
			status = Status::Suspended;
		} else {
			_runContext->wakeupStatus =
					externalCall ? Status::Suspended : Status::Done; // forced wakeup
			status = Status::Ok;
		}

		// this stop will terminate top-level enter()
		if (_stopContext) {
			_stopContext(_runContext);
		}

		// prevent from multiple stops
		_runContext->state = nextState;
	}

	return status;
}

Status PlatformQueueData::stopRootContext(WakeupFlags flags, bool external) {
	if (_runContext) {
		// find and wakeup root context
		auto rootCtx = _runContext;
		while (rootCtx->prev) { rootCtx = rootCtx->prev; }

		return stopContext(rootCtx, flags, external);
	}
	return Status::ErrorInvalidArguemnt;
}

void PlatformQueueData::pushContext(RunContext *ctx, RunContext::CallMode m) {
	ctx->mode = m;
	ctx->prev = _runContext;
	ctx->queue = this;
	_runContext = ctx;
}

void PlatformQueueData::popContext(RunContext *ctx) {
	_runContext = ctx->prev;

	if (_runContext && _runContext->state == RunContext::Signaled) {
		stopContext(_runContext, _runContext->runWakeupFlags, false);
	}
}

bool PlatformQueueData::hasContext(void *ptr) {
	auto ctx = _runContext;
	while (ctx) {
		if (ctx == ptr) {
			return true;
		}
		ctx = ctx->prev;
	}
	return false;
}

void PlatformQueueData::handleSuspendedAll() {
	if (_runContext && _runContext->state == RunContext::Stopping) {
		if (_suspended) {
			_suspended(_runContext);
		} else {
			_runContext->state = RunContext::Stopped;
		}
	}
}

PlatformQueueData::PlatformQueueData(QueueRef *q, Queue::Data *data, QueueFlags f)
: _queue(q), _data(data), _flags(f) { }

} // namespace stappler::event
