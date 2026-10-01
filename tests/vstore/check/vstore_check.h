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

#ifndef TESTS_VSTORE_CHECK_VSTORE_CHECK_H_
#define TESTS_VSTORE_CHECK_VSTORE_CHECK_H_

#include "SPCommon.h"
#include "SPMemory.h" // mem_std::Value
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::test {

// Deterministic linear congruential generator. Same constants as tests/runtime/cxx/cxx_int_set.cpp.
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

} // namespace stappler::test

#endif /* TESTS_VSTORE_CHECK_VSTORE_CHECK_H_ */
