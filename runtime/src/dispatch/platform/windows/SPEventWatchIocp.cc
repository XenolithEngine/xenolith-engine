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

#include "SPEventWatchIocp.h"
#include "SPEvent-windows.h"
#include "SPEvent-iocp.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/runtime/filesystem/filepath.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>

namespace sprt::dispatch {

// One completion batch of FILE_NOTIFY_INFORMATION records. Must be DWORD-aligned.
static constexpr size_t WatchIocpBufferSize = 4'096;

struct WatchIocpIO {
	OVERLAPPED ov;
	alignas(4) uint8_t buf[WatchIocpBufferSize];
};

// The name-lifecycle filter is always requested — create/delete/rename of the
// watched name is what keeps the watch anchored; content/metadata filters are
// added only when the mask asks for them. (FILE_NOTIFY_CHANGE_CREATION tracks
// creation-*time* changes, not file creation, and is deliberately not used.)
static uint32_t toNotifyFilter(WatchFlags f) {
	uint32_t m = FILE_NOTIFY_CHANGE_FILE_NAME;
	if (hasFlag(f, WatchFlags::Modified)) {
		m |= FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
	}
	if (hasFlag(f, WatchFlags::Attrib)) {
		m |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
	}
	return m;
}

void WatchIocpSource::cancel(Handle *) {
	if (hDir && hDir != INVALID_HANDLE_VALUE) {
		CloseHandle(hDir);
	}
	hDir = nullptr;
	io = nullptr; // pool-owned
}

bool WatchIocpHandle::init(HandleClass *cl, void *hDir, WatchIocpIO *io, StringView path,
		WatchFlags mask, CompletionHandle<WatchHandle> &&c) {
	static_assert(sizeof(WatchIocpSource) <= DataSize
			&& sprt::is_standard_layout<WatchIocpSource>::value);

	if (!Handle::init(cl, move(c))) {
		return false;
	}

	_path = path.str<decltype(_path)>();
	_mask = (mask == WatchFlags::None) ? WatchFlags::Any : mask;
	_filter = toNotifyFilter(_mask);

	auto name = filepath::lastComponent(_path);
	if (name.empty()) {
		return false;
	}

	auto len = unicode::getUtf16Length(name);
	_name16.resize(len);
	unicode::toUtf16(_name16.data(), len, name, nullptr);

	auto source = new (_data) WatchIocpSource;
	source->hDir = hDir;
	source->io = io;
	source->associated = false;
	return true;
}

Status WatchIocpHandle::submit(WatchIocpSource *source) {
	sprt::memset(&source->io->ov, 0, sizeof(OVERLAPPED));
	if (!ReadDirectoryChangesW(source->hDir, source->io->buf, DWORD(WatchIocpBufferSize), FALSE,
				_filter, nullptr, &source->io->ov, nullptr)) {
		return sprt::status::lastErrorToStatus(GetLastError());
	}

	// a completion WILL be posted: hold a reference until it is dequeued (notify),
	// so the handle and the record buffer survive a completion after teardown
	_opRefId = sprt::retain(this);
	_opPending = true;
	return Status::Ok;
}

Status WatchIocpHandle::rearm(IocpData *iocp, WatchIocpSource *source) {
	auto status = prepareRearm();
	if (status != Status::Ok) {
		return status;
	}
	if (!source->associated) {
		if (!CreateIoCompletionPort(source->hDir, iocp->_port, reinterpret_cast<uintptr_t>(this),
					0)) {
			return sprt::status::lastErrorToStatus(GetLastError());
		}
		source->associated = true;
	}
	if (!_opPending) {
		return submit(source);
	}
	return Status::Ok;
}

Status WatchIocpHandle::disarm(IocpData *, WatchIocpSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		if (_opPending && source->hDir) {
			// the cancelled completion is drained (and the op ref released) by
			// notify(), which ignores it via the _status check
			CancelIoEx(source->hDir, &source->io->ov);
		}
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void WatchIocpHandle::notify(IocpData *iocp, WatchIocpSource *source, const NotifyData &data) {
	bool hadOp = _opPending;
	auto opId = _opRefId;
	_opPending = false;

	if (_status == Status::Ok) {
		WatchFlags pending = WatchFlags::None;
		bool dead = false;

		if (data.result > 0) {
			// walk the FILE_NOTIFY_INFORMATION chain, matching the watched basename
			const uint8_t *buf = source->io->buf;
			size_t len = size_t(data.result);
			size_t off = 0;
			static constexpr size_t HeaderSize = sizeof(DWORD) * 3; // fields before FileName
			while (off + HeaderSize <= len) {
				auto rec = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(buf + off);
				auto nameBytes = size_t(rec->FileNameLength);
				if (off + HeaderSize + nameBytes > len) {
					break;
				}
				WideStringView name(reinterpret_cast<const char16_t *>(rec->FileName),
						nameBytes / sizeof(char16_t));
				if (name == WideStringView(_name16.data(), _name16.size())) {
					switch (rec->Action) {
					case FILE_ACTION_ADDED: pending |= WatchFlags::Created; break;
					case FILE_ACTION_REMOVED: pending |= WatchFlags::Deleted; break;
					case FILE_ACTION_MODIFIED: pending |= WatchFlags::Modified; break;
					case FILE_ACTION_RENAMED_OLD_NAME: pending |= WatchFlags::MovedFrom; break;
					case FILE_ACTION_RENAMED_NEW_NAME: pending |= WatchFlags::MovedTo; break;
					default: break;
					}
				}
				if (rec->NextEntryOffset == 0) {
					break;
				}
				off += rec->NextEntryOffset;
			}
			if (submit(source) != Status::Ok) {
				// the directory became unwatchable under us
				pending |= WatchFlags::DeleteSelf;
				dead = true;
			}
		} else {
			// zero-length completion: either the record buffer overflowed (records
			// were dropped) or the op failed (directory deleted / handle revoked);
			// a resubmit attempt tells the two apart
			if (submit(source) == Status::Ok) {
				pending |= WatchFlags::Overflow;
			} else {
				pending |= WatchFlags::DeleteSelf;
				dead = true;
			}
		}

		// the user mask narrows only the maskable kinds; lifecycle events always pass
		pending = (pending & _mask) | (pending & ~WatchFlags::Any);

		if (pending != WatchFlags::None) {
			_last = pending;
			sendCompletion(toInt(pending), Status::Ok);
		}

		if (dead && _status == Status::Ok) {
			cancel(Status::Done);
		}
	}

	if (hadOp) {
		sprt::release(this, opId); // may free the handle; do nothing after this
	}
}

Rc<WatchHandle> makeWatchIocpHandle(QueueData *qdata, HandleClass *cl, WatchInfo &&info, Ref *ref) {
	if (filepath::lastComponent(info.path).empty()) {
		return nullptr;
	}

	auto dir = filepath::root(info.path);
	String dirStr = dir.empty() ? String(".") : String(dir.data(), dir.size());

	// CreateFileW accepts forward slashes, so the runtime's '/'-separated paths
	// need only the UTF-16 conversion
	auto dirLen = unicode::getUtf16Length(StringView(dirStr));
	__malloc_basic_string<char16_t> wdir;
	wdir.resize(dirLen);
	unicode::toUtf16(wdir.data(), dirLen, StringView(dirStr), nullptr);

	auto hDir = CreateFileW(reinterpret_cast<LPCWSTR>(wdir.c_str()), FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	if (!hDir || hDir == INVALID_HANDLE_VALUE) {
		return nullptr;
	}

	auto io = new (sprt::memory::pool::palloc(qdata->_pool, sizeof(WatchIocpIO))) WatchIocpIO();
	auto h = Rc<WatchIocpHandle>::create(cl, hDir, io, info.path, info.mask,
			sprt::move(info.completion));
	if (!h) {
		CloseHandle(hDir);
		return nullptr;
	}
	if (ref) {
		h->setUserdata(ref);
	}
	return h;
}

} // namespace sprt::dispatch
