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

// Tests for the engine's own ZIP reader and writer, directly rather than through ZipArchive
// (docs/design/libzip-removal-plan.adoc, stages 2-7). zip.cpp covers the public surface.
//
// The oracle here is the corpus itself: every archive was assembled byte by byte from known values,
// so `Case::meta` records exactly what was written and the parser has to read exactly that back.
// For the numeric fields this was always a stronger oracle than libzip, whose public API does not
// even expose an entry's local header offset.

#include "SPCommon.h"
#include "SPZip.h"
#include "SPZipReader.h"
#include "SPFilesystem.h"
#include "SPMemory.h"
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

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

// Text as bytes, without the NUL-clamping StringView's (pointer, length) constructor applies.
static BytesView viewOf(StringView s) { return BytesView((const uint8_t *)s.data(), s.size()); }

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

	// Regression, found by ASan: StringView::terminated() reads the byte at data()[size()] and
	// requires it to exist, and everything that turns a view into a C string calls it. An
	// exactly-sized name arena made every name a one-byte overread for such a caller - which a
	// non-sanitized run passes without a murmur.
	bool terminatedOk = true;
	for (size_t i = 0; i < c.meta.size(); ++i) {
		if (!catalog.entry(i)->name.terminated()) {
			terminatedOk = false;
			break;
		}
	}
	check(terminatedOk, named("every entry name is NUL-terminated"));

	// -- stage 4: the entries' content --
	//
	// Every entry is read, whether or not it is supposed to succeed: a refusal has to arrive with
	// the RIGHT status, since several separate rules can refuse one and "it failed" would pass for
	// any of them.
	bool statusOk = true;
	bool contentOk = true;

	for (size_t i = 0; i < c.meta.size(); ++i) {
		auto entry = catalog.entry(i);
		typename Interface::BytesType content;

		auto st = zipReadEntry<Interface>(source, *entry, catalog.prefix(), content);

		if (st != c.meta[i].expectRead) {
			statusOk = false;
			sprt::cout << "         entry #" << i << " \"" << escapeName(entry->name)
					   << "\" read status: got " << int32_t(st) << ", expected "
					   << int32_t(c.meta[i].expectRead) << "\n";
			continue;
		}

		if (st != Status::Ok) {
			// A refused read must not leave anything behind for a caller to mistake for content.
			if (!content.empty()) {
				contentOk = false;
				sprt::cout << "         entry #" << i << " produced " << content.size()
						   << " bytes despite being refused\n";
			}
			continue;
		}

		auto &expect = c.meta[i].content;
		if (content.size() != expect.size()
				|| (!expect.empty()
						&& sprt::memcmp(content.data(), expect.data(), expect.size()) != 0)) {
			contentOk = false;
			sprt::cout << "         entry #" << i << " \"" << escapeName(entry->name)
					   << "\" content: got " << content.size() << " bytes, expected "
					   << expect.size() << "\n";
		}
	}

	check(statusOk, named("every entry reads with the expected status"));
	check(contentOk, named("every readable entry yields the bytes the builder wrote"));
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

/* The differential check against libzip lived here until stage 6.
 *
 * It compared, for every entry both readers would read, that the bytes matched. Stage 6 removed
 * libzip from underneath ZipArchive, so both sides of that comparison became the same code and the
 * check stopped meaning anything. Deleted rather than left passing: a test whose oracle is gone
 * still reports success, which is worse than no test at all. What it used to guard now lives in
 * the golden expectations of tests/stappler/zip/zip.cpp.
 */

/* -- stage 7: the writer --
 *
 * Written archives are read back with the engine's OWN catalog, which is not circular reasoning as
 * long as the reader is independently anchored - and it is, by the 37 hand-built corpus archives
 * above. What this adds is the other direction: that what the writer emits is what the format says
 * it should be, field by field.
 *
 * The genuinely external check is `unzip -t`, run at the end of this file.
 */
