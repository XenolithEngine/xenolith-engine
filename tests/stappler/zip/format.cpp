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

// Tests for the engine's OWN ZIP reader (stages 2-3 of docs/design/libzip-removal-plan.adoc),
// as opposed to zip.cpp, which characterizes libzip.
//
// The oracle here is the corpus itself: every archive was assembled byte by byte from known values,
// so `Case::meta` records exactly what was written and the parser has to read exactly that back.
// For the numeric fields this is a stronger oracle than libzip - libzip's public API does not even
// expose an entry's local header offset.

#include "SPCommon.h"
#include "SPZipCatalog.h"
#include "SPFilesystem.h"
#include "SPMemory.h"

#include "corpus.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

static test::zip::String escapeBytes(BytesView data) {
	test::zip::String ret;
	const char *hex = "0123456789abcdef";
	for (size_t i = 0; i < data.size(); ++i) {
		auto b = data.data()[i];
		if (b >= 0x20 && b < 0x7F) {
			ret.push_back(char(b));
		} else {
			ret.append("\\x");
			ret.push_back(hex[(b >> 4) & 0xF]);
			ret.push_back(hex[b & 0xF]);
		}
	}
	return ret;
}

static test::zip::String escapeName(StringView name) {
	return escapeBytes(BytesView((const uint8_t *)name.data(), name.size()));
}

struct Namer {
	test::zip::String prefix;

	explicit Namer(const test::zip::Case &c, StringView label) {
		prefix.append("zipfmt[");
		prefix.append(c.name.data(), c.name.size());
		prefix.append("/");
		prefix.append(label.data(), label.size());
		prefix.append("] ");
	}

	test::zip::String operator()(StringView what) const {
		auto s = prefix;
		s.append(what.data(), what.size());
		return s;
	}
};

// Compares one parsed entry against what the builder wrote. Every mismatch is reported with both
// values, because "field X differs" without the numbers is not actionable.
static bool compareEntry(const ZipEntry &got, const test::zip::EntryMeta &expect, size_t index,
		const Namer &named) {
	bool ok = true;

	auto fail = [&](StringView field, uint64_t gotValue, uint64_t expectValue) {
		ok = false;
		sprt::cout << "         entry #" << index << " " << field << ": got " << gotValue
				   << ", expected " << expectValue << "\n";
	};

	if (got.method != expect.method) {
		fail("method", got.method, expect.method);
	}
	if (got.flags != expect.flags) {
		fail("flags", got.flags, expect.flags);
	}
	if (got.crc32 != expect.crc32) {
		fail("crc32", got.crc32, expect.crc32);
	}
	if (got.compressedSize != expect.compressedSize) {
		fail("compressedSize", got.compressedSize, expect.compressedSize);
	}
	if (got.uncompressedSize != expect.uncompressedSize) {
		fail("uncompressedSize", got.uncompressedSize, expect.uncompressedSize);
	}
	if (got.localOffset != expect.localOffset) {
		fail("localOffset", got.localOffset, expect.localOffset);
	}

	auto name = StringView((const char *)expect.decodedName.data(), expect.decodedName.size());
	if (got.name.size() != name.size()
			|| (!name.empty() && sprt::memcmp(got.name.data(), name.data(), name.size()) != 0)) {
		ok = false;
		sprt::cout << "         entry #" << index << " name: got \"" << escapeName(got.name)
				   << "\", expected \"" << escapeName(name) << "\"\n";
	}

	auto flagFail = [&](StringView field, bool gotValue, bool expectValue) {
		ok = false;
		sprt::cout << "         entry #" << index << " " << field << ": got " << gotValue
				   << ", expected " << expectValue << "\n";
	};

	auto has = [&](ZipEntryFlags f) { return (got.state & f) != ZipEntryFlags::None; };

	if (has(ZipEntryFlags::Directory) != expect.expectDirectory) {
		flagFail("Directory", has(ZipEntryFlags::Directory), expect.expectDirectory);
	}
	if (has(ZipEntryFlags::NameRejected) != expect.expectNameRejected) {
		flagFail("NameRejected", has(ZipEntryFlags::NameRejected), expect.expectNameRejected);
	}
	if (has(ZipEntryFlags::Encrypted) != expect.expectEncrypted) {
		flagFail("Encrypted", has(ZipEntryFlags::Encrypted), expect.expectEncrypted);
	}
	if (has(ZipEntryFlags::UnsupportedMethod) != expect.expectUnsupportedMethod) {
		flagFail("UnsupportedMethod", has(ZipEntryFlags::UnsupportedMethod),
				expect.expectUnsupportedMethod);
	}

	return ok;
}

