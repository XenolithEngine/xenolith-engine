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

#include "SPEventFile.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/runtime/status.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_stat.h>

namespace sprt::dispatch {

FileState::~FileState() {
	if (ownsFd && fd >= 0) {
		::__sprt_close(fd);
		fd = -1;
	}
}

Status FileState::stepRead(FileOp &op, intptr_t result, bool &opDone) {
	opDone = false;
	if (result == 0) {
		opDone = true; // EOF
		return Status::Ok;
	}
	op.offset += size_t(result);
	filePos += size_t(result);
	if (op.reader) {
		op.reader(BytesView(chunkBuf, size_t(result)));
	}
	return Status::Ok;
}

Status FileState::stepWrite(FileOp &op, intptr_t result, bool &opDone) {
	opDone = false;
	op.offset += size_t(result);
	filePos += size_t(result);
	if (op.offset >= op.writeData.size()) {
		opDone = true;
		return Status::Ok;
	}
	if (result == 0) {
		// no progress with bytes still pending: avoid an infinite loop
		opDone = true;
		return Status::Incomplete;
	}
	return Status::Ok;
}

void FileState::completeFrontOp(Status status) {
	if (ops.empty()) {
		return;
	}
	// hold the node alive across the callback (which may append a new op)
	Rc<FileOpNode> node = sprt::move(ops.front());
	ops.erase(ops.begin());
	opInFlight = false;

	auto &op = node->op;
	if (op.completion.fn) {
		op.completion.fn(op.completion.userdata, handle, uint32_t(op.offset), status);
	}
}

void FileState::failAllOps(Status status) {
	while (!ops.empty()) {
		Rc<FileOpNode> node = sprt::move(ops.front());
		ops.erase(ops.begin());
		auto &op = node->op;
		if (op.completion.fn) {
			op.completion.fn(op.completion.userdata, handle, uint32_t(op.offset), status);
		}
	}
}

void FileState::finalizeChannel() {
	if (finalized) {
		return;
	}
	finalized = true;
	if (!handle) {
		return;
	}
	// Defer the actual cancel out of the current notify (a driver-timer / CQE
	// callback may be on the stack): perform() runs it in this cycle's
	// runAllTasks, after the callback unwinds. Capture the handle (which owns this
	// FileState) so both stay alive until then.
	Rc<FileHandle> h(handle);
	if (qdata->perform([h]() { h->cancel(Status::Done); }, h.get()) != Status::Ok) {
		// not inside a notify cycle: cancel directly
		h->cancel(Status::Done);
	}
}

void FileState::driveInline() {
	if (terminating) {
		return;
	}
	if (ops.empty()) {
		finalizeChannel();
		return;
	}

	// keep the front op alive across the read callback (it may append)
	Rc<FileOpNode> node = ops.front();
	FileOp &op = node->op;

	intptr_t n;
	if (op.kind == FileOp::Read) {
		n = ::__sprt_read(fd, chunkBuf, FileChunkSize);
	} else {
		size_t rem = op.writeData.size() - op.offset;
		if (rem == 0) {
			completeFrontOp(Status::Ok);
			return;
		}
		size_t want = rem < FileChunkSize ? rem : FileChunkSize;
		n = ::__sprt_write(fd, op.writeData.data() + op.offset, want);
	}

	if (n < 0) {
		auto e = __sprt_errno;
		if (e == EINTR || e == EAGAIN) {
			return; // retry on the next timer fire
		}
		completeFrontOp(status::errnoToStatus(e));
		return;
	}

	bool opDone = false;
	Status st =
			(op.kind == FileOp::Read) ? stepRead(op, n, opDone) : stepWrite(op, n, opDone);
	if (opDone) {
		completeFrontOp(st);
		// the completion may have appended a follow-up op; if not, stop driving
		if (ops.empty()) {
			finalizeChannel();
		}
	}
	// otherwise the next timer fire continues this op
}

NativeHandle FileHandleImpl::getNativeHandle() const {
	auto s = getFileState();
	return s ? NativeHandle(s->fd) : NativeHandle(-1);
}

bool FileInlineHandle::init(HandleClass *cl) { return Handle::init(cl, CompletionHandle<void>()); }

void FileInlineHandle::start() {
	auto state = getFileState();
	if (!state || !state->qdata) {
		return;
	}

	TimerInfo tinfo;
	tinfo.timeout = TimeInterval::microseconds(1);
	tinfo.interval = TimeInterval::microseconds(1);
	tinfo.count = TimerInfo::Infinite;
	tinfo.completion = TimerInfo::Completion::create<FileState>(state,
			[](FileState *st, TimerHandle *, uint32_t, Status status) {
		if (status == Status::Ok) {
			st->driveInline();
		}
	});

	auto timer = state->qdata->scheduleTimer(sprt::move(tinfo));
	if (!timer) {
		// cannot drive chunks: fail every queued op
		state->terminating = true;
		state->failAllOps(Status::ErrorNotImplemented);
		state->finalizeChannel();
		return;
	}
	state->driver = timer;
	state->qdata->runHandle(timer.get());
}

// --- public FileHandle API ---------------------------------------------------

bool FileHandle::isBusy() const {
	auto s = static_cast<FileState *>(getUserdata());
	return s && (!s->ops.empty() || s->opInFlight);
}

namespace {

struct FileOpCbData : public Ref {
	Function<void(Status)> onDone;
};

static CompletionHandle<FileHandle> makeOpCompletion(FileOpCbData *cb) {
	return CompletionHandle<FileHandle>::create<FileOpCbData>(cb,
			[](FileOpCbData *d, FileHandle *, uint32_t, Status st) {
		if (d->onDone) {
			d->onDone(st);
		}
	});
}

} // namespace

Status FileHandle::appendRead(Function<void(BytesView)> &&reader, Function<void(Status)> &&onDone) {
	auto state = static_cast<FileState *>(getUserdata());
	if (!state || state->terminating || state->finalized || getStatus() != Status::Ok) {
		if (onDone) {
			onDone(Status::ErrorCancelled);
		}
		return Status::ErrorCancelled;
	}

	auto node = Rc<FileOpNode>::alloc();
	node->op.kind = FileOp::Read;
	node->op.reader = sprt::move(reader);
	if (onDone) {
		auto cb = Rc<FileOpCbData>::alloc();
		cb->onDone = sprt::move(onDone);
		node->op.ref = cb;
		node->op.completion = makeOpCompletion(cb.get());
	}
	state->ops.emplace_back(sprt::move(node));
	return Status::Ok;
}

Status FileHandle::appendWrite(BytesView data, Function<void(Status)> &&onDone) {
	auto state = static_cast<FileState *>(getUserdata());
	if (!state || state->terminating || state->finalized || getStatus() != Status::Ok) {
		if (onDone) {
			onDone(Status::ErrorCancelled);
		}
		return Status::ErrorCancelled;
	}

	auto node = Rc<FileOpNode>::alloc();
	node->op.kind = FileOp::Write;
	node->op.writeData = data;
	if (onDone) {
		auto cb = Rc<FileOpCbData>::alloc();
		cb->onDone = sprt::move(onDone);
		node->op.ref = cb;
		node->op.completion = makeOpCompletion(cb.get());
	}
	state->ops.emplace_back(sprt::move(node));
	return Status::Ok;
}

// --- HandleClass setup + factories -------------------------------------------

void setupInlineFileHandleClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_cast<FileInlineHandle *>(handle)->start();
		return HandleClass::run(cl, handle, data);
	};

	// suspend/resume are plain bookkeeping: the actual chunk driving is done by
	// the FileState driver timer (a separate, reactor-suspendable handle). Marking
	// the handle resumable is what makes HandleClass::run keep it alive in the
	// queue's _suspendableHandles set for the duration of the operation. Cancel /
	// completion sets `terminating` in cancelFn, NOT here, so a graceful wakeup
	// pause does not abort an in-flight operation.
	cl->suspendFn = HandleClass::suspend;
	cl->resumeFn = HandleClass::resume;

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto state = static_cast<FileState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			if (state->driver) {
				state->driver->cancel();
				state->driver = nullptr;
			}
			state->failAllOps(Status::ErrorCancelled);
		}
		return HandleClass::cancel(cl, handle, data, st);
	};
}

