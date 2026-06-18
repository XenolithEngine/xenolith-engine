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
#include <sprt/wrappers/windows/file_api.h> // GetTempPathW/CreateFileW/WriteFile/DeleteFileW for response files

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
				// EOF after this chunk: capture the process back-ref before cancel() destructs source,
				// then tell the process the output is fully drained so it can complete.
				auto proc = source->io ? source->io->proc : nullptr;
				cancel();
				if (proc) {
					proc->onReaderDrained();
				}
			}
		} else {
			// zero bytes transferred: EOF
			auto proc = source->io ? source->io->proc : nullptr;
			cancel();
			if (proc) {
				proc->onReaderDrained();
			}
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
	_childExited = true;

	// Do NOT tear the reader down yet: the child's final output (notably error text written just
	// before it exited) may still be buffered in the pipe or in an in-flight overlapped read.
	// Cancelling the reader here would race — and usually beat — that last completion, dropping the
	// text. Instead let the reader flush the pipe to EOF (the child closed its write end on exit, so
	// the parent-held end EOFs promptly); the reader then calls onReaderDrained() to complete us.
	// This mirrors the POSIX backends' drain-before-exit (drainProcessPipe). If there is no reader, or
	// it already reached EOF, complete now.
	auto state = static_cast<ProcessState *>(getUserdata());
	if (!state || !state->readerHandle || _readerDrained) {
		finishProcess();
	}
}

void ProcessIocpHandle::onReaderDrained() {
	_readerDrained = true;
	if (_childExited && _status == Status::Ok) {
		finishProcess();
	}
}

void ProcessIocpHandle::finishProcess() {
	// drop the temporary argument response file (if any): the child has exited, so it is done reading
	if (auto state = static_cast<ProcessState *>(getUserdata())) {
		if (state->tempRespFile) {
			DeleteFileW(reinterpret_cast<LPCWSTR>(state->tempRespFile));
			state->tempRespFile = nullptr;
		}
	}
	// the wait completion is already consumed; skip the disarm and complete
	_status = Status::Suspended;
	cancel(Status::Done, uint32_t(_exitCode));
}

// ---- command-line construction ----------------------------------------------
//
// cmd.exe caps its command tail at 8191 chars; CreateProcessW allows 32767. So a shell-free command
// runs DIRECTLY (no cmd.exe wrapper) for the larger ceiling and to skip the shell process; when even
// that is exceeded, its arguments spill to a response file (`program @file`, which linkers/compilers
// read — effectively unlimited). Commands that use the shell keep going through `cmd.exe /c`. A
// shell-free command that still fails to launch (a cmd built-in, a .bat, not an .exe) is retried via
// cmd.exe by the caller. NOTE: a command that BOTH needs the shell AND exceeds 8191 still truncates
// (a .bat has the same per-line limit) — rare for builds, left as-is.

static constexpr size_t kDirectCmdMax = 32000; // margin under CreateProcessW's 32767 limit

// True if the command relies on cmd.exe — an unquoted redirection / pipe / conditional / escape or
// %-expansion. Quote-aware so a quoted path (e.g. "C:/Program Files (x86)/cl.exe") is not flagged.
static bool commandNeedsShell(StringView cmd) {
	bool inQuote = false;
	for (size_t i = 0; i < cmd.size(); ++i) {
		char c = cmd[i];
		if (c == '"') {
			inQuote = !inQuote;
		} else if (!inQuote) {
			switch (c) {
			case '|':
			case '<':
			case '>':
			case '&':
			case '^':
			case '%': return true;
			default: break;
			}
		}
	}
	return false;
}

// Split the leading program token (quote-aware) from the remaining argument string.
static StringView firstToken(StringView cmd, StringView &rest) {
	StringView s = cmd;
	while (!s.empty() && (s[0] == ' ' || s[0] == '\t')) { s = s.sub(1); }
	size_t i = 0;
	bool inQuote = false;
	while (i < s.size()) {
		char c = s[i];
		if (c == '"') {
			inQuote = !inQuote;
		} else if (!inQuote && (c == ' ' || c == '\t')) {
			break;
		}
		++i;
	}
	StringView program = s.sub(0, i);
	StringView r = s.sub(i);
	while (!r.empty() && (r[0] == ' ' || r[0] == '\t')) { r = r.sub(1); }
	rest = r;
	return program;
}

