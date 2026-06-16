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

#include "SPEventProcessIocp.h"
#include "SPEvent-windows.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/basic_api.h>

namespace sprt::dispatch {

// ---- overlapped pipe reader -------------------------------------------------

bool ReadIocpSource::init(void *h, ReadIocpState *i, ProcessState *s) {
	hRead = h;
	io = i;
	state = s;
	associated = false;
	return true;
}

void ReadIocpSource::cancel(Handle *) {
	if (hRead) {
		// closing the handle cancels any pending overlapped read
		CloseHandle(hRead);
		hRead = nullptr;
	}
	io = nullptr; // pool-owned
	state = nullptr;
}

bool ReadIocpHandle::init(HandleClass *cl, void *hRead, ReadIocpState *io, ProcessState *state,
		CompletionHandle<PollHandle> &&c) {
	static_assert(sizeof(ReadIocpSource) <= DataSize
			&& sprt::is_standard_layout<ReadIocpSource>::value);
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	auto source = new (_data) ReadIocpSource;
	return source->init(hRead, io, state);
}

NativeHandle ReadIocpHandle::getNativeHandle() const {
	return reinterpret_cast<const ReadIocpSource *>(_data)->hRead;
}

bool ReadIocpHandle::reset(PollFlags) { return Handle::reset(); }

Status ReadIocpHandle::issueRead(ReadIocpSource *source) {
	sprt::memset(&source->io->ov, 0, sizeof(OVERLAPPED));
	BOOL ok = ReadFile(source->hRead, source->io->buf, sizeof(source->io->buf), nullptr,
			&source->io->ov);
	if (!ok) {
		auto err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			// ERROR_BROKEN_PIPE / ERROR_HANDLE_EOF: the child closed its end; no completion follows
			return sprt::status::lastErrorToStatus(err);
		}
	}
	// success or pending: a completion WILL be posted for this read. Hold a reference until it is
	// dequeued (see notify()), so the handle survives a completion delivered after teardown.
	_opRefId = sprt::retain(this);
	_opPending = true;
	return Status::Ok;
}

Status ReadIocpHandle::rearm(IocpData *iocp, ReadIocpSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!source->associated) {
			if (!CreateIoCompletionPort(source->hRead, iocp->_port,
						reinterpret_cast<uintptr_t>(this), 0)) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
			source->associated = true;
		}
		status = issueRead(source);
	}
	return status;
}

