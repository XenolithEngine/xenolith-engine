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

#ifndef STAPPLER_FILESYSTEM_SPFILESYSTEMMAP_H_
#define STAPPLER_FILESYSTEM_SPFILESYSTEMMAP_H_

#include "SPFilesystemLookup.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem {

using sprt::filesystem::MappingType;

class SP_PUBLIC MemoryMappedRegion final {
public:
	using PlatformStorage = std::array<uint8_t, 16>;

	static MemoryMappedRegion mapFile(const FileInfo &, MappingType, ProtFlags, size_t offset = 0,
			size_t len = maxOf<size_t>());

	~MemoryMappedRegion();

	MemoryMappedRegion(MemoryMappedRegion &&);
	MemoryMappedRegion &operator=(MemoryMappedRegion &&);

	MemoryMappedRegion(const MemoryMappedRegion &) = delete;
	MemoryMappedRegion &operator=(const MemoryMappedRegion &) = delete;

	MappingType getType() const { return _type; }
	ProtFlags getProtectionFlags() const { return _prot; }

	uint8_t *getRegion() const { return _region; }
	size_t getSize() const { return _size; }

	BytesView getView() const { return BytesView(_region, _size); }

	operator bool() const { return _region != nullptr; }

	void sync();

protected:
	MemoryMappedRegion();
	MemoryMappedRegion(const LocationInfo &, PlatformStorage &&, uint8_t *, MappingType, ProtFlags,
			size_t);

	PlatformStorage _storage;
	const LocationInfo *_location = nullptr;
	uint8_t *_region = nullptr;
	size_t _size = 0;
	MappingType _type;
	ProtFlags _prot;
};

SP_PUBLIC Access getAccessProtFlags(ProtFlags);

} // namespace stappler::filesystem

#endif // STAPPLER_FILESYSTEM_SPFILESYSTEMMAP_H_
