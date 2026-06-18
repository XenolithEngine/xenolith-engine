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

#include "SPEventFileFd.h"
#include "../uring/SPEvent-uring.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/c/__sprt_errno.h>

namespace sprt::dispatch {

bool FileURingHandle::init(HandleClass *cl) { return Handle::init(cl, CompletionHandle<void>()); }

void FileURingHandle::submitChunk(URingData *uring, FileState *state) {
	auto &op = state->ops.front()->op;
	uint64_t ud = reinterpret_cast<uintptr_t>(this) | URING_USERDATA_RETAIN_BIT
			| (_timeline & URING_USERDATA_SERIAL_MASK);
	if (op.kind == FileOp::Read) {
		uring->pushRead(state->fd, state->chunkBuf, FileChunkSize, ud);
	} else {
		size_t rem = op.writeData.size() - op.offset;
		size_t want = rem < FileChunkSize ? rem : FileChunkSize;
		uring->pushWrite(state->fd, op.writeData.data() + op.offset, want, ud);
	}
}

void FileURingHandle::pump(URingData *uring, FileState *state) {
	if (state->terminating) {
		return;
	}
	if (state->ops.empty()) {
		state->finalizeChannel();
		return;
	}
	state->opInFlight = true;
	submitChunk(uring, state);
}

Status FileURingHandle::rearm(URingData *uring, FileSource *) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		auto state = getFileState();
		if (state && !state->ops.empty()) {
			state->opInFlight = true;
			submitChunk(uring, state);
		}
	}
	return status;
}

Status FileURingHandle::disarm(URingData *, FileSource *) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		// bump the serial so any late CQE for the in-flight chunk is filtered out;
		// the RETAIN_BIT keeps this handle (and FileState::chunkBuf) alive until
		// that CQE is consumed, so there is no use-after-free.
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void FileURingHandle::notify(URingData *uring, FileSource *, const NotifyData &data) {
	if (_status != Status::Ok) {
		return; // stale CQE after cancel / disarm
	}

	auto state = getFileState();
	if (!state || state->ops.empty()) {
		return;
	}
	state->opInFlight = false;

	Rc<FileOpNode> node = state->ops.front();
	FileOp &op = node->op;

	intptr_t result = data.result;
	if (result < 0) {
		if (result == -EINTR || result == -EAGAIN) {
			state->opInFlight = true;
			submitChunk(uring, state); // retry the same chunk
			return;
		}
		state->completeFrontOp(URingData::getErrnoStatus(int(result)));
		pump(uring, state);
		return;
	}

	bool opDone = false;
	Status st = (op.kind == FileOp::Read) ? state->stepRead(op, result, opDone)
										  : state->stepWrite(op, result, opDone);
	if (opDone) {
		state->completeFrontOp(st);
		pump(uring, state);
	} else {
		state->opInFlight = true;
		submitChunk(uring, state); // next chunk of the same op
	}
}

Rc<FileHandle> makeFileUringHandle(QueueData *qdata, HandleClass *cl, Rc<FileState> &&state) {
	auto h = Rc<FileURingHandle>::create(cl);
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	state->qdata = qdata;
	return h;
}

} // namespace sprt::dispatch
