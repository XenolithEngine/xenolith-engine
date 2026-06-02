/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_UNICODE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_UNICODE_H_

#include <sprt/runtime/callback.h>
#include <sprt/runtime/status.h>

namespace sprt::unicode {

// clang-format off

// Length lookup table
constexpr const uint8_t utf8_length_data[256] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1
};

constexpr const uint8_t utf16_length_data[256] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1
};

constexpr const uint8_t utf8_length_mask[256] = {
    0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x7f
};

// clang-format on

SPRT_INLINE constexpr inline bool isUtf8Surrogate(char c) { return (c & 0xC0) == 0x80; }

SPRT_INLINE constexpr inline bool isUtf16Surrogate(char16_t c) {
	return c >= 0xD800 && c <= 0xDFFF;
}

SPRT_INLINE constexpr inline bool isUtf16HighSurrogate(char16_t c) {
	return c >= 0xD800 && c <= 0xDBFF;
}
SPRT_INLINE constexpr inline bool isUtf16LowSurrogate(char16_t c) {
	return c >= 0xDC00 && c <= 0xDFFF;
}

SPRT_INLINE constexpr inline char32_t utf16CombineSurrogates(char16_t ch1, char16_t ch2) {
	return (char32_t(0b0000'0011'1111'1111 & ch1) << 10 | char32_t(0b0000'0011'1111'1111 & ch2))
			+ 0x1'0000;
}

constexpr inline char32_t utf8Decode32(const char *ptr, size_t len, uint8_t &offset) {
	if (len == 0) {
		offset = 0;
		return 0;
	}
	uint8_t mask = sprt::unicode::utf8_length_mask[uint8_t(*ptr)];
	offset = sprt::unicode::utf8_length_data[uint8_t(*ptr)];
	if (offset > len) {
		return 0;
	}
	char32_t ret = ptr[0] & mask;
	for (uint8_t c = 1; c < offset; ++c) {
		if ((ptr[c] & 0xc0) != 0x80) {
			ret = 0;
			break;
		}
		ret <<= 6;
		ret |= (ptr[c] & 0x3f);
	}
	return ret;
}

SPRT_INLINE constexpr inline char32_t utf8Decode32(const char *ptr, size_t len) {
	uint8_t offset;
	return utf8Decode32(ptr, len, offset);
}

SPRT_INLINE constexpr inline uint8_t utf8EncodeLength(char16_t c) {
	return (c < 0x80 ? 1 : (c < 0x800 ? 2 : 3));
}

// ---------------------------------------------------------------------------
// Extending Unicode past U+10FFFF (private codepoints / "extended UTF-8")
// ---------------------------------------------------------------------------
//
// The `char32_t` encode/decode primitives implement the original (RFC 2279)
// UTF-8 transformation, which spans 1..6 bytes and can carry any 31-bit scalar
// value, i.e. the full range U+0000 .. U+7FFFFFFF. Modern Unicode only assigns
// U+0000 .. U+10FFFF (1..4 bytes); the range U+110000 .. U+7FFFFFFF is left
// unassigned and is available to a host as a *private extension*.
//
// Byte length per codepoint (utf8EncodeLength / the utf8_length_data table):
//
//     U+000000 .. U+00007F   1 byte    0xxxxxxx
//     U+000080 .. U+0007FF   2 bytes   110xxxxx ...
//     U+000800 .. U+00FFFF   3 bytes   1110xxxx ...
//     U+010000 .. U+1FFFFF   4 bytes   11110xxx ...        (Unicode ends at 0x10FFFF)
//     U+200000 .. U+3FFFFFF  5 bytes   111110xx ...        \ private extension
//     U+4000000 .. U+7FFFFFFF 6 bytes  1111110x ...        /
//
// How to use it:
//   * Pick your private codepoints anywhere in U+110000 .. U+7FFFFFFF.
//   * Store and transport them as UTF-8 (`char`/StringView) or UTF-32
//     (`char32_t`/StringViewBase<char32_t>). Both forms round-trip losslessly:
//       toUtf8(cb, StringViewBase<char32_t>{...})  // encode
//       utf8Decode32(ptr, len, offset)             // decode one codepoint
//   * The encoders are length-exact: getUtf8Length()/utf8EncodeLength() always
//     agree with what utf8EncodeBuf()/utf8EncodeCb() writes, so a buffer sized
//     from getUtf8Length() never overflows.
//
// Hard limits and caveats:
//   * U+D800 .. U+DFFF are UTF-16 surrogate code points, not scalar values.
//     They are rejected by the UTF-16 encoder (length 0, nothing emitted).
//   * UTF-16 / WideString CANNOT represent anything above U+10FFFF. Encoding a
//     private codepoint to UTF-16 emits nothing (utf16EncodeLength() == 0); the
//     codepoint is silently dropped. Keep extended text in UTF-8 or UTF-32 and
//     never route it through char16_t / WideStringView.
//   * Values above U+7FFFFFFF do not fit extended UTF-8 (31 bits) and are not
//     representable; do not use them.
//   * 5/6-byte sequences are NOT valid modern (RFC 3629) UTF-8. Strict external
//     decoders will reject them. This scheme is for host-internal data only.
// ---------------------------------------------------------------------------
SPRT_INLINE constexpr inline uint8_t utf8EncodeLength(char32_t c) {
	if (c < 0x80) {
		return 1;
	} else if (c < 0x800) {
		return 2;
	} else if (c < 0x1'0000) {
		return 3;
	} else if (c < 0x20'0000) {
		return 4;
	} else if (c < 0x400'0000) {
		return 5;
	} else {
		return 6;
	}
}

