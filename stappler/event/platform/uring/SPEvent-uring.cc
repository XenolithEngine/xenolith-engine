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

#include "SPCore.h"
#include "SPEventQueue.h"
#include "SPTime.h"

#include "platform/linux/SPEvent-linux.h"
#include "SPEvent-uring.h"

#include <sprt/c/sys/__sprt_utsname.h>
#include <signal.h>

namespace STAPPLER_VERSIONIZED stappler::event {

static constexpr uint32_t URING_CANCEL_FLAG = 0x8000'0000;

static unsigned atomicLoadAcquire(unsigned *ptr) {
	unsigned result;
	__atomic_load(ptr, &result, __ATOMIC_ACQUIRE);
	return result;
}

static unsigned atomicLoadRelaxed(unsigned *ptr) {
	unsigned result;
	__atomic_load(ptr, &result, __ATOMIC_RELAXED);
	return result;
}

static void atomicStoreRelease(unsigned *ptr, unsigned value) {
	__atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

// Ignore -1 error in debug mode - it indicated debugger interrupt
#if DEBUG
static constexpr int DEBUG_ERROR_THRESHOLD = -1;
#else
static constexpr int DEBUG_ERROR_THRESHOLD = 0;
#endif

bool URingData::checkSupport() {
	struct __SPRT_UTSNAME_NAME buffer;

	if (__sprt_uname(&buffer) != 0) {
		log::source().info("event::URingData", "Fail to detect kernel version");
		return false;
	}

	if (strverscmp(buffer.release, "5.15.0") < 0) {
		log::source().info("event::URingData",
				"io_uring backend available since 5.15 kernel release, current release: ",
				buffer.release);
		return false;
	}

	auto ret = __sprt_io_uring_register(0, IORING_UNREGISTER_BUFFERS, NULL, 0);

	if (ret == 0 && errno == ENOSYS) {
		log::source().info("event::URingData", "io_uring disabled in OS");
		return false;
	} else {
		return true;
	}
}

uint16_t URingData::registerBufferGroup(uint32_t count, uint32_t size, uint8_t *data,
		io_uring_sqe *sqe) {
	uint16_t id = 0;
	if (_unregistredBuffers.empty()) {
		if (_bufferGroupId == maxOf<uint16_t>()) {
			log::source().error("URingData", "Buffer group overflow");
			return 0;
		}

		id = _bufferGroupId++;
	} else {
		id = _unregistredBuffers.back();
		_unregistredBuffers.pop_back();
	}

	auto fillSqe = [&](io_uring_sqe *target) {
		target->fd = count;
		target->addr = reinterpret_cast<uintptr_t>(data);
		target->len = size;
		target->buf_group = id;
		target->off = 0;
		target->user_data = URING_USERDATA_IGNORED;
	};

	if (id) {
		if (sqe) {
			fillSqe(sqe);
		} else {
			pushSqe({IORING_OP_PROVIDE_BUFFERS}, [&](io_uring_sqe *target, uint32_t) {
				fillSqe(target);
			}, URingPushFlags::Submit);
		}
	}
	return id;
}

uint16_t URingData::reloadBufferGroup(uint16_t id, uint32_t count, uint32_t size, uint8_t *data) {
	pushSqe({IORING_OP_REMOVE_BUFFERS, IORING_OP_PROVIDE_BUFFERS},
			[&](io_uring_sqe *sqe, uint32_t idx) {
		switch (idx) {
		case 0: unregisterBufferGroup(count, id, sqe); break;
		case 1: id = registerBufferGroup(count, size, data, sqe); break;
		}
	}, URingPushFlags::Submit);
	return id;
}

void URingData::unregisterBufferGroup(uint16_t id, uint32_t count, io_uring_sqe *sqe) {
	auto fillSqe = [&](io_uring_sqe *target) {
		target->fd = count;
		target->buf_group = id;
		target->user_data = URING_USERDATA_IGNORED;
	};

	if (sqe) {
		fillSqe(sqe);
	} else {
		pushSqe({IORING_OP_REMOVE_BUFFERS},
				[&](io_uring_sqe *target, uint32_t) { fillSqe(target); }, URingPushFlags::Submit);
	}

	_unregistredBuffers.emplace_back(id);
}

unsigned URingData::getUnprocessedSqeCount() {
	unsigned head;

	if (_params.flags & IORING_SETUP_SQPOLL) {
		head = atomicLoadAcquire(sq.head);
	} else {
		head = *sq.head;
	}

	/* always use real head, to avoid losing sync for short submit */
	return sq.userspaceTail - head;
}

unsigned URingData::flushSqe() {
	unsigned tail = sq.userspaceTail;

	if (sq.userspaceHead != tail) {
		sq.userspaceHead = tail;

		if ((_params.flags & IORING_SETUP_SQPOLL) == 0) {
			*sq.tail = tail;
		} else {
			atomicStoreRelease(sq.tail, tail);
		}
	}

	return tail - atomicLoadRelaxed(sq.head);
}

URingData::SqeBlock URingData::tryGetNextSqe(uint32_t count) {
	unsigned int head, first = sq.userspaceTail, next = sq.userspaceTail + count;

	if ((_params.flags & IORING_SETUP_SQPOLL) == 0) {
		head = *sq.head;
	} else {
		head = atomicLoadAcquire(sq.head);
	}

	if (next - head <= *sq.entries) {
		struct io_uring_sqe *sqe;

		auto index = (sq.userspaceTail & *sq.mask);
		sqe = &sq.sqes[index];

		sq.array[index] = index;
		sq.userspaceTail = next;

		memset(sqe, 0, sizeof(io_uring_sqe));

		return SqeBlock{sqe, first, count, Status::Ok};
	}

	return SqeBlock{nullptr, 0, 0, Status::ErrorBusy};
}

URingData::SqeBlock URingData::getNextSqe(uint32_t count) {
	if (count > _params.sq_entries) {
		return SqeBlock{nullptr, 0, 0, Status::ErrorInvalidArguemnt};
	}

	auto sqe = tryGetNextSqe(count);
	if (!sqe) {
		int ret = submitSqe(flushSqe(), 0, true);
		if (ret < 0) {
			log::source().error("event::URingData", "getNextSqe(): io_uring_enter failed: ", ret);
			return SqeBlock{nullptr, 0, 0, Status(getErrnoStatus(ret))};
		}
		sqe = tryGetNextSqe(count);
		if (!sqe) {
			log::
					source()
							.warn("event::URingData",
									"getNextSqe(): io_uring_enter on timoeut (possible "
									"IORING_SETUP_SQPOLL " "overload)");
			return SqeBlock{nullptr, 0, 0, Status::ErrorBusy};
		}
	}
	return sqe;
}

Status URingData::pushSqe(std::initializer_list<uint8_t> ops,
		const Callback<void(io_uring_sqe *, uint32_t n)> &cb, URingPushFlags flags) {
	for (auto &it : ops) {
		if (!_probe.isOpcodeSupported(it)) {
			return Status::ErrorNotImplemented;
		}
	}

	auto size = uint32_t(ops.size());

	Handle *handlesToRetain[size];
	memset(handlesToRetain, 0, sizeof(handlesToRetain));

	auto linked = hasFlag(flags, URingPushFlags::Linked);
	auto sqe = getNextSqe(size);
	if (sqe) {
		uint32_t n = 0;
		while (n < sqe.count) {
			auto index = ((sqe.first++) & *sq.mask);
			auto ptr = &sq.sqes[index];

			sq.array[index] = index;

			ptr->opcode = *(ops.begin() + n);
			ptr->flags = 0;
			cb(ptr, n);

			if (linked && n < sqe.count) {
				ptr->flags |= IOSQE_IO_LINK;
			}

			if (ptr->user_data & URING_USERDATA_RETAIN_BIT) {
				handlesToRetain[n] = (Handle *)(ptr->user_data & URING_USERDATA_PTR_MASK);
			}

			++n;
		}

		// submit now for IORING_SETUP_SQPOLL
		if ((_params.flags & IORING_SETUP_SQPOLL) != 0 || hasFlag(flags, URingPushFlags::Submit)) {
			submitPending();
			sqe.status = Status::Ok;
		} else {
			sqe.status = Status::Suspended;
		}

		for (uint32_t i = 0; i < size; ++i) {
			if (handlesToRetain[i]) {
				handlesToRetain[i]->retain(reinterpret_cast<uintptr_t>(this)
						^ reinterpret_cast<uintptr_t>(handlesToRetain[i]));
			}
		}
	}
	return sqe.status;
}

Status URingData::pushSqe(uint8_t op, const Callback<void(io_uring_sqe *)> &cb,
		const _linux_timespec *ts) {
	if (ts) {
		return pushSqe({op, uint8_t(IORING_OP_LINK_TIMEOUT)}, [&](io_uring_sqe *sqe, uint32_t n) {
			switch (n) {
			case 0: cb(sqe); break;
			case 1:
				sqe->addr = reinterpret_cast<uintptr_t>(ts);
				sqe->len = 1;
				sqe->timeout_flags = 0;
				sqe->off = 0;
				sqe->user_data = URING_USERDATA_TIMEOUT;
				break;
			default: break;
			}
		}, URingPushFlags::Linked);
	} else {
		return pushSqe({op}, [&](io_uring_sqe *sqe, uint32_t) { cb(sqe); }, URingPushFlags::None);
	}
}

inline void updateIoSqe(io_uring_sqe *sqe, int fd, const void *addr, unsigned len, uint64_t offset,
		uint64_t udata) {
	sqe->fd = fd;
	sqe->len = len;
	sqe->off = offset;
	sqe->addr = reinterpret_cast<uintptr_t>(addr);
	sqe->user_data = udata;
}

inline void updateIoSqe(io_uring_sqe *sqe, int fd, uint64_t addr, unsigned len, uint64_t offset,
		uint64_t udata) {
	sqe->fd = fd;
	sqe->len = len;
	sqe->off = offset;
	sqe->addr = addr;
	sqe->user_data = udata;
}

static bool isCqePending(unsigned flags) {
	return (flags
				   & (IORING_SQ_CQ_OVERFLOW
#ifdef IORING_SQ_TASKRUN
						   | IORING_SQ_TASKRUN
#endif
						   ))
			!= 0;
}

int URingData::submitSqe(unsigned sub, unsigned wait, bool waitAvailable, bool force) {
	std::atomic_thread_fence(std::memory_order_seq_cst);

	unsigned targetFlags = 0;
	unsigned sourceFlags = atomicLoadRelaxed(sq.flags);
	if (wait || isCqePending(sourceFlags)) {
		targetFlags |= IORING_ENTER_GETEVENTS;
	}

	if (sub > 0) {
		if (!force && (_params.flags & IORING_SETUP_SQPOLL)) {
			if ((sourceFlags & IORING_SQ_NEED_WAKEUP) != 0) {
				targetFlags |= IORING_ENTER_SQ_WAKEUP;
			}
		} else {
			targetFlags |= IORING_ENTER_GETEVENTS;
		}
	}

	if (waitAvailable && (_params.flags & IORING_SETUP_SQPOLL) != 0) {
		targetFlags |= IORING_ENTER_SQ_WAIT;
	}

	if (targetFlags != 0) {
		return enter(sub, 0, targetFlags, nullptr);
	}
	return sub;
}

int URingData::submitPending(bool force) { return submitSqe(flushSqe(), 0, false, force); }

Status URingData::pushRead(int fd, uint8_t *buf, size_t bsize, uint64_t userdata) {
	return pushSqe({IORING_OP_READ}, [&](io_uring_sqe *sqe, uint32_t) {
		updateIoSqe(sqe, fd, buf, unsigned(bsize), -1, userdata);
	}, URingPushFlags::Submit);
}

Status URingData::pushWrite(int fd, const uint8_t *buf, size_t bsize, uint64_t userdata) {
	return pushSqe({IORING_OP_WRITE}, [&](io_uring_sqe *sqe, uint32_t) {
		updateIoSqe(sqe, fd, buf, unsigned(bsize), -1, userdata);
	}, URingPushFlags::Submit);
}

Status URingData::cancelOp(uint64_t userdata, URingCancelFlags cancelFlags) {
	if ((hasFlag(cancelFlags, URingCancelFlags::All) || hasFlag(cancelFlags, URingCancelFlags::Any))
			&& !hasFlag(_uflags, URingFlags::AsyncCancelAnyAllSupported)) {
		return Status::ErrorNotImplemented;
	}
	return pushSqe({IORING_OP_ASYNC_CANCEL}, [&](io_uring_sqe *sqe, uint32_t) {
		uint64_t udata = URING_USERDATA_IGNORED;
		uint32_t cFlags = 0;

		if (hasFlag(cancelFlags, URingCancelFlags::Suspend)) {
			udata = URING_USERDATA_SUSPENDED;
		}
		if (hasFlag(cancelFlags, URingCancelFlags::All)) {
			cFlags |= IORING_ASYNC_CANCEL_ALL;
		}
		if (hasFlag(cancelFlags, URingCancelFlags::Any)) {
			cFlags |= IORING_ASYNC_CANCEL_ANY;
		}

		updateIoSqe(sqe, -1, userdata, 0, 0, udata);
		sqe->cancel_flags = cFlags;
	}, URingPushFlags::Submit);
}

Status URingData::cancelFd(int fd, URingCancelFlags cancelFlags) {
	if (!hasFlag(_uflags, URingFlags::AsyncCancelFdSupported)) {
		return Status::ErrorNotImplemented;
	}
	if ((hasFlag(cancelFlags, URingCancelFlags::All) || hasFlag(cancelFlags, URingCancelFlags::Any))
			&& !hasFlag(_uflags, URingFlags::AsyncCancelAnyAllSupported)) {
		return Status::ErrorNotImplemented;
	}
	if (hasFlag(cancelFlags, URingCancelFlags::FixedFile)
			&& !hasFlag(_uflags, URingFlags::AsyncCancelFdFixedSupported)) {
		return Status::ErrorNotImplemented;
	}
	return pushSqe({IORING_OP_ASYNC_CANCEL}, [&](io_uring_sqe *sqe, uint32_t) {
		updateIoSqe(sqe, fd, nullptr, 0, 0, URING_USERDATA_IGNORED);
		sqe->cancel_flags = IORING_ASYNC_CANCEL_FD
				| (hasFlag(cancelFlags, URingCancelFlags::All) ? IORING_ASYNC_CANCEL_ALL : 0)
				| (hasFlag(cancelFlags, URingCancelFlags::Any) ? IORING_ASYNC_CANCEL_ANY : 0)
				| (hasFlag(cancelFlags, URingCancelFlags::FixedFile) ? IORING_ASYNC_CANCEL_FD_FIXED
																	 : 0);
	}, URingPushFlags::Submit);
}

uint32_t URingData::pop() {
	uint32_t count = 0;
	if (_receivedEvents != _processedEvents) {
		auto cqes = _out.data();
		while (_processedEvents < _receivedEvents) {
			auto &cqe = cqes[_processedEvents++];
			processEvent(cqe.res, cqe.flags, cqe.user_data);
			++count;
		}
		_receivedEvents = _processedEvents = 0;
		return count;
	}

	_processedEvents = 0;
	_receivedEvents = atomicLoadAcquire(cq.tail) - *cq.head;
	if (_receivedEvents) {
		auto cqes = _out.data();

		unsigned head = *cq.head;
		unsigned mask = *cq.mask;
		unsigned last;
		unsigned i = 0;

		last = head + _receivedEvents;
		for (; head != last; head++, i++) { cqes[i] = cq.cqes[(head & mask)]; }

		atomicStoreRelease(cq.head, last);

		bool scheduleEnter = isCqePending(atomicLoadRelaxed(sq.flags));
		if (scheduleEnter) {
			_uflags |= URingFlags::PendingGetEvents;
		}

		// processEvent can schedule new submissions
		while (_processedEvents < _receivedEvents) {
			auto &cqe = cqes[_processedEvents++];
			processEvent(cqe.res, cqe.flags, cqe.user_data);
			++count;
		}

		if (hasFlag(_uflags, URingFlags::PendingGetEvents)) {
			enter(0, 0, IORING_ENTER_GETEVENTS, NULL);
		}

		_receivedEvents = _processedEvents = 0;
	}
	return count;
}

void URingData::processEvent(int32_t res, uint32_t flags, uint64_t userdata) {
	auto userFlags = userdata & URING_USERDATA_USER_MASK;
	auto userptr = userdata & URING_USERDATA_PTR_MASK;

	++_tick;

	if (userdata == reinterpret_cast<uintptr_t>(this)) {
		// general timeout
		if (res == -ETIME) {
			if (_runContext) {
				stopContext(nullptr, _runContext->runWakeupFlags, false);
			}
		}
	} else if (userdata == URING_USERDATA_IGNORED) {
		// do nothing
		//std::cout << "URING_USERDATA_IGNORED: " << res << " : " << flags << "\n";
	} else if (userdata == URING_USERDATA_TIMEOUT) {
		// graceful wakeup timeout
		//std::cout << "URING_USERDATA_TIMEOUT: " << res << " : " << flags << "\n";
		if (_runContext) {
			if (_runContext->wakeupCounter != 0 && res == -ETIME) {
				_runContext->state = RunContext::Stopped;
				_runContext->wakeupStatus = Status::ErrorTimerExpired;
			}
		}
	} else if (userdata == URING_USERDATA_SUSPENDED) {
		// graceful wakeup suspend task completed
		if (_data->_info.suspendedHandles == _data->_info.runningHandles && _runContext
				&& _runContext->state == RunContext::Stopping) {
			_runContext->state = RunContext::Stopped;

			if (_runContext->wakeupTimeout) {
				pushSqe({IORING_OP_TIMEOUT_REMOVE}, [&](io_uring_sqe *sqe, uint32_t n) {
					sqe->addr = URING_USERDATA_TIMEOUT;
					sqe->len = 0;
					sqe->off = 0;
					sqe->user_data = URING_USERDATA_IGNORED;
				}, URingPushFlags::Submit);
			}

			_runContext->wakeupStatus = Status::Ok;
		}
	} else if (userdata) {
		if (hasContext(reinterpret_cast<void *>(userptr))) {
			stopContext(reinterpret_cast<RunContext *>(userptr), _runContext->runWakeupFlags,
					false);
			return;
		}

		auto h = reinterpret_cast<Handle *>(userptr);

		bool retainedByRing = hasFlag(userFlags, URING_USERDATA_RETAIN_BIT);

		if (h->isResumable()) {
			if ((h->getTimeline() & URING_USERDATA_SERIAL_MASK)
					!= (userFlags & URING_USERDATA_SERIAL_MASK)) {
				// messages from previous submission

				if (retainedByRing && (flags & IORING_CQE_F_MORE) == 0) {
					h->release(reinterpret_cast<uintptr_t>(this) ^ reinterpret_cast<uintptr_t>(h));
				}
				return;
			}
		}

		uint64_t refId = 0;
		if (!retainedByRing) {
			refId = h->retain();
		}

		NotifyData notifyData = {.result = res,
			.queueFlags = flags,
			.userFlags = static_cast<uint32_t>(userFlags)};

		_data->notify(h, notifyData);

		if (!retainedByRing) {
			h->release(refId);
		}

		// do not release handles, if IORING_CQE_F_MORE flags is set, only release if it's last CQE
		if (retainedByRing && (flags & IORING_CQE_F_MORE) == 0) {
			h->release(reinterpret_cast<uintptr_t>(this) ^ reinterpret_cast<uintptr_t>(h));
		}
	} else {
		log::source().info("URingData", "no userdata: ", res, " ", flags);
	}
}

Status URingData::submit() {
	auto ret = submitSqe(flushSqe(), 0, false);
	if (ret >= 0) {
		return Status::Ok;
	} else {
		return Status::Declined;
	}
}

uint32_t URingData::doPoll() {
	uint32_t ret = 0;
	while (auto v = pop()) { ret += v; }
	return ret;
}

uint32_t URingData::poll() {
	uint32_t ret = 0;
	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	ret = doPoll();

	popContext(&ctx);
	return ret;
}

uint32_t URingData::wait(TimeInterval ival) {
	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	auto events = doPoll();
	if (events == 0 && ival) {
		while (true) {
			_linux_timespec ts;
			setNanoTimespec(ts, ival);

			int err = enter(0, 1, IORING_ENTER_GETEVENTS, ival ? &ts : nullptr);

			events += doPoll();

			if (err < DEBUG_ERROR_THRESHOLD) {
				log::source().error("event::URingData", "io_uring_enter: ", -err);
				break;
			} else if (err >= 0) {
				break;
			}
		}
	}

	popContext(&ctx);
	return events;
}

Status URingData::run(TimeInterval ival, WakeupFlags flags, TimeInterval wakeupTimeout) {
	RunContext ctx;

	ctx.wakeupStatus = Status::Suspended;
	ctx.wakeupTimeout = wakeupTimeout;
	ctx.runWakeupFlags = flags;

	_linux_timespec ts;

	if (ival && ival != TimeInterval::Infinite) {
		// set timeout
		setNanoTimespec(ts, ival);
		pushSqe({IORING_OP_TIMEOUT}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->addr = reinterpret_cast<uintptr_t>(&ts);
			sqe->len = 1;
			sqe->user_data = reinterpret_cast<uintptr_t>(&ctx);
		}, URingPushFlags::Submit);
	}

	pushContext(&ctx, RunContext::Run);

	doPoll();

	while (ctx.state == RunContext::Running || ctx.state == RunContext::Stopping) {
		int err = enter(0, 1, IORING_ENTER_GETEVENTS, nullptr);
		doPoll();
		if (err < DEBUG_ERROR_THRESHOLD) {
			log::source().error("event::URingData", "io_uring_enter: ", -err);
			ctx.wakeupStatus = getErrnoStatus(err);
			break;
		}
	}

	if (ival && ival != TimeInterval::Infinite) {
		// remove timeout if set
		pushSqe({IORING_OP_TIMEOUT_REMOVE}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->addr = reinterpret_cast<uintptr_t>(&ctx);
			sqe->user_data = URING_USERDATA_IGNORED;
		}, URingPushFlags::Submit);
	}

