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

// A byte-wise copy of the chunks IS the complete state.
//
// This is the invariant the versioned store is built on - snapshots, rollback and keyframes are all
// just consequences of it. It lands early, while the allocator is still simple, so that every later
// allocator change is checked against it immediately instead of being debugged together with it.
//
// The strongest form of the check is not "the copy holds the same data" but "the copy saves to the
// same bytes": that also catches state that lives outside the chunks, padding that depends on
// allocation history, and anything the image format fails to describe.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

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

} // namespace

void performArenaRelocateTests() {
	sprt::cout << "\n== vstore: arena relocate ==\n";

	Arena src;
	check(src.init(Config()), "arena-relocate: init");

	Lcg lcg(0xfeed'face'dead'beefull);
	mem_std::Map<Addr, uint32_t> model;

	// The workload has to leave the store in an awkward shape, not a pristine one: holes in the
	// block chains, at least one released oversize run (so the image contains an empty slot) and
	// blocks that have been moved by realloc. A store that only ever grew would round-trip even
	// if half the format were undescribed.
	{
		bool ok = true;
		mem_std::Vector<Addr> live;

		for (int i = 0; i < 50000 && ok; ++i) {
			auto roll = lcg.next(100);

			if (roll < 55 || live.empty()) {
				uint32_t size = 1 + lcg.next(700);
				if (lcg.next(400) == 0) {
					size = MaxInChunkPayload + 1 + lcg.next(150000);
				}
				Addr a = src.alloc(size);
				if (a == NullAddr) {
					ok = false;
					break;
				}
				uint32_t actual = src.sizeOf(a);
				test::fillPattern(src.write(a, actual), actual, a);
				model.emplace(a, actual);
				live.emplace_back(a);
			} else if (roll < 85) {
				size_t index = lcg.next(uint32_t(live.size()));
				Addr a = live[index];
				src.free(a, model[a]);
				model.erase(a);
				live[index] = live.back();
				live.pop_back();
			} else {
				size_t index = lcg.next(uint32_t(live.size()));
				Addr a = live[index];
				Addr moved = src.realloc(a, 1 + lcg.next(900));
				if (moved == NullAddr) {
					ok = false;
					break;
				}
				model.erase(a);
				live[index] = moved;
				uint32_t actual = src.sizeOf(moved);
				test::fillPattern(src.write(moved, actual), actual, moved);
				model.emplace(moved, actual);
			}
		}
		check(ok, "arena-relocate: workload ran");

		// Released slots get reused by later allocations, so a hole surviving to the end is a
		// matter of luck. Make it deterministic: drop one oversize block last, and allocate
		// nothing after it.
		for (auto &it : model) {
			if (it.second > MaxInChunkPayload) {
				src.free(it.first, it.second);
				model.erase(it.first);
				break;
			}
		}

		check(src.verify() == Status::Ok, "arena-relocate: source verifies");

		mem_std::Value shape;
		src.describe(shape);
		int64_t empties = 0, runs = 0, frees = shape.getValue("free").getInteger("count");
		for (auto &it : shape.getValue("chunks").getArray()) {
			auto kind = it.getInteger("kind");
			if (kind == int64_t(ChunkKind::Empty)) {
				++empties;
			} else if (kind == int64_t(ChunkKind::OversizeHead)) {
				++runs;
			}
		}
		sprt::cout << "       shape: " << src.getChunkCount() << " slots, " << empties
				   << " released, " << runs << " runs, " << frees << " free blocks\n";
		check(empties > 0 && runs > 0 && frees > 0,
				"arena-relocate: the image contains holes, live runs and free blocks");
	}

	// The user root travels in the image or the invariant is only true of the bytes a layer above
	// does not care about: an adopted store whose entry point has to be passed alongside it is
	// exactly the out-of-band state Root's own comment says the format does not require.
	auto userRoot = model.begin()->first;
	check(src.setUserRoot(userRoot) == Status::Ok, "arena-relocate: the user root is accepted");
	check(src.getUserRoot() == userRoot, "arena-relocate: and reads back");
	check(src.verify() == Status::Ok, "arena-relocate: a live user root verifies");

	auto image = takeImage(src);
	check(image.size() == src.saveSize(), "arena-relocate: image size matches saveSize()");

	// The saving is the point of the zero flag: a store with released slots must describe them
	// rather than hand the caller megabytes of nothing to copy. Checked against the shape the
	// store actually has, so it cannot pass by emitting no holes at all.
	{
		size_t pieces = 0, zeroBytes = 0, carried = 0;
		src.save([&](const uint8_t *data, size_t size, bool zero) {
			++pieces;
			if (zero) {
				zeroBytes += size;
				check(data == nullptr, "arena-relocate: a zero piece carries a null pointer");
			} else {
				carried += size;
			}
		});

		mem_std::Value shape;
		src.describe(shape);
		size_t empties = 0;
		for (auto &it : shape.getValue("chunks").getArray()) {
			if (it.getInteger("kind") == int64_t(ChunkKind::Empty)) {
				++empties;
			}
		}

		sprt::cout << "       save: " << pieces << " pieces, " << carried << " bytes carried, "
				   << zeroBytes << " described as zeros\n";
		check(zeroBytes == empties * ChunkSize,
				"arena-relocate: every released slot is described, not materialised");
		check(carried + zeroBytes == src.saveSize(), "arena-relocate: the pieces cover the image");
		check(pieces < src.getChunkCount(),
				"arena-relocate: adjacent contiguous slots are merged into one piece");
	}

	// Re-hosting against the store's OWN descriptor table has to be a no-op. This is the identity
	// case of the machinery a rollback uses to put the host chunk table back in step with restored
	// descriptors, and it is the cheapest place to catch a plan that reshuffles memory it should
	// have left alone.
	{
		RehostPlan plan;
		mem_std::Vector<ChunkDesc> live;
		live.reserve(src.getChunkCount());
		for (uint32_t slot = 0; slot < src.getChunkCount(); ++slot) {
			live.emplace_back(src.getChunkDesc(slot));
		}

		check(src.planRehost(live.data(), src.getChunkCount(), plan) == Status::Ok,
				"arena-relocate: planning against the live table succeeds");
		check(plan.entries.empty(),
				mem_std::toString("arena-relocate: and changes nothing (", plan.entries.size(),
						" entries)"));

		src.rehost(plan);
		check(src.verify() == Status::Ok, "arena-relocate: the store verifies after a null rehost");

		auto after = takeImage(src);
		check(test::compareBytes(BytesView(after.data(), after.size()),
					  BytesView(image.data(), image.size()), "arena-relocate"),
				"arena-relocate: a null rehost leaves the image byte-identical");
	}

	Arena dst;
	check(dst.adopt(BytesView(image.data(), image.size())) == Status::Ok, "arena-relocate: adopt");
	check(dst.verify() == Status::Ok, "arena-relocate: the copy verifies");
	check(dst.getUserRoot() == userRoot,
			"arena-relocate: the copy finds the user root with nothing passed alongside");

	// 1. Everything reads back at the same addresses.
	{
		bool patterns = true;
		Addr bad = NullAddr;
		for (auto &it : model) {
			if (!test::checkPattern(dst.read(it.first, it.second), it.second, it.first)) {
				patterns = false;
				bad = it.first;
				break;
			}
		}
		if (!patterns) {
			sprt::cout << "       first damaged block: addr " << bad << "\n";
		}
		check(patterns, "arena-relocate: every block reads back from the copy");
	}

	// 2. The logical dumps agree.
	{
		mem_std::Value a, b;
		src.describe(a);
		dst.describe(b);
		check(test::compareValues(b, a, "arena-relocate"),
				"arena-relocate: the copy describes identically");
	}

	// 3. And the copy saves to the same bytes.
	{
		auto again = takeImage(dst);
		check(test::compareBytes(BytesView(again.data(), again.size()),
					  BytesView(image.data(), image.size()), "arena-relocate"),
				"arena-relocate: the copy saves to a byte-identical image");
	}

	// 4. The copy is a working store, not a read-only snapshot.
	{
		bool ok = true;
		mem_std::Map<Addr, uint32_t> added;
		for (int i = 0; i < 2000; ++i) {
			uint32_t size = 1 + lcg.next(400);
			Addr a = dst.alloc(size);
			if (a == NullAddr || model.find(a) != model.end()) {
				ok = false; // a reused address would mean the copy lost track of what is live
				break;
			}
			uint32_t actual = dst.sizeOf(a);
			test::fillPattern(dst.write(a, actual), actual, a);
			added.emplace(a, actual);
		}
		check(ok,
				"arena-relocate: allocation continues in the copy without reusing live addresses");

		bool patterns = true;
		for (auto &it : added) {
			patterns = patterns
					&& test::checkPattern(dst.read(it.first, it.second), it.second, it.first);
		}
		for (auto &it : model) {
			patterns = patterns
					&& test::checkPattern(dst.read(it.first, it.second), it.second, it.first);
		}
		check(patterns, "arena-relocate: old and new blocks coexist in the copy");
		check(dst.verify() == Status::Ok, "arena-relocate: the copy verifies after further work");
	}

	// 5. The two stores are genuinely independent.
	{
		auto srcAgain = takeImage(src);
		check(test::compareBytes(BytesView(srcAgain.data(), srcAgain.size()),
					  BytesView(image.data(), image.size()), "arena-relocate"),
				"arena-relocate: work in the copy left the source untouched");
	}

	// 6. Malformed images are refused, and refusing leaves the object safely uninitialized.
	{
		sprt::cout << "       (the reports below are expected)\n";

		auto tryAdopt = [&](mem_std::Vector<uint8_t> &img, StringView what) {
			Arena bad;
			auto st = bad.adopt(BytesView(img.data(), img.size()));
			check(st == Status::ErrorInvalidArguemnt, what);
			check(!bad.isInitialized(), mem_std::toString(what, " leaves the store uninitialized"));
		};

		auto corrupt = image;
		reinterpret_cast<Root *>(corrupt.data())->magic = 0xdead'beef;
		tryAdopt(corrupt, "arena-relocate: a bad magic is refused");

		corrupt = image;
		reinterpret_cast<Root *>(corrupt.data())->formatVersion = RootFormatVersion + 1;
		tryAdopt(corrupt, "arena-relocate: a future format version is refused");

		corrupt = image;
		reinterpret_cast<Root *>(corrupt.data())->chunkShift = ChunkShift - 1;
		tryAdopt(corrupt, "arena-relocate: a different chunk shift is refused");

		corrupt = image;
		// Far enough that the table runs past the ARENA-P0 floor and firstDataOffset really does
		// stop fitting. A bump of one no longer does it: at a 16 KiB page the floor absorbs every
		// table below four thousand slots, so the derived offset would still agree.
		reinterpret_cast<Root *>(corrupt.data())->maxChunks = MaxChunks;
		tryAdopt(corrupt, "arena-relocate: an inconsistent chunk table size is refused");

		corrupt = image;
		corrupt.pop_back();
		tryAdopt(corrupt, "arena-relocate: a truncated image is refused");

		corrupt = image;
		// Damage a block extent deep inside the store: the header still checks out, so this is
		// only caught by the structural sweep adopt() runs before handing the store back.
		reinterpret_cast<BlockHeader *>(corrupt.data() + ChunkSize)->total += Granule;
		tryAdopt(corrupt, "arena-relocate: a structurally broken image is refused");

		corrupt = image;
		reinterpret_cast<Root *>(corrupt.data())->userRoot = Addr(corrupt.size());
		tryAdopt(corrupt, "arena-relocate: a user root outside the image is refused");
	}

	// 7. A user root that is in range but names freed space is the shape a rollback whose rehost
	// phase went wrong would leave behind. adopt() cannot see it - only the block walk can - so it
	// has to be verify() that says no, and the check is worth nothing unless it is exercised.
	{
		Arena probe;
		check(probe.init(Config()), "arena-relocate: probe init");

		auto keep = probe.alloc(64);
		auto other = probe.alloc(64);
		auto gone = probe.alloc(64);
		check(keep != NullAddr && other != NullAddr && gone != NullAddr,
				"arena-relocate: probe blocks");
		check(probe.setUserRoot(gone) == Status::Ok, "arena-relocate: probe root set");
		check(probe.verify() == Status::Ok, "arena-relocate: a live probe root verifies");

		probe.free(gone);
		sprt::cout << "       (the report below is the point of the next check)\n";
		check(probe.verify() == Status::ErrorNotRecoverable,
				"arena-relocate: a user root pointing at freed space is caught");

		check(probe.setUserRoot(keep) == Status::Ok, "arena-relocate: probe root moved");
		check(probe.verify() == Status::Ok,
				"arena-relocate: and moving it back restores the store");

		// A root outside the store never reaches the image: it is a caller error, refused at the
		// call, where it is still distinguishable from corruption.
		sprt::cout << "       (the report below is expected)\n";
		check(probe.setUserRoot(Addr(uint64_t(probe.getChunkCount()) * ChunkSize))
						== Status::ErrorInvalidArguemnt,
				"arena-relocate: an out-of-range user root is refused at the call");
		check(probe.getUserRoot() == keep, "arena-relocate: and the refusal changed nothing");

#if DEBUG
		// setUserRoot writes into Root, which lives in slot 0 page 0. If it ever stopped going
		// through rootForWrite() the journal would restore a store whose entry point is one
		// version stale - a corruption with no visible symptom until something dereferences it.
		check(probe.hasShadow(), "arena-relocate: the shadow is on in a debug build");
		probe.resync();
		check(probe.setUserRoot(other) == Status::Ok, "arena-relocate: barriered root write");
		check(probe.validate([](uint32_t slot, uint32_t page) {
			sprt::cout << "       unannounced write at slot " << slot << " page " << page << "\n";
		}) == Status::Ok,
				"arena-relocate: setUserRoot announces itself to the barrier");
#endif
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
