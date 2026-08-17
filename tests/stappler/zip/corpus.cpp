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

// The ZIP corpus: every archive the stage-0 test bench reads is assembled here, field by field,
// from the format's own structures. This is a writer only in the sense that a test fixture is one -
// it exists so the reader has something with known content to be measured against, and so that a
// case like "sizes live in the data descriptor, not the local header" can be expressed at all.
//
// Layout of every archive produced here:
//   [local header + name + extra + payload (+ data descriptor)] * N
//   [central directory header + name + extra] * N
//   [zip64 EOCD + zip64 EOCD locator]?
//   [EOCD + comment]

#include "SPCommon.h"

#include "corpus.h"

#include <zlib.h>

namespace STAPPLER_VERSIONIZED stappler::test::zip {

// Signatures, little-endian on the wire.
static constexpr uint32_t SIG_LOCAL = 0x04034b50;
static constexpr uint32_t SIG_DESCRIPTOR = 0x08074b50;
static constexpr uint32_t SIG_CENTRAL = 0x02014b50;
static constexpr uint32_t SIG_EOCD64 = 0x06064b50;
static constexpr uint32_t SIG_LOCATOR64 = 0x07064b50;
static constexpr uint32_t SIG_EOCD = 0x06054b50;

// General purpose bit flags used by the corpus.
static constexpr uint16_t FLAG_ENCRYPTED = 1 << 0;
static constexpr uint16_t FLAG_DESCRIPTOR = 1 << 3;
static constexpr uint16_t FLAG_UTF8 = 1 << 11;

// 2024-01-01 00:00:00 in DOS packed form; fixed so archives are byte-reproducible.
static constexpr uint16_t DOS_TIME = 0;
static constexpr uint16_t DOS_DATE = uint16_t(((2024 - 1980) << 9) | (1 << 5) | 1);

static constexpr uint32_t ZIP64_MARK = 0xFFFFFFFF;

Bytes deflateRaw(BytesView in) {
	z_stream zs = {};
	// negative window bits: a bare deflate stream, no zlib header or trailer - which is what a
	// method-8 ZIP entry stores
	if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY)
			!= Z_OK) {
		return Bytes();
	}

	Bytes out;
	out.resize(size_t(deflateBound(&zs, uLong(in.size()))) + 16);

	zs.next_in = (Bytef *)in.data();
	zs.avail_in = uInt(in.size());
	zs.next_out = out.data();
	zs.avail_out = uInt(out.size());

	auto ret = deflate(&zs, Z_FINISH);
	auto produced = out.size() - zs.avail_out;
	deflateEnd(&zs);

	if (ret != Z_STREAM_END) {
		return Bytes();
	}
	out.resize(produced);
	return out;
}

static uint32_t crcOf(BytesView v) {
	return uint32_t(::crc32(::crc32(0, nullptr, 0), v.data(), uInt(v.size())));
}

// Per-entry knobs. Each one exists to express exactly one corpus case; defaults give a plain
// stored entry with honest sizes.
struct EntryOptions {
	uint16_t method = 0; // 0 = Store, 8 = Deflate
	uint16_t flags = 0;

	Bytes localExtra;
	Bytes centralExtra;

	// bit 3: local header carries zeroes, real values follow the payload
	bool dataDescriptor = false;

	// write 0xFFFFFFFF sentinels in place of the 32-bit sizes/offset
	bool zip64Sizes = false;

	// declare an uncompressed size unrelated to the payload (the zip-bomb case)
	bool declaredSizeUsed = false;
	uint64_t declaredSize = 0;
};

struct Builder {
	Bytes out;

	struct Central {
		String name;
		Bytes extra;
		uint16_t method = 0;
		uint16_t flags = 0;
		uint32_t crc = 0;
		uint64_t compSize = 0;
		uint64_t rawSize = 0;
		uint64_t localOffset = 0;
		bool zip64Sizes = false;
	};

	Vector<Central> central;

	void u8(uint8_t v) { out.emplace_back(v); }

	void u16(uint16_t v) {
		u8(uint8_t(v & 0xFF));
		u8(uint8_t((v >> 8) & 0xFF));
	}

	void u32(uint32_t v) {
		u16(uint16_t(v & 0xFFFF));
		u16(uint16_t((v >> 16) & 0xFFFF));
	}

	void u64(uint64_t v) {
		u32(uint32_t(v & 0xFFFFFFFF));
		u32(uint32_t((v >> 32) & 0xFFFFFFFF));
	}

