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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTPROCESSFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTPROCESSFD_H_

#include "SPEventFd.h"
#include "SPEventProcess.h"
#include "../../detail/SPRuntimeDispatchHandleClass.h"

namespace sprt::dispatch {

// pidfd-backed source: the pidfd becomes readable once the child exits.
struct ProcessFdSource {
	int pidfd = -1;
	int pid = -1;
	epoll_event event;
	PollFlags flags;
	bool exited = false; // child reaped via the exit path; cancel() must not kill a recycled pid

	bool init(int pidfd, int pid);
	void cancel();
};

class SPRT_API ProcessFdHandle : public ProcessHandle {
public:
	virtual ~ProcessFdHandle() = default;

	bool init(HandleClass *, int pidfd, int pid, CompletionHandle<ProcessHandle> &&);

	virtual NativeHandle getNativeHandle() const override;
};

class SPRT_API ProcessFdURingHandle : public ProcessFdHandle {
public:
	virtual ~ProcessFdURingHandle() = default;

	Status rearm(URingData *, ProcessFdSource *);
	Status disarm(URingData *, ProcessFdSource *);

	void notify(URingData *, ProcessFdSource *, const NotifyData &);
};

class SPRT_API ProcessFdEPollHandle : public ProcessFdHandle {
public:
	virtual ~ProcessFdEPollHandle() = default;

	Status rearm(EPollData *, ProcessFdSource *);
	Status disarm(EPollData *, ProcessFdSource *);

	void notify(EPollData *, ProcessFdSource *, const NotifyData &);
};

// Launch `command` via /bin/sh -c with stdout+stderr merged onto a pipe.
// On success, returns true and writes the child pid and the (non-blocking,
// close-on-exec) read end of the pipe.
bool posixSpawnPipe(StringView command, int *outPid, int *outReadFd);

// Create + run the reader sub-handle over `readFd`, reusing the backend's
// pollable-fd path (data->listenHandle). Output is forwarded to state->reader.
Rc<PollHandle> createProcessReader(QueueData *data, int readFd, ProcessState *state);

// Full spawn for the fd/pidfd backends (epoll & io_uring). `processClass` is the
// per-backend process HandleClass; `uring` selects the concrete handle type.
Rc<ProcessHandle> spawnProcessFd(QueueData *data, HandleClass *processClass, bool uring,
		ProcessInfo &&info, Ref *ref);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTPROCESSFD_H_ */
