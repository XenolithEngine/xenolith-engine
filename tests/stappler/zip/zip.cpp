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

// The public ZipArchive API, end to end (docs/design/libzip-removal-plan.adoc).
//
// This began as a CHARACTERIZATION bench: it pinned down what the libzip-backed ZipArchive actually
// did, so that the from-scratch reader of stages 2-4 could be measured against a recorded truth
// instead of an assumption. Stage 6 removed libzip from underneath it, so the oracle is gone and
// the expectations are now golden data - recorded behaviour, frozen deliberately.
//
// It stays valuable for a reason zipformat cannot cover: this is the only test that goes through
// the PUBLIC surface, while zipformat exercises the catalog and reader directly.
//
// Where the answers changed at the switch, the case in corpus.cpp says what libzip used to do and
// why the new answer differs. A case whose `characterized` flag is false asserts nothing about its
// entries and merely prints them.

#include "SPCommon.h"
#include "SPZip.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPMemory.h"

#include "corpus.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

struct Observed {
	test::zip::String name;
	size_t size = 0;
	uint64_t index = 0;
};

// Names in a ZIP are bytes, and two of the corpus cases carry names that are deliberately not
// UTF-8. Escape anything outside printable ASCII so the log stays readable and diffable.
static test::zip::String escapeName(StringView name) {
	test::zip::String ret;
	const char *hex = "0123456789abcdef";
	for (auto ch : name) {
		auto b = uint8_t(ch);
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

template <typename Interface>
static test::zip::Vector<Observed> listEntries(const ZipArchive<Interface> &archive) {
	test::zip::Vector<Observed> ret;
	archive.ftw([&](uint64_t index, StringView path, size_t size, Time) {
		ret.emplace_back(Observed{path.template str<memory::StandardInterface>(), size, index});
	});
	return ret;
}

template <typename Interface>
static void dumpEntries(const ZipArchive<Interface> &archive) {
	auto observed = listEntries(archive);
	sprt::cout << "         entries: " << observed.size() << "\n";
	for (auto &it : observed) {
		sprt::cout << "         #" << it.index << "  \"" << escapeName(it.name) << "\"  size="
				   << it.size << "\n";
	}
}

// Reads an entry and hands back its bytes; `ok` reports whether the read was accepted at all.
template <typename Interface>
static test::zip::Bytes readEntry(const ZipArchive<Interface> &archive, StringView name, bool &ok) {
	test::zip::Bytes ret;
	ok = archive.readFile(name, [&](BytesView data) {
		ret.assign(data.data(), data.data() + data.size());
	});
	return ret;
}

template <typename Interface>
static test::zip::Bytes readEntry(const ZipArchive<Interface> &archive, uint64_t index, bool &ok) {
	test::zip::Bytes ret;
	ok = archive.readFile(index, [&](BytesView data) {
		ret.assign(data.data(), data.data() + data.size());
	});
	return ret;
}

static bool sameBytes(const test::zip::Bytes &l, const test::zip::Bytes &r) {
	return l.size() == r.size() && (l.empty() || sprt::memcmp(l.data(), r.data(), l.size()) == 0);
}

// The full API sweep for one open archive. `label` distinguishes the source (memory/file) and the
// memory interface, because a failure that shows up in only one of them is the interesting kind.
template <typename Interface>
static void verifyArchive(const ZipArchive<Interface> &archive, const test::zip::Case &c,
		StringView label) {
	auto prefix = test::zip::String("zip[");
	prefix.append(c.name.data(), c.name.size());
	prefix.append("/");
	prefix.append(label.data(), label.size());
	prefix.append("] ");

	auto named = [&](StringView what) {
		auto s = prefix;
		s.append(what.data(), what.size());
		return s;
	};

	if (!c.characterized) {
		sprt::cout << "         (not characterized - recording libzip behaviour)\n";
		dumpEntries(archive);
		return;
	}

	auto observed = listEntries(archive);

	check(archive.size() == c.entries.size(), named("size() matches entry count"));
	check(observed.size() == c.entries.size(), named("ftw() yields every entry"));

	if (observed.size() != c.entries.size()) {
		dumpEntries(archive);
		return;
	}

	bool namesMatch = true;
	bool sizesMatch = true;
	for (size_t i = 0; i < observed.size(); ++i) {
		auto &exp = c.entries[i];
		if (StringView(observed[i].name) != StringView(exp.name)) {
			namesMatch = false;
		}
		// a directory entry has no content of its own, so only readable entries carry a size
		if (exp.readable && observed[i].size != exp.content.size()) {
			sizesMatch = false;
		}
	}
	check(namesMatch, named("ftw() reports the expected names"));
	check(sizesMatch, named("ftw() reports the expected sizes"));
	if (!namesMatch || !sizesMatch) {
		dumpEntries(archive);
	}

	bool locateOk = true;
	bool roundTripOk = true;
	bool contentByNameOk = true;
	bool contentByIndexOk = true;

	for (auto &exp : c.entries) {
		auto idx = archive.locateFile(exp.name);
		if (idx == maxOf<uint64_t>()) {
			locateOk = false;
			continue;
		}
		if (StringView(archive.getFileName(idx)) != StringView(exp.name)) {
			roundTripOk = false;
		}

		bool ok = false;
		auto byName = readEntry(archive, StringView(exp.name), ok);
		if (ok != exp.readable || (exp.readable && !sameBytes(byName, exp.content))) {
			contentByNameOk = false;
			sprt::cout << "         mismatch on \"" << escapeName(exp.name) << "\": readable="
					   << ok << " (expected " << exp.readable << "), got " << byName.size()
					   << " bytes, expected " << exp.content.size() << "\n";
		}

		auto byIndex = readEntry(archive, idx, ok);
		if (ok != exp.readable || (exp.readable && !sameBytes(byIndex, exp.content))) {
			contentByIndexOk = false;
		}
	}

	check(locateOk, named("locateFile() finds every entry"));
	check(roundTripOk, named("getFileName() round-trips locateFile()"));
	check(contentByNameOk, named("readFile(name) yields the expected bytes"));
	check(contentByIndexOk, named("readFile(index) yields the expected bytes"));

	check(archive.locateFile("no/such/entry.txt") == maxOf<uint64_t>(),
			named("locateFile() reports a missing entry"));
}

// One case, read out of a memory buffer.
template <typename Interface>
static void runFromMemory(const test::zip::Case &c, StringView label) {
	ZipArchive<Interface> archive(BytesView(c.archive.data(), c.archive.size()), true);

	auto prefix = test::zip::String("zip[");
	prefix.append(c.name.data(), c.name.size());
	prefix.append("/");
	prefix.append(label.data(), label.size());
	prefix.append("] opens as expected");

	check(bool(archive) == c.openable, prefix);
	if (!archive) {
		return;
	}
	verifyArchive(archive, c, label);
}

// The same case, read out of a real file - the other constructor, and the other libzip source
// machine behind it.
static void runFromFile(const test::zip::Case &c) {
	FileInfo info("xlzip_probe.zip", LocationCategory::Custom);
	filesystem::remove(info);

	if (!filesystem::write(info, c.archive.data(), c.archive.size())) {
		check(false, "zip: probe archive written to disk");
		return;
	}

	// The file case is only meaningful if the bytes on disk are the bytes we meant to write - so
	// prove that before blaming anything the archive reader does with them.
	{
		auto readBack = filesystem::readIntoMemory<memory::StandardInterface>(info);
		auto prefix = test::zip::String("zip[");
		prefix.append(c.name.data(), c.name.size());
		prefix.append("/file] probe archive round-trips through the filesystem");
		check(readBack.size() == c.archive.size()
						&& (c.archive.empty()
								|| sprt::memcmp(readBack.data(), c.archive.data(), c.archive.size())
										== 0),
				prefix);
	}

	{
		ZipArchive<mem_std::Interface> archive(info);

		auto prefix = test::zip::String("zip[");
		prefix.append(c.name.data(), c.name.size());
		prefix.append("/file] opens as expected");

		check(bool(archive) == c.openable, prefix);
		if (archive) {
			verifyArchive(archive, c, "file");
		}
	}

	filesystem::remove(info);
}

// The write path has no consumer in the tree, but it shares the in-memory source with the read path
// - a commit REPLACES the buffer everything reads through. So it gets a round-trip: build an
// archive, save it, and read the result back with the same reader the rest of these tests use.
static void runWriteRoundTrip() {
	sprt::cout << "\n  -- case: write round-trip --\n";

	StringView deflated("hello from the writer, long enough to be worth compressing");
	StringView stored("stored verbatim");

	test::zip::Bytes saved;

	{
		ZipArchive<mem_std::Interface> archive(BytesView(), false);
		check(bool(archive), "zip[write] empty archive is created");
		if (!archive) {
			return;
		}

		check(archive.addDir("dir"), "zip[write] addDir()");
		check(archive.addFile("dir/hello.txt", deflated), "zip[write] addFile()");
		check(archive.addFile("stored.bin", stored, true), "zip[write] addFile() uncompressed");

		auto buf = archive.save();
		check(buf.input() > 0, "zip[write] save() produced bytes");
		saved.assign(buf.data(), buf.data() + buf.input());
	}

	ZipArchive<mem_std::Interface> readBack(BytesView(saved.data(), saved.size()), true);
	check(bool(readBack), "zip[write] the saved archive reopens");
	if (!readBack) {
		return;
	}

	bool ok = false;
	auto got = readEntry(readBack, StringView("dir/hello.txt"), ok);
	check(ok && sameBytes(got, test::zip::Bytes(deflated.data(), deflated.data() + deflated.size())),
			"zip[write] the deflated entry round-trips");

	got = readEntry(readBack, StringView("stored.bin"), ok);
	check(ok && sameBytes(got, test::zip::Bytes(stored.data(), stored.data() + stored.size())),
			"zip[write] the stored entry round-trips");

	check(readBack.locateFile("dir/hello.txt") != maxOf<uint64_t>(),
			"zip[write] locateFile() finds the written entry");
}

} // namespace

void performZipTests() {
	sprt::cout << "\n== stappler zip tests (characterization against libzip) ==\n";

	auto corpus = test::zip::buildCorpus();

	for (auto &c : corpus) {
		sprt::cout << "\n  -- case: " << c.name << " (" << c.archive.size() << " bytes) --\n";

		runFromMemory<mem_std::Interface>(c, "mem_std");
		runFromFile(c);
	}

	runWriteRoundTrip();

	// The pool interface is the one EPUB actually uses (EpubData::archive), so it gets the same
	// sweep - in its own pool, since BufferTemplate<PoolInterface> allocates from the active one.
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
