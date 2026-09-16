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
#include "SPMemInterface.h"
#include "SPFontBidi.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

// Exercises the SheenBidi resolver wrapper (font::TextBidi): UAX #9 embedding-level resolution and
// the visual reordering of mixed-direction text. TextBidi allocates from the current memory pool, so
// the whole body runs inside an explicit memory::perform(pool) scope (mirroring the makefile test) —
// the resolver objects must not outlive that pool context.
void performBidiTests() {
	sprt::cout << "\n== stappler font bidi tests (SheenBidi resolver) ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		// "abc <aleph><bet> 12": Latin (LTR) + Hebrew aleph/bet (RTL) + digits.
		// UTF-8 bytes:  a  b  c  SP | D7 90 | D7 91 | SP  1  2  -> Hebrew occupies byte offsets 4..7.
		StringView text("abc \xD7\x90\xD7\x91 12", 11);

		font::TextBidi bidi;
		check(bidi.init(text, font::TextDirection::Neutral), "bidi: init UTF-8");
		check(bool(bidi), "bidi: valid after init");
		check(bidi.getLength() == uint32_t(text.size()), "bidi: length matches source");

		// --- paragraph levels (rules X1-I2) ---
		// The first strong character is Latin 'a', so the auto base level resolves to 0 (LTR).
		uint32_t paragraphs = 0;
		uint8_t baseLevel = 0xFF;
		bool hebrewIsRtl = false;
		bidi.foreachParagraph([&](uint32_t, uint32_t length, uint8_t base, SpanView<uint8_t> levels) {
			++paragraphs;
			baseLevel = base;
			// the two Hebrew code points span byte offsets 4..7 and must carry an odd (RTL) level
			if (length >= 8 && levels.size() >= 8) {
				hebrewIsRtl = (levels[4] & 1) != 0 && (levels[5] & 1) != 0;
			}
		});
		check(paragraphs == 1, "bidi: single paragraph");
		check(baseLevel == 0, "bidi: auto base level is LTR (first strong = Latin)");
		check(hebrewIsRtl, "bidi: Hebrew code units resolve to an odd (RTL) level");

		// --- visual reordering (rules L1-L2) ---
		// The runs must include both an LTR and an embedded RTL run and partition the whole line.
		uint32_t runCount = 0, covered = 0;
		bool anyRtl = false;
		bidi.foreachVisualRun(0, uint32_t(text.size()), [&](const font::BidiRun &run) {
			++runCount;
			covered += run.length;
			anyRtl = anyRtl || run.isRightToLeft();
		});
		check(runCount >= 2, "bidi: at least two visual runs (LTR + embedded RTL)");
		check(anyRtl, "bidi: at least one RTL visual run");
		check(covered == uint32_t(text.size()), "bidi: visual runs cover the whole line");

		// --- explicit RTL base override: the paragraph base level becomes odd ---
		font::TextBidi rtl;
		rtl.init(text, font::TextDirection::RightToLeft);
		uint8_t rtlBase = 0;
		rtl.foreachParagraph([&](uint32_t, uint32_t, uint8_t base, SpanView<uint8_t>) {
			rtlBase = base;
		});
		check((rtlBase & 1) != 0, "bidi: forced RTL base level is odd");

		// --- pure LTR text reorders to a single LTR run ---
		font::TextBidi ltr;
		ltr.init(StringView("hello world", 11), font::TextDirection::Neutral);
		uint32_t ltrRuns = 0;
		bool ltrAllLtr = true;
		ltr.foreachVisualRun(0, 11, [&](const font::BidiRun &run) {
			++ltrRuns;
			if (run.isRightToLeft()) {
				ltrAllLtr = false;
			}
		});
		check(ltrRuns == 1 && ltrAllLtr, "bidi: pure-LTR text is one LTR run");

		// --- empty / invalid input is handled gracefully ---
		font::TextBidi empty;
		check(!empty.init(StringView()), "bidi: init on empty input returns false");
		check(!bool(empty), "bidi: empty resolver is invalid");
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
