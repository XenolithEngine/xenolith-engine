/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Journal compression: the blob log's new arithmetic, and the measurement that decides the default.
//
// TWO WORKLOADS, because one gives a misleading verdict. W1 is arena metadata - Root, the descriptor
// table, near-identical block headers, long runs of zeros - and compresses very well. W2 writes
// test::fillPattern, a multiply-xor-shift hash, so its payload bytes are effectively random and
// INCOMPRESSIBLE BY CONSTRUCTION. W2 is a floor, not a typical case; real data lies between
// the two. Reporting only W1 would recommend the heaviest codec and be wrong; only W2 would
// recommend none and be wrong.
//
// THE WRITE TIMER COVERS THE MUTATIONS, NOT JUST commit(). Compression is paid inside the write
// barrier, at announcement time, while commit() only writes a Mark and clears the dirty map - so
// timing commit() alone would show compression as free. Do not "simplify" that back.
//
// This section is a stopwatch, not a correctness check: byte-exactness under every codec is
// journal-rollback's job, and duplicating its image comparison here would drag takeImage()'s
// allocations into the measured region.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreJournal.h"

#include "../tests.h"
#include "../check/vstore_check.h"

#include <sprt/c/__sprt_time.h>

namespace STAPPLER_VERSIONIZED stappler {

// This whole section is ABOUT versioning, so without it there is nothing here to test and
// nothing here that would compile. The section keeps its place in the list and says so.

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace vstore;

// Test-local on purpose. Accumulated around whole loops, never per call: a clock read is ~20-25 ns
// and a serialization point, while one small commit() is microseconds.
struct Stopwatch {
	uint64_t start = 0;
	uint64_t total = 0;

	void begin() { start = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC); }
	void end() { total += __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC) - start; }
};

struct Result {
	size_t journalBytes = 0;
	size_t keyframeBytes = 0;
	uint64_t writeNs = 0;
	uint64_t rollbackNs = 0;
	uint64_t versions = 0;
	uint64_t rollbacks = 0;
	uint64_t compressedPages = 0;
	uint64_t rawPages = 0;
	uint64_t payloadBytes = 0;
};

// One version of W1: ten small appends. Every version touches Root, the descriptor table and the
// new blocks' headers, so the pages captured are almost all allocator metadata.
void stepMetadata(Arena &arena, Lcg &, mem_std::Vector<Addr> &live) {
	for (int i = 0; i < 10; ++i) {
		Addr a = arena.alloc(64);
		if (a == NullAddr) {
			return;
		}
		test::fillPattern(arena.write(a, 64), 64, a);
		live.emplace_back(a);
	}
}

// One version of W2: the mixed alloc/free/realloc workload from journal-rollback, whose payloads
// are pseudorandom.
void stepPayload(Arena &arena, Lcg &lcg, mem_std::Vector<Addr> &live) {
	for (int op = 0; op < 20; ++op) {
		auto roll = lcg.next(100);
		if (roll < 50 || live.empty()) {
			uint32_t size = 1 + lcg.next(1500);
			if (lcg.next(64) == 0) {
				size = MaxInChunkPayload + 1 + lcg.next(120000);
			}
			Addr a = arena.alloc(size);
			if (a == NullAddr) {
				return;
			}
			test::fillPattern(arena.write(a, arena.sizeOf(a)), arena.sizeOf(a), a);
			live.emplace_back(a);
		} else if (roll < 80) {
			size_t i = lcg.next(uint32_t(live.size()));
			arena.free(live[i]);
			live[i] = live.back();
			live.pop_back();
		} else {
			size_t i = lcg.next(uint32_t(live.size()));
			Addr moved = arena.realloc(live[i], 1 + lcg.next(2000));
			if (moved == NullAddr) {
				return;
			}
			test::fillPattern(arena.write(moved, arena.sizeOf(moved)), arena.sizeOf(moved), moved);
			live[i] = moved;
		}
	}
}

using StepFn = void (*)(Arena &, Lcg &, mem_std::Vector<Addr> &);