// One case, read through a catalog over the given source.
template <typename Interface>
static void verifyCatalog(ZipSource &source, const test::zip::Case &c, StringView label) {
	Namer named(c, label);

	ZipCatalog<Interface> catalog;
	auto st = catalog.read(source);

	check(sprt::status::isSuccessful(st) == c.parsable, named("read() accepts iff expected"));

	if (!c.parsable) {
		// A malformed archive has to be refused with a definite status, not merely "not Ok" - an
		// unset Status would also read as failure and would hide a path that never ran.
		check(st == Status::ErrorInvalidArguemnt, named("read() reports a definite error"));
		return;
	}

	if (!sprt::status::isSuccessful(st)) {
		return;
	}

	check(catalog.prefix() == c.expectedPrefix, named("prefix() matches"));

	if (c.meta.empty()) {
		// Cases whose archive was corrupted after building carry no metadata; the parse itself is
		// the whole assertion for them.
		return;
	}

	check(catalog.size() == c.meta.size(), named("size() matches the written entry count"));
	if (catalog.size() != c.meta.size()) {
		sprt::cout << "         got " << catalog.size() << " entries, expected " << c.meta.size()
				   << "\n";
		return;
	}

	bool fieldsOk = true;
	for (size_t i = 0; i < c.meta.size(); ++i) {
		auto entry = catalog.entry(i);
		if (!entry) {
			fieldsOk = false;
			continue;
		}
		if (!compareEntry(*entry, c.meta[i], i, named)) {
			fieldsOk = false;
		}
	}
	check(fieldsOk, named("every entry field matches what the builder wrote"));

	// The whole corpus is stamped with one timestamp, so a single expectation covers every entry -
	// and a conversion that silently used the process timezone would miss it in any locale but UTC.
	bool timeOk = true;
	for (size_t i = 0; i < c.meta.size(); ++i) {
		if (catalog.entry(i)->mtime.toSeconds() != test::zip::CORPUS_MTIME_UTC) {
			timeOk = false;
			sprt::cout << "         entry #" << i << " mtime: got "
					   << catalog.entry(i)->mtime.toSeconds() << ", expected "
					   << test::zip::CORPUS_MTIME_UTC << "\n";
			break;
		}
	}
	check(timeOk, named("mtime is the DOS stamp read as UTC"));

	check(catalog.entry(c.meta.size()) == nullptr, named("entry() refuses an out-of-range index"));

	// locate() has to find every entry it listed, and round-trip to the same name.
	bool locateOk = true;
	for (size_t i = 0; i < c.meta.size(); ++i) {
		auto name = catalog.entry(i)->name;
		auto idx = catalog.locate(name);
		if (idx == maxOf<uint64_t>()) {
			locateOk = false;
			sprt::cout << "         locate() missed \"" << escapeName(name) << "\"\n";
			break;
		}
		if (catalog.entry(idx)->name != name) {
			locateOk = false;
			break;
		}
	}
	check(locateOk, named("locate() finds every entry"));

	check(catalog.locate("no/such/entry.txt") == maxOf<uint64_t>(),
			named("locate() reports a missing entry"));
}

template <typename Interface>
static void runFromMemory(const test::zip::Case &c, StringView label) {
	ZipSource source;
	source.setMemory(BytesView(c.archive.data(), c.archive.size()));
	verifyCatalog<Interface>(source, c, label);
}

// The same case through the other backing, so the catalog is proved not to depend on the bytes
// being contiguous in memory.
static void runFromFile(const test::zip::Case &c) {
	FileInfo info("xlzipfmt_probe.zip", LocationCategory::Custom);
	filesystem::remove(info);

	if (!filesystem::write(info, c.archive.data(), c.archive.size())) {
		check(false, "zipfmt: probe archive written to disk");
		return;
	}

	{
		auto f = filesystem::openForReading(info);
		if (!f) {
			check(false, "zipfmt: probe archive reopened");
			filesystem::remove(info);
			return;
		}

		ZipSource source;
		source.setFile(sprt::move(f));
		verifyCatalog<mem_std::Interface>(source, c, "file");
	}

	filesystem::remove(info);
}

