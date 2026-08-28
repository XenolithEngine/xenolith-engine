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

/* Stage 5 of docs/design/libzip-removal-plan.adoc: fuzzing the engine's own ZIP reader.
 *
 * This is the one item on the plan's risk table rated HIGH - parsing an untrusted binary format is
 * now the engine's own problem - so it is a separate stage precisely so that it cannot be quietly
 * skipped. There is nothing to assert about the OUTPUT here: a mutated archive may legitimately
 * parse, or legitimately be refused. The property under test is that neither ever corrupts memory,
 * hangs, or allocates without bound.
 *
 * Deterministic by construction: a fixed seed and a xorshift PRNG, so a failing iteration is
 * identified by (seed, iteration) and reproduced exactly by rerunning. That is what lets a finding
 * become a regression case instead of a story about something that happened once.
 *
 * Run it under a sanitizer for the checks that matter - a plain run only catches what crashes on
 * its own, which for a parser is the small minority of what can go wrong:
 *
 *     make -C tests/stappler ASAN=1 -j8
 *
 *     # the build links -shared-libasan, so the runtime has to be findable
 *     ASAN_LIB=runtime/toolchains/targets/x86_64-unknown-linux-gnu/lib/clang/lib/linux
 *     env LD_LIBRARY_PATH=$ASAN_LIB ASAN_OPTIONS=detect_leaks=0 \
 *         XL_ZIP_FUZZ_ITERATIONS=5000000 <binary> zipfuzz
 *
 * detect_leaks=0 only because the crypto backend leaks its one-time init allocation at exit
 * (SPCrypto-openssl.cc), which has nothing to do with this and drowns the output.
 *
 * The in-suite iteration count is deliberately small so the ordinary test run stays fast; the soak
 * is a separate, deliberate act.
 */

#include "SPCommon.h"
#include "SPZipReader.h"
#include "SPMemory.h"

#include "corpus.h"

#include "../tests.h"

#include <stdlib.h>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

// How many mutated archives the ordinary test run tries. Enough to exercise every mutation kind
// against every seed several times over; a real soak is driven by the environment variable.
static constexpr uint64_t ZIP_FUZZ_DEFAULT_ITERATIONS = 20'000;

// xorshift64*: small, fast, and - the point here - reproducible from its seed on every platform.
struct Random {
	uint64_t state;

