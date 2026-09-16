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

// io::Producer is a type-erased seam: whoever holds one cannot tell whether the bytes behind it
// live in memory (CoderSource) or in a file (filesystem::File). That only works if both
// implementations agree on what seek() means, and for Seek::End they did not - CoderSource treated
// the offset as absolute from the START, so seek(0, End) reported 0 instead of the size, while
// filesystem::File followed POSIX lseek.
//
// These checks pin the shared contract down on both implementations at once, and through the
// Producer seam, so the two cannot drift apart again.

#include "SPCommon.h"
#include "SPCoreCrypto.h"
#include "SPIO.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

// 16 bytes, distinguishable so a wrong position shows up as wrong content
static constexpr StringView SAMPLE("0123456789abcdef");

// The contract every seekable io source must honour. Templated on the source so the very same
// assertions run against CoderSource and filesystem::File.
template <typename Source>
static void checkSeekContract(Source &src, size_t size, StringView label) {
	auto named = [&](StringView what) {
		mem_std::String s;
		s.append(label.data(), label.size());
		s.append(": ");
		s.append(what.data(), what.size());
		return s;
	};

	check(src.seek(0, io::Seek::Set) == 0, named("seek(0, Set) is the start"));
	check(src.tell() == 0, named("tell() after seek to start"));

	// the case that was broken: End with a zero offset is the end, i.e. the size
	check(src.seek(0, io::Seek::End) == size, named("seek(0, End) reports the size"));
	check(src.tell() == size, named("tell() after seek to end"));

	check(src.seek(-4, io::Seek::End) == size - 4, named("seek(-4, End) counts back from the end"));

	check(src.seek(4, io::Seek::Set) == 4, named("seek(4, Set) is absolute"));
	check(src.seek(2, io::Seek::Current) == 6, named("seek(2, Current) is relative"));
	check(src.seek(-3, io::Seek::Current) == 3, named("seek(-3, Current) moves back"));

	// reading from where the last seek left off must yield the bytes at that offset
	uint8_t buf[4] = {0};
	check(src.seek(10, io::Seek::Set) == 10, named("seek(10, Set)"));
	check(src.read(buf, 4) == 4, named("read() after seek returns the full request"));
	check(sprt::memcmp(buf, SAMPLE.data() + 10, 4) == 0, named("read() lands on the sought bytes"));
}

// The same contract as seen through the type-erased seam.
static void checkProducerContract(const io::Producer &p, size_t size, StringView label) {
	auto named = [&](StringView what) {
		mem_std::String s;
		s.append(label.data(), label.size());
		s.append(": ");
		s.append(what.data(), what.size());
		return s;
	};

	check(p.seek(0, io::Seek::End) == size, named("Producer seek(0, End) reports the size"));
	check(p.seek(-4, io::Seek::End) == size - 4, named("Producer seek(-4, End) counts back"));

	uint8_t buf[4] = {0};
	check(p.seekAndRead(10, buf, 4) == 4, named("Producer seekAndRead() reads"));
	check(sprt::memcmp(buf, SAMPLE.data() + 10, 4) == 0,
			named("Producer seekAndRead() lands on the right bytes"));
}

} // namespace

void performIoSourceTests() {
	sprt::cout << "\n== stappler io source tests (seek contract) ==\n";

	{
		CoderSource src(SAMPLE);
		checkSeekContract(src, SAMPLE.size(), "CoderSource");
	}

	{
		CoderSource src(SAMPLE);
		io::Producer p(src);
		checkProducerContract(p, SAMPLE.size(), "CoderSource");
	}

	FileInfo info("xlio_probe.bin", LocationCategory::Custom);
	filesystem::remove(info);

	if (!filesystem::write(info, (const uint8_t *)SAMPLE.data(), SAMPLE.size())) {
		check(false, "io: probe file written to disk");
		return;
	}

	{
		auto f = filesystem::openForReading(info);
		if (f) {
			checkSeekContract(f, SAMPLE.size(), "filesystem::File");
		} else {
			check(false, "io: probe file opened for reading");
		}
	}

	{
		auto f = filesystem::openForReading(info);
		if (f) {
			io::Producer p(f);
			checkProducerContract(p, SAMPLE.size(), "filesystem::File");
		} else {
			check(false, "io: probe file opened for reading (producer)");
		}
	}

	// Note a deliberate NON-contract: seeking past the end. CoderSource clamps to the size, while
	// filesystem::File follows POSIX and allows a position beyond EOF. Nothing in the engine relies
	// on either, so it is left alone rather than harmonized on a guess.

	filesystem::remove(info);
}

} // namespace stappler
