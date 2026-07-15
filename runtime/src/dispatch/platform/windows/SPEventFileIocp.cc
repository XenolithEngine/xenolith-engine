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

#include "SPEventFileIocp.h"
#include "SPEvent-windows.h"
#include "SPEvent-iocp.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/unistd/io.h>

namespace sprt::dispatch {

struct FileIocpIO {
	OVERLAPPED ov;
};

void FileIocpSource::cancel(Handle *) {
	// hFile is borrowed from the libc fd, which closes it via ~FileState; do not
	// close it here. io is pool-owned.
	hFile = nullptr;
	io = nullptr;
}

bool FileIocpHandle::init(HandleClass *cl, void *hFile, FileIocpIO *io) {
	static_assert(sizeof(FileIocpSource) <= DataSize
			&& sprt::is_standard_layout<FileIocpSource>::value);
	if (!Handle::init(cl, CompletionHandle<void>())) {
		return false;
	}
	auto source = new (_data) FileIocpSource;
	source->hFile = hFile;
	source->io = io;
	source->associated = false;
	return true;
}

void FileIocpHandle::submitChunk(FileState *state, FileIocpSource *source) {
	auto &op = state->ops.front()->op;

	// zero-length write completes immediately (no overlapped op posts a completion
	// reliably for a 0-byte transfer)
	if (op.kind == FileOp::Write && op.writeData.size() == op.offset) {
		state->completeFrontOp(Status::Ok);
		pump(state, source);
		return;
	}

	sprt::memset(&source->io->ov, 0, sizeof(OVERLAPPED));
	// overlapped handles have no OS file pointer: drive the offset explicitly from
	// the cumulative file position (which advances across chunks AND chained ops).
	uint64_t off = state->filePos;
	if (op.kind == FileOp::Write && state->appendMode) {
		off = 0xFFFFFFFFFFFFFFFFull; // overlapped-write append sentinel
	}
	source->io->ov.Offset = DWORD(off & 0xFFFFFFFFu);
	source->io->ov.OffsetHigh = DWORD(off >> 32);

	BOOL ok;
	if (op.kind == FileOp::Read) {
		ok = ReadFile(source->hFile, state->chunkBuf, DWORD(FileChunkSize), nullptr,
				&source->io->ov);
	} else {
		size_t rem = op.writeData.size() - op.offset;
		DWORD want = rem < FileChunkSize ? DWORD(rem) : DWORD(FileChunkSize);
		ok = WriteFile(source->hFile, op.writeData.data() + op.offset, want, nullptr,
				&source->io->ov);
	}

	if (!ok) {
		auto err = GetLastError();
		if (op.kind == FileOp::Read && err == ERROR_HANDLE_EOF) {
			// read started at EOF: no completion will be posted — finish here
			state->completeFrontOp(Status::Ok);
			pump(state, source);
			return;
		}
		if (err != ERROR_IO_PENDING) {
			state->completeFrontOp(sprt::status::lastErrorToStatus(err));
			pump(state, source);
			return;
		}
	}

	// success or pending: a completion WILL be posted. Hold a reference until it
	// is dequeued (notify), so the handle survives a completion after teardown.
	_opRefId = sprt::retain(this);
	_opPending = true;
}

void FileIocpHandle::pump(FileState *state, FileIocpSource *source) {
	if (state->terminating) {
		return;
	}
	if (state->ops.empty()) {
		state->finalizeChannel();
		return;
	}
	state->opInFlight = true;
	submitChunk(state, source);
}

Status FileIocpHandle::rearm(IocpData *iocp, FileIocpSource *source) {
	auto status = prepareRearm();
	if (status != Status::Ok) {
		return status;
	}
	if (!source->associated) {
		if (!CreateIoCompletionPort(source->hFile, iocp->_port, reinterpret_cast<uintptr_t>(this),
					0)) {
			return sprt::status::lastErrorToStatus(GetLastError());
		}
		source->associated = true;
	}
	auto state = getFileState();
	if (state && !state->ops.empty()) {
		state->opInFlight = true;
		submitChunk(state, source);
	}
	return status;
}

Status FileIocpHandle::disarm(IocpData *, FileIocpSource *) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		// Bump the serial; the RETAIN_BIT-equivalent _opRefId keeps this handle (and
		// FileState::chunkBuf) alive until any in-flight completion is dequeued and
		// dropped by notify() (which ignores it via the _status check). The pending
		// overlapped read/write therefore cannot use freed memory.
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void FileIocpHandle::notify(IocpData *iocp, FileIocpSource *source, const NotifyData &data) {
	bool hadOp = _opPending;
	auto opId = _opRefId;
	_opPending = false;

	if (_status == Status::Ok) {
		auto state = getFileState();
		if (state && !state->ops.empty()) {
			state->opInFlight = false;
			Rc<FileOpNode> node = state->ops.front();
			FileOp &op = node->op;

			intptr_t result = intptr_t(data.result);
			bool opDone = false;
			Status st = (op.kind == FileOp::Read) ? state->stepRead(op, result, opDone)
												  : state->stepWrite(op, result, opDone);
			if (opDone) {
				state->completeFrontOp(st);
				pump(state, source);
			} else {
				state->opInFlight = true;
				submitChunk(state, source);
			}
		}
	}

	if (hadOp) {
		sprt::release(this, opId); // may free the handle; do nothing after this
	}
}

Rc<FileHandle> makeFileIocpHandle(QueueData *qdata, HandleClass *cl, Rc<FileState> &&state) {
	auto hFile = reinterpret_cast<HANDLE>(_get_osfhandle(state->fd));
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	auto io = new (sprt::memory::pool::palloc(qdata->_pool, sizeof(FileIocpIO))) FileIocpIO();
	auto h = Rc<FileIocpHandle>::create(cl, hFile, io);
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	state->qdata = qdata;
	return h;
}

} // namespace sprt::dispatch