// Write `args` to a uniquely-named temp file; return its UTF-16 path (pool-allocated, null-
// terminated) for use as `program @<path>` and later DeleteFileW. nullptr on failure.
static wchar_t *createResponseFile(QueueData *data, StringView args) {
	wchar_t dir[MAX_PATH + 1];
	DWORD n = GetTempPathW(MAX_PATH + 1, reinterpret_cast<LPWSTR>(dir));
	if (n == 0 || n > MAX_PATH) {
		return nullptr;
	}
	static sprt::atomic<uint32_t> s_respCounter(0);
	auto id = ++s_respCounter;
	String nameUtf8 = toString("xlmake.", uint32_t(GetCurrentProcessId()), ".", uint32_t(id), ".rsp");

	WideString wpath;
	wpath.append(reinterpret_cast<char16_t *>(dir), n); // GetTempPathW result ends with a backslash
	unicode::toUtf16([&](WideStringView w) { wpath.append(w.data(), w.size()); }, nameUtf8);
	wpath.push_back(char16_t(0));

	HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(wpath.data()), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	if (!args.empty()) {
		DWORD written = 0;
		WriteFile(h, args.data(), DWORD(args.size()), &written, nullptr);
	}
	CloseHandle(h);

	auto bytes = wpath.size() * sizeof(char16_t);
	auto path = reinterpret_cast<wchar_t *>(sprt::memory::pool::palloc(data->_pool, bytes));
	sprt::memcpy(path, wpath.data(), bytes);
	return path; // includes the trailing null pushed above
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

	// Build the child command line (mutable, null-terminated UTF-16). See the notes above
	// commandNeedsShell(): shell-free commands run directly (larger ceiling, no cmd.exe); over the
	// direct limit their arguments spill to a response file; shell commands use cmd.exe /c.
	wchar_t *respPath = nullptr; // pool-allocated path of a response file, if one was created
	bool viaShell = commandNeedsShell(info.command);

	WideString wcmd;
	auto appendUtf8 = [&](StringView s) {
		unicode::toUtf16([&](WideStringView w) { wcmd.append(w.data(), w.size()); }, s);
	};
	auto buildShellCmd = [&]() {
		wcmd.clear();
		appendUtf8(toString("cmd.exe /c ", info.command));
		wcmd.push_back(char16_t(0));
	};

	if (viaShell) {
		buildShellCmd();
	} else if (info.command.size() <= kDirectCmdMax) {
		appendUtf8(info.command); // CreateProcessW resolves the program from the first token
		wcmd.push_back(char16_t(0));
	} else {
		// too long for any command line: pass the arguments through a response file (program @file)
		StringView args;
		StringView program = firstToken(info.command, args);
		respPath = createResponseFile(data, args);
		if (respPath) {
			appendUtf8(program);
			wcmd.push_back(char16_t(' '));
			wcmd.push_back(char16_t('@'));
			bool quote = false;
			for (auto *p = respPath; *p; ++p) {
				if (*p == L' ') {
					quote = true;
					break;
				}
			}
			if (quote) {
				wcmd.push_back(char16_t('"'));
			}
			for (auto *p = respPath; *p; ++p) { wcmd.push_back(char16_t(*p)); }
			if (quote) {
				wcmd.push_back(char16_t('"'));
			}
			wcmd.push_back(char16_t(0));
		} else {
			appendUtf8(info.command); // could not spill: best-effort direct (OS may truncate)
			wcmd.push_back(char16_t(0));
		}
	}

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
	if (!ok && !viaShell) {
		// the program could not be launched directly (a cmd built-in, a .bat, not an .exe, or not on
		// PATH): retry once through the shell. cmd gets the full command, so a response file (if any)
		// is irrelevant — drop it.
		if (respPath) {
			DeleteFileW(reinterpret_cast<LPCWSTR>(respPath));
			respPath = nullptr;
		}
		buildShellCmd();
		ok = CreateProcessW(nullptr, reinterpret_cast<LPWSTR>(wcmd.data()), nullptr, nullptr, TRUE,
				CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	}
	CloseHandle(hWrite); // parent never writes
	if (!ok) {
		if (respPath) {
			DeleteFileW(reinterpret_cast<LPCWSTR>(respPath));
		}
		CloseHandle(hRead);
		return nullptr;
	}
	CloseHandle(pi.hThread);

	auto state = Rc<ProcessState>::alloc();
	state->reader = sprt::move(info.reader);
	state->userRef = ref;
	state->tempRespFile = respPath; // deleted in finishProcess() once the child has exited

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
		if (respPath) {
			DeleteFileW(reinterpret_cast<LPCWSTR>(respPath)); // finishProcess() will never run
		}
		CloseHandle(pi.hProcess);
		return nullptr;
	}
	proc->setUserdata(state);
	// Let the reader complete the process once it drains the pipe to EOF (see ProcessIocpHandle::notify).
	io->proc = static_cast<ProcessIocpHandle *>(proc.get());
	return proc;
}

} // namespace sprt::dispatch