Result runWorkload(JournalCodec codec, StepFn step, uint32_t versionCount, uint64_t seed) {
	Result r;

	Config storeCfg;
	// The two debug costs that would otherwise swamp the numbers: the shadow validator maintains a
	// byte copy of the whole store on every barrier call, and the rollback assert sweeps the entire
	// store structurally after each rollback.
	storeCfg.debugShadow = false;
	storeCfg.debugVerify = false;

	Arena arena;
	if (!arena.init(storeCfg)) {
		return r;
	}

	Journal::Config cfg;
	cfg.codec = codec;
	Journal journal;
	if (!journal.init(cfg)) {
		return r;
	}
	journal.attach(&arena);

	// Same seed for every codec, so all of them see a bit-identical operation stream - that is what
	// makes the byte counts comparable rather than merely similar.
	Lcg lcg(seed);
	mem_std::Vector<Addr> live;
	mem_std::Vector<Version> versions;

	Stopwatch write;
	for (uint32_t v = 0; v < versionCount; ++v) {
		write.begin();
		step(arena, lcg, live);
		versions.emplace_back(journal.commit());
		write.end();
	}
	r.writeNs = write.total;
	r.versions = versions.size();
	r.journalBytes = journal.getJournalBytes();

	if (journal.setKeyframe() == Status::Ok) {
		r.keyframeBytes = journal.getKeyframeBytes();
	}

	// Targets drawn from their own generator, so changing the workload cannot silently reshuffle
	// them, and replayed strictly descending: a rollback truncates the timeline, so a target above
	// one already restored no longer exists. journal-rollback owns random order; this owns the clock.
	mem_std::Vector<Version> targets;
	{
		Lcg pick(seed ^ 0xf00d'00000000'0000ull);
		size_t at = versions.size();
		while (at > 1 && targets.size() < 25) {
			at -= 1 + pick.next(uint32_t(at > 8 ? 8 : at));
			targets.emplace_back(versions[at]);
		}
	}

	Stopwatch back;
	for (auto v : targets) {
		back.begin();
		auto st = journal.rollback(v);
		back.end();
		if (st != Status::Ok) {
			break;
		}
		++r.rollbacks;
	}
	r.rollbackNs = back.total;

	auto &m = journal.getMetrics();
	r.compressedPages = m.compressedPages;
	r.rawPages = m.rawPages;
	r.payloadBytes = m.payloadBytes;
	return r;
}

void reportWorkload(StringView name, StringView note, StepFn step, uint32_t versionCount,
		uint64_t seed) {
	sprt::cout << "\n       -- " << name << ": " << note << " --\n";
	sprt::cout << "       codec     jrnl KiB    of none    kf KiB   write us/v   back us/rb"
				  "   comp%   mean B\n";

	size_t baseline = 0;
	for (uint32_t i = 0; i < JournalCodecCount; ++i) {
		auto codec = JournalCodec(i);
		if (!isCodecAvailable(codec)) {
			continue;
		}

		auto r = runWorkload(codec, step, versionCount, seed);
		if (codec == JournalCodec::None) {
			baseline = r.journalBytes;
		}

		// Whole percents and whole microseconds: the differences that matter here are factors, and
		// printing a double's full precision only makes the table unreadable.
		uint64_t pages = r.compressedPages + r.rawPages;
		auto pad = [](StringView s, size_t width) {
			for (size_t p = s.size(); p < width; ++p) { sprt::cout << " "; }
		};

		sprt::cout << "       " << getCodecName(codec);
		pad(getCodecName(codec), 10);
		sprt::cout << (r.journalBytes >> 10) << "\t  "
				   << (baseline ? r.journalBytes * 100 / baseline : 100) << "%\t    "
				   << (r.keyframeBytes >> 10) << "\t  "
				   << (r.versions ? r.writeNs / 1000 / r.versions : 0) << "\t       "
				   << (r.rollbacks ? r.rollbackNs / 1000 / r.rollbacks : 0) << "\t     "
				   << (pages ? r.compressedPages * 100 / pages : 0) << "%\t "
				   << (pages ? r.payloadBytes / pages : 0) << "\n";

		check(r.versions == versionCount,
				mem_std::toString("journal-compress[", getCodecName(codec), "]: ", name,
						" ran every version"));
		check(r.rollbacks > 0,
				mem_std::toString("journal-compress[", getCodecName(codec), "]: ", name,
						" rolled back"));
	}
}

