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

#include "SPEventStatWatch.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/c/sys/__sprt_stat.h>

namespace sprt::dispatch {

bool StatWatchHandle::init(HandleClass *cl, QueueData *qdata, StringView path, WatchFlags mask,
		CompletionHandle<WatchHandle> &&c) {
	if (!Handle::init(cl, move(c))) {
		return false;
	}

	_qdata = qdata;
	_path = path.str<decltype(_path)>();
	_mask = (mask == WatchFlags::None) ? WatchFlags::Any : mask;

	auto dir = filepath::root(_path);
	if (dir.empty()) {
		_dir = ".";
	} else {
		_dir = String(dir.data(), dir.size());
	}

	if (filepath::lastComponent(_path).empty()) {
		return false;
	}

	return true;
}

void StatWatchHandle::takeSnapshot(Snapshot &snap) const {
	struct __SPRT_STAT_NAME st;
	if (::__sprt_stat(_path.c_str(), &st) == 0) {
		snap.exists = true;
		snap.size = uint64_t(st.st_size);
		snap.ino = uint64_t(st.st_ino);
		snap.mode = uint32_t(st.st_mode);
		snap.mtimSec = int64_t(st.st_mtim.tv_sec);
		snap.mtimNsec = int64_t(st.st_mtim.tv_nsec);
	} else {
		snap = Snapshot();
	}
}

Status StatWatchHandle::doRun() {
	takeSnapshot(_snap);

	TimerInfo tinfo;
	tinfo.timeout = StatWatchInterval;
	tinfo.interval = StatWatchInterval;
	tinfo.count = TimerInfo::Infinite;
	tinfo.completion = TimerInfo::Completion::create<StatWatchHandle>(this,
			[](StatWatchHandle *h, TimerHandle *, uint32_t, Status status) {
		if (status == Status::Ok) {
			h->check();
		}
	});

	auto timer = _qdata->scheduleTimer(sprt::move(tinfo));
	if (!timer) {
		return Status::ErrorNotImplemented;
	}
	_driver = timer;
	_qdata->runHandle(timer.get());
	return Status::Ok;
}

Status StatWatchHandle::doStop() {
	if (_driver) {
		// Defer the timer's cancel out of a possibly-on-stack driver notify (doStop
		// is reached from within check() when the completion callback cancels this
		// watch): perform() runs it in this cycle's runAllTasks, after the callback
		// unwinds (the FileState::finalizeChannel pattern).
		Rc<Handle> d = sprt::move(_driver);
		_driver = nullptr;
		if (_qdata->perform([d]() { d->cancel(); }, d.get()) != Status::Ok) {
			// not inside a notify cycle: cancel directly
			d->cancel();
		}
	}
	return Status::Ok;
}

void StatWatchHandle::check() {
	if (_status != Status::Ok) {
		return;
	}

	Snapshot now;
	takeSnapshot(now);

	WatchFlags pending = WatchFlags::None;
	bool dead = false;

	if (now.exists && !_snap.exists) {
		pending |= WatchFlags::Created;
	} else if (!now.exists && _snap.exists) {
		pending |= WatchFlags::Deleted;
		struct __SPRT_STAT_NAME st;
		if (::__sprt_stat(_dir.c_str(), &st) != 0) {
			// the containing directory is gone with the file; the watch is dead
			pending |= WatchFlags::DeleteSelf;
			dead = true;
		}
	} else if (now.exists) {
		if (now.ino != 0 && _snap.ino != 0 && now.ino != _snap.ino) {
			// a different inode under the watched name: atomic replace
			pending |= WatchFlags::MovedTo;
		} else {
			if (now.size != _snap.size || now.mtimSec != _snap.mtimSec
					|| now.mtimNsec != _snap.mtimNsec) {
				pending |= WatchFlags::Modified;
			}
			if (now.mode != _snap.mode) {
				pending |= WatchFlags::Attrib;
			}
		}
	}

	_snap = now;

	// the user mask narrows only the maskable kinds; lifecycle events always pass
	pending = (pending & _mask) | (pending & ~WatchFlags::Any);

	if (pending != WatchFlags::None) {
		_last = pending;
		sendCompletion(toInt(pending), Status::Ok);
	}

	if (dead && _status == Status::Ok) {
		// doStop defers the driver's cancel, so this is safe within its notify
		cancel(Status::Done);
	}
}

void setupStatWatchClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;

	// No per-backend Source lives in Handle::_data — the watch keeps its state in
	// members and only drives its own reactor timer.
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = static_cast<StatWatchHandle *>(handle)->doRun();
		if (status == Status::Ok || status == Status::Done) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		static_cast<StatWatchHandle *>(handle)->doStop();
		return HandleClass::cancel(cl, handle, data, st);
	};

	cl->suspendFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_cast<StatWatchHandle *>(handle)->doStop();
		return HandleClass::suspend(cl, handle, data);
	};

	cl->resumeFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = HandleClass::resume(cl, handle, data);
		if (status == Status::Ok || status == Status::Done) {
			status = static_cast<StatWatchHandle *>(handle)->doRun();
		}
		return status;
	};

	// never notified by the OS queue directly — the driver timer calls check()
	cl->notifyFn = nullptr;
}

Rc<WatchHandle> makeStatWatchHandle(QueueData *qdata, HandleClass *cl, WatchInfo &&info, Ref *ref) {
	auto h = Rc<StatWatchHandle>::create(cl, qdata, info.path, info.mask,
			sprt::move(info.completion));
	if (h && ref) {
		h->setUserdata(ref);
	}
	return h;
}

} // namespace sprt::dispatch
