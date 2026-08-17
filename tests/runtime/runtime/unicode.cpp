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
// performUnicodeTests() asserts. Lowercasing and uppercasing are now one
// implementation on every platform (runtime/src/unicode, ported from ICU), so
// everything about them can be required rather than measured - including the
// mappings that produce several characters from one, the ones that depend on the
// text around them, and the ones that depend on the language. Nothing here is
// platform code any more: titlecasing brought its own UAX #29 word breaker, and
// the two orderings - code point order, and code point order after full case
// folding - came off the platform with it. Collation did not come with them, and
// is not what these compare.
//
// performUnicodeCaseConformanceTests() walks the whole Unicode Character
// Database and REPORTS A SCORE rather than failing. It was the baseline for
// docs/design/unicode-case-port-plan.adoc while the port replaced seven
// disagreeing backends one stage at a time; every column reads 100% now, and a
// drop is the regression signal.

#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#include "data/case_test.cc"

namespace sprt::unicode::detail {

// The batch case folder, declared here rather than published: see foldWide().
int32_t mapFoldUtf16(uint32_t options, char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength);

} // namespace sprt::unicode::detail

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

// Same, for a specific language.
static MapResult mapString(CaseOp op, StringView src, StringView locale) {
	MapResult r;
	auto sink = [&](StringView str) {
		++r.invocations;
		if (str.size() < sizeof(r.buf)) {
			for (size_t i = 0; i < str.size(); ++i) { r.buf[i] = str[i]; }
			r.result = StringView(r.buf, str.size());
		}
	};
	switch (op) {
	case CaseOp::Lower: r.ok = unicode::tolower(sink, src, locale); break;
	case CaseOp::Upper: r.ok = unicode::toupper(sink, src, locale); break;
	case CaseOp::Title: r.ok = unicode::totitle(sink, src, locale); break;
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

static WideMapResult mapWide(CaseOp op, WideStringView src, StringView locale) {
	WideMapResult r;
	auto sink = [&](WideStringView str) {
		++r.invocations;
		if (str.size() < sizeof(r.buf) / sizeof(char16_t)) {
			for (size_t i = 0; i < str.size(); ++i) { r.buf[i] = str[i]; }
			r.result = WideStringView(r.buf, str.size());
		}
	};
	switch (op) {
	case CaseOp::Lower: r.ok = unicode::tolower(sink, src, locale); break;
	case CaseOp::Upper: r.ok = unicode::toupper(sink, src, locale); break;
	case CaseOp::Title: r.ok = unicode::totitle(sink, src, locale); break;
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

static void checkMapped(CaseOp op, StringView src, StringView locale, StringView expected,
		StringView what) {
	auto r = mapString(op, src, locale);
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

// Every code point the UCD gives no case mapping must come back unchanged. The
// conformance test below checks the 2989 code points that DO map; this is the
// complementary half, and it is the one that catches a mis-read table: a wrong
// trie index returns a perfectly valid mapping for a code point that should have
// none, which nothing else here would notice.
//
// s_caseSimple is in code point order (UnicodeData.txt order), so this walks the
// two sequences together instead of searching.
static void testUnmappedCodepoints() {
	constexpr size_t entries = sizeof(s_caseSimple) / sizeof(s_caseSimple[0]);
	size_t next = 0;
	char32_t firstBad = 0;
	int bad = 0;

	for (char32_t c = 0; c <= 0x10'FFFF; ++c) {
		while (next < entries && s_caseSimple[next].cp < c) { ++next; }
		if (next < entries && s_caseSimple[next].cp == c) {
			continue; // has a mapping; the conformance test owns this one
		}
		if (unicode::tolower(c) != c || unicode::toupper(c) != c || unicode::totitle(c) != c) {
			if (bad == 0) {
				firstBad = c;
			}
			++bad;
		}
	}

	++s_checks;
	if (bad != 0) {
		++s_failures;
		sprt::cerr << "  FAIL: " << bad
				   << " unmapped code points were changed, first at decimal "
				   << uint32_t(firstBad) << "\n";
	}
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

// The mappings that produce more than one character, and the one context rule
// that applies in every language. These are what a per-code-point API cannot
// express at all, so before the port no backend implemented all of them and
// several implemented none.
static void testFullMappings() {
	// One character in, several out. The uppercase of these has no single code
	// point, so a mapper that works code point by code point returns the input.
	checkMapped(CaseOp::Upper, "ß", "SS", "toupper: sharp s -> SS");
	checkMapped(CaseOp::Upper, "ﬁ", "FI", "toupper: fi ligature -> FI");
	checkMapped(CaseOp::Upper, "ŉ", "ʼN", "toupper: n preceded by apostrophe");
	checkMapped(CaseOp::Upper, "straße", "STRASSE", "toupper: sharp s inside a word");

	// Final_Sigma, the only SpecialCasing condition with no language attached:
	// a capital sigma lowercases to the final form only at the end of a word.
	// (The vector table below is what claims this condition as covered.)
	checkMapped(CaseOp::Lower, "ΣΑ", "σα", "tolower: sigma at start of word");
	checkMapped(CaseOp::Lower, "ΑΣΑ", "ασα", "tolower: sigma inside a word");
	// A cased letter after a case-ignorable one still counts as following, so
	// this sigma is NOT final.
	checkMapped(CaseOp::Lower, "ΑΣ'Α", "ασ'α",
			"tolower: sigma before an apostrophe and a letter");

	// A supplementary-plane pair must survive as a pair.
	checkMapped(CaseOp::Upper, "\U00010428", "\U00010400", "toupper: Deseret, outside the BMP");
	checkMapped(CaseOp::Lower, "\U00010400", "\U00010428", "tolower: Deseret, outside the BMP");

	// Unpaired surrogates and lone code units are not text, but they must not be
	// destroyed either: a mapper that decodes them as U+0000 would silently
	// truncate. (UTF-16 only - the UTF-8 side cannot represent them.)
	checkMappedWide(CaseOp::Lower, WideStringView(u"A\xd800Z", 3), WideStringView(u"a\xd800z", 3),
			"tolower(WideStringView) keeps an unpaired surrogate");
}

// Titlecasing is the one case operation that works on words rather than
// characters, so what it gets right or wrong is where the word boundaries fall.
// The boundaries themselves are checked exhaustively by
// runtime_wordbreak_conformance; these are the cases that show what they mean
// for the result.
static void testTitleMappings() {
	// Every word gets a capital, and the rest of each word is lowercased - the
	// second half is what "titlecase" means beyond "capitalize".
	checkMapped(CaseOp::Title, "hello WORLD", "Hello World", "totitle: one capital per word");
	checkMapped(CaseOp::Title, "привет МИР", "Привет Мир", "totitle: Cyrillic");
	checkMapped(CaseOp::Title, "Hello", "Hello", "totitle fixes 'Hello'");

	// The reason for UAX #29 rather than "capitalize after every non-letter":
	// these disagree with that rule, in both directions. An apostrophe holds a
	// word together (WB6/WB7), so O'brien gets one capital and not two; a hyphen
	// does not, so well-known gets two; an underscore joins (WB13a/WB13b) while a
	// space of course does not.
	checkMapped(CaseOp::Title, "o'brien", "O'brien", "totitle: an apostrophe does not start a word");
	checkMapped(CaseOp::Title, "well-known", "Well-Known", "totitle: a hyphen does");
	checkMapped(CaseOp::Title, "x1_2 y", "X1_2 Y", "totitle: underscore joins, space does not");

	// A word need not begin with the letter that gets titlecased: the search
	// moves on to the first letter, number or symbol. When that turns out to be a
	// number, nothing in the word is capitalized at all.
	checkMapped(CaseOp::Title, "\"hello\"", "\"Hello\"",
			"totitle: the quote is skipped, not titlecased");
	checkMapped(CaseOp::Title, "42nd", "42nd", "totitle: a digit is where the word starts");

	// The first cased letter of a word may be a 1:N mapping, and the rest of the
	// word is lowercased with full context - here the final sigma.
	checkMapped(CaseOp::Title, "ﬂoor", "Floor", "totitle: ligature expands");
	checkMapped(CaseOp::Title, "ΟΔΟΣ", "Οδος", "totitle: final sigma in the lowercased tail");

	// Without a dictionary these scripts have no internal word boundaries. Both
	// come back unchanged because neither is cased - which is the point: the
	// limitation costs nothing here.
	checkMapped(CaseOp::Title, "กรุงเทพมหานคร", "กรุงเทพมหานคร",
			"totitle: Thai has no dictionary and no case");
	checkMapped(CaseOp::Title, "東京都", "東京都", "totitle: CJK breaks per ideograph and is uncased");

	// Empty input still fires the callback exactly once.
	checkMapped(CaseOp::Title, "", "", "totitle: empty input");
}

// Hand-written inputs for the SpecialCasing rows that only fire for a particular
// language. Each names the condition it stands for; testLocaleCoverage() checks
// that between them they name every condition the UCD has.
struct LocaleVector {
	CaseOp op;
	const char *src;
	const char *locale;
	const char *expected;
	const char *covers; // the SpecialCasing condition, verbatim, or "" if none
	const char *what;
};

static constexpr LocaleVector s_localeVectors[] = {
	// Final_Sigma is the one SpecialCasing condition with no language attached,
	// so it belongs here for coverage even though the locale is root.
	{CaseOp::Lower, "ΑΣ", "", "ας", "Final_Sigma", "tolower: sigma at end of word"},

	// Turkish and Azerbaijani: I and i-dotless, I-dot and i are the case pairs.
	{CaseOp::Upper, "i", "tr", "İ", "tr", "toupper tr: i -> I with dot"},
	{CaseOp::Upper, "i", "az", "İ", "az", "toupper az: i -> I with dot"},
	{CaseOp::Lower, "İ", "tr", "i", "tr", "tolower tr: I with dot -> i"},
	{CaseOp::Lower, "İ", "az", "i", "az", "tolower az: I with dot -> i"},
	{CaseOp::Lower, "I", "tr", "ı", "tr Not_Before_Dot", "tolower tr: I -> dotless i"},
	{CaseOp::Lower, "I", "az", "ı", "az Not_Before_Dot", "tolower az: I -> dotless i"},
	// I followed by a combining dot above is the decomposed I-with-dot, so the
	// dot is absorbed rather than left behind.
	{CaseOp::Lower, "İ", "tr", "i", "tr After_I", "tolower tr: I + dot above -> i"},
	{CaseOp::Lower, "İ", "az", "i", "az After_I", "tolower az: I + dot above -> i"},
	// Turkish rules must not leak into the root locale.
	{CaseOp::Lower, "I", "", "i", "", "tolower root: I -> i, not dotless"},
	{CaseOp::Upper, "i", "", "I", "", "toupper root: i -> I, not dotted"},
	{CaseOp::Lower, "İ", "", "i̇", "", "tolower root: I with dot -> i + dot above"},
	// The language subtag is what matters, in either case and either length.
	{CaseOp::Lower, "I", "TR-TR", "ı", "", "tolower TR-TR: region and case ignored"},
	{CaseOp::Lower, "I", "tur", "ı", "", "tolower tur: 3-letter code"},
	{CaseOp::Lower, "I", "trx", "i", "", "tolower trx: not Turkish, so root"},

	// Lithuanian keeps the dot on a lowercase i under an accent...
	{CaseOp::Lower, "Ì", "lt", "i̇̀", "lt More_Above",
			"tolower lt: I with an accent above keeps the dot"},
	{CaseOp::Lower, "J̀", "lt", "j̇̀", "lt More_Above",
			"tolower lt: J with an accent above keeps the dot"},
	{CaseOp::Lower, "Ì", "lt", "i̇̀", "lt",
			"tolower lt: precomposed I-grave decomposes with a dot"},
	// ... and drops it again when the letter goes back to upper or title case.
	{CaseOp::Upper, "i̇", "lt", "I", "lt After_Soft_Dotted",
			"toupper lt: dot above a soft-dotted i is removed"},
	// Without the language, none of that happens.
	{CaseOp::Lower, "Ì", "", "ì", "", "tolower root: no Lithuanian dot"},

	// Dutch writes IJ with both letters capital, which is the one case where a
	// word start produces two capitals. It only applies to a real digraph: the
	// negatives below are the conditions maybeTitleDutchIJ tests.
	{CaseOp::Title, "ijsland", "nl", "IJsland", "", "totitle nl: IJ takes two capitals"},
	{CaseOp::Title, "ijsland", "", "Ijsland", "", "totitle root: IJ is just an i"},
	{CaseOp::Title, "ixsland", "nl", "Ixsland", "", "totitle nl: no j, no digraph"},
	{CaseOp::Title, "íj", "nl", "Íj", "", "totitle nl: acute on the i only"},
	{CaseOp::Title, "íj́", "nl", "ÍJ́", "", "totitle nl: acute on both"},
	{CaseOp::Title, "íj́́", "nl", "Íj́́", "",
			"totitle nl: a further combining mark cancels it"},
	{CaseOp::Title, "iJsland", "nl", "IJsland", "", "totitle nl: the J is already capital"},

	// Turkish titlecase follows the same dotted-I rule as uppercase.
	{CaseOp::Title, "istanbul", "tr", "İstanbul", "", "totitle tr: i takes a dot"},
	{CaseOp::Title, "istanbul", "", "Istanbul", "", "totitle root: i does not"},

	// Armenian: the ech-yiwn ligature uppercases differently in the east.
	{CaseOp::Upper, "և", "hy", "ԵՎ", "", "toupper hy: ech-yiwn -> ech + vew"},
	{CaseOp::Upper, "և", "", "ԵՒ", "", "toupper root: ech-yiwn -> ech + yiwn"},

	// Greek uppercasing drops the tonos, which is a property of the string rather
	// than of any character in it. ICU gates the whole Greek path on the locale,
	// so root leaves the accents alone - that is not a gap, it is the contract.
	{CaseOp::Upper, "οδός", "el", "ΟΔΟΣ", "", "toupper el: drops the tonos"},
	{CaseOp::Upper, "οδός", "", "ΟΔΌΣ", "", "toupper root: keeps the tonos"},
	// A tonos removed from one vowel becomes a dialytika on the next, so that
	// the pair still does not read as a diphthong.
	{CaseOp::Upper, "άι", "el", "ΑΪ", "", "toupper el: adds a dialytika"},
	{CaseOp::Upper, "μάιος", "el", "ΜΑΪΟΣ", "", "toupper el: dialytika inside a word"},
	// Eta with a tonos standing alone is the disjunctive "or" and keeps it.
	{CaseOp::Upper, "ή", "el", "Ή", "", "toupper el: lone eta keeps its tonos"},
	{CaseOp::Upper, "ήταν", "el", "ΗΤΑΝ", "", "toupper el: eta in a word loses it"},
};

static void testLocaleMappings() {
	for (auto &v : s_localeVectors) {
		checkMapped(v.op, StringView(v.src), StringView(v.locale), StringView(v.expected),
				StringView(v.what));
	}
}

// The same vectors through the UTF-16 overloads. This is not redundant: the
// contextual rules are the one place where the two encodings run genuinely
// different code - each has its own context iterator, walking its own units
// backwards to answer "is this I preceded by a dot above?". The conformance run
// cannot cover it, because the rows it walks have no context.
static void testLocaleMappingsWide() {
	int disagreed = 0;
	const char *firstBad = nullptr;

	for (auto &v : s_localeVectors) {
		bool matched = false;
		unicode::toUtf16([&](WideStringView src) {
			auto r = mapWide(v.op, src, StringView(v.locale));
			if (!r.ok) {
				return;
			}
			unicode::toUtf16([&](WideStringView expected) { matched = r.result == expected; },
					StringView(v.expected));
		}, StringView(v.src));
		if (!matched) {
			if (disagreed == 0) {
				firstBad = v.what;
			}
			++disagreed;
		}
	}

	++s_checks;
	if (disagreed != 0) {
		++s_failures;
		sprt::cerr << "  FAIL: " << disagreed
				   << " locale vectors differ through UTF-16, first is '" << firstBad << "'\n";
	}
}

// Every conditional row in SpecialCasing.txt must be claimed by a vector above.
// Without this a rule added in a future UCD would simply go untested: the
// conformance run below skips the conditional rows by construction, because
// their inputs are single characters with no context.
static void testLocaleCoverage() {
	int uncovered = 0;
	StringView firstMissing;
	for (auto &e : s_caseConditional) {
		StringView condition = e.condition;
		bool covered = false;
		for (auto &v : s_localeVectors) {
			if (condition == StringView(v.covers)) {
				covered = true;
				break;
			}
		}
		if (!covered) {
			if (uncovered == 0) {
				firstMissing = condition;
			}
			++uncovered;
		}
	}

	++s_checks;
	if (uncovered != 0) {
		++s_failures;
		sprt::cerr << "  FAIL: " << uncovered
				   << " SpecialCasing conditions have no test vector, first is '" << firstMissing
				   << "'\n";
	}
}

// UTF-8 and UTF-16 are two implementations of the same mapping, and they are
// allowed to differ only in encoding. Running the whole UCD through both and
// comparing is what keeps them from drifting apart.
static void testEncodingsAgree() {
	int disagreed = 0;
	StringView firstBad;

	auto compareBoth = [&](CaseOp op, StringView src) {
		auto viaUtf8 = mapString(op, src);
		bool sameText = false;
		unicode::toUtf16([&](WideStringView wide) {
			auto viaUtf16 = mapWide(op, wide);
			if (!viaUtf8.ok || !viaUtf16.ok) {
				return;
			}
			unicode::toUtf8([&](StringView back) { sameText = back == viaUtf8.result; },
					viaUtf16.result);
		}, src);
		if (!sameText) {
			if (disagreed == 0) {
				firstBad = src;
			}
			++disagreed;
		}
	};

	for (auto &e : s_caseFull) {
		compareBoth(CaseOp::Lower, e.source);
		compareBoth(CaseOp::Upper, e.source);
	}
	// Plus a code point per plane's worth of the simple mappings; the full sweep
	// is what the conformance run does, and doing it twice here would double a
	// test that already takes a moment.
	for (size_t i = 0; i < sizeof(s_caseSimple) / sizeof(s_caseSimple[0]); i += 37) {
		char buf[8] = {0};
		auto len = unicode::utf8EncodeBuf(buf, sizeof(buf), s_caseSimple[i].cp);
		compareBoth(CaseOp::Lower, StringView(buf, len));
		compareBoth(CaseOp::Upper, StringView(buf, len));
	}

	++s_checks;
	if (disagreed != 0) {
		++s_failures;
		sprt::cerr << "  FAIL: UTF-8 and UTF-16 disagreed on " << disagreed
				   << " mappings, first for '" << firstBad << "'\n";
	}
}

// Ill-formed UTF-8 must come back byte-identical. The mapper decodes to decide
// what to change, and a permissive decoder would turn an overlong encoding of
// 'A' into a real 'a' - malformed input quietly becoming well-formed text. Each
// case below is a sequence that decodes to a cased letter under a decoder that
// does not check, and to nothing under one that does.
static void testIllFormedUtf8() {
	static constexpr struct {
		const char *bytes;
		size_t size;
		const char *what;
	} cases[] = {
		{"\xc1\x81", 2, "overlong two-byte 'A'"},
		{"\xe0\x81\x81", 3, "overlong three-byte 'A'"},
		{"\xf0\x80\x81\x81", 4, "overlong four-byte 'A'"},
		{"\xed\xa0\x80", 3, "a surrogate encoded as UTF-8"},
		{"\xf5\x80\x80\x80", 4, "a code point above U+10FFFF"},
		{"\xc3", 1, "a truncated two-byte sequence"},
		{"\x80\x80", 2, "stray continuation bytes"},
	};

	for (auto &c : cases) {
		StringView src(c.bytes, c.size);
		for (auto op : {CaseOp::Lower, CaseOp::Upper}) {
			auto r = mapString(op, src);
			++s_checks;
			if (!r.ok || r.result != src) {
				++s_failures;
				sprt::cerr << "  FAIL: " << (op == CaseOp::Lower ? "tolower" : "toupper")
						   << " did not pass through " << c.what << "\n";
			}
		}
	}

	// The valid text around a bad sequence still has to be mapped, and the mapper
	// has to resync exactly at the offending byte to find it: one byte too far and
	// a letter is swallowed, one too few and a stray byte is decoded as one.
	checkMapped(CaseOp::Upper, StringView("a\xc1\x81z", 4), StringView("A\xc1\x81Z", 4),
			"toupper maps around an ill-formed sequence");
	checkMapped(CaseOp::Lower, StringView("A\xc3", 2), StringView("a\xc3", 2),
			"tolower maps a letter before a truncated sequence");
	checkMapped(CaseOp::Lower, StringView("\xc3\x41", 2), StringView("\xc3\x61", 2),
			"tolower maps a letter that follows a lone lead byte");
}

// --- comparison --------------------------------------------------------------
//
// Both orderings come from the tables now, so these can assert values. Before
// the port this test could only assert that the backend was self-consistent -
// the actual order depended on the installed library and on LC_COLLATE, so there
// was no right answer to compare against.

static int sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

// The batch case folder. Same reasoning as the word breaker in
// wordbreak_conformance.cpp: this is not public API, but it is the independent
// implementation of what case_compare.cc does one code point at a time, and the
// runtime's objects link into this executable directly, so it resolves.
static WideStringView foldWide(WideStringView src, char16_t *buf, int32_t capacity) {
	auto n = unicode::detail::mapFoldUtf16(0, buf, capacity, src.data(), int32_t(src.size()));
	if (n < 0 || n > capacity) {
		return WideStringView();
	}
	return WideStringView(buf, size_t(n));
}

static void testCodepointOrder() {
	check(unicode::compareCodepoints(StringView("example"), StringView("example")) == 0,
			"compareCodepoints(x, x) == 0");
	check(unicode::compareCodepoints(StringView("abc"), StringView("abcd")) < 0,
			"compareCodepoints: a prefix sorts first");
	check(sign(unicode::compareCodepoints(StringView("alpha"), StringView("beta")))
					== -sign(unicode::compareCodepoints(StringView("beta"), StringView("alpha"))),
			"compareCodepoints is antisymmetric");

	// Code point order is not code unit order, and this is the only place they
	// differ: U+10000 is written as the surrogate pair D800 DC00, whose first
	// unit is below U+FFFD. An implementation that compares char16_t
	// element-wise - which is what memcmp and the old byte-wise comparator do -
	// gets this backwards. It is the entire reason the UTF-16 overload decodes.
	const char16_t bmp[] = {0xfffd};
	const char16_t supplementary[] = {0xd800, 0xdc00}; // U+10000
	check(unicode::compareCodepoints(WideStringView(bmp, 1), WideStringView(supplementary, 2)) < 0,
			"compareCodepoints: U+FFFD < U+10000 in UTF-16");
	check(unicode::compareCodepoints(WideStringView(supplementary, 2), WideStringView(bmp, 1)) > 0,
			"compareCodepoints: U+10000 > U+FFFD in UTF-16");

	// The same two, through the encodings where the order is the natural one, so
	// the UTF-16 answer is pinned to something independent of it.
	check(unicode::compareCodepoints(StringView("\xef\xbf\xbd"), StringView("\xf0\x90\x80\x80")) < 0,
			"compareCodepoints: U+FFFD < U+10000 in UTF-8");
	const char32_t bmp32[] = {0xfffd};
	const char32_t supplementary32[] = {0x1'0000};
	check(unicode::compareCodepoints(StringViewBase<char32_t>(bmp32, 1),
				  StringViewBase<char32_t>(supplementary32, 1))
					< 0,
			"compareCodepoints: U+FFFD < U+10000 in UTF-32");
}

static void testFoldedOrder() {
	// Full case folding maps one character to several, which no towupper-style
	// fold can do - and which is why "ß" and "ss" are the same string here.
	struct FoldVector {
		StringView l;
		StringView r;
		bool equal;
		StringView what;
	};

	// clang-format off
	static constexpr FoldVector vectors[] = {
		{"Example", "example", true, "compareFolded ignores ASCII case"},
		{"Example", "examples", false, "compareFolded still separates different strings"},
		{"\xc3\x9f", "ss", true, "compareFolded: LATIN SMALL LETTER SHARP S folds to ss"},
		{"\xc3\x9f", "SS", true, "compareFolded: sharp s equals SS"},
		{"stra\xc3\x9f""e", "STRASSE", true, "compareFolded: straße equals STRASSE"},
		{"\xef\xac\x81", "fi", true, "compareFolded: ligature fi folds to two letters"},
		{"\xef\xac\x84", "ffl", true, "compareFolded: ligature ffl folds to three letters"},
		// The three forms of sigma are one letter for folding purposes, which is
		// the whole point of Final_Sigma existing in the case mappings.
		{"\xcf\x82", "\xcf\x83", true, "compareFolded: final sigma equals sigma"},
		{"\xce\xa3", "\xcf\x82", true, "compareFolded: capital sigma equals final sigma"},
		// Folding is language-independent by design: the dotted capital I of
		// Turkish folds to i + combining dot above, not to a plain i, and this
		// overload takes no locale to change that with.
		{"\xc4\xb0", "i", false, "compareFolded: dotted capital I does not fold to i"},
		{"\xc4\xb0", "i\xcc\x87", true, "compareFolded: dotted capital I folds to i + dot above"},
	};
	// clang-format on

	for (auto &v : vectors) {
		auto direct = unicode::compareFolded(v.l, v.r);
		check((direct == 0) == v.equal, v.what);
		check(sign(direct) == -sign(unicode::compareFolded(v.r, v.l)),
				"compareFolded is antisymmetric");

		// Every vector through all three encodings: the three take different code
		// paths through the same folding engine, and they must not disagree.
		unicode::toUtf16([&](WideStringView l16) {
			unicode::toUtf16([&](WideStringView r16) {
				check(sign(unicode::compareFolded(l16, r16)) == sign(direct),
						"compareFolded agrees between utf-8 and utf-16");
			}, v.r);
		}, v.l);
		unicode::toUtf32([&](StringViewBase<char32_t> l32) {
			unicode::toUtf32([&](StringViewBase<char32_t> r32) {
				check(sign(unicode::compareFolded(l32, r32)) == sign(direct),
						"compareFolded agrees between utf-8 and utf-32");
			}, v.r);
		}, v.l);
	}

	check(unicode::compareFolded(StringView("abc"), StringView("abcd")) < 0,
			"compareFolded: a prefix sorts first");
}

// A byte that starts no well-formed sequence is not a character and cannot be
// folded, but the result still has to be a total order - these functions sit
// under container comparators, where a comparison that is not one silently
// corrupts the container rather than failing.
static void testIllFormedOrder() {
	auto truncated = StringView("a\xc3", 2);
	auto overlong = StringView("a\xc0\xaf", 3);
	auto plain = StringView("a");

	check(sign(unicode::compareFolded(truncated, plain))
					== -sign(unicode::compareFolded(plain, truncated)),
			"compareFolded is antisymmetric on ill-formed input");
	check(unicode::compareFolded(truncated, plain) > 0,
			"compareFolded: an ill-formed byte sorts after the end of the string");
	check(unicode::compareFolded(truncated, overlong) != 0,
			"compareFolded: different ill-formed bytes stay different");
	check(unicode::compareFolded(truncated, truncated) == 0,
			"compareFolded: an ill-formed string equals itself");
	check(unicode::compareFolded(StringView("\xc3\xa9"), StringView("\xc3")) < 0,
			"compareFolded: a real character sorts before an ill-formed byte");
}

// Every code point in the space, folded by the batch mapper and compared against
// itself by the incremental one. Full case folding is idempotent, so the two
// must agree on all 0x110000 - and this is what catches a pending run that is
// read one unit short or dropped at the end of the input, which is where a
// hand-written incremental folder goes wrong.
static void testFoldSweep() {
	int mismatches = 0;
	char32_t firstMismatch = 0;
	for (char32_t c = 0; c < 0x11'0000; ++c) {
		if (c >= 0xd800 && c <= 0xdfff) {
			continue; // a lone surrogate is not a character
		}
		char16_t src[2];
		auto srcLength = unicode::utf16EncodeBuf(src, 2, c);

		char16_t buf[8];
		auto folded = foldWide(WideStringView(src, srcLength), buf, 8);
		if (unicode::compareFolded(WideStringView(src, srcLength), folded) != 0) {
			if (mismatches == 0) {
				firstMismatch = c;
			}
			++mismatches;
		}
	}
	if (mismatches != 0) {
		sprt::cerr << "  (" << mismatches << " code points, first U+" << firstMismatch << ")\n";
	}
	check(mismatches == 0, "compareFolded matches the batch folder over the whole code space");
}

// The same check with two different strings, over the rows the conformance suite
// already reads: sign(compareFolded(a, b)) must equal
// sign(compareCodepoints(fold(a), fold(b))), which is the definition the
// incremental engine is supposed to implement.
static void testFoldMatchesBatch() {
	constexpr int32_t Capacity = 64;
	int mismatches = 0;
	auto count = int(sizeof(s_caseFull) / sizeof(s_caseFull[0]));
	for (int i = 0; i < count; ++i) {
		// Each row against its own mappings and against the next row, so the
		// pairs cover both "equal after folding" and "different".
		StringView pairs[][2] = {
			{s_caseFull[i].source, s_caseFull[i].lower},
			{s_caseFull[i].source, s_caseFull[i].upper},
			{s_caseFull[i].source, s_caseFull[(i + 1) % count].source},
		};
		for (auto &p : pairs) {
			auto incremental = sign(unicode::compareFolded(p[0], p[1]));
			unicode::toUtf16([&](WideStringView l16) {
				unicode::toUtf16([&](WideStringView r16) {
					char16_t lbuf[Capacity], rbuf[Capacity];
					auto batch = sign(unicode::compareCodepoints(foldWide(l16, lbuf, Capacity),
							foldWide(r16, rbuf, Capacity)));
					if (batch != incremental) {
						++mismatches;
					}
				}, p[1]);
			}, p[0]);
		}
	}
	check(mismatches == 0,
			"compareFolded matches fold-then-compare over the SpecialCasing corpus");
}

static void testComparators() {
	testCodepointOrder();
	testFoldedOrder();
	testIllFormedOrder();
	testFoldSweep();
	testFoldMatchesBatch();

	// The comparator templates built on top of them.
	check(StringView("Example").equals<StringCaseComparator>(StringView("example")),
			"StringCaseComparator ignores ASCII case");
	check(!StringView("Example").equals<StringCaseComparator>(StringView("examples")),
			"StringCaseComparator still separates different strings");
	check(StringView("\xc3\x9f").equals<StringUnicodeCaseComparator>(StringView("ss")),
			"StringUnicodeCaseComparator folds one character into two");
}

void performUnicodeTests() {
	s_checks = 0;
	s_failures = 0;

	testAsciiCodepoints();
	testUnmappedCodepoints();
	testAsciiStrings();
	testWideStrings();
	testFullMappings();
	testTitleMappings();
	testLocaleMappings();
	testLocaleMappingsWide();
	testLocaleCoverage();
	testEncodingsAgree();
	testIllFormedUtf8();
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

	// The same rows through both string overloads. Keeping the two apart matters:
	// they were separate backends before the port, and they are separate code
	// paths after it.
	Score fullLower, fullUpper, fullTitle, wideLower, wideUpper, wideTitle;
	for (auto &e : s_caseFull) {
		auto lower = mapString(CaseOp::Lower, e.source);
		fullLower.add(lower.ok && lower.result == StringView(e.lower));

		auto upper = mapString(CaseOp::Upper, e.source);
		fullUpper.add(upper.ok && upper.result == StringView(e.upper));

		// Each source is a single code point, so it is a whole word and its
		// titlecasing is the file's title column - no word breaking involved.
		auto title = mapString(CaseOp::Title, e.source);
		fullTitle.add(title.ok && title.result == StringView(e.title));

		unicode::toUtf16([&](WideStringView src) {
			auto wl = mapWide(CaseOp::Lower, src);
			unicode::toUtf16([&](WideStringView expected) {
				wideLower.add(wl.ok && wl.result == expected);
			}, StringView(e.lower));

			auto wu = mapWide(CaseOp::Upper, src);
			unicode::toUtf16([&](WideStringView expected) {
				wideUpper.add(wu.ok && wu.result == expected);
			}, StringView(e.upper));

			auto wt = mapWide(CaseOp::Title, src);
			unicode::toUtf16([&](WideStringView expected) {
				wideTitle.add(wt.ok && wt.result == expected);
			}, StringView(e.title));
		}, StringView(e.source));
	}

	sprt::cout << "unicode case conformance (UCD " << int(s_caseUcdVersion[0]) << "."
			   << int(s_caseUcdVersion[1]) << "." << int(s_caseUcdVersion[2]) << "):\n";
	reportScore("simple tolower", simpleLower);
	reportScore("simple toupper", simpleUpper);
	reportScore("simple totitle", simpleTitle);
	reportScore("full tolower (utf-8)", fullLower);
	reportScore("full toupper (utf-8)", fullUpper);
	reportScore("full totitle (utf-8)", fullTitle);
	reportScore("full tolower (utf-16)", wideLower);
	reportScore("full toupper (utf-16)", wideUpper);
	reportScore("full totitle (utf-16)", wideTitle);
	sprt::cout << "  (" << int(sizeof(s_caseConditional) / sizeof(s_caseConditional[0]))
			   << " conditional SpecialCasing rows are not here: they need context, and are"
				  " asserted by hand in runtime_unicode)\n";
	sprt::cout << "unicode case conformance: reported, not asserted - see "
				  "docs/design/unicode-and-idn.adoc\n";
}

} // namespace sprt
