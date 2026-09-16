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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTINOTIFY_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTINOTIFY_H_

#include "SPEventFd.h"
#include "../../detail/SPRuntimeDispatchHandleClass.h"

#include <sprt/c/cross/__sprt_syscall.h>

#ifdef __SPRT_SYSCALL_inotify_init1

namespace sprt::dispatch {

class InotifyWatchHandle;

// The single inotify fd. inotify *instances* are a scarce per-user kernel
// resource (fs.inotify.max_user_instances, ~128), so exactly one is created per
// Queue; individual files are cheap *watch descriptors* on it. The `event` slot
// mirrors SignalFdSource and is reused for the epoll registration.
struct SPRT_API InotifySource {
	int fd;
	epoll_event event;

	bool init();
	void cancel();
};

// The shared reader: one per Queue, registered with the OS queue for readiness on
// the inotify fd. It drains the event stream and routes each record to the
// per-file InotifyWatchHandle listeners keyed by watch descriptor. Created lazily
// on the first watchFile() and kept for the Queue's lifetime.
class SPRT_API InotifyReaderHandle : public Handle {
public:
	virtual ~InotifyReaderHandle() = default;

	bool init(HandleClass *);

	bool isValid() const;

	// Register/unregister a per-file listener. addWatch returns the watch
	// descriptor (>= 0) or -1 on error; several files in the same directory share
	// one descriptor, so listeners are kept per-wd and the descriptor is removed
	// only when its last listener goes away.
	int addWatch(InotifyWatchHandle *, const char *dir, uint32_t mask);
	void removeWatch(InotifyWatchHandle *, int wd);

protected:
	bool read();
	void dispatch(int wd, uint32_t mask, StringView name, bool ignored);

	int nativeFd() const;

	HashMap<int, Vector<InotifyWatchHandle *>> _watches; // wd -> listeners
	alignas(8) uint8_t _buf[4096];
};

class SPRT_API InotifyReaderURingHandle : public InotifyReaderHandle {
public:
	virtual ~InotifyReaderURingHandle() = default;

	Status rearm(URingData *, InotifySource *);
	Status disarm(URingData *, InotifySource *);

	void notify(URingData *, InotifySource *, const NotifyData &);
};

class SPRT_API InotifyReaderEPollHandle : public InotifyReaderHandle {
public:
	virtual ~InotifyReaderEPollHandle() = default;

	Status rearm(EPollData *, InotifySource *);
	Status disarm(EPollData *, InotifySource *);

	void notify(EPollData *, InotifySource *, const NotifyData &);
};

class SPRT_API InotifyReaderALooperHandle : public InotifyReaderHandle {
public:
	virtual ~InotifyReaderALooperHandle() = default;

	Status rearm(ALooperData *, InotifySource *);
	Status disarm(ALooperData *, InotifySource *);

	void notify(ALooperData *, InotifySource *, const NotifyData &);
};

// A per-file watch. This is a purely logical handle: it is never registered with
// the OS queue itself — its run/cancel only add/remove a watch descriptor on the
// shared reader, and the reader calls feed() when a matching event arrives.
class SPRT_API InotifyWatchHandle : public WatchHandle {
public:
	virtual ~InotifyWatchHandle() = default;

	bool init(HandleClass *, StringView path, WatchFlags, CompletionHandle<WatchHandle> &&,
			InotifyReaderHandle *reader);

	// run/stop hooks driven by the backend-agnostic HandleClass (see
	// setupInotifyWatchClass): add/remove the watch descriptor on the reader.
	Status doRun();
	Status doStop();

	// Called by the reader for each event on this handle's watch descriptor.
	// `ignored` marks the descriptor being auto-removed by the kernel (directory
	// gone / unmounted).
	void feed(uint32_t mask, StringView name, bool ignored);

	StringView getDir() const { return _dir; }

protected:
	Rc<InotifyReaderHandle> _reader;
	String _dir; // parent directory, null-terminated for the syscall
	StringView _name; // target basename to filter (view into WatchHandle::_path)
	int _wd = -1;
};

// Backend-agnostic HandleClass for InotifyWatchHandle: run/cancel/suspend/resume
// add/remove the watch on the shared reader; the handle is never notified by the
// OS queue directly.
void setupInotifyWatchClass(QueueHandleClassInfo *info, HandleClass *cl);

} // namespace sprt::dispatch

#endif // __SPRT_SYSCALL_inotify_init1

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTINOTIFY_H_ */
