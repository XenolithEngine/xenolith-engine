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

// Rolling back to any committed version restores the store byte for byte.
//
// "Byte for byte" means the save() image, which is the only definition of a store's content the
// layer recognises. Every assertion here compares images against references captured at the time,
// because anything weaker - "the blocks read back", "verify() passes" - misses exactly the failures
// that matter: a slot re-hosted to memory holding someone else's bytes, a run whose slots stopped
// being contiguous, a page restored from the wrong version.
//
// The scripted fixtures come first. Each one targets a specific way the three-phase rollback can go
// wrong, and each one is a case the random workload would hit only by luck.
//
// The whole section runs once per AVAILABLE codec. That is not redundancy: byte-exactness under a
// codec rests on framing - the stored size, the destination capacity, the absence of a phantom size
// prefix - and an image comparison is the only check that catches a framing mistake. Compression is
// also the one thing that can make a payload's stored form differ from its page, so every fixture
// that reasons about "the bytes come back" has to be re-run to still mean anything.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreJournal.h"

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

// This whole section is ABOUT versioning, so without it there is nothing here to test and
// nothing here that would compile. The section keeps its place in the list and says so.

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace vstore;

mem_std::Vector<uint8_t> takeImage(const Arena &arena) {
	mem_std::Vector<uint8_t> image;
	image.reserve(arena.saveSize());
	arena.save([&](const uint8_t *data, size_t size, bool zero) {
		if (zero) {
			image.resize(image.size() + size, 0);
		} else {
			image.insert(image.end(), data, data + size);
		}
	});
	return image;
}

bool imagesMatch(const Arena &arena, const mem_std::Vector<uint8_t> &expect, StringView label) {
	auto now = takeImage(arena);
	return test::compareBytes(BytesView(now.data(), now.size()),
			BytesView(expect.data(), expect.size()), label);
}

// The delta policy this pass is running under. File-scope rather than threaded through every
// fixture, because it is not an input to any of them: each one would do nothing with it but hand it
// straight back to config(), and a parameter that is only forwarded says it was read.
//
// The codec stays a parameter because it was one, and because the two are not symmetric - a fixture
// may reasonably come to care which codec it ran under, and none of them can care about this.
DeltaPolicy s_delta = DeltaPolicy::Smallest;

// Every assertion carries the arm it ran under, so a failure names the configuration that broke
// rather than the last one that happened to run.
mem_std::String tag(JournalCodec codec, StringView what) {
	return mem_std::toString("journal-rollback[", getCodecName(codec), "/",
			getDeltaPolicyName(s_delta), "]: ", what);
}

Journal::Config config(JournalCodec codec) {
	Journal::Config cfg;
	cfg.codec = codec;
	cfg.delta = s_delta;
	return cfg;
}

} // namespace

// A run is released, and its slots are taken again in a DIFFERENT shape. Rolling back has to put
// the original run back - which means re-hosting it onto contiguous memory. A rollback that only
// restored descriptors would leave a three-chunk block hosted across two unrelated allocations, and
// the first read of it would run off the end of one.
static void performRunReshape(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "reshape store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "reshape journal init"));
	journal.attach(&arena);

	uint32_t size = 3 * ChunkSize - 4096;
	Addr big = arena.alloc(size);
	check(big != NullAddr, tag(codec, "reshape fixture allocated"));
	test::fillPattern(arena.write(big, size), size, big);

	auto reference = takeImage(arena);
	auto target = journal.commit();

	// Take the same slots back in a different shape: a two-chunk run plus a single chunk.
	arena.free(big);
	Addr pair = arena.alloc(2 * ChunkSize - 500);
	Addr single = arena.alloc(MaxInChunkPayload);
	check(pair != NullAddr && single != NullAddr, tag(codec, "reshape allocations succeed"));
	journal.commit();

	check(journal.rollback(target) == Status::Ok, tag(codec, "reshape rolls back"));
	check(arena.verify() == Status::Ok, tag(codec, "the restored run is contiguous"));
	check(imagesMatch(arena, reference, "reshape"), tag(codec, "reshape restores the image"));

	// Read the whole payload back: if the run were hosted across two allocations this would walk
	// off the end of the first one.
	check(test::checkPattern(arena.read(big, size), size, big),
			tag(codec, "the whole oversize payload reads back after the reshape"));
}

