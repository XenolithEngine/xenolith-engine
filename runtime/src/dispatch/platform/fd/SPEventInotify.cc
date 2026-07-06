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

#include "SPEventInotify.h"

#ifdef __SPRT_SYSCALL_inotify_init1

#include "../uring/SPEvent-uring.h"
#include "../epoll/SPEvent-epoll.h"
#include "../android/SPEvent-alooper.h"

#include <sprt/runtime/filesystem/filepath.h>

#include <unistd.h>

__SPRT_C_FUNC long int syscall(long int __sysno, ...);

namespace sprt {

// Thin passthroughs over the raw syscalls (numbers from
// sprt/c/cross/{linux,android}_sprt/*/syscall.h). inotify has no argument or
// struct translation in glibc either, so no libc/wrapper layer is involved.
static int __SPRT_ID(inotify_init1)(int flags) {
	return (int)syscall(__SPRT_SYSCALL_inotify_init1, flags);
}

static int __SPRT_ID(inotify_add_watch)(int fd, const char *path, unsigned int mask) {
	return (int)syscall(__SPRT_SYSCALL_inotify_add_watch, fd, path, mask);
}

static int __SPRT_ID(inotify_rm_watch)(int fd, int wd) {
	return (int)syscall(__SPRT_SYSCALL_inotify_rm_watch, fd, wd);
}

} // namespace sprt

namespace sprt::dispatch {

// Kernel ABI, glibc-independent. `name` (a null-terminated, null-padded field of
// `len` bytes) follows each record; we reach it via a pointer past the header
// rather than a flexible array member.
struct InotifyEvent {
	int wd;
	uint32_t mask;
	uint32_t cookie;
	uint32_t len;
};

static constexpr uint32_t IN_MODIFY = 0x0000'0002;
static constexpr uint32_t IN_ATTRIB = 0x0000'0004;
static constexpr uint32_t IN_CLOSE_WRITE = 0x0000'0008;
static constexpr uint32_t IN_MOVED_FROM = 0x0000'0040;
static constexpr uint32_t IN_MOVED_TO = 0x0000'0080;
static constexpr uint32_t IN_CREATE = 0x0000'0100;
static constexpr uint32_t IN_DELETE = 0x0000'0200;
static constexpr uint32_t IN_DELETE_SELF = 0x0000'0400;
static constexpr uint32_t IN_MOVE_SELF = 0x0000'0800;
static constexpr uint32_t IN_IGNORED = 0x0000'8000;
static constexpr uint32_t IN_Q_OVERFLOW = 0x0000'4000;
static constexpr uint32_t IN_ONLYDIR = 0x0100'0000;
static constexpr uint32_t IN_MASK_ADD = 0x2000'0000;

// inotify_init1 flags == O_NONBLOCK / O_CLOEXEC values
static constexpr int IN_NONBLOCK = 0x0000'0800;
static constexpr int IN_CLOEXEC = 0x0008'0000;

static uint32_t toInotifyMask(WatchFlags f) {
	uint32_t m = 0;
	if (hasFlag(f, WatchFlags::Created)) {
		m |= IN_CREATE;
	}
	if (hasFlag(f, WatchFlags::Modified)) {
		m |= IN_MODIFY | IN_CLOSE_WRITE;
	}
	if (hasFlag(f, WatchFlags::Deleted)) {
		m |= IN_DELETE;
	}
	if (hasFlag(f, WatchFlags::MovedTo)) {
		m |= IN_MOVED_TO;
	}
	if (hasFlag(f, WatchFlags::MovedFrom)) {
		m |= IN_MOVED_FROM;
	}
	if (hasFlag(f, WatchFlags::Attrib)) {
		m |= IN_ATTRIB;
	}
	// always observe the directory's own lifecycle, and require it to be a dir
	m |= IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;
	return m;
}

bool InotifySource::init() {
	fd = __SPRT_ID(inotify_init1)(IN_NONBLOCK | IN_CLOEXEC);
	return fd >= 0;
}

void InotifySource::cancel() {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

//
// InotifyReaderHandle — the single shared reader
//

bool InotifyReaderHandle::init(HandleClass *cl) {
	static_assert(
			sizeof(InotifySource) <= DataSize && sprt::is_standard_layout<InotifySource>::value);

	if (!Handle::init(cl, CompletionHandle<void>())) {
		return false;
	}

	auto source = reinterpret_cast<InotifySource *>(_data);
	return source->init();
}

bool InotifyReaderHandle::isValid() const { return nativeFd() >= 0; }

int InotifyReaderHandle::nativeFd() const {
	return reinterpret_cast<const InotifySource *>(_data)->fd;
}

int InotifyReaderHandle::addWatch(InotifyWatchHandle *h, const char *dir, uint32_t mask) {
	// IN_MASK_ADD: several watched files in one directory collapse to a single
	// watch descriptor, so union the masks instead of replacing.
	auto wd = __SPRT_ID(inotify_add_watch)(nativeFd(), dir, mask | IN_MASK_ADD);
	if (wd < 0) {
		return -1;
	}

	auto tok = _watches[wd];
	if (auto vec = tok.ptr()) {
		vec->emplace_back(h);
	} else {
		Vector<InotifyWatchHandle *> v;
		v.emplace_back(h);
		tok = sprt::move(v);
	}
	return wd;
}

void InotifyReaderHandle::removeWatch(InotifyWatchHandle *h, int wd) {
	auto vec = _watches[wd].ptr();
	if (!vec) {
		return;
	}

	for (auto vit = vec->begin(); vit != vec->end(); ++vit) {
		if (*vit == h) {
			vec->erase(vit);
			break;
		}
	}

	if (vec->empty()) {
		__SPRT_ID(inotify_rm_watch)(nativeFd(), wd);
		_watches.erase(wd);
	}
}

void InotifyReaderHandle::dispatch(int wd, uint32_t mask, StringView name, bool ignored) {
	auto vec = _watches[wd].ptr();
	if (!vec) {
		return;
	}

	// feed() may cancel a handle, which mutates _watches; iterate a snapshot.
	auto listeners = *vec;
	for (auto h : listeners) { h->feed(mask, name, ignored); }

	if (ignored) {
		// the kernel has dropped this watch descriptor; forget it
		_watches.erase(wd);
	}
}

bool InotifyReaderHandle::read() {
	auto source = reinterpret_cast<InotifySource *>(_data);

	auto s = ::read(source->fd, _buf, sizeof(_buf));
	if (s <= 0) {
		return false;
	}

	size_t off = 0;
	auto len = size_t(s);
	while (off + sizeof(InotifyEvent) <= len) {
		auto rec = reinterpret_cast<const InotifyEvent *>(_buf + off);
		auto name = reinterpret_cast<const char *>(_buf + off + sizeof(InotifyEvent));
		auto nameView = (rec->len > 0) ? StringView(name) : StringView();
		off += sizeof(InotifyEvent) + rec->len;

		dispatch(rec->wd, rec->mask, nameView, (rec->mask & IN_IGNORED) != 0);
	}
	return true;
}

Status InotifyReaderURingHandle::rearm(URingData *uring, InotifySource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = uring->pushRead(source->fd, _buf, sizeof(_buf),
				reinterpret_cast<uintptr_t>(this) | (_timeline & URING_USERDATA_SERIAL_MASK));
		if (status == Status::Suspended) {
			status = uring->submit();
		}
	}
	return status;
}

Status InotifyReaderURingHandle::disarm(URingData *uring, InotifySource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	}
	return status;
}

