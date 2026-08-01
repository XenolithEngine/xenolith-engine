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

#include "SPEventProcess.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/c/__sprt_errno.h>

#include <signal.h> // SIGKILL
#include <unistd.h>
#include <fcntl.h>

#if SPRT_APPLE
extern "C" int waitpid(int __pid, int *__status, int __options);
#else
// Linux/Android: reach the kernel directly (the freestanding libc offers no kill()/wait4()),
// mirroring how SPEventProcessFd.cc issues pidfd_open()/wait4().
#include <sprt/c/cross/__sprt_syscall.h>
__SPRT_C_FUNC long int syscall(long int __sysno, ...);
#endif

namespace sprt::dispatch {

void killProcessChild(int pid) {
	if (pid <= 0) {
		return;
	}
	// SIGKILL is uncatchable, so the child dies at once; the blocking reap that follows
	// returns immediately and clears the zombie. The caller guarantees the child has not
	// already been reaped (see the header), so this never signals a recycled pid.
	//
	// TODO: this kills only the direct child (the `/bin/sh -c` pid). A shell that forks
	// grandchildren leaves them orphaned (reparented to init) and still running. To kill
	// the whole tree we would put the child in its own process group (setpgid() in
	// posixSpawnPipe) and signal the group here via kill(-pgid, SIGKILL) / killpg(); the
	// Windows analogue (SPEventProcessIocp.cc) would assign the child to a Job Object and
	// terminate that instead of a single TerminateProcess.
	int status = 0;
#if SPRT_APPLE
	::kill(pid, SIGKILL);
	::waitpid(pid, &status, 0);
#else
	syscall(__SPRT_SYSCALL_kill, pid, SIGKILL);
	syscall(__SPRT_SYSCALL_wait4, pid, &status, 0, nullptr);
#endif
}

bool drainProcessPipe(int fd, ProcessState *state) {
	if (fd < 0) {
		return true;
	}
	char buf[4'096];
	for (;;) {
		auto n = ::read(fd, buf, sizeof(buf));
		if (n > 0) {
			if (state->reader) {
				state->reader(StringView(buf, size_t(n)));
			}
			continue;
		}
		if (n == 0) {
			return true; // EOF: child closed its stdout/stderr
		}
		auto e = __sprt_errno;
		if (e == EINTR) {
			continue;
		}
		if (e == EAGAIN || e == EWOULDBLOCK) {
			return false; // nothing more for now
		}
		return true; // unexpected error: treat as finished
	}
}

int decodeWaitStatus(int status) {
	if ((status & 0x7f) == 0) {
		return (status >> 8) & 0xff;
	}
	return 128 + (status & 0x7f);
}

// Completion for the reader sub-handle: forward output, stop on EOF/hangup.
static void processReaderNotify(ProcessState *state, PollHandle *h, uint32_t value, Status st) {
	if (st != Status::Ok) {
		return;
	}
	auto fl = PollFlags(value);
	bool eof = false;
	if (state->readFd >= 0
			&& (hasFlag(fl, PollFlags::In) || hasFlag(fl, PollFlags::Pri)
					|| hasFlag(fl, PollFlags::HungUp))) {
		eof = drainProcessPipe(state->readFd, state);
	}
	if (eof || hasFlag(fl, PollFlags::HungUp) || hasFlag(fl, PollFlags::Err)) {
		state->readFd = -1; // the handle's CloseFd flag closes the fd on cancel
		h->cancel();
	}
}

bool posixSpawnPipe(StringView command, int *outPid, int *outReadFd) {
	int fds[2];

	// Plain pipe(), not pipe2(O_CLOEXEC): the latter was observed to fail (ENOENT) in practice.
	// Close-on-exec is instead applied explicitly, after fork — so the bare pipe fds stay
	// inheritable across the fork (the child needs them) but the parent's retained read end
	// cannot leak into a subsequently spawned child. Because the fds are not close-on-exec at
	// creation, the child's redirected stdout/stderr survive exec naturally (no dup2 corner case).
	if (::pipe(fds) != 0) {
		return false;
	}

	// Parent's read end is non-blocking so the reactor can drain it without stalling.
	::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);

	// Null-terminated copy for execl; built before fork so the child only reads it.
	String cmd(command.data(), command.size());

	auto pid = ::fork();
	if (pid < 0) {
		::close(fds[0]);
		::close(fds[1]);
		return false;
	}
	if (pid == 0) {
		// child: merge stdout+stderr onto the pipe write end, then drop the bare pipe fds so
		// they do not outlive exec (they are not close-on-exec). The `> 2` guards avoid closing
		// a descriptor that became 1/2 when stdin/out/err were closed at spawn time.
		// (async-signal-safe path only: dup2/close/execl/_exit)
		::dup2(fds[1], 1); // STDOUT_FILENO
		::dup2(fds[1], 2); // STDERR_FILENO
		if (fds[0] > 2) {
			::close(fds[0]);
		}
		if (fds[1] > 2) {
			::close(fds[1]);
		}
		::execl("/bin/sh", "sh", "-c", cmd.data(), (char *)nullptr);
		::_exit(127);
	}

	// Parent: mark the retained read end close-on-exec *after* the fork (so it cannot leak into a
	// later spawn) and drop the write end.
	::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
	::close(fds[1]);
	*outPid = int(pid);
	*outReadFd = fds[0];
	return true;
}

Rc<PollHandle> createProcessReader(QueueData *data, int readFd, ProcessState *state) {
	auto h = data->listenHandle(readFd, PollFlags::In | PollFlags::HungUp | PollFlags::CloseFd,
			CompletionHandle<PollHandle>::create<ProcessState>(state, processReaderNotify));
	if (h) {
		data->runHandle(h);
	}
	return h;
}

} // namespace sprt::dispatch
