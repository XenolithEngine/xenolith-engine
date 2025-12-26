/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPFilesystem.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

struct MmapStorage {
	uint64_t length;
	uint64_t offset;
};

#define SP_TERMINATED_DATA(view) (view.terminated()?view.data():view.str<memory::StandartInterface>().data())

uint8_t *_mapFile(uint8_t storage[16], StringView path, MappingType type, ProtFlags prot,
		size_t offset, size_t len) {
	int oFlags = 0;
	if (hasFlag(prot, ProtFlags::MapRead)) {
		if (hasFlag(prot, ProtFlags::MapWrite)) {
			oFlags = O_RDWR;
		} else {
			oFlags = O_RDONLY;
		}
	} else if (hasFlag(prot, ProtFlags::MapWrite)) {
		oFlags = O_WRONLY;
	}

	int fd = ::open(SP_TERMINATED_DATA(path), oFlags);

	if (fd < 0) {
		return nullptr;
	}

	int mProt = 0;
	if (hasFlag(prot, ProtFlags::MapRead)) {
		mProt = PROT_READ;
	}
	if (hasFlag(prot, ProtFlags::MapWrite)) {
		mProt = PROT_WRITE;
	}
	if (hasFlag(prot, ProtFlags::MapExecute)) {
		mProt = PROT_EXEC;
	}

	int mFlags = 0;
	switch (type) {
	case MappingType::Private: mFlags = MAP_PRIVATE; break;
	case MappingType::Shared: mFlags = MAP_SHARED; break;
	}

	auto ptr = ::mmap(nullptr, len, mProt, mFlags, fd, (off_t)offset);

	::close(fd); // Pre-close file, God bless POSIX

	if (ptr != MAP_FAILED) {
		auto s = (MmapStorage *)&storage[0];
		s->length = len;
		s->offset = offset;
		return (uint8_t *)ptr;
	}
	return nullptr;
}

#undef SP_TERMINATED_DATA

bool _unmapFile(uint8_t *region, uint8_t storage[16]) {
	if (region) {
		auto s = (MmapStorage *)&storage[0];
		return ::munmap(region, size_t(s->length)) == 0;
	}
	return false;
}

bool _syncMappedRegion(uint8_t *region, uint8_t storage[16]) {
	if (region) {
		auto s = (MmapStorage *)&storage[0];
		return ::msync(region, size_t(s->length), MS_SYNC) == 0;
	}
	return false;
}

} // namespace stappler::filesystem::platform
