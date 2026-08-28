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


#include "SPSearchFuzzy.h"

namespace STAPPLER_VERSIONIZED stappler::search {

namespace unicode = sprt::unicode;

// ---- word boundaries -------------------------------------------------------------------------------

// The simple mapping is the honest predicate here: a code point that has a lowercase form is an
// uppercase one, whatever its script. The reference tests `a <= c && c <= 'z'`, which is the same
// answer for Latin and no answer at all for anything else.
static bool SearchFuzzy_isUpperCp(char32_t c) { return unicode::tolower(c) != c; }
static bool SearchFuzzy_isLowerCp(char32_t c) { return unicode::toupper(c) != c; }

static bool SearchFuzzy_wordStartBetween(char32_t prev, char32_t cur, bool atStart) {
	if (atStart) {
		return true;
	}
	if (prev == U' ' || prev == U'_' || prev == U'-' || prev == U'/' || prev == U'.') {
		return true;
	}
	return SearchFuzzy_isLowerCp(prev) && SearchFuzzy_isUpperCp(cur);
}

bool isWordStart(StringView target, uint32_t codepointIndex) {
	StringViewUtf8 r(target);
	char32_t prev = 0;
	for (uint32_t i = 0; !r.empty(); ++i) {
		const char32_t c = r.readChar();
		if (i == codepointIndex) {
			return SearchFuzzy_wordStartBetween(prev, c, i == 0);
		}
		prev = c;
	}
	return false; // past the end of the string is not the start of a word
}

// ---- the matcher -----------------------------------------------------------------------------------

void fuzzyMatch(StringView query, StringView target, FuzzyMatch &out, const FuzzyConfig &config) {
	out.matched = false;
	out.score = 0;
	out.indices.clear();

	if (query.empty()) {
		// Matches anything, neutrally. The caller uses this to mean "no filter".
		out.matched = true;
		return;
	}

	StringViewUtf8 q(query);
	StringViewUtf8 t(target);

	// Both sides are walked by CODE POINT, not by byte: a query in Cyrillic would otherwise be
	// compared one UTF-8 continuation byte at a time and match nothing.
	char32_t wanted = unicode::tolower(q.readChar());

	uint32_t index = 0;
	int32_t score = 0;
	int64_t prevMatch = -2; // so index 0 is never "consecutive", exactly as in the reference
	char32_t prev = 0;
	bool atStart = true;
	bool queryStart = true;

	while (!t.empty()) {
		const char32_t raw = t.readChar();
		if (unicode::tolower(raw) == wanted) {
			out.indices.emplace_back(index);

			int32_t charScore = config.base;
			if (int64_t(index) == prevMatch + 1) {
				charScore += config.consecutive;
			}
			// The ORIGINAL character, not the folded one: the boundary is about the spelling.
			if (SearchFuzzy_wordStartBetween(prev, raw, atStart)) {
				charScore += config.wordStart;
			}
			if (queryStart && index == 0) {
				charScore += config.firstChar;
			}
			const int32_t decay = config.decayFrom - int32_t(index);
			if (decay > 0) {
				charScore += decay;
			}

			score += charScore;
			prevMatch = int64_t(index);
			queryStart = false;

			if (q.empty()) {
				out.matched = true;
				out.score = score;
				return;
			}
			wanted = unicode::tolower(q.readChar());
		}
		prev = raw;
		atStart = false;
		++index;
	}

	// The target ran out with query characters left over: not a subsequence.
	out.indices.clear();
	return;
}

// ---- from code points to what a label counts in -----------------------------------------------------

size_t codepointToUtf16Offset(StringView target, size_t codepointIndex) {
	StringViewUtf8 r(target);
	size_t offset = 0;
	for (size_t i = 0; !r.empty(); ++i) {
		if (i == codepointIndex) {
			return offset;
		}
		offset += unicode::utf16EncodeLength(r.readChar());
	}
	return offset; // at or past the end: the total length
}

size_t byteToUtf16Offset(StringView target, size_t byteOffset) {
	StringViewUtf8 r(target);
	size_t offset = 0;
	while (!r.empty()) {
		const size_t start = size_t(r.data() - target.data());
		if (start >= byteOffset) {
			return offset;
		}

		const char32_t c = r.readChar();
		if (size_t(r.data() - target.data()) > byteOffset) {
			// The offset lands inside this character. Its start is the only UTF-16 position that
			// exists there, so that is the answer rather than the position after it.
			return offset;
		}

		offset += unicode::utf16EncodeLength(c);
	}
	return offset;
}

void makeHighlightRanges(StringView target, SpanView<uint32_t> codepointIndices,
		const Callback<void(size_t, size_t)> &cb) {
	if (codepointIndices.empty() || !cb) {
		return;
	}

	StringViewUtf8 r(target);
	size_t cursor = 0; // index of the next index to look for
	size_t offset = 0; // running UTF-16 offset of the current code point
	size_t runStart = 0;
	size_t runLength = 0;
	int64_t runPrev = -2; // code point index of the last character added to the open run
	bool inRun = false;

	for (uint32_t index = 0; !r.empty() && cursor < codepointIndices.size(); ++index) {
		const uint8_t length = unicode::utf16EncodeLength(r.readChar());

		if (index == codepointIndices[cursor]) {
			if (inRun && int64_t(index) == runPrev + 1) {
				runLength += length;
			} else {
				if (inRun) {
					cb(runStart, runLength);
				}
				inRun = true;
				runStart = offset;
				runLength = length;
			}
			runPrev = int64_t(index);

			// A duplicate or a descending entry cannot be revisited by a forward walk, so it is
			// dropped rather than silently turned into a range pointing somewhere else.
			do {
				++cursor;
			} while (cursor < codepointIndices.size() && codepointIndices[cursor] <= index);
		}

		offset += length;
	}

	if (inRun) {
		cb(runStart, runLength);
	}
}

} // namespace stappler::search
