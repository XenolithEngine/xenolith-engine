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

#include "SPZipNames.h"
#include "SPFilepath.h"

#include <zlib.h>

namespace STAPPLER_VERSIONIZED stappler {

/* CP437 to Unicode, transcribed literally from libzip's lib/zip_utf-8.c (_cp437_to_unicode).
 *
 * This is the code page the format falls back to, and the mapping is what decides whether a legacy
 * archive reads correctly - so it is copied rather than derived, and must not be "simplified".
 * Note that 0x00-0x1F are NOT control characters here: CP437 gives them printable glyphs, and a
 * name carrying such a byte therefore decodes to visible symbols.
 */
static const uint16_t s_cp437ToUnicode[256] = {
	/* 0x00 - 0x0F */
	0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642,
	0x2640, 0x266A, 0x266B, 0x263C,

	/* 0x10 - 0x1F */
	0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190,
	0x221F, 0x2194, 0x25B2, 0x25BC,

	/* 0x20 - 0x2F */
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B,
	0x002C, 0x002D, 0x002E, 0x002F,

	/* 0x30 - 0x3F */
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F,

	/* 0x40 - 0x4F */
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B,
	0x004C, 0x004D, 0x004E, 0x004F,

	/* 0x50 - 0x5F */
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B,
	0x005C, 0x005D, 0x005E, 0x005F,

	/* 0x60 - 0x6F */
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F,

	/* 0x70 - 0x7F */
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B,
	0x007C, 0x007D, 0x007E, 0x2302,

	/* 0x80 - 0x8F */
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF,
	0x00EE, 0x00EC, 0x00C4, 0x00C5,

	/* 0x90 - 0x9F */
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2,
	0x00A3, 0x00A5, 0x20A7, 0x0192,

	/* 0xA0 - 0xAF */
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD,
	0x00BC, 0x00A1, 0x00AB, 0x00BB,

	/* 0xB0 - 0xBF */
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557,
	0x255D, 0x255C, 0x255B, 0x2510,

	/* 0xC0 - 0xCF */
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566,
	0x2560, 0x2550, 0x256C, 0x2567,

	/* 0xD0 - 0xDF */
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588,
	0x2584, 0x258C, 0x2590, 0x2580,

	/* 0xE0 - 0xEF */
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4,
	0x221E, 0x03C6, 0x03B5, 0x2229,

	/* 0xF0 - 0xFF */
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A,
	0x207F, 0x00B2, 0x25A0, 0x00A0,
};

bool zipIsValidUtf8(BytesView name) {
	auto data = name.data();
	auto size = name.size();

	size_t i = 0;
	while (i < size) {
		auto lead = data[i];

		if (lead < 0x80) {
			++i;
			continue;
		}

		size_t extra = 0;
		uint32_t cp = 0;

		if ((lead & 0xE0) == 0xC0) {
			extra = 1;
			cp = lead & 0x1F;
		} else if ((lead & 0xF0) == 0xE0) {
			extra = 2;
			cp = lead & 0x0F;
		} else if ((lead & 0xF8) == 0xF0) {
			extra = 3;
			cp = lead & 0x07;
		} else {
			// a continuation byte in lead position, or an out-of-range 5/6-byte form
			return false;
		}

		// `extra` counts the CONTINUATION bytes, so the sequence occupies i .. i+extra and fits only
		// while i+extra is still an index. (libzip spells the same test the same way.)
		if (i + extra >= size) {
			return false;
		}

		for (size_t j = 1; j <= extra; ++j) {
			auto cont = data[i + j];
			if ((cont & 0xC0) != 0x80) {
				return false;
			}
			cp = (cp << 6) | (cont & 0x3F);
		}

		// Overlong forms give a second spelling for a character that already has one - including
		// '/' - so they are refused rather than normalized.
		if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800)
				|| (extra == 3 && cp < 0x1'0000)) {
			return false;
		}

		// surrogates are not characters, and nothing above U+10FFFF exists
		if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10'FFFF) {
			return false;
		}

		i += extra + 1;
	}

	return true;
}

static bool isAsciiOnly(BytesView name) {
	for (size_t i = 0; i < name.size(); ++i) {
		if (name.data()[i] >= 0x80) {
			return false;
		}
	}
	return true;
}