// Duplicate names are legal in a ZIP, and which one wins is a decision, not an accident: the first
// occurrence does, so a later entry cannot shadow an earlier one. Built here rather than in the
// corpus because no other test needs it.
static void runDuplicateNames() {
	sprt::cout << "\n  -- case: duplicate-names --\n";

	// Assembled here rather than in the corpus because the corpus Builder keys entries by name and
	// no other test needs a duplicate; two stored entries are little enough to spell out.
	StringView first("first wins");
	StringView second("second must not shadow it");

	auto build = [&]() -> test::zip::Bytes {
		test::zip::Bytes out;
		auto u8v = [&](uint8_t v) { out.emplace_back(v); };
		auto u16v = [&](uint16_t v) {
			u8v(uint8_t(v & 0xFF));
			u8v(uint8_t((v >> 8) & 0xFF));
		};
		auto u32v = [&](uint32_t v) {
			u16v(uint16_t(v & 0xFFFF));
			u16v(uint16_t((v >> 16) & 0xFFFF));
		};
		auto rawv = [&](StringView s) {
			for (auto ch : s) { u8v(uint8_t(ch)); }
		};

		StringView name("same.txt");
		uint64_t offsets[2] = {0, 0};
		StringView payloads[2] = {first, second};

		for (int i = 0; i < 2; ++i) {
			offsets[i] = out.size();
			u32v(0x04034b50);
			u16v(20);
			u16v(0);
			u16v(0);
			u16v(0);
			u16v(uint16_t(((2024 - 1980) << 9) | (1 << 5) | 1));
			u32v(0);
			u32v(uint32_t(payloads[i].size()));
			u32v(uint32_t(payloads[i].size()));
			u16v(uint16_t(name.size()));
			u16v(0);
			rawv(name);
			rawv(payloads[i]);
		}

		auto cdOffset = out.size();
		for (int i = 0; i < 2; ++i) {
			u32v(0x02014b50);
			u16v(20);
			u16v(20);
			u16v(0);
			u16v(0);
			u16v(0);
			u16v(uint16_t(((2024 - 1980) << 9) | (1 << 5) | 1));
			u32v(0);
			u32v(uint32_t(payloads[i].size()));
			u32v(uint32_t(payloads[i].size()));
			u16v(uint16_t(name.size()));
			u16v(0);
			u16v(0);
			u16v(0);
			u16v(0);
			u32v(0);
			u32v(uint32_t(offsets[i]));
			rawv(name);
		}
		auto cdSize = out.size() - cdOffset;

		u32v(0x06054b50);
		u16v(0);
		u16v(0);
		u16v(2);
		u16v(2);
		u32v(uint32_t(cdSize));
		u32v(uint32_t(cdOffset));
		u16v(0);
		return out;
	};

	auto archive = build();

	ZipSource source;
	source.setMemory(BytesView(archive.data(), archive.size()));

	ZipCatalog<mem_std::Interface> catalog;
	auto st = catalog.read(source);

	check(sprt::status::isSuccessful(st), "zipfmt[duplicate-names] read() accepts the archive");
	if (!sprt::status::isSuccessful(st)) {
		return;
	}

	check(catalog.size() == 2, "zipfmt[duplicate-names] both entries are listed");
	check(catalog.locate("same.txt") == 0,
			"zipfmt[duplicate-names] locate() resolves to the first occurrence");
}

// -- stage 3: names, checked directly rather than only through archives --

