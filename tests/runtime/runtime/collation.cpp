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

// Collation, asserted against the CLDR root order.
//
// The full UCA conformance suite (434 000 sequences) is a separate test with
// generated data; this one is the hand-written half: the orderings a person can
// check by reading them, and the options.
//
// Only the root order is asserted here. Locale tailorings are a build option, so
// a test that required them would fail in a configuration that is deliberately
// smaller - those vectors go with hasCollation().

#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

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

static int sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

// Every vector is checked in both directions: an ordering that is not
// antisymmetric is not an ordering, and a comparison built from levels can fail
// that way in one direction only.
static void checkOrder(StringView l, StringView r, int expected, StringView what,
		unicode::CollateOptions options = unicode::CollateOptions()) {
	auto forward = sign(unicode::collate(l, r, StringView(), options));
	auto backward = sign(unicode::collate(r, l, StringView(), options));
	check(forward == expected, what);
	check(backward == -expected, "antisymmetry");

	// The same pair through UTF-16, which is a different code path into the same
	// tables (the UTF-8 entry point converts, but the identical-prefix skip and the
	// FCD segmentation see different lengths).
	unicode::toUtf16([&](WideStringView lw) {
		unicode::toUtf16([&](WideStringView rw) {
			check(sign(unicode::collate(lw, rw, StringView(), options)) == expected,
					"utf-16 agrees with utf-8");
		}, r);
	}, l);
}

static void testPrimaryLevel() {
	checkOrder("a", "b", -1, "a < b");
	checkOrder("apple", "banana", -1, "apple < banana");
	checkOrder("a", "a", 0, "a == a");
	checkOrder("", "a", -1, "the empty string sorts first");
	checkOrder("", "", 0, "empty == empty");

	// Case and accents do not decide the primary level, which is what makes
	// primary strength useful for searching.
	unicode::CollateOptions primary;
	primary.strength = unicode::Strength::Primary;
	checkOrder("resume", "RESUME", 0, "primary ignores case", primary);
	checkOrder("resume", "r\xc3\xa9sum\xc3\xa9", 0, "primary ignores accents", primary);
	checkOrder("resume", "rhyme", -1, "primary still orders letters", primary);
}

static void testSecondaryAndTertiary() {
	// The default strength is tertiary: accents beat case, and both beat nothing.
	checkOrder("resume", "r\xc3\xa9sum\xc3\xa9", -1, "resume < resume with accents");
	checkOrder("r\xc3\xa9sum\xc3\xa9", "R\xc3\x89SUM\xc3\x89", -1, "lowercase before uppercase");
	checkOrder("resume", "Resume", -1, "case decides at the tertiary level");

	unicode::CollateOptions secondary;
	secondary.strength = unicode::Strength::Secondary;
	checkOrder("r\xc3\xa9sum\xc3\xa9", "R\xc3\x89SUM\xc3\x89", 0, "secondary ignores case",
			secondary);
	checkOrder("resume", "r\xc3\xa9sum\xc3\xa9", -1, "secondary still orders accents", secondary);
}

// The root order is not code point order, and these are the cases where the
// difference is visible without any tailoring at all.
static void testRootOrderIsNotCodepointOrder() {
	// Digits and letters: '9' is U+0039 and 'a' is U+0061, so code point order
	// agrees here - but 'A' (U+0041) sorts *after* 'a' in collation and before it
	// by code point.
	check(sign(unicode::collate(StringView("a"), StringView("A"), StringView())) < 0,
			"a < A in collation");
	check(sign(unicode::compareCodepoints(StringView("a"), StringView("A"))) > 0,
			"a > A by code point");
	++s_checks; // the pair above is one fact stated twice

	// Punctuation sorts before letters and digits, whatever its code point: '_' is
	// U+005F, above 'A' and below 'a'.
	checkOrder("_", "a", -1, "punctuation before letters");
	checkOrder("_", "0", -1, "punctuation before digits");
	checkOrder("0", "a", -1, "digits before letters");

	// Canonically equivalent strings compare equal at every level, whichever way
	// they are written: e + combining acute against the precomposed é.
	checkOrder("e\xcc\x81", "\xc3\xa9", 0, "decomposed equals precomposed");
	checkOrder("e\xcc\x81z", "\xc3\xa9z", 0, "... and in the middle of a word");

	// Reordered combining marks are canonically equivalent too, and this is the
	// case the FCD check exists for: two marks with different combining classes
	// written in the non-canonical order.
	checkOrder("q\xcc\xa3\xcc\x87", "q\xcc\x87\xcc\xa3", 0,
			"a non-canonical mark order equals the canonical one");
}