	popContext(&ctx);

	return ctx.wakeupStatus;
}

Status URingData::wakeup(WakeupFlags flags) {
	_eventFd->write(1, toInt(flags));
	return Status::Ok;
}

int URingData::enter(unsigned sub, unsigned wait, unsigned flags, _linux_timespec *ts) {
	const sigset_t *sigset = nullptr;

	if (hasFlag(_flags, QueueFlags::Protected)) {
		sigset = _signalFd->getDefaultSigset();
	}

	_uflags &= ~URingFlags::PendingGetEvents;

	if ((sigset || ts) && (_params.features & IORING_FEAT_EXT_ARG) != 0) {
		struct io_uring_getevents_arg arg = {.sigmask = reinterpret_cast<uint64_t>(sigset),
			.sigmask_sz = sigset ? uint32_t(_NSIG / 8) : uint32_t(0),
			.ts = reinterpret_cast<uint64_t>(ts)};

		return __sprt_io_uring_enter2(_ringFd, sub, wait, flags | IORING_ENTER_EXT_ARG, &arg,
				sizeof(io_uring_getevents_arg));
	} else {
		return __sprt_io_uring_enter(_ringFd, sub, wait, flags, sigset);
	}
}

void URingData::runInternalHandles() {
	// run internal services
	if (_signalFd && hasFlag(_flags, QueueFlags::Protected)) {
		// enable with current mask
		_signalFd->enable();
		_data->runHandle(_signalFd);
	}

	_data->runHandle(_eventFd);
}

