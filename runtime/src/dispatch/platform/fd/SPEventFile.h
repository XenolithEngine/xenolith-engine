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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTFILE_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTFILE_H_

#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchHandleClass.h"

// Platform-neutral pieces of the async file-I/O channel (Looper::readFile /
// writeFile). A FileHandle owns or borrows one fd and runs a serial queue of
// read/write operations. Two execution strategies share the per-op state machine
// here (FileState): io_uring submits IORING_OP_READ/WRITE (see SPEventFileFd.cc),
// while every other backend uses the portable FileInlineHandle below, which is
// driven by a repeating reactor timer (one bounded read/write per fire, yielding
// to the loop in between). Reader callbacks and completions fire on the looper
// thread; the design mirrors the ProcessState/ProcessHandle split.

namespace sprt::dispatch {

struct QueueData;
struct FileState;

// One read/write step. For io_uring this is one async op; for the inline
// strategy it bounds how much the loop thread does per timer fire.
static constexpr size_t FileChunkSize = 128 * 1'024;

// One queued file operation. Operations on a FileHandle run strictly serially;
// each fires its own completion exactly once when finished.
struct FileOp {
	enum Kind : uint8_t {
		Read,
		Write,
	};

	Kind kind = Read;
	BytesView writeData; // Write: remaining bytes are writeData[offset..]
	size_t offset = 0; // bytes read (Read) / written (Write) so far
	FileReadInfo::ReaderCallback reader; // Read: per-chunk callback (looper thread)
	CompletionHandle<FileHandle> completion; // per-op completion, fired once
	Rc<Ref> ref; // keeps convenience-callback closures alive until the op completes
};

// Ref-counted op node: the op queue holds these (not FileOp by value) so a
// reader/completion callback that appends a new op cannot invalidate the op
// currently executing — callers hold a local Rc<FileOpNode> across callbacks.
struct SPRT_API FileOpNode : public Ref {
	FileOp op;
};

// Heavy, cross-handle state for a file channel. Held alive as the FileHandle's
// userdata; the handle is referenced back only by a raw pointer (the handle owns
// the state, so there is no cycle) — the same shape as ProcessState.
struct SPRT_API FileState : public Ref {
	// malloc-backed (pool-independent): a FileState outlives the transient notify
	// pools its callbacks run in, so its op-queue must not be pool-bound.
	Vector<Rc<FileOpNode>> ops;
	QueueData *qdata = nullptr;
	FileHandle *handle = nullptr; // raw back-ptr (the handle owns this state)
	Rc<Handle> driver; // inline strategy: the repeating timer that drives chunks

	int fd = -1;
	bool ownsFd = false; // close fd in the destructor iff true
	bool appendMode = false; // opened with OpenFlags::Append (IOCP overlapped writes append)
	// Cumulative file position across all ops. The fd-based backends (io_uring,
	// inline) rely on the OS file pointer auto-advancing, so they ignore this; the
	// IOCP backend has no OS file pointer for overlapped handles and uses it as the
	// explicit per-chunk offset.
	uint64_t filePos = 0;
	bool opInFlight = false; // a chunk is submitted (io_uring) / being read (inline)
	bool terminating = false; // teardown started; accept no new work
	bool finalized = false; // handle finalize already requested

	uint8_t chunkBuf[FileChunkSize]; // reusable read buffer (stable address for io_uring)

	virtual ~FileState();

	// Apply one chunk result to the front op `op` (the caller holds it alive via a
	// local Rc<FileOpNode>). `result` is the non-negative byte count. Sets
	// `opDone` and returns the op-final Status when the operation finishes
	// (EOF / all bytes written), else leaves opDone=false. Fires the read reader.
	Status stepRead(FileOp &op, intptr_t result, bool &opDone);
	Status stepWrite(FileOp &op, intptr_t result, bool &opDone);

	// Pop the front op and fire its completion (value = bytes done).
	void completeFrontOp(Status);

	// Fire every remaining queued op's completion with `status` and clear the
	// queue (used on cancellation / teardown).
	void failAllOps(Status);

	// Inline strategy: perform exactly one bounded read/write of the front op
	// (called from the driver timer). Finalizes the handle when the queue drains.
	void driveInline();

	// Request the channel to finalize (drain done): cancels the handle.
	void finalizeChannel();
};

// Internal base adding the shared fd accessor over the public FileHandle API.
class SPRT_API FileHandleImpl : public FileHandle {
public:
	virtual ~FileHandleImpl() = default;

	virtual NativeHandle getNativeHandle() const override;

protected:
	FileState *getFileState() const { return static_cast<FileState *>(getUserdata()); }
};

// Portable, loop-driven file handle for every backend that cannot do native
// async regular-file I/O (epoll, ALooper, kqueue, RunLoop, non-overlapped
// Windows fds). Driven by a repeating reactor timer stored in FileState.
class SPRT_API FileInlineHandle : public FileHandleImpl {
public:
	virtual ~FileInlineHandle() = default;

	bool init(HandleClass *);

	// kick off the driver timer (called from runFn)
	void start();
};

// HandleClass setup for FileInlineHandle. Custom (no reactor rearm/notify):
// runFn starts the driver timer, suspendFn marks terminating + stops the timer,
// cancelFn tears the channel down.
void setupInlineFileHandleClass(QueueHandleClassInfo *info, HandleClass *cl);

// Open `path`, translating OpenFlags to libc open flags (adding O_OVERLAPPED on
// Windows so the fd is IOCP-capable). Returns the fd or -1 with *st set.
int openFileForOp(StringView path, OpenFlags, Status *st);

// Build the FileState for one initial operation. Opens `path` (owned fd) or
// adopts `fd` (borrowed). On open failure fires `op.completion` with the error
// and returns nullptr.
Rc<FileState> prepareFileState(QueueData *, StringView path, NativeHandle fd, OpenFlags,
		FileOp &&op);

// Build FileOp records from the public Info structs.
FileOp makeReadOp(FileReadInfo &&, Ref *);
FileOp makeWriteOp(FileWriteInfo &&, Ref *);

// Inline-strategy factory (used by every non-io_uring backend).
Rc<FileHandle> makeFileInlineHandle(QueueData *, HandleClass *, Rc<FileState> &&);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTFILE_H_ */
