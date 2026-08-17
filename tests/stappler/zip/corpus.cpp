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

	// write a CRC that does not belong to the payload
	bool crcOverrideUsed = false;
	uint32_t crcOverride = 0;

	// declare a compressed size unrelated to the payload (the truncated-data case)
	bool compSizeOverrideUsed = false;
	uint64_t compSizeOverride = 0;
};

struct Builder {
	Bytes out;

	struct Central {
		String name;
		Bytes extra;
		Bytes content; // what add() was handed, so the reader tests have something to compare to
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

	// The name is taken as BYTES, not as a StringView: StringView's (pointer, length) constructor
	// stops at the first NUL, so a name carrying one could not be written at all through it - and a
	// name carrying one is exactly what the sanitizer has to be tested against.
	void add(BytesView name, BytesView content, const EntryOptions &opts = EntryOptions()) {
		Central c;
		c.localOffset = out.size();
		c.name.assign((const char *)name.data(), name.size());
		c.extra = opts.centralExtra;
		c.content.assign(content.data(), content.data() + content.size());
		c.method = opts.method;
		c.flags = opts.flags;
		c.crc = opts.crcOverrideUsed ? opts.crcOverride : crcOf(content);
		c.rawSize = opts.declaredSizeUsed ? opts.declaredSize : content.size();
		c.zip64Sizes = opts.zip64Sizes;

		Bytes payload;
		if (opts.method == 8) {
			payload = deflateRaw(content);
		} else {
			payload.assign(content.data(), content.data() + content.size());
		}
		c.compSize = opts.compSizeOverrideUsed ? opts.compSizeOverride : payload.size();

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

	// Convenience for the overwhelming majority of names, which are ordinary text.
	void add(StringView name, BytesView content, const EntryOptions &opts = EntryOptions()) {
		add(BytesView((const uint8_t *)name.data(), name.size()), content, opts);
	}

	// What was written, in central-directory order. Valid from the moment the entries are added -
	// `finish()` moves the byte buffer out, but leaves this behind untouched.
	Vector<EntryMeta> metadata() const {
		Vector<EntryMeta> ret;
		for (auto &c : central) {
			EntryMeta m;
			m.rawName.assign((const uint8_t *)c.name.data(),
					(const uint8_t *)c.name.data() + c.name.size());

			// Right for every name that needs no decoding; the cases that do override it.
			m.decodedName = m.rawName;

			m.expectDirectory = !c.name.empty() && c.name.back() == '/';
			m.expectEncrypted = (c.flags & FLAG_ENCRYPTED) != 0;
			m.expectUnsupportedMethod = (c.method != 0 && c.method != 8);

			m.content = c.content;

			// The default read outcome, in the same precedence the reader applies. NameRejected is
			// not visible here - it depends on the decoded name - so a case that expects one uses
			// markNameRejected() below, which sets the flag and this status together.
			if (m.expectEncrypted) {
				m.expectRead = Status::ErrorNotImplemented;
			} else if (m.expectUnsupportedMethod) {
				m.expectRead = Status::ErrorNotSupported;
			} else if (m.expectDirectory) {
				m.expectRead = Status::Declined;
			} else {
				m.expectRead = Status::Ok;
			}

			m.method = c.method;
			m.flags = c.flags;
			m.crc32 = c.crc;

			// What lands in the file, not what was asked for: outside ZIP64 these are 32-bit
			// fields, so a value that does not fit is truncated on the way out and a reader can
			// only ever hand back the truncated one.
			m.compressedSize = c.zip64Sizes ? c.compSize : uint32_t(c.compSize);
			m.uncompressedSize = c.zip64Sizes ? c.rawSize : uint32_t(c.rawSize);
			m.localOffset = uint32_t(c.localOffset);
			ret.emplace_back(sprt::move(m));
		}
		return ret;
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
			// As BYTES: routing the name through StringView here would truncate it at the first NUL
			// while the length field above still claims the full size, and every later record in
			// the directory would then be read from the wrong offset.
			raw(BytesView((const uint8_t *)c.name.data(), c.name.size()));
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

// Marks an entry as one the sanitizer refuses. Sets the flag and the read outcome together, so the
// two cannot drift apart.
static void markNameRejected(EntryMeta &m) {
	m.expectNameRejected = true;
	m.expectRead = Status::ErrorNotPermitted;
}

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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

	// MEASURED against libzip: a name without bit 11 is decoded as CP437 and handed back as UTF-8,
	// so 0x81/0x94 arrive as U+00FC/U+00F6. The from-scratch reader has to carry the same table.
	c.entries.emplace_back(EntryExpect{"üöü.txt", bytesOf("cp437 named"), true});
	c.meta[0].decodedName = bytesOf("üöü.txt");
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
	c.meta = b.metadata();

	// MEASURED against libzip: the 0x7075 field WINS over the header name, so the entry is known by
	// its UTF-8 spelling and not as "legacy.txt".
	c.entries.emplace_back(EntryExpect{"настоящее.txt", bytesOf("unicode path extra"), true});
	c.meta[0].decodedName = bytesOf("настоящее.txt");
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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"big.txt", bytesOf(payload), true});
	return c;
}

static Case makeEmpty() {
	Case c;
	c.name = "empty";

	Builder b;
	c.archive = b.finish();
	c.meta = b.metadata();
	return c; // no entries, but must open
}

static Case makeComment() {
	Case c;
	c.name = "comment";

	Builder b;
	b.add("hello.txt", viewOf("archive has a trailing comment"));
	c.archive = b.finish("a comment long enough to push the EOCD away from the very end");
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"hello.txt", bytesOf("archive has a trailing comment"), true});
	return c;
}