void URingData::cancel() {
	if (_runContext) {
		_eventFd->write(1, toInt(WakeupFlags::ContextDefault) | URING_CANCEL_FLAG);
	}
}

URingData::URingData(QueueRef *q, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs)
: PlatformQueueData(q, data, info.flags) {

	_suspend = [](RunContext *ctx) {
		if (ctx->wakeupCounter == 0) {
			static_cast<URingData *>(ctx->queue)->submitPending();
			return Status::Done;
		}

		// we will receive reports from all commands, or it will be forced wakeup on timeout
		if (ctx->wakeupTimeout) {
			setNanoTimespec(ctx->wakeupTimespec, ctx->wakeupTimeout);
			static_cast<URingData *>(ctx->queue)
					->pushSqe({IORING_OP_TIMEOUT}, [&](io_uring_sqe *sqe, uint32_t n) {
				sqe->addr = reinterpret_cast<uintptr_t>(&ctx->wakeupTimespec);
				sqe->len = 1;
				sqe->off = 0;
				sqe->timeout_flags = 0;
				sqe->user_data = URING_USERDATA_TIMEOUT;
				sqe->flags = 0;
			}, URingPushFlags::None);
			static_cast<URingData *>(ctx->queue)->submitPending();
		}

		return Status::Ok;
	};

	_eventFd = Rc<EventFdURingHandle>::create(&data->_uringEventFdClass,
			CompletionHandle<EventFdURingHandle>::create<URingData>(this,
					[](URingData *data, EventFdURingHandle *h, uint32_t value, Status st) {
		if (st == Status::Ok) {
			if (hasFlag(value, URING_CANCEL_FLAG)) {
				data->stopRootContext(WakeupFlags(value), true);
			} else {
				data->stopContext(data->_runContext, WakeupFlags(value), true);
			}
		}
	}));

	if (!_eventFd) {
		log::source().error("event::Queue", "Fail to initialize eventfd");
		return;
	}

	if (hasFlag(_flags, QueueFlags::Protected)) {
		_signalFd = Rc<SignalFdURingHandle>::create(&data->_uringSignalFdClass, sigs);
		if (!_signalFd) {
			log::source().error("event::Queue", "Fail to initialize signalfd");
			return;
		}
	}

	int ringFd = -1;

	auto cleanup = [&]() {
		if (ringFd >= 0) {
			::close(ringFd);
		}
	};

	memset(&_params, 0, sizeof(_params));

	// check available features
	struct __SPRT_UTSNAME_NAME buffer;
	if (__sprt_uname(&buffer) != 0) {
		log::source().info("event::URingData", "Fail to detect kernel version");
		return;
	}

	if (hasFlag(_flags, QueueFlags::SubmitImmediate)) {
		_params.flags |= IORING_SETUP_SQPOLL;

		if (!info.osIdleInterval) {
			_params.sq_thread_idle = DefaultIdleInterval;
		} else {
			_params.sq_thread_idle = static_cast<uint32_t>(info.osIdleInterval.toMillis());
		}
	}

#ifdef IORING_SETUP_SUBMIT_ALL
	if (strverscmp(buffer.release, "5.18.0") >= 0) {
		_uflags |= URingFlags::SubmitAllSupported;
		_params.flags |= IORING_SETUP_SUBMIT_ALL;
	}
#endif

#ifdef IORING_SETUP_COOP_TASKRUN
	if (strverscmp(buffer.release, "5.19.0") >= 0) {
		_uflags |= URingFlags::CoopTaskrunSupported;
		_uflags |= URingFlags::AsyncCancelFdSupported;
		if (!hasFlag(_flags, QueueFlags::SubmitImmediate)) {
			_params.flags |= (IORING_SETUP_COOP_TASKRUN | IORING_SETUP_TASKRUN_FLAG);
		}
	}
#endif

#ifdef IORING_SETUP_SINGLE_ISSUER
	if (strverscmp(buffer.release, "6.0.0") >= 0) {
		_uflags |= URingFlags::SingleIssuerSupported;
		_uflags |= URingFlags::AsyncCancelFdFixedSupported;
		_uflags |= URingFlags::InternalFdsSupported;
		_params.flags |= IORING_SETUP_SINGLE_ISSUER;
	}
#endif

#ifdef IORING_SETUP_DEFER_TASKRUN
	if (strverscmp(buffer.release, "6.1.0") >= 0) {
		_uflags |= URingFlags::DeferTaskrunSupported;
		if (!hasFlag(_flags, QueueFlags::SubmitImmediate)) {
			_params.flags |= (IORING_SETUP_DEFER_TASKRUN);
		}
	}
#endif

	if (strverscmp(buffer.release, "6.4.0") >= 0) {
		_uflags |= URingFlags::TimerMultishotSupported;
	}

	if (strverscmp(buffer.release, "6.7.0") >= 0) {
		_uflags |= URingFlags::FutexSupported;
		_uflags |= URingFlags::ReadMultishotSupported;
	}

	if (info.completeQueueSize != 0) {
		_params.flags |= IORING_SETUP_CQSIZE;
		_params.cq_entries = info.completeQueueSize;
	}

	_params.flags |= IORING_SETUP_CLAMP;

	ringFd = __sprt_io_uring_setup(math::npot(info.submitQueueSize), &_params);
	if (ringFd < 0) {
		log::source().error("event::URingData",
				"io_uring_setup: Fail to setup io_uring instance: ", errno);
		return;
	}

	mem_std::StringStream features;
	if (_params.features & IORING_FEAT_SINGLE_MMAP) {
		features << " " << "IORING_FEAT_SINGLE_MMAP";
	}
	if (_params.features & IORING_FEAT_NODROP) {
		features << " " << "IORING_FEAT_NODROP";
	}
	if (_params.features & IORING_FEAT_SUBMIT_STABLE) {
		features << " " << "IORING_FEAT_SUBMIT_STABLE";
	}
	if (_params.features & IORING_FEAT_RW_CUR_POS) {
		features << " " << "IORING_FEAT_RW_CUR_POS";
	}
	if (_params.features & IORING_FEAT_CUR_PERSONALITY) {
		features << " " << "IORING_FEAT_CUR_PERSONALITY";
	}
	if (_params.features & IORING_FEAT_FAST_POLL) {
		features << " " << "IORING_FEAT_FAST_POLL";
	}
	if (_params.features & IORING_FEAT_POLL_32BITS) {
		features << " " << "IORING_FEAT_POLL_32BITS";
	}
	if (_params.features & IORING_FEAT_SQPOLL_NONFIXED) {
		features << " " << "IORING_FEAT_SQPOLL_NONFIXED";
	}
	if (_params.features & IORING_FEAT_EXT_ARG) {
		features << " " << "IORING_FEAT_EXT_ARG";
	}
	if (_params.features & IORING_FEAT_NATIVE_WORKERS) {
		features << " " << "IORING_FEAT_NATIVE_WORKERS";
	}
	if (_params.features & IORING_FEAT_RSRC_TAGS) {
		features << " " << "IORING_FEAT_RSRC_TAGS";
	}

#ifdef IORING_FEAT_CQE_SKIP
	if (_params.features & IORING_FEAT_CQE_SKIP) {
		features << " " << "IORING_FEAT_CQE_SKIP";
	}
#endif
#ifdef IORING_FEAT_LINKED_FILE
	if (_params.features & IORING_FEAT_LINKED_FILE) {
		features << " " << "IORING_FEAT_LINKED_FILE";
	}
#endif
#ifdef IORING_FEAT_REG_REG_RING
	if (_params.features & IORING_FEAT_REG_REG_RING) {
		features << " " << "IORING_FEAT_REG_REG_RING";
	}
#endif
#ifdef IORING_FEAT_RECVSEND_BUNDLE
	if (_params.features & IORING_FEAT_RECVSEND_BUNDLE) {
		features << " " << "IORING_FEAT_RECVSEND_BUNDLE";
	}
#endif

	// log::source().info("event::URingData", "io_uring features: ", features.str());

	sq.ringSize = _params.sq_off.array + _params.sq_entries * sizeof(unsigned);
	cq.ringSize = _params.cq_off.cqes + _params.cq_entries * CQESize;

	if (_params.features & IORING_FEAT_SINGLE_MMAP) {
		cq.ringSize = sq.ringSize = std::max(sq.ringSize, cq.ringSize);
	}

	sq.ring = (uint8_t *)::mmap(0, sq.ringSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
			ringFd, IORING_OFF_SQ_RING);
	if (sq.ring == MAP_FAILED) {
		log::source().error("event::URingData", "Fail to mmap SQ");
		cleanup();
		return;
	}

	if (_params.features & IORING_FEAT_SINGLE_MMAP) {
		cq.ring = sq.ring;
	} else {
		/* Map in the completion queue ring buffer in older kernels separately */
		cq.ring = (uint8_t *)::mmap(0, cq.ringSize, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_POPULATE, ringFd, IORING_OFF_CQ_RING);
		if (cq.ring == MAP_FAILED) {
			log::source().error("event::URingData", "Fail to mmap CQ");
			cleanup();
			return;
		}
	}

	/* Map in the submission queue entries array */
	sq.sqes = (io_uring_sqe *)::mmap(0, _params.sq_entries * sizeof(io_uring_sqe),
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ringFd, IORING_OFF_SQES);
	if (sq.sqes == MAP_FAILED) {
		log::source().error("event::URingData", "Fail to mmap SQE");
		cleanup();
		return;
	}

	// Register probe
	memset(&_probe, 0, sizeof(URingProbe));
	auto err = __sprt_io_uring_register(ringFd, IORING_REGISTER_PROBE, &_probe,
			URingProbe::OpcodeCount);
	if (err < 0) {
		log::source().error("event::URingData", "Fail to register probe: ", err);
		cleanup();
		return;
	}

	sq.head = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.head);
	sq.tail = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.tail);
	sq.mask = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.ring_mask);
	sq.entries = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.ring_entries);
	sq.flags = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.flags);
	sq.dropped = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.dropped);
	sq.array = reinterpret_cast<unsigned *>(sq.ring + _params.sq_off.array);

	cq.head = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.head);
	cq.tail = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.tail);
	cq.mask = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.ring_mask);
	cq.entries = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.ring_entries);
	cq.overflow = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.overflow);
	cq.cqes = reinterpret_cast<io_uring_cqe *>(cq.ring + _params.cq_off.cqes);
	if (_params.cq_off.flags) {
		cq.flags = reinterpret_cast<unsigned *>(cq.ring + _params.cq_off.flags);
	}

	_out.resize(_params.cq_entries);

	for (unsigned index = 0; index < *sq.entries; index++) { sq.array[index] = index; }

	// allocate internal FD table
	auto totalHandles = info.externalHandles + info.internalHandles;

	if (totalHandles > 0) {
		_fds.resize(totalHandles);
		_tags.resize(totalHandles);

		io_uring_rsrc_register fdTableReg;
		fdTableReg.nr = totalHandles;
		fdTableReg.flags = 0;
		fdTableReg.resv2 = 0;
		fdTableReg.data = reinterpret_cast<uintptr_t>(_fds.data());
		fdTableReg.tags = reinterpret_cast<uintptr_t>(_tags.data());

		err = __sprt_io_uring_register(ringFd, IORING_REGISTER_FILES2, &fdTableReg,
				sizeof(io_uring_rsrc_register));
		if (err < 0) {
			log::source().error("event::URingData", "Fail to set fd table");
			cleanup();
			return;
		}

		if (info.internalHandles > 0 && hasFlag(_uflags, URingFlags::InternalFdsSupported)) {
			struct io_uring_file_index_range {
				_ring_u32 off;
				_ring_u32 len;
				_ring_u64 resv;
			};

			io_uring_file_index_range range;
			range.off = 0;
			range.len = info.internalHandles;
			range.resv = 0;

			err = __sprt_io_uring_register(ringFd, IORING_REGISTER_FILE_ALLOC_RANGE, &range, 0);
			if (err < 0) {
				log::source().error("event::URingData", "Fail to register file alloc range");
				cleanup();
				return;
			}
		}
	}

	_ringFd = ringFd;
}

URingData::~URingData() {
	_signalFd = nullptr;
	_eventFd = nullptr;

	if (_ringFd >= 0) {
		::close(_ringFd);
		_ringFd = -1;
	}
}

} // namespace stappler::event
