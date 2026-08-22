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

/* The diagnostic message registry: text addressed by number.

WHAT IT REPLACED. A control that is locked has to be able to say WHY, and it used to say so by
carrying a String next to its state - a heap allocation and a copy per node, for a sentence that is
identical in every instance of the same situation. Now the state carries a uint32_t.

THE TWO PROPERTIES WORTH CHECKING are the ones the callers rely on and neither can see:

  * the same text always yields the same code, so two modules naming one situation cannot hand a
    reader two numbers for it;
  * a code, once handed out, keeps answering - including after the storage has grown past a chunk
    boundary, which is exactly where a Vector-backed registry would have moved the entries out from
    under a StringView somebody was still holding.

The second is why the walk below registers more than a chunk's worth of messages and then re-reads
the FIRST one. */

#include "SPCommon.h"
#include "SPDiagnosticRegistry.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

// Registered the way the contract says a message must be: a constant of its module, over a literal
static const uint32_t s_first = diagnostic::registerMessage("the value arrives on a wire");
static const uint32_t s_second = diagnostic::registerMessage("this field is required");

} // namespace

void performDiagnosticRegistryTests() {
	check(s_first != diagnostic::NoMessage, "diag-registry: a registered message gets a code");
	check(s_second != s_first, "diag-registry: two messages get two codes");

	check(diagnostic::getMessage(s_first) == "the value arrives on a wire",
			"diag-registry: the code answers with its text");
	check(diagnostic::getMessage(s_second) == "this field is required",
			"diag-registry: ...and so does the second");

	// The property every caller leans on: naming the same situation twice is not two situations
	check(diagnostic::registerMessage("the value arrives on a wire") == s_first,
			"diag-registry: the same text yields the same code");

	check(diagnostic::getMessage(diagnostic::NoMessage).empty(),
			"diag-registry: NoMessage has no text");
	check(diagnostic::registerMessage(StringView()) == diagnostic::NoMessage,
			"diag-registry: an empty message IS NoMessage");
	check(diagnostic::getMessage(1'000'000).empty(),
			"diag-registry: a code nobody handed out has no text");

	// Past a chunk boundary (64), which is where a growing array would have relocated the entries
	// a previously handed-out StringView still points at.
	static constexpr const char *s_bulk[] = {"m00", "m01", "m02", "m03", "m04", "m05", "m06", "m07",
		"m08", "m09", "m10", "m11", "m12", "m13", "m14", "m15", "m16", "m17", "m18", "m19", "m20",
		"m21", "m22", "m23", "m24", "m25", "m26", "m27", "m28", "m29", "m30", "m31", "m32", "m33",
		"m34", "m35", "m36", "m37", "m38", "m39", "m40", "m41", "m42", "m43", "m44", "m45", "m46",
		"m47", "m48", "m49", "m50", "m51", "m52", "m53", "m54", "m55", "m56", "m57", "m58", "m59",
		"m60", "m61", "m62", "m63", "m64", "m65", "m66", "m67", "m68", "m69", "m70", "m71", "m72",
		"m73", "m74", "m75", "m76", "m77", "m78", "m79"};

	const auto before = diagnostic::getMessageCount();
	uint32_t codes[sizeof(s_bulk) / sizeof(s_bulk[0])] = {0};
	for (size_t i = 0; i < sizeof(s_bulk) / sizeof(s_bulk[0]); ++i) {
		codes[i] = diagnostic::registerMessage(StringView(s_bulk[i]));
	}

	check(diagnostic::getMessageCount() == before + uint32_t(sizeof(s_bulk) / sizeof(s_bulk[0])),
			"diag-registry: every message was kept");

	bool allDistinct = true;
	bool allAnswer = true;
	for (size_t i = 0; i < sizeof(s_bulk) / sizeof(s_bulk[0]); ++i) {
		if (codes[i] == diagnostic::NoMessage) {
			allDistinct = false;
		}
		for (size_t j = i + 1; j < sizeof(s_bulk) / sizeof(s_bulk[0]); ++j) {
			if (codes[i] == codes[j]) {
				allDistinct = false;
			}
		}
		if (diagnostic::getMessage(codes[i]) != StringView(s_bulk[i])) {
			allAnswer = false;
		}
	}
	check(allDistinct, "diag-registry: distinct texts got distinct codes across chunks");
	check(allAnswer, "diag-registry: every code answers with its own text");

	// ...and the code handed out BEFORE all that still answers, which is the whole reason the
	// storage is chunked rather than a growing array
	check(diagnostic::getMessage(s_first) == "the value arrives on a wire",
			"diag-registry: an old code survives the growth");
}

} // namespace STAPPLER_VERSIONIZED stappler
