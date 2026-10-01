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

// The retention horizon, a shared version counter over two stores, and the
// cost profile.
//
// A keyframe is not how a rollback works - the undo log is. What a keyframe does is define how far
// back "any committed version" reaches, and give the tests an independent oracle: a state reached
// again by rolling back must equal the image captured when that version was first closed.

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

// A few allocations, so a version is not empty.
void churn(Arena &arena, Lcg &lcg, mem_std::Vector<Addr> &live) {
	for (int i = 0; i < 12; ++i) {
		if (lcg.next(100) < 60 || live.empty()) {
			Addr a = arena.alloc(1 + lcg.next(900));
			if (a != NullAddr) {
				test::fillPattern(arena.write(a, arena.sizeOf(a)), arena.sizeOf(a), a);
				live.emplace_back(a);
			}
		} else {
			size_t i2 = lcg.next(uint32_t(live.size()));
			arena.free(live[i2]);
			live[i2] = live.back();
			live.pop_back();
		}
	}
}

// Assertions name the codec they ran under, so a failure points at the arm that broke.
mem_std::String tag(JournalCodec codec, StringView what) {
	return mem_std::toString("journal-keyframe[", getCodecName(codec), "]: ", what);
}

Journal::Config config(JournalCodec codec) {
	Journal::Config cfg;
	cfg.codec = codec;
	return cfg;
}

// Sections 2-4 run once per available codec: a keyframe image is compressed with the same codec as
// the journal, so "the state reached by rolling back equals the image captured then" is a check on
// the keyframe block format as much as on the rollback.
void performKeyframeSuite(JournalCodec codec) {
	// 2. A keyframe is an independent oracle: rolling back to its version must reproduce it byte
	//    for byte. This one check covers every way the three phases can go wrong at once.
	{
		Arena arena;
		check(arena.init(Config()), tag(codec, "anchor store init"));
		Journal journal;
		check(journal.init(config(codec)), tag(codec, "anchor journal init"));
		journal.attach(&arena);

		Lcg lcg(0xa'11ce'00000007ull);
		mem_std::Vector<Addr> live;
		for (int i = 0; i < 10; ++i) { churn(arena, lcg, live); }

		auto anchor = journal.commit();
		check(journal.setKeyframe() == Status::Ok, tag(codec, "anchor taken"));

		for (int i = 0; i < 30; ++i) {
			churn(arena, lcg, live);
			journal.commit();
		}

		check(journal.rollback(anchor) == Status::Ok, tag(codec, "rolled back to the anchor"));
		auto now = takeImage(arena);
		mem_std::Vector<uint8_t> kf;
		check(journal.readKeyframeImage(0, 0, kf) == Status::Ok,
				tag(codec, "the keyframe image reads back"));
		check(test::compareBytes(BytesView(now.data(), now.size()), BytesView(kf.data(), kf.size()),
					  "journal-keyframe"),
				tag(codec, "the restored state equals the keyframe image byte for byte"));

		// The timeline above the anchor never happened, so a keyframe from it must not survive.
		check(journal.getKeyframeCount() == 1,
				tag(codec, "keyframes above the target are dropped"));
	}

	// 3. The horizon. Below it a rollback cannot be reconstructed, and saying so is the whole point
	//    of having one.
	{
		Arena arena;
		check(arena.init(Config()), tag(codec, "horizon store init"));

		// Only the count limit is in play here; the byte budget is left generous so the two
		// eviction triggers can be told apart.
		auto cfg = config(codec);
		cfg.maxKeyframes = 2;
		Journal journal;
		check(journal.init(cfg), tag(codec, "horizon journal init"));
		journal.attach(&arena);

		Lcg lcg(0xb0000000000011ull);
		mem_std::Vector<Addr> live;
		mem_std::Vector<Version> anchors;

		for (int i = 0; i < 5; ++i) {
			churn(arena, lcg, live);
			anchors.emplace_back(journal.commit());
			check(journal.setKeyframe() == Status::Ok, tag(codec, "horizon keyframe taken"));
		}

		check(journal.getKeyframeCount() == 2,
				mem_std::toString(tag(codec, "the ring evicted down to two ("),
						journal.getKeyframeCount(), ")"));
		check(journal.getHorizon() > 0, tag(codec, "eviction moved the horizon up"));

		check(journal.rollback(0) == Status::ErrorNotFound,
				tag(codec, "a version below the horizon is ErrorNotFound"));
		check(journal.rollback(journal.getVersion()) == Status::ErrorInvalidArguemnt,
				tag(codec, "the open version is not a rollback target"));
		check(journal.rollback(journal.getVersion() + 10) == Status::ErrorInvalidArguemnt,
				tag(codec, "a version ahead of the store is refused"));

		// Inside the horizon it is still exact.
		auto reachable = anchors.back();
		auto reference = takeImage(arena);
		churn(arena, lcg, live);
		journal.commit();
		check(journal.rollback(reachable) == Status::Ok,
				tag(codec, "a version inside the horizon still rolls back"));
		auto now = takeImage(arena);
		check(test::compareBytes(BytesView(now.data(), now.size()),
					  BytesView(reference.data(), reference.size()), "journal-keyframe"),
				tag(codec, "and does so exactly"));
	}

	// 4. B5: one counter, two stores. A version is a slice of both, including versions in which
	//    only one of them changed.
	{
		Arena scene, local;
		check(scene.init(Config()) && local.init(Config()), tag(codec, "paired: both stores init"));

		Journal journal;
		check(journal.init(config(codec)), tag(codec, "paired: journal init"));
		auto sceneIndex = journal.attach(&scene);
		auto localIndex = journal.attach(&local);
		check(sceneIndex == 0 && localIndex == 1, tag(codec, "paired: both stores attached"));

		Lcg lcg(0xdead'beef'00000002ull);
		mem_std::Vector<Addr> sceneLive, localLive;
		mem_std::Vector<Version> versions;
		mem_std::Vector<mem_std::Vector<uint8_t>> sceneRefs, localRefs;

		for (int i = 0; i < 60; ++i) {
			// Some versions touch one store only - the case where a naive shared counter would
			// take the other store's records from the wrong version.
			auto which = lcg.next(3);
			if (which != 1) {
				churn(scene, lcg, sceneLive);
			}
			if (which != 0) {
				churn(local, lcg, localLive);
			}
			sceneRefs.emplace_back(takeImage(scene));
			localRefs.emplace_back(takeImage(local));
			versions.emplace_back(journal.commit());
		}

		bool exact = true;
		for (int i = 0; i < 20 && exact; ++i) {
			size_t pick = lcg.next(uint32_t(versions.size()));
			if (journal.rollback(versions[pick]) != Status::Ok) {
				exact = false;
				break;
			}
			auto s = takeImage(scene);
			auto l = takeImage(local);
			exact = test::compareBytes(BytesView(s.data(), s.size()),
							BytesView(sceneRefs[pick].data(), sceneRefs[pick].size()),
							"paired-scene")
					&& test::compareBytes(BytesView(l.data(), l.size()),
							BytesView(localRefs[pick].data(), localRefs[pick].size()),
							"paired-local")
					&& scene.verify() == Status::Ok && local.verify() == Status::Ok;
			versions.resize(pick + 1);
			sceneRefs.resize(pick + 1);
			localRefs.resize(pick + 1);
			if (versions.empty()) {
				break;
			}
		}
		check(exact, tag(codec, "paired: one version restores both stores byte for byte"));
	}
}

} // namespace

