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

#ifndef STAPPLER_FILESYSTEM_SPFILESYSTEMEMBEDDED_H_
#define STAPPLER_FILESYSTEM_SPFILESYSTEMEMBEDDED_H_

#include "SPFilesystemLookup.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem::embedded {

/*
	BundleFS: a read-only filesystem served from data linked into the binary.

	The build system turns a directory into a single translation unit (see make/embed
	and the xlmake $(EMBED) directive), and that TU registers a Bundle here. Bundles are
	then reachable through the ordinary filesystem API under FileCategory::Embedded:

		filesystem::readIntoMemory<mem_std::Interface>(
				FileInfo{"resources/style.css", FileCategory::Embedded});

	The first path component is the bundle name, so a bundle generated from the directory
	`resources` keeps the very same paths an on-disk `resources` directory would have.

	This header is the single source of truth for the format: both the reader below and
	the generator in stappler_makefile include it, so they can not drift apart.
*/

// Format revision of Bundle/Entry. A generated TU states the revision it was written for;
// the reader rejects a bundle it does not understand instead of misreading it.
static constexpr uint32_t BundleVersion = 1;

// Name of the extensible SharedModule generated TUs register themselves in. The symbol name
// within it is the bundle name, and the symbol itself is a `const Bundle *`.
static constexpr const char *BundleModuleName = "stappler_filesystem_embedded";

enum class EntryFlags : uint32_t {
	None = 0,

	// The entry is a directory. Directories are stored explicitly, so that stat() on a
	// directory and empty directories both work.
	Dir = 1 << 0,

	// `size` bytes at `offset` are compressed and expand to `originalSize` bytes. Decompression
	// goes through the stappler_data seam, so a compressed bundle needs that module linked in.
	Compressed = 1 << 1,
};

SP_DEFINE_ENUM_AS_MASK(EntryFlags)

// A single file or directory within a bundle. Written by the generator as an aggregate, so the
// field order is part of the format.
struct Entry {
	const char *path; // bundle-relative, '/'-separated, no leading slash; not NUL-guaranteed
	uint32_t pathSize;
	EntryFlags flags;
	uint64_t offset; // offset into Bundle::data; 0 for a directory
	uint64_t size; // stored size (the compressed size when Compressed is set)
	uint64_t originalSize; // size after decompression; equals `size` when not compressed

	StringView getPath() const { return StringView(path, pathSize); }

	bool isDir() const { return hasFlag(flags, EntryFlags::Dir); }
	bool isCompressed() const { return hasFlag(flags, EntryFlags::Compressed); }
};

// One embedded directory. `entries` is sorted bytewise by path, which is what makes lookup a
// binary search and directory enumeration a scan of a contiguous range.
struct Bundle {
	uint32_t version;
	const char *name; // mount name == the first path component, e.g. "resources"
	const Entry *entries;
	uint32_t entriesCount;
	const uint8_t *data;
	uint64_t dataSize;
	int64_t buildTime; // microseconds since epoch, reported by stat() as mtime/ctime/atime

	StringView getName() const { return StringView(name); }
	SpanView<Entry> getEntries() const { return SpanView<Entry>(entries, entriesCount); }
};

/*
	The canonical ordering of Bundle::entries: bytewise, except that '/' compares below every
	other character.

	This is what makes a directory's subtree a contiguous range in the table, so that a lookup is
	a binary search and enumeration is a range scan. A plain bytewise sort does not: '.' (0x2E)
	is below '/' (0x2F), so a sibling "a.txt" would fall between "a" and "a/b" and split the
	subtree of "a" in two.

	Both the reader and the generator order entries with this comparison — it is part of the
	format, not an implementation detail.
*/
constexpr int comparePath(StringView l, StringView r) {
	auto rank = [](char c) { return (c == '/') ? 1 : (int(uint8_t(c)) + 1); };

	auto len = sprt::min(l.size(), r.size());
	for (size_t i = 0; i < len; ++i) {
		auto lc = rank(l[i]);
		auto rc = rank(r[i]);
		if (lc != rc) {
			return (lc < rc) ? -1 : 1;
		}
	}
	if (l.size() == r.size()) {
		return 0;
	}
	return (l.size() < r.size()) ? -1 : 1;
}

// Enumerates every registered bundle. Return false from the callback to stop.
SP_PUBLIC void enumerateBundles(const Callback<bool(const Bundle &)> &);

// Finds a registered bundle by its mount name, or nullptr.
SP_PUBLIC const Bundle *getBundle(StringView name);

// True when at least one bundle is linked into the binary.
SP_PUBLIC bool hasBundles();

/*
	Makes FileCategory::Embedded usable, adding the read-only location that serves the registered
	bundles. Idempotent and cheap after the first call.

	Called by the lookup entry points in stappler_filesystem, so ordinary code never needs it: the
	registration is deferred to first use rather than done from a static initializer, because the
	runtime's own filesystem tables may not exist yet while the app's static objects are built.
*/
SP_PUBLIC void ensureRegistered();

} // namespace stappler::filesystem::embedded

#endif /* STAPPLER_FILESYSTEM_SPFILESYSTEMEMBEDDED_H_ */