// UTF-8 validation, including the shapes that are structurally well-formed but still refused.
static void runUtf8Validation() {
	sprt::cout << "\n  -- names: utf-8 validation --\n";

	struct Row {
		const char *what;
		const uint8_t *bytes;
		size_t size;
		bool valid;
	};

	static const uint8_t asciiOnly[] = {'a', 'b', 'c'};
	static const uint8_t twoByte[] = {0xD0, 0xBF}; // U+043F
	static const uint8_t threeByte[] = {0xE2, 0x82, 0xAC}; // U+20AC
	static const uint8_t fourByte[] = {0xF0, 0x9F, 0x98, 0x80}; // U+1F600
	static const uint8_t trailingMultibyte[] = {'a', 0xD0, 0xBF}; // ends mid-alphabet
	static const uint8_t truncated[] = {'a', 0xD0}; // lead byte with nothing after it
	static const uint8_t loneContinuation[] = {0x81};
	static const uint8_t badContinuation[] = {0xD0, 0x41};
	static const uint8_t overlongSlash[] = {0xC0, 0xAF}; // '/' spelled the long way
	static const uint8_t overlongNul[] = {0xC0, 0x80};
	static const uint8_t surrogate[] = {0xED, 0xA0, 0x80}; // U+D800
	static const uint8_t tooLarge[] = {0xF5, 0x80, 0x80, 0x80}; // beyond U+10FFFF

	static const Row rows[] = {
		{"ascii only", asciiOnly, sizeof(asciiOnly), true},
		{"two-byte sequence", twoByte, sizeof(twoByte), true},
		{"three-byte sequence", threeByte, sizeof(threeByte), true},
		{"four-byte sequence", fourByte, sizeof(fourByte), true},
		// The boundary every UTF-8 validator gets wrong first: the last character is multi-byte and
		// the sequence ends exactly at the end of the buffer.
		{"name ending in a multi-byte character", trailingMultibyte, sizeof(trailingMultibyte),
			true},
		{"lead byte with no continuation", truncated, sizeof(truncated), false},
		{"continuation byte in lead position", loneContinuation, sizeof(loneContinuation), false},
		{"continuation byte that is not one", badContinuation, sizeof(badContinuation), false},
		// Refused although libzip accepts them: an overlong '/' would pass a byte-scanning
		// sanitizer and be folded back into a separator downstream.
		{"overlong '/'", overlongSlash, sizeof(overlongSlash), false},
		{"overlong NUL", overlongNul, sizeof(overlongNul), false},
		{"surrogate", surrogate, sizeof(surrogate), false},
		{"beyond U+10FFFF", tooLarge, sizeof(tooLarge), false},
	};

	for (auto &row : rows) {
		auto got = zipIsValidUtf8(BytesView(row.bytes, row.size));

		test::zip::String label("zipfmt[utf8] ");
		label.append(row.what);
		check(got == row.valid, label);
	}
}

// The path sanitizer. Each row names the rule it exercises, so a rejection for the WRONG reason is
// a failure rather than an accidental pass.
static void runSanitizer() {
	sprt::cout << "\n  -- names: path sanitizer --\n";

	struct Row {
		const char *what;
		const uint8_t *bytes;
		size_t size;
		ZipNameRejection expect;
	};

	static const uint8_t plain[] = {'a', '.', 't', 'x', 't'};
	static const uint8_t nested[] = {'a', '/', 'b', '/', 'c', '.', 't', 'x', 't'};
	static const uint8_t dirEntry[] = {'d', 'i', 'r', '/'};
	static const uint8_t dotInName[] = {'a', '.', '.', 'b'}; // dots INSIDE a segment are fine
	static const uint8_t empty[] = {'x'};
	static const uint8_t absolute[] = {'/', 'a'};
	static const uint8_t drive[] = {'C', ':', '/', 'a'};
	static const uint8_t driveLower[] = {'c', ':', 'a'};
	static const uint8_t backslash[] = {'a', '\\', 'b'};
	static const uint8_t parent[] = {'.', '.', '/', 'a'};
	static const uint8_t bareParent[] = {'.', '.'};
	static const uint8_t buried[] = {'a', '/', '.', '.', '/', 'b'};
	static const uint8_t dotSegment[] = {'.', '/', 'a'};
	static const uint8_t doubleSlash[] = {'a', '/', '/', 'b'};
	static const uint8_t withNul[] = {'a', 0x00, 'b'};

	static const Row rows[] = {
		{"a plain name is accepted", plain, sizeof(plain), ZipNameRejection::None},
		{"a nested path is accepted", nested, sizeof(nested), ZipNameRejection::None},
		// A trailing slash is the conventional spelling of a directory entry, not a defect.
		{"a directory entry is accepted", dirEntry, sizeof(dirEntry), ZipNameRejection::None},
		{"dots inside a segment are accepted", dotInName, sizeof(dotInName),
			ZipNameRejection::None},
		{"an empty name is refused", empty, 0, ZipNameRejection::Empty},
		{"an absolute path is refused", absolute, sizeof(absolute), ZipNameRejection::Absolute},
		{"a drive letter is refused", drive, sizeof(drive), ZipNameRejection::DriveLetter},
		{"a lowercase drive letter is refused", driveLower, sizeof(driveLower),
			ZipNameRejection::DriveLetter},
		// Refused rather than rewritten: on POSIX a backslash is an ordinary filename character, so
		// turning it into '/' would silently name a different file.
		{"a backslash is refused", backslash, sizeof(backslash), ZipNameRejection::Backslash},
		{"a leading .. is refused", parent, sizeof(parent), ZipNameRejection::PathSegments},
		{"a bare .. is refused", bareParent, sizeof(bareParent), ZipNameRejection::PathSegments},
		{"a buried .. is refused", buried, sizeof(buried), ZipNameRejection::PathSegments},
		{"a . segment is refused", dotSegment, sizeof(dotSegment), ZipNameRejection::PathSegments},
		{"an empty segment is refused", doubleSlash, sizeof(doubleSlash),
			ZipNameRejection::PathSegments},
		{"an embedded NUL is refused", withNul, sizeof(withNul), ZipNameRejection::EmbeddedNul},
	};

	for (auto &row : rows) {
		auto got = zipCheckName(BytesView(row.bytes, row.size));

		test::zip::String label("zipfmt[sanitize] ");
		label.append(row.what);

		bool ok = (got == row.expect);
		if (!ok) {
			sprt::cout << "         got rejection " << uint32_t(got) << ", expected "
					   << uint32_t(row.expect) << "\n";
		}
		check(ok, label);
	}
}