static Case makeBadEocd() {
	Case c;
	c.name = "bad-eocd";
	// MEASURED against libzip: refused at open, not at read
	c.openable = false;
	c.parsable = false;

	Builder b;
	b.add("hello.txt", viewOf("the end record is about to be corrupted"));
	auto data = b.finish();
	c.meta = b.metadata();

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
	c.parsable = false;

	Builder b;
	b.add("hello.txt", viewOf("this archive gets cut in half"));
	auto data = b.finish();
	c.meta = b.metadata();
	data.resize(data.size() / 2);

	c.archive = sprt::move(data);
	return c;
}

static Case makeBomb() {
	Case c;
	c.name = "bomb";

	// Tiny payload, enormous declared uncompressed size: the ratio the guard in SPZip.cc rejects.
	//
	// The declared size has to FIT IN 32 BITS. It used to be 1<<32, which the builder writes into a
	// u32 field as plain zero - so the archive declared nothing at all, and libzip refused it on the
	// `stat.size == 0` early-out rather than on the bomb ratio. The case passed while testing
	// something else entirely. 512 MiB against a ~4 KiB payload clears both the 16x ratio and the
	// 8 MiB floor, so the guard is what does the rejecting now.
	EntryOptions opts;
	opts.method = 8;
	opts.declaredSizeUsed = true;
	opts.declaredSize = uint64_t(512) << 20;

	String payload;
	payload.resize(4096, 'A');

	Builder b;
	b.add("bomb.txt", viewOf(StringView(payload.data(), payload.size())), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	// listed, but reading it must be refused
	c.entries.emplace_back(EntryExpect{"bomb.txt", Bytes(), false});
	c.meta[0].expectRead = Status::ErrorBufferOverflow;
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
	c.meta = b.metadata();

	// MEASURED against libzip: it does NOT sanitize - both hostile names come back verbatim and are
	// readable. This is the one expectation the replacement is meant to BREAK: stage 3 adds a path
	// sanitizer, and when it lands the first two entries must stop being reachable. Until then this
	// records the hole rather than pretending it is not there.
	c.entries.emplace_back(EntryExpect{"../escape.txt", bytesOf("one level up"), true});
	c.entries.emplace_back(EntryExpect{"/absolute.txt", bytesOf("rooted"), true});
	c.entries.emplace_back(EntryExpect{"ok.txt", bytesOf("harmless"), true});

	// ...and here is the replacement breaking it, as promised. Both hostile names are still LISTED -
	// an archive that carries one is something a caller should be able to see - but they are marked
	// and reading them is refused.
	markNameRejected(c.meta[0]);
	markNameRejected(c.meta[1]);
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
	c.meta = b.metadata();

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
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"mimetype", bytesOf(mimetype), true});
	c.entries.emplace_back(EntryExpect{"META-INF/container.xml", bytesOf(container), true});
	c.entries.emplace_back(EntryExpect{"OEBPS/content.opf", bytesOf(opf), true});
	c.entries.emplace_back(EntryExpect{"OEBPS/ch1.xhtml", bytesOf(chapter), true});
	return c;
}

// -- helpers for the cases that corrupt an already-built archive --
//
// A comment-less EOCD is the last 22 bytes, so its fields sit at known offsets from the end. These
// exist so a case can say "and now claim the directory is somewhere else" in one line.