Status ReadIocpHandle::disarm(IocpData *iocp, ReadIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		// the pending read is cancelled when the handle is closed in source->cancel()
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ReadIocpHandle::notify(IocpData *iocp, ReadIocpSource *source, const NotifyData &data) {
	// This completion corresponds to the outstanding read; capture its lifetime reference and drop
	// it at the very end (which may free the handle). Read the members before any cancel(), since
	// cancel() destructs the source (but not the handle members).
	bool hadOp = _opPending;
	auto opId = _opRefId;
	_opPending = false;

	if (_status == Status::Ok) {
		if (data.result > 0) {
			if (source->state && source->state->reader) {
				source->state->reader(StringView(source->io->buf, size_t(data.result)));
			}
			// queue the next read; a closed pipe ends the stream (no further completion)
			if (issueRead(source) != Status::Ok) {
				cancel();
			}
		} else {
			// zero bytes transferred: EOF
			cancel();
		}
	}

	if (hadOp) {
		sprt::release(this, opId); // releases the completed read's reference; do nothing after
	}
}

// ---- process-exit wait ------------------------------------------------------

// Context for the legacy RegisterWaitForSingleObject fallback callback. Pool-allocated; outlives the
// (one-shot) wait, which UnregisterWaitEx drains before teardown. Holds only stable values so the
// thread-pool callback can post to the IOCP without touching the handle.
struct ProcessWaitCtx {
	void *port = nullptr;
	void *key = nullptr;
};

static void NTAPI processWaitCallback(PVOID param, BOOLEAN /*timedOut*/) {
	auto ctx = reinterpret_cast<ProcessWaitCtx *>(param);
	// minimal, thread-safe: just wake the loop; the loop thread reads the exit code in notify()
	PostQueuedCompletionStatus(ctx->port, 0, reinterpret_cast<uintptr_t>(ctx->key), nullptr);
}

bool ProcessIocpSource::init(void *h, int p) {
	hProcess = h;
	pid = p;
	event = nullptr;
	wait = nullptr;
	port = nullptr;
	return true;
}

void ProcessIocpSource::cancel(Handle *) {
	if (event) {
		__sprt_CancelEventCompletion(event, false);
		CloseHandle(event);
		event = nullptr;
	}
	if (wait) {
		// blocks until any in-flight callback finishes, so the pool ctx is safe to drop afterwards
		UnregisterWaitEx(wait, INVALID_HANDLE_VALUE);
		wait = nullptr;
	}
	if (hProcess) {
		CloseHandle(hProcess);
		hProcess = nullptr;
	}
}

bool ProcessIocpHandle::init(HandleClass *cl, void *hProcess, int pid,
		CompletionHandle<ProcessHandle> &&c) {
	static_assert(sizeof(ProcessIocpSource) <= DataSize
			&& sprt::is_standard_layout<ProcessIocpSource>::value);
	if (!Handle::init(cl, move(c))) {
		return false;
	}
	auto source = new (_data) ProcessIocpSource;
	return source->init(hProcess, pid);
}

NativeHandle ProcessIocpHandle::getNativeHandle() const {
	return reinterpret_cast<const ProcessIocpSource *>(_data)->hProcess;
}

Status ProcessIocpHandle::rearm(IocpData *iocp, ProcessIocpSource *source) {
	auto status = prepareRearm();
	if (status != Status::Ok) {
		return status;
	}

	if (NtCompletionPacketAvailable()) {
		// primary path: associate the process HANDLE directly with the IOCP
		if (!source->event) {
			source->event = __sprt_ReportEventAsCompletion(iocp->_port, source->hProcess, 1,
					reinterpret_cast<uintptr_t>(this), nullptr);
			if (!source->event) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
		} else {
			if (!__sprt_RestartEventCompletion2(source->event, iocp->_port, source->hProcess, 1,
						reinterpret_cast<uintptr_t>(this), nullptr)) {
				return sprt::status::lastErrorToStatus(GetLastError());
			}
		}
	} else if (!source->wait) {
		// fallback (no wait-completion-packet support, e.g. Wine): a legacy thread-pool wait whose
		// callback posts a completion to the port. WT_EXECUTEONLYONCE: fires once on exit.
		source->port = iocp->_port;
		auto ctx = new (sprt::memory::pool::palloc(_class->info->pool, sizeof(ProcessWaitCtx)))
				ProcessWaitCtx{iocp->_port, reinterpret_cast<void *>(this)};
		if (!RegisterWaitForSingleObject(&source->wait, source->hProcess, &processWaitCallback, ctx,
					INFINITE, WT_EXECUTEONLYONCE)) {
			return sprt::status::lastErrorToStatus(GetLastError());
		}
	}
	return status;
}

Status ProcessIocpHandle::disarm(IocpData *iocp, ProcessIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		if (source->event) {
			__sprt_CancelEventCompletion(source->event, true);
			CloseHandle(source->event);
			source->event = nullptr;
		}
		if (source->wait) {
			UnregisterWaitEx(source->wait, INVALID_HANDLE_VALUE);
			source->wait = nullptr;
		}
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void ProcessIocpHandle::notify(IocpData *iocp, ProcessIocpSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	// the process HANDLE was signalled: the child has exited
	DWORD code = 0;
	GetExitCodeProcess(source->hProcess, &code);
	_exitCode = int(code);

	auto state = static_cast<ProcessState *>(getUserdata());
	if (state && state->readerHandle) {
		// stop the reader (closes hRead, cancelling any pending overlapped read)
		state->readerHandle->cancel();
	}

	// the wait completion is already consumed; skip the disarm and complete
	_status = Status::Suspended;
	cancel(Status::Done, uint32_t(_exitCode));
}

Rc<ProcessHandle> spawnProcessIocp(QueueData *data, HandleClass *processClass,
		HandleClass *readClass, ProcessInfo &&info, Ref *ref) {
	// Anonymous CreatePipe() handles do not support overlapped I/O, so build the pipe from a
	// uniquely-named server end (overlapped, kept by us) + a client end (inheritable, given to the
	// child as stdout/stderr).
	static sprt::atomic<uint32_t> s_pipeCounter(0);
	auto pipeId = ++s_pipeCounter;
	String nameUtf8 = toString("\\\\.\\pipe\\sprtproc.", uint32_t(GetCurrentProcessId()), ".",
			uint32_t(pipeId));
	WideString wname;
	unicode::toUtf16([&](WideStringView w) { wname.append(w.data(), w.size()); }, nameUtf8);
	wname.push_back(char16_t(0));

	HANDLE hRead = CreateNamedPipeW(reinterpret_cast<LPCWSTR>(wname.data()),
			PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
	if (hRead == INVALID_HANDLE_VALUE) {
		return nullptr;
	}

	// client (write) end: inheritable, synchronous; the child writes stdout/stderr here
	SECURITY_ATTRIBUTES sa;
	sprt::memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hWrite = CreateFileW(reinterpret_cast<LPCWSTR>(wname.data()), GENERIC_WRITE, 0, &sa,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hWrite == INVALID_HANDLE_VALUE) {
		CloseHandle(hRead);
		return nullptr;
	}

	// "cmd.exe /c <command>" as a mutable, null-terminated UTF-16 string
	WideString wcmd;
	String full = toString("cmd.exe /c ", info.command);
	unicode::toUtf16([&](WideStringView w) { wcmd.append(w.data(), w.size()); }, full);
	wcmd.push_back(char16_t(0));

	STARTUPINFOW si;
	sprt::memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = nullptr;
	si.hStdOutput = hWrite;
	si.hStdError = hWrite;

	PROCESS_INFORMATION pi;
	sprt::memset(&pi, 0, sizeof(pi));

	BOOL ok = CreateProcessW(nullptr, reinterpret_cast<LPWSTR>(wcmd.data()), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	CloseHandle(hWrite); // parent never writes
	if (!ok) {
		CloseHandle(hRead);
		return nullptr;
	}
	CloseHandle(pi.hThread);

	auto state = Rc<ProcessState>::alloc();
	state->reader = sprt::move(info.reader);
	state->userRef = ref;

	// overlapped reader: its OVERLAPPED + buffer live in the queue pool (too large for the 40-byte
	// Source). Allocate via palloc() + placement-new — `new (pool_t*)` would select standard
	// placement-new (at the pool's address) where the pool operator new isn't visible.
	auto io = new (sprt::memory::pool::palloc(data->_pool, sizeof(ReadIocpState))) ReadIocpState();
	auto reader = Rc<ReadIocpHandle>::create(readClass, hRead, io, state.get(),
			CompletionHandle<PollHandle>());
	if (reader) {
		data->runHandle(reader);
		state->readerHandle = reader;
	}

	// process-exit handle (owns ProcessState as userdata)
	auto proc = Rc<ProcessIocpHandle>::create(processClass, pi.hProcess, int(pi.dwProcessId),
			sprt::move(info.completion));
	if (!proc) {
		if (reader) {
			reader->cancel();
		}
		CloseHandle(pi.hProcess);
		return nullptr;
	}
	proc->setUserdata(state);
	return proc;
}

} // namespace sprt::dispatch
