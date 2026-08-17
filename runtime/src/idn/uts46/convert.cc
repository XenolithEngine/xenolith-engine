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

// UTF-8 <-> UTF-16 for the IDN path, with U+FFFD substitution.
//
// This deliberately does NOT reuse sprt::unicode::toUtf16 / getUtf16Length, and the
// difference is not cosmetic:
//
//  * sprt::unicode::utf8Decode32 (runtime/include/sprt/runtime/unicode.h:93) is the
//    permissive RFC 2279 decoder. It returns 0 - not U+FFFD - for an ill-formed
//    sequence, and it accepts overlong forms, surrogate code points and 5/6-byte
//    sequences by design, because it doubles as the host's private extended
//    encoding. Feeding that to UTS-46 would turn "disallowed character" into "NUL",
//    which UTS-46 then happily maps, so a malformed name would come back as a
//    different, plausible-looking, *accepted* name instead of being rejected.
//  * sprt::unicode::getUtf16Length(StringView) (runtime/core/runtime_core_unicode.cpp:183)
//    stops at an embedded NUL. A hostname is a byte string; a NUL in it is data, and
//    truncating there silently drops the rest of the name.
//
// So the IDN path gets a strict decoder that follows the Unicode "maximal subpart"
// rule (Unicode 5.2 §3.9, the same substitution ICU's UnicodeString::fromUTF8 and
// the WHATWG URL Standard apply): each maximal ill-formed subpart becomes exactly
// one U+FFFD, and decoding continues at the byte that ended it.

namespace sprt::idn::detail {

static constexpr char32_t ReplacementChar = 0xFFFD;

// Number of trail bytes a lead byte announces, and the valid range of the FIRST
// trail byte, which is narrower than 0x80..0xBF for 0xE0, 0xED, 0xF0 and 0xF4.
// Rejecting there is what excludes overlongs, surrogates and > U+10FFFF.
struct Utf8LeadInfo {
	uint8_t trailCount;
	uint8_t firstTrailMin;
	uint8_t firstTrailMax;
};

static constexpr Utf8LeadInfo utf8LeadInfo(uint8_t lead) {
	if (lead < 0x80) {
		return {0, 0, 0};
	} else if (lead < 0xC2) {
		return {0xFF, 0, 0}; // continuation byte or overlong 2-byte lead: ill-formed
	} else if (lead < 0xE0) {
		return {1, 0x80, 0xBF};
	} else if (lead == 0xE0) {
		return {2, 0xA0, 0xBF}; // exclude overlong 3-byte forms
	} else if (lead == 0xED) {
		return {2, 0x80, 0x9F}; // exclude the surrogate range
	} else if (lead < 0xF0) {
		return {2, 0x80, 0xBF};
	} else if (lead == 0xF0) {
		return {3, 0x90, 0xBF}; // exclude overlong 4-byte forms
	} else if (lead < 0xF4) {
		return {3, 0x80, 0xBF};
	} else if (lead == 0xF4) {
		return {3, 0x80, 0x8F}; // exclude > U+10FFFF
	} else {
		return {0xFF, 0, 0};
	}
}

static constexpr bool isUtf8Trail(uint8_t b) { return (b & 0xC0) == 0x80; }

// Decodes one code point at `src`, advancing it. On ill-formed input returns
// U+FFFD and advances past the maximal subpart (at least one byte).
static char32_t utf8DecodeStrict(const char *&src, const char *limit) {
	auto lead = uint8_t(*src++);
	auto info = utf8LeadInfo(lead);
	if (info.trailCount == 0) {
		return char32_t(lead);
	}
	if (info.trailCount == 0xFF) {
		return ReplacementChar;
	}
	if (src == limit) {
		return ReplacementChar;
	}

	auto t = uint8_t(*src);
	if (t < info.firstTrailMin || t > info.firstTrailMax) {
		// The lead byte alone is the maximal subpart; do not consume `t`.
		return ReplacementChar;
	}
	++src;
	char32_t c =
			char32_t(lead & (info.trailCount == 1 ? 0x1F : (info.trailCount == 2 ? 0x0F : 0x07)));
	c = (c << 6) | char32_t(t & 0x3F);

	for (uint8_t i = 1; i < info.trailCount; ++i) {
		if (src == limit || !isUtf8Trail(uint8_t(*src))) {
			return ReplacementChar;
		}
		c = (c << 6) | char32_t(uint8_t(*src++) & 0x3F);
	}
	return c;
}

// UTF-8 -> UTF-16 into `dest`, which is cleared first. False only on allocation
// failure; ill-formed input is substituted, never rejected here (UTS-46 rejects it
// later, as U+FFFD is a disallowed character).
static bool toUtf16(Utf16Buffer &dest, StringView src) {
	dest.clear();
	// Every input byte yields at most one UTF-16 unit except a 4-byte sequence,
	// which yields two from four bytes - so the byte count is always an upper bound.
	if (!dest.reserve(int32_t(src.size()))) {
		return false;
	}

	auto ptr = src.data();
	auto limit = ptr + src.size();
	while (ptr != limit) {
		auto c = utf8DecodeStrict(ptr, limit);
		if (!dest.appendCodepoint(c)) {
			return false;
		}
	}
	return true;
}

// Byte length of the UTF-8 form of `src`, substituting unpaired surrogates.
static size_t utf8Length(WideStringView src) {
	size_t ret = 0;
	auto ptr = src.data();
	auto limit = ptr + src.size();
	while (ptr != limit) {
		char32_t c;
		if (unicode::isUtf16HighSurrogate(*ptr) && (ptr + 1) != limit
				&& unicode::isUtf16LowSurrogate(*(ptr + 1))) {
			c = unicode::utf16CombineSurrogates(ptr[0], ptr[1]);
			ptr += 2;
		} else if (unicode::isUtf16Surrogate(*ptr)) {
			c = ReplacementChar;
			++ptr;
		} else {
			c = char32_t(*ptr++);
		}
		ret += unicode::utf8EncodeLength(c);
	}
	return ret;
}

// UTF-16 -> UTF-8 into a buffer the caller sized with utf8Length(). Returns the
// number of bytes written.
static size_t toUtf8(char *buf, size_t bufSize, WideStringView src) {
	auto target = buf;
	auto remains = bufSize;
	auto ptr = src.data();
	auto limit = ptr + src.size();
	while (ptr != limit) {
		char32_t c;
		if (unicode::isUtf16HighSurrogate(*ptr) && (ptr + 1) != limit
				&& unicode::isUtf16LowSurrogate(*(ptr + 1))) {
			c = unicode::utf16CombineSurrogates(ptr[0], ptr[1]);
			ptr += 2;
		} else if (unicode::isUtf16Surrogate(*ptr)) {
			c = ReplacementChar;
			++ptr;
		} else {
			c = char32_t(*ptr++);
		}
		auto written = unicode::utf8EncodeBuf(target, remains, c);
		target += written;
		remains -= written;
	}
	return size_t(target - buf);
}

} // namespace sprt::idn::detail