static void patchU16(Bytes &data, size_t offset, uint16_t value) {
	data[offset] = uint8_t(value & 0xFF);
	data[offset + 1] = uint8_t((value >> 8) & 0xFF);
}

static void patchU32(Bytes &data, size_t offset, uint32_t value) {
	patchU16(data, offset, uint16_t(value & 0xFFFF));
	patchU16(data, offset + 2, uint16_t((value >> 16) & 0xFFFF));
}

static constexpr size_t EOCD_TOTAL_ENTRIES = 10;
static constexpr size_t EOCD_CD_SIZE = 12;
static constexpr size_t EOCD_CD_OFFSET = 16;

static Case makePrefix() {
	Case c;
	c.name = "prefix";

	Builder b;
	b.add("hello.txt", viewOf("preceded by junk"));
	auto archive = b.finish();
	c.meta = b.metadata();

	// A self-extracting archive puts an executable stub in front; an embedded one puts a container
	// there. Either way every offset stored inside the archive is short by the length of that
	// prefix, and a reader that does not correct for it looks for the central directory in the junk.
	StringView junk("MZ this is not really an executable, but it is in the way\n");
	c.expectedPrefix = junk.size();

	// MEASURED: the current ZipArchive refuses this archive, and it is not libzip that refuses it -
	// both constructors sniff the FIRST FOUR BYTES for a local/central/descriptor signature
	// (SPZip.cc:49 and :164) and give up before libzip is ever handed the bytes. The engine's own
	// catalog locates the directory from the end and handles the prefix, hence the divergence.
	// Stage 6 has to decide the offset-0 sniff's fate: keeping it would throw that capability away.
	c.openable = false;

	c.archive.assign((const uint8_t *)junk.data(), (const uint8_t *)junk.data() + junk.size());
	c.archive.insert(c.archive.end(), archive.begin(), archive.end());

	c.entries.emplace_back(EntryExpect{"hello.txt", bytesOf("preceded by junk"), true});
	return c;
}

static Case makeEocdInComment() {
	Case c;
	c.name = "eocd-in-comment";

	// The comment opens with a well-formed EOCD signature. Scanning backwards finds THIS one first,
	// so the only thing that tells it apart from the real record is that its declared comment length
	// does not reach the end of the file. A reader that stops at the first signature reads an
	// archive with zero entries and no error.
	String comment;
	comment.append("PK\x05\x06", 4);
	comment.append(18, '\0');
	comment.append("trailing");

	Builder b;
	b.add("hello.txt", viewOf("the comment lies about being an EOCD"));
	c.archive = b.finish(StringView(comment.data(), comment.size()));
	c.meta = b.metadata();

	c.entries.emplace_back(
			EntryExpect{"hello.txt", bytesOf("the comment lies about being an EOCD"), true});
	return c;
}

