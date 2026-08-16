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

// Two tests over sprt::unicode::, with deliberately different contracts.
//
// performUnicodeTests() asserts only what every platform must get right, which
// today means ASCII and structural properties: the case functions have seven
// separate backends (libunistring/ICU via dlopen, NDK ICU, CoreFoundation,
// LCMapStringEx, a JS host, and two ASCII-only stubs), and they do NOT agree
// beyond ASCII. A failure here is a real bug on that platform - notably, the
// linux backend returns 0, not the input, when neither library can be dlopen'd.
//
// performUnicodeCaseConformanceTests() measures the rest against the Unicode
// Character Database and REPORTS A SCORE rather than failing. That is the point:
// it is the baseline for docs/design/unicode-case-port-plan.adoc, whose stages 2
// and 4 replace all seven backends with one ported from ICU. Before that port the
// score differs per platform by design; after it, every platform should read
// 100%, and any drop is the regression signal.

#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#include "data/case_test.cc"

namespace sprt {

static int s_checks = 0;
static int s_failures = 0;

static void check(bool ok, StringView what) {
	++s_checks;
	if (!ok) {
		++s_failures;
		sprt::cerr << "  FAIL: " << what << "\n";
	}
}

// --- helpers -----------------------------------------------------------------

enum class CaseOp {
	Lower,
	Upper,
	Title,
};

// Runs one of the string case overloads into a fixed buffer. Records whether the
// backend claimed success and how many times it invoked the callback - both are
// part of the contract, and neither was looked at before.
struct MapResult {
	bool ok = false;
	int invocations = 0;
	char buf[256] = {0};
	StringView result;
};

static MapResult mapString(CaseOp op, StringView src) {
	MapResult r;
	auto sink = [&](StringView str) {
		++r.invocations;
		if (str.size() < sizeof(r.buf)) {
			for (size_t i = 0; i < str.size(); ++i) { r.buf[i] = str[i]; }
			r.result = StringView(r.buf, str.size());
		}
	};
	switch (op) {
	case CaseOp::Lower: r.ok = unicode::tolower(sink, src); break;
	case CaseOp::Upper: r.ok = unicode::toupper(sink, src); break;
	case CaseOp::Title: r.ok = unicode::totitle(sink, src); break;
	}
	return r;
}

struct WideMapResult {
	bool ok = false;
	int invocations = 0;
	char16_t buf[128] = {0};
	WideStringView result;
};

static WideMapResult mapWide(CaseOp op, WideStringView src) {
	WideMapResult r;
	auto sink = [&](WideStringView str) {
		++r.invocations;
		if (str.size() < sizeof(r.buf) / sizeof(char16_t)) {
			for (size_t i = 0; i < str.size(); ++i) { r.buf[i] = str[i]; }
			r.result = WideStringView(r.buf, str.size());
		}
	};
	switch (op) {
	case CaseOp::Lower: r.ok = unicode::tolower(sink, src); break;
	case CaseOp::Upper: r.ok = unicode::toupper(sink, src); break;
	case CaseOp::Title: r.ok = unicode::totitle(sink, src); break;
	}
	return r;
}

// Asserts a string overload: it succeeded, fired the callback exactly once, and
// produced exactly this output.
static void checkMapped(CaseOp op, StringView src, StringView expected, StringView what) {
	auto r = mapString(op, src);
	check(r.ok, what);
	if (!r.ok) {
		return;
	}
	check(r.invocations == 1, what);
	++s_checks;
	if (r.result != expected) {
		++s_failures;
		sprt::cerr << "  FAIL: " << what << ": got '" << r.result << "', expected '" << expected
				   << "'\n";
	}
}

static void checkMappedWide(CaseOp op, WideStringView src, WideStringView expected,
		StringView what) {
	auto r = mapWide(op, src);
	check(r.ok, what);
	if (!r.ok) {
		return;
	}
	check(r.invocations == 1, what);
	check(r.result == expected, what);
}

// --- strict: what every backend must get right --------------------------------

static void testAsciiCodepoints() {
	// The whole ASCII range, one code point at a time. A backend that cannot load
	// its library fails here rather than silently mapping everything to 0.
	bool lowerOk = true, upperOk = true, titleOk = true, otherOk = true, idempotent = true;
	for (char32_t c = 1; c < 0x80; ++c) {
		char32_t expectLower = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
		char32_t expectUpper = (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;

		if (unicode::tolower(c) != expectLower) {
			lowerOk = false;
		}
		if (unicode::toupper(c) != expectUpper) {
			upperOk = false;
		}
		// Titlecase equals uppercase throughout ASCII: no ASCII character has a
		// titlecase form distinct from its uppercase one.
		if (unicode::totitle(c) != expectUpper) {
			titleOk = false;
		}
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
				&& (unicode::tolower(c) != c || unicode::toupper(c) != c)) {
			otherOk = false;
		}
		// Applying the same fold twice must not move any further.
		if (unicode::tolower(unicode::tolower(c)) != unicode::tolower(c)
				|| unicode::toupper(unicode::toupper(c)) != unicode::toupper(c)) {
			idempotent = false;
		}
	}
	check(lowerOk, "tolower over ASCII");
	check(upperOk, "toupper over ASCII");
	check(titleOk, "totitle over ASCII");
	check(otherOk, "non-letter ASCII is unchanged");
	check(idempotent, "ASCII case folds are idempotent");
}

static void testAsciiStrings() {
	StringView mixed = "Hello, World! 123";
	checkMapped(CaseOp::Lower, mixed, "hello, world! 123", "tolower(StringView) over ASCII");
	checkMapped(CaseOp::Upper, mixed, "HELLO, WORLD! 123", "toupper(StringView) over ASCII");

	// A string with nothing to fold must come back byte-identical.
	StringView uncased = "0123456789 -_.!?";
	checkMapped(CaseOp::Lower, uncased, uncased, "tolower(StringView) leaves uncased text alone");
	checkMapped(CaseOp::Upper, uncased, uncased, "toupper(StringView) leaves uncased text alone");

	// Titlecasing an already-titlecased ASCII word is a fixed point on every
	// backend, whether it breaks words or only touches the first letter.
	checkMapped(CaseOp::Title, "Hello", "Hello", "totitle(StringView) fixes 'Hello'");
}

static void testWideStrings() {
	WideStringView mixed = u"Hello, World! 123";
	checkMappedWide(CaseOp::Lower, mixed, u"hello, world! 123",
			"tolower(WideStringView) over ASCII");
	checkMappedWide(CaseOp::Upper, mixed, u"HELLO, WORLD! 123",
			"toupper(WideStringView) over ASCII");
}

static void testComparators() {
	int result = 0;

	// A string compares equal to itself under both orderings, whatever collation
	// the backend happens to implement.
	if (unicode::compare(StringView("example"), StringView("example"), &result)) {
		check(result == 0, "compare(x, x) == 0");
	}
	if (unicode::caseCompare(StringView("Example"), StringView("example"), &result)) {
		check(result == 0, "caseCompare of strings differing only by case -> 0");
	}

	// Antisymmetry: whatever order the backend picks, it must pick it consistently.
	int forward = 0, backward = 0;
	if (unicode::compare(StringView("alpha"), StringView("beta"), &forward)
			&& unicode::compare(StringView("beta"), StringView("alpha"), &backward)) {
		check((forward < 0) == (backward > 0) && (forward == 0) == (backward == 0),
				"compare is antisymmetric");
	}

	// The comparator templates built on top of them.
	check(StringView("Example").equals<StringCaseComparator>(StringView("example")),
			"StringCaseComparator ignores ASCII case");
	check(!StringView("Example").equals<StringCaseComparator>(StringView("examples")),
			"StringCaseComparator still separates different strings");
}

void performUnicodeTests() {
	s_checks = 0;
	s_failures = 0;

	testAsciiCodepoints();
	testAsciiStrings();
	testWideStrings();
	testComparators();

	sprt::cout << "unicode tests: " << s_checks << " checks, " << s_failures << " failures\n";
}

// --- scored: how much of the UCD the current backend actually implements -------

struct Score {
	int matched = 0;
	int total = 0;

