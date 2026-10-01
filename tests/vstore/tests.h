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

#ifndef TESTS_VSTORE_TESTS_H_
#define TESTS_VSTORE_TESTS_H_

#include "SPCommon.h"
#include "SPVStoreArena.h"

#include <sprt/runtime/stream.h>

namespace STAPPLER_VERSIONIZED stappler::test {

// Same assertions as tests/stappler: failures accumulate in one counter and main() returns it.
inline int s_failures = 0;

inline void check(bool cond, StringView name) {
	sprt::cout << (cond ? "[ OK ] " : "[FAIL] ") << name << "\n";
	if (!cond) {
		++s_failures;
	}
}

inline void checkEq(StringView got, StringView expect, StringView name) {
	bool ok = (got == expect);
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << name;
	if (!ok) {
		sprt::cout << "  (got \"" << got << "\", expected \"" << expect << "\")";
	}
	sprt::cout << "\n";
	if (!ok) {
		++s_failures;
	}
}

inline int failures() { return s_failures; }

// `--full` is the gate: every sweep and workload at full size. Without it a section runs its
// reduced arm - the same code, assertions and oracles, with fewer arms of a sweep and smaller
// workloads. Which arms a section drops is decided and explained in the section.
inline bool s_full = false;

inline bool full() { return s_full; }

// The two sizes of a workload, written at the constant so the full number stays visible.
inline uint32_t sized(uint32_t fullValue, uint32_t reducedValue) {
	return s_full ? fullValue : reducedValue;
}

} // namespace stappler::test

namespace STAPPLER_VERSIONIZED stappler {

// The kind most sections run on: it announces its writes and keeps a shadow copy to prove the
// barrier had no holes. In a release build it is TrackedArena. Sections about the kinds name
// PlainArena and TrackedArena directly.
using Arena = vstore::ShadowArena;

void performArenaBasicTests();
void performArenaRelocateTests();
void performArenaTortureTests();
void performArenaBarrierTests();
void performArenaOomTests();
void performArenaHeaderTests();
void performArenaKindsTests();
void performArenaWriteViewTests();
void performJournalPagesTests();
void performJournalRollbackTests();
void performJournalKeyframeTests();
void performJournalCompressTests();

} // namespace stappler

#endif /* TESTS_VSTORE_TESTS_H_ */
