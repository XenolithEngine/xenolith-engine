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

#include "SPKdlParser.h"

namespace STAPPLER_VERSIONIZED stappler::kdl {

namespace detail {

bool readNumber(StringView token, ValueType &type, mem_std::String &text);
void appendHex(mem_std::String &out, uint32_t value, uint32_t minDigits);

// Strict UTF-8: no overlong forms, no surrogates, nothing above U+10FFFF; 0 on a bad sequence.
uint8_t decodeUtf8(const char *ptr, size_t len, char32_t &out) {
	auto b0 = uint8_t(ptr[0]);
	if (b0 < 0x80) {
		out = b0;
		return 1;
	}
	uint8_t n = 0;
	char32_t c = 0;
	char32_t min = 0;
	if ((b0 & 0xE0) == 0xC0) {
		n = 2;
		c = b0 & 0x1F;
		min = 0x80;
	} else if ((b0 & 0xF0) == 0xE0) {
		n = 3;
		c = b0 & 0x0F;
		min = 0x800;
	} else if ((b0 & 0xF8) == 0xF0) {
		n = 4;
		c = b0 & 0x07;
		min = 0x1'0000;
	} else {
		return 0;
	}
	if (len < n) {
		return 0;
	}
	for (uint8_t i = 1; i < n; ++i) {
		auto b = uint8_t(ptr[i]);
		if ((b & 0xC0) != 0x80) {
			return 0;
		}
		c = (c << 6) | (b & 0x3F);
	}
	if (c < min || c > 0x10'FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
		return 0;
	}
	out = c;
	return n;
}

bool isUnicodeSpace(char32_t c) {
	switch (c) {
	case 0x09:
	case 0x20:
	case 0xA0:
	case 0x1680:
	case 0x202F:
	case 0x205F:
	case 0x3000: return true;
	default: return c >= 0x2000 && c <= 0x200A;
	}
}

bool isNewline(char32_t c) {
	switch (c) {
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x85:
	case 0x2028:
	case 0x2029: return true;
	default: return false;
	}
}

bool isDisallowed(char32_t c) {
	return c <= 0x08 || (c >= 0x0E && c <= 0x1F) || c == 0x7F || (c >= 0xD800 && c <= 0xDFFF)
			|| c == 0x200E || c == 0x200F || (c >= 0x202A && c <= 0x202E)
			|| (c >= 0x2066 && c <= 0x2069) || c == 0xFEFF || c > 0x10'FFFF;
}

bool isIdentifierChar(char32_t c) {
	if (isUnicodeSpace(c) || isNewline(c) || isDisallowed(c)) {
		return false;
	}
	switch (c) {
	case '\\':
	case '/':
	case '(':
	case ')':
	case '{':
	case '}':
	case ';':
	case '[':
	case ']':
	case '"':
	case '#':
	case '=': return false;
	default: return true;
	}
}

bool isDigit(char32_t c) { return c >= '0' && c <= '9'; }

bool isKeywordIdentifier(StringView str) {
	return str == "true" || str == "false" || str == "null" || str == "inf" || str == "-inf"
			|| str == "nan";
}

// Whether a run of identifier characters is an identifier string rather than a number, an
// almost-number (`.1`, `-.5`) or a bare keyword.
bool isIdentifierToken(StringView str) {
	if (str.empty() || isKeywordIdentifier(str)) {
		return false;
	}
	auto first = str[0];
	if (isDigit(first)) {
		return false;
	}
	if (first == '+' || first == '-') {
		if (str.size() > 1 && isDigit(str[1])) {
			return false;
		}
		if (str.size() > 2 && str[1] == '.' && isDigit(str[2])) {
			return false;
		}
	} else if (first == '.') {
		if (str.size() > 1 && isDigit(str[1])) {
			return false;
		}
	}
	return true;
}

} // namespace detail

namespace {

using detail::isDigit;
using detail::isIdentifierChar;
using detail::isNewline;
using detail::isUnicodeSpace;

void appendUtf8(mem_std::String &out, char32_t c) {
	sprt::unicode::utf8EncodeCb([&](char ch) { out.push_back(ch); }, c);
}

int hexValue(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

class Parser {
public:
	Parser(StringView source, ParserHandler &handler) : _src(source), _handler(&handler) { }

	bool run();

	size_t getErrorOffset() const { return _errorOffset; }
	StringView getError() const { return _error; }

private:
	// The codepoint at a byte position, and its length; zero length at the end.
	uint8_t at(size_t pos, char32_t &c) const {
		if (pos >= _src.size()) {
			c = 0;
			return 0;
		}
		return detail::decodeUtf8(_src.data() + pos, _src.size() - pos, c);
	}

	char32_t peek(size_t ahead = 0) const {
		auto pos = _pos;
		char32_t c = 0;
		for (size_t i = 0; i <= ahead; ++i) {
			auto len = at(pos, c);
			if (len == 0) {
				return 0;
			}
			pos += len;
		}
		return c;
	}

	bool eof() const { return _pos >= _src.size(); }
	bool startsWith(StringView str) const { return _src.sub(_pos).starts_with(str); }

	void advance() {
		char32_t c;
		_pos += at(_pos, c);
	}

	bool fail(size_t offset, StringView message) {
		if (_error.empty()) {
			_errorOffset = offset;
			_error = message.str<memory::StandardInterface>();
		}
		return false;
	}

	bool failed() const { return !_error.empty(); }

	bool validate();

	bool consumeNewline();
	void skipEscapedWhitespace();
	bool skipSingleLineComment();
	bool skipMultiLineComment();
	bool skipWs();
	bool skipEscline();
	bool skipNodeSpace();
	bool skipLineSpace();

	bool parseNodes(bool inChildren);
	bool parseNode(bool inChildren);
	bool parseChildren(bool muted);
	bool parseEntry(bool muted);
	bool parseValue(Scalar &out, mem_std::String &text, mem_std::String &annotation);
	bool parseAnnotation(mem_std::String &out);

	bool isStringStart() const;
	bool parseString(mem_std::String &out);
	bool parseIdentifier(mem_std::String &out);
	bool parseQuoted(mem_std::String &out);
	bool parseRaw(mem_std::String &out);
	bool readMultiLineBody(mem_std::String &raw, bool escapes, size_t hashes);
	bool dedent(StringView raw, size_t start, mem_std::String &out);
	bool resolveEscapes(StringView in, size_t start, mem_std::String &out);
	bool readToken(StringView &out);

	void emitBegin(const Scalar &s) {
		if (_muted == 0) {
			_handler->handleBeginNode(s);
		}
	}
	void emitEnd() {
		if (_muted == 0) {
			_handler->handleEndNode();
		}
	}
	void emitArgument(const Scalar &s) {
		if (_muted == 0) {
			_handler->handleArgument(s);
		}
	}
	void emitProperty(const Scalar &k, const Scalar &v) {
		if (_muted == 0) {
			_handler->handleProperty(k, v);
		}
	}

	StringView _src;
	ParserHandler *_handler = nullptr;
	size_t _pos = 0;
	uint32_t _muted = 0;

	size_t _errorOffset = 0;
	mem_std::String _error;

	mem_std::String _nodeName;
	mem_std::String _nodeAnnotation;
	mem_std::String _key;
	mem_std::String _value;
	mem_std::String _valueAnnotation;
};

bool Parser::validate() {
	size_t pos = 0;
	while (pos < _src.size()) {
		char32_t c = 0;
		auto len = at(pos, c);
		if (len == 0) {
			return fail(pos, "invalid UTF-8");
		}
		if (detail::isDisallowed(c) && !(c == 0xFEFF && pos == 0)) {
			mem_std::String message("disallowed code point U+");
			detail::appendHex(message, uint32_t(c), 4);
			return fail(pos, message);
		}
		pos += len;
	}
	return true;
}

bool Parser::consumeNewline() {
	auto c = peek();
	if (!isNewline(c)) {
		return false;
	}
	if (c == '\r' && peek(1) == '\n') {
		_pos += 2;
	} else {
		advance();
	}
	return true;
}

void Parser::skipEscapedWhitespace() {
	while (true) {
		if (isUnicodeSpace(peek())) {
			advance();
		} else if (!consumeNewline()) {
			break;
		}
	}
}

bool Parser::skipSingleLineComment() {
	if (!startsWith("//")) {
		return false;
	}
	_pos += 2;
	while (!eof() && !consumeNewline()) { advance(); }
	return true;
}

bool Parser::skipMultiLineComment() {
	if (!startsWith("/*")) {
		return false;
	}
	auto start = _pos;
	_pos += 2;
	uint32_t depth = 1;
	while (depth > 0) {
		if (eof()) {
			return fail(start, "unclosed comment");
		}
		if (startsWith("/*")) {
			_pos += 2;
			++depth;
		} else if (startsWith("*/")) {
			_pos += 2;
			--depth;
		} else {
			advance();
		}
	}
	return true;
}

bool Parser::skipWs() {
	bool any = false;
	while (!failed()) {
		if (isUnicodeSpace(peek())) {
			advance();
		} else if (!skipMultiLineComment()) {
			break;
		}
		any = true;
	}
	return any;
}

bool Parser::skipEscline() {
	auto start = _pos;
	++_pos;
	skipWs();
	if (failed()) {
		return false;
	}
	if (skipSingleLineComment() || consumeNewline() || eof()) {
		return true;
	}
	return fail(start, "line continuation must end the line");
}

bool Parser::skipNodeSpace() {
	bool any = false;
	while (!failed()) {
		bool step = skipWs();
		if (peek() == '\\') {
			if (!skipEscline()) {
				break;
			}
			step = true;
		}
		if (!step) {
			break;
		}
		any = true;
	}
	return any;
}

bool Parser::skipLineSpace() {
	bool any = false;
	while (!failed()) {
		if (!skipNodeSpace() && !consumeNewline() && !skipSingleLineComment()) {
			break;
		}
		any = true;
	}
	return any;
}

bool Parser::run() {
	if (!validate()) {
		return false;
	}
	if (peek() == 0xFEFF) {
		advance();
	}

	// `/- kdl-version 1` declares a document this parser does not read; `2` is a node like any other.
	StringView marker = _src.sub(_pos);
	if (marker.starts_with("/-")) {
		marker += 2;
		marker.skipChars<StringView::Chars<' ', '\t'>>();
		if (marker.starts_with("kdl-version")) {
			marker += StringView("kdl-version").size();
			marker.skipChars<StringView::Chars<' ', '\t'>>();
			if (marker.is('1')) {
				return fail(_pos, "KDL v1 documents are not supported");
			}
		}
	}

	return parseNodes(false);
}

bool Parser::parseNodes(bool inChildren) {
	while (true) {
		skipLineSpace();
		if (failed()) {
			return false;
		}
		if (eof()) {
			return inChildren ? fail(_pos, "unclosed children block") : true;
		}
		if (peek() == '}') {
			return inChildren ? true : fail(_pos, "unexpected '}'");
		}
		if (!parseNode(inChildren)) {
			return false;
		}
	}
}

bool Parser::parseNode(bool inChildren) {
	bool slashdash = false;
	if (startsWith("/-")) {
		_pos += 2;
		skipLineSpace();
		if (failed()) {
			return false;
		}
		if (eof() || peek() == '}') {
			return fail(_pos, "slashdash without a node");
		}
		slashdash = true;
		++_muted;
	}

	Scalar name;
	name.type = ValueType::String;
	name.offset = _pos;
	if (peek() == '(') {
		if (!parseAnnotation(_nodeAnnotation)) {
			return false;
		}
		name.hasAnnotation = true;
		name.annotation = _nodeAnnotation;
		skipNodeSpace();
		if (failed()) {
			return false;
		}
	}
	if (!isStringStart()) {
		return fail(_pos, "expected a node name");
	}
	if (!parseString(_nodeName)) {
		return false;
	}
	name.text = _nodeName;
	emitBegin(name);

	bool children = false;
	bool anyChildren = false;
	while (true) {
		bool spaced = skipNodeSpace();
		if (failed()) {
			return false;
		}
		auto c = peek();
		if (eof() || isNewline(c) || c == ';' || startsWith("//")) {
			break;
		}
		if (c == '}') {
			if (!inChildren) {
				return fail(_pos, "unexpected '}'");
			}
			break;
		}
		if (startsWith("/-")) {
			_pos += 2;
			skipLineSpace();
			if (failed()) {
				return false;
			}
			if (peek() == '{') {
				if (!parseChildren(true)) {
					return false;
				}
				anyChildren = true;
				continue;
			}
			if (anyChildren) {
				return fail(_pos, "an entry after a children block");
			}
			if (!parseEntry(true)) {
				return false;
			}
			continue;
		}
		if (c == '{') {
			if (children) {
				return fail(_pos, "a second children block");
			}
			if (!parseChildren(false)) {
				return false;
			}
			children = anyChildren = true;
			continue;
		}
		if (anyChildren) {
			return fail(_pos, "an entry after a children block");
		}
		if (!spaced) {
			return fail(_pos, "expected whitespace before an entry");
		}
		if (!parseEntry(false)) {
			return false;
		}
	}

	if (peek() == ';') {
		advance();
	} else if (!skipSingleLineComment()) {
		consumeNewline();
	}

	emitEnd();
	if (slashdash) {
		--_muted;
	}
	return true;
}

bool Parser::parseChildren(bool muted) {
	auto start = _pos;
	advance();
	if (muted) {
		++_muted;
	}
	if (!parseNodes(true)) {
		return false;
	}
	if (peek() != '}') {
		return fail(start, "unclosed children block");
	}
	advance();
	if (muted) {
		--_muted;
	}
	return true;
}

bool Parser::parseEntry(bool muted) {
	if (muted) {
		++_muted;
	}

	Scalar value;
	if (peek() != '(' && isStringStart()) {
		Scalar key;
		key.type = ValueType::String;
		key.offset = _pos;
		if (!parseString(_key)) {
			return false;
		}
		key.text = _key;

		auto save = _pos;
		skipNodeSpace();
		if (failed()) {
			return false;
		}
		if (peek() == '=') {
			advance();
			skipNodeSpace();
			if (failed() || !parseValue(value, _value, _valueAnnotation)) {
				return false;
			}
			emitProperty(key, value);
		} else {
			_pos = save;
			emitArgument(key);
		}
	} else {
		if (!parseValue(value, _value, _valueAnnotation)) {
			return false;
		}
		if (peek() == '=') {
			return fail(_pos, "a property name must be a string without a type annotation");
		}
		emitArgument(value);
	}

	if (muted) {
		--_muted;
	}
	return true;
}

bool Parser::parseAnnotation(mem_std::String &out) {
	auto start = _pos;
	advance();
	skipNodeSpace();
	if (failed()) {
		return false;
	}
	if (!isStringStart()) {
		return fail(_pos, "a type annotation must be a string");
	}
	if (!parseString(out)) {
		return false;
	}
	skipNodeSpace();
	if (failed()) {
		return false;
	}
	if (peek() != ')') {
		return fail(start, "unclosed type annotation");
	}
	advance();
	return true;
}

bool Parser::parseValue(Scalar &out, mem_std::String &text, mem_std::String &annotation) {
	out = Scalar();
	out.offset = _pos;
	if (peek() == '(') {
		if (!parseAnnotation(annotation)) {
			return false;
		}
		out.hasAnnotation = true;
		out.annotation = annotation;
		skipNodeSpace();
		if (failed()) {
			return false;
		}
	}

	auto start = _pos;
	if (isStringStart()) {
		if (!parseString(text)) {
			return false;
		}
		out.type = ValueType::String;
		out.text = text;
		return true;
	}

	if (peek() == '#') {
		advance();
		StringView word;
		if (!readToken(word)) {
			return fail(start, "expected a keyword after '#'");
		}
		if (word == "true" || word == "false") {
			out.type = ValueType::Bool;
			out.boolean = word == "true";
		} else if (word == "null") {
			out.type = ValueType::Null;
		} else if (word == "inf") {
			out.type = ValueType::Inf;
		} else if (word == "-inf") {
			out.type = ValueType::NegInf;
		} else if (word == "nan") {
			out.type = ValueType::NaN;
		} else {
			return fail(start, "unknown keyword");
		}
		out.literal = StringView(_src.data() + start, _pos - start);
		return true;
	}

	StringView token;
	if (!readToken(token)) {
		return fail(start, "expected a value");
	}
	if (!detail::readNumber(token, out.type, text)) {
		return fail(start, "invalid number");
	}
	out.text = text;
	out.literal = token;
	return true;
}

bool Parser::readToken(StringView &out) {
	auto start = _pos;
	char32_t c;
	while (true) {
		auto len = at(_pos, c);
		if (len == 0 || !isIdentifierChar(c)) {
			break;
		}
		_pos += len;
	}
	out = StringView(_src.data() + start, _pos - start);
	return !out.empty();
}

bool Parser::isStringStart() const {
	auto c = peek();
	if (c == '"') {
		return true;
	}
	if (c == '#') {
		auto n = peek(1);
		return n == '"' || n == '#';
	}
	if (!isIdentifierChar(c) || isDigit(c)) {
		return false;
	}
	if (c == '+' || c == '-') {
		auto n = peek(1);
		if (isDigit(n)) {
			return false;
		}
		if (n == '.' && isDigit(peek(2))) {
			return false;
		}
	} else if (c == '.' && isDigit(peek(1))) {
		return false;
	}
	return true;
}

bool Parser::parseString(mem_std::String &out) {
	out.clear();
	auto c = peek();
	if (c == '"') {
		return parseQuoted(out);
	} else if (c == '#') {
		return parseRaw(out);
	}
	return parseIdentifier(out);
}

bool Parser::parseIdentifier(mem_std::String &out) {
	auto start = _pos;
	StringView token;
	readToken(token);
	if (detail::isKeywordIdentifier(token)) {
		return fail(start, "a keyword must be written with '#', or quoted to be a string");
	}
	if (!detail::isIdentifierToken(token)) {
		return fail(start, "invalid identifier");
	}
	out.assign(token.data(), token.size());
	return true;
}

bool Parser::parseQuoted(mem_std::String &out) {
	auto start = _pos;
	if (startsWith("\"\"\"")) {
		_pos += 3;
		mem_std::String raw;
		if (!readMultiLineBody(raw, true, 0)) {
			return false;
		}
		mem_std::String dedented;
		return dedent(raw, start, dedented) && resolveEscapes(dedented, start, out);
	}

	advance();
	while (true) {
		if (eof()) {
			return fail(start, "unclosed string");
		}
		char32_t c;
		auto len = at(_pos, c);
		if (c == '"') {
			_pos += len;
			return true;
		}
		if (isNewline(c)) {
			return fail(_pos, "a newline in a string; use \\n or a multi-line string");
		}
		if (c == '\\') {
			auto escape = _pos;
			_pos += len;
			auto n = peek();
			if (isUnicodeSpace(n) || isNewline(n)) {
				skipEscapedWhitespace();
				continue;
			}
			// One escape, through the same code the multi-line form resolves its escapes with.
			auto escStart = _pos;
			if (n == 'u') {
				while (!eof() && peek() != '}' && peek() != '"' && !isNewline(peek())) {
					advance();
				}
				if (peek() == '}') {
					advance();
				}
			} else if (!eof()) {
				advance();
			}
			auto seq = mem_std::toString("\\", _src.sub(escStart, _pos - escStart));
			if (!resolveEscapes(seq, escape, out)) {
				return false;
			}
			continue;
		}
		out.append(_src.data() + _pos, len);
		_pos += len;
	}
}

bool Parser::parseRaw(mem_std::String &out) {
	auto start = _pos;
	size_t hashes = 0;
	while (peek() == '#') {
		advance();
		++hashes;
	}
	if (peek() != '"') {
		return fail(start, "expected '\"' after '#' in a raw string");
	}

	if (startsWith("\"\"\"")) {
		_pos += 3;
		mem_std::String raw;
		if (!readMultiLineBody(raw, false, hashes)) {
			return false;
		}
		return dedent(raw, start, out);
	}

	advance();
	while (true) {
		if (eof()) {
			return fail(start, "unclosed raw string");
		}
		char32_t c;
		auto len = at(_pos, c);
		if (c == '"') {
			size_t n = 0;
			while (n < hashes && _pos + 1 + n < _src.size() && _src[_pos + 1 + n] == '#') { ++n; }
			if (n == hashes) {
				_pos += 1 + hashes;
				return true;
			}
		}
		if (isNewline(c)) {
			return fail(_pos, "a newline in a raw string; use a multi-line raw string");
		}
		out.append(_src.data() + _pos, len);
		_pos += len;
	}
}

// The body of a multi-line string up to its closing quotes: newlines become LF and whitespace
// escapes are dropped, while other escapes stay as written, to be resolved after the dedent.
bool Parser::readMultiLineBody(mem_std::String &raw, bool escapes, size_t hashes) {
	auto start = _pos - 3 - hashes;
	if (!consumeNewline()) {
		return fail(start, "a multi-line string must start with a newline");
	}
	while (true) {
		if (eof()) {
			return fail(start, "unclosed multi-line string");
		}
		if (startsWith("\"\"\"")) {
			size_t n = 0;
			while (n < hashes && _pos + 3 + n < _src.size() && _src[_pos + 3 + n] == '#') { ++n; }
			if (n == hashes) {
				_pos += 3 + hashes;
				return true;
			}
		}
		if (consumeNewline()) {
			raw.push_back('\n');
			continue;
		}
		char32_t c;
		auto len = at(_pos, c);
		if (escapes && c == '\\') {
			auto n = peek(1);
			if (isUnicodeSpace(n) || isNewline(n)) {
				_pos += len;
				skipEscapedWhitespace();
				continue;
			}
			raw.push_back('\\');
			_pos += len;
			if (eof()) {
				continue;
			}
			len = at(_pos, c);
		}
		raw.append(_src.data() + _pos, len);
		_pos += len;
	}
}

bool Parser::dedent(StringView raw, size_t start, mem_std::String &out) {
	auto isSpaceOnly = [](StringView line) {
		size_t pos = 0;
		while (pos < line.size()) {
			char32_t c;
			auto len = detail::decodeUtf8(line.data() + pos, line.size() - pos, c);
			if (len == 0 || !isUnicodeSpace(c)) {
				return false;
			}
			pos += len;
		}
		return true;
	};

	mem_std::Vector<StringView> lines;
	size_t lineStart = 0;
	for (size_t i = 0; i <= raw.size(); ++i) {
		if (i == raw.size() || raw[i] == '\n') {
			lines.emplace_back(raw.data() + lineStart, i - lineStart);
			lineStart = i + 1;
		}
	}

	auto prefix = lines.back();
	if (!isSpaceOnly(prefix)) {
		return fail(start,
				"the closing quotes of a multi-line string must be on a line of their own");
	}

	out.clear();
	for (size_t i = 0; i + 1 < lines.size(); ++i) {
		if (i > 0) {
			out.push_back('\n');
		}
		auto line = lines[i];
		if (isSpaceOnly(line)) {
			continue;
		}
		if (!line.starts_with(prefix)) {
			return fail(start,
					"a line of a multi-line string does not start with the closing line's "
					"indentation");
		}
		out.append(line.data() + prefix.size(), line.size() - prefix.size());
	}
	return true;
}

bool Parser::resolveEscapes(StringView in, size_t start, mem_std::String &out) {
	size_t i = 0;
	while (i < in.size()) {
		auto c = in[i];
		if (c != '\\') {
			out.push_back(c);
			++i;
			continue;
		}
		if (i + 1 >= in.size()) {
			return fail(start, "an escape at the end of a string");
		}
		auto e = in[i + 1];
		i += 2;
		switch (e) {
		case 'n': out.push_back('\n'); break;
		case 'r': out.push_back('\r'); break;
		case 't': out.push_back('\t'); break;
		case '\\': out.push_back('\\'); break;
		case '"': out.push_back('"'); break;
		case 'b': out.push_back('\b'); break;
		case 'f': out.push_back('\f'); break;
		case 's': out.push_back(' '); break;
		case 'u': {
			if (i >= in.size() || in[i] != '{') {
				return fail(start, "a unicode escape is written \\u{hex}");
			}
			++i;
			char32_t value = 0;
			size_t digits = 0;
			while (i < in.size() && hexValue(in[i]) >= 0 && digits < 6) {
				value = value * 16 + char32_t(hexValue(in[i]));
				++i;
				++digits;
			}
			if (digits == 0 || i >= in.size() || in[i] != '}') {
				return fail(start, "a unicode escape is written \\u{hex} with 1 to 6 digits");
			}
			++i;
			if (value > 0x10'FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
				return fail(start, "a unicode escape must name a scalar value");
			}
			appendUtf8(out, value);
			break;
		}
		default: return fail(start, "invalid escape");
		}
	}
	return true;
}

} // namespace

LineIndex::LineIndex(StringView source) : _source(source) {
	_starts.emplace_back(0);
	size_t pos = 0;
	while (pos < source.size()) {
		char32_t c = 0;
		auto len = detail::decodeUtf8(source.data() + pos, source.size() - pos, c);
		if (len == 0) {
			len = 1;
		}
		pos += len;
		if (isNewline(c)) {
			if (c == '\r' && pos < source.size() && source[pos] == '\n') {
				++pos;
			}
			_starts.emplace_back(pos);
		}
	}
}

Location LineIndex::locate(size_t offset) const {
	auto it = sprt::upper_bound(_starts.begin(), _starts.end(), offset);
	auto line = size_t(it - _starts.begin());
	auto start = _starts[line - 1];
	uint32_t column = 1;
	for (size_t i = start; i < offset && i < _source.size(); ++i) {
		if ((uint8_t(_source[i]) & 0xC0) != 0x80) {
			++column;
		}
	}
	return Location{uint32_t(line), column};
}

Status parse(StringView source, ParserHandler &handler, ParseError *err) {
	Parser parser(source, handler);
	if (parser.run()) {
		return Status::Ok;
	}
	if (err) {
		err->offset = parser.getErrorOffset();
		err->location = LineIndex(source).locate(err->offset);
		err->message = parser.getError().str<memory::StandardInterface>();
	}
	return Status::ErrorInvalidArguemnt;
}

} // namespace stappler::kdl
