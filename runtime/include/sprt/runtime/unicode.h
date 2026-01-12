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

constexpr inline char32_t utf8Decode32(const char *ptr, size_t len, uint8_t &offset) {
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

SPRT_INLINE constexpr inline uint8_t utf8EncodeLength(char32_t c) {
	if (c < 0x80) {
		return 1;
	} else if (c < 0x800) {
		return 2;
	} else if (c < 0x1'0000) {
		return 3;
	} else if (c < 0x11'0000) {
		return 4;
	} else {
		return 5;
	}
}

template <typename PutCharFn>
SPRT_INLINE constexpr inline uint8_t utf8EncodeCb(const PutCharFn &cb, char16_t c) {
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
	} else if (c < 0x11'0000) {
		cb(0b1111'0000 | (c >> 18));
		cb(0x80 | (c >> 12 & 0x3f));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 4;
	} else {
		cb(0b1111'1000 | (c >> 24));
		cb(0x80 | (c >> 18 & 0x3f));
		cb(0x80 | (c >> 12 & 0x3f));
		cb(0x80 | (c >> 6 & 0x3f));
		cb(0x80 | (c & 0x3f));
		return 5;
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
	if ((*ptr & char16_t(0xD800)) != 0) {
		offset = 2;
		if (offset > len) {
			return 0;
		}
		return char32_t(0b0000'0011'1111'1111 & ptr[0]) << 10
				| char32_t(0b0000'0011'1111'1111 & ptr[1]);
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
		// do nothing, wrong encoding
		return 0;
	} else if (c < 0x1'0000) {
		return 1;
	} else {
		return 2;
	}
}

template <typename PutCharFn>
SPRT_INLINE constexpr inline uint8_t utf16EncodeCb(const PutCharFn &cb, char32_t c) {
	if (c < 0xD800) {
		cb(char16_t(c));
		return 1;
	} else if (c <= 0xDFFF) {
		return 0;
	} else if (c < 0x1'0000) {
		cb(char16_t(c));
		return 1;
	} else {
		cb(char16_t(((0b1111'1111'1100'0000'0000 & c) >> 10) + 0xD800));
		cb(char16_t(((0b0000'0000'0011'1111'1111 & c) >> 00) + 0xDC00));
		return 2;
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

} // namespace sprt::unicode

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_UNICODE_H_