int openFileForOp(StringView path, OpenFlags flags, Status *st) {
	int f = 0;
	if (hasFlag(flags, OpenFlags::Read) && hasFlag(flags, OpenFlags::Write)) {
		f |= __SPRT_O_RDWR;
	} else if (hasFlag(flags, OpenFlags::Write)) {
		f |= __SPRT_O_WRONLY;
	} else {
		f |= __SPRT_O_RDONLY;
	}

	if (hasFlag(flags, OpenFlags::Create)) {
		f |= __SPRT_O_CREAT;
	}
	if (hasFlag(flags, OpenFlags::Append)) {
		f |= __SPRT_O_APPEND;
	}
	if (hasFlag(flags, OpenFlags::Truncate)) {
		f |= __SPRT_O_TRUNC;
	}
	if (hasFlag(flags, OpenFlags::CreateExclusive)) {
		f |= __SPRT_O_CREAT | __SPRT_O_EXCL;
	}

#if SPRT_WINDOWS
	// Open the underlying HANDLE for overlapped I/O so the dispatch layer can do
	// true async (IOCP) reads/writes on this fd (see SPEventFileIocp).
	f |= __SPRT_O_OVERLAPPED;
#endif

	String tpath(path.data(), path.size());
	auto fd = ::__sprt_open(tpath.data(), f,
			__SPRT_S_IRUSR | __SPRT_S_IWUSR | __SPRT_S_IRGRP | __SPRT_S_IROTH);
	if (fd < 0) {
		if (st) {
			*st = status::errnoToStatus(__sprt_errno);
		}
		return -1;
	}
	if (st) {
		*st = Status::Ok;
	}
	return fd;
}

