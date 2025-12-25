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

#include "SPRuntimeUnicode.h"

#include <stdlib.h>

namespace sprt::unicode {

static char32_t Utf8DecodeHtml32(const char *ptr, uint32_t len) {
	if (ptr[0] == '#') {
		if (len > 1 && (ptr[1] == 'x' || ptr[1] == 'X')) {
			return char32_t(strtol(ptr + 2, nullptr, 16));
		}
		return char32_t(strtol(ptr + 1, nullptr, 10));
	} else if (__sprt_strncmp(ptr, "amp", len) == 0) {
		return '&';
	} else if (__sprt_strncmp(ptr, "nbsp", len) == 0) {
		return 0xA0;
	} else if (__sprt_strncmp(ptr, "quot", len) == 0) {
		return '"';
	} else if (__sprt_strncmp(ptr, "apos", len) == 0) {
		return '\'';
	} else if (__sprt_strncmp(ptr, "lt", len) == 0) {
		return '<';
	} else if (__sprt_strncmp(ptr, "gt", len) == 0) {
		return '>';
	} else if (__sprt_strncmp(ptr, "shy", len) == 0) {
		return char32_t(0x00AD);
	}
	return 0;
}

char32_t utf8HtmlDecode32(const char *utf8, size_t bufLen, uint8_t &offset) {
	if (utf8[0] == '&') {
		size_t maxchars = bufLen;

		uint32_t len = 0;
		while (maxchars > 0 && utf8[len] && utf8[len] != ';' && len < 10) {
			++len;
			--maxchars;
		}

		char32_t c = 0;
		if (maxchars > 0 && utf8[len] == ';' && len > 2) {
			c = Utf8DecodeHtml32(utf8 + 1, len - 2);
		}

		if (c == 0) {
			return utf8Decode32(utf8, bufLen, offset);
		} else {
			offset = (len + 1);
			return c;
		}
	} else {
		return utf8Decode32(utf8, bufLen, offset);
	}
}

template <class T>
static inline T Utf8NextChar(T p) {
	return (p + utf8_length_data[((const uint8_t *)p)[0]]);
}

template <class T>
static inline T Utf8NextChar(T p, size_t &counter) {
	auto l = utf8_length_data[((const uint8_t *)p)[0]];
	counter += 1;
	return (p + l);
}

bool isValidUtf8(StringView r) {
	static const uint8_t utf8_valid_data[256] = {
		//	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 6, 6, 0, 0};

	auto ptr = r.data();
	const auto end = ptr + r.size();
	while (ptr < end && *ptr != 0) {
		auto l = utf8_valid_data[((const uint8_t *)ptr)[0]];
		if (l == 0) {
			return false;
		} else if (l == 1) {
			++ptr;
		} else {
			while (l > 1) {
				--l;
				++ptr;

				if ((((const uint8_t *)ptr)[0] & 0b1100'0000) != 0b1000'0000) {
					return false;
				}
			}
			++ptr;
		}
	};
	return true;
}

size_t getUtf16Length(const StringView &input) {
	size_t counter = 0;
	auto ptr = input.data();
	const auto end = ptr + input.size();
	while (ptr < end && *ptr != 0) {
		counter += utf16_length_data[uint8_t(*ptr)];
		ptr += utf8_length_data[uint8_t(*ptr)];
	};
	return counter;
}

size_t getUtf16HtmlLength(const StringView &input) {
	size_t counter = 0;
	auto ptr = input.data();
	const auto end = ptr + input.size();
	while (ptr < end && *ptr != 0) {
		if (ptr[0] == '&') {
			uint8_t len = 0;
			while (ptr[len] && ptr[len] != ';' && len < 10) { len++; }

			if (ptr[len] == ';' && len > 2) {
				counter++;
				ptr += len;
			} else if (ptr[len] == 0) {
				ptr += len;
			} else {
				counter += utf16_length_data[uint8_t(*ptr)];
				ptr += utf8_length_data[uint8_t(*ptr)];
			}
		} else {
			counter += utf16_length_data[uint8_t(*ptr)];
			ptr += utf8_length_data[uint8_t(*ptr)];
		}
	};
	return counter;
}

size_t getUtf8HtmlLength(const StringView &input) {
	size_t counter = 0;
	auto ptr = input.data();
	const auto end = ptr + input.size();
	while (ptr < end && *ptr != 0) {
		if (ptr[0] == '&') {
			uint8_t len = 0;
			while (ptr[len] && ptr[len] != ';' && len < 10) { len++; }

			if (ptr[len] == ';' && len > 2) {
				auto c = Utf8DecodeHtml32(ptr + 1, len - 2);
				counter += utf8EncodeLength(c);
				ptr += len;
			} else if (ptr[len] == 0) {
				ptr += len;
			} else {
				counter += 1;
				ptr += 1;
			}
		} else {
			counter += 1;
			ptr += 1;
		}
	};
	return counter;
}

size_t getUtf8Length(const WideStringView &str) {
	const char16_t *ptr = str.data();
	const char16_t *end = ptr + str.size();
	size_t ret = 0;
	while (ptr < end) {
		auto c = *ptr++;
		if (c >= 0xD800 && c <= 0xDFFF) {
			// surrogates is 4-byte
			ret += 4;
			++ptr;
		} else {
			ret += utf8EncodeLength(c);
		}
	}
	return ret;
}

size_t getUtf8Length(const StringViewBase<char32_t> &str) {
	size_t ret = 0;
	for (auto &c : str) { ret += utf8EncodeLength(c); }
	return ret;
}

Status toUtf16(char16_t *ibuf, size_t bufSize, const StringView &utf8_str, size_t *ret) {
	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	uint8_t offset = 0;
	auto ptr = utf8_str.data();
	auto end = ptr + utf8_str.size();
	while (ptr < end) {
		auto ch = utf8Decode32(ptr, offset);
		if (bufEnd - buf < utf16EncodeLength(ch)) {
			return Status::ErrorBufferOverflow;
		}
		buf += utf16EncodeBuf(buf, bufEnd - buf, ch);
		ptr += offset;
	}
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf16(char16_t *ibuf, size_t bufSize, char32_t ch, size_t *ret) {
	if (bufSize < utf16EncodeLength(ch)) {
		return Status::ErrorBufferOverflow;
	}

	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	buf += utf16EncodeBuf(buf, bufEnd - buf, ch);
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf16Html(char16_t *ibuf, size_t bufSize, const StringView &utf8_str, size_t *ret) {
	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	uint8_t offset = 0;
	auto ptr = utf8_str.data();
	auto len = utf8_str.size();
	auto end = ptr + utf8_str.size();
	while (ptr < end) {
		auto ch = utf8HtmlDecode32(ptr, len, offset);
		if (bufEnd - buf < utf16EncodeLength(ch)) {
			return Status::ErrorBufferOverflow;
		}
		buf += utf16EncodeBuf(buf, bufEnd - buf, ch);
		ptr += offset;
		len -= offset;
	}
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf16(const callback<void(WideStringView)> &cb, const StringView &data) {
	auto len = getUtf16Length(data);
	auto buf = new char16_t[len + 1];

	auto st = toUtf16(buf, len + 1, data, &len);
	buf[len] = 0;

	if (st == Status::Ok) {
		cb(WideStringView(buf, len));
	}

	delete[] buf;
	return st;
}

Status toUtf16Html(const callback<void(WideStringView)> &cb, const StringView &data) {
	auto len = getUtf16HtmlLength(data);
	auto buf = new char16_t[len + 1];

	auto st = toUtf16Html(buf, len + 1, data, &len);
	buf[len] = 0;

	if (st == Status::Ok) {
		cb(WideStringView(buf, len));
	}

	delete[] buf;
	return st;
}

Status toUtf8(char *ibuf, size_t bufSize, const WideStringView &str, size_t *ret) {
	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	uint8_t offset;
	auto ptr = str.data();
	auto len = str.size();
	auto end = ptr + str.size();
	while (ptr < end) {
		auto ch = utf16Decode32(ptr, len, offset);
		if (bufEnd - buf < utf8EncodeLength(ch)) {
			return Status::ErrorBufferOverflow;
		}
		buf += utf8EncodeBuf(buf, bufEnd - buf, ch);
		ptr += offset;
		len -= offset;
	}
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf8(char *ibuf, size_t bufSize, char16_t ch, size_t *ret) {
	if (bufSize < utf8EncodeLength(ch)) {
		return Status::ErrorBufferOverflow;
	}
	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	buf += utf8EncodeBuf(buf, bufEnd - buf, ch);
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf8(char *ibuf, size_t bufSize, char32_t ch, size_t *ret) {
	if (bufSize < utf8EncodeLength(ch)) {
		return Status::ErrorBufferOverflow;
	}
	auto buf = ibuf;
	auto bufEnd = buf + bufSize;
	buf += utf8EncodeBuf(buf, bufEnd - buf, ch);
	if (ret) {
		*ret = buf - ibuf;
	}
	return Status::Ok;
}

Status toUtf8(const callback<void(StringView)> &cb, const WideStringView &data) {
	auto len = getUtf8Length(data);
	auto buf = new char[len + 1];

	auto st = toUtf8(buf, len + 1, data, &len);
	buf[len] = 0;

	if (st == Status::Ok) {
		cb(StringView(buf, len));
	}

	delete[] buf;
	return st;
}

} // namespace sprt::unicode
