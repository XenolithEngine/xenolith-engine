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

#ifndef TESTS_STAPPLER_ZIP_CORPUS_H_
#define TESTS_STAPPLER_ZIP_CORPUS_H_

#include "SPCommon.h"
#include "SPMemInterface.h"

namespace STAPPLER_VERSIONIZED stappler::test::zip {

using Bytes = memory::StandardInterface::BytesType;
using String = memory::StandardInterface::StringType;

template <typename T>
using Vector = memory::StandardInterface::VectorType<T>;

// One entry the archive is expected to expose, spelled the way ZipArchive reports it (not the way
// the ZIP file spells it - name transcoding is precisely one of the things under test).
struct EntryExpect {
	String name;
	Bytes content;

	// readFile() must succeed and hand back exactly `content`. False means the entry is expected to
	// be listed but not readable (a directory entry, or one the zip-bomb guard rejects).
	bool readable = true;
};

// What the builder ACTUALLY wrote for one entry, field by field.
//
// This is the oracle the from-scratch catalog parser (stage 2) is measured against, and it is a
// better one than libzip for the numbers: libzip's public API does not expose an entry's local
// header offset at all, while the corpus knows it exactly, having chosen it. Names here are the raw
// bytes as stored - decoding them is stage 3's job and is checked separately.
struct EntryMeta {
	Bytes rawName;

	// What the engine's own reader must call this entry after decoding. Defaults to the raw bytes,
	// which is right for every name that is already ASCII or UTF-8; a case whose name is transcoded
	// (CP437) or replaced (the 0x7075 extra field) sets it, and that assignment is where the
	// expected behaviour is written down.
	Bytes decodedName;

	// What ZipEntryFlags must come out as. Spelled as booleans rather than the engine's enum so that
	// the corpus stays independent of the module's headers.
	bool expectDirectory = false;
	bool expectNameRejected = false;
	bool expectEncrypted = false;
	bool expectUnsupportedMethod = false;

	// The bytes the builder was handed. What reading the entry has to produce.
	Bytes content;

	/* Exactly what zipReadEntry() must return for this entry.
	 *
	 * A status rather than a bool: several distinct rules refuse a read, and "it failed" is an
	 * assertion that passes even when the WRONG rule fires. Defaults are derived from the flags
	 * above; a case whose data is deliberately broken (bad CRC, truncated, size mismatch) sets it.
	 */
	Status expectRead = Status::Ok;

	uint16_t method = 0;
	uint16_t flags = 0;
	uint32_t crc32 = 0;

	uint64_t compressedSize = 0;
	uint64_t uncompressedSize = 0;

	// offset of the local header, relative to the start of the ARCHIVE (not of the source - see
	// Case::expectedPrefix)
	uint64_t localOffset = 0;
};

// Every corpus archive is stamped 2024-01-01 00:00:00, so that the bytes are reproducible. Read as
// UTC - which is what the engine's own reader does - that is this many seconds since the epoch.
// Spelled out rather than computed, so the test does not check the conversion against itself.
static constexpr uint64_t CORPUS_MTIME_UTC = 1'704'067'200;

// A single archive plus what reading it must produce.
//
// `entries` empty means "not yet characterized": the test prints the observed listing and asserts
// nothing about it. That is the deliberate starting state for the cases whose libzip behaviour
// cannot be predicted from the specification (CP437 names, 0x7075 precedence, path traversal,
// encrypted entries) - the first run reveals the truth and it gets written down here, rather than
// the reader later being bent to match a guess.
struct Case {
	String name;
	Bytes archive;
	Vector<EntryExpect> entries;

	// what the builder wrote, entry for entry, in central-directory order
	Vector<EntryMeta> meta;

	// the ZipArchive constructor must produce a usable handle
	bool openable = true;

	// entries are known and must match exactly; false = characterization-only (see above)
	bool characterized = true;

	// the engine's own ZipCatalog must accept the archive. Kept separate from `openable`, which is
	// about libzip: the two are allowed to disagree, and where they do, the divergence is deliberate
	// and spelled out in a comment on the case.
	bool parsable = true;

	// bytes preceding the archive inside the source - non-zero only for the self-extracting-style
	// case. Offsets stored in the archive are short by exactly this much.
	uint64_t expectedPrefix = 0;
};

// Builds every archive in the corpus from literals, byte by byte. Nothing is read from disk: a
// binary fixture in git could not be reviewed, and a generated one documents its own structure.
Vector<Case> buildCorpus();

// Raw-deflate a buffer (zlib with -MAX_WBITS), the wire form a ZIP method-8 entry carries.
Bytes deflateRaw(BytesView);

} // namespace stappler::test::zip

#endif /* TESTS_STAPPLER_ZIP_CORPUS_H_ */
