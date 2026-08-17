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

#include "SPZipFormat.h"

namespace STAPPLER_VERSIONIZED stappler {

bool zipCheckedAdd(uint64_t a, uint64_t b, uint64_t &out) {
	if (a > maxOf<uint64_t>() - b) {
		return false;
	}
	out = a + b;
	return true;
}

bool zipRangeFits(uint64_t offset, uint64_t length, uint64_t total) {
	uint64_t end = 0;
	if (!zipCheckedAdd(offset, length, end)) {
		return false;
	}
	return end <= total;
}

// Reads a fixed-size little-endian field only if the cursor still holds it, and consumes it. A
// short read leaves the cursor untouched and says so, which is what every caller here branches on.
static bool takeU16(ZipView &v, uint16_t &out) {
	if (v.size() < 2) {
		return false;
	}
	out = v.readUnsigned16();
	return true;
}

static bool takeU32(ZipView &v, uint32_t &out) {
	if (v.size() < 4) {
		return false;
	}
	out = v.readUnsigned32();
	return true;
}

static bool takeU64(ZipView &v, uint64_t &out) {
	if (v.size() < 8) {
		return false;
	}
	out = v.readUnsigned64();
	return true;
}

BytesView zipFindExtraField(BytesView extra, uint16_t id) {
	ZipView v(extra.data(), extra.size());

	// An extra-field block is a chain of (id, size, payload). A block whose declared size runs past
	// the end is not repairable - stop rather than guess, and report "not found" for the rest.
	while (v.size() >= 4) {
		uint16_t fieldId = 0;
		uint16_t fieldSize = 0;
		takeU16(v, fieldId);
		takeU16(v, fieldSize);

		if (v.size() < fieldSize) {
			break;
		}

		if (fieldId == id) {
			return BytesView(v.data(), fieldSize);
		}

		v += fieldSize;
	}

	return BytesView();
}

Status zipFindEocd(BytesView tail, uint64_t tailOffset, ZipEocd &out) {
	if (tail.size() < ZIP_EOCD_SIZE) {
		return Status::ErrorInvalidArguemnt;
	}

	// Scan backwards. The signature can legitimately occur inside an archive comment, so the record
	// is only accepted when its declared comment length lands exactly on the end of the source -
	// that is what tells a real record from four bytes of payload that happen to look like one.
	for (size_t back = 0; back + ZIP_EOCD_SIZE <= tail.size(); ++back) {
		size_t pos = tail.size() - ZIP_EOCD_SIZE - back;

		ZipView v(tail.data() + pos, tail.size() - pos);

		uint32_t sig = 0;
		takeU32(v, sig);
		if (sig != ZIP_SIG_EOCD) {
			continue;
		}

		uint16_t thisDisk = 0;
		uint16_t cdDisk = 0;
		uint16_t diskEntries = 0;
		uint16_t totalEntries = 0;
		uint32_t cdSize = 0;
		uint32_t cdOffset = 0;
		uint16_t commentLength = 0;

		takeU16(v, thisDisk);
		takeU16(v, cdDisk);
		takeU16(v, diskEntries);
		takeU16(v, totalEntries);
		takeU32(v, cdSize);
		takeU32(v, cdOffset);
		takeU16(v, commentLength);

		if (v.size() != commentLength) {
			continue;
		}

		// Multi-disk archives are out of scope, and a non-zero disk number is also the cheapest
		// signal that this is not really an EOCD.
		if (thisDisk != 0 || cdDisk != 0) {
			continue;
		}

		out.entryCount = totalEntries;
		out.cdSize = cdSize;
		out.cdOffset = cdOffset;
		out.eocdOffset = tailOffset + pos;
		out.recordOffset = out.eocdOffset;
		out.zip64 = (totalEntries == ZIP_MARK16 || diskEntries == ZIP_MARK16
				|| cdSize == ZIP_MARK32 || cdOffset == ZIP_MARK32);
		return Status::Ok;
	}

	return Status::ErrorInvalidArguemnt;
}

Status zipResolveZip64(BytesView tail, uint64_t tailOffset, ZipEocd &eocd) {
	if (!eocd.zip64) {
		return Status::Declined;
	}

	// The locator sits immediately before the ordinary EOCD.
	if (eocd.eocdOffset < tailOffset + ZIP_LOCATOR64_SIZE) {
		return Status::ErrorInvalidArguemnt;
	}

	size_t locatorPos = size_t(eocd.eocdOffset - ZIP_LOCATOR64_SIZE - tailOffset);
	ZipView v(tail.data() + locatorPos, ZIP_LOCATOR64_SIZE);

	uint32_t sig = 0;
	uint32_t cdDisk = 0;
	uint64_t eocd64Offset = 0;
	uint32_t totalDisks = 0;

	takeU32(v, sig);
	takeU32(v, cdDisk);
	takeU64(v, eocd64Offset);
	takeU32(v, totalDisks);

	if (sig != ZIP_SIG_LOCATOR64) {
		return Status::ErrorInvalidArguemnt;
	}

	// The ZIP64 EOCD it points at must be inside the buffer we hold, and before the locator.
	if (eocd64Offset < tailOffset || eocd64Offset >= tailOffset + tail.size()) {
		return Status::ErrorInvalidArguemnt;
	}

	size_t recordPos = size_t(eocd64Offset - tailOffset);
	if (tail.size() - recordPos < ZIP_EOCD64_SIZE) {
		return Status::ErrorInvalidArguemnt;
	}

	ZipView r(tail.data() + recordPos, ZIP_EOCD64_SIZE);

	uint32_t recordSig = 0;
	uint64_t recordSize = 0;
	uint16_t versionMade = 0;
	uint16_t versionNeeded = 0;
	uint32_t recordDisk = 0;
	uint32_t recordCdDisk = 0;
	uint64_t diskEntries = 0;
	uint64_t totalEntries = 0;
	uint64_t cdSize = 0;
	uint64_t cdOffset = 0;

	takeU32(r, recordSig);
	takeU64(r, recordSize);
	takeU16(r, versionMade);
	takeU16(r, versionNeeded);
	takeU32(r, recordDisk);
	takeU32(r, recordCdDisk);
	takeU64(r, diskEntries);
	takeU64(r, totalEntries);
	takeU64(r, cdSize);
	takeU64(r, cdOffset);

	if (recordSig != ZIP_SIG_EOCD64) {
		return Status::ErrorInvalidArguemnt;
	}

	if (recordDisk != 0 || recordCdDisk != 0) {
		return Status::ErrorInvalidArguemnt;
	}

	eocd.entryCount = totalEntries;
	eocd.cdSize = cdSize;
	eocd.cdOffset = cdOffset;

	// The ZIP64 record is now the thing that directly follows the central directory, so it - not the
	// ordinary EOCD, which sits another 20 bytes further on behind the locator - is what a prefix is
	// measured against.
	eocd.recordOffset = eocd64Offset;
	return Status::Ok;
}

bool zipPrefixOffset(const ZipEocd &eocd, uint64_t &out) {
	// Where the central directory would end if the archive started at offset 0.
	uint64_t declaredEnd = 0;
	if (!zipCheckedAdd(eocd.cdOffset, eocd.cdSize, declaredEnd)) {
		return false;
	}

	// Where it actually ends: immediately before the record that describes it.
	if (declaredEnd > eocd.recordOffset) {
		return false;
	}

	out = eocd.recordOffset - declaredEnd;
	return true;
}

Status zipReadCentralEntry(ZipView &cursor, ZipRawEntry &out) {
	if (cursor.size() < ZIP_CENTRAL_SIZE) {
		return Status::ErrorInvalidArguemnt;
	}

	ZipView v(cursor.data(), ZIP_CENTRAL_SIZE);

	uint32_t sig = 0;
	uint16_t versionMade = 0;
	uint16_t versionNeeded = 0;
	uint16_t flags = 0;
	uint16_t method = 0;
	uint16_t dosTime = 0;
	uint16_t dosDate = 0;
	uint32_t crc = 0;
	uint32_t compSize = 0;
	uint32_t rawSize = 0;
	uint16_t nameLength = 0;
	uint16_t extraLength = 0;
	uint16_t commentLength = 0;
	uint16_t diskStart = 0;
	uint16_t internalAttrs = 0;
	uint32_t externalAttrs = 0;
	uint32_t localOffset = 0;

	takeU32(v, sig);
	takeU16(v, versionMade);
	takeU16(v, versionNeeded);
	takeU16(v, flags);
	takeU16(v, method);
	takeU16(v, dosTime);
	takeU16(v, dosDate);
	takeU32(v, crc);
	takeU32(v, compSize);
	takeU32(v, rawSize);
	takeU16(v, nameLength);
	takeU16(v, extraLength);
	takeU16(v, commentLength);
	takeU16(v, diskStart);
	takeU16(v, internalAttrs);
	takeU32(v, externalAttrs);
	takeU32(v, localOffset);

	if (sig != ZIP_SIG_CENTRAL) {
		return Status::ErrorInvalidArguemnt;
	}

	uint64_t variable = uint64_t(nameLength) + uint64_t(extraLength) + uint64_t(commentLength);
	if (cursor.size() - ZIP_CENTRAL_SIZE < variable) {
		return Status::ErrorInvalidArguemnt;
	}

	auto base = cursor.data() + ZIP_CENTRAL_SIZE;

	out.name = BytesView(base, nameLength);
	out.extra = BytesView(base + nameLength, extraLength);
	out.flags = flags;
	out.method = method;
	out.dosTime = dosTime;
	out.dosDate = dosDate;
	out.crc32 = crc;
	out.compressedSize = compSize;
	out.uncompressedSize = rawSize;
	out.localOffset = localOffset;

	// ZIP64: the sentinels are replaced from the 0x0001 extra field, and the field packs only the
	// values that were actually sentinelled, in this fixed order. Reading it positionally is the
	// whole reason the order matters.
	if (rawSize == ZIP_MARK32 || compSize == ZIP_MARK32 || localOffset == ZIP_MARK32
			|| diskStart == ZIP_MARK16) {
		auto zip64 = zipFindExtraField(out.extra, ZIP_EXTRA_ZIP64);
		ZipView z(zip64.data(), zip64.size());

		if (rawSize == ZIP_MARK32 && !takeU64(z, out.uncompressedSize)) {
			return Status::ErrorInvalidArguemnt;
		}
		if (compSize == ZIP_MARK32 && !takeU64(z, out.compressedSize)) {
			return Status::ErrorInvalidArguemnt;
		}
		if (localOffset == ZIP_MARK32 && !takeU64(z, out.localOffset)) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	cursor += size_t(ZIP_CENTRAL_SIZE + variable);
	return Status::Ok;
}

Time zipDosToUtc(uint16_t date, uint16_t time) {
	uint32_t year = ((date >> 9) & 0x7F) + 1'980;
	uint32_t month = (date >> 5) & 0x0F;
	uint32_t day = date & 0x1F;

	uint32_t hour = (time >> 11) & 0x1F;
	uint32_t minute = (time >> 5) & 0x3F;
	uint32_t second = (time & 0x1F) * 2;

	// A DOS field can hold values that are not a date at all (month 0, day 0, hour 24). Clamp into
	// range rather than refuse: a nonsense timestamp is not a reason to reject an entry whose
	// content is fine, and every consumer of this value only displays it.
	if (month < 1) {
		month = 1;
	} else if (month > 12) {
		month = 12;
	}
	if (day < 1) {
		day = 1;
	} else if (day > 31) {
		day = 31;
	}
	if (hour > 23) {
		hour = 23;
	}
	if (minute > 59) {
		minute = 59;
	}
	if (second > 59) {
		second = 59;
	}

	// days_from_civil (Howard Hinnant): shift the year so that it starts in March, which puts the
	// leap day at the end of the cycle and removes every special case from the arithmetic. Pure
	// integers, no libc, no timezone - which is exactly why this is used instead of mktime().
	int64_t y = int64_t(year) - (month <= 2 ? 1 : 0);
	int64_t era = (y >= 0 ? y : y - 399) / 400;
	int64_t yoe = y - era * 400; // [0, 399]
	int64_t doy = (153 * (int64_t(month) + (month > 2 ? -3 : 9)) + 2) / 5 + int64_t(day) - 1;
	int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
	int64_t days = era * 146'097 + doe - 719'468; // days since 1970-01-01

	int64_t seconds = days * 86'400 + int64_t(hour) * 3'600 + int64_t(minute) * 60 + int64_t(second);

	return Time::seconds(sprt::time_t(seconds));
}

} // namespace STAPPLER_VERSIONIZED stappler