static Case makeCdOffsetLies() {
	Case c;
	c.name = "cd-offset-lies";
	c.openable = false;
	c.parsable = false;

	Builder b;
	b.add("hello.txt", viewOf("the directory is not where the record says"));
	auto data = b.finish();

	patchU32(data, data.size() - 22 + EOCD_CD_OFFSET, 0x7000'0000);
	c.archive = sprt::move(data);
	return c;
}

static Case makeCdCountLies() {
	Case c;
	c.name = "cd-count-lies";
	c.openable = false;
	c.parsable = false;

	Builder b;
	b.add("hello.txt", viewOf("one entry, many claimed"));
	auto data = b.finish();

	// More entries than physically fit in the directory: 46 bytes is the smallest a central header
	// can be. Without that check the count alone decides how much gets allocated.
	patchU16(data, data.size() - 22 + EOCD_TOTAL_ENTRIES, 0xFFFE);
	c.archive = sprt::move(data);
	return c;
}

static Case makeZip64NoRecord() {
	Case c;
	c.name = "zip64-no-record";
	c.openable = false;
	c.parsable = false;

	Builder b;
	b.add("hello.txt", viewOf("claims zip64, carries nothing"));
	auto data = b.finish();

	// Sentinels say "the real values are in the ZIP64 record" - and there is no ZIP64 record.
	patchU32(data, data.size() - 22 + EOCD_CD_SIZE, 0xFFFFFFFF);
	patchU32(data, data.size() - 22 + EOCD_CD_OFFSET, 0xFFFFFFFF);
	c.archive = sprt::move(data);
	return c;
}

static Case makeZip64BadLocator() {
	Case c;
	c.name = "zip64-bad-locator";
	c.openable = false;
	c.parsable = false;

	StringView payload("zip64 with a locator pointing nowhere");

	EntryOptions opts;
	opts.zip64Sizes = true;
	opts.localExtra = zip64Extra(payload.size(), payload.size());
	opts.centralExtra = opts.localExtra;

	Builder b;
	b.add("big.txt", viewOf(payload), opts);
	auto data = b.finish(StringView(), true);

	// The locator sits in the 20 bytes before the EOCD; its 64-bit field at +8 is where the ZIP64
	// EOCD is said to be. Point it past the end of the file.
	auto locator = data.size() - 22 - 20;
	patchU32(data, locator + 8, 0x7000'0000);
	patchU32(data, locator + 12, 0);

	c.archive = sprt::move(data);
	return c;
}

static Case makeManyEntries() {
	Case c;
	c.name = "many-entries";

	// Enough entries that offset arithmetic and the lookup index are doing real work, and that a
	// linear-scan mistake in either would be visible.
	Builder b;
	for (int i = 0; i < 300; ++i) {
		String name;
		name.append("dir");
		name.append(1, char('0' + (i / 100) % 10));
		name.append("/file");
		name.append(1, char('0' + (i / 100) % 10));
		name.append(1, char('0' + (i / 10) % 10));
		name.append(1, char('0' + i % 10));
		name.append(".txt");

		auto content = name;
		content.append(" content");

		b.add(StringView(name.data(), name.size()),
				viewOf(StringView(content.data(), content.size())));

		c.entries.emplace_back(EntryExpect{name, bytesOf(StringView(content.data(), content.size())),
			true});
	}
	c.archive = b.finish();
	c.meta = b.metadata();
	return c;
}

// -- stage 3: names --

static Case makeUtf8NameNoFlag() {
	Case c;
	c.name = "utf8-name-no-flag";

	// The stock Linux `zip` writes UTF-8 names and does NOT set bit 11. Reading the flag literally
	// would send these bytes through CP437 and produce mojibake, which is why the encoding is
	// guessed rather than trusted.
	Builder b;
	b.add("документ.txt", viewOf("utf-8 without the flag"));
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"документ.txt", bytesOf("utf-8 without the flag"), true});
	return c;
}

static Case makeUtf8NameTrailing() {
	Case c;
	c.name = "utf8-name-trailing";

	// A name whose LAST character is multi-byte - the boundary case of any UTF-8 validator, where
	// an off-by-one turns a valid name into mojibake. Both readers get it right; the case exists so
	// that a future edit to either cannot break it silently.
	EntryOptions opts;
	opts.flags = FLAG_UTF8;

	Builder b;
	b.add("файл", viewOf("the name ends mid-alphabet"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"файл", bytesOf("the name ends mid-alphabet"), true});
	return c;
}

static Case makeUnicodePathBadCrc() {
	Case c;
	c.name = "unicode-path-bad-crc";

	// The 0x7075 field carries the CRC32 of the header name it replaces. Here it carries somebody
	// else's, which is exactly the shape an attempt to make an entry answer to a different name
	// would take - so the field must be ignored and the header name must stand.
	auto extra = unicodePathExtra("a-completely-different-name", "подменённое.txt");

	EntryOptions opts;
	opts.localExtra = extra;
	opts.centralExtra = extra;

	Builder b;
	b.add("legacy.txt", viewOf("the extra field does not match"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"legacy.txt", bytesOf("the extra field does not match"),
		true});
	return c;
}

static Case makeUnicodePathBadVersion() {
	Case c;
	c.name = "unicode-path-bad-version";

	StringView headerName("legacy.txt");

	// Same field, correct CRC, version byte 2 - a version this reader does not know how to read.
	Builder v;
	v.u16(0x7075);
	v.u16(uint16_t(5 + StringView("будущее.txt").size()));
	v.u8(2);
	v.u32(crcOf(viewOf(headerName)));
	v.raw(StringView("будущее.txt"));
	auto extra = sprt::move(v.out);

	EntryOptions opts;
	opts.localExtra = extra;
	opts.centralExtra = extra;

	Builder b;
	b.add(headerName, viewOf("the extra field is a version ahead"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"legacy.txt", bytesOf("the extra field is a version ahead"),
		true});
	return c;
}