	explicit Random(uint64_t seed) : state(seed ? seed : 0x9E37'79B9'7F4A'7C15ull) { }

	uint64_t next() {
		state ^= state >> 12;
		state ^= state << 25;
		state ^= state >> 27;
		return state * 0x2545'F491'4F6C'DD1Dull;
	}

	uint64_t below(uint64_t bound) { return bound == 0 ? 0 : next() % bound; }
};

/* The mutation kinds.
 *
 * The first three are ordinary noise. The rest are aimed: they hit the fields the reader does
 * arithmetic on, because that is where a parser of this shape goes wrong - a plain bit-flipper
 * spends almost all its time in payload bytes, which are the least interesting part.
 */
enum class Mutation {
	FlipBits,
	SetByte,
	Truncate,
	Extend,
	Splice,
	CorruptTail, // the end records: EOCD, the ZIP64 pair, the comment length
	CorruptSizes, // the 32-bit size and offset fields, wherever they happen to land
	Count,
};

static void mutate(test::zip::Bytes &data, Random &rnd) {
	if (data.empty()) {
		return;
	}

	switch (Mutation(rnd.below(uint64_t(Mutation::Count)))) {
	case Mutation::FlipBits: {
		auto count = 1 + rnd.below(8);
		for (uint64_t i = 0; i < count; ++i) {
			data[size_t(rnd.below(data.size()))] ^= uint8_t(1u << rnd.below(8));
		}
		break;
	}

	case Mutation::SetByte: {
		auto count = 1 + rnd.below(4);
		for (uint64_t i = 0; i < count; ++i) {
			data[size_t(rnd.below(data.size()))] = uint8_t(rnd.below(256));
		}
		break;
	}

	case Mutation::Truncate: data.resize(size_t(rnd.below(data.size()))); break;

	case Mutation::Extend: {
		auto extra = 1 + rnd.below(64);
		for (uint64_t i = 0; i < extra; ++i) { data.emplace_back(uint8_t(rnd.below(256))); }
		break;
	}

	case Mutation::Splice: {
		// Move a run of bytes somewhere else, which desynchronizes records rather than merely
		// damaging one.
		auto length = 1 + rnd.below(data.size() < 32 ? data.size() : 32);
		auto from = rnd.below(data.size() - length + 1);
		auto to = rnd.below(data.size() - length + 1);
		for (uint64_t i = 0; i < length; ++i) {
			data[size_t(to + i)] = data[size_t(from + i)];
		}
		break;
	}

	case Mutation::CorruptTail: {
		// The last 128 bytes hold the EOCD and, when present, the ZIP64 records - every number that
		// decides where the reader looks next.
		auto window = data.size() < 128 ? data.size() : 128;
		auto pos = data.size() - window + rnd.below(window);
		data[size_t(pos)] = uint8_t(rnd.below(256));
		break;
	}

	case Mutation::CorruptSizes: {
		// Overwrite an aligned 32-bit word with a hostile value: the sentinels, values just past
		// the end of the file, and the extremes are what offset arithmetic has to survive.
		static const uint32_t hostile[] = {0xFFFF'FFFFu, 0xFFFF'FFFEu, 0x8000'0000u, 0x7FFF'FFFFu,
			0u, 1u, 0xFFFF'0000u};

		if (data.size() < 4) {
			break;
		}
		auto pos = size_t(rnd.below(data.size() - 3));
		auto value = hostile[rnd.below(sizeof(hostile) / sizeof(hostile[0]))];
		data[pos] = uint8_t(value & 0xFF);
		data[pos + 1] = uint8_t((value >> 8) & 0xFF);
		data[pos + 2] = uint8_t((value >> 16) & 0xFF);
		data[pos + 3] = uint8_t((value >> 24) & 0xFF);
		break;
	}

	case Mutation::Count: break;
	}
}

/* How far the mutated inputs actually got.
 *
 * A fuzz run that reports "no crashes" is worth exactly as much as the code it reached, and a
 * mutator that only ever produces buffers rejected at the first check would report that happily
 * while testing nothing. These counters are the run's own evidence that it exercised the parser
 * rather than its front door, and they are printed whether or not anything failed.
 */
struct Coverage {
	uint64_t parsed = 0;
	uint64_t entriesSeen = 0;
	uint64_t readOk = 0;
	uint64_t readRefused = 0;
};

/* One iteration: parse the buffer as an archive and read everything it claims to contain.
 *
 * Success and failure are equally valid outcomes - what is being tested is that getting here and
 * back is safe, which the sanitizer, the allocator and the clock decide, not this function.
 */
static void runOnce(BytesView data, Coverage &cov) {
	ZipSource source;
	source.setMemory(data);

	ZipCatalog<mem_std::Interface> catalog;
	if (!sprt::status::isSuccessful(catalog.read(source))) {
		return;
	}

	++cov.parsed;

	for (size_t i = 0; i < catalog.size(); ++i) {
		auto entry = catalog.entry(i);
		if (!entry) {
			continue;
		}

		++cov.entriesSeen;

		mem_std::Interface::BytesType content;
		if (sprt::status::isSuccessful(
					zipReadEntry<mem_std::Interface>(source, *entry, catalog.prefix(), content))) {
			++cov.readOk;
		} else {
			++cov.readRefused;
		}
	}
}

static uint64_t iterationCount() {
	if (auto env = ::getenv("XL_ZIP_FUZZ_ITERATIONS")) {
		auto parsed = StringView(env).readInteger(10).get(0);
		if (parsed > 0) {
			return uint64_t(parsed);
		}
	}
	return ZIP_FUZZ_DEFAULT_ITERATIONS;
}

} // namespace

void performZipFuzzTests() {
	sprt::cout << "\n== stappler zip fuzzing (the engine's own reader) ==\n";

	auto corpus = test::zip::buildCorpus();
	auto iterations = iterationCount();

	sprt::cout << "  seeds: " << corpus.size() << ", iterations: " << iterations << "\n";

	Coverage seedCov;

	// Every corpus archive is first run UNMUTATED, so that a crash on well-formed input is
	// attributed to the reader rather than blamed on the mutator.
	for (auto &c : corpus) { runOnce(BytesView(c.archive.data(), c.archive.size()), seedCov); }

	Random rnd(0x5A17'2E9D'0F31'C4B7ull);

	Coverage cov;
	test::zip::Bytes buffer;
	for (uint64_t i = 0; i < iterations; ++i) {
		auto &seed = corpus[size_t(rnd.below(corpus.size()))];

		buffer.assign(seed.archive.begin(), seed.archive.end());

		// Stacked mutations: one edit usually leaves a structure that fails the first check, and the
		// interesting states are a few edits deep.
		auto rounds = 1 + rnd.below(3);
		for (uint64_t r = 0; r < rounds; ++r) { mutate(buffer, rnd); }

		runOnce(BytesView(buffer.data(), buffer.size()), cov);
	}

	/* The second phase, and the one the plan actually names: an ARBITRARY buffer.
	 *
	 * Almost none of these will look like an archive, so this reaches far less code than the
	 * mutation phase - but it is the input a caller can genuinely be handed (a truncated download,
	 * a file that is not a ZIP at all), and it costs a fraction of the run.
	 */
	Coverage randomCov;
	auto randomIterations = iterations / 4;
	for (uint64_t i = 0; i < randomIterations; ++i) {
		buffer.clear();
		auto length = rnd.below(4'096);
		for (uint64_t b = 0; b < length; ++b) { buffer.emplace_back(uint8_t(rnd.below(256))); }

		runOnce(BytesView(buffer.data(), buffer.size()), randomCov);
	}

	sprt::cout << "  reached: " << cov.parsed << " archives parsed, " << cov.entriesSeen
			   << " entries decoded, " << cov.readOk << " read, " << cov.readRefused
			   << " refused\n";
	sprt::cout << "  random buffers: " << randomIterations << " tried, " << randomCov.parsed
			   << " parsed\n";

	// Reaching this line IS the result: no crash, no hang, no unbounded allocation across every
	// input tried. Under a sanitizer it also means no out-of-bounds access.
	check(true, "zipfuzz: the reader survived every mutated archive");

	// ...but only if the run got past the front door. A mutator that produced nothing but buffers
	// rejected at the first check would otherwise report success while testing the EOCD scan and
	// nothing else, and that failure mode is silent by nature.
	check(cov.parsed * 100 >= iterations, "zipfuzz: at least 1% of mutants parsed as archives");
	check(cov.readOk > 0 && cov.readRefused > 0,
			"zipfuzz: entry reading was exercised on both paths");
}

} // namespace stappler
