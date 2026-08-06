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

// data::Value::Null — the sentinel every failed lookup returns. It used to be an EMPTY,
// writable global: `v.getValue("missing") = 42` inserted nothing and instead rewrote the
// sentinel, after which every missing-key read in the process returned 42. It now holds a real
// Type::NONE (so the guards in operator=/reset/convertTo* fire) and is constant-initialized into
// read-only memory (so a write that gets past them faults instead of corrupting).
//
// The checks below cover the states that must hold. The two failure modes are destructive, so
// they run only on request, in their own process:
//   XL_NULL_WRITE=guarded — an API write into the sentinel: aborts on the assert in a debug
//                           build, is dropped in a release build (verified there)
//   XL_NULL_WRITE=raw     — a raw write into its storage: must fault, proving .rodata placement
//   XL_NULL_WRITE=type    — a non-const get*() on a missing key: it would hand out the shared
//                           null container, so a debug build asserts (release stays lenient)

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include <sprt/c/__sprt_stdlib.h> // getenv
#include <sprt/c/__sprt_string.h> // memset

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

void performDataValueTests() {
	using Value = mem_std::Value;

	sprt::cout << "\n== stappler data::Value tests ==\n";

	StringView mode;
	if (auto env = __sprt_getenv("XL_NULL_WRITE")) {
		mode = StringView(env);
	}

	if (mode == "guarded") {
		Value cfg;
		cfg.setInteger(1, "a");
		sprt::cout << "attempting an API write into the sentinel...\n";
		cfg.getValue("nope") = 42;
		// only reachable in a release build
		sprt::cout << "no abort; Null type = " << int(Value::Null.getType())
				   << " (NONE == 255), unrelated default: " << cfg.getInteger("other", 5) << "\n";
		check(Value::Null.getType() == Value::Type::NONE, "release: sentinel survived the write");
		check(cfg.getInteger("other", 5) == 5, "release: defaults are not poisoned");
		return;
	}

	if (mode == "type") {
		Value cfg;
		cfg.setInteger(1, "a");
		sprt::cout << "asking a non-const getString() for a missing key...\n";
		auto &s = cfg.getString("nope");
		// only reachable in a release build
		sprt::cout << "no abort; the shared null container came back, empty = " << s.empty() << "\n";
		check(s.empty(), "release: the null container is still empty");
		return;
	}

	if (mode == "raw") {
		sprt::cout << "attempting a raw write into the sentinel's storage...\n";
		__sprt_memset((void *)&Value::Null, 0x7f, sizeof(Value));
		sprt::cout << "NO FAULT — the sentinel is in writable memory\n";
		return;
	}

	check(Value::Null.getType() == Value::Type::NONE, "Null really holds Type::NONE");
	check(Value::Null.isNull(), "Null is null");
	check(!bool(Value::Null), "Null is falsy");
	checkEq(data::toString(Value::Null, false), StringView("null"), "Null encodes as null");

	Value cfg;
	cfg.setInteger(1, "a");

	// reads of a missing key: the whole point of handing out the sentinel
	check(cfg.getValue("nope").isNull(), "missing key reads as null");
	check(cfg.getValue("nope").getValue("deeper").isNull(), "a chain through a missing key is null");
	check(cfg.getInteger("nope", 5) == 5, "missing key yields the default");
	check(cfg.getType("nope") == Value::Type::NONE, "getType(missing) is NONE");
	// a defensive read of a container/string goes through a const reference: the non-const
	// accessors would hand out the shared null container instead, and say so (XL_NULL_WRITE=type)
	const Value &roCfg = cfg;
	check(roCfg.getString("nope").empty(), "const getString(missing) is empty");
	check(roCfg.getArray("nope").empty(), "const getArray(missing) is empty");
	check(roCfg.getDict("nope").empty(), "const getDict(missing) is empty");
	check(roCfg.getBytes("nope").empty(), "const getBytes(missing) is empty");
	check(roCfg.getString().empty(), "const getString() on a dictionary is empty");

	// moving OUT of a missing key: 13 call sites in the tree do this (sp::move(v.getValue(0)))
	Value moved = sp::move(cfg.getValue("nope"));
	check(moved.isNull(), "move from a missing key yields an empty value");
	check(Value::Null.getType() == Value::Type::NONE, "... and leaves the sentinel intact");

	// copying it collapses NONE to EMPTY, as before
	Value copied = cfg.getValue("nope");
	check(copied.getType() == Value::Type::EMPTY, "a copy of the sentinel is EMPTY");

	// a real insertion still works
	cfg.setInteger(42, "nope");
	check(cfg.getInteger("nope") == 42, "insertion by key works");
	check(Value::Null.getType() == Value::Type::NONE, "insertion left the sentinel intact");

	// a write the target cannot accept is reported by handing back the sentinel — reading that
	// answer must not disturb it either
	Value scalar(int64_t(1));
	check(scalar.setValue(Value(2), "key").isNull(), "a key write into a scalar returns null");
	check(scalar.getInteger() == 1, "... and leaves the target alone");
	check(scalar.setValue(Value(2), 0).isNull(), "an indexed write into a scalar returns null");
	check(Value::Null.getType() == Value::Type::NONE, "rejected writes left the sentinel intact");

	// indexed writes: an existing element is replaced, anything past the end takes the next free
	// slot, so one write appends one element at most and never leaves holes
	Value arr(Value::Type::ARRAY);
	arr.addInteger(1);
	arr.addInteger(2);

	arr.setValue(Value(9), 1);
	check(arr.size() == 2 && arr.getInteger(1) == 9, "an existing index is replaced");

	arr.setValue(Value(3), 2);
	check(arr.size() == 3 && arr.getInteger(2) == 3, "index == size appends");

	arr.setValue(Value(4), 100);
	check(arr.size() == 4 && arr.getInteger(3) == 4, "an index past the end appends once");

	check(arr.setValue(Value(5), -1).isNull(), "a negative index is refused");
	check(arr.size() == 4, "... and changes nothing");

	// index 0 on an empty array used to run off the end of the storage
	Value fresh(Value::Type::ARRAY);
	fresh.setValue(Value(7), 0);
	check(fresh.size() == 1 && fresh.getInteger(0) == 7, "index 0 on an empty array appends");

	// an EMPTY value becomes an array of one, not one padded with nulls up to the index
	Value blank;
	blank.setValue(Value(8), 3);
	check(blank.isArray() && blank.size() == 1 && blank.getInteger(0) == 8,
			"an indexed write converts an empty value without padding");
}

} // namespace stappler