	void raw(const uint8_t *d, size_t n) {
		for (size_t i = 0; i < n; ++i) { u8(d[i]); }
	}

	void raw(BytesView v) { raw(v.data(), v.size()); }
	void raw(StringView v) { raw((const uint8_t *)v.data(), v.size()); }

	void add(StringView name, BytesView content, const EntryOptions &opts = EntryOptions()) {
		Central c;
		c.localOffset = out.size();
		c.name = name.str<memory::StandardInterface>();
		c.extra = opts.centralExtra;
		c.method = opts.method;
		c.flags = opts.flags;
		c.crc = crcOf(content);
		c.rawSize = opts.declaredSizeUsed ? opts.declaredSize : content.size();
		c.zip64Sizes = opts.zip64Sizes;

		Bytes payload;
		if (opts.method == 8) {
			payload = deflateRaw(content);
		} else {
			payload.assign(content.data(), content.data() + content.size());
		}
		c.compSize = payload.size();

		u32(SIG_LOCAL);
		u16(opts.zip64Sizes ? 45 : 20); // version needed to extract
		u16(opts.flags);
		u16(opts.method);
		u16(DOS_TIME);
		u16(DOS_DATE);

		if (opts.dataDescriptor) {
			// the whole point of bit 3: the writer did not know these yet
			u32(0);
			u32(0);
			u32(0);
		} else if (opts.zip64Sizes) {
			u32(c.crc);
			u32(ZIP64_MARK);
			u32(ZIP64_MARK);
		} else {
			u32(c.crc);
			u32(uint32_t(c.compSize));
			u32(uint32_t(c.rawSize));
		}

		u16(uint16_t(name.size()));
		u16(uint16_t(opts.localExtra.size()));
		raw(name);
		raw(BytesView(opts.localExtra.data(), opts.localExtra.size()));
		raw(BytesView(payload.data(), payload.size()));

		if (opts.dataDescriptor) {
			u32(SIG_DESCRIPTOR);
			u32(c.crc);
			u32(uint32_t(c.compSize));
			u32(uint32_t(c.rawSize));
		}

		central.emplace_back(sprt::move(c));
	}

	// Closes the archive. `zip64` adds the ZIP64 EOCD record and its locator ahead of the ordinary
	// EOCD, with the 32-bit EOCD fields left as sentinels.
	Bytes finish(StringView comment = StringView(), bool zip64 = false) {
		auto cdOffset = out.size();

		for (auto &c : central) {
			u32(SIG_CENTRAL);
			u16(c.zip64Sizes ? 45 : 20); // version made by
			u16(c.zip64Sizes ? 45 : 20); // version needed
			u16(c.flags);
			u16(c.method);
			u16(DOS_TIME);
			u16(DOS_DATE);
			u32(c.crc);
			if (c.zip64Sizes) {
				u32(ZIP64_MARK);
				u32(ZIP64_MARK);
			} else {
				u32(uint32_t(c.compSize));
				u32(uint32_t(c.rawSize));
			}
			u16(uint16_t(c.name.size()));
			u16(uint16_t(c.extra.size()));
			u16(0); // comment length
			u16(0); // disk number start
			u16(0); // internal attributes
			u32(0); // external attributes
			u32(uint32_t(c.localOffset));
			raw(StringView(c.name.data(), c.name.size()));
			raw(BytesView(c.extra.data(), c.extra.size()));
		}

		auto cdSize = out.size() - cdOffset;

		if (zip64) {
			auto eocd64Offset = out.size();

			u32(SIG_EOCD64);
			u64(44); // size of the remainder of this record
			u16(45); // version made by
			u16(45); // version needed
			u32(0); // this disk
			u32(0); // disk with central directory
			u64(central.size()); // entries on this disk
			u64(central.size()); // entries total
			u64(cdSize);
			u64(cdOffset);

			u32(SIG_LOCATOR64);
			u32(0); // disk with the zip64 EOCD
			u64(eocd64Offset);
			u32(1); // total disks
		}

		u32(SIG_EOCD);
		u16(0); // this disk
		u16(0); // disk with central directory
		u16(uint16_t(central.size()));
		u16(uint16_t(central.size()));
		u32(zip64 ? ZIP64_MARK : uint32_t(cdSize));
		u32(zip64 ? ZIP64_MARK : uint32_t(cdOffset));
		u16(uint16_t(comment.size()));
		raw(comment);

		return sprt::move(out);
	}
};

