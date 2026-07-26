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

#include "SPGitProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::git {

// --- small local helpers ---------------------------------------------------

static int hexNibble(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

// Parse a 4-char pkt-line length header; returns -1 on non-hex input.
static int parsePktLength(const char *p) {
	int n = 0;
	for (int i = 0; i < 4; ++i) {
		int v = hexNibble(p[i]);
		if (v < 0) {
			return -1;
		}
		n = (n << 4) | v;
	}
	return n;
}

// Format `value` as a 4-char lowercase hex header into `out`.
static void writePktLength(Bytes &out, uint32_t value) {
	static const char *digits = "0123456789abcdef";
	char hdr[4] = {digits[(value >> 12) & 0xf], digits[(value >> 8) & 0xf],
		digits[(value >> 4) & 0xf], digits[value & 0xf]};
	out.insert(out.end(), reinterpret_cast<const uint8_t *>(hdr),
			reinterpret_cast<const uint8_t *>(hdr) + 4);
}

// Drop a single trailing '\n' (and optional '\r') if present.
static StringView trimEol(StringView s) {
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
		s = StringView(s.data(), s.size() - 1);
	}
	return s;
}

// Split off the first space-separated token; advances `s` past it and the space.
static StringView nextToken(StringView &s) {
	size_t i = 0;
	while (i < s.size() && s.data()[i] != ' ') { ++i; }
	StringView tok(s.data(), i);
	if (i < s.size()) {
		++i; // skip the space
	}
	s = StringView(s.data() + i, s.size() - i);
	return tok;
}

// --- Oid -------------------------------------------------------------------

Oid Oid::fromHex(StringView hex, ObjectFormat fmt) {
	Oid o;
	o.format = fmt;
	auto sz = getOidSize(fmt);
	if (hex.size() >= sz * 2) {
		sprt::base16::decode(hex.data(), sz * 2, o.bytes, sz);
	}
	return o;
}

bool Oid::empty() const {
	auto sz = size();
	for (size_t i = 0; i < sz; ++i) {
		if (bytes[i] != 0) {
			return false;
		}
	}
	return true;
}

void Oid::toHex(const Callback<void(StringView)> &cb) const {
	char buf[64];
	auto n = sprt::base16::encode(bytes, size(), buf, sizeof(buf), false);
	cb(StringView(buf, n));
}

String Oid::str() const {
	String ret;
	toHex([&](StringView v) { ret = v.str<memory::StandartInterface>(); });
	return ret;
}

// --- PktLineReader ---------------------------------------------------------

PktLineReader::PktLineReader(BytesView data)
: _ptr(reinterpret_cast<const char *>(data.data())), _size(data.size()) { }

bool PktLineReader::next(PktLine &out) {
	if (_size < 4) {
		return false;
	}

	int len = parsePktLength(_ptr);
	if (len < 0) {
		return false;
	}

	if (len == 0) {
		out.type = PktType::Flush;
		out.data = StringView();
		_ptr += 4;
		_size -= 4;
		return true;
	}
	if (len == 1) {
		out.type = PktType::Delim;
		out.data = StringView();
		_ptr += 4;
		_size -= 4;
		return true;
	}
	if (len == 2) {
		out.type = PktType::ResponseEnd;
		out.data = StringView();
		_ptr += 4;
		_size -= 4;
		return true;
	}
	if (len < 4 || size_t(len) > _size) {
		return false; // malformed (length 3) or truncated frame
	}

	out.type = PktType::Data;
	// NUL-safe: pkt-line payloads (packfile sideband) are binary and may contain
	// embedded NULs, so bypass the strlen-truncating pointer constructor.
	out.data.set(_ptr + 4, size_t(len) - 4);
	_ptr += len;
	_size -= len;
	return true;
}

// --- PktLineWriter ---------------------------------------------------------

void PktLineWriter::writeLine(StringView s) {
	writePktLength(_buf, uint32_t(s.size() + 4));
	_buf.insert(_buf.end(), reinterpret_cast<const uint8_t *>(s.data()),
			reinterpret_cast<const uint8_t *>(s.data()) + s.size());
}

void PktLineWriter::writeFlush() {
	static const char f[4] = {'0', '0', '0', '0'};
	_buf.insert(_buf.end(), reinterpret_cast<const uint8_t *>(f),
			reinterpret_cast<const uint8_t *>(f) + 4);
}

void PktLineWriter::writeDelim() {
	static const char d[4] = {'0', '0', '0', '1'};
	_buf.insert(_buf.end(), reinterpret_cast<const uint8_t *>(d),
			reinterpret_cast<const uint8_t *>(d) + 4);
}

// --- ServiceAdvertisement --------------------------------------------------

bool ServiceAdvertisement::hasCapability(StringView name) const {
	for (auto &c : capabilities) {
		StringView cv(c);
		if (cv == name) {
			return true;
		}
		// capabilities may carry a value: `fetch=shallow filter`
		if (cv.starts_with(name) && cv.size() > name.size() && cv.data()[name.size()] == '=') {
			return true;
		}
	}
	return false;
}

ServiceAdvertisement parseServiceAdvertisement(BytesView data) {
	ServiceAdvertisement adv;

	PktLineReader reader(data);
	PktLine line;
	while (reader.next(line)) {
		if (line.type != PktType::Data) {
			continue;
		}

		StringView s = trimEol(line.data);
		if (s.empty()) {
			continue;
		}

		if (s.starts_with("# service=")) {
			continue; // the service header, ignore
		}

		if (s.starts_with("version ")) {
			StringView v(s.data() + 8, s.size() - 8);
			auto res = v.readInteger(10);
			if (res.valid()) {
				adv.version = uint32_t(res.get(0));
			}
			continue;
		}

		adv.capabilities.emplace_back(s.str<memory::StandartInterface>());

		if (s.starts_with("object-format=")) {
			StringView v(s.data() + 14, s.size() - 14);
			if (v == "sha256") {
				adv.format = ObjectFormat::Sha256;
			} else {
				adv.format = ObjectFormat::Sha1;
			}
		}
	}

	if (adv.version < 2) {
		adv.status = Status::ErrorNotSupported;
	}

	return adv;
}

