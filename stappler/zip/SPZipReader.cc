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

#include "SPZipReader.h"

#include <zlib.h>

namespace STAPPLER_VERSIONIZED stappler {

// How much compressed input is pulled from the source at a time. The same 16 KiB the packfile
// inflate wrapper uses (SPGitPack.cc); big enough that the per-call overhead disappears, small
// enough that it costs nothing to hold.
static constexpr size_t ZIP_CHUNK = 16'384;

/* The zip-bomb guard, carried over from the libzip-backed implementation with its constants
 * unchanged (SPZip.cc): an entry may not declare more than 16x its compressed size, with an 8 MiB
 * floor so that small entries are not caught by the ratio alone.
 *
 * It runs BEFORE anything is allocated, which is the whole point - the declared size comes off the
 * wire and a corrupt one would otherwise become an allocation request.
 */
static bool zipWithinBombLimit(const ZipEntry &entry) {
	uint64_t maxOutput = entry.compressedSize * 16;
	if (maxOutput < (uint64_t(8) << 20)) {
		maxOutput = uint64_t(8) << 20;
	}
	return entry.uncompressedSize <= maxOutput;
}

// Why this entry cannot be read at all, decided from what the catalog already classified in stages
// 2-3. Status::Ok means there is nothing standing in the way.
static Status zipEntryRefusal(const ZipEntry &entry) {
	if ((entry.state & ZipEntryFlags::NameRejected) != ZipEntryFlags::None) {
		return Status::ErrorNotPermitted;
	}
	if ((entry.state & ZipEntryFlags::Encrypted) != ZipEntryFlags::None) {
		return Status::ErrorNotImplemented;
	}
	if ((entry.state & ZipEntryFlags::UnsupportedMethod) != ZipEntryFlags::None) {
		return Status::ErrorNotSupported;
	}
	if ((entry.state & ZipEntryFlags::Directory) != ZipEntryFlags::None) {
		// Not an error: a directory entry simply has no content, and asking for it is a reasonable
		// thing for a caller to do.
		return Status::Declined;
	}
	return Status::Ok;
}

Status zipLocateEntryData(ZipSource &source, const ZipEntry &entry, uint64_t prefix,
		uint64_t &dataOffset) {
	auto sourceSize = source.size();

	uint64_t headerOffset = 0;
	if (!zipCheckedAdd(prefix, entry.localOffset, headerOffset)) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!zipRangeFits(headerOffset, ZIP_LOCAL_SIZE, sourceSize)) {
		return Status::ErrorInvalidArguemnt;
	}

	uint8_t buf[ZIP_LOCAL_SIZE];
	if (!source.readAt(headerOffset, buf, sizeof(buf))) {
		return Status::ErrorInvalidArguemnt;
	}

	ZipLocalHeader local;
	auto st = zipReadLocalHeader(BytesView(buf, sizeof(buf)), local);
	if (!sprt::status::isSuccessful(st)) {
		return st;
	}

	// The variable part is measured by the LOCAL header, never the central one - that is the only
	// reason this record is read at all.
	uint64_t variable = uint64_t(local.nameLength) + uint64_t(local.extraLength);

	uint64_t offset = 0;
	if (!zipCheckedAdd(headerOffset, ZIP_LOCAL_SIZE + variable, offset)) {
		return Status::ErrorInvalidArguemnt;
	}

	// The compressed range has to be inside the source before a single byte of it is read.
	if (!zipRangeFits(offset, entry.compressedSize, sourceSize)) {
		return Status::ErrorInvalidArguemnt;
	}

	dataOffset = offset;
	return Status::Ok;
}

// Copies a stored entry through, in chunks.
template <typename Bytes>
static Status zipReadStored(ZipSource &source, const ZipEntry &entry, uint64_t dataOffset,
		Bytes &out) {
	// Store means "as-is", so a header claiming otherwise is describing something that cannot exist.
	if (entry.compressedSize != entry.uncompressedSize) {
		return Status::ErrorNotRecoverable;
	}

	out.resize(size_t(entry.uncompressedSize));
	if (entry.uncompressedSize == 0) {
		return Status::Ok;
	}

	if (!source.readAt(dataOffset, out.data(), size_t(entry.uncompressedSize))) {
		return Status::ErrorInvalidArguemnt;
	}
	return Status::Ok;
}