	void add(bool ok) {
		++total;
		if (ok) {
			++matched;
		}
	}

	// Whole percent is enough to see a regression and does not pretend the number
	// is more stable than it is.
	int percent() const { return total == 0 ? 100 : (matched * 100) / total; }
};

static void reportScore(StringView what, const Score &s) {
	sprt::cout << "  " << what << ": " << s.matched << "/" << s.total << " (" << s.percent()
			   << "%)\n";
}

void performUnicodeCaseConformanceTests() {
	Score simpleLower, simpleUpper, simpleTitle;
	for (auto &e : s_caseSimple) {
		simpleLower.add(unicode::tolower(e.cp) == e.lower);
		simpleUpper.add(unicode::toupper(e.cp) == e.upper);
		simpleTitle.add(unicode::totitle(e.cp) == e.title);
	}

	Score fullLower, fullUpper;
	for (auto &e : s_caseFull) {
		auto lower = mapString(CaseOp::Lower, e.source);
		fullLower.add(lower.ok && lower.result == StringView(e.lower));

		auto upper = mapString(CaseOp::Upper, e.source);
		fullUpper.add(upper.ok && upper.result == StringView(e.upper));
	}

	sprt::cout << "unicode case conformance (UCD " << int(s_caseUcdVersion[0]) << "."
			   << int(s_caseUcdVersion[1]) << "." << int(s_caseUcdVersion[2]) << "):\n";
	reportScore("simple tolower", simpleLower);
	reportScore("simple toupper", simpleUpper);
	reportScore("simple totitle", simpleTitle);
	reportScore("full tolower", fullLower);
	reportScore("full toupper", fullUpper);
	sprt::cout << "  (" << s_caseConditionalSkipped
			   << " conditional SpecialCasing rows not asserted: they need a locale)\n";
	sprt::cout << "unicode case conformance: reported, not asserted - see "
				  "docs/design/unicode-case-port-plan.adoc\n";
}

} // namespace sprt
