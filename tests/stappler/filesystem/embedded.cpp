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

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPFilesystemEmbedded.h"
#include "SPFilesystemMap.h"
#include "SPMemory.h"
#include "SPString.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

// The two bundles the project embeds (see tests/stappler/Makefile): the same content stored raw and
// stored compressed. `resources` is also present on disk, which is what lets every read be compared
// against the original bytes.
static constexpr auto RawBundle = "resources";
static constexpr auto PackedBundle = "resources-packed";

static mem_std::Bytes readEmbedded(StringView bundle, StringView path) {
	auto full = filepath::merge<mem_std::Interface>(bundle, path);
	return filesystem::readIntoMemory<mem_std::Interface>(
			FileInfo(StringView(full), FileCategory::Embedded));
}

static mem_std::Bytes readOnDisk(StringView path) {
	auto full = filepath::merge<mem_std::Interface>(StringView(RawBundle), path);
	return filesystem::readIntoMemory<mem_std::Interface>(
			FileInfo(StringView(full), FileCategory::Custom));
}

// Looks an entry up in a registered bundle, so a test can assert on how it was actually stored.
// Whether a given file ends up compressed is the generator's call — LZ4 is skipped when it would
// not shrink the data, and the sh fallback can not compress at all — so tests key off this rather
// than assuming.
static const filesystem::embedded::Entry *findEntry(StringView bundle, StringView path) {
	auto b = filesystem::embedded::getBundle(bundle);
	if (!b) {
		return nullptr;
	}
	for (auto &it : b->getEntries()) {
		if (it.getPath() == path) {
			return &it;
		}
	}
	return nullptr;
}

// Walks a bundle subtree and renders the result as "path:type" joined with '|', so that ordering,
// relative paths and the empty-string-is-the-root convention are all asserted at once.
static mem_std::String walk(StringView path, int depth, bool dirFirst) {
	mem_std::StringStream out;
	auto full = mem_std::String(path.data(), path.size());
	filesystem::ftw(FileInfo(StringView(full), FileCategory::Embedded),
			[&](const FileInfo &info, FileType type) {
		auto rel = info.path;
		rel += path.size();
		rel.skipChars<StringView::Chars<'/'>>();
		out << (rel.empty() ? StringView(".") : rel) << ":" << (type == FileType::Dir ? "d" : "f")
			<< "|";
		return true;
	}, depth, dirFirst);
	return out.str();
}

static void testContent(StringView bundle, StringView label) {
	// Every file must come back byte-identical to the on-disk original it was generated from
	static const StringView files[] = {
		StringView("config.json"),
		StringView("hello.txt"),
		StringView("nested/binary.dat"),
		StringView("nested/deep/leaf.txt"),
		StringView("вставленный файл.txt"),
	};

	for (auto &file : files) {
		auto embedded = readEmbedded(bundle, file);
		auto onDisk = readOnDisk(file);
		check(!onDisk.empty(), mem_std::toString(label, ": on-disk original is readable: ", file));
		check(embedded.size() == onDisk.size() && embedded == onDisk,
				mem_std::toString(label, ": content matches the original: ", file));
	}
}