static Case makeCp437Control() {
	Case c;
	c.name = "cp437-control";

	// A name with bytes below 0x20. CP437 gives those printable glyphs, so the answer depends
	// entirely on whether a reader calls the name ASCII or CP437.
	//
	// MEASURED against libzip: it calls it CP437 - "ctrl\x01\x02.txt" comes back as "ctrl☺☻.txt",
	// with 0x01/0x02 turned into U+263A/U+263B. The engine's reader leaves ASCII alone, so the two
	// disagree here on purpose; see the encoding-guess table in tests/stappler/zip/format.cpp.
	// Left uncharacterized so that libzip's spelling is recorded rather than asserted.
	c.characterized = false;

	Builder b;
	b.add(StringView("ctrl\x01\x02.txt", 11), viewOf("control bytes in the name"));
	c.archive = b.finish();
	c.meta = b.metadata();
	return c;
}

static Case makeTraversalExtended() {
	Case c;
	c.name = "traversal-extended";

	// Every shape the sanitizer has to refuse, in one archive. `ok.txt` is last so that a reader
	// which gives up on the first hostile name is visibly wrong rather than merely stricter.
	//
	// Not characterized against libzip: it does not sanitize at all, and how it spells a name with
	// an embedded NUL is its own business. What matters here is what OUR catalog does, which
	// Case::meta pins down.
	c.characterized = false;

	static const uint8_t withNul[] = {'n', 'u', 'l', 0x00, '.', 't', 'x', 't'};

	Builder b;
	b.add("a\\..\\b.txt", viewOf("backslashes"));
	b.add("..", viewOf("bare dotdot"));
	b.add("C:/absolute.txt", viewOf("drive letter"));
	b.add("a//b.txt", viewOf("empty segment"));
	b.add("./here.txt", viewOf("dot segment"));
	b.add(BytesView(withNul, sizeof(withNul)), viewOf("embedded nul"));
	b.add("ok.txt", viewOf("harmless"));
	c.archive = b.finish();
	c.meta = b.metadata();

	for (size_t i = 0; i + 1 < c.meta.size(); ++i) { markNameRejected(c.meta[i]); }
	return c;
}

// -- stage 4: content --

static Case makeEmptyFile() {
	Case c;
	c.name = "empty-file";

	// A legitimately empty file. The engine's reader returns success with nothing in it; libzip's
	// wrapper refuses it on the `stat.size == 0` early-out in _readFile (SPZip.cc), which makes an
	// empty file inside an archive unreadable. That is a defect, not a behaviour to carry over.
	Builder b;
	b.add("empty.txt", BytesView());
	b.add("nonempty.txt", viewOf("so the archive is not entirely empty"));
	c.archive = b.finish();
	c.meta = b.metadata();

	// MEASURED against libzip: the empty entry is listed, but reading it fails.
	c.entries.emplace_back(EntryExpect{"empty.txt", Bytes(), false});
	c.entries.emplace_back(EntryExpect{"nonempty.txt",
		bytesOf("so the archive is not entirely empty"), true});
	return c;
}