// --- ls-refs ---------------------------------------------------------------

Bytes buildLsRefsRequest(ObjectFormat fmt, bool sendObjectFormat) {
	PktLineWriter w;
	w.writeLine("command=ls-refs\n");
	if (sendObjectFormat) {
		w.writeLine(
				fmt == ObjectFormat::Sha256 ? "object-format=sha256\n" : "object-format=sha1\n");
	}
	w.writeDelim();
	w.writeLine("peel\n");
	w.writeLine("symrefs\n");
	w.writeLine("ref-prefix HEAD\n");
	w.writeLine("ref-prefix refs/heads/\n");
	w.writeLine("ref-prefix refs/tags/\n");
	w.writeFlush();
	return w.takeData();
}

Status parseLsRefsResponse(BytesView data, ObjectFormat fmt, Vector<RefInfo> &out) {
	PktLineReader reader(data);
	PktLine line;
	while (reader.next(line)) {
		if (line.type != PktType::Data) {
			continue;
		}

		StringView s = trimEol(line.data);
		if (s.empty()) {
			continue;
		}

		// Format: "<oid> <refname>[ symref-target:<ref>][ peeled:<oid>]"
		StringView oidTok = nextToken(s);
		StringView nameTok = nextToken(s);
		if (oidTok.empty() || nameTok.empty()) {
			continue;
		}

		RefInfo ref;
		ref.oid = Oid::fromHex(oidTok, fmt);
		ref.name = nameTok.str<memory::StandartInterface>();

		while (!s.empty()) {
			StringView attr = nextToken(s);
			if (attr.starts_with("symref-target:")) {
				StringView v(attr.data() + 14, attr.size() - 14);
				ref.symref = v.str<memory::StandartInterface>();
			} else if (attr.starts_with("peeled:")) {
				StringView v(attr.data() + 7, attr.size() - 7);
				ref.peeled = Oid::fromHex(v, fmt);
			}
		}

		out.emplace_back(sp::move(ref));
	}

	return Status::Ok;
}

// --- fetch (v2) ---

static void appendDecimal(String &s, uint32_t value) {
	char tmp[16];
	size_t nn = 0;
	if (value == 0) {
		tmp[nn++] = '0';
	} else {
		while (value > 0) {
			tmp[nn++] = char('0' + (value % 10));
			value /= 10;
		}
	}
	for (size_t k = 0; k < nn; ++k) { s.push_back(tmp[nn - 1 - k]); }
}

Bytes buildFetchRequest(const Oid &oid, uint32_t depth, ObjectFormat fmt, bool sendObjectFormat) {
	PktLineWriter w;
	w.writeLine("command=fetch\n");
	if (sendObjectFormat) {
		w.writeLine(
				fmt == ObjectFormat::Sha256 ? "object-format=sha256\n" : "object-format=sha1\n");
	}
	w.writeDelim();
	w.writeLine("no-progress\n");
	w.writeLine("ofs-delta\n");
	if (depth > 0) {
		String line("deepen ");
		appendDecimal(line, depth);
		line.push_back('\n');
		w.writeLine(StringView(line));
	}
	String want("want ");
	oid.toHex([&](StringView h) { want.append(h.data(), h.size()); });
	want.push_back('\n');
	w.writeLine(StringView(want));
	w.writeLine("done\n");
	w.writeFlush();
	return w.takeData();
}

Status parseFetchResponse(BytesView data, ObjectFormat fmt, Bytes &packOut,
		Vector<Oid> &shallowOut) {
	enum class Section {
		None,
		ShallowInfo,
		Packfile,
		Other
	};
	Section sec = Section::None;

	PktLineReader reader(data);
	PktLine line;
	while (reader.next(line)) {
		if (line.type == PktType::Flush) {
			sec = Section::None; // end of stream or section
			continue;
		}
		if (line.type == PktType::Delim) {
			sec = Section::None; // section boundary; next data line is a header
			continue;
		}
		if (line.type != PktType::Data) {
			continue;
		}

		if (sec == Section::None) {
			StringView h = trimEol(line.data);
			if (h == "shallow-info") {
				sec = Section::ShallowInfo;
			} else if (h == "packfile") {
				sec = Section::Packfile;
			} else {
				sec = Section::Other; // acknowledgments / wanted-refs / packfile-uris / unknown
			}
			continue;
		}

		if (sec == Section::ShallowInfo) {
			StringView s = trimEol(line.data);
			if (s.starts_with("shallow ")) {
				shallowOut.emplace_back(Oid::fromHex(StringView(s.data() + 8, s.size() - 8), fmt));
			}
			continue;
		}

		if (sec == Section::Packfile) {
			if (line.data.empty()) {
				continue;
			}
			uint8_t channel = uint8_t(line.data.data()[0]);
			const uint8_t *payload = reinterpret_cast<const uint8_t *>(line.data.data() + 1);
			size_t payloadSize = line.data.size() - 1;
			if (channel == 1) { // pack data
				packOut.insert(packOut.end(), payload, payload + payloadSize);
			} else if (channel == 3) { // fatal error
				return Status::ErrorNotRecoverable;
			}
			// channel 2 (progress) ignored
			continue;
		}
		// Section::Other content ignored
	}

	if (packOut.empty()) {
		return Status::ErrorNotFound;
	}
	return Status::Ok;
}

} // namespace stappler::git