// BundleFS: files embedded into the binary at build time, read through FileCategory::Embedded.
// Two bundles are linked in — one raw, one compressed — so both storage forms of identical content
// go through the same assertions.
void performEmbeddedFilesystemTests() {
	sprt::cout << "\n== stappler filesystem tests (embedded / BundleFS) ==\n";

	// --- registration ---
	check(filesystem::embedded::hasBundles(), "embed: bundles are linked into the binary");

	size_t bundleCount = 0;
	filesystem::embedded::enumerateBundles([&](const filesystem::embedded::Bundle &) {
		++bundleCount;
		return true;
	});
	check(bundleCount == 2, "embed: both declared bundles are registered");

	auto raw = filesystem::embedded::getBundle(StringView(RawBundle));
	check(raw != nullptr, "embed: getBundle finds a bundle by its mount name");
	check(filesystem::embedded::getBundle(StringView("no-such-bundle")) == nullptr,
			"embed: getBundle returns null for an unknown name");

	// --- existence ---
	check(filesystem::exists(FileInfo("resources/hello.txt", FileCategory::Embedded)),
			"embed: exists() finds an embedded file");
	check(filesystem::exists(FileInfo("resources/nested", FileCategory::Embedded)),
			"embed: exists() finds an embedded directory");
	check(filesystem::exists(FileInfo("resources", FileCategory::Embedded)),
			"embed: exists() finds the bundle root");
	check(!filesystem::exists(FileInfo("resources/nope.txt", FileCategory::Embedded)),
			"embed: exists() rejects a missing path");
	check(!filesystem::exists(FileInfo("no-such-bundle/hello.txt", FileCategory::Embedded)),
			"embed: exists() rejects an unknown bundle");

	// --- stat ---
	filesystem::Stat st;
	check(filesystem::stat(FileInfo("resources/hello.txt", FileCategory::Embedded), st),
			"embed: stat() succeeds for a file");
	check(st.type == FileType::File, "embed: stat() reports a regular file");
	check(st.size == 36, "embed: stat() reports the original size");

	filesystem::Stat dirSt;
	check(filesystem::stat(FileInfo("resources/nested", FileCategory::Embedded), dirSt),
			"embed: stat() succeeds for a directory");
	check(dirSt.type == FileType::Dir, "embed: stat() reports a directory");

	// An entry must report its ORIGINAL size, not the stored one (which differs when compressed)
	filesystem::Stat packedSt;
	check(filesystem::stat(FileInfo("resources-packed/nested/binary.dat", FileCategory::Embedded),
				  packedSt),
			"embed: stat() succeeds for a file in the packed bundle");
	check(packedSt.size == 1'024, "embed: stat() reports the uncompressed size of a packed entry");
	check(readEmbedded(StringView(PackedBundle), StringView("nested/binary.dat")).size() == 1'024,
			"embed: a packed entry reads back at its original size");

	// --- reading ---
	testContent(StringView(RawBundle), StringView("embed/raw"));
	testContent(StringView(PackedBundle), StringView("embed/packed"));

	auto text = filesystem::readTextFile<mem_std::Interface>(
			FileInfo("resources/hello.txt", FileCategory::Embedded));
	checkEq(StringView(text), StringView("Hello, embedded world!\nSecond line.\n"),
			"embed: readTextFile returns the whole file");

	// Partial read: exercises the provider's seek + bounded read
	auto part = filesystem::readIntoMemory<mem_std::Interface>(
			FileInfo("resources/hello.txt", FileCategory::Embedded), 7, 8);
	checkEq(StringView((const char *)part.data(), part.size()), StringView("embedded"),
			"embed: readIntoMemory honours offset and size");

	// A compressed entry is materialized whole on open, so the same bounds apply to the expanded
	// content — binary.dat is 0x00..0xFF four times over, so offset 512 is another 0x00
	auto packedPart = filesystem::readIntoMemory<mem_std::Interface>(
			FileInfo("resources-packed/nested/binary.dat", FileCategory::Embedded), 512, 16);
	check(packedPart.size() == 16 && packedPart.front() == 0x00 && packedPart.back() == 0x0F,
			"embed: a bounded read of a packed entry lands at the right offset");

	// Seeking past the end is legal and yields nothing
	auto tailFile =
			filesystem::openForReading(FileInfo("resources/hello.txt", FileCategory::Embedded));
	tailFile.seek(1'000, io::Seek::Set);
	uint8_t tailBuf[4] = {0};
	check(tailFile.read(tailBuf, sizeof(tailBuf)) == 0, "embed: a read past the end returns 0");
	tailFile.close();

	// A directory is not readable as a file
	check(filesystem::openForReading(FileInfo("resources/nested", FileCategory::Embedded)).is_open()
					== false,
			"embed: a directory can not be opened for reading");

	// --- enumeration ---
	checkEq(StringView(walk(StringView("resources/nested"), -1, true)),
			StringView(".:d|binary.dat:f|deep:d|deep/leaf.txt:f|"),
			"embed: ftw pre-order reports the root first, then the subtree in order");

	checkEq(StringView(walk(StringView("resources/nested"), -1, false)),
			StringView("binary.dat:f|deep/leaf.txt:f|deep:d|.:d|"),
			"embed: ftw post-order reports a directory after its contents");

	checkEq(StringView(walk(StringView("resources/nested"), 1, true)),
			StringView(".:d|binary.dat:f|deep:d|"),
			"embed: ftw depth == 1 reports a subdirectory without descending");

	checkEq(StringView(walk(StringView("resources"), -1, true)),
			StringView(".:d|config.json:f|empty:d|hello.txt:f|nested:d|nested/binary.dat:f|" "neste"
																							 "d/" "deep:d|" "nested/" "deep/" "leaf." "txt:f|" "вставлен" "ный " "файл." "txt:f|"),
			"embed: ftw of the bundle root walks everything, empty directories included");

	// Early exit: returning false from the callback must stop the walk
	size_t visited = 0;
	check(!filesystem::ftw(FileInfo("resources", FileCategory::Embedded),
				  [&](const FileInfo &, FileType) {
		++visited;
		return visited < 3;
	}, -1, true),
			"embed: ftw reports failure when the callback aborts the walk");
	check(visited == 3, "embed: ftw stops as soon as the callback returns false");

	// --- read-only ---
	check(!filesystem::remove(FileInfo("resources/hello.txt", FileCategory::Embedded)),
			"embed: remove() refuses an embedded file");
	check(!filesystem::touch(FileInfo("resources/new.txt", FileCategory::Embedded)),
			"embed: touch() refuses an embedded path");
	check(!filesystem::write(FileInfo("resources/hello.txt", FileCategory::Embedded),
				  (const uint8_t *)"x", 1),
			"embed: write() refuses an embedded file");
	check(filesystem::exists(FileInfo("resources/hello.txt", FileCategory::Embedded)),
			"embed: the file survived the write attempts");

	// --- %EMBEDDED%: prefix ---
	auto prefixed = mem_std::String("%EMBEDDED%:resources/hello.txt");
	FileInfo byPrefix{StringView(prefixed)};
	check(byPrefix.category == FileCategory::Embedded,
			"embed: the %EMBEDDED%: prefix selects the category");
	check(filesystem::exists(byPrefix), "embed: a %EMBEDDED%:-prefixed path resolves");


	// --- mapping: a raw entry is mapped straight out of .rodata ---
	auto region = filesystem::MemoryMappedRegion::mapFile(
			FileInfo("resources/hello.txt", FileCategory::Embedded),
			filesystem::MappingType::Private, filesystem::ProtFlags::MapRead);
	check(region.getRegion() != nullptr, "embed: a raw entry can be memory-mapped");
	if (region.getRegion()) {
		checkEq(StringView((const char *)region.getRegion(), 5), StringView("Hello"),
				"embed: the mapped region holds the file's bytes");
	}

	// A compressed entry has no contiguous image to point at, so mapping must fail rather than
	// hand back the compressed bytes. When the generator stored it raw (nothing to gain, or the
	// sh fallback, which can not compress), it is mappable like any other raw entry.
	auto packedEntry = findEntry(StringView(PackedBundle), StringView("nested/binary.dat"));
	check(packedEntry != nullptr, "embed: the packed bundle has the expected entry");

	auto packedRegion = filesystem::MemoryMappedRegion::mapFile(
			FileInfo("resources-packed/nested/binary.dat", FileCategory::Embedded),
			filesystem::MappingType::Private, filesystem::ProtFlags::MapRead);
	if (packedEntry && packedEntry->isCompressed()) {
		check(packedRegion.getRegion() == nullptr, "embed: a compressed entry can not be mapped");
	} else {
		sprt::cout << "    (bundle written uncompressed by this generator)\n";
		check(packedRegion.getRegion() != nullptr,
				"embed: an entry stored raw can be mapped even in a compressed bundle");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
