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

#if WIN32

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

struct MmapStorage {
	HANDLE fd;
	HANDLE mapping;
};

#define SP_TERMINATED_DATA(view) (view.terminated()?view.data():view.str<memory::StandartInterface>().data())

#define DWORD_HI(x) (x >> 32)
#define DWORD_LO(x) ((x) & 0xffff'ffff)

uint8_t *_mapFile(uint8_t storage[16], StringView path, MappingType type, ProtFlags prot,
		size_t offset, size_t len) {
	DWORD flProtect;
	if ((prot & ProtFlags::MapWrite) != ProtFlags::None) {
		if ((prot & ProtFlags::MapExecute) != ProtFlags::None) {
			flProtect = PAGE_EXECUTE_READWRITE;
		} else {
			flProtect = PAGE_READWRITE;
		}
	} else if ((prot & ProtFlags::MapExecute) != ProtFlags::None) {
		if ((prot & ProtFlags::MapRead) != ProtFlags::None) {
			flProtect = PAGE_EXECUTE_READ;
		} else {
			flProtect = PAGE_EXECUTE;
		}
	} else {
		flProtect = PAGE_READONLY;
	}

	uint64_t end = len + offset;

	DWORD dwDesiredAccess = 0;
	DWORD dwDesiredAccessMap = 0;
	DWORD dwShareMode = 0;
	if ((prot & ProtFlags::MapRead) != ProtFlags::None) {
		dwDesiredAccessMap |= FILE_MAP_READ;
		dwDesiredAccess |= GENERIC_READ;
		dwShareMode |= FILE_SHARE_READ;
	}
	if ((prot & ProtFlags::MapWrite) != ProtFlags::None) {
		dwDesiredAccessMap |= FILE_MAP_WRITE;
		dwDesiredAccess |= GENERIC_WRITE;
		dwShareMode = 0;
	}
	if ((prot & ProtFlags::MapExecute) != ProtFlags::None) {
		dwDesiredAccessMap |= FILE_MAP_EXECUTE;
	}
	if (type == MappingType::Private) {
		flProtect |= PAGE_WRITECOPY;
		dwDesiredAccessMap |= FILE_MAP_COPY;
	}

	HANDLE fd = CreateFileA(SP_TERMINATED_DATA(path), dwDesiredAccess, dwShareMode, 0,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fd == NULL) {
		return nullptr;
	}

	HANDLE h = CreateFileMappingA(fd, NULL, flProtect, DWORD_HI(end), DWORD_LO(end), NULL);
	if (h == NULL) {
		return nullptr;
	}

	void *ret = MapViewOfFile(h, dwDesiredAccessMap, DWORD_HI(offset), DWORD_LO(offset), len);
	if (ret == NULL) {
		CloseHandle(h);
		CloseHandle(fd);
		return nullptr;
	}

	auto s = (MmapStorage *)&storage[0];
	s->fd = fd;
	s->mapping = h;
	return (uint8_t *)ret;
}

#undef SP_TERMINATED_DATA
#undef DWORD_HI
#undef DWORD_LO

bool _unmapFile(uint8_t *region, uint8_t storage[16]) {
	if (region) {
		UnmapViewOfFile(region);

		auto s = (MmapStorage *)&storage[0];
		CloseHandle(s->mapping);
		CloseHandle(s->fd);

		s->mapping = nullptr;
		s->fd = nullptr;
		return true;
	}
	return false;
}

bool _syncMappedRegion(uint8_t *region, uint8_t storage[16]) {
	if (region) {
		auto s = (MmapStorage *)&storage[0];

		FlushViewOfFile(region, 0); //Async flush of dirty pages
		FlushFileBuffers(s->fd); // flush metadata and wait
		return true;
	}
	return false;
}

} // namespace stappler::filesystem::platform

#endif
