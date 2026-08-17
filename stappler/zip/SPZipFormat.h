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

#ifndef STAPPLER_ZIP_SPZIPFORMAT_H_
#define STAPPLER_ZIP_SPZIPFORMAT_H_

#include "SPCore.h" // Time, BytesView, Status

namespace STAPPLER_VERSIONIZED stappler {

/* The ZIP record layer: everything here decodes fixed-layout structures out of a byte range that
 * somebody else has already read, and NOTHING here allocates. That split is what keeps the format
 * knowledge independent of the memory interface - the owning, Interface-templated part lives in
 * SPZipCatalog.h.
 *
 * Every field this layer hands back has been bounds-checked against the buffer it came from. The
 * inputs are untrusted by definition, so the rule throughout is: validate, then read; never read and
 * then decide whether that was allowed.
 */

// Little-endian is not a property of the host here, it is a property of the format.
using ZipView = BytesViewTemplate<sprt::endian::little>;

// On-wire signatures.
static constexpr uint32_t ZIP_SIG_LOCAL = 0x04034b50;
static constexpr uint32_t ZIP_SIG_DESCRIPTOR = 0x08074b50;
static constexpr uint32_t ZIP_SIG_CENTRAL = 0x02014b50;
static constexpr uint32_t ZIP_SIG_EOCD64 = 0x06064b50;
static constexpr uint32_t ZIP_SIG_LOCATOR64 = 0x07064b50;
static constexpr uint32_t ZIP_SIG_EOCD = 0x06054b50;

// General purpose bit flags this reader cares about.
static constexpr uint16_t ZIP_FLAG_ENCRYPTED = 1 << 0;
static constexpr uint16_t ZIP_FLAG_DESCRIPTOR = 1 << 3;
static constexpr uint16_t ZIP_FLAG_UTF8 = 1 << 11;

// Compression methods this reader supports; everything else is refused by name.
static constexpr uint16_t ZIP_METHOD_STORE = 0;
static constexpr uint16_t ZIP_METHOD_DEFLATE = 8;

// Extra field ids.
static constexpr uint16_t ZIP_EXTRA_ZIP64 = 0x0001;
static constexpr uint16_t ZIP_EXTRA_UNICODE_PATH = 0x7075;

// Fixed sizes of the records, name/extra/comment excluded.
static constexpr size_t ZIP_EOCD_SIZE = 22;
static constexpr size_t ZIP_EOCD64_SIZE = 56;
static constexpr size_t ZIP_LOCATOR64_SIZE = 20;
static constexpr size_t ZIP_CENTRAL_SIZE = 46;
static constexpr size_t ZIP_LOCAL_SIZE = 30;

// The archive comment length field is 16 bits, so the EOCD can never start further back than this.
static constexpr size_t ZIP_MAX_COMMENT = 0xFFFF;

// The sentinel a 32-bit field carries when its real value lives in the ZIP64 extra field.
static constexpr uint32_t ZIP_MARK32 = 0xFFFFFFFF;
static constexpr uint16_t ZIP_MARK16 = 0xFFFF;

// The end-of-central-directory record, after ZIP64 has been resolved.
struct ZipEocd {
	uint64_t entryCount = 0;
	uint64_t cdSize = 0;
	uint64_t cdOffset = 0;

	// Where the record itself was found, as an absolute offset in the source.
	uint64_t eocdOffset = 0;

	// Absolute offset of the first record that FOLLOWS the central directory - the ordinary EOCD,
	// or the ZIP64 EOCD when there is one. This, not eocdOffset, is what the central directory
	// really ends at, and therefore what a prefix is measured against.
	uint64_t recordOffset = 0;

	bool zip64 = false;
};

// One central directory entry, exactly as the bytes spell it. `name` and `extra` are views INTO the
// central directory buffer the caller supplied, so they live exactly as long as it does.
struct ZipRawEntry {
	BytesView name;
	BytesView extra;

	uint64_t localOffset = 0;
	uint64_t compressedSize = 0;
	uint64_t uncompressedSize = 0;

	uint32_t crc32 = 0;
	uint16_t method = 0;
	uint16_t flags = 0;

	// packed DOS timestamp, kept raw so the time conversion stays in one place
	uint16_t dosTime = 0;
	uint16_t dosDate = 0;
};

/* Checked arithmetic. Offsets in a ZIP come straight off the wire, so `offset + length` is an
 * attacker-controlled expression and wrapping it is how a parser ends up reading somebody else's
 * memory. These are the only way this module adds an offset to a length.
 */
SP_PUBLIC bool zipCheckedAdd(uint64_t a, uint64_t b, uint64_t &out);

// True when [offset, offset + length) fits entirely inside a region of `total` bytes.
SP_PUBLIC bool zipRangeFits(uint64_t offset, uint64_t length, uint64_t total);

/* Locates the EOCD inside a tail buffer and decodes it.
 *
 * `tail` must be the last min(size, ZIP_MAX_COMMENT + ZIP_EOCD_SIZE) bytes of the source and
 * `tailOffset` their absolute position, because the record stores absolute offsets and the caller
 * gets them back in the same coordinates.
 *
 * The scan runs BACKWARDS: an archive comment may itself contain a valid EOCD signature, and the
 * real record is the last one whose declared comment length reaches exactly to the end of the file.
 */
SP_PUBLIC Status zipFindEocd(BytesView tail, uint64_t tailOffset, ZipEocd &out);

/* Upgrades an EOCD carrying ZIP64 sentinels using the ZIP64 locator and record that precede it.
 * `head` is the buffer the ordinary EOCD was found in, in the same coordinates as zipFindEocd.
 * Returns Declined when there are no sentinels to resolve - that is a success, not a failure.
 */
SP_PUBLIC Status zipResolveZip64(BytesView tail, uint64_t tailOffset, ZipEocd &eocd);

/* Works out how far the archive is shifted from the start of the source.
 *
 * A ZIP may be preceded by arbitrary bytes - a self-extracting stub, or a container that embeds it -
 * and in that case every offset stored inside it is short by the length of that prefix. The prefix
 * is what makes `recordOffset` and `cdOffset + cdSize` disagree. Returns false when the two cannot
 * be reconciled, which means the record is lying rather than shifted; the caller still has to
 * confirm the guess by looking for a central header at the shifted offset.
 */
SP_PUBLIC bool zipPrefixOffset(const ZipEocd &, uint64_t &out);

/* Decodes one central directory header, advancing `cursor` past it (header, name, extra and
 * comment). ZIP64 sentinels are resolved from the entry's own 0x0001 extra field.
 */
SP_PUBLIC Status zipReadCentralEntry(ZipView &cursor, ZipRawEntry &out);

/* Finds one extra field by id inside an extra-field block. Returns an empty view when absent; a
 * malformed block simply stops the walk, since a truncated tail cannot be trusted anyway.
 */
SP_PUBLIC BytesView zipFindExtraField(BytesView extra, uint16_t id);

/* Converts a packed DOS timestamp.
 *
 * A DOS timestamp carries no zone and has two-second granularity, so SOMETHING has to be assumed.
 * This reads it as UTC and computes the epoch with pure integer arithmetic: the result is then the
 * same on every machine, and the parser never touches the process timezone or DST rules.
 *
 * libzip assumes local time instead (mktime() in _zip_d2u_time), so the two differ by whatever the
 * reader's UTC offset happens to be. That divergence is deliberate - see the stage 2/3 notes in
 * docs/design/libzip-removal-plan.adoc.
 */
SP_PUBLIC Time zipDosToUtc(uint16_t date, uint16_t time);

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* STAPPLER_ZIP_SPZIPFORMAT_H_ */
