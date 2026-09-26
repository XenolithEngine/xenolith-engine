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
#include "SPData.h"

#include "XLCoreFrameDataCache.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

using core::FrameDataCache;
using core::FrameDataKind;
using core::FrameDataMirror;

static Bytes FrameData_body(size_t size, uint8_t fill) { return Bytes(size, fill); }

// How many operations of `code` the list carries
static size_t FrameData_countOps(const Value &ops, core::FrameDataOp code) {
	size_t ret = 0;
	for (auto &op : ops.getValue("ops").asArray()) {
		if (core::FrameDataOp(op.getInteger(0)) == code) {
			++ret;
		}
	}
	return ret;
}

// A value that went through the wire, as the server reads it
static Value FrameData_wire(const Value &val) {
	return data::read<memory::StandardInterface>(data::write(val, data::EncodeFormat::Cbor));
}

static void FrameData_testMirror() {
	{
		auto mirror = Rc<FrameDataMirror>::create(0);
		mirror->beginSerialization();
		auto body = FrameData_body(100, 1);
		check(!mirror->store(FrameDataKind::VertexSet, 1, 0, body)
						&& !mirror->reference(FrameDataKind::VertexSet, 1, 0)
						&& mirror->takeOps().empty(),
				"framedata: a disabled mirror sends everything inline");
	}

	{
		auto mirror = Rc<FrameDataMirror>::create(4'096);
		auto cache = Rc<FrameDataCache>::create(4'096);

		mirror->beginSerialization();
		check(!mirror->reference(FrameDataKind::VertexSet, 7, 1),
				"framedata: an unknown set is not a reference");
		check(mirror->store(FrameDataKind::VertexSet, 7, 1, FrameData_body(100, 7)),
				"framedata: a set that fits is stored");
		check(!mirror->store(FrameDataKind::VertexSet, 0, 1, FrameData_body(100, 7)),
				"framedata: id 0 (no data) is never stored");

		auto ops = mirror->takeOps();
		checkEq(FrameData_countOps(ops, core::FrameDataOp::Store), 1, "framedata: one Store op");
		check(ops.getInteger("e") == 0, "framedata: the list carries the epoch");
		check(mirror->takeOps().getValue("ops").empty(), "framedata: takeOps empties the list");

		check(sprt::status::isSuccessful(cache->apply(FrameData_wire(ops))),
				"framedata: the cache applies the list");
		auto e = cache->find(FrameDataKind::VertexSet, 7, 1);
		check(e && e->raw == FrameData_body(100, 7) && e->cost == core::getFrameDataCost(100),
				"framedata: the cache holds the body");

		mirror->beginSerialization();
		check(mirror->reference(FrameDataKind::VertexSet, 7, 1),
				"framedata: the next input references it");
		check(!mirror->reference(FrameDataKind::VertexSet, 7, 2),
				"framedata: another generation is not a reference");
		check(!mirror->reference(FrameDataKind::Gradient, 7, 1),
				"framedata: another kind is not a reference");

		check(!cache->find(FrameDataKind::VertexSet, 8, 1),
				"framedata: cache miss on an unknown id");
		check(!cache->find(FrameDataKind::VertexSet, 7, 2),
				"framedata: cache miss on another generation");
		check(!cache->find(FrameDataKind::Gradient, 7, 1), "framedata: cache miss on another kind");
		auto st = cache->getStats();
		check(st.hits == 1 && st.misses == 3, "framedata: hits and misses are counted");
	}

	{
		// A new generation replaces the old one on both sides without a Drop
		auto mirror = Rc<FrameDataMirror>::create(4'096);
		auto cache = Rc<FrameDataCache>::create(4'096);
		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 3, 1, FrameData_body(200, 1));
		cache->apply(FrameData_wire(mirror->takeOps()));

		mirror->beginSerialization();
		check(mirror->store(FrameDataKind::VertexSet, 3, 2, FrameData_body(300, 2)),
				"framedata: a new generation is stored");
		auto ops = mirror->takeOps();
		checkEq(FrameData_countOps(ops, core::FrameDataOp::Drop), 0,
				"framedata: the replaced generation is not dropped explicitly");
		cache->apply(FrameData_wire(ops));
		check(cache->find(FrameDataKind::VertexSet, 3, 2)
						&& !cache->find(FrameDataKind::VertexSet, 3, 1),
				"framedata: the cache replaced the generation");
		checkEq(cache->getStats().bytes, mirror->getStats().bytes,
				"framedata: both sides count the same bytes after a replacement");
		checkEq(cache->getStats().entries, 1, "framedata: one entry after a replacement");
	}

	{
		// Replacing an entry this input already references would pull it from under the reference
		auto mirror = Rc<FrameDataMirror>::create(4'096);
		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 3, 1, FrameData_body(200, 1));
		mirror->takeOps();
		mirror->beginSerialization();
		mirror->reference(FrameDataKind::VertexSet, 3, 1);
		check(!mirror->store(FrameDataKind::VertexSet, 3, 2, FrameData_body(200, 2)),
				"framedata: an entry referenced by this input is not replaced");
	}

	{
		// Least recently used goes first; what this input references is never evicted
		const size_t cost = core::getFrameDataCost(100);
		auto mirror = Rc<FrameDataMirror>::create(cost * 3);
		auto cache = Rc<FrameDataCache>::create(cost * 3);
		for (uint64_t id = 1; id <= 3; ++id) {
			mirror->beginSerialization();
			mirror->store(FrameDataKind::VertexSet, id, 0, FrameData_body(100, uint8_t(id)));
		}
		cache->apply(FrameData_wire(mirror->takeOps()));

		mirror->beginSerialization();
		mirror->reference(FrameDataKind::VertexSet, 1, 0); // 2 is now the oldest
		check(mirror->store(FrameDataKind::VertexSet, 4, 0, FrameData_body(100, 4)),
				"framedata: a store past the budget evicts");
		auto ops = mirror->takeOps();
		check(FrameData_countOps(ops, core::FrameDataOp::Drop) == 1
						&& ops.getValue("ops").getValue(0).getInteger(2) == 2,
				"framedata: the least recently used entry is dropped");
		check(sprt::status::isSuccessful(cache->apply(FrameData_wire(ops))),
				"framedata: the cache follows the eviction");
		check(!cache->find(FrameDataKind::VertexSet, 2, 0)
						&& cache->find(FrameDataKind::VertexSet, 1, 0),
				"framedata: the cache dropped the same entry");

		// all three held entries are referenced now: nothing can be evicted
		mirror->beginSerialization();
		mirror->reference(FrameDataKind::VertexSet, 1, 0);
		mirror->reference(FrameDataKind::VertexSet, 3, 0);
		mirror->reference(FrameDataKind::VertexSet, 4, 0);
		check(!mirror->store(FrameDataKind::VertexSet, 5, 0, FrameData_body(100, 5)),
				"framedata: entries referenced by this input are not evicted");
		checkEq(FrameData_countOps(mirror->takeOps(), core::FrameDataOp::Drop), 0,
				"framedata: no Drop when nothing can be evicted");

		mirror->beginSerialization();
		check(!mirror->store(FrameDataKind::VertexSet, 6, 0, FrameData_body(cost * 3, 6)),
				"framedata: a body larger than the budget goes inline");
		check(mirror->getStats().inlined == 2, "framedata: inline decisions are counted");
	}
}

static void FrameData_testCache() {
	const auto live = FrameDataCache::getLiveEntries();
	{
		auto mirror = Rc<FrameDataMirror>::create(1 << 20);
		auto cache = Rc<FrameDataCache>::create(1 << 20);
		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 1, 0, FrameData_body(10, 1));
		mirror->store(FrameDataKind::Gradient, 2, 0, FrameData_body(10, 2));
		cache->apply(FrameData_wire(mirror->takeOps()));
		checkEq(FrameDataCache::getLiveEntries(), live + 2, "framedata: live entries are counted");

		// an older epoch is refused as a whole, without an error
		auto epoch = cache->reset();
		checkEq(epoch, 1, "framedata: reset starts a new epoch");
		checkEq(cache->getStats().entries, 0, "framedata: reset drops everything");
		checkEq(FrameDataCache::getLiveEntries(), live, "framedata: reset releases live entries");

		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 3, 0, FrameData_body(10, 3));
		check(cache->apply(FrameData_wire(mirror->takeOps())) == Status::Declined,
				"framedata: a list of the old epoch is declined");
		checkEq(cache->getStats().entries, 0, "framedata: a declined list applies nothing");

		mirror->reset(epoch);
		checkEq(mirror->getStats().entries, 0, "framedata: the mirror starts over on reset");
		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 3, 0, FrameData_body(10, 3));
		check(sprt::status::isSuccessful(cache->apply(FrameData_wire(mirror->takeOps()))),
				"framedata: the new epoch is applied");

		Value empty;
		check(sprt::status::isSuccessful(cache->apply(empty)),
				"framedata: no list is not an error");

		// malformed: a Drop of an unknown entry, a Store without a body
		Value bad;
		bad.setInteger(int64_t(epoch), "e");
		auto &ops = bad.emplace("ops");
		auto &op = ops.emplace();
		op.addInteger(toInt(core::FrameDataOp::Drop));
		op.addInteger(toInt(FrameDataKind::VertexSet));
		op.addInteger(42);
		op.addInteger(0);
		check(!sprt::status::isSuccessful(cache->apply(bad)),
				"framedata: a Drop of an unknown entry fails");

		Value noBody;
		noBody.setInteger(int64_t(epoch), "e");
		auto &op2 = noBody.emplace("ops").emplace();
		op2.addInteger(toInt(core::FrameDataOp::Store));
		op2.addInteger(toInt(FrameDataKind::VertexSet));
		op2.addInteger(43);
		op2.addInteger(0);
		check(!sprt::status::isSuccessful(cache->apply(noBody)),
				"framedata: a Store without a body fails");
	}
	{
		// the budget is the server's to enforce: a client that ignores it is refused
		auto mirror = Rc<FrameDataMirror>::create(1 << 20);
		auto cache = Rc<FrameDataCache>::create(core::getFrameDataCost(100));
		mirror->beginSerialization();
		mirror->store(FrameDataKind::VertexSet, 1, 0, FrameData_body(100, 1));
		mirror->store(FrameDataKind::VertexSet, 2, 0, FrameData_body(100, 2));
		check(cache->apply(FrameData_wire(mirror->takeOps())) == Status::ErrorBufferOverflow,
				"framedata: a list past the budget is refused");
	}
	checkEq(FrameDataCache::getLiveEntries(), live,
			"framedata: destroyed caches leave no live entries");
}