static Bytes bytesOf(StringView s) {
	Bytes b;
	b.assign((const uint8_t *)s.data(), (const uint8_t *)s.data() + s.size());
	return b;
}

static BytesView viewOf(StringView s) { return BytesView((const uint8_t *)s.data(), s.size()); }

// The zip64 extended information extra field (0x0001): uncompressed then compressed size, both
// 64-bit, in the order the sentinels appeared.
static Bytes zip64Extra(uint64_t rawSize, uint64_t compSize) {
	Builder b;
	b.u16(0x0001);
	b.u16(16);
	b.u64(rawSize);
	b.u64(compSize);
	return sprt::move(b.out);
}

// Info-ZIP Unicode Path extra field (0x7075): version, CRC of the name in the header, then the
// UTF-8 spelling of the name.
static Bytes unicodePathExtra(StringView headerName, StringView utf8Name) {
	Builder b;
	b.u16(0x7075);
	b.u16(uint16_t(5 + utf8Name.size()));
	b.u8(1); // version
	b.u32(crcOf(viewOf(headerName)));
	b.raw(utf8Name);
	return sprt::move(b.out);
}

static Case makeStoreSingle() {
	Case c;
	c.name = "store-single";

	Builder b;
	b.add("hello.txt", viewOf("hello, stored world"));
	c.archive = b.finish();

	c.entries.emplace_back(EntryExpect{"hello.txt", bytesOf("hello, stored world"), true});
	return c;
}

static Case makeDeflateSingle() {
	Case c;
	c.name = "deflate-single";

	// long enough that deflate actually compresses it, so the stored/deflated paths differ
	String payload;
	for (int i = 0; i < 64; ++i) { payload.append("the quick brown fox jumps over the lazy dog\n"); }

	EntryOptions opts;
	opts.method = 8;

	Builder b;
	b.add("hello.txt", viewOf(StringView(payload.data(), payload.size())), opts);
	c.archive = b.finish();

	c.entries.emplace_back(
			EntryExpect{"hello.txt", bytesOf(StringView(payload.data(), payload.size())), true});
	return c;
}

static Case makeMulti() {
	Case c;
	c.name = "multi";

	EntryOptions deflated;
	deflated.method = 8;

	// Binary payloads must NOT travel through StringView: its (ptr, len) constructor runs the length
	// through detail::length(), which clamps at the first NUL, so a view over these four bytes would
	// come out empty. BytesView carries them verbatim.
	static const uint8_t binaryBytes[] = {0x00, 0x01, 0x02, 0x03};
	BytesView binary(binaryBytes, sizeof(binaryBytes));

	Builder b;
	b.add("first.txt", viewOf("first"));
	b.add("a/b/c.txt", viewOf("nested payload, deflated"), deflated);
	b.add("last.bin", binary);
	c.archive = b.finish();

	Bytes binaryExpect;
	binaryExpect.assign(binaryBytes, binaryBytes + sizeof(binaryBytes));

	c.entries.emplace_back(EntryExpect{"first.txt", bytesOf("first"), true});
	c.entries.emplace_back(EntryExpect{"a/b/c.txt", bytesOf("nested payload, deflated"), true});
	c.entries.emplace_back(EntryExpect{"last.bin", binaryExpect, true});
	return c;
}

static Case makeDirEntry() {
	Case c;
	c.name = "dir-entry";

	Builder b;
	b.add("dir/", BytesView()); // trailing slash, zero length: the conventional directory entry
	b.add("dir/file.txt", viewOf("inside"));
	c.archive = b.finish();

	// a directory entry is listed but has nothing to read
	c.entries.emplace_back(EntryExpect{"dir/", Bytes(), false});
	c.entries.emplace_back(EntryExpect{"dir/file.txt", bytesOf("inside"), true});
	return c;
}

static Case makeUtf8Name() {
	Case c;
	c.name = "utf8-name";

	EntryOptions opts;
	opts.flags = FLAG_UTF8;

	Builder b;
	b.add("привет.txt", viewOf("utf-8 named"), opts);
	c.archive = b.finish();

	c.entries.emplace_back(EntryExpect{"привет.txt", bytesOf("utf-8 named"), true});
	return c;
}

static Case makeCp437Name() {
	Case c;
	c.name = "cp437-name";

	// 0x81 0x94 0x81 = ü ö ü in CP437
	Builder b;
	b.add(StringView("\x81\x94\x81.txt", 7), viewOf("cp437 named"));
	c.archive = b.finish();

	// MEASURED against libzip: a name without bit 11 is decoded as CP437 and handed back as UTF-8,
	// so 0x81/0x94 arrive as U+00FC/U+00F6. The from-scratch reader has to carry the same table.
	c.entries.emplace_back(EntryExpect{"üöü.txt", bytesOf("cp437 named"), true});
	return c;
}

