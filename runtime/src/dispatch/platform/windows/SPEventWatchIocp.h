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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTWATCHIOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTWATCHIOCP_H_

#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchHandleClass.h"

namespace sprt::dispatch {

struct QueueData;
struct IocpData;
struct WatchIocpIO; // OVERLAPPED + record buffer holder; defined in the .cc (avoids <windows.h>)

// Windows IOCP-native file-watch (Looper::watchFile): an overlapped
// ReadDirectoryChangesW on the parent directory, completing via the IOCP, with
// records filtered by the watched basename — name-based like the inotify
// backend, so it survives "save = write temp + rename" and reports a file that
// does not exist yet. The directory HANDLE is owned here; the OVERLAPPED and
// the record buffer live in a pool-allocated WatchIocpIO (the FileIocpIO shape).
struct WatchIocpSource {
	void *hDir = nullptr;
	WatchIocpIO *io = nullptr; // pool-owned, non-owning here
	bool associated = false;

	void cancel(Handle *);
};

class SPRT_API WatchIocpHandle : public WatchHandle {
public:
	virtual ~WatchIocpHandle() = default;

	bool init(HandleClass *, void *hDir, WatchIocpIO *io, StringView path, WatchFlags,
			CompletionHandle<WatchHandle> &&);

	Status rearm(IocpData *, WatchIocpSource *);
	Status disarm(IocpData *, WatchIocpSource *);
	void notify(IocpData *, WatchIocpSource *, const NotifyData &);

protected:
	// post one overlapped ReadDirectoryChangesW; retains the handle until the
	// matching completion is dequeued (the FileIocpHandle pattern)
	Status submit(WatchIocpSource *);

	// UTF-16 basename to match against FILE_NOTIFY_INFORMATION records (they
	// report UTF-16 names relative to the watched directory)
	__malloc_basic_string<char16_t> _name16;
	uint32_t _filter = 0; // FILE_NOTIFY_CHANGE_* set derived from the mask
	uint64_t _opRefId = 0;
	bool _opPending = false;
};

// Factory for QueueData::_watchFile: opens the parent directory HANDLE
// (FILE_LIST_DIRECTORY, backup semantics, overlapped); nullptr if it cannot.
Rc<WatchHandle> makeWatchIocpHandle(QueueData *, HandleClass *, WatchInfo &&, Ref *);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENTWATCHIOCP_H_ */
