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

#ifndef STAPPLER_ZIP_SPZIPCATALOG_H_
#define STAPPLER_ZIP_SPZIPCATALOG_H_

#include "SPZipFormat.h"
#include "SPZipNames.h"
#include "SPZipSource.h"

namespace STAPPLER_VERSIONIZED stappler {

// What the catalog knows about an entry beyond its numbers. A rejected entry stays VISIBLE - it
// keeps its index and is still listed - because an archive that carries a hostile name is something
// the caller should be able to see, not something that should silently not exist.
enum class ZipEntryFlags : uint32_t {
	None = 0,

	// name ends in '/' and the entry has no content of its own
	Directory = 1 << 0,

	// the name did not survive sanitizeName(); reading this entry is refused
	NameRejected = 1 << 1,

	// general purpose bit 0; this reader has no decryption and refuses rather than hand back
	// ciphertext that looks like content
	Encrypted = 1 << 2,

	// compression method outside {Store, Deflate}
	UnsupportedMethod = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(ZipEntryFlags)

// One entry as the catalog exposes it. `name` is decoded and points into the catalog's own name
// arena, so it is valid exactly as long as the catalog is.
struct ZipEntry {
	StringView name;

	uint64_t localOffset = 0;
	uint64_t compressedSize = 0;
	uint64_t uncompressedSize = 0;

	uint32_t crc32 = 0;
	uint16_t method = 0;
	uint16_t flags = 0;

	Time mtime;

	ZipEntryFlags state = ZipEntryFlags::None;

	bool readable() const {
		return (state
					   & (ZipEntryFlags::Directory | ZipEntryFlags::NameRejected
							   | ZipEntryFlags::Encrypted | ZipEntryFlags::UnsupportedMethod))
				== ZipEntryFlags::None;
	}
};

/* The archive's central directory, read into memory and decoded.
 *
 * Reading is a two-pass affair on purpose. The first pass decodes the records and works out each
 * final name; the second copies the names into a single arena and only THEN takes the StringViews
 * that ZipEntry carries. Building the arena entry by entry would not do: appending can reallocate,
 * and every view handed out earlier would be left pointing at the old block.
 *
 * The arena is a byte vector rather than a string for a related reason - a vector keeps its buffer
 * when the vector itself is moved, whereas a short string lives inside the string object and would
 * travel with it, quietly invalidating every name the moment the catalog was moved.
 */
template <typename Interface>
class SP_PUBLIC ZipCatalog : public Interface::AllocBaseType {
public:
	using Bytes = typename Interface::BytesType;

	template <typename T>
	using Vector = typename Interface::template VectorType<T>;

	// Locates and decodes the central directory. The source is left seeked wherever reading
	// finished; every subsequent read of file content seeks explicitly anyway.
	Status read(ZipSource &);

	bool valid() const { return _valid; }

	size_t size() const { return _entries.size(); }

	// nullptr when the index is out of range
	const ZipEntry *entry(uint64_t index) const;

	// maxOf<uint64_t>() when absent. A ZIP may carry the same name twice; like libzip, the FIRST
	// occurrence wins, so an archive cannot shadow an earlier entry with a later one.
	uint64_t locate(StringView name) const;

	// How far the archive is shifted from the start of the source - non-zero for a self-extracting
	// stub or an embedded archive. Content reads have to add it to every stored offset.
	uint64_t prefix() const { return _prefix; }

protected:
	// every decoded name, concatenated; ZipEntry::name views into it. The central directory itself
	// is NOT retained: once the names are decoded nothing else in it is needed, and content reads
	// go to the local headers in the source.
	Bytes _names;

	Vector<ZipEntry> _entries;

	// (name, index) sorted by name, for binary-search lookup
	Vector<Pair<StringView, uint64_t>> _index;

	uint64_t _prefix = 0;
	bool _valid = false;
};

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPCATALOG_H_ */
