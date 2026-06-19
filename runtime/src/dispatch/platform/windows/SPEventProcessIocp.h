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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPROCESSIOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPROCESSIOCP_H_

#include "SPEvent-iocp.h"
#include "../fd/SPEventProcess.h"

namespace sprt::dispatch {

class ProcessIocpHandle;

// Heap-allocated read state for the overlapped pipe reader: a stable OVERLAPPED
// plus its read buffer (kept off the 40-byte Source).
struct ReadIocpState {
	OVERLAPPED ov;
	char buf[8192];
	// Back-reference to the process-exit handle so the reader can complete it once the pipe drains to
	// EOF. Raw (the process handle owns ProcessState which owns this reader; no cycle). Pool-allocated
	// alongside this struct, outliving both handles.
	ProcessIocpHandle *proc = nullptr;
	// Output transcoder carry-over (see emitChildOutput): the trailing bytes of a UTF-8 sequence that
	// was split across a read boundary, held to be prepended to the next chunk so the split character
	// is not misjudged as invalid. At most 3 bytes (a 4-byte lead missing its last continuation).
	unsigned char encPending[4] = {};
	uint8_t encPendingLen = 0;
};

// Overlapped-read reader sub-handle: issues ReadFile on the child's stdout/stderr
// pipe, associated with the IOCP, and forwards each completion to the reader.
struct ReadIocpSource {
	void *hRead = nullptr;
	ReadIocpState *io = nullptr;
	ProcessState *state = nullptr; // raw (owned via the process handle's userdata); no cycle
	bool associated = false;

	bool init(void *hRead, ReadIocpState *io, ProcessState *state);
	void cancel(Handle *);
};

class SPRT_API ReadIocpHandle : public PollHandle {
public:
	virtual ~ReadIocpHandle() = default;

	bool init(HandleClass *, void *hRead, ReadIocpState *, ProcessState *,
			CompletionHandle<PollHandle> &&);

	Status rearm(IocpData *, ReadIocpSource *);
	Status disarm(IocpData *, ReadIocpSource *);

	void notify(IocpData *, ReadIocpSource *, const NotifyData &);

	virtual NativeHandle getNativeHandle() const override;
	virtual bool reset(PollFlags) override;

protected:
	Status issueRead(ReadIocpSource *);

	// Lifetime reference for the single outstanding overlapped read. An IOCP completion (including
	// the aborted one produced by closing the pipe during teardown) is dequeued on a later loop
	// iteration; this reference keeps the handle alive until that completion drains, so the queue
	// never dereferences a freed handle via the stale completion key.
	uint64_t _opRefId = 0;
	bool _opPending = false;
};

// Process-exit handle: waits for the process HANDLE to be signalled and posts a completion to the
// IOCP. Primary path uses ReportEventAsCompletion (NtAssociateWaitCompletionPacket); when that
// family is unavailable (e.g. Wine), it falls back to the legacy RegisterWaitForSingleObject whose
// thread-pool callback PostQueuedCompletionStatus()es the port.
struct ProcessIocpSource {
	void *hProcess = nullptr;
	void *event = nullptr; // primary: I/O packet from ReportEventAsCompletion
	void *wait = nullptr; // fallback: RegisterWaitForSingleObject wait handle
	void *port = nullptr; // fallback: the IOCP port (for the thread-pool callback)
	int pid = 0;

	bool init(void *hProcess, int pid);
	void cancel(Handle *);
};

class SPRT_API ProcessIocpHandle : public ProcessHandle {
public:
	virtual ~ProcessIocpHandle() = default;

	bool init(HandleClass *, void *hProcess, int pid, CompletionHandle<ProcessHandle> &&);

	Status rearm(IocpData *, ProcessIocpSource *);
	Status disarm(IocpData *, ProcessIocpSource *);

	void notify(IocpData *, ProcessIocpSource *, const NotifyData &);

	// Called by the pipe reader once it has drained the child's output to EOF. Completes the process
	// if the child has already exited (otherwise the exit notify completes it).
	void onReaderDrained();

	virtual NativeHandle getNativeHandle() const override;

protected:
	// Complete the process (exit code already captured). The reader has finished (or never existed),
	// so there is nothing left to drain.
	void finishProcess();

	bool _childExited = false; // the process HANDLE was signalled (exit code captured)
	bool _readerDrained = false; // the output pipe reached EOF (all output delivered)
};

// Full spawn for the IOCP backend: CreatePipe + CreateProcessW + overlapped reader
// + process-exit wait.
Rc<ProcessHandle> spawnProcessIocp(QueueData *data, HandleClass *processClass,
		HandleClass *readClass, ProcessInfo &&info, Ref *ref);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENTPROCESSIOCP_H_ */