static Case makeUnicodePathExtra() {
	Case c;
	c.name = "unicode-path-extra";

	StringView headerName("legacy.txt");
	auto extra = unicodePathExtra(headerName, "настоящее.txt");

	EntryOptions opts;
	opts.localExtra = extra;
	opts.centralExtra = extra;

	Builder b;
	b.add(headerName, viewOf("unicode path extra"), opts);
	c.archive = b.finish();

	// MEASURED against libzip: the 0x7075 field WINS over the header name, so the entry is known by
	// its UTF-8 spelling and not as "legacy.txt".
	c.entries.emplace_back(EntryExpect{"настоящее.txt", bytesOf("unicode path extra"), true});
	return c;
}

static Case makeDataDescriptor() {
	Case c;
	c.name = "data-descriptor";

	EntryOptions opts;
	opts.flags = FLAG_DESCRIPTOR;
	opts.dataDescriptor = true;
	opts.method = 8;

	Builder b;
	b.add("streamed.txt", viewOf("written without knowing the size up front"), opts);
	c.archive = b.finish();

	c.entries.emplace_back(EntryExpect{"streamed.txt",
		bytesOf("written without knowing the size up front"), true});
	return c;
}

static Case makeZip64() {
	Case c;
	c.name = "zip64";

	// A small archive that nonetheless speaks ZIP64: sentinels in the 32-bit fields, real values in
	// the 0x0001 extra field. Size is not what makes an archive ZIP64 - the encoding is.
	StringView payload("zip64 encoded, small anyway");

	EntryOptions opts;
	opts.zip64Sizes = true;
	opts.localExtra = zip64Extra(payload.size(), payload.size());
	opts.centralExtra = opts.localExtra;

	Builder b;
	b.add("big.txt", viewOf(payload), opts);
	c.archive = b.finish(StringView(), true);

	c.entries.emplace_back(EntryExpect{"big.txt", bytesOf(payload), true});
	return c;
}

static Case makeEmpty() {
	Case c;
	c.name = "empty";

	Builder b;
	c.archive = b.finish();
	return c; // no entries, but must open
}

static Case makeComment() {
	Case c;
	c.name = "comment";

	Builder b;
	b.add("hello.txt", viewOf("archive has a trailing comment"));
	c.archive = b.finish("a comment long enough to push the EOCD away from the very end");

	c.entries.emplace_back(EntryExpect{"hello.txt", bytesOf("archive has a trailing comment"), true});
	return c;
}

static Case makeBadEocd() {
	Case c;
	c.name = "bad-eocd";
	// MEASURED against libzip: refused at open, not at read
	c.openable = false;

	Builder b;
	b.add("hello.txt", viewOf("the end record is about to be corrupted"));
	auto data = b.finish();

	// break the EOCD signature: the last 22 bytes are the record, its first four are the signature
	if (data.size() >= 22) {
		data[data.size() - 22] = 0x00;
	}
	c.archive = sprt::move(data);
	return c;
}

static Case makeTruncated() {
	Case c;
	c.name = "truncated";
	// MEASURED against libzip: cutting the file removes the central directory, so open fails
	c.openable = false;

	Builder b;
	b.add("hello.txt", viewOf("this archive gets cut in half"));
	auto data = b.finish();
	data.resize(data.size() / 2);

	c.archive = sprt::move(data);
	return c;
}

static Case makeBomb() {
	Case c;
	c.name = "bomb";

	// tiny payload, enormous declared uncompressed size: the ratio the guard in SPZip.cpp rejects
	EntryOptions opts;
	opts.method = 8;
	opts.declaredSizeUsed = true;
	opts.declaredSize = uint64_t(1) << 32;

	String payload;
	payload.resize(4096, 'A');

	Builder b;
	b.add("bomb.txt", viewOf(StringView(payload.data(), payload.size())), opts);
	c.archive = b.finish();

	// listed, but reading it must be refused
	c.entries.emplace_back(EntryExpect{"bomb.txt", Bytes(), false});
	return c;
}