// Phase 2 must carry a slot's bytes to its new home itself. If it relied on the record set covering
// every page of a re-hosted slot, a workload that re-hosts a slot it did not fully rewrite would
// come back with garbage.
static void performRehostPreserves(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "preserve store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "preserve journal init"));
	journal.attach(&arena);

	// Two runs, so that releasing one and taking the other's shape forces a re-host.
	uint32_t size = 2 * ChunkSize - 200;
	Addr a = arena.alloc(size);
	Addr b = arena.alloc(size);
	test::fillPattern(arena.write(a, size), size, a);
	test::fillPattern(arena.write(b, size), size, b);

	auto reference = takeImage(arena);
	auto target = journal.commit();

	arena.free(a);
	arena.free(b);
	Addr wide = arena.alloc(4 * ChunkSize - 100);
	check(wide != NullAddr, tag(codec, "preserve reshape allocated"));
	test::fillPattern(arena.write(wide, 4 * ChunkSize - 100), 4 * ChunkSize - 100, wide);
	journal.commit();

	check(journal.rollback(target) == Status::Ok, tag(codec, "preserve rolls back"));
	check(arena.verify() == Status::Ok, tag(codec, "preserve verifies"));
	check(imagesMatch(arena, reference, "preserve"),
			tag(codec, "both runs come back byte-identical"));
	check(test::checkPattern(arena.read(a, size), size, a)
					&& test::checkPattern(arena.read(b, size), size, b),
			tag(codec, "and both payloads read back"));
}

// The oldest record for a page holds the target version's content. Writing the same page in three
// consecutive versions and rolling back to the first is the smallest case that tells oldest-wins
// from newest-wins - they differ by exactly one version.
static void performOldestWins(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "oldest store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "oldest journal init"));
	journal.attach(&arena);

	Addr a = arena.alloc(512);
	test::fillPattern(arena.write(a, 512), 512, a + 1);
	auto first = takeImage(arena);
	auto v1 = journal.commit();

	test::fillPattern(arena.write(a, 512), 512, a + 2);
	auto second = takeImage(arena);
	journal.commit();

	test::fillPattern(arena.write(a, 512), 512, a + 3);
	journal.commit();

	check(journal.rollback(v1) == Status::Ok, tag(codec, "oldest rolls back"));
	check(imagesMatch(arena, first, "oldest"),
			tag(codec, "three versions back lands on the first, not the second"));
	sprt::cout << "       (the difference report below is the point of the next check)\n";
	check(!imagesMatch(arena, second, "oldest-negative"),
			tag(codec, "and is distinguishable from the second (the newest-wins answer)"));
}

// chunkCount grows, then the rollback target is below the growth. The host table has to shrink with
// it, or verify() sees a table longer than the store claims to be.
static void performShrink(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "shrink store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "shrink journal init"));
	journal.attach(&arena);

	mem_std::Vector<Addr> live;
	for (int i = 0; i < 40; ++i) { live.emplace_back(arena.alloc(1024)); }
	auto reference = takeImage(arena);
	auto before = arena.getChunkCount();
	auto target = journal.commit();

	for (int i = 0; i < 400; ++i) { arena.alloc(4000); }
	check(arena.getChunkCount() > before, tag(codec, "shrink fixture actually grew"));
	journal.commit();

	check(journal.rollback(target) == Status::Ok, tag(codec, "shrink rolls back"));
	check(arena.getChunkCount() == before, tag(codec, "the chunk count shrank back"));
	check(arena.saveSize() == size_t(before) * ChunkSize, tag(codec, "and so did the image"));
	check(arena.verify() == Status::Ok, tag(codec, "shrink verifies"));
	check(imagesMatch(arena, reference, "shrink"), tag(codec, "shrink restores the image"));
}

// A slot that is Empty at the target but live now, and one that is live at the target but Empty
// now. Phase 3 must skip the first (its content is zeros regardless) and Phase 2 must have hosted
// the second.
static void performEmptyBothWays(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "empty store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "empty journal init"));
	journal.attach(&arena);

	uint32_t size = 2 * ChunkSize - 64;
	Addr kept = arena.alloc(size);
	test::fillPattern(arena.write(kept, size), size, kept);
	auto reference = takeImage(arena);
	auto target = journal.commit();

	arena.free(kept); // live at the target, Empty now
	Addr other = arena.alloc(2 * ChunkSize - 64); // Empty at the target, live now
	check(other != NullAddr, tag(codec, "empty fixture reallocated"));
	test::fillPattern(arena.write(other, size), size, other);
	journal.commit();

	check(journal.rollback(target) == Status::Ok, tag(codec, "empty rolls back"));
	check(arena.verify() == Status::Ok, tag(codec, "empty verifies"));
	check(imagesMatch(arena, reference, "empty"),
			tag(codec, "a slot Empty at the target reads as zeros again"));
	check(test::checkPattern(arena.read(kept, size), size, kept),
			tag(codec, "and the slot live at the target has its bytes back"));
}

