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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTPROCESS_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTPROCESS_H_

#include <sprt/runtime/dispatch/handle.h>

// Platform-neutral pieces shared by every POSIX process backend (Linux pidfd,
// macOS/BSD kqueue). Only the exit-wait primitive differs per backend; spawning
// the child, reading its output, and decoding its exit status are common.

namespace sprt::dispatch {

struct QueueData;

// Shared, cross-handle state for a spawned process. Held alive as the process
// handle's userdata; the reader sub-handle references it via a raw pointer in
// its (non-owning) completion userdata, so there is no reference cycle.
struct SPRT_API ProcessState : public Ref {
	ProcessInfo::ReaderCallback reader; // user output callback (merged stdout/stderr)
	Rc<Ref> userRef; // user-provided ref / convenience-callback holder
	Rc<PollHandle> readerHandle; // the pipe reader sub-handle
	int readFd = -1; // pipe read end; -1 once closed (POSIX backends)
	// (Windows IOCP) pool-allocated UTF-16 path of a temporary argument response file created when the
	// command was too long for the OS command-line limit; DeleteFileW'd on completion. null otherwise
	// (and always null on POSIX backends).
	void *tempRespFile = nullptr;
};

// Non-blocking drain of the read pipe into state->reader. Returns true once EOF
// is reached (all write ends closed), false when merely drained for now (EAGAIN).
bool drainProcessPipe(int fd, ProcessState *state);

// Decode a wait(2)-style status into an exit code (128 + signal if killed).
int decodeWaitStatus(int status);

// Launch `command` via /bin/sh -c with stdout+stderr merged onto a pipe. On
// success returns true and writes the child pid and the (non-blocking,
// close-on-exec) read end of the pipe.
bool posixSpawnPipe(StringView command, int *outPid, int *outReadFd);

// Forcibly terminate a child (SIGKILL) and reap its zombie. Every backend calls this
// from its cancel path when a process handle is cancelled while the child is still
// running, so the child neither outlives its handle nor leaks as a zombie. The signal
// and reap primitives are platform-correct (libSystem on macOS, raw syscalls on Linux).
//
// The caller MUST guarantee the child has not already been reaped (each backend tracks
// this with an `exited` flag set on the normal-exit reap); otherwise the pid may have
// been recycled and an unrelated process would be signalled.
void killProcessChild(int pid);

// Create + run the reader sub-handle over `readFd`, reusing the backend's
// pollable-fd path (QueueData::listenHandle). Output is forwarded to state->reader.
Rc<PollHandle> createProcessReader(QueueData *data, int readFd, ProcessState *state);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTPROCESS_H_ */