static Case makeTraversal() {
	Case c;
	c.name = "traversal";

	Builder b;
	b.add("../escape.txt", viewOf("one level up"));
	b.add("/absolute.txt", viewOf("rooted"));
	b.add("ok.txt", viewOf("harmless"));
	c.archive = b.finish();

	// MEASURED against libzip: it does NOT sanitize - both hostile names come back verbatim and are
	// readable. This is the one expectation the replacement is meant to BREAK: stage 3 adds a path
	// sanitizer, and when it lands the first two entries must stop being reachable. Until then this
	// records the hole rather than pretending it is not there.
	c.entries.emplace_back(EntryExpect{"../escape.txt", bytesOf("one level up"), true});
	c.entries.emplace_back(EntryExpect{"/absolute.txt", bytesOf("rooted"), true});
	c.entries.emplace_back(EntryExpect{"ok.txt", bytesOf("harmless"), true});
	return c;
}

static Case makeEncrypted() {
	Case c;
	c.name = "encrypted";

	EntryOptions opts;
	opts.flags = FLAG_ENCRYPTED;

	Builder b;
	b.add("secret.txt", viewOf("not actually encrypted, but flagged as such"), opts);
	c.archive = b.finish();

	// MEASURED against libzip: the archive opens and the entry is listed with its true size; only
	// the read is refused, because no password was supplied. The replacement must refuse it too -
	// with an explicit "encrypted" rejection rather than by handing back the raw ciphertext.
	c.entries.emplace_back(EntryExpect{"secret.txt", Bytes(), false});
	return c;
}

static Case makeEpubMin() {
	Case c;
	c.name = "epub-min";

	StringView mimetype("application/epub+zip");
	StringView container(
			"<?xml version=\"1.0\"?>\n<container version=\"1.0\" "
			"xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
			"<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
			"media-type=\"application/oebps-package+xml\"/></rootfiles></container>\n");
	StringView opf(
			"<?xml version=\"1.0\"?>\n<package xmlns=\"http://www.idpf.org/2007/opf\" "
			"version=\"3.0\" unique-identifier=\"id\">\n"
			"<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
			"<dc:identifier id=\"id\">urn:uuid:test</dc:identifier>"
			"<dc:title>Minimal</dc:title><dc:language>en</dc:language></metadata>\n"
			"<manifest><item id=\"c1\" href=\"ch1.xhtml\" "
			"media-type=\"application/xhtml+xml\"/></manifest>\n"
			"<spine><itemref idref=\"c1\"/></spine>\n</package>\n");
	StringView chapter(
			"<?xml version=\"1.0\"?>\n<html xmlns=\"http://www.w3.org/1999/xhtml\">"
			"<head><title>Chapter</title></head><body><p>Hello.</p></body></html>\n");

	EntryOptions deflated;
	deflated.method = 8;

	Builder b;
	// OCF requires `mimetype` first and stored uncompressed - that is what makes the format
	// sniffable from the first bytes of the file
	b.add("mimetype", viewOf(mimetype));
	b.add("META-INF/container.xml", viewOf(container), deflated);
	b.add("OEBPS/content.opf", viewOf(opf), deflated);
	b.add("OEBPS/ch1.xhtml", viewOf(chapter), deflated);
	c.archive = b.finish();

	c.entries.emplace_back(EntryExpect{"mimetype", bytesOf(mimetype), true});
	c.entries.emplace_back(EntryExpect{"META-INF/container.xml", bytesOf(container), true});
	c.entries.emplace_back(EntryExpect{"OEBPS/content.opf", bytesOf(opf), true});
	c.entries.emplace_back(EntryExpect{"OEBPS/ch1.xhtml", bytesOf(chapter), true});
	return c;
}

Vector<Case> buildCorpus() {
	Vector<Case> ret;
	ret.emplace_back(makeStoreSingle());
	ret.emplace_back(makeDeflateSingle());
	ret.emplace_back(makeMulti());
	ret.emplace_back(makeDirEntry());
	ret.emplace_back(makeUtf8Name());
	ret.emplace_back(makeCp437Name());
	ret.emplace_back(makeUnicodePathExtra());
	ret.emplace_back(makeDataDescriptor());
	ret.emplace_back(makeZip64());
	ret.emplace_back(makeEmpty());
	ret.emplace_back(makeComment());
	ret.emplace_back(makeBadEocd());
	ret.emplace_back(makeTruncated());
	ret.emplace_back(makeBomb());
	ret.emplace_back(makeTraversal());
	ret.emplace_back(makeEncrypted());
	ret.emplace_back(makeEpubMin());
	return ret;
}

} // namespace stappler::test::zip