static void runWriter() {
	sprt::cout << "\n  -- writer --\n";

	StringView compressible;
	test::zip::String payload;
	for (int i = 0; i < 64; ++i) { payload.append("compressible compressible compressible\n"); }
	compressible = StringView(payload.data(), payload.size());

	// Random-looking bytes: deflate cannot help, so the writer has to fall back to Store rather
	// than emit an entry larger than its input.
	static const uint8_t incompressible[] = {0x8F, 0x2C, 0xE1, 0x77, 0x03, 0xBA, 0x51, 0xD9, 0x64,
		0x1E, 0xAC, 0x38, 0xF5, 0x90, 0x2B, 0xC7};

	ZipWriter<mem_std::Interface> writer;

	check(writer.addDir("dir"), "zipfmt[writer] addDir()");
	check(writer.addFile("dir/text.txt", viewOf(compressible), false),
			"zipfmt[writer] addFile() deflated");
	check(writer.addFile("stored.bin", BytesView(incompressible, sizeof(incompressible)), false),
			"zipfmt[writer] addFile() incompressible");
	check(writer.addFile("forced.txt", viewOf(compressible), true),
			"zipfmt[writer] addFile() forced to store");
	check(writer.addFile("empty.txt", BytesView(), false), "zipfmt[writer] addFile() empty");
	// Compressible on purpose: this entry has to exercise the UTF-8 flag AND deflate at once. With a
	// short payload the writer would rightly fall back to Store and the method assertion below would
	// be testing the fallback twice instead of both branches.
	check(writer.addFile("имя.txt", viewOf(compressible), false),
			"zipfmt[writer] addFile() non-ascii name");

	// Refusing to write what our own reader refuses to read. Without this the module could produce
	// archives it classifies as hostile.
	check(!writer.addFile("../escape.txt", viewOf("nope"), false),
			"zipfmt[writer] a traversal name is refused");
	check(!writer.addFile("/absolute.txt", viewOf("nope"), false),
			"zipfmt[writer] an absolute name is refused");
	check(!writer.addFile("a\\b.txt", viewOf("nope"), false),
			"zipfmt[writer] a backslash name is refused");

	auto buffer = writer.save();
	check(buffer.input() > 0, "zipfmt[writer] save() produced bytes");

	// -- read it back --

	ZipSource source;
	source.setMemory(BytesView(buffer.data(), buffer.input()));

	ZipCatalog<mem_std::Interface> catalog;
	check(sprt::status::isSuccessful(catalog.read(source)),
			"zipfmt[writer] the result parses as an archive");
	if (!catalog.valid()) {
		return;
	}

	check(catalog.size() == 6, "zipfmt[writer] every accepted entry is present");
	check(catalog.prefix() == 0, "zipfmt[writer] no prefix");

	struct Expect {
		const char *name;
		bool directory;
		uint16_t method;
		bool utf8Flag;
	};

	static const Expect expects[] = {
		{"dir/", true, ZIP_METHOD_STORE, false},
		{"dir/text.txt", false, ZIP_METHOD_DEFLATE, false},
		// deflate would have grown these 16 bytes, so Store is the honest choice
		{"stored.bin", false, ZIP_METHOD_STORE, false},
		{"forced.txt", false, ZIP_METHOD_STORE, false},
		{"empty.txt", false, ZIP_METHOD_STORE, false},
		{"имя.txt", false, ZIP_METHOD_DEFLATE, true},
	};

	bool shapeOk = true;
	for (size_t i = 0; i < catalog.size() && i < sizeof(expects) / sizeof(expects[0]); ++i) {
		auto entry = catalog.entry(i);
		auto &e = expects[i];

		bool isDir = (entry->state & ZipEntryFlags::Directory) != ZipEntryFlags::None;
		bool hasUtf8 = (entry->flags & ZIP_FLAG_UTF8) != 0;

		if (entry->name != StringView(e.name) || isDir != e.directory || hasUtf8 != e.utf8Flag) {
			shapeOk = false;
			sprt::cout << "         entry #" << i << " \"" << escapeName(entry->name)
					   << "\": dir=" << isDir << " utf8=" << hasUtf8 << " method=" << entry->method
					   << "\n";
		}
	}
	check(shapeOk, "zipfmt[writer] names, directory flags and UTF-8 flags are as written");

	// Method choice is asserted separately: falling back to Store on incompressible data is a
	// decision, and a silent change of it would otherwise pass unnoticed.
	bool methodOk = true;
	for (size_t i = 0; i < catalog.size() && i < sizeof(expects) / sizeof(expects[0]); ++i) {
		if (catalog.entry(i)->method != expects[i].method) {
			methodOk = false;
			sprt::cout << "         entry #" << i << " method " << catalog.entry(i)->method
					   << ", expected " << expects[i].method << "\n";
		}
	}
	check(methodOk, "zipfmt[writer] Store is chosen when deflate would not help");

	// -- content --

	struct Content {
		const char *name;
		BytesView data;
	};

	const Content contents[] = {
		{"dir/text.txt", viewOf(compressible)},
		{"stored.bin", BytesView(incompressible, sizeof(incompressible))},
		{"forced.txt", viewOf(compressible)},
		{"empty.txt", BytesView()},
		{"имя.txt", viewOf(compressible)},
	};

	bool contentOk = true;
	for (auto &it : contents) {
		auto idx = catalog.locate(it.name);
		if (idx == maxOf<uint64_t>()) {
			contentOk = false;
			continue;
		}

		mem_std::Interface::BytesType got;
		auto st = zipReadEntry<mem_std::Interface>(source, *catalog.entry(idx), 0, got);
		if (!sprt::status::isSuccessful(st) || got.size() != it.data.size()
				|| (!it.data.empty()
						&& sprt::memcmp(got.data(), it.data.data(), it.data.size()) != 0)) {
			contentOk = false;
			sprt::cout << "         \"" << it.name << "\" round-trip failed: status "
					   << int32_t(st) << ", " << got.size() << " bytes, expected "
					   << it.data.size() << "\n";
		}
	}
	check(contentOk, "zipfmt[writer] every entry round-trips byte for byte");

	// A directory entry has no content of its own; the reader says so with Declined rather than an
	// error, and that distinction should survive the round trip.
	{
		mem_std::Interface::BytesType got;
		auto st = zipReadEntry<mem_std::Interface>(source, *catalog.entry(0), 0, got);
		check(st == Status::Declined, "zipfmt[writer] the directory entry declines to be read");
	}

	// -- an archive large enough to need a second block of entries --
	{
		ZipWriter<mem_std::Interface> many;
		for (int i = 0; i < 500; ++i) {
			test::zip::String name;
			name.append("f");
			name.append(1, char('0' + (i / 100) % 10));
			name.append(1, char('0' + (i / 10) % 10));
			name.append(1, char('0' + i % 10));
			name.append(".txt");
			many.addFile(StringView(name.data(), name.size()),
					viewOf(StringView(name.data(), name.size())), true);
		}

		auto buf = many.save();

		ZipSource s2;
		s2.setMemory(BytesView(buf.data(), buf.input()));

		ZipCatalog<mem_std::Interface> c2;
		check(sprt::status::isSuccessful(c2.read(s2)) && c2.size() == 500,
				"zipfmt[writer] 500 entries survive the round trip");
	}
}

