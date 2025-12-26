/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPEventDirFd.h"

#include "../uring/SPEvent-uring.h"
#include "dirent.h"

namespace STAPPLER_VERSIONIZED stappler::event {

/*bool DirFdSource::init() {
	return FdSource::init(-1);
}

DirFdHandle::~DirFdHandle() {
	getData<DirFdSource>()->cancel();
}

bool DirFdHandle::init(QueueRef *q, QueueData *d, OpenDirInfo &&info) {
	static_assert(sizeof(DirFdSource) <= DataSize && std::is_standard_layout<DirFdSource>::value);

	auto source = new (_data) DirFdSource;

	if (!source->init() || !DirHandle::init(q, d, move(info))) {
		return false;
	}

	return true;
}

Status DirFdHandle::scan(const Callback<void(FileType, StringView)> &cb) {
	switch (_status) {
	case Status::Done: {
		struct dirent **namelist;
		int n = ::scandirat(getData<DirFdSource>()->getFd(), ".", &namelist, NULL, alphasort);
		if (n == -1) {
			return status::errnoToStatus(errno);
		}

		for (int i = 0; i < n; ++ i) {
			FileType t = FileType::Unknown;
			StringView name(namelist[i]->d_name);

			if (name == StringView(".") || name == StringView("..")) {
				::free(namelist[i]);
				continue;
			}

			switch (namelist[i]->d_type) {
			case DT_DIR: t = FileType::Dir; break;
			case DT_REG: t = FileType::File; break;
			case DT_LNK: t = FileType::Link; break;
			case DT_FIFO: t = FileType::Pipe; break;
			case DT_CHR: t = FileType::CharDevice; break;
			case DT_BLK: t = FileType::BlockDevice; break;
			case DT_SOCK: t = FileType::Socket; break;
			default: break;
			}

			cb(t, name);
			::free(namelist[i]);
		}
		::free(namelist);
		return Status::Ok;
		break;
	}
	case Status::Ok:
	case Status::Suspended:
	case Status::Declined:
		return Status::ErrorInProgress;
		break;
	default:
		break;
	}
	return Status::ErrorInvalidArguemnt;
}

bool DirFdURingHandle::init(URingData *uring, OpenDirInfo &&info) {
	if (!DirFdHandle::init(uring->_queue, uring->_data, sp::move(info))) {
		return false;
	}

	_runFn = [] (Handle *h) -> Status {
		return ((DirFdURingHandle *)h)->run(h->getData<DirFdSource>());
	};

	auto source = getData<DirFdSource>();
	source->setURingCallback(uring, [] (Handle *h, int32_t res, uint32_t flags, URingUserFlags uflags) {
		reinterpret_cast<DirFdURingHandle *>(h)->notify(h->getData<DirFdSource>(), res, flags, uflags);
	});

	return true;
}

Status DirFdURingHandle::run(DirFdSource *source) {
	if (_status == Status::Declined) {
		auto uring = source->getURingData();
		auto result = uring->pushSqe({IORING_OP_OPENAT}, [&] (io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = _root ? _root->getData<DirFdSource>()->getFd() : -1;
			sqe->addr = reinterpret_cast<uintptr_t>(_pathname.data());
			sqe->open_flags = O_PATH | O_NOFOLLOW;
			sqe->len = 0;
			uring->retainHandleForSqe(sqe, this);
		}, URingPushFlags::Submit);
		if (isSuccessful(result)) {
			_status = Status::Ok;
		}
	}
	return Status::ErrorAlreadyPerformed;
}

void DirFdURingHandle::notify(DirFdSource *source, int32_t res, uint32_t flags, URingUserFlags uflags) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended; // to allow Handle to be canceled

	if (res >= 0) {
		source->setFd(res);
		setValue(uint32_t(res));
		cancel(Status::Done);
	} else {
		cancel(URingData::getErrnoStatus(res));
	}

	_root = nullptr;
}*/

}