// The blob log's straddle/truncate/evict arithmetic is the genuinely new code in this change, and
// nothing else exercises it directly - the journal only ever appends whole pages.
void performBlobLogFixture() {
	auto pool = memory::pool::create();
	check(pool != nullptr, "journal-compress: blob fixture pool");

	JournalBlobLog log;
	log.init(pool);

	mem_std::Vector<uint8_t> scratch(PageSize, 0);
	mem_std::Vector<uint32_t> refs;
	mem_std::Vector<uint32_t> sizes;

	// Enough payloads of assorted sizes to cross a segment boundary several times over.
	Lcg lcg(0xb10b'00000000'0001ull);
	bool readBack = true;
	for (uint32_t i = 0; i < 1200; ++i) {
		uint32_t size = 1 + lcg.next(PageSize);
		for (uint32_t b = 0; b < size; ++b) { scratch[b] = uint8_t(i * 31 + b); }

		auto ref = log.append(scratch.data(), size);
		refs.emplace_back(ref);
		sizes.emplace_back(size);
	}

	for (uint32_t i = 0; i < refs.size() && readBack; ++i) {
		auto data = log.getBlob(refs[i], sizes[i]);
		if (!data) {
			readBack = false;
			break;
		}
		for (uint32_t b = 0; b < sizes[i]; ++b) {
			if (data[b] != uint8_t(i * 31 + b)) {
				readBack = false;
				break;
			}
		}
	}
	check(readBack, "journal-compress: every variable-sized payload reads back across segments");
	check(log.getSegmentCount() > 1, "journal-compress: the fixture actually spanned segments");
	check(log.getBytes() == size_t(log.getEnd() - log.getBase()) * Granule,
			"journal-compress: the byte count is the granule span, skipped tails included");

	// Truncating back into an earlier segment and refilling must reuse what is already there.
	auto segments = log.getSegmentCount();
	auto rewind = refs[refs.size() / 4];
	log.truncate(rewind);
	check(log.getEnd() == rewind, "journal-compress: truncate rewinds to the given reference");
	for (uint32_t i = 0; i < 200; ++i) { log.append(scratch.data(), PageSize); }
	check(log.getSegmentCount() <= segments,
			"journal-compress: refilling after a truncate allocates no new segment");

	// Front eviction recycles whole segments rather than dropping them: a pool never gives an
	// individual block back, so a log that leaked them would make recordBudget unenforceable.
	auto liveSegments = log.getSegmentCount();
	log.evictTo(log.getBase() + JournalBlobLog::GranulesPerSegment * 2);
	check(log.getFreeSegmentCount() > 0,
			"journal-compress: eviction moved segments to the free list");

	auto freed = log.getFreeSegmentCount();
	for (uint32_t i = 0; i < JournalBlobLog::GranulesPerSegment / (PageSize / Granule); ++i) {
		log.append(scratch.data(), PageSize);
	}
	check(log.getFreeSegmentCount() < freed || log.getSegmentCount() <= liveSegments,
			"journal-compress: and the next appends consumed a recycled segment");

	memory::pool::destroy(pool);
}

} // namespace

void performJournalCompressTests() {
	sprt::cout << "\n== vstore: journal compression and cost ==\n";

	performBlobLogFixture();

	for (uint32_t i = 0; i < JournalCodecCount; ++i) {
		auto codec = JournalCodec(i);
		if (!isCodecAvailable(codec)) {
			sprt::cout << "       -- " << getCodecName(codec) << ": not built in, skipped\n";
		}
	}

#if DEBUG
	sprt::cout << "       (debug build: absolute times are inflated; only the comparison between\n"
				  "        codecs is meaningful - rebuild tests/vstore in release)\n";
#endif

	// This section IS the codec table, so the codec axis is the one thing a reduced run may not
	// touch - a table with one row says nothing about a ratio. What it shortens is the timeline
	// both workloads run for. The ratios stay comparable because every codec still sees the same
	// bit-identical operation stream; only the absolute KiB shrink with it, and those are already
	// documented as figures of the workload rather than of the journal.
	const uint32_t w1 = test::sized(200, 60);
	const uint32_t w2 = test::sized(500, 120);
	reportWorkload("W1 metadata", "versions x 10 x alloc(64), highly compressible", &stepMetadata,
			w1, 0xc057'00000000'0003ull);
	reportWorkload("W2 payload", "versions x 20 mixed ops, INCOMPRESSIBLE BY CONSTRUCTION",
			&stepPayload, w2, 0x5eed'1234'abcd'0001ull);
}


} // namespace STAPPLER_VERSIONIZED stappler