/* The one check nothing inside this codebase can provide: hand the archive to an unrelated
 * implementation.
 *
 * Reading back what we wrote with what we wrote it with proves internal consistency and nothing
 * else - a shared misreading of the specification would pass. Info-ZIP's `unzip -t` verifies the
 * structure and every CRC independently.
 *
 * When unzip is absent the check is reported as NOT RUN rather than quietly passing: an
 * unavailable oracle is not evidence.
 */
static void runExternalValidation() {
	sprt::cout << "\n  -- writer: external validation --\n";

	ZipWriter<mem_std::Interface> writer;

	test::zip::String payload;
	for (int i = 0; i < 32; ++i) { payload.append("external validation payload\n"); }

	writer.addDir("dir");
	writer.addFile("dir/text.txt", viewOf(StringView(payload.data(), payload.size())), false);
	writer.addFile("stored.txt", viewOf("stored verbatim"), true);
	writer.addFile("empty.txt", BytesView(), false);
	writer.addFile("имя.txt", viewOf("non-ascii name"), false);

	auto buffer = writer.save();

	FileInfo info("xlzip_external.zip", LocationCategory::Custom);
	filesystem::remove(info);
	if (!filesystem::write(info, buffer.data(), buffer.input())) {
		check(false, "zipfmt[external] the archive was written to disk");
		return;
	}

	auto path = filesystem::findPath<mem_std::Interface>(info);

	// Through the runtime's process API rather than system(): there is no system() in the sprt libc,
	// and spawning is one of the things the runtime deliberately owns (see the platform notes in
	// docs/usage/codestyle).
	test::zip::String command("unzip -t -qq '");
	command.append(path.data(), path.size());
	command.append("'");

	auto looper = sprt::dispatch::Looper::acquire();

	int exitCode = -1;
	bool finished = false;
	test::zip::String output;

	auto handle = looper->spawnProcess(StringView(command.data(), command.size()),
			[&](StringView chunk) { output.append(chunk.data(), chunk.size()); },
			[&](int code, Status) {
		exitCode = code;
		finished = true;
		looper->wakeup();
	});

	if (!handle) {
		sprt::cout << "         NOT RUN: the process could not be spawned\n";
		filesystem::remove(info);
		return;
	}

	// Bounded, so that a hung external tool cannot hang the suite.
	while (!finished) {
		if (looper->run(TimeInterval::seconds(30)) == Status::ErrorCancelled) {
			break;
		}
	}

	if (!finished) {
		sprt::cout << "         NOT RUN: unzip did not finish within the timeout\n";
	} else if (exitCode == 127) {
		// An unavailable oracle is not evidence, so this is reported rather than counted as a pass.
		sprt::cout << "         NOT RUN: unzip is not installed on this machine\n";
	} else {
		if (exitCode != 0) {
			sprt::cout << "         unzip said: " << StringView(output.data(), output.size())
					   << "\n";
		}
		check(exitCode == 0, "zipfmt[external] Info-ZIP unzip -t accepts the archive");
	}

	filesystem::remove(info);
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

	runWriter();
	runExternalValidation();

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