// Raw inflate: a ZIP entry carries a bare deflate stream, with no zlib header or trailer, which is
// what the negative window size selects.
template <typename Bytes>
static Status zipReadDeflated(ZipSource &source, const ZipEntry &entry, uint64_t dataOffset,
		Bytes &out) {
	z_stream zs = {};
	if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
		return Status::ErrorNotRecoverable;
	}

	out.reserve(size_t(entry.uncompressedSize));

	uint8_t inBuf[ZIP_CHUNK];
	uint8_t outBuf[ZIP_CHUNK];

	uint64_t remaining = entry.compressedSize;
	uint64_t position = dataOffset;

	auto fail = [&](Status st) {
		inflateEnd(&zs);
		return st;
	};

	int ret = Z_OK;
	while (ret != Z_STREAM_END) {
		if (zs.avail_in == 0) {
			if (remaining == 0) {
				// The stream ended before it said it was done: the entry is short, not merely at a
				// chunk boundary.
				return fail(Status::ErrorNotRecoverable);
			}

			auto want = size_t(remaining < ZIP_CHUNK ? remaining : ZIP_CHUNK);
			if (!source.readAt(position, inBuf, want)) {
				return fail(Status::ErrorInvalidArguemnt);
			}
			position += want;
			remaining -= want;

			zs.next_in = inBuf;
			zs.avail_in = uInt(want);
		}

		zs.next_out = outBuf;
		zs.avail_out = uInt(sizeof(outBuf));

		auto availInBefore = zs.avail_in;

		ret = inflate(&zs, Z_NO_FLUSH);
		if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
			return fail(Status::ErrorNotRecoverable);
		}

		size_t have = sizeof(outBuf) - zs.avail_out;

		/* Termination is guaranteed HERE, and only here.
		 *
		 * The loop's own condition is Z_STREAM_END, which a corrupt stream is under no obligation
		 * to reach. zlib documents that it always makes progress or reports Z_BUF_ERROR, so this
		 * should be unreachable - but "should be" is not a termination argument, and the failure
		 * mode is a hang, which is the one outcome the fuzzing stage exists to rule out. A call
		 * that neither consumed input nor produced output cannot be followed by a different one.
		 */
		if (have == 0 && zs.avail_in == availInBefore && ret != Z_STREAM_END) {
			return fail(Status::ErrorNotRecoverable);
		}

		// Hard cap at the declared size. That number already survived the bomb guard, so honouring
		// it bounds the output no matter what the stream tries to expand to.
		if (out.size() + have > entry.uncompressedSize) {
			return fail(Status::ErrorNotRecoverable);
		}

		out.insert(out.end(), outBuf, outBuf + have);

		if (ret == Z_BUF_ERROR && have == 0 && zs.avail_in == 0 && remaining == 0) {
			// no input left, no progress possible, and the stream never terminated
			return fail(Status::ErrorNotRecoverable);
		}
	}

	inflateEnd(&zs);

	if (out.size() != entry.uncompressedSize) {
		return Status::ErrorNotRecoverable;
	}
	return Status::Ok;
}

template <typename Interface>
Status zipReadEntry(ZipSource &source, const ZipEntry &entry, uint64_t prefix,
		typename Interface::BytesType &out) {
	out.clear();

	if (!source.valid()) {
		return Status::ErrorInvalidArguemnt;
	}

	auto refusal = zipEntryRefusal(entry);
	if (refusal != Status::Ok) {
		return refusal;
	}

	if (!zipWithinBombLimit(entry)) {
		return Status::ErrorBufferOverflow;
	}

	uint64_t dataOffset = 0;
	auto st = zipLocateEntryData(source, entry, prefix, dataOffset);
	if (!sprt::status::isSuccessful(st)) {
		return st;
	}

	if (entry.method == ZIP_METHOD_STORE) {
		st = zipReadStored(source, entry, dataOffset, out);
	} else {
		st = zipReadDeflated(source, entry, dataOffset, out);
	}

	if (!sprt::status::isSuccessful(st)) {
		out.clear();
		return st;
	}

	// CRC last, over what was actually produced. A mismatch refuses the read outright rather than
	// handing back plausible-looking bytes: whatever consumes this parses it, and a parser given
	// corrupt input fails somewhere far less informative than here.
	auto crc = uint32_t(::crc32(::crc32(0, nullptr, 0), out.data(), uInt(out.size())));
	if (crc != entry.crc32) {
		out.clear();
		return Status::ErrorNotRecoverable;
	}

	return Status::Ok;
}

template Status zipReadEntry<mem_std::Interface>(ZipSource &, const ZipEntry &, uint64_t,
		mem_std::Interface::BytesType &);

template Status zipReadEntry<memory::PoolInterface>(ZipSource &, const ZipEntry &, uint64_t,
		memory::PoolInterface::BytesType &);

} // namespace STAPPLER_VERSIONIZED stappler