static void testOptions() {
	unicode::CollateOptions numeric;
	numeric.numeric = true;
	checkOrder("item2", "item10", -1, "numeric: item2 < item10", numeric);
	checkOrder("item9", "item10", -1, "numeric: item9 < item10", numeric);
	// The same two pairs go the other way without the option, digit by digit:
	// that reversal is the whole point of it.
	checkOrder("item10", "item2", -1, "digits sort as characters by default");
	checkOrder("item10", "item9", -1, "... so item10 comes before item9");

	unicode::CollateOptions shifted;
	shifted.shifted = true;
	checkOrder("de-luge", "deluge", 0, "shifted: a hyphen is ignored to the third level", shifted);
	checkOrder("de-luge", "delugf", -1, "shifted still orders letters", shifted);

	unicode::CollateOptions upperFirst;
	upperFirst.caseFirst = unicode::CaseFirst::Upper;
	checkOrder("A", "a", -1, "caseFirst=upper puts uppercase first", upperFirst);

	unicode::CollateOptions identical;
	identical.strength = unicode::Strength::Identical;
	// Canonically equivalent strings are equal even at the identical level: it
	// compares NFD, not the code units as written.
	checkOrder("e\xcc\x81", "\xc3\xa9", 0, "identical level compares NFD", identical);
}

// A key is only useful if memcmp on two of them says what collate() says. The
// conformance suite proves that over 434 000 pairs; these are the cases a person
// can read.
static void testSortKeys() {
	struct Key {
		uint8_t bytes[128];
		int32_t length = 0;
		bool ok = false;
	};

	auto build = [](StringView s, unicode::CollateOptions options) {
		Key key;
		auto out = &key;
		out->ok = unicode::sortKey([out](BytesView k) {
			out->length = int32_t(k.size());
			for (int32_t i = 0; i < out->length; ++i) { out->bytes[i] = k[size_t(i)]; }
		}, s, StringView(), options);
		return key;
	};

	auto compareKeys = [](const Key &a, const Key &b) {
		auto n = a.length < b.length ? a.length : b.length;
		for (int32_t i = 0; i < n; ++i) {
			if (a.bytes[i] != b.bytes[i]) {
				return a.bytes[i] < b.bytes[i] ? -1 : 1;
			}
		}
		return a.length == b.length ? 0 : (a.length < b.length ? -1 : 1);
	};

	unicode::CollateOptions options;
	auto a = build("apple", options);
	auto b = build("banana", options);
	auto A = build("Apple", options);
	check(a.ok && b.ok && A.ok, "sort keys are produced");
	check(compareKeys(a, b) < 0, "sort keys order apple before banana");
	check(compareKeys(a, A) < 0, "sort keys order apple before Apple");
	check(compareKeys(a, a) == 0, "a sort key equals itself");

	// Canonically equivalent strings have the SAME key, byte for byte - the key
	// is built from collation elements, not from the text.
	auto precomposed = build("r\xc3\xa9sum\xc3\xa9", options);
	auto decomposed = build("re\xcc\x81sume\xcc\x81", options);
	check(compareKeys(precomposed, decomposed) == 0,
			"canonically equivalent strings get equal keys");

	// A shorter strength gives a shorter key: the levels below it are not written.
	unicode::CollateOptions primary;
	primary.strength = unicode::Strength::Primary;
	auto primaryKey = build("apple", primary);
	check(primaryKey.ok && primaryKey.length < a.length, "a primary key is shorter than a tertiary one");

	// And it no longer separates case, which is the point of asking for it.
	check(compareKeys(primaryKey, build("APPLE", primary)) == 0,
			"a primary key ignores case");
}