// Encoding guessing, which is where this reader and libzip part company on purpose.
static void runEncodingGuess() {
	sprt::cout << "\n  -- names: encoding guess --\n";

	struct Row {
		const char *what;
		const uint8_t *bytes;
		size_t size;
		bool utf8Flag;
		ZipNameEncoding expect;
	};

	static const uint8_t ascii[] = {'a', '.', 't', 'x', 't'};
	static const uint8_t control[] = {'a', 0x01, 'b'};
	static const uint8_t utf8[] = {0xD0, 0xBF, '.', 't', 'x', 't'};
	static const uint8_t cp437[] = {0x81, 0x94, 0x81};
	static const uint8_t overlong[] = {0xC0, 0xAF};

	static const Row rows[] = {
		{"plain ascii, no flag", ascii, sizeof(ascii), false, ZipNameEncoding::Ascii},
		// libzip routes ascii-with-control-characters through CP437, which turns 0x01 into a
		// dingbat. Left alone here; the sanitizer is what decides whether such a name is usable.
		{"ascii with a control byte", control, sizeof(control), false, ZipNameEncoding::Ascii},
		// The important one: valid UTF-8 is taken as UTF-8 even though bit 11 is clear, because the
		// stock Linux `zip` never sets it.
		{"valid utf-8 without the flag", utf8, sizeof(utf8), false, ZipNameEncoding::Utf8},
		{"valid utf-8 with the flag", utf8, sizeof(utf8), true, ZipNameEncoding::Utf8},
		{"invalid utf-8 falls back to cp437", cp437, sizeof(cp437), false, ZipNameEncoding::Cp437},
		// libzip calls this an encoding error and gives up; CP437 at least yields a readable name.
		{"the flag lies, bytes are not utf-8", cp437, sizeof(cp437), true, ZipNameEncoding::Cp437},
		// Refused as UTF-8 by the strict validator, so it lands in CP437 and its bytes become
		// visible characters instead of a separator.
		{"an overlong sequence is not utf-8", overlong, sizeof(overlong), true,
			ZipNameEncoding::Cp437},
	};

	for (auto &row : rows) {
		auto got = zipGuessEncoding(BytesView(row.bytes, row.size), row.utf8Flag);

		test::zip::String label("zipfmt[encoding] ");
		label.append(row.what);
		check(got == row.expect, label);
	}
}

} // namespace

void performZipFormatTests() {
	sprt::cout << "\n== stappler zip format tests (the engine's own reader) ==\n";

	auto corpus = test::zip::buildCorpus();

	for (auto &c : corpus) {
		sprt::cout << "\n  -- case: " << c.name << " (" << c.archive.size() << " bytes) --\n";

		runFromMemory<mem_std::Interface>(c, "mem_std");
		runFromFile(c);
	}

	runDuplicateNames();

	runUtf8Validation();
	runSanitizer();
	runEncodingGuess();

	// The pool interface is the one EPUB uses, so the catalog gets the same sweep there - in its own
	// pool, since the containers allocate from the active one.
	auto pool = memory::pool::create((memory::pool_t *)nullptr);
	memory::perform([&] {
		for (auto &c : corpus) {
			sprt::cout << "\n  -- case: " << c.name << " (pool) --\n";
			runFromMemory<memory::PoolInterface>(c, "pool");
		}
	}, pool);
	memory::pool::destroy(pool);
}

} // namespace stappler