ZipNameEncoding zipGuessEncoding(BytesView name, bool utf8Flag) {
	if (isAsciiOnly(name)) {
		// Note that this includes bytes below 0x20. libzip routes those through CP437, which turns
		// them into printable glyphs; here they are left alone, and the sanitizer is what decides
		// whether such a name is usable.
		return ZipNameEncoding::Ascii;
	}

	if (zipIsValidUtf8(name)) {
		// Valid UTF-8 is taken as UTF-8 whether or not bit 11 says so. The flag being absent is not
		// evidence of anything: the stock Linux `zip` never sets it.
		return ZipNameEncoding::Utf8;
	}

	// Bit 11 claimed UTF-8 and the bytes are not UTF-8. libzip calls that an encoding error;
	// falling back to CP437 instead gives a readable - if wrong - name rather than nothing.
	(void)utf8Flag;
	return ZipNameEncoding::Cp437;
}

BytesView zipUnicodePathName(const ZipRawEntry &entry) {
	auto field = zipFindExtraField(entry.extra, ZIP_EXTRA_UNICODE_PATH);

	// version byte + CRC32 is the minimum; anything shorter cannot be this field
	if (field.size() < 5) {
		return BytesView();
	}
	if (field.data()[0] != 1) {
		return BytesView();
	}

	ZipView v(field.data() + 1, 4);
	auto declaredCrc = v.readUnsigned32();

	auto actualCrc = uint32_t(
			::crc32(::crc32(0, nullptr, 0), entry.name.data(), uInt(entry.name.size())));

	// The CRC ties the field to the header name it replaces. Skipping this check would turn 0x7075
	// into a way of making an entry answer to a name that has nothing to do with its content.
	if (declaredCrc != actualCrc) {
		return BytesView();
	}

	return BytesView(field.data() + 5, field.size() - 5);
}

void zipDecodeName(const ZipRawEntry &entry, const Callback<void(BytesView)> &cb) {
	auto unicodePath = zipUnicodePathName(entry);
	if (!unicodePath.empty()) {
		// The field is UTF-8 by definition. It is still validated, because "by definition" is not a
		// property of untrusted bytes - a malformed one is dropped in favour of the header name.
		if (zipIsValidUtf8(unicodePath)) {
			cb(unicodePath);
			return;
		}
	}

	auto name = entry.name;
	auto encoding = zipGuessEncoding(name, (entry.flags & ZIP_FLAG_UTF8) != 0);

	if (encoding != ZipNameEncoding::Cp437) {
		cb(name);
		return;
	}

	// CP437 maps into the BMP, so three bytes always suffice.
	char buf[4] = {0};
	for (size_t i = 0; i < name.size(); ++i) {
		auto cp = char32_t(s_cp437ToUnicode[name.data()[i]]);
		auto written = sprt::unicode::utf8EncodeBuf(buf, sizeof(buf), cp);
		cb(BytesView((const uint8_t *)buf, written));
	}
}

ZipNameRejection zipCheckName(BytesView name) {
	if (name.empty()) {
		return ZipNameRejection::Empty;
	}

	auto data = name.data();
	auto size = name.size();

	// Scanned before anything else looks at the name as text: a NUL truncates it in every consumer
	// downstream, so what follows one would never be checked at all.
	for (size_t i = 0; i < size; ++i) {
		if (data[i] == 0) {
			return ZipNameRejection::EmbeddedNul;
		}
		if (data[i] == '\\') {
			return ZipNameRejection::Backslash;
		}
	}

	if (data[0] == '/') {
		return ZipNameRejection::Absolute;
	}

	// "C:" - a drive-relative or drive-absolute Windows path. The colon is meaningless on POSIX,
	// but the name would still be wrong on the platform that wrote it.
	if (size >= 2 && data[1] == ':'
			&& ((data[0] >= 'A' && data[0] <= 'Z') || (data[0] >= 'a' && data[0] <= 'z'))) {
		return ZipNameRejection::DriveLetter;
	}

	// '.', '..' and empty segments. The runtime already knows this rule and the reconstruct/merge
	// path relies on the same definition, so it is asked rather than re-implemented.
	if (!sprt::filepath::validatePath(StringView((const char *)data, size))) {
		return ZipNameRejection::PathSegments;
	}

	return ZipNameRejection::None;
}

} // namespace STAPPLER_VERSIONIZED stappler
