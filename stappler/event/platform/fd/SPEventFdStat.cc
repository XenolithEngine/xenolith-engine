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

#include "SPEventFd.h"
#include "../uring/SPEvent-uring.h"

namespace STAPPLER_VERSIONIZED stappler::event {

/*static void fillStatData(Stat &target, const struct statx &source) {
	target.size = size_t(source.stx_size);

	if (S_ISBLK(source.stx_mode)) { target.type = FileType::BlockDevice; }
	else if (S_ISCHR(source.stx_mode)) { target.type = FileType::CharDevice; }
	else if (S_ISDIR(source.stx_mode)) { target.type = FileType::Dir; }
	else if (S_ISFIFO(source.stx_mode)) { target.type = FileType::Pipe; }
	else if (S_ISREG(source.stx_mode)) { target.type = FileType::File; }
	else if (S_ISLNK(source.stx_mode)) { target.type = FileType::Link; }
	else if (S_ISSOCK(source.stx_mode)) { target.type = FileType::Socket; }
	else { target.type = FileType::Unknown; }

	if (source.stx_mode & S_IRUSR) { target.prot |= ProtFlags::UserRead; }
	if (source.stx_mode & S_IWUSR) { target.prot |= ProtFlags::UserWrite; }
	if (source.stx_mode & S_IXUSR) { target.prot |= ProtFlags::UserExecute; }
	if (source.stx_mode & S_ISUID) { target.prot |= ProtFlags::UserSetId; }
	if (source.stx_mode & S_IRGRP) { target.prot |= ProtFlags::GroupRead; }
	if (source.stx_mode & S_IWGRP) { target.prot |= ProtFlags::GroupWrite; }
	if (source.stx_mode & S_IXGRP) { target.prot |= ProtFlags::GroupExecute; }
	if (source.stx_mode & S_ISGID) { target.prot |= ProtFlags::GroupSetId; }
	if (source.stx_mode & S_IROTH) { target.prot |= ProtFlags::AllRead; }
	if (source.stx_mode & S_IWOTH) { target.prot |= ProtFlags::AllWrite; }
	if (source.stx_mode & S_IXOTH) { target.prot |= ProtFlags::AllExecute; }

	target.user = source.stx_uid;
	target.group = source.stx_gid;

	target.atime = Time::microseconds(source.stx_atime.tv_sec * 1000000 + source.stx_atime.tv_nsec / 1000);
	target.ctime = Time::microseconds(source.stx_ctime.tv_sec * 1000000 + source.stx_ctime.tv_nsec / 1000);
	target.mtime = Time::microseconds(source.stx_mtime.tv_sec * 1000000 + source.stx_mtime.tv_nsec / 1000);
}

bool StatURingHandle::init(URingData *uring, StatOpInfo &&info) {
	static_assert(sizeof(FdSource) <= DataSize && std::is_standard_layout<FdSource>::value);

	auto source = new (_data) FdSource;

	if (!source->init(-1) || !StatHandle::init(uring->_queue, uring->_data, move(info))) {
		return false;
	}

	_runFn = [] (Handle *h) -> Status {
		return ((StatURingHandle *)h)->run(h->getData<FdSource>());
	};

	source->setURingCallback(uring, [] (Handle *h, int32_t res, uint32_t flags, URingUserFlags uflags) {
		reinterpret_cast<StatURingHandle *>(h)->notify(h->getData<FdSource>(), res, flags, uflags);
	});

	return true;
}

Status StatURingHandle::run(FdSource *source) {
	if (_status == Status::Declined) {
		auto uring = source->getURingData();
		auto result = uring->pushSqe({IORING_OP_STATX}, [&] (io_uring_sqe *sqe, uint32_t n) {
			sqe->fd = _root ? _root->getData<FdSource>()->getFd() : -1;
			sqe->addr = reinterpret_cast<uintptr_t>(_pathname.data());
			sqe->statx_flags = 0;
			sqe->len = STATX_ALL;
			sqe->off = reinterpret_cast<uintptr_t>(&_buffer);
			uring->retainHandleForSqe(sqe, this);
		}, URingPushFlags::Submit);
		if (isSuccessful(result)) {
			_status = Status::Ok;
		}
	}
	return Status::ErrorAlreadyPerformed;
}

void StatURingHandle::notify(FdSource *source, int32_t res, uint32_t flags, URingUserFlags uflags) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended; // to allow Handle to be canceled

	if (res >= 0) {
		fillStatData(_stat, _buffer);
		setValue(uint32_t(res));
		cancel(Status::Done);
	} else {
		cancel(URingData::getErrnoStatus(res));
	}

	_root = nullptr;
}*/

}
