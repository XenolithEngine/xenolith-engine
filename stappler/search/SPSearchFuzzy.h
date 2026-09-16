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


#ifndef STAPPLER_SEARCH_SPSEARCHFUZZY_H_
#define STAPPLER_SEARCH_SPSEARCHFUZZY_H_

#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::search {

/* Approximate matching of a typed query against a short name: the subsequence matcher a search
palette runs on, and the arithmetic that turns its answer into something a label can highlight.

This file deliberately does NOT open `mem_pool` into the namespace, unlike its neighbours
SPSearchDistance.h and SPSearchParser.h. `FuzzyMatch::indices` is a `mem_std::Vector` and has to stay
one: the matcher is called from UI code and from application logic that has no pool at hand, and a
result whose lifetime ends with the enclosing pool would be a trap in exactly those callers.

---- which text comparison, and why -----------------------------------------------------------------

The runtime offers three, and picking the wrong one is the classic bug in this area because it does
not show until someone changes a build (codestyle/core/unicode-and-text.adoc):

  * `unicode::compareCodepoints` - no locale, same answer in every build and every Unicode version;
    for keys, indexes and TESTS THAT MUST PRODUCE THE SAME OUTPUT FOREVER.
  * `unicode::compareFolded` - the same, after full case folding; for case-insensitive EQUALITY.
  * `unicode::collate` - the reading order of a language; for a list a person looks at.

A caller that sorts results wants two of them at once, so they are kept apart rather than
compromised: the order this matcher's scores produce is broken by `compareCodepoints`, and `collate`
is not called here at all. A collation order is a matter of convention - it depends on CLDR and is
explicitly not promised stable across Unicode versions - so an expected-output table built on it
would be a test that passes today and fails on a data refresh. The alphabetical order a PERSON reads
is the presentation layer's, over a copy.

---- and why the matcher folds one character at a time -----------------------------------------------

The general rule is "case-map strings, not characters", and `compareFolded` is the full folding where
`sz` equals `ss`. It cannot be used here: full folding CHANGES LENGTH, and this returns the positions
of the matched characters so a UI can highlight them - after one character became two, a position no
longer points where the reader looks. That is the case the codestyle article carves out: reach for
the single-code-point form when the unit really IS a character. Here it is - one typed character
against one character of a name, and its index goes back out.

The cost is stated rather than hidden: the German sharp s does not match `ss` IN THE MATCHER. Where
whole strings have to compare equal ignoring case, `compareFolded` is the right call and this is
not it. */

// The reference's constants, multiplied by ten.
//
// Its positional decay is `max(0, 4 - 0.1*i)` - floating point, and 0.1 is not exact in binary. An
// expected ORDER computed from a sum of such terms is a test that agrees here and disagrees on the
// first ABI that contracts the sum differently. Times ten the decay is `max(0, 40 - i)`: the same
// arithmetic, computed exactly. Scaling a sum by a constant cannot reorder it.
struct SP_PUBLIC FuzzyConfig {
	int32_t base = 10; // every matched character
	int32_t consecutive = 60; // it immediately follows the previous match
	int32_t wordStart = 50; // it lands on a word boundary
	int32_t firstChar = 80; // the first query character at the very start of the target
	int32_t decayFrom = 40; // plus max(0, decayFrom - index): earlier is better
};

struct SP_PUBLIC FuzzyMatch {
	bool matched = false;
	int32_t score = 0;

	// Indices of the matched characters in the target, ascending, one per query character.
	//
	// CODE POINTS, not bytes and not UTF-16 code units. A highlight is drawn over characters; an
	// index counted in UTF-16 silently slips by one on every astral character. Use
	// `makeHighlightRanges` to convert, rather than doing it at the call site.
	mem_std::Vector<uint32_t> indices;
};

// Can `query` be read out of `target` in order?
//
// Subsequence, greedy from the left, WITHOUT BACKTRACKING: the first occurrence of each query
// character after the previous match wins, even where a later one would have scored higher. This is
// behaviour, not an accident of the implementation - the same query has to highlight the same
// characters in every consumer.
//
// An empty query matches everything with score 0 and no indices. There is no threshold and no
// penalty: scores are comparable WITHIN one query and meaningless between two.
SP_PUBLIC void fuzzyMatch(StringView query, StringView target, FuzzyMatch &out,
		const FuzzyConfig & = FuzzyConfig());

// Does the character at this index begin a word? The start of the string, anything after ' ', '_',
// '-', '/' or '.', and the upper half of a lowercase-to-uppercase transition.
//
// Past the end of the string is not the start of a word.
SP_PUBLIC bool isWordStart(StringView target, uint32_t codepointIndex);

// ---- from code points to what a label counts in -----------------------------------------------------

/* `Label` addresses its text in UTF-16 code units, because that is what it stores; the matcher
answers in code points, because that is what a person sees highlighted. The conversion is one
running sum, and it lives here rather than at each call site for two reasons: the label's internal
representation is not visible from outside, so a caller doing this arithmetic is guessing; and the
sum has to agree with the label's own encoder about code points UTF-16 cannot represent, which
`utf16EncodeLength` reports as length 0 and both sides therefore drop.

Positions at or past the end of the string return the total length. */
SP_PUBLIC size_t codepointToUtf16Offset(StringView target, size_t codepointIndex);

// The same conversion from a BYTE offset, which is what anything built on `SearchIndex` has:
// its slices address the canonical string in bytes, not in characters.
//
// A byte offset in the middle of a multi-byte character is rounded down to that character's start,
// because there is no such position in UTF-16 to round it up to.
SP_PUBLIC size_t byteToUtf16Offset(StringView target, size_t byteOffset);

// Merges consecutive matched indices into runs and reports each as a UTF-16 (start, length) pair -
// the exact arguments of `Label::setTextRangeStyle`.
//
// `codepointIndices` must be ascending. This is a single forward walk of the string, so a duplicate
// or a descending entry cannot be revisited: it is DROPPED rather than turned into a range that
// points somewhere else.
SP_PUBLIC void makeHighlightRanges(StringView target, SpanView<uint32_t> codepointIndices,
		const Callback<void(size_t utf16Start, size_t utf16Length)> &);

} // namespace stappler::search

#endif /* STAPPLER_SEARCH_SPSEARCHFUZZY_H_ */
