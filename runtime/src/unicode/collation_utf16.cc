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

// The two UTF-16 iterators. Ported from ICU utf16collationiterator.h and
// utf16collationiterator.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// UTF16CollationIterator walks the text directly. FCDUTF16CollationIterator adds
// the FCD check: it advances a segment at a time, and when a segment would fail
// the check it normalizes that segment to NFD and iterates over the copy instead.
//
// Why FCD and not NFD everywhere: comparison must agree for canonically
// equivalent strings, and FCD - "no canonical reordering could change this" - is
// the weakest condition under which reading the text as it stands is already
// right. Nearly all real text is FCD, so nearly all text is never normalized.
//
// Two departures from ICU, both subtractions:
//
//   NUL-terminated text is not supported; `limit` is never null. Our public API
//     takes a view with a length, so `foundNULTerminator` never has anything to
//     find and the U0000 tag falls through to the ordinary CE32 for U+0000 - which
//     is what ICU does for length-specified text too.
//   The copy constructors and operator== are gone; nothing copies or compares an
//     iterator.

namespace sprt::unicode::detail {

// Tibetan composite vowel signs (U+0F73, U+0F75, U+0F81) have to be decomposed
// before the core collation code sees them: some sequences containing them do not
// give canonically equivalent results otherwise, even when they pass the FCD
// check. They are the only characters with these lccc/tccc pairs, 129/130 and
// 129/132, which is what makes both tests exact.
static constexpr bool maybeTibetanCompositeVowel(int32_t c) {
	return (c & 0x1F'FF01) == 0xF01;
}

static constexpr bool isFCD16OfTibetanCompositeVowel(uint16_t fcd16) {
	return fcd16 == 0x8182 || fcd16 == 0x8184;
}

// Normalizer2Impl::nextFCD16 and previousFCD16: read one code point, stepping the
// pointer, and return its fcd16.
static uint16_t nextFCD16(const char16_t *&p, const char16_t *limit) {
	char32_t c = *p++;
	if (isUtf16HighSurrogate(char16_t(c)) && p != limit && isUtf16LowSurrogate(*p)) {
		c = utf16CombineSurrogates(char16_t(c), *p++);
	}
	return getFCD16(c);
}

static uint16_t previousFCD16(const char16_t *start, const char16_t *&p) {
	char32_t c = *--p;
	if (isUtf16LowSurrogate(char16_t(c)) && p != start && isUtf16HighSurrogate(*(p - 1))) {
		c = utf16CombineSurrogates(*--p, char16_t(c));
	}
	return getFCD16(c);
}

// --- text already known to be FCD ----------------------------------------------

class UTF16CollationIterator : public CollationIterator {
public:
	UTF16CollationIterator(const CollationData *d, bool numeric, const char16_t *s,
			const char16_t *p, const char16_t *lim)
	: CollationIterator(d, numeric), start(s), pos(p), limit(lim) { }

	void resetToOffset(int32_t newOffset) override {
		reset();
		pos = start + newOffset;
	}

	int32_t getOffset() const override { return int32_t(pos - start); }

	int32_t nextCodePoint() override {
		if (pos == limit) {
			return NoCodePoint;
		}
		int32_t c = *pos++;
		char16_t trail;
		if (isUtf16HighSurrogate(char16_t(c)) && pos != limit
				&& isUtf16LowSurrogate(trail = *pos)) {
			++pos;
			return int32_t(utf16CombineSurrogates(char16_t(c), trail));
		}
		return c;
	}

	int32_t previousCodePoint() override {
		if (pos == start) {
			return NoCodePoint;
		}
		int32_t c = *--pos;
		char16_t lead;
		if (isUtf16LowSurrogate(char16_t(c)) && pos != start
				&& isUtf16HighSurrogate(lead = *(pos - 1))) {
			--pos;
			return int32_t(utf16CombineSurrogates(lead, char16_t(c)));
		}
		return c;
	}

protected:
	uint32_t handleNextCE32(int32_t &c) override {
		if (pos == limit) {
			c = NoCodePoint;
			return FallbackCE32;
		}
		c = *pos++;
		// One code *unit*: a lead surrogate is answered by the LeadSurrogateTag
		// branch in appendCEsFromCE32, which then asks for the trail.
		return data->trie.getFromU16SingleLead(char16_t(c));
	}

	char16_t handleGetTrailSurrogate() override {
		if (pos == limit) {
			return 0;
		}
		char16_t trail = *pos;
		if (isUtf16LowSurrogate(trail)) {
			++pos;
		}
		return trail;
	}

	void forwardNumCodePoints(int32_t num) override {
		while (num > 0 && pos != limit) {
			int32_t c = *pos++;
			--num;
			if (isUtf16HighSurrogate(char16_t(c)) && pos != limit && isUtf16LowSurrogate(*pos)) {
				++pos;
			}
		}
	}

	void backwardNumCodePoints(int32_t num) override {
		while (num > 0 && pos != start) {
			int32_t c = *--pos;
			--num;
			if (isUtf16LowSurrogate(char16_t(c)) && pos != start
					&& isUtf16HighSurrogate(*(pos - 1))) {
				--pos;
			}
		}
	}

	const char16_t *start;
	const char16_t *pos;
	const char16_t *limit;
};

// --- text checked for FCD as it goes -------------------------------------------

class FCDUTF16CollationIterator : public UTF16CollationIterator {
public:
	FCDUTF16CollationIterator(const CollationData *d, bool numeric, const char16_t *s,
			const char16_t *p, const char16_t *lim)
	: UTF16CollationIterator(d, numeric, s, p, lim)
	, rawStart(s)
	, segmentStart(p)
	, segmentLimit(nullptr)
	, rawLimit(lim)
	, checkDir(1) { }

	void resetToOffset(int32_t newOffset) override {
		reset();
		start = segmentStart = pos = rawStart + newOffset;
		limit = rawLimit;
		checkDir = 1;
	}

	int32_t getOffset() const override {
		if (checkDir != 0 || start == segmentStart) {
			return int32_t(pos - rawStart);
		}
		// Inside a normalized segment there is no per-character offset to give, so
		// the answer is one end of the segment or the other.
		return pos == start ? int32_t(segmentStart - rawStart) : int32_t(segmentLimit - rawStart);
	}

	int32_t nextCodePoint() override {
		int32_t c;
		for (;;) {
			if (checkDir > 0) {
				if (pos == limit) {
					return NoCodePoint;
				}
				c = *pos++;
				if (hasTccc(char32_t(c))
						&& (maybeTibetanCompositeVowel(c)
								|| (pos != limit && hasLccc(*pos)))) {
					--pos;
					if (!nextSegment()) {
						return NoCodePoint;
					}
					c = *pos++;
				}
				break;
			} else if (checkDir == 0 && pos != limit) {
				c = *pos++;
				break;
			} else {
				switchToForward();
			}
		}
		char16_t trail;
		if (isUtf16HighSurrogate(char16_t(c)) && pos != limit
				&& isUtf16LowSurrogate(trail = *pos)) {
			++pos;
			return int32_t(utf16CombineSurrogates(char16_t(c), trail));
		}
		return c;
	}

	int32_t previousCodePoint() override {
		int32_t c;
		for (;;) {
			if (checkDir < 0) {
				if (pos == start) {
					return NoCodePoint;
				}
				c = *--pos;
				if (hasLccc(char32_t(c))
						&& (maybeTibetanCompositeVowel(c)
								|| (pos != start && hasTccc(*(pos - 1))))) {
					++pos;
					if (!previousSegment()) {
						return NoCodePoint;
					}
					c = *--pos;
				}
				break;
			} else if (checkDir == 0 && pos != start) {
				c = *--pos;
				break;
			} else {
				switchToBackward();
			}
		}
		char16_t lead;
		if (isUtf16LowSurrogate(char16_t(c)) && pos != start
				&& isUtf16HighSurrogate(lead = *(pos - 1))) {
			--pos;
			return int32_t(utf16CombineSurrogates(lead, char16_t(c)));
		}
		return c;
	}

protected:
	uint32_t handleNextCE32(int32_t &c) override {
		for (;;) {
			if (checkDir > 0) {
				if (pos == limit) {
					c = NoCodePoint;
					return FallbackCE32;
				}
				c = *pos++;
				if (hasTccc(char32_t(c))
						&& (maybeTibetanCompositeVowel(c)
								|| (pos != limit && hasLccc(*pos)))) {
					--pos;
					if (!nextSegment()) {
						c = NoCodePoint;
						return FallbackCE32;
					}
					c = *pos++;
				}
				break;
			} else if (checkDir == 0 && pos != limit) {
				c = *pos++;
				break;
			} else {
				switchToForward();
			}
		}
		return data->trie.getFromU16SingleLead(char16_t(c));
	}

	// Named explicitly rather than through the vtable, as ICU does: these two are
	// the leaf implementations and there is nothing below them to dispatch to.
	void forwardNumCodePoints(int32_t num) override {
		while (num > 0 && FCDUTF16CollationIterator::nextCodePoint() >= 0) { --num; }
	}

	void backwardNumCodePoints(int32_t num) override {
		while (num > 0 && FCDUTF16CollationIterator::previousCodePoint() >= 0) { --num; }
	}

private:
	// Turns around to forward checking. Called when checkDir < 0, or checkDir == 0
	// at the end of the segment.
	void switchToForward() {
		if (checkDir < 0) {
			start = segmentStart = pos;
			if (pos == segmentLimit) {
				limit = rawLimit;
				checkDir = 1;
			} else {
				checkDir = 0; // stay inside the FCD segment
			}
		} else {
			// At the end of the segment.
			if (start != segmentStart) {
				// The segment had to be normalized; resume in the raw text after it.
				pos = start = segmentStart = segmentLimit;
			}
			// Otherwise the segment was FCD as it stood, and extends forward.
			limit = rawLimit;
			checkDir = 1;
		}
	}

	// Extends the FCD segment forward, or normalizes around pos. Called with
	// checkDir > 0 and pos != limit; returns with checkDir == 0 and pos != limit.
	bool nextSegment() {
		// [segmentStart, pos) is known to pass the FCD check.
		auto p = pos;
		uint8_t prevCC = 0;
		for (;;) {
			auto q = p;
			auto fcd16 = nextFCD16(p, rawLimit);
			auto leadCC = uint8_t(fcd16 >> 8);
			if (leadCC == 0 && q != pos) {
				// An FCD boundary before the character at [q, p).
				limit = segmentLimit = q;
				break;
			}
			if (leadCC != 0
					&& (prevCC > leadCC || isFCD16OfTibetanCompositeVowel(fcd16))) {
				// Not FCD: find the next boundary and normalize up to it.
				do {
					q = p;
				} while (p != rawLimit && nextFCD16(p, rawLimit) > 0xFF);
				if (!normalize(pos, q)) {
					return false;
				}
				pos = start;
				break;
			}
			prevCC = uint8_t(fcd16);
			if (p == rawLimit || prevCC == 0) {
				// An FCD boundary after the last character read.
				limit = segmentLimit = p;
				break;
			}
		}
		checkDir = 0;
		return true;
	}

	void switchToBackward() {
		if (checkDir > 0) {
			limit = segmentLimit = pos;
			if (pos == segmentStart) {
				start = rawStart;
				checkDir = -1;
			} else {
				checkDir = 0;
			}
		} else {
			if (start != segmentStart) {
				pos = limit = segmentLimit = segmentStart;
			}
			start = rawStart;
			checkDir = -1;
		}
	}

	bool previousSegment() {
		// [pos, segmentLimit) is known to pass the FCD check.
		auto p = pos;
		uint8_t nextCC = 0;
		for (;;) {
			auto q = p;
			auto fcd16 = previousFCD16(rawStart, p);
			auto trailCC = uint8_t(fcd16);
			if (trailCC == 0 && q != pos) {
				start = segmentStart = q;
				break;
			}
			if (trailCC != 0
					&& ((nextCC != 0 && trailCC > nextCC)
							|| isFCD16OfTibetanCompositeVowel(fcd16))) {
				do {
					q = p;
				} while (fcd16 > 0xFF && p != rawStart
						&& (fcd16 = previousFCD16(rawStart, p)) != 0);
				if (!normalize(q, pos)) {
					return false;
				}
				pos = limit;
				break;
			}
			nextCC = uint8_t(fcd16 >> 8);
			if (p == rawStart || nextCC == 0) {
				start = segmentStart = p;
				break;
			}
		}
		checkDir = 0;
		return true;
	}

	// NFD of [from, to), after which iteration continues inside the buffer.
	bool normalize(const char16_t *from, const char16_t *to) {
		if (!normalizeNfd(from, int32_t(to - from), normalized, scratch)) {
			return false;
		}
		segmentStart = from;
		segmentLimit = to;
		start = normalized.data();
		limit = start + normalized.size();
		return true;
	}

	// The input is [rawStart, rawLimit). The three states of checkDir, and what is
	// known in each, are:
	//
	//   > 0: [segmentStart, pos) passes the FCD check; moving forward checks
	//        incrementally; segmentLimit is undefined and limit == rawLimit.
	//   < 0: [pos, segmentLimit) passes; moving backward checks incrementally;
	//        segmentStart is undefined and start == rawStart.
	//  == 0: [segmentStart, segmentLimit) is the segment being processed, and both
	//        ends are FCD boundaries. Either it passed as it stood, and
	//        segmentStart == start <= pos <= limit == segmentLimit; or it had to be
	//        normalized, and start/limit point into `normalized` instead.
	const char16_t *rawStart;
	const char16_t *segmentStart;
	const char16_t *segmentLimit;
	const char16_t *rawLimit;

	NormBuffer normalized;
	CodepointBuffer scratch;
	int8_t checkDir;
};

} // namespace sprt::unicode::detail
