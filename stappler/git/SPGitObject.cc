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

#include "SPGitObject.h"
#include "SPCoreCrypto.h"

namespace STAPPLER_VERSIONIZED stappler::git {

StringView getObjectTypeName(ObjectType t) {
	switch (t) {
	case ObjectType::Commit: return StringView("commit");
	case ObjectType::Tree: return StringView("tree");
	case ObjectType::Blob: return StringView("blob");
	case ObjectType::Tag: return StringView("tag");
	default: return StringView();
	}
}

// Write `value` as decimal into buf, return the number of chars written.
static size_t writeDecimal(char *buf, size_t value) {
	char tmp[24];
	size_t n = 0;
	if (value == 0) {
		tmp[n++] = '0';
	} else {
		while (value > 0) {
			tmp[n++] = char('0' + (value % 10));
			value /= 10;
		}
	}
	for (size_t i = 0; i < n; ++i) { buf[i] = tmp[n - 1 - i]; }
	return n;
}

Oid hashObject(ObjectType type, BytesView content, ObjectFormat fmt) {
	// git object header: "<type> <size>\0"
	char hdr[40];
	size_t pos = 0;
	StringView name = getObjectTypeName(type);
	for (size_t i = 0; i < name.size(); ++i) { hdr[pos++] = name.data()[i]; }
	hdr[pos++] = ' ';
	pos += writeDecimal(hdr + pos, content.size());
	hdr[pos++] = '\0';

	Oid o;
	o.format = fmt;
	if (fmt == ObjectFormat::Sha256) {
		crypto::Sha256 h;
		h.update(reinterpret_cast<const uint8_t *>(hdr), pos);
		h.update(content.data(), content.size());
		auto buf = h.final();
		for (size_t i = 0; i < 32; ++i) { o.bytes[i] = buf[i]; }
	} else {
		crypto::Sha1 h;
		h.update(reinterpret_cast<const uint8_t *>(hdr), pos);
		h.update(content.data(), content.size());
		auto buf = h.final();
		for (size_t i = 0; i < 20; ++i) { o.bytes[i] = buf[i]; }
	}
	return o;
}

Vector<TreeEntry> parseTree(BytesView data, ObjectFormat fmt) {
	Vector<TreeEntry> out;
	const uint8_t *p = data.data();
	size_t n = data.size();
	size_t oidSize = getOidSize(fmt);
	size_t i = 0;

	while (i < n) {
		// mode: ascii octal digits until space
		size_t start = i;
		while (i < n && p[i] != ' ') { ++i; }
		if (i >= n) {
			break;
		}
		uint32_t mode = 0;
		for (size_t k = start; k < i; ++k) {
			if (p[k] < '0' || p[k] > '7') {
				break;
			}
			mode = mode * 8 + uint32_t(p[k] - '0');
		}
		++i; // skip space

		// name: bytes until NUL
		size_t ns = i;
		while (i < n && p[i] != 0) { ++i; }
		if (i >= n) {
			break;
		}
		String name(reinterpret_cast<const char *>(p + ns), i - ns);
		++i; // skip NUL

		// oid: raw bytes
		if (i + oidSize > n) {
			break;
		}
		TreeEntry e;
		e.mode = mode;
		e.name = sp::move(name);
		e.oid.format = fmt;
		for (size_t k = 0; k < oidSize; ++k) { e.oid.bytes[k] = p[i + k]; }
		i += oidSize;

		out.emplace_back(sp::move(e));
	}
	return out;
}

// Find a header line "<prefix> <hex>" in a commit/tag body and return the hex oid.
static Oid findHeaderOid(BytesView data, StringView prefix, ObjectFormat fmt) {
	const char *p = reinterpret_cast<const char *>(data.data());
	size_t n = data.size();
	size_t i = 0;
	while (i < n) {
		size_t lineStart = i;
		while (i < n && p[i] != '\n') { ++i; }
		StringView line(p + lineStart, i - lineStart);
		if (i < n) {
			++i; // skip newline
		}
		if (line.starts_with(prefix) && line.size() > prefix.size()
				&& line.data()[prefix.size()] == ' ') {
			StringView hex(line.data() + prefix.size() + 1, line.size() - prefix.size() - 1);
			return Oid::fromHex(hex, fmt);
		}
		if (line.empty()) {
			break; // blank line ends the header block
		}
	}
	return Oid();
}

Oid commitTree(BytesView data, ObjectFormat fmt) { return findHeaderOid(data, "tree", fmt); }

Oid tagObject(BytesView data, ObjectFormat fmt) { return findHeaderOid(data, "object", fmt); }

} // namespace stappler::git
