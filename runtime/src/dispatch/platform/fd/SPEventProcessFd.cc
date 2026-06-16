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

#include "SPEventProcessFd.h"
#include "../epoll/SPEvent-epoll.h"
#include "../uring/SPEvent-uring.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/c/cross/__sprt_syscall.h>

#include <unistd.h>

#if defined(__SPRT_SYSCALL_pidfd_open) || defined(__SPRT_SYSCALL_wait4)
__SPRT_C_FUNC long int syscall(long int __sysno, ...);
#endif

namespace sprt {

#ifdef __SPRT_SYSCALL_wait4

static int __SPRT_ID(wait4)(int pid, int *status, int options, void *rusage) {
	return (int)syscall(__SPRT_SYSCALL_wait4, pid, status, options, rusage);
}

#endif

#ifdef __SPRT_SYSCALL_pidfd_open

static int __SPRT_ID(pidfd_open)(int pid, unsigned int flags) {
	return (int)syscall(__SPRT_SYSCALL_pidfd_open, pid, flags);
}

#endif

} // namespace sprt

namespace sprt::dispatch {

bool ProcessFdSource::init(int pfd, int p) {
	pidfd = pfd;
	pid = p;
	flags = PollFlags::In;
	return true;
}

void ProcessFdSource::cancel() {
	if (pidfd >= 0) {
		::close(pidfd);
		pidfd = -1;
	}
}

bool ProcessFdHandle::init(HandleClass *cl, int pidfd, int pid,
		CompletionHandle<ProcessHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	auto source = new (_data) ProcessFdSource;
	return source->init(pidfd, pid);
}

NativeHandle ProcessFdHandle::getNativeHandle() const {
	return reinterpret_cast<const ProcessFdSource *>(_data)->pidfd;
}

Status ProcessFdURingHandle::rearm(URingData *uring, ProcessFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		uring->pushSqe({IORING_OP_POLL_ADD}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = source->pidfd;
			sqe->poll_events = toInt(source->flags & PollFlags::PollMask);
			sqe->user_data = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
					| (_timeline & URING_USERDATA_SERIAL_MASK);
		}, URingPushFlags::Submit);
	}
	return status;
}

Status ProcessFdURingHandle::disarm(URingData *uring, ProcessFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		uring->pushSqe({IORING_OP_POLL_REMOVE}, [&](io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = source->pidfd;
			sqe->user_data = URING_USERDATA_IGNORED;
		}, URingPushFlags::Submit);
		++_timeline;
	}
	return status;
}

void ProcessFdURingHandle::notify(URingData *uring, ProcessFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.result < 0 && data.result != -EAGAIN) {
		cancel(URingData::getErrnoStatus(data.result));
		return;
	}
	if (data.result == -EAGAIN) {
		// spurious wakeup: the one-shot poll was consumed, re-arm it
		_status = Status::Suspended;
		rearm(uring, source);
		return;
	}

	// the pidfd is readable: the child has exited. Reap it (clears the zombie and
	// yields the wait-status) and complete with the decoded exit code.
	int status = 0;
	__sprt_wait4(source->pid, &status, 0, nullptr);
	_exitCode = decodeWaitStatus(status);

	auto state = static_cast<ProcessState *>(getUserdata());
	if (state) {
		drainProcessPipe(state->readFd, state); // flush any final output first
		state->readFd = -1;
		if (state->readerHandle) {
			state->readerHandle->cancel();
		}
	}

	// the one-shot poll is already consumed, so skip the disarm SQE
	_status = Status::Suspended;
	cancel(Status::Done, uint32_t(_exitCode));
}

Status ProcessFdEPollHandle::rearm(EPollData *epoll, ProcessFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = __SPRT_EPOLLIN;
		status = epoll->add(source->pidfd, source->event);
	}
	return status;
}

Status ProcessFdEPollHandle::disarm(EPollData *epoll, ProcessFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->pidfd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ProcessFdEPollHandle::notify(EPollData *epoll, ProcessFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// the pidfd is readable: the child has exited. Reap it and complete.
	int status = 0;
	__sprt_wait4(source->pid, &status, 0, nullptr);
	_exitCode = decodeWaitStatus(status);

	auto state = static_cast<ProcessState *>(getUserdata());
	if (state) {
		drainProcessPipe(state->readFd, state); // flush any final output first
		state->readFd = -1;
		if (state->readerHandle) {
			state->readerHandle->cancel();
		}
	}

	// _status is still Ok, so cancel() will run suspendFn (epoll->remove) to
	// unregister the pidfd before completing.
	cancel(Status::Done, uint32_t(_exitCode));
}

Rc<ProcessHandle> spawnProcessFd(QueueData *data, HandleClass *processClass, bool uring,
		ProcessInfo &&info, Ref *ref) {
	int pid = -1;
	int readFd = -1;
	if (!posixSpawnPipe(info.command, &pid, &readFd)) {
		return nullptr;
	}

	// pidfd_open() always returns a close-on-exec fd (the kernel ORs in O_CLOEXEC
	// unconditionally), so it cannot leak into a subsequently spawned child.
	int pidfd = __sprt_pidfd_open(pid, 0);
	if (pidfd < 0) {
		// cannot watch the child for exit: reap it and bail
		::close(readFd);
		int status = 0;
		__sprt_wait4(pid, &status, 0, nullptr);
		return nullptr;
	}

	auto state = Rc<ProcessState>::alloc();
	state->reader = sprt::move(info.reader);
	state->userRef = ref;
	state->readFd = readFd;

	Rc<ProcessHandle> proc;
	if (uring) {
		proc = Rc<ProcessFdURingHandle>::create(processClass, pidfd, pid,
				sprt::move(info.completion));
	} else {
		proc = Rc<ProcessFdEPollHandle>::create(processClass, pidfd, pid,
				sprt::move(info.completion));
	}
	if (!proc) {
		::close(readFd);
		::close(pidfd);
		int status = 0;
		__sprt_wait4(pid, &status, 0, nullptr);
		return nullptr;
	}

	// the process handle owns ProcessState (userdata); ProcessState owns the
	// reader sub-handle; the reader's completion references ProcessState by raw
	// pointer only (no cycle).
	proc->setUserdata(state);
	state->readerHandle = createProcessReader(data, readFd, state.get());
	return proc;
}

} // namespace sprt::dispatch