void InotifyReaderURingHandle::notify(URingData *uring, InotifySource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	if (data.result > 0) {
		size_t off = 0;
		auto len = size_t(data.result);
		while (off + sizeof(InotifyEvent) <= len) {
			auto rec = reinterpret_cast<const InotifyEvent *>(_buf + off);
			auto name = reinterpret_cast<const char *>(_buf + off + sizeof(InotifyEvent));
			auto nameView = (rec->len > 0) ? StringView(name) : StringView();
			off += sizeof(InotifyEvent) + rec->len;

			dispatch(rec->wd, rec->mask, nameView, (rec->mask & IN_IGNORED) != 0);
		}

		if (_status == Status::Suspended) {
			rearm(uring, source);
		}
	} else {
		cancel(URingData::getErrnoStatus(data.result));
	}
}

Status InotifyReaderEPollHandle::rearm(EPollData *epoll, InotifySource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = __SPRT_EPOLLIN;

		status = epoll->add(source->fd, source->event);
	}
	return status;
}

Status InotifyReaderEPollHandle::disarm(EPollData *epoll, InotifySource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void InotifyReaderEPollHandle::notify(EPollData *epoll, InotifySource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.queueFlags & __SPRT_EPOLLIN) {
		while (read()) { }
	}

	if ((data.queueFlags & __SPRT_EPOLLERR) || (data.queueFlags & __SPRT_EPOLLHUP)) {
		cancel();
	}
}

