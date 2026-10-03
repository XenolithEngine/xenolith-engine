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

#ifndef TESTS_FLOW_CHECK_FLOW_CHECK_H_
#define TESTS_FLOW_CHECK_FLOW_CHECK_H_

#include "SPCommon.h"
#include "SPMemory.h" // mem_std::Value
#include "SPData.h"

#include <sprt/runtime/mem/context.h>

#include "SPFlowEnv.h"
#include "SPFlowDiag.h"

namespace STAPPLER_VERSIONIZED stappler::test {

// Deterministic linear congruential generator. Same constants as
// tests/runtime/cxx/cxx_int_set.cpp so torture runs here are comparable with those.
//
// Determinism is not a convenience: it is what makes the torture sections' measured numbers
// (fragmentation, chunk counts, op mixes) bit-identical on every platform and every run, so a
// threshold on them is a regression tripwire rather than a statistical bound.
struct Lcg {
	uint64_t state;

	explicit Lcg(uint64_t seed = 0x853c'49e6'748f'ea9bull) : state(seed) { }

	uint64_t next() {
		state = state * 6'364'136'223'846'793'005ull + 1'442'695'040'888'963'407ull;
		return state >> 16;
	}

	uint32_t next(uint32_t bound) { return uint32_t(next() % bound); }
};

// The byte an arena block at `addr` must hold at `index`. Depends on BOTH, so that a block written
// at one address and read back at another - the signature of an allocator handing out overlapping
// or shifted blocks - fails the check instead of silently matching.
inline uint8_t patternByte(uint32_t addr, uint32_t index) {
	uint64_t x = uint64_t(addr) * 0x9E37'79B9'7F4A'7C15ull + index;
	x ^= x >> 33;
	x *= 0xFF51'AFD7'ED55'8CCDull;
	x ^= x >> 29;
	return uint8_t(x);
}

void fillPattern(uint8_t *dst, uint32_t size, uint32_t addr);

// `firstBad`, when given, receives the index of the first mismatching byte.
bool checkPattern(const uint8_t *src, uint32_t size, uint32_t addr, uint32_t *firstBad = nullptr);

// Byte-wise comparison; on mismatch prints the label and up to `maxReported` differing offsets.
bool compareBytes(BytesView got, BytesView expect, StringView label, uint32_t maxReported = 8);

// Structural comparison; on mismatch prints the label and the path to the first difference.
bool compareValues(const mem_std::Value &got, const mem_std::Value &expect, StringView label);

// The same, under the device tolerance: a double is `flow::nearlyEqual`, everything else is
// exact. What a run on a device is compared with, and nothing else - a CPU-only run is exact.
bool compareValuesNearly(const mem_std::Value &got, const mem_std::Value &expect, StringView label);

// ---- diagnostic reports ------------------------------------------------------------------------
//
// The kernel reports numbers; a kernel call handed a `mem_std::Value *` writes them with
// `flow::writeDiagNumbers` (the specialization at the end of this file). These name a code the way a
// check reads best, by the table in flow_names.cpp, so a check says `hasDiag(report, "unknown-op")`.

StringView getDiagCodeName(flow::DiagCode);
StringView getDiagCodeName(flow::value::DiagCode);

// The name of the code an entry carries, by its domain.
StringView getDiagCodeName(const mem_std::Value &entry);

// The severity an entry carries, by name: "error", "warning" or "advice".
StringView getDiagSeverityName(const mem_std::Value &entry);

// Does this report carry an entry with this code? A check names the CODE rather than a count,
// because a check that only asked "was it refused" passes for a refusal for the wrong reason.
bool hasDiag(const mem_std::Value &report, StringView code);

// A sink over a diagnostic array, writing numbers: what a value-layer call, which takes a sink and
// no array, is handed.
class NumberSink final : public flow::DiagSink {
public:
	explicit NumberSink(mem_std::Value *out) : _out(out) { }

	void add(const flow::Diag &d) override { flow::writeDiagNumbers(_out, d); }

	flow::DiagSink *get() { return _out ? this : nullptr; }

private:
	mem_std::Value *_out = nullptr;
};

/* ---- memory ------------------------------------------------------------------------------------

A pool, pushed as the current context, and how many bytes were taken from it while it was.

`get_allocated_bytes` is monotonic - a free never decrements it - so `taken() == 0` is a true
statement that nothing was allocated rather than a statement that nothing was left over. Pushing the
pool as the CONTEXT is what makes the number mean anything: without it, a `mem_pool::` allocation
inside the code under test would land in whatever pool was already current and this one would read
zero for the wrong reason.

What it does NOT see is the `mem_std::` half, which goes to malloc and has no counter anywhere. That
half is answered structurally instead - by a class with no owning member to put an allocation in -
and the two together are the whole of "this allocated nothing". */
struct PoolScope {
	using Context = sprt::memory::context<memory::pool_t *>;

	memory::pool_t *pool = nullptr;

	// The RAII push, and NOT a hand-written push/pop pair: in a debug build the stack matches a pop
	// against the SOURCE STRING its push carried, so a pair written by hand out of two different
	// functions is reported as unbalanced and aborts. `destroy` makes the pop and the pool's end
	// one thing, in the right order.
	Context ctx;
	size_t base = 0;

	PoolScope() : pool(memory::pool::create()), ctx(pool, Context::destroy) {
		base = sprt::memory::pool::get_allocated_bytes(pool);
	}

	PoolScope(const PoolScope &) = delete;
	PoolScope &operator=(const PoolScope &) = delete;

	size_t taken() const { return sprt::memory::pool::get_allocated_bytes(pool) - base; }
};

} // namespace stappler::test

namespace STAPPLER_VERSIONIZED stappler::flow {

// A kernel call handed a diagnostic array writes numbers into it.
template <>
struct DiagWriterFor<mem_std::Value> {
	using Sink = test::NumberSink;
};

} // namespace stappler::flow

#endif /* TESTS_FLOW_CHECK_FLOW_CHECK_H_ */
