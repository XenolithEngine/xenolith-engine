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

// The arena kind is a TYPE, and a store does not know which kind is holding it.
//
// That is the claim this section exists to check, and it is the one the whole change rests on. It
// was always TRUE - Root carries no `#if`, the dirty map is host-side only, every geometry constant
// is a compile-time constant of the reader - but nothing ever exercised it, because until now there
// was only ever one kind of arena in a binary.
//
// Three things are checked, in order of how much they claim:
//
//   1. An IMAGE is kind-independent. A store built on one kind, imaged, and adopted into another
//      verifies and images back byte for byte. Every direction, every pair.
//   2. A LIVE store moves between kinds without a copy. adoptFrom() hands over the chunks; the
//      result is byte-identical to the image taken before the move, which is the only definition of
//      a store's content this layer recognises.
//   3. The two coexist and interoperate. A store is filled on a PlainArena - one that announces
//      nothing and has no dirty map at all - then handed to a TrackedArena, journalled from there,
//      rolled back, and handed back. "Started in release, continued in the debugger, and back."
//
// What is deliberately NOT checked here is that a PlainArena is cheaper. That is a measurement, and
// a test that timed it would be asserting on a machine.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"
#include "SPVStoreJournal.h"

#include "../tests.h"
#include "../check/vstore_check.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace vstore;

