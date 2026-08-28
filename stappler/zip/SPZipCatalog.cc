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

#include "SPZipCatalog.h"

#include <sprt/cxx/algorithm>

namespace STAPPLER_VERSIONIZED stappler {

// How much of the tail has to be in hand to find the end records: the largest comment the length
// field can express, the record itself, and room for the ZIP64 pair that may precede it.
static constexpr uint64_t ZIP_TAIL_WINDOW =
		ZIP_MAX_COMMENT + ZIP_EOCD_SIZE + ZIP_EOCD64_SIZE + ZIP_LOCATOR64_SIZE;

/* Name handling. The decision about what an entry is CALLED lives entirely in SPZipNames.cc; this
 * is only about where the result is put.
 *
 * The same decoder runs twice - once counting, once filling - because the arena has to be sized
 * before a single view into it is taken, and decoding can change the length (a CP437 byte becomes
 * two or three UTF-8 ones). Running it twice is cheaper than the alternatives and keeps one
 * definition of the name.
 */
static size_t decodedNameLength(const ZipRawEntry &raw) {
	size_t length = 0;
	zipDecodeName(raw, [&](BytesView chunk) { length += chunk.size(); });
	return length;
}

template <typename Bytes>
static void decodeName(const ZipRawEntry &raw, Bytes &arena) {
	zipDecodeName(raw,
			[&](BytesView chunk) { arena.insert(arena.end(), chunk.data(), chunk.data() + chunk.size()); });
}

// Everything about an entry that decides whether it can be read at all.
//
// `name` is the DECODED name as bytes, not as a StringView: a name carrying a NUL would already be
// truncated by the time it became a StringView, and its NUL would then be unfindable.
static ZipEntryFlags classifyEntry(const ZipRawEntry &raw, BytesView name) {
	auto ret = ZipEntryFlags::None;

	if (name.size() > 0 && name.data()[name.size() - 1] == '/') {
		ret |= ZipEntryFlags::Directory;
	}
	if ((raw.flags & ZIP_FLAG_ENCRYPTED) != 0) {
		ret |= ZipEntryFlags::Encrypted;
	}
	if (raw.method != ZIP_METHOD_STORE && raw.method != ZIP_METHOD_DEFLATE) {
		ret |= ZipEntryFlags::UnsupportedMethod;
	}
	if (zipCheckName(name) != ZipNameRejection::None) {
		ret |= ZipEntryFlags::NameRejected;
	}

	return ret;
}

template <typename Interface>
const ZipEntry *ZipCatalog<Interface>::entry(uint64_t index) const {
	if (index >= _entries.size()) {
		return nullptr;
	}
	return &_entries[size_t(index)];
}

template <typename Interface>
uint64_t ZipCatalog<Interface>::locate(StringView name) const {
	auto it = sprt::lower_bound(_index.begin(), _index.end(), name,
			[](const Pair<StringView, uint64_t> &l, StringView r) { return l.first < r; });

	if (it == _index.end() || it->first != name) {
		return maxOf<uint64_t>();
	}
	return it->second;
}

template <typename Interface>
Status ZipCatalog<Interface>::read(ZipSource &source) {
	_names.clear();
	_entries.clear();
	_index.clear();
	_prefix = 0;
	_valid = false;

	if (!source.valid()) {
		return Status::ErrorInvalidArguemnt;
	}

	auto sourceSize = source.size();
	if (sourceSize < ZIP_EOCD_SIZE) {
		return Status::ErrorInvalidArguemnt;
	}

	// -- locate the end records --

	auto tailSize = size_t(sourceSize < ZIP_TAIL_WINDOW ? sourceSize : ZIP_TAIL_WINDOW);
	auto tailOffset = sourceSize - tailSize;

	Bytes tail;
	tail.resize(tailSize);
	if (!source.readAt(tailOffset, tail.data(), tailSize)) {
		return Status::ErrorInvalidArguemnt;
	}

	ZipEocd eocd;
	auto st = zipFindEocd(BytesView(tail.data(), tail.size()), tailOffset, eocd);
	if (!sprt::status::isSuccessful(st)) {
		return st;
	}

	if (eocd.zip64) {
		st = zipResolveZip64(BytesView(tail.data(), tail.size()), tailOffset, eocd);
		if (!sprt::status::isSuccessful(st)) {
			return st;
		}
	}

	// -- sanity limits, before anything is sized from these numbers --
	//
	// Without these a corrupt record turns straight into a multi-gigabyte allocation: the sizes come
	// off the wire and nothing has vouched for them yet.

	if (eocd.cdSize > sourceSize) {
		return Status::ErrorInvalidArguemnt;
	}
	if (eocd.entryCount > eocd.cdSize / ZIP_CENTRAL_SIZE) {
		// every entry needs at least a fixed-size header, so the count cannot exceed what fits
		return Status::ErrorInvalidArguemnt;
	}

	// -- work out the prefix --
	//
	// Offsets inside the archive are relative to where the archive begins, which is not necessarily
	// where the source begins. The shift is a guess derived from the arithmetic, so it is confirmed
	// by looking for a central header where it predicts one; zero is tried first, because a
	// well-formed archive with no prefix must not be re-interpreted on the strength of a coincidence.

	uint64_t candidates[2] = {0, 0};
	size_t candidateCount = 1;
	uint64_t guess = 0;
	if (zipPrefixOffset(eocd, guess) && guess != 0) {
		candidates[1] = guess;
		candidateCount = 2;
	}

	bool located = false;
	uint64_t cdStart = 0;
	for (size_t i = 0; i < candidateCount && !located; ++i) {
		uint64_t start = 0;
		if (!zipCheckedAdd(candidates[i], eocd.cdOffset, start)) {
			continue;
		}
		if (!zipRangeFits(start, eocd.cdSize, sourceSize)) {
			continue;
		}

		if (eocd.entryCount == 0) {
			// nothing to look at; an empty directory is located wherever it is claimed to be
			_prefix = candidates[i];
			cdStart = start;
			located = true;
			break;
		}

		uint8_t sig[4] = {0};
		if (!source.readAt(start, sig, sizeof(sig))) {
			continue;
		}

		ZipView v(sig, sizeof(sig));
		if (v.readUnsigned32() == ZIP_SIG_CENTRAL) {
			_prefix = candidates[i];
			cdStart = start;
			located = true;
		}
	}

	if (!located) {
		return Status::ErrorInvalidArguemnt;
	}

	// -- read and decode the central directory --

	Bytes directory;
	directory.resize(size_t(eocd.cdSize));
	if (eocd.cdSize > 0 && !source.readAt(cdStart, directory.data(), size_t(eocd.cdSize))) {
		return Status::ErrorInvalidArguemnt;
	}

	Vector<ZipRawEntry> raw;
	raw.reserve(size_t(eocd.entryCount));

	ZipView cursor(directory.data(), directory.size());
	for (uint64_t i = 0; i < eocd.entryCount; ++i) {
		ZipRawEntry entry;
		st = zipReadCentralEntry(cursor, entry);
		if (!sprt::status::isSuccessful(st)) {
			return st;
		}
		raw.emplace_back(entry);
	}

	/* -- build the name arena, then the entries --
	 *
	 * Two passes: sizing first, so that not one view is taken before the arena has stopped growing.
	 *
	 * Every name is followed by a NUL that is NOT part of it. StringView::terminated() reads the
	 * byte at data()[size()] and is documented to require it to exist (stringview.h), and it is
	 * called by anything that wants a C string out of a view - which is most of what a name gets
	 * handed to. An exactly-sized arena would make every ZipEntry::name a one-byte overread waiting
	 * for such a caller; found by running the suite under ASan, invisible without it.
	 */

	size_t nameTotal = 0;
	for (auto &it : raw) { nameTotal += decodedNameLength(it) + 1; }

	_names.reserve(nameTotal);
	for (auto &it : raw) {
		decodeName(it, _names);
		_names.emplace_back(0);
	}

	_entries.reserve(raw.size());

	size_t nameOffset = 0;
	for (auto &it : raw) {
		auto length = decodedNameLength(it);

		auto nameBytes = BytesView(_names.data() + nameOffset, length);

		ZipEntry entry;

		// StringView stops at the first NUL, so a name carrying one is exposed truncated. That is
		// safe only because such a name is rejected below and the entry is refused a read; the
		// classification itself is done on the untruncated bytes.
		entry.name = StringView((const char *)_names.data() + nameOffset, length);
		entry.localOffset = it.localOffset;
		entry.compressedSize = it.compressedSize;
		entry.uncompressedSize = it.uncompressedSize;
		entry.crc32 = it.crc32;
		entry.method = it.method;
		entry.flags = it.flags;
		entry.mtime = zipDosToUtc(it.dosDate, it.dosTime);
		entry.state = classifyEntry(it, nameBytes);

		_entries.emplace_back(entry);
		nameOffset += length + 1; // step over the terminator
	}

	// -- the lookup index --
	//
	// Sorted by name, with the original position as the tie-break, so that a duplicated name always
	// resolves to its FIRST occurrence - the same rule libzip follows, and the one that keeps a
	// later entry from shadowing an earlier one.

	_index.reserve(_entries.size());
	for (size_t i = 0; i < _entries.size(); ++i) {
		_index.emplace_back(_entries[i].name, uint64_t(i));
	}

	sprt::sort(_index.begin(), _index.end(),
			[](const Pair<StringView, uint64_t> &l, const Pair<StringView, uint64_t> &r) {
		if (l.first != r.first) {
			return l.first < r.first;
		}
		return l.second < r.second;
	});

	_valid = true;
	return Status::Ok;
}

template class ZipCatalog<mem_std::Interface>;
template class ZipCatalog<memory::PoolInterface>;

} // namespace STAPPLER_VERSIONIZED stappler
