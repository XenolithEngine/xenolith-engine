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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTSTATWATCH_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTSTATWATCH_H_

#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchHandleClass.h"

// Portable file-watch (Looper::watchFile) for backends without a native
// filesystem-notification primitive (CFRunLoop, wasm). A repeating reactor
// timer stats the watched path and diffs snapshots — the polling counterpart
// of the inline strategy in SPEventFile.h. Coarser than inotify/kqueue/RDCW:
// changes are observed with up to one poll interval of latency, MovedFrom is
// not distinguishable, and a replace is reported as MovedTo only where the
// libc fills st_ino (not on wasm memfs, where it degrades to Modified).

namespace sprt::dispatch {

struct QueueData;

// How often the watched path is re-stat'ed.
static constexpr TimeInterval StatWatchInterval = TimeInterval::milliseconds(250);

// A per-file polling watch. Purely logical: never registered with the OS
// queue — its run/cancel only start/stop the driver timer, which calls back
// into check().
class SPRT_API StatWatchHandle : public WatchHandle {
public:
	virtual ~StatWatchHandle() = default;

	bool init(HandleClass *, QueueData *, StringView path, WatchFlags,
			CompletionHandle<WatchHandle> &&);

	// run/stop hooks driven by the backend-agnostic HandleClass (see
	// setupStatWatchClass): snapshot the path and start / stop the driver timer.
	Status doRun();
	Status doStop();

	// One poll: stat the path, diff against the previous snapshot, fire the
	// completion for observed changes (called from the driver timer).
	void check();

protected:
	// What the last stat observed; the diff source for change detection.
	struct Snapshot {
		bool exists = false;
		uint64_t size = 0;
		uint64_t ino = 0;
		uint32_t mode = 0;
		int64_t mtimSec = 0;
		int64_t mtimNsec = 0;
	};

	void takeSnapshot(Snapshot &) const;

	QueueData *_qdata = nullptr;
	Rc<Handle> _driver; // the repeating timer that drives check()
	String _dir; // parent directory, checked to detect DeleteSelf
	Snapshot _snap;
};

// Backend-agnostic HandleClass for StatWatchHandle: run/cancel/suspend/resume
// start/stop the driver timer; the handle is never notified by the OS queue.
void setupStatWatchClass(QueueHandleClassInfo *info, HandleClass *cl);

// Factory for QueueData::_watchFile on polling backends.
Rc<WatchHandle> makeStatWatchHandle(QueueData *, HandleClass *, WatchInfo &&, Ref *);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTSTATWATCH_H_ */
