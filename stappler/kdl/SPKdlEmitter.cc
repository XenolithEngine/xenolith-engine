/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "SPKdlEmitter.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

namespace detail {

uint8_t decodeUtf8(const char *ptr, size_t len, char32_t &out);
bool isIdentifierChar(char32_t c);
bool isIdentifierToken(StringView str);
bool isNewline(char32_t c);
bool isDisallowed(char32_t c);
void appendHex(mem_std::String &out, uint32_t value, uint32_t minDigits);

} // namespace detail

bool isIdentifier(StringView str) {
	size_t pos = 0;
	while (pos < str.size()) {
		char32_t c = 0;
		auto len = detail::decodeUtf8(str.data() + pos, str.size() - pos, c);
		if (len == 0 || !detail::isIdentifierChar(c)) {
			return false;
		}
		pos += len;
	}
	return detail::isIdentifierToken(str);
}

void writeQuoted(const OutputCallback &out, StringView str) {
	out << "\"";
	size_t pos = 0;
	size_t run = 0;
	auto flush = [&]() {
		if (pos > run) {
			out << StringView(str.data() + run, pos - run);
		}
	};
	while (pos < str.size()) {
		char32_t c = 0;
		auto len = detail::decodeUtf8(str.data() + pos, str.size() - pos, c);
		if (len == 0) {
			c = uint8_t(str[pos]);
			len = 1;
		}
		StringView escape;
		switch (c) {
		case '"': escape = "\\\""; break;
		case '\\': escape = "\\\\"; break;
		case '\b': escape = "\\b"; break;
		case '\f': escape = "\\f"; break;
		case '\n': escape = "\\n"; break;
		case '\r': escape = "\\r"; break;
		case '\t': escape = "\\t"; break;
		default: break;
		}
		if (!escape.empty() || detail::isNewline(c) || detail::isDisallowed(c)) {
			flush();
			if (!escape.empty()) {
				out << escape;
			} else {
				mem_std::String hex("\\u{");
				detail::appendHex(hex, uint32_t(c), 1);
				hex.push_back('}');
				out << StringView(hex);
			}
			pos += len;
			run = pos;
			continue;
		}
		pos += len;
	}
	flush();
	out << "\"";
}

void writeString(const OutputCallback &out, StringView str) {
	if (isIdentifier(str)) {
		out << str;
	} else {
		writeQuoted(out, str);
	}
}

void writeScalar(const OutputCallback &out, ValueType type, bool boolean, StringView text) {
	switch (type) {
	case ValueType::Null: out << "#null"; break;
	case ValueType::Bool: out << (boolean ? "#true" : "#false"); break;
	case ValueType::String: writeString(out, text); break;
	case ValueType::Integer:
	case ValueType::Float: out << text; break;
	case ValueType::Inf: out << "#inf"; break;
	case ValueType::NegInf: out << "#-inf"; break;
	case ValueType::NaN: out << "#nan"; break;
	}
}

} // namespace stappler::kdl