void performJournalKeyframeTests() {
	sprt::cout << "\n== vstore: journal keyframes and cost ==\n";

	// 1. A keyframe is a version's state, so it is only legal on a committed one. Codec-independent.
	{
		Arena arena;
		check(arena.init(Config()), "journal-keyframe: store init");
		Journal journal;
		check(journal.init(), "journal-keyframe: journal init");
		journal.attach(&arena);

		arena.alloc(256);
		sprt::cout << "       (the report below is expected)\n";
		check(journal.setKeyframe() == Status::ErrorInvalidArguemnt,
				"journal-keyframe: a keyframe with uncommitted changes is refused");

		journal.commit();
		check(journal.setKeyframe() == Status::Ok, "journal-keyframe: and accepted after a commit");
		check(journal.getKeyframeCount() == 1, "journal-keyframe: the keyframe was kept");
	}

	for (uint32_t i = 0; i < JournalCodecCount; ++i) {
		auto codec = JournalCodec(i);
		if (!isCodecAvailable(codec)) {
			sprt::cout << "       -- " << getCodecName(codec) << ": not built in, skipped\n";
			continue;
		}
		// A reduced run takes the default codec alone. What the suite proves - a keyframe is a
		// version's state, the ring evicts oldest-first, a restore from one is byte-exact - is
		// codec-independent in its statement and codec-dependent only in its framing, and framing
		// is what the full run is for. The cost profiles below are NOT cut: they are pinned to
		// JournalCodec::None on purpose and they are cheap.
		if (!test::full() && codec != DefaultCodec) {
			continue;
		}
		sprt::cout << "       -- codec " << getCodecName(codec) << " --\n";
		performKeyframeSuite(codec);
	}

	// 5. B6: the cost profile. Printed unconditionally - the write amplification of a page-granular
	//    journal is the number that decides whether sub-page deltas are needed, and it cannot be
	//    argued about without being measured.
	//
	//    Pinned to JournalCodec::None even though the default is now Zstd. This measures PAGE
	//    GRANULARITY - how many pages a small version drags in - while compression answers the
	//    unrelated question of how many bytes those pages then cost. Letting the default in would
	//    make the figure a compound of the two and destroy its value as a tripwire. Compression is
	//    reported separately, by journal-compress.
	//
	//    Swept over the delta policy for the same reason it is pinned to a codec: None is the
	//    page-granular figure this profile has always reported, and the others are what a record that
	//    can say less than a page costs instead. Down one axis the two are indistinguishable.
	{
		const DeltaPolicy policies[] = {DeltaPolicy::None, DeltaPolicy::Range, DeltaPolicy::Scatter,
			DeltaPolicy::Smallest};

		for (auto policy : policies) {
			Arena arena;
			check(arena.init(Config()), "journal-cost: store init");

			auto cfg = config(JournalCodec::None);
			cfg.delta = policy;
			Journal journal;
			check(journal.init(cfg), "journal-cost: journal init");
			journal.attach(&arena);

			mem_std::Vector<Addr> live;
			uint64_t written = 0;

			for (int v = 0; v < 200; ++v) {
				for (int i = 0; i < 10; ++i) {
					Addr a = arena.alloc(64);
					if (a == NullAddr) {
						break;
					}
					test::fillPattern(arena.write(a, 64), 64, a);
					written += 64;
					live.emplace_back(a);
				}
				journal.commit();
			}

			auto &m = journal.getMetrics();
			double perVersion = double(journal.getJournalBytes()) / double(m.versions);
			double amplification = double(m.pageBytes) / double(written > 0 ? written : 1);

			sprt::cout << "       -- delta " << getDeltaPolicyName(policy) << " --\n";
			sprt::cout << "       " << m.versions << " versions, " << m.records << " records ("
					   << m.zeroRecords << " zero, " << m.deltaRecords << " delta), "
					   << (journal.getJournalBytes() >> 10) << " KiB journal\n";
			sprt::cout << "       " << uint64_t(perVersion) << " bytes/version, "
					   << (m.records / m.versions) << " pages/version, write amplification "
					   << amplification << "x\n";
			if (m.deltaRecords > 0) {
				sprt::cout << "       " << (m.deltaSubBlocks / m.deltaRecords) << " of "
						   << SubBlocksPerPage << " sub-blocks per delta\n";
			}
			// What the map's entries covered against what the sub-blocks they round out to cover.
			// Equal under the bitmap by construction; under the range encoding the gap is the
			// ceiling on what a byte-granular RECORD could have saved, and it is printed rather
			// than asserted because it is an argument for a format change, not a property.
			if (m.markedSubBlockBytes > 0) {
				sprt::cout << "       marked " << m.markedBytes << " bytes, rounding out to "
						   << m.markedSubBlockBytes << " ("
						   << (m.markedSubBlockBytes * 100 / m.markedBytes) << "% of the extent)\n";
			}
			if (policy == DeltaPolicy::Smallest) {
				sprt::cout << "       smallest picked: " << m.wholeWins << " whole, " << m.rangeWins
						   << " range, " << m.scatterWins << " scatter, " << m.sizingEncodes
						   << " surplus encodes\n";
			}

			// What the sizing pass cost, as a count rather than as a timing - which is the only form
			// of it a test can hold. Every recorded page pays one encode whatever the policy; this
			// counts the ones spent asking whether a WIDER shape would have been smaller.
			//
			// Zero is the claim, and it is a strong one: this workload appends small blocks, so
			// every delta is contiguous, so the span and the exact set are the same mask and there
			// is nothing to ask. It used to be two per record regardless - the whole page and the
			// span - and the whole page was a 16 KiB compression bidding for a 256-byte payload.
			// See DeltaPolicy::Smallest.
			check(m.sizingEncodes == 0,
					mem_std::toString("journal-cost[", getDeltaPolicyName(policy),
							"]: a contiguous delta is not asked twice (", m.sizingEncodes,
							" surplus encodes)"));

			check(m.versions == 200, "journal-cost: every version was counted");
			check(m.records > 0 && m.pageBytes > 0, "journal-cost: records and bytes were counted");

			// A version that only appends small blocks touches Root, the block header's page and the
			// payload's page - and at four pages to a chunk those are often the same page twice
			// over. Well above the floor means something re-reads pages it should have already
			// marked dirty.
			check(m.records / m.versions <= 4,
					mem_std::toString("journal-cost[", getDeltaPolicyName(policy),
							"]: a small version stays near the page floor (",
							m.records / m.versions, " pages)"));

			// What the delta is FOR: a version that wrote 640 bytes must not be storing pages. The
			// bound is deliberately loose - the point is the order of magnitude, and a tighter one
			// would be pinning an allocator layout rather than a property.
			if (policy != DeltaPolicy::None) {
				check(perVersion < double(PageSize),
						mem_std::toString("journal-cost[", getDeltaPolicyName(policy),
								"]: a small version costs less than one page (",
								uint64_t(perVersion), ")"));
			}
		}
	}

	// 6. The case that tells the two delta shapes apart. Everything above writes contiguously, so
	//    Range and Scatter produce the same mask and the knob measures nothing - which is exactly the
	//    trap a one-workload measurement falls into.
	//
	//    Here each version writes sixteen bytes near the start of a page and sixteen near its end.
	//    Scatter stores two sub-blocks; Range stores the span between them, which is the page. That
	//    is the whole disagreement between the two, isolated, and it is what makes "Smallest picks
	//    the smaller" a claim with evidence rather than a description of the code.
	{
		const DeltaPolicy policies[] = {DeltaPolicy::Range, DeltaPolicy::Scatter,
			DeltaPolicy::Smallest};

		uint64_t perVersionFor[3] = {};
		uint64_t index = 0;

		sprt::cout << "       -- scattered writes, one page, two ends --\n";
		for (auto policy : policies) {
			Arena arena;
			check(arena.init(Config()), "journal-cost: scattered store init");

			auto cfg = config(JournalCodec::None);
			cfg.delta = policy;
			Journal journal;
			check(journal.init(cfg), "journal-cost: scattered journal init");
			journal.attach(&arena);

			// Two pages' worth, anchored to a page boundary inside it. The alignment is load-bearing:
			// firstDataOffset follows the chunk table's size, not the page, so a block's first byte
			// sits at an arbitrary offset into a page - and the two writes below would then land in
			// two different pages, one sub-block each. That reads as a triumph for Range and is a
			// fixture measuring nothing.
			Addr block = arena.alloc(2 * PageSize);
			check(block != NullAddr, "journal-cost: scattered fixture allocated");
			Addr a = (block + PageMask) & ~PageMask;
			test::fillPattern(arena.write(a, PageSize), PageSize, a);
			journal.commit();

			auto before = journal.getJournalBytes();
			for (uint32_t v = 0; v < 200; ++v) {
				test::fillPattern(arena.write(a + 16, 16), 16, a + v);
				test::fillPattern(arena.write(a + PageSize - 32, 16), 16, a + v + 1);
				journal.commit();
			}
			perVersionFor[index++] = (journal.getJournalBytes() - before) / 200;

			auto &m = journal.getMetrics();
			sprt::cout << "       " << getDeltaPolicyName(policy) << ": "
					   << perVersionFor[index - 1] << " bytes/version";
			if (m.deltaRecords > 0) {
				sprt::cout << ", " << (m.deltaSubBlocks / m.deltaRecords) << " of "
						   << SubBlocksPerPage << " sub-blocks per delta";
			}
			if (policy == DeltaPolicy::Smallest) {
				sprt::cout << ", picked " << m.wholeWins << "/" << m.rangeWins << "/"
						   << m.scatterWins << " whole/range/scatter";
			}
			sprt::cout << "\n";
		}

		// True under BOTH encodings of a map entry, which is not obvious and is worth an assertion
		// rather than a comment. A range entry cannot name two extents, so under SP_VSTORE_DIRTY_RANGE
		// the map hands `capture()` a mask rounded out to the whole span - and the records come out
		// identical anyway, because `changedSubBlocks()` re-derives the exact set by COMPARING the
		// sub-blocks the entry covers. The arena forgetting where the writes were does not lose the
		// answer; it moves the cost of knowing it into the capture.
		check(perVersionFor[1] < perVersionFor[0],
				mem_std::toString(
						"journal-cost: scattered writes cost less under scatter than " "under " "ra"
																								"ng"
																								"e "
																								"(",
						perVersionFor[1], " against ", perVersionFor[0], ")"));
		check(perVersionFor[2] <= perVersionFor[1],
				mem_std::toString("journal-cost: and smallest is no worse than either (",
						perVersionFor[2], ")"));
	}
}


} // namespace STAPPLER_VERSIONIZED stappler