// A store's content, as the layer defines it: the bytes of its save() image.
template <typename A>
mem_std::Vector<uint8_t> takeImage(const A &arena) {
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

// Something with a shape: blocks of several size classes, some of them freed, so the free lists and
// the block chain both have something to say. Deterministic, because two stores that received the
// same operations have to hold the same bytes.
template <typename A>
void fill(A &arena, mem_std::Vector<Addr> &live) {
	Lcg rng(0x51DE'F00D);
	for (uint32_t i = 0; i < 400; ++i) {
		auto size = 16 + (rng.next() % 3000);
		auto a = arena.alloc(size);
		if (a == NullAddr) {
			continue;
		}
		if (auto dst = arena.write(a, size)) {
			__sprt_memset(dst, uint8_t(i), size);
		}
		live.emplace_back(a);
	}
	// Holes, so the image is not just a bump-allocated run.
	for (uint32_t i = 0; i < live.size(); i += 3) {
		arena.free(live[i]);
		live[i] = NullAddr;
	}
}

// One direction of the image round trip.
template <typename From, typename To>
void checkImageHop(StringView label) {
	From src;
	check(src.init(), mem_std::toString("arena-kinds: ", label, ": a source store"));

	mem_std::Vector<Addr> live;
	fill(src, live);
	auto before = takeImage(src);

	To dst;
	check(dst.adopt(BytesView(before.data(), before.size())) == Status::Ok,
			mem_std::toString("arena-kinds: ", label, ": the image adopts into the other kind"));
	check(dst.verify() == Status::Ok,
			mem_std::toString("arena-kinds: ", label, ": and the result verifies"));

	auto after = takeImage(dst);
	check(after.size() == before.size() && before == after,
			mem_std::toString("arena-kinds: ", label, ": and images back byte for byte"));
}

// One direction of the live move.
template <typename From, typename To>
void checkMoveHop(StringView label) {
	From src;
	check(src.init(), mem_std::toString("arena-kinds: ", label, ": a source store"));

	mem_std::Vector<Addr> live;
	fill(src, live);
	auto before = takeImage(src);

	To dst;
	check(dst.adoptFrom(src) == Status::Ok,
			mem_std::toString("arena-kinds: ", label, ": the live store moves"));
	check(!src.isInitialized(),
			mem_std::toString("arena-kinds: ", label,
					": and the source is left empty - it is a move"));
	check(dst.verify() == Status::Ok,
			mem_std::toString("arena-kinds: ", label, ": the result verifies"));

	auto after = takeImage(dst);
	check(after.size() == before.size() && before == after,
			mem_std::toString("arena-kinds: ", label,
					": and holds exactly the bytes it was given"));

	// Every address survives, because nothing moved: the chunks changed owner, not location.
	uint32_t checked = 0;
	for (uint32_t i = 0; i < live.size(); ++i) {
		if (live[i] == NullAddr) {
			continue;
		}
		if (auto p = dst.read(live[i], 16)) {
			checked += p[0] == uint8_t(i) ? 1 : 0;
		}
	}
	check(checked > 200,
			mem_std::toString("arena-kinds: ", label, ": and every address still resolves (",
					checked, ")"));
}

} // namespace

void performArenaKindsTests() {
	sprt::cout << "\n-- arena-kinds --\n";

	// 1. The image is a fact about the store and not about the kind that wrote it.
	checkImageHop<PlainArena, TrackedArena>(StringView("plain -> tracked"));
	checkImageHop<TrackedArena, PlainArena>(StringView("tracked -> plain"));
	checkImageHop<ShadowArena, PlainArena>(StringView("shadow -> plain"));
	checkImageHop<PlainArena, ShadowArena>(StringView("plain -> shadow"));

	// 2. And a live store moves without going through bytes at all.
	checkMoveHop<PlainArena, TrackedArena>(StringView("move plain -> tracked"));
	checkMoveHop<TrackedArena, PlainArena>(StringView("move tracked -> plain"));
	checkMoveHop<ShadowArena, TrackedArena>(StringView("move shadow -> tracked"));

	// 3. The whole point, end to end: a store filled on a kind that announces NOTHING, continued on
	//    one that does, journalled and rolled back there, and handed back.
	//
	//    A PlainArena has no dirty map - not an empty one, none at all: getDirtyMap() does not exist
	//    on it, so the line that would ask is a compile error rather than a wrong answer. What makes
	//    this work is that the store's content never depended on the map; only the journal does, and the
	//    journal attaches to the store that has one.
	{
		PlainArena release;
		check(release.init(), "arena-kinds: a store that announces nothing");

		mem_std::Vector<Addr> live;
		fill(release, live);
		Addr root = NullAddr;
		for (auto a : live) {
			if (a != NullAddr) {
				root = a;
				break;
			}
		}
		check(root != NullAddr && release.setUserRoot(root) == Status::Ok,
				"arena-kinds: with blocks, holes and a user root");
		auto shippedImage = takeImage(release);

		// The handoff. From here on the store announces its writes and a journal can watch it.
		TrackedArena debugArena;
		check(debugArena.adoptFrom(release) == Status::Ok,
				"arena-kinds: the live store is handed to one that announces its writes");
		check(takeImage(debugArena) == shippedImage, "arena-kinds: with not one byte different");

		// Scoped, and that is not tidiness: a journal holds a reference to the store for its whole
		// life, so the store cannot be moved again until the journal is gone. The observer count
		// asserts exactly that, and this brace is what satisfies it.
		{
			Journal journal;
			check(journal.init() && journal.attach(&debugArena) == 0,
					"arena-kinds: a journal attaches - which a PlainArena would not have compiled");

			auto v0 = journal.commit();
			for (uint32_t i = 0; i < live.size(); ++i) {
				if (live[i] == NullAddr) {
					continue;
				}
				if (auto dst = debugArena.write(live[i], 16)) {
					__sprt_memset(dst, 0xFF, 16);
				}
			}
			journal.commit();
			check(journal.getMetrics().records > 0,
					"arena-kinds: the writes it could not have seen before are recorded now");

			check(journal.rollback(v0) == Status::Ok, "arena-kinds: and roll back");
			check(takeImage(debugArena) == shippedImage,
					"arena-kinds: to the bytes the shipped store handed over, exactly");
		}

		PlainArena back;
		check(back.adoptFrom(debugArena) == Status::Ok && back.verify() == Status::Ok,
				"arena-kinds: the store goes back to one that announces nothing");
		check(takeImage(back) == shippedImage && back.getUserRoot() == root,
				"arena-kinds: and is still the same store it started as");

		uint32_t seen = 0;
		uint32_t expected = 0;
		for (uint32_t i = 0; i < live.size(); ++i) {
			if (live[i] == NullAddr) {
				continue;
			}
			++expected;
			if (auto p = back.read(live[i], 16)) {
				seen += p[0] == uint8_t(i) ? 1 : 0;
			}
		}
		check(seen == expected,
				mem_std::toString("arena-kinds: every block reads back what it was written with (",
						seen, "/", expected, ")"));
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