FileOp makeReadOp(FileReadInfo &&info, Ref *ref) {
	FileOp op;
	op.kind = FileOp::Read;
	op.reader = sprt::move(info.reader);
	op.completion = info.completion;
	op.ref = ref;
	return op;
}

FileOp makeWriteOp(FileWriteInfo &&info, Ref *ref) {
	FileOp op;
	op.kind = FileOp::Write;
	op.writeData = info.data;
	op.completion = info.completion;
	op.ref = ref;
	return op;
}

Rc<FileState> prepareFileState(QueueData *qdata, StringView path, NativeHandle fd, OpenFlags flags,
		FileOp &&op) {
	int rfd = -1;
	bool owns = false;
	if (!path.empty()) {
		Status st = Status::Ok;
		rfd = openFileForOp(path, flags, &st);
		if (rfd < 0) {
			if (op.completion.fn) {
				op.completion.fn(op.completion.userdata, nullptr, 0, st);
			}
			return nullptr;
		}
		owns = true;
	} else {
		rfd = fd.fd;
		if (rfd < 0) {
			if (op.completion.fn) {
				op.completion.fn(op.completion.userdata, nullptr, 0, Status::ErrorInvalidArguemnt);
			}
			return nullptr;
		}
	}

	auto state = Rc<FileState>::alloc();
	state->qdata = qdata;
	state->fd = rfd;
	state->ownsFd = owns;
	state->appendMode = hasFlag(flags, OpenFlags::Append);

	auto node = Rc<FileOpNode>::alloc();
	node->op = sprt::move(op);
	state->ops.emplace_back(sprt::move(node));
	return state;
}

Rc<FileHandle> makeFileInlineHandle(QueueData *qdata, HandleClass *cl, Rc<FileState> &&state) {
	auto h = Rc<FileInlineHandle>::create(cl);
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	state->qdata = qdata;
	return h;
}

Rc<FileHandle> QueueData::readFile(FileReadInfo &&info, Ref *ref) {
	if (!_makeFileHandle) {
		if (info.completion.fn) {
			info.completion.fn(info.completion.userdata, nullptr, 0, Status::ErrorNotImplemented);
		}
		return nullptr;
	}
	auto path = info.path;
	auto fd = info.fd;
	auto flags = info.flags;
	auto op = makeReadOp(sprt::move(info), ref);
	auto state = prepareFileState(this, path, fd, flags, sprt::move(op));
	if (!state) {
		return nullptr;
	}
	return _makeFileHandle(this, _platformQueue, sprt::move(state));
}

Rc<FileHandle> QueueData::writeFile(FileWriteInfo &&info, Ref *ref) {
	if (!_makeFileHandle) {
		if (info.completion.fn) {
			info.completion.fn(info.completion.userdata, nullptr, 0, Status::ErrorNotImplemented);
		}
		return nullptr;
	}
	auto path = info.path;
	auto fd = info.fd;
	auto flags = info.flags;
	auto op = makeWriteOp(sprt::move(info), ref);
	auto state = prepareFileState(this, path, fd, flags, sprt::move(op));
	if (!state) {
		return nullptr;
	}
	return _makeFileHandle(this, _platformQueue, sprt::move(state));
}

} // namespace sprt::dispatch