Status InotifyReaderALooperHandle::rearm(ALooperData *alooper, InotifySource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = alooper->add(source->fd, __SPRT_ALOOPER_EVENT_INPUT, this);
	}
	return status;
}

Status InotifyReaderALooperHandle::disarm(ALooperData *alooper, InotifySource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void InotifyReaderALooperHandle::notify(ALooperData *alooper, InotifySource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	if (data.queueFlags & __SPRT_ALOOPER_EVENT_INPUT) {
		while (read()) { }
	}

	if ((data.queueFlags & __SPRT_ALOOPER_EVENT_ERROR)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_INVALID)) {
		cancel();
	}
}

//
// InotifyWatchHandle — the per-file logical handle
//

bool InotifyWatchHandle::init(HandleClass *cl, StringView path, WatchFlags mask,
		CompletionHandle<WatchHandle> &&c, InotifyReaderHandle *reader) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	_reader = reader;
	_path = path.str<decltype(_path)>();
	_mask = (mask == WatchFlags::None) ? WatchFlags::Any : mask;

	auto dir = filepath::root(_path);
	if (dir.empty()) {
		_dir = ".";
	} else {
		_dir = String(dir.data(), dir.size());
	}

	// StringView into the stable _path buffer (safe: _path is not mutated again)
	_name = filepath::lastComponent(_path);
	if (_name.empty()) {
		return false;
	}

	return true;
}

Status InotifyWatchHandle::doRun() {
	if (!_reader) {
		return Status::ErrorInvalidArguemnt;
	}
	_wd = _reader->addWatch(this, _dir.c_str(), toInotifyMask(_mask));
	return (_wd >= 0) ? Status::Ok : Status::ErrorNotPermitted;
}

Status InotifyWatchHandle::doStop() {
	if (_reader && _wd >= 0) {
		_reader->removeWatch(this, _wd);
		_wd = -1;
	}
	return Status::Ok;
}

void InotifyWatchHandle::feed(uint32_t m, StringView name, bool ignored) {
	if (_status != Status::Ok) {
		return;
	}

	WatchFlags pending = WatchFlags::None;
	bool dead = false;

	if (ignored) {
		// watch descriptor removed by the kernel (directory gone / unmounted)
		dead = true;
	} else if (m & IN_Q_OVERFLOW) {
		pending |= WatchFlags::Overflow;
	} else if (m & IN_DELETE_SELF) {
		pending |= WatchFlags::DeleteSelf;
		dead = true;
	} else if (m & IN_MOVE_SELF) {
		pending |= WatchFlags::MoveSelf;
		dead = true;
	} else if (name.empty() || _name != name) {
		// a named event for some other file in the shared directory
		return;
	} else {
		if (m & IN_CREATE) {
			pending |= WatchFlags::Created;
		}
		if (m & (IN_MODIFY | IN_CLOSE_WRITE)) {
			pending |= WatchFlags::Modified;
		}
		if (m & IN_DELETE) {
			pending |= WatchFlags::Deleted;
		}
		if (m & IN_MOVED_TO) {
			pending |= WatchFlags::MovedTo;
		}
		if (m & IN_MOVED_FROM) {
			pending |= WatchFlags::MovedFrom;
		}
		if (m & IN_ATTRIB) {
			pending |= WatchFlags::Attrib;
		}
	}

	if (pending != WatchFlags::None) {
		_last = pending;
		sendCompletion(toInt(pending), Status::Ok);
	}

	if (dead) {
		// the descriptor is gone; the reader has (or will) drop it from its map,
		// and doStop() on the resulting cancel is a no-op for an unknown wd.
		_wd = -1;
		cancel(Status::Done);
	}
}

void setupInotifyWatchClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;

	// No per-backend Source lives in Handle::_data — the watch keeps its state in
	// members and only talks to the shared reader.
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = static_cast<InotifyWatchHandle *>(handle)->doRun();
		if (status == Status::Ok || status == Status::Done) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		static_cast<InotifyWatchHandle *>(handle)->doStop();
		return HandleClass::cancel(cl, handle, data, st);
	};

	cl->suspendFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_cast<InotifyWatchHandle *>(handle)->doStop();
		return HandleClass::suspend(cl, handle, data);
	};

	cl->resumeFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = HandleClass::resume(cl, handle, data);
		if (status == Status::Ok || status == Status::Done) {
			status = static_cast<InotifyWatchHandle *>(handle)->doRun();
		}
		return status;
	};

	// never notified by the OS queue directly — the reader drives feed()
	cl->notifyFn = nullptr;
}

} // namespace sprt::dispatch

#endif // __SPRT_SYSCALL_inotify_init1
