/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

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

#include "SPZipWriter.h"

#include <zlib.h>

namespace STAPPLER_VERSIONIZED stappler {

// Values a 32-bit field cannot hold, which is what makes an entry or an archive need ZIP64.
static constexpr uint64_t ZIP_LIMIT32 = 0xFFFF'FFFEull;
static constexpr uint64_t ZIP_LIMIT16 = 0xFFFEull;

// Version-needed-to-extract: 4.5 says "this uses ZIP64", 2.0 says "deflate may be present".
static constexpr uint16_t ZIP_VERSION_BASE = 20;
static constexpr uint16_t ZIP_VERSION_ZIP64 = 45;

// Little-endian writers. The format is little-endian regardless of the host, so the conversion is
// spelled out rather than left to a memcpy of a native integer.
template <typename Buffer>
static void putU16(Buffer &out, uint16_t v) {
	uint8_t b[2] = {uint8_t(v & 0xFF), uint8_t((v >> 8) & 0xFF)};
	out.put((const char *)b, sizeof(b));
}

template <typename Buffer>
static void putU32(Buffer &out, uint32_t v) {
	uint8_t b[4] = {uint8_t(v & 0xFF), uint8_t((v >> 8) & 0xFF), uint8_t((v >> 16) & 0xFF),
		uint8_t((v >> 24) & 0xFF)};
	out.put((const char *)b, sizeof(b));
}

template <typename Buffer>
static void putU64(Buffer &out, uint64_t v) {
	putU32(out, uint32_t(v & 0xFFFF'FFFFull));
	putU32(out, uint32_t((v >> 32) & 0xFFFF'FFFFull));
}

// Raw deflate: a bare stream with no zlib header or trailer, which is what method 8 stores.
template <typename Bytes>
static bool deflateRaw(BytesView in, Bytes &out) {
	z_stream zs = {};
	if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY)
			!= Z_OK) {
		return false;
	}

	out.resize(size_t(deflateBound(&zs, uLong(in.size()))) + 16);

	zs.next_in = (Bytef *)in.data();
	zs.avail_in = uInt(in.size());
	zs.next_out = out.data();
	zs.avail_out = uInt(out.size());

	auto ret = deflate(&zs, Z_FINISH);
	auto produced = out.size() - zs.avail_out;
	deflateEnd(&zs);

	if (ret != Z_STREAM_END) {
		return false;
	}
	out.resize(produced);
	return true;
}

// General purpose bit 11 announces a UTF-8 name. Set whenever the name is not pure ASCII - the
// absence of this bit in the wild is exactly what forces the READER to guess (see SPZipNames.cc),
// so there is no excuse for writing an archive that makes somebody else guess.
static bool nameNeedsUtf8Flag(BytesView name) {
	for (size_t i = 0; i < name.size(); ++i) {
		if (name.data()[i] >= 0x80) {
			return true;
		}
	}
	return false;
}

template <typename Interface>
const ZipEntry *ZipWriter<Interface>::entry(uint64_t index) const {
	if (index >= _records.size()) {
		return nullptr;
	}
	return &_records[size_t(index)].entry;
}

template <typename Interface>
uint64_t ZipWriter<Interface>::locate(StringView name) const {
	// Linear: an archive being built is walked far less often than it is appended to, and keeping a
	// sorted index correct across appends would cost more than it saves.
	for (size_t i = 0; i < _records.size(); ++i) {
		if (_records[i].entry.name == name) {
			return uint64_t(i);
		}
	}
	return maxOf<uint64_t>();
}

template <typename Interface>
BytesView ZipWriter<Interface>::bytes() const {
	return BytesView(_out.data(), _out.input());
}

template <typename Interface>
bool ZipWriter<Interface>::addDir(StringView name) {
	if (name.empty()) {
		return false;
	}

	// The trailing slash IS the directory marker - without it this would be an empty file.
	if (name.back() == '/') {
		return addFile(name, BytesView(), true);
	}

	String withSlash;
	withSlash.assign(name.data(), name.size());
	withSlash.append("/");
	return addFile(StringView(withSlash.data(), withSlash.size()), BytesView(), true);
}

template <typename Interface>
bool ZipWriter<Interface>::addFile(StringView name, BytesView data, bool uncompressed) {
	if (_finished) {
		return false;
	}

	auto nameBytes = BytesView((const uint8_t *)name.data(), name.size());

	/* Refuse to write a name this module's own reader would refuse to read.
	 *
	 * Without this the module could produce archives it classifies as hostile - absolute paths,
	 * '..' segments, backslashes - which is a strange thing for one piece of software to do to
	 * itself, and a worse thing to hand to somebody else.
	 */
	if (zipCheckName(nameBytes) != ZipNameRejection::None) {
		return false;
	}

	if (name.size() > 0xFFFF) {
		// the name length field is 16 bits; there is no extension for it
		return false;
	}

	Bytes deflated;
	uint16_t method = ZIP_METHOD_STORE;
	BytesView payload = data;

	if (!uncompressed && !data.empty()) {
		// Fall back to Store when compression did not help. An archive that is larger than the sum
		// of its inputs is a silly artifact, and the decision is free here because the compressed
		// size is known before anything is written.
		if (deflateRaw(data, deflated) && deflated.size() < data.size()) {
			method = ZIP_METHOD_DEFLATE;
			payload = BytesView(deflated.data(), deflated.size());
		}
	}

	auto crc = uint32_t(::crc32(::crc32(0, nullptr, 0), data.data(), uInt(data.size())));

	uint64_t localOffset = _out.input();

	// Per-entry ZIP64 is decided HERE, with every number already in hand - which is the whole
	// benefit of buffering rather than streaming.
	bool zip64 = (data.size() > ZIP_LIMIT32) || (payload.size() > ZIP_LIMIT32)
			|| (localOffset > ZIP_LIMIT32);

	uint16_t flags = 0;
	if (nameNeedsUtf8Flag(nameBytes)) {
		flags |= ZIP_FLAG_UTF8;
	}

	uint16_t dosDate = 0;
	uint16_t dosTime = 0;
	zipUtcToDos(Time::now(), dosDate, dosTime);

	// -- local file header --

	putU32(_out, ZIP_SIG_LOCAL);
	putU16(_out, zip64 ? ZIP_VERSION_ZIP64 : ZIP_VERSION_BASE);
	putU16(_out, flags);
	putU16(_out, method);
	putU16(_out, dosTime);
	putU16(_out, dosDate);
	putU32(_out, crc);
	putU32(_out, zip64 ? uint32_t(0xFFFF'FFFFu) : uint32_t(payload.size()));
	putU32(_out, zip64 ? uint32_t(0xFFFF'FFFFu) : uint32_t(data.size()));
	putU16(_out, uint16_t(name.size()));
	putU16(_out, uint16_t(zip64 ? 20 : 0)); // extra length: the zip64 field is 2 + 2 + 8 + 8
	_out.put(name.data(), name.size());

	if (zip64) {
		// Order is fixed by the specification: uncompressed size, then compressed size.
		putU16(_out, ZIP_EXTRA_ZIP64);
		putU16(_out, 16);
		putU64(_out, data.size());
		putU64(_out, payload.size());
	}

	if (!payload.empty()) {
		_out.put((const char *)payload.data(), payload.size());
	}

	// -- record it --

	auto namesBefore = _names.data();

	Record record;
	record.nameOffset = _names.size();
	record.nameLength = uint32_t(name.size());

	_names.insert(_names.end(), nameBytes.data(), nameBytes.data() + nameBytes.size());
	_names.emplace_back(0); // the terminator ZipEntry::name's readers rely on

	record.entry.localOffset = localOffset;
	record.entry.compressedSize = payload.size();
	record.entry.uncompressedSize = data.size();
	record.entry.crc32 = crc;
	record.entry.method = method;
	record.entry.flags = flags;
	record.entry.mtime = zipDosToUtc(dosDate, dosTime);
	record.entry.state =
			(name.back() == '/') ? ZipEntryFlags::Directory : ZipEntryFlags::None;

	_records.emplace_back(sprt::move(record));

	// The arena may have moved; every name view into it has to follow. Cheap because it only
	// happens when the vector actually grew its block.
	if (_names.data() != namesBefore) {
		for (auto &it : _records) {
			it.entry.name = StringView((const char *)_names.data() + it.nameOffset, it.nameLength);
		}
	} else {
		auto &added = _records.back();
		added.entry.name =
				StringView((const char *)_names.data() + added.nameOffset, added.nameLength);
	}

	return true;
}

template <typename Interface>
auto ZipWriter<Interface>::save() -> BufferTemplate<Interface> {
	if (_finished) {
		return Buffer();
	}
	_finished = true;

	uint64_t cdOffset = _out.input();

	for (auto &it : _records) {
		auto &e = it.entry;

		bool zip64 = (e.uncompressedSize > ZIP_LIMIT32) || (e.compressedSize > ZIP_LIMIT32)
				|| (e.localOffset > ZIP_LIMIT32);

		// Only the fields that actually overflow become sentinels, and the extra field carries
		// exactly those, in the specification's order - the reader decodes it positionally.
		uint16_t extraLength = 0;
		if (zip64) {
			extraLength = 4;
			if (e.uncompressedSize > ZIP_LIMIT32) {
				extraLength += 8;
			}
			if (e.compressedSize > ZIP_LIMIT32) {
				extraLength += 8;
			}
			if (e.localOffset > ZIP_LIMIT32) {
				extraLength += 8;
			}
		}

		putU32(_out, ZIP_SIG_CENTRAL);
		putU16(_out, zip64 ? ZIP_VERSION_ZIP64 : ZIP_VERSION_BASE); // version made by
		putU16(_out, zip64 ? ZIP_VERSION_ZIP64 : ZIP_VERSION_BASE); // version needed
		putU16(_out, e.flags);
		putU16(_out, e.method);

		uint16_t dosDate = 0;
		uint16_t dosTime = 0;
		zipUtcToDos(e.mtime, dosDate, dosTime);
		putU16(_out, dosTime);
		putU16(_out, dosDate);

		putU32(_out, e.crc32);
		putU32(_out,
				e.compressedSize > ZIP_LIMIT32 ? uint32_t(0xFFFF'FFFFu)
											   : uint32_t(e.compressedSize));
		putU32(_out,
				e.uncompressedSize > ZIP_LIMIT32 ? uint32_t(0xFFFF'FFFFu)
												 : uint32_t(e.uncompressedSize));
		putU16(_out, uint16_t(e.name.size()));
		putU16(_out, extraLength);
		putU16(_out, 0); // comment length
		putU16(_out, 0); // disk number start
		putU16(_out, 0); // internal attributes
		putU32(_out, 0); // external attributes
		putU32(_out,
				e.localOffset > ZIP_LIMIT32 ? uint32_t(0xFFFF'FFFFu) : uint32_t(e.localOffset));

		_out.put(e.name.data(), e.name.size());

		if (zip64) {
			putU16(_out, ZIP_EXTRA_ZIP64);
			putU16(_out, uint16_t(extraLength - 4));
			if (e.uncompressedSize > ZIP_LIMIT32) {
				putU64(_out, e.uncompressedSize);
			}
			if (e.compressedSize > ZIP_LIMIT32) {
				putU64(_out, e.compressedSize);
			}
			if (e.localOffset > ZIP_LIMIT32) {
				putU64(_out, e.localOffset);
			}
		}
	}

	uint64_t cdSize = _out.input() - cdOffset;
	uint64_t entryCount = _records.size();

	// -- end records --

	bool zip64 = (entryCount > ZIP_LIMIT16) || (cdSize > ZIP_LIMIT32) || (cdOffset > ZIP_LIMIT32);

	if (zip64) {
		uint64_t eocd64Offset = _out.input();

		putU32(_out, ZIP_SIG_EOCD64);
		putU64(_out, 44); // size of the remainder of this record
		putU16(_out, ZIP_VERSION_ZIP64); // version made by
		putU16(_out, ZIP_VERSION_ZIP64); // version needed
		putU32(_out, 0); // this disk
		putU32(_out, 0); // disk with the central directory
		putU64(_out, entryCount); // entries on this disk
		putU64(_out, entryCount); // entries total
		putU64(_out, cdSize);
		putU64(_out, cdOffset);

		putU32(_out, ZIP_SIG_LOCATOR64);
		putU32(_out, 0); // disk with the zip64 EOCD
		putU64(_out, eocd64Offset);
		putU32(_out, 1); // total disks
	}

	putU32(_out, ZIP_SIG_EOCD);
	putU16(_out, 0); // this disk
	putU16(_out, 0); // disk with the central directory
	putU16(_out, entryCount > ZIP_LIMIT16 ? uint16_t(0xFFFFu) : uint16_t(entryCount));
	putU16(_out, entryCount > ZIP_LIMIT16 ? uint16_t(0xFFFFu) : uint16_t(entryCount));
	putU32(_out, cdSize > ZIP_LIMIT32 ? uint32_t(0xFFFF'FFFFu) : uint32_t(cdSize));
	putU32(_out, cdOffset > ZIP_LIMIT32 ? uint32_t(0xFFFF'FFFFu) : uint32_t(cdOffset));
	putU16(_out, 0); // comment length

	return sprt::move(_out);
}

template class ZipWriter<mem_std::Interface>;
template class ZipWriter<memory::PoolInterface>;

} // namespace STAPPLER_VERSIONIZED stappler
