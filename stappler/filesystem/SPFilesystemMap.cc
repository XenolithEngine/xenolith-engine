/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include "SPFilesystemMap.h"
#include "SPFilesystem.h"
#include "SPPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

MemoryMappedRegion MemoryMappedRegion::mapFile(const FileInfo &info, MappingType type,
		ProtFlags prot, size_t offset, size_t len) {
	if (math::align(offset, size_t(sprt::platform::getMemoryPageSize())) != offset) {
		log::source().error("filesystem",
				"offset for MemoryMappedRegion::mapFile should be aligned as "
				"platform::_getMemoryPageSize");
		return MemoryMappedRegion();
	}

	bool exists = false;
	struct __SPRT_STAT_NAME stat;
	mem_std::String path;
	const LocationInfo *loc = nullptr;

	enumeratePaths(info, getAccessProtFlags(prot), [&](const LocationInfo &info, StringView str) {
		path = str.str<mem_std::Interface>();
		if (info.interface->_stat(info, str, &stat) == Status::Ok) {
			exists = true;
			loc = &info;
		}
		return false;
	});

	if (!exists) {
		log::source().error("filesystem", "Fail to get stat for a file: ", path);
		return MemoryMappedRegion();
	}

	len = std::min(len, size_t(stat.st_size));

	if (offset > 0) {
		if (offset > stat.st_size) {
			log::source().error("filesystem", "Offset (", offset, ") for a file ", path,
					" is larger then file itself");
			return MemoryMappedRegion();
		} else {
			auto remains = stat.st_size - offset;
			len = std::min(len, remains);
		}
	}

	Status st = Status::Ok;
	PlatformStorage storage;
	auto region = loc->interface->_mmap(storage.data(), *loc, path, type,
			getModeFromProtFlags(prot), offset, len, &st);
	if (region) {
		return MemoryMappedRegion(*loc, move(storage), region, type, prot, len);
	}

	return MemoryMappedRegion();
}

MemoryMappedRegion::~MemoryMappedRegion() {
	if (_region) {
		_location->interface->_munmap(_region, _storage.data());
		_region = nullptr;
	}
}

MemoryMappedRegion::MemoryMappedRegion(MemoryMappedRegion &&other) {
	_region = other._region;
	_storage = sp::move(other._storage);
	_type = other._type;
	_prot = other._prot;

	other._region = nullptr;
	memset(other._storage.data(), 0, other._storage.size());
}

MemoryMappedRegion &MemoryMappedRegion::operator=(MemoryMappedRegion &&other) {
	_region = other._region;
	_storage = sp::move(other._storage);
	_type = other._type;
	_prot = other._prot;

	other._region = nullptr;
	memset(other._storage.data(), 0, other._storage.size());
	return *this;
}

void MemoryMappedRegion::sync() { _location->interface->_msync(_region, _storage.data()); }

MemoryMappedRegion::MemoryMappedRegion()
: _region(nullptr), _type(MappingType::Private), _prot(ProtFlags::None) { }

MemoryMappedRegion::MemoryMappedRegion(const LocationInfo &loc, PlatformStorage &&storage,
		uint8_t *ptr, MappingType t, ProtFlags p, size_t s)
: _storage(sp::move(storage)), _location(&loc), _region(ptr), _size(s), _type(t), _prot(p) { }

Access getAccessProtFlags(ProtFlags flags) {
	Access ret = Access::None;

	if (hasFlag(flags, ProtFlags::UserRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::UserWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::UserExecute)) {
		ret |= Access::Execute;
	}
	if (hasFlag(flags, ProtFlags::GroupRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::GroupWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::GroupExecute)) {
		ret |= Access::Execute;
	}
	if (hasFlag(flags, ProtFlags::AllRead)) {
		ret |= Access::Read;
	}
	if (hasFlag(flags, ProtFlags::AllWrite)) {
		ret |= Access::Write;
	}
	if (hasFlag(flags, ProtFlags::AllExecute)) {
		ret |= Access::Execute;
	}
	return ret;
}

} // namespace stappler::filesystem
