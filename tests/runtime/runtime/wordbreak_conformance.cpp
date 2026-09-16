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

// The Unicode UAX #29 word boundary conformance suite. This is the test that
// matters for titlecasing: the rules are a transcription of a specification, and
// a mis-transcription does not crash - it puts a word boundary in the wrong
// place, which shows up as one capital letter out of place in a sentence, if it
// shows up at all.
//
// The file is the standard, not ICU. ICU's own break iterator layers dictionary
// breaking and locale tailorings on top of UAX #29 and does not pass all of this
// file (its rbbitst.cpp carries a skip list). The runtime implements plain
// UAX #29, so every case here is asserted.
//
// This is the one test in the suite that reaches past the public API. The word
// breaker is not published - nothing outside the titlecaser needs it yet - but
// checking it through `totitle` alone would only exercise the boundaries that
// happen to fall on a cased letter, which is a small and unrepresentative
// fraction of these cases. The declaration below is the entry point in
// runtime/src/unicode/word_break.cc; the runtime's objects are linked into this
// executable directly, so it resolves.

#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>

#include "data/wordbreak_test.cc"

namespace sprt::unicode::detail {

int32_t findWordBoundaries(const char16_t *text, int32_t length, int32_t *out, int32_t outCapacity);

} // namespace sprt::unicode::detail

namespace sprt {

// Every case is short; the longest line in the file is a handful of code points.
static constexpr int32_t MaxBoundaries = 64;

static void printCase(const WordBreakTestCase &c, const int32_t *got, int32_t gotCount) {
	auto text = s_wordBreakTestText + c.textOffset;
	sprt::cerr << "    text:";
	for (uint16_t i = 0; i < c.textLength; ++i) {
		sprt::cerr << " " << uint32_t(text[i]);
	}
	sprt::cerr << "\n    expected:";
	for (uint16_t i = 0; i < c.boundsCount; ++i) {
		sprt::cerr << " " << uint32_t(s_wordBreakTestBounds[c.boundsOffset + i]);
	}
	sprt::cerr << "\n    got:     ";
	for (int32_t i = 0; i < gotCount && i < MaxBoundaries; ++i) {
		sprt::cerr << " " << uint32_t(got[i]);
	}
	sprt::cerr << "\n";
}

void performWordBreakConformanceTests() {
	int checks = 0;
	int failures = 0;
	int reported = 0;
	constexpr int MaxReported = 20;

	for (auto &c : s_wordBreakTestCases) {
		int32_t got[MaxBoundaries];
		auto gotCount = unicode::detail::findWordBoundaries(s_wordBreakTestText + c.textOffset,
				int32_t(c.textLength), got, MaxBoundaries);

		++checks;
		bool ok = gotCount == int32_t(c.boundsCount) && gotCount <= MaxBoundaries;
		if (ok) {
			for (int32_t i = 0; i < gotCount; ++i) {
				if (got[i] != int32_t(s_wordBreakTestBounds[c.boundsOffset + i])) {
					ok = false;
					break;
				}
			}
		}
		if (!ok) {
			++failures;
			if (reported++ < MaxReported) {
				sprt::cerr << "  FAIL: word boundaries differ\n";
				printCase(c, got, gotCount);
			}
		}
	}

	sprt::cout << "word break conformance (UAX #29 " << int(s_wordBreakUcdVersion[0]) << "."
			   << int(s_wordBreakUcdVersion[1]) << "." << int(s_wordBreakUcdVersion[2]) << ", "
			   << int(sizeof(s_wordBreakTestCases) / sizeof(s_wordBreakTestCases[0]))
			   << " cases): " << checks << " checks, " << failures << " failures\n";
	if (failures > MaxReported) {
		sprt::cout << "  (" << failures - MaxReported << " further failures not printed)\n";
	}
}

} // namespace sprt