// The point of the whole stage: languages that do not sort like the root.
// Each vector is guarded by hasCollation(), because which groups are compiled in
// is a build option - a smaller build must not fail this test, it must skip the
// languages it deliberately does not carry.
static void checkLocale(StringView locale, StringView l, StringView r, int expected,
		StringView what) {
	if (!unicode::hasCollation(locale)) {
		return; // this build left the group out
	}
	check(sign(unicode::collate(l, r, locale)) == expected, what);
	check(sign(unicode::collate(r, l, locale)) == -expected, "antisymmetry");
}

static void testTailorings() {
	// Swedish puts ö at the very end of the alphabet; German sorts it inside o.
	// This one pair is the clearest statement of what a tailoring is for.
	checkLocale("sv", "z", "\xc3\xb6", -1, "sv: ö comes after z");
	checkLocale("de", "z", "\xc3\xb6", 1, "de: ö comes before z");
	check(sign(unicode::collate(StringView("z"), StringView("\xc3\xb6"), StringView())) > 0,
			"root: ö sorts with o, as German does");

	// Czech treats ch as a single letter, after h.
	checkLocale("cs", "h", "ch", -1, "cs: h before ch");
	checkLocale("cs", "ch", "i", -1, "cs: ch before i");

	// Danish and Norwegian put å at the end, after z.
	checkLocale("da", "z", "\xc3\xa5", -1, "da: å comes after z");

	// Turkish separates dotted and dotless i, and the dotless one comes first.
	checkLocale("tr", "\xc4\xb1", "i", -1, "tr: dotless ı before i");

	// Ukrainian orders ґ right after г, where the root does not.
	checkLocale("uk", "\xd0\xb3", "\xd2\x91", -1, "uk: г before ґ");

	// Polish gives ą its own place after a.
	checkLocale("pl", "a", "\xc4\x85", -1, "pl: a before ą");
	checkLocale("pl", "\xc4\x85", "b", -1, "pl: ą before b");

	// Lithuanian puts y between į and j.
	checkLocale("lt", "y", "j", -1, "lt: y before j");

	// Spanish traditional sorting is a named variant, not the default any more:
	// modern Spanish sorts ch inside c, like the root.
	checkLocale("es", "cz", "ch", 1, "es: ch is not a letter of its own");
}

static void testLocaleFallback() {
	// A tag is tried as written, then with subtags dropped, so a region or a
	// script that has no table of its own still finds the language's.
	if (unicode::hasCollation(StringView("sv"))) {
		check(unicode::hasCollation(StringView("sv_SE")), "sv_SE falls back to sv");
		check(unicode::hasCollation(StringView("sv-se")), "a hyphen and lowercase work too");
		check(sign(unicode::collate(StringView("z"), StringView("\xc3\xb6"), StringView("sv_FI")))
						== sign(unicode::collate(StringView("z"), StringView("\xc3\xb6"),
								StringView("sv"))),
				"a region does not change the order unless it has its own table");
	}

	check(!unicode::hasCollation(StringView("xx")), "an invented tag has no tailoring");
	check(!unicode::hasCollation(StringView("")), "an empty tag has no tailoring");
	check(sign(unicode::collate(StringView("a"), StringView("b"), StringView("xx"))) < 0,
			"an unknown locale still collates, with the root order");
	check(sign(unicode::collate(StringView("a"), StringView("b"), StringView()))
					== sign(unicode::collate(StringView("a"), StringView("b"), StringView("xx"))),
			"an unknown tag equals the root");

	// German has no tailoring at all - its order *is* the root order - so
	// hasCollation is false for it and that is not a gap.
	check(!unicode::hasCollation(StringView("de")),
			"de has no tailoring because it does not need one");
}

void performCollationTests() {
	s_checks = 0;
	s_failures = 0;

	testPrimaryLevel();
	testSecondaryAndTertiary();
	testRootOrderIsNotCodepointOrder();
	testOptions();
	testSortKeys();
	testTailorings();
	testLocaleFallback();

	sprt::cout << "collation tests: " << s_checks << " checks, " << s_failures << " failures\n";
}

} // namespace sprt