template <typename PutCharFn>
SPRT_INLINE constexpr inline uint8_t utf8EncodeCb(const PutCharFn &cb, char16_t c) {
	static_assert(sprt::is_invocable_v<PutCharFn, char>, "Invalid callback type");
	if (c < 0x80) {
		cb(char(c));
		return 1;
	} else if (c < 0x800) {
		cb(0xc0 | (c >> 6));
		cb(0x80 | (c & 0x3f));
		return 2;
	} else {
		cb(0xe0 | (c >> 12));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 3;
	}
}

template <typename PutCharFn>
SPRT_INLINE constexpr inline uint8_t utf8EncodeCb(const PutCharFn &cb, char32_t c) {
	static_assert(sprt::is_invocable_v<PutCharFn, char>, "Invalid callback type");
	if (c < 0x80) {
		cb(char(c));
		return 1;
	} else if (c < 0x800) {
		cb(0xc0 | (c >> 6));
		cb(0x80 | (c & 0x3f));
		return 2;
	} else if (c < 0x1'0000) {
		cb(0b1110'0000 | (c >> 12));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 3;
	} else if (c < 0x20'0000) {
		cb(0b1111'0000 | (c >> 18));
		cb(0x80 | (c >> 12 & 0x3f));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 4;
	} else if (c < 0x400'0000) {
		cb(0b1111'1000 | (c >> 24));
		cb(0x80 | (c >> 18 & 0x3f));
		cb(0x80 | (c >> 12 & 0x3f));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 5;
	} else {
		cb(0b1111'1100 | (c >> 30));
		cb(0x80 | (c >> 24 & 0x3f));
		cb(0x80 | (c >> 18 & 0x3f));
		cb(0x80 | (c >> 12 & 0x3f));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 6;
	}
}

SPRT_INLINE constexpr inline uint8_t utf8EncodeBuf(char *ptr, size_t bufSize, char16_t ch) {
	size_t remains = bufSize;
	utf8EncodeCb([&](char c) SPRT_INLINE_LAMBDA {
		if (remains > 0) {
			*ptr++ = c;
			--remains;
		}
	}, ch);
	return bufSize - remains;
}

SPRT_INLINE constexpr inline uint8_t utf8EncodeBuf(char *ptr, size_t bufSize, char32_t ch) {
	size_t remains = bufSize;
	utf8EncodeCb([&](char c) SPRT_INLINE_LAMBDA {
		if (remains > 0) {
			*ptr++ = c;
			--remains;
		}
	}, ch);
	return bufSize - remains;
}

SPRT_INLINE constexpr inline char32_t utf16Decode32(const char16_t *ptr, size_t len,
		uint8_t &offset) {
	if (len == 0) {
		offset = 0;
		return 0;
	}
	if (isUtf16HighSurrogate(*ptr)) {
		// A surrogate pair needs a low surrogate in the next unit; anything else
		// (truncated input or an unpaired high surrogate) consumes a single unit.
		if (len < 2 || !isUtf16LowSurrogate(ptr[1])) {
			offset = 1;
			return 0;
		}
		offset = 2;
		return utf16CombineSurrogates(ptr[0], ptr[1]);
	} else {
		offset = 1;
		if (offset > len) {
			return 0;
		}
		return char32_t(*ptr);
	}
}

SPRT_INLINE constexpr inline uint8_t utf16EncodeLength(char32_t c) {
	if (c < 0xD800) {
		return 1;
	} else if (c <= 0xDFFF) {
		// surrogate code points are not scalar values
		return 0;
	} else if (c < 0x1'0000) {
		return 1;
	} else if (c <= 0x10'FFFF) {
		return 2;
	} else {
		// beyond U+10FFFF: not representable in UTF-16 (see extended-UTF-8 note above)
		return 0;
	}
}

template <typename PutCharFn>
SPRT_INLINE constexpr inline uint8_t utf16EncodeCb(const PutCharFn &cb, char32_t c) {
	static_assert(sprt::is_invocable_v<PutCharFn, char16_t>, "Invalid callback type");
	if (c < 0xD800) {
		cb(char16_t(c));
		return 1;
	} else if (c <= 0xDFFF) {
		return 0; // surrogate code points are not scalar values
	} else if (c < 0x1'0000) {
		cb(char16_t(c));
		return 1;
	} else if (c <= 0x10'FFFF) {
		// split into a high/low surrogate pair over the 20-bit (c - 0x10000)
		char32_t v = c - 0x1'0000;
		cb(char16_t(0xD800 + (v >> 10)));
		cb(char16_t(0xDC00 + (v & 0x3FF)));
		return 2;
	} else {
		return 0; // beyond U+10FFFF: not representable in UTF-16
	}
}

SPRT_INLINE constexpr inline uint8_t utf16EncodeBuf(char16_t *ptr, size_t bufSize, char32_t ch) {
	size_t remains = bufSize;
	utf16EncodeCb([&](char16_t c) SPRT_INLINE_LAMBDA {
		if (remains > 0) {
			*ptr++ = c;
			--remains;
		}
	}, ch);
	return bufSize - remains;
}

SPRT_API char32_t utf8HtmlDecode32(const char *utf8, size_t len, uint8_t &offset);

SPRT_API char toKoi8r(char16_t c);

} // namespace sprt::unicode

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_UNICODE_H_