// A client drawing frames of random sets through a small budget: every reference it writes must
// resolve on the server after the server applied the input's operations.
static void FrameData_testRandom() {
	const size_t budget = 16 * 1'024;
	auto mirror = Rc<FrameDataMirror>::create(budget);
	auto cache = Rc<FrameDataCache>::create(budget);

	uint64_t seed = 0x1234'5678;
	auto next = [&]() {
		seed = seed * 6'364'136'223'846'793'005ULL + 1'442'695'040'888'963'407ULL;
		return uint32_t(seed >> 33);
	};

	struct TestSet {
		FrameDataKind kind;
		uint32_t generation;
		size_t size;
	};
	Map<uint64_t, TestSet> sets;
	for (uint64_t id = 1; id <= 200; ++id) {
		sets.emplace(id,
				TestSet{(id % 5 == 0) ? FrameDataKind::Gradient : FrameDataKind::VertexSet, 0,
					size_t(16 + next() % 1'024)});
	}

	bool resolved = true;
	bool bounded = true;
	bool matched = true;
	size_t references = 0;
	for (size_t frame = 0; frame < 2'000; ++frame) {
		mirror->beginSerialization();
		Vector<Pair<uint64_t, TestSet>> refs;
		auto count = 5 + next() % 30;
		for (size_t i = 0; i < count; ++i) {
			auto id = 1 + next() % 200;
			auto &set = sets.find(id)->second;
			if (next() % 20 == 0) {
				++set.generation; // the client changed the set
			}
			if (mirror->reference(set.kind, id, set.generation)
					|| mirror->store(set.kind, id, set.generation,
							FrameData_body(set.size, uint8_t(id)))) {
				refs.emplace_back(id, set);
			}
		}
		if (!sprt::status::isSuccessful(cache->apply(FrameData_wire(mirror->takeOps())))) {
			resolved = false;
			break;
		}
		for (auto &it : refs) {
			auto e = cache->find(it.second.kind, it.first, it.second.generation);
			if (!e || e->raw.size() != it.second.size) {
				resolved = false;
			}
		}
		references += refs.size();
		auto ms = mirror->getStats();
		auto cs = cache->getStats();
		bounded = bounded && ms.bytes <= budget && cs.bytes <= budget;
		matched = matched && ms.bytes == cs.bytes && ms.entries == cs.entries;
	}
	check(resolved, "framedata: every reference resolves after the operations");
	check(bounded, "framedata: both sides stay within the budget");
	check(matched, "framedata: the mirror and the cache agree after every input");
	check(references > 10'000 && mirror->getStats().drops > 0,
			"framedata: the random run evicted and referenced");
}

void performFrameDataTests() {
	sprt::cout << "--- core::FrameDataCache ---\n";
	FrameData_testMirror();
	FrameData_testCache();
	FrameData_testRandom();
}

} // namespace stappler::xenolith::remote