// Releasing and re-taking the same slots many times inside ONE version. Only the first record for
// each page may survive, or the rollback lands mid-version.
static void performChurnInOneVersion(JournalCodec codec) {
	Arena arena;
	check(arena.init(Config()), tag(codec, "churn store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "churn journal init"));
	journal.attach(&arena);

	uint32_t size = 2 * ChunkSize - 1000;
	Addr a = arena.alloc(size);
	test::fillPattern(arena.write(a, size), size, a);
	auto reference = takeImage(arena);
	auto target = journal.commit();

	for (int i = 0; i < 20; ++i) {
		arena.free(a);
		a = arena.alloc(size + uint32_t(i) * 64);
		test::fillPattern(arena.write(a, size), size, a);
	}
	journal.commit();

	check(journal.rollback(target) == Status::Ok, tag(codec, "churn rolls back"));
	check(arena.verify() == Status::Ok, tag(codec, "churn verifies"));
	check(imagesMatch(arena, reference, "churn"),
			tag(codec, "twenty reshapes in one version roll back as one"));
}

// The workload: committed versions, a reference image for each, then rollbacks to a sample of them
// in random order. Random order matters - rolling back only backwards in sequence would never
// exercise a target below a target already restored.
//
// Keeping an image per version is what makes the oracle exact, and it is also what makes the
// fixture expensive: the store grows as the workload runs, so the images cost O(n^2). At 500
// versions that measures 1.7 GB of peak RSS - fine on a 64-bit host, and more than a wasm32 module
// can address at all, where it aborts rather than failing a check.
//
// So the SIZE is reduced on a 32-bit target and nothing else is. Every fixture above, the random
// order, the byte-exact comparison and the per-codec loop are identical; there is simply less of
// it. The alternative - keeping hashes instead of images everywhere - would have made the oracle
// weaker on the host too, to fix a problem the host does not have.
//
// The reduced RUN (tests.h, `test::full`) cuts the same number for the same reason, harder: the
// O(n^2) is why this one fixture is 88% of the suite's wall clock, and n is the only term in it
// that a run may honestly choose. 80 versions still commits, still keeps an image per version, and
// still lands its rollbacks in random order - it simply has a shorter timeline to land them in.
#if __SIZEOF_POINTER__ == 4
static constexpr int WorkloadVersions = 150;
static constexpr int WorkloadRollbacks = 30;
#else
static constexpr int WorkloadVersions = 500;
static constexpr int WorkloadRollbacks = 100;
#endif

static void performWorkload(JournalCodec codec) {
	const int workloadVersions = int(test::sized(WorkloadVersions, 80));
	const int workloadRollbacks = int(test::sized(WorkloadRollbacks, 20));

	Arena arena;
	check(arena.init(Config()), tag(codec, "workload store init"));

	Journal journal;
	check(journal.init(config(codec)), tag(codec, "workload journal init"));
	journal.attach(&arena);

	Lcg lcg(0x5eed'1234'abcd'0001ull);
	mem_std::Vector<Addr> live;
	mem_std::Vector<Version> versions;
	mem_std::Vector<mem_std::Vector<uint8_t>> references;

	references.reserve(workloadVersions);

	bool ok = true;
	for (int step = 0; step < workloadVersions && ok; ++step) {
		for (int op = 0; op < 20; ++op) {
			auto roll = lcg.next(100);
			if (roll < 50 || live.empty()) {
				uint32_t size = 1 + lcg.next(1500);
				if (lcg.next(64) == 0) {
					size = MaxInChunkPayload + 1 + lcg.next(120000);
				}
				Addr a = arena.alloc(size);
				if (a == NullAddr) {
					ok = false;
					break;
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
					ok = false;
					break;
				}
				test::fillPattern(arena.write(moved, arena.sizeOf(moved)), arena.sizeOf(moved),
						moved);
				live[i] = moved;
			}
		}

		references.emplace_back(takeImage(arena));
		versions.emplace_back(journal.commit());
	}
	check(ok, tag(codec, "the workload ran"));
	sprt::cout << "       " << versions.size() << " versions, " << journal.getMetrics().records
			   << " records, " << (journal.getJournalBytes() >> 10) << " KiB of journal\n";

	bool exact = true;
	bool healthy = true;
	for (int i = 0; i < workloadRollbacks && exact && healthy; ++i) {
		size_t pick = lcg.next(uint32_t(versions.size()));
		auto st = journal.rollback(versions[pick]);
		if (st != Status::Ok) {
			exact = false;
			sprt::cout << "       rollback to version " << versions[pick] << " failed\n";
			break;
		}
		if (arena.verify() != Status::Ok) {
			healthy = false;
			sprt::cout << "       store broken after rolling back to " << versions[pick] << "\n";
			break;
		}
		if (!imagesMatch(arena, references[pick], "workload")) {
			exact = false;
			sprt::cout << "       image differs after rolling back to " << versions[pick] << "\n";
			break;
		}
		// Rolling back truncates the timeline, so the versions above the target are gone.
		versions.resize(pick + 1);
		references.resize(pick + 1);
		if (versions.empty()) {
			break;
		}
	}
	check(healthy, tag(codec, "every rollback leaves a healthy store"));
	check(exact, tag(codec, "every rollback restores the image byte for byte"));
}

void performJournalRollbackTests() {
	sprt::cout << "\n== vstore: journal rollback ==\n";

	// Codec x delta policy, because the two make byte-exactness fail in unrelated ways: a codec
	// breaks it through framing, and a policy through WHICH BYTES a record answers for.
	//
	// Scatter is the arm that carries the sweep. Under it a record restores only the sub-blocks it
	// names, so the oldest-wins rule has to hold per sub-block; under Range - and still more under
	// None - a record covers ground it was not asked about, which puts the right bytes back for the
	// wrong reason and hides exactly the bug this is looking for. A pass under Range alone would
	// prove nothing, which is why none of the four is skipped for being slow.
	//
	// A REDUCED run keeps the first two and the default codec, and the order of this array is that
	// choice: Scatter is the arm the paragraph above calls the one that carries the sweep, and
	// Smallest is what everything outside this file actually runs under. What the full run adds is
	// the cross-terms - a framing mistake that only shows under lzma, a policy that only misbehaves
	// once the codec below it changed the stored size - and those are exactly the failures that are
	// worth minutes at a gate and not worth them at every edit.
	const DeltaPolicy policies[] = {DeltaPolicy::Scatter, DeltaPolicy::Smallest, DeltaPolicy::None,
		DeltaPolicy::Range};
	const size_t policyCount = test::full() ? 4 : 2;

	// The targeted fixtures take the whole product; the workload takes the CROSS through the default
	// codec and the default policy. Not a sampling compromise made to save time for its own sake -
	// the workload keeps an image per version, so it is O(n^2) and costs more than the other six put
	// together, and running it twenty times would put a debug suite into double-digit minutes.
	//
	// What justifies it is that the workload is the broad oracle and the six above are the sharp
	// ones: oldest-wins-per-sub-block is what performOldestWins and performChurnInOneVersion are FOR,
	// and they run under every arm. The workload is there to find the case nobody thought of, and a
	// case nobody thought of is not more likely to need lz4hc-with-scatter specifically than it is to
	// need either of them with the default of the other.
	for (size_t p = 0; p < policyCount; ++p) {
		auto policy = policies[p];
		s_delta = policy;
		for (uint32_t i = 0; i < JournalCodecCount; ++i) {
			auto codec = JournalCodec(i);
			if (!isCodecAvailable(codec)) {
				if (policy == policies[0]) {
					sprt::cout << "       -- " << getCodecName(codec)
							   << ": not built in, skipped\n";
				}
				continue;
			}
			if (!test::full() && codec != DefaultCodec) {
				continue;
			}
			sprt::cout << "       -- codec " << getCodecName(codec) << ", delta "
					   << getDeltaPolicyName(policy) << " --\n";

			performRunReshape(codec);
			performRehostPreserves(codec);
			performOldestWins(codec);
			performShrink(codec);
			performEmptyBothWays(codec);
			performChurnInOneVersion(codec);

			// Reduced: the cross above leaves only the default codec, so taking it as written
			// would run the expensive half of the section twice - once per kept policy. One
			// workload, under both defaults, is what the reduced arm is for.
			const bool workloadHere = test::full()
					? (codec == DefaultCodec || policy == DeltaPolicy::Smallest)
					: (policy == DeltaPolicy::Smallest);
			if (workloadHere) {
				performWorkload(codec);
			}
		}
	}
	s_delta = DeltaPolicy::Smallest;
}


} // namespace STAPPLER_VERSIONIZED stappler