static Case makeCrcMismatch() {
	Case c;
	c.name = "crc-mismatch";

	// The stored CRC belongs to nothing in particular. The data decompresses fine, so nothing but
	// the checksum can catch it - which is exactly the case the check exists for.
	EntryOptions opts;
	opts.method = 8;
	opts.crcOverrideUsed = true;
	opts.crcOverride = 0xDEAD'BEEF;

	Builder b;
	b.add("wrong-crc.txt", viewOf("the checksum does not describe these bytes"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();
	c.meta[0].expectRead = Status::ErrorNotRecoverable;

	// MEASURED against libzip: it hands the 42 bytes back regardless - it does not verify the CRC on
	// read at all. The engine's reader refuses, which is the whole point of checking: whatever gets
	// this content parses it, and a parser fed corrupt input fails somewhere far less informative.
	c.entries.emplace_back(EntryExpect{"wrong-crc.txt",
		bytesOf("the checksum does not describe these bytes"), true});
	return c;
}

static Case makeDeflateCorrupt() {
	Case c;
	c.name = "deflate-corrupt";

	StringView name("corrupt.txt");
	String payload;
	for (int i = 0; i < 32; ++i) { payload.append("compressible compressible compressible\n"); }

	EntryOptions opts;
	opts.method = 8;

	Builder b;
	b.add(name, viewOf(StringView(payload.data(), payload.size())), opts);
	auto data = b.finish();
	c.meta = b.metadata();

	// The payload starts right after the local header and the name; there is no extra field here,
	// so the offset is exact. Damage it a little way in, past the deflate block header.
	auto payloadStart = 30 + name.size();
	data[payloadStart + 12] ^= 0xFF;
	data[payloadStart + 13] ^= 0xFF;

	c.archive = sprt::move(data);
	c.meta[0].expectRead = Status::ErrorNotRecoverable;

	// MEASURED against libzip: it returns 1248 bytes - the full declared length - from the damaged
	// stream, and reports success. Flipped bits in a Huffman stream still decode, just to the wrong
	// symbols, and nothing downstream of libzip notices. Left uncharacterized because those bytes
	// are garbage and pinning them would assert nothing worth asserting; what matters is recorded
	// here and in the engine's own expectation above.
	c.characterized = false;
	return c;
}

static Case makeTruncatedData() {
	Case c;
	c.name = "truncated-data";

	// The header claims far more compressed data than the file contains. Caught before any of it is
	// read - the range is checked against the source size first.
	EntryOptions opts;
	opts.compSizeOverrideUsed = true;
	opts.compSizeOverride = 0x10'0000;

	Builder b;
	b.add("short.txt", viewOf("the header claims much more than this"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();
	c.meta[0].expectRead = Status::ErrorInvalidArguemnt;

	// libzip's own answer is its business here; what matters is that ours refuses rather than
	// reading past the end.
	c.characterized = false;
	return c;
}

static Case makeLocalExtraDiffers() {
	Case c;
	c.name = "local-extra-differs";

	// The local extra field is present and the central one is empty - which is legal and common.
	// An entry's data offset can therefore only be computed from the LOCAL header; using the
	// central lengths would start the read 12 bytes early and hand back the extra field as content.
	Builder v;
	v.u16(0x9999); // an id nothing interprets
	v.u16(8);
	v.u64(0);
	auto extra = sprt::move(v.out);

	EntryOptions opts;
	opts.localExtra = extra;
	// centralExtra deliberately left empty

	Builder b;
	b.add("offset.txt", viewOf("found only if the local header is consulted"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"offset.txt",
		bytesOf("found only if the local header is consulted"), true});
	return c;
}

static Case makeMethodUnsupported() {
	Case c;
	c.name = "method-unsupported";

	// Method 12 is bzip2. OCF forbids everything but Store and Deflate, and this reader implements
	// exactly those two, so the entry is refused by name rather than misread.
	EntryOptions opts;
	opts.method = 12;

	Builder b;
	b.add("bzipped.txt", viewOf("stored bytes wearing a bzip2 label"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();

	c.entries.emplace_back(EntryExpect{"bzipped.txt", Bytes(), false});
	return c;
}

static Case makeStoreSizeMismatch() {
	Case c;
	c.name = "store-size-mismatch";

	// Store means the two sizes are the same number. A header that says otherwise describes an
	// entry that cannot exist, and believing either size would be a guess.
	EntryOptions opts;
	opts.declaredSizeUsed = true;
	opts.declaredSize = 64;

	Builder b;
	b.add("mismatched.txt", viewOf("twelve bytes?"), opts);
	c.archive = b.finish();
	c.meta = b.metadata();
	c.meta[0].expectRead = Status::ErrorNotRecoverable;

	c.characterized = false;
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

	// Added for stage 2 (the engine's own catalog parser): archives whose END RECORDS are the
	// interesting part, rather than their entries.
	ret.emplace_back(makePrefix());
	ret.emplace_back(makeEocdInComment());
	ret.emplace_back(makeCdOffsetLies());
	ret.emplace_back(makeCdCountLies());
	ret.emplace_back(makeZip64NoRecord());
	ret.emplace_back(makeZip64BadLocator());
	ret.emplace_back(makeManyEntries());

	// Added for stage 3 (name decoding and the path sanitizer).
	ret.emplace_back(makeUtf8NameNoFlag());
	ret.emplace_back(makeUtf8NameTrailing());
	ret.emplace_back(makeUnicodePathBadCrc());
	ret.emplace_back(makeUnicodePathBadVersion());
	ret.emplace_back(makeCp437Control());
	ret.emplace_back(makeTraversalExtended());

	// Added for stage 4 (reading an entry's content).
	ret.emplace_back(makeEmptyFile());
	ret.emplace_back(makeCrcMismatch());
	ret.emplace_back(makeDeflateCorrupt());
	ret.emplace_back(makeTruncatedData());
	ret.emplace_back(makeLocalExtraDiffers());
	ret.emplace_back(makeMethodUnsupported());
	ret.emplace_back(makeStoreSizeMismatch());
	return ret;
}

} // namespace stappler::test::zip
