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

// The two UTF-8 iterators. Ported from ICU utf8collationiterator.h and
// utf8collationiterator.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// The point of these is that UTF-8 text is collated without being converted
// first. Text is UTF-8 nearly everywhere it enters this codebase, and a
// conversion would double the work of the common case - comparing two short
// ASCII strings - for no gain.
//
// The trie lookups here go through the UTF-8 side doors of UTrie2: a single byte
// indexes data32 directly, and a two-byte sequence has a dedicated index block, so
// neither needs the code point to be assembled first. That is why this file
// reaches into the trie's arrays rather than calling get().
//
// Ill-formed input yields U+FFFD, and U+FFFD's own CE32, so an invalid sequence
// sorts as a replacement character rather than dropping out of the comparison.
// Surrogate code points cannot appear in well-formed UTF-8 at all, which is why
// forbidSurrogateCodePoints() is true here and false for UTF-16.
//
// Departures from ICU: NUL-terminated text is not supported (see
// collation_utf16.cc for why), and the copy constructors are gone.

namespace sprt::unicode::detail {

// --- UTF-8 primitives ----------------------------------------------------------
//
// U8_* from ICU utf8.h, in the "or U+FFFD" flavour the collation iterators use.
// The runtime's own decoders next door (case_utf8.cc) report ill-formed input
// instead of substituting, which is right for case mapping and wrong here.

static constexpr bool isU8Single(int32_t b) { return (b & 0x80) == 0; }

static constexpr bool isU8Trail(int32_t b) { return (b & 0xC0) == 0x80; }

// U8_IS_VALID_LEAD3_AND_T1 and its U8_LEAD3_T1_BITS table: for a three-byte
// sequence the lead byte alone does not pin down the range, and the first trail
// byte decides it. E0 admits only A0..BF (below that is an overlong form) and ED
// only 80..9F (above that is a surrogate); every other lead takes all of 80..BF.
static constexpr uint8_t s_lead3T1Bits[16] = {
	0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
	0x30, 0x30, 0x30, 0x30, 0x30, 0x10, 0x30, 0x30,
};

static constexpr bool isValidLead3AndT1(int32_t lead, int32_t t1) {
	return (s_lead3T1Bits[lead & 0xF] & (1 << (uint8_t(t1) >> 5))) != 0;
}

// The number of bytes a code point takes in UTF-8.
static constexpr int32_t u8Length(int32_t c) {
	return c <= 0x7F ? 1 : (c <= 0x7FF ? 2 : (c <= 0xFFFF ? 3 : 4));
}

// U8_NEXT_OR_FFFD: one code point forward, U+FFFD for an ill-formed sequence,
// advancing past the maximal subpart either way.
static int32_t u8NextOrFFFD(const uint8_t *s, int32_t &i, int32_t length) {
	int32_t lead = s[i++];
	if (isU8Single(lead)) {
		return lead;
	}
	int32_t trailCount;
	int32_t c;
	if (lead >= 0xC2 && lead <= 0xDF) {
		trailCount = 1;
		c = lead & 0x1F;
	} else if (lead >= 0xE0 && lead <= 0xEF) {
		trailCount = 2;
		c = lead & 0x0F;
	} else if (lead >= 0xF0 && lead <= 0xF4) {
		trailCount = 3;
		c = lead & 0x07;
	} else {
		return 0xFFFD;
	}
	for (int32_t k = 0; k < trailCount; ++k) {
		if (i >= length) {
			return 0xFFFD;
		}
		int32_t trail = s[i];
		if (!isU8Trail(trail)) {
			return 0xFFFD;
		}
		if (k == 0
				&& ((lead == 0xE0 && trail < 0xA0) || (lead == 0xED && trail > 0x9F)
						|| (lead == 0xF0 && trail < 0x90) || (lead == 0xF4 && trail > 0x8F))) {
			return 0xFFFD;
		}
		c = (c << 6) | (trail & 0x3F);
		++i;
	}
	return c;
}

// U8_PREV_OR_FFFD: one code point backward. Finds the lead byte, then decodes
// forward and requires the sequence to end exactly where it started.
static int32_t u8PrevOrFFFD(const uint8_t *s, int32_t start, int32_t &i) {
	int32_t limit = i;
	int32_t lead = limit - 1;
	while (lead > start && isU8Trail(s[lead]) && (limit - lead) < 4) { --lead; }
	int32_t j = lead;
	auto c = u8NextOrFFFD(s, j, limit);
	if (j == limit && c != 0xFFFD) {
		i = lead;
		return c;
	}
	// Ill-formed: step back one byte, as ICU's macro does.
	i = limit - 1;
	return 0xFFFD;
}

static void u8Forward(const uint8_t *s, int32_t &i, int32_t length, int32_t num) {
	while (num > 0 && i < length) {
		u8NextOrFFFD(s, i, length);
		--num;
	}
}

static void u8Backward(const uint8_t *s, int32_t start, int32_t &i, int32_t num) {
	while (num > 0 && i > start) {
		u8PrevOrFFFD(s, start, i);
		--num;
	}
}

// --- text already known to be FCD ----------------------------------------------

class UTF8CollationIterator : public CollationIterator {
public:
	UTF8CollationIterator(const CollationData *d, bool numeric, const uint8_t *s, int32_t p,
			int32_t len)
	: CollationIterator(d, numeric), u8(s), pos(p), length(len) { }

	void resetToOffset(int32_t newOffset) override {
		reset();
		pos = newOffset;
	}

	int32_t getOffset() const override { return pos; }

	int32_t nextCodePoint() override {
		if (pos == length) {
			return NoCodePoint;
		}
		return u8NextOrFFFD(u8, pos, length);
	}

	int32_t previousCodePoint() override {
		if (pos == 0) {
			return NoCodePoint;
		}
		return u8PrevOrFFFD(u8, 0, pos);
	}

protected:
	uint32_t handleNextCE32(int32_t &c) override {
		if (pos == length) {
			c = NoCodePoint;
			return FallbackCE32;
		}
		// U8_NEXT_OR_FFFD combined with the trie's UTF-8 side doors.
		c = u8[pos++];
		if (isU8Single(c)) {
			return data->trie.data[c];
		}
		int32_t t1, t2;
		if (0xE0 <= c && c < 0xF0 && (pos + 1) < length
				&& isValidLead3AndT1(c, t1 = u8[pos]) && (t2 = (u8[pos + 1] - 0x80)) <= 0x3F) {
			// U+0800..U+FFFF, surrogates excluded by isValidLead3AndT1.
			c = ((c & 0xF) << 12) | ((t1 & 0x3F) << 6) | t2;
			pos += 2;
			return data->trie.getFromU16SingleLead(char16_t(c));
		}
		if (c < 0xE0 && c >= 0xC2 && pos != length && (t1 = (u8[pos] - 0x80)) <= 0x3F) {
			// U+0080..U+07FF, through the two-byte index block.
			auto ce32 = data->trie.data[data->trie.index[(Utrie2Utf8_2bIndex2Offset - 0xC0) + c]
					+ t1];
			c = ((c & 0x1F) << 6) | t1;
			++pos;
			return ce32;
		}
		// Supplementary characters and every error case.
		--pos;
		c = u8NextOrFFFD(u8, pos, length);
		return data->getCE32(char32_t(c));
	}

	// Surrogate code points cannot occur in well-formed UTF-8, so an unpaired one
	// is treated as U+FFFD rather than getting an implicit weight of its own.
	bool forbidSurrogateCodePoints() const override { return true; }

	void forwardNumCodePoints(int32_t num) override { u8Forward(u8, pos, length, num); }

	void backwardNumCodePoints(int32_t num) override { u8Backward(u8, 0, pos, num); }

	const uint8_t *u8;
	int32_t pos;
	int32_t length;
};

// --- text checked for FCD as it goes -------------------------------------------

class FCDUTF8CollationIterator : public UTF8CollationIterator {
public:
	FCDUTF8CollationIterator(const CollationData *d, bool numeric, const uint8_t *s, int32_t p,
			int32_t len)
	: UTF8CollationIterator(d, numeric, s, p, len), state(CheckFwd), start(p), limit(0) { }

	void resetToOffset(int32_t newOffset) override {
		reset();
		start = pos = newOffset;
		state = CheckFwd;
	}

	int32_t getOffset() const override {
		if (state != InNormalized) {
			return pos;
		}
		return pos == 0 ? start : limit;
	}

	int32_t nextCodePoint() override {
		int32_t c;
		for (;;) {
			if (state == CheckFwd) {
				if (pos == length) {
					return NoCodePoint;
				}
				c = u8[pos];
				if (isU8Single(c)) {
					++pos;
					return c;
				}
				c = u8NextOrFFFD(u8, pos, length);
				if (hasTccc(char32_t(c <= 0xFFFF ? c : 0xD7C0 + (c >> 10)))
						&& (maybeTibetanCompositeVowel(c) || (pos != length && nextHasLccc()))) {
					// Not FCD-inert, so not U+FFFD and a valid sequence: its length
					// follows from the code point.
					pos -= u8Length(c);
					if (!nextSegment()) {
						return NoCodePoint;
					}
					continue;
				}
				return c;
			} else if (state == InFcdSegment && pos != limit) {
				return u8NextOrFFFD(u8, pos, length);
			} else if (state == InNormalized && pos != normalized.size()) {
				char32_t u = normalized[pos++];
				if (isUtf16HighSurrogate(char16_t(u)) && pos != normalized.size()
						&& isUtf16LowSurrogate(normalized[pos])) {
					u = utf16CombineSurrogates(char16_t(u), normalized[pos++]);
				}
				return int32_t(u);
			} else {
				switchToForward();
			}
		}
	}

	int32_t previousCodePoint() override {
		int32_t c;
		for (;;) {
			if (state == CheckBwd) {
				if (pos == 0) {
					return NoCodePoint;
				}
				c = u8[pos - 1];
				if (isU8Single(c)) {
					--pos;
					return c;
				}
				c = u8PrevOrFFFD(u8, 0, pos);
				if (hasLccc(char32_t(c <= 0xFFFF ? c : 0xD7C0 + (c >> 10)))
						&& (maybeTibetanCompositeVowel(c) || (pos != 0 && previousHasTccc()))) {
					pos += u8Length(c);
					if (!previousSegment()) {
						return NoCodePoint;
					}
					continue;
				}
				return c;
			} else if (state == InFcdSegment && pos != start) {
				return u8PrevOrFFFD(u8, 0, pos);
			} else if (state >= InNormalized && pos != 0) {
				char32_t u = normalized[--pos];
				if (isUtf16LowSurrogate(char16_t(u)) && pos != 0
						&& isUtf16HighSurrogate(normalized[pos - 1])) {
					u = utf16CombineSurrogates(normalized[--pos], char16_t(u));
				}
				return int32_t(u);
			} else {
				switchToBackward();
			}
		}
	}

protected:
	uint32_t handleNextCE32(int32_t &c) override {
		for (;;) {
			if (state == CheckFwd) {
				// UTF8CollationIterator::handleNextCE32 with the FCD test folded in.
				if (pos == length) {
					c = NoCodePoint;
					return FallbackCE32;
				}
				c = u8[pos++];
				if (isU8Single(c)) {
					return data->trie.data[c];
				}
				int32_t t1, t2;
				if (0xE0 <= c && c < 0xF0 && (pos + 1) < length
						&& isValidLead3AndT1(c, t1 = u8[pos])
						&& (t2 = (u8[pos + 1] - 0x80)) <= 0x3F) {
					c = ((c & 0xF) << 12) | ((t1 & 0x3F) << 6) | t2;
					pos += 2;
					if (hasTccc(char32_t(c))
							&& (maybeTibetanCompositeVowel(c)
									|| (pos != length && nextHasLccc()))) {
						pos -= 3;
					} else {
						break; // the CE32 of a BMP character
					}
				} else if (c < 0xE0 && c >= 0xC2 && pos != length
						&& (t1 = (u8[pos] - 0x80)) <= 0x3F) {
					auto ce32 =
							data->trie.data[data->trie.index[(Utrie2Utf8_2bIndex2Offset - 0xC0) + c]
									+ t1];
					c = ((c & 0x1F) << 6) | t1;
					++pos;
					if (hasTccc(char32_t(c)) && pos != length && nextHasLccc()) {
						pos -= 2;
					} else {
						return ce32;
					}
				} else {
					--pos;
					c = u8NextOrFFFD(u8, pos, length);
					if (c == 0xFFFD) {
						return FFFDCE32;
					}
					// Supplementary: the lead surrogate answers the FCD question.
					if (hasTccc(char32_t(0xD7C0 + (c >> 10))) && pos != length && nextHasLccc()) {
						pos -= 4;
					} else {
						return data->getCE32FromSupplementary(char32_t(c));
					}
				}
				if (!nextSegment()) {
					c = NoCodePoint;
					return FallbackCE32;
				}
				continue;
			} else if (state == InFcdSegment && pos != limit) {
				return UTF8CollationIterator::handleNextCE32(c);
			} else if (state == InNormalized && pos != normalized.size()) {
				c = normalized[pos++];
				break;
			} else {
				switchToForward();
			}
		}
		return data->trie.getFromU16SingleLead(char16_t(c));
	}

	// Only a normalized segment can hold a surrogate pair; raw UTF-8 never does.
	char16_t handleGetTrailSurrogate() override {
		if (state != InNormalized) {
			return 0;
		}
		char16_t trail = normalized[pos];
		if (isUtf16LowSurrogate(trail)) {
			++pos;
		}
		return trail;
	}

	void forwardNumCodePoints(int32_t num) override {
		while (num > 0 && FCDUTF8CollationIterator::nextCodePoint() >= 0) { --num; }
	}

	void backwardNumCodePoints(int32_t num) override {
		while (num > 0 && FCDUTF8CollationIterator::previousCodePoint() >= 0) { --num; }
	}

private:
	// The FCD fast path for the byte after pos. U+0300, the first character with
	// ccc != 0, is CC 80 in UTF-8; and CJK U+4000..U+DFFF except U+Axxx - lead bytes
	// E4..ED other than EA - is FCD-inert, which is most of the text that gets here.
	bool nextHasLccc() const {
		int32_t c = u8[pos];
		if (c < 0xCC || (0xE4 <= c && c <= 0xED && c != 0xEA)) {
			return false;
		}
		auto i = pos;
		c = u8NextOrFFFD(u8, i, length);
		return mayHaveLccc(c);
	}

	bool previousHasTccc() const {
		int32_t c = u8[pos - 1];
		if (isU8Single(c)) {
			return false;
		}
		auto i = pos;
		c = u8PrevOrFFFD(u8, 0, i);
		return hasTccc(char32_t(c <= 0xFFFF ? c : 0xD7C0 + (c >> 10)));
	}

	void switchToForward() {
		if (state == CheckBwd) {
			start = pos;
			state = pos == limit ? CheckFwd : InFcdSegment;
		} else {
			if (state == InNormalized) {
				// The segment was normalized; resume in the raw text after it.
				start = pos = limit;
			}
			state = CheckFwd;
		}
	}

	// Extends the FCD segment forward, or normalizes around pos.
	bool nextSegment() {
		// [start, pos) is known to pass the FCD check.
		auto segmentStart = pos;
		// The characters being checked are collected as UTF-16, in case they have to
		// be normalized.
		segment.clear();
		uint8_t prevCC = 0;
		for (;;) {
			auto cpStart = pos;
			auto c = u8NextOrFFFD(u8, pos, length);
			auto fcd16 = getFCD16(char32_t(c));
			auto leadCC = uint8_t(fcd16 >> 8);
			if (leadCC == 0 && cpStart != segmentStart) {
				// An FCD boundary before this character.
				pos = cpStart;
				break;
			}
			if (!segment.appendCodePoint(char32_t(c))) {
				return false;
			}
			if (leadCC != 0 && (prevCC > leadCC || isFCD16OfTibetanCompositeVowel(fcd16))) {
				// Not FCD: collect to the next boundary and normalize.
				while (pos != length) {
					cpStart = pos;
					c = u8NextOrFFFD(u8, pos, length);
					if (getFCD16(char32_t(c)) <= 0xFF) {
						pos = cpStart;
						break;
					}
					if (!segment.appendCodePoint(char32_t(c))) {
						return false;
					}
				}
				if (!normalizeNfd(segment.data(), segment.size(), normalized, scratch)) {
					return false;
				}
				start = segmentStart;
				limit = pos;
				state = InNormalized;
				pos = 0;
				return true;
			}
			prevCC = uint8_t(fcd16);
			if (pos == length || prevCC == 0) {
				// An FCD boundary after the last character read.
				break;
			}
		}
		limit = pos;
		pos = segmentStart;
		state = InFcdSegment;
		return true;
	}

	void switchToBackward() {
		if (state == CheckFwd) {
			limit = pos;
			state = pos == start ? CheckBwd : InFcdSegment;
		} else {
			if (state >= InNormalized) {
				limit = pos = start;
			}
			state = CheckBwd;
		}
	}

	bool previousSegment() {
		// [pos, limit) is known to pass the FCD check.
		auto segmentLimit = pos;
		segment.clear();
		uint8_t nextCC = 0;
		for (;;) {
			auto cpLimit = pos;
			auto c = u8PrevOrFFFD(u8, 0, pos);
			auto fcd16 = getFCD16(char32_t(c));
			auto trailCC = uint8_t(fcd16);
			if (trailCC == 0 && cpLimit != segmentLimit) {
				pos = cpLimit;
				break;
			}
			if (!segment.appendCodePoint(char32_t(c))) {
				return false;
			}
			if (trailCC != 0
					&& ((nextCC != 0 && trailCC > nextCC)
							|| isFCD16OfTibetanCompositeVowel(fcd16))) {
				while (fcd16 > 0xFF && pos != 0) {
					cpLimit = pos;
					c = u8PrevOrFFFD(u8, 0, pos);
					fcd16 = getFCD16(char32_t(c));
					if (fcd16 == 0) {
						pos = cpLimit;
						break;
					}
					if (!segment.appendCodePoint(char32_t(c))) {
						return false;
					}
				}
				// Collected from the end backwards, so put it in text order first.
				segment.reverseCodePoints();
				if (!normalizeNfd(segment.data(), segment.size(), normalized, scratch)) {
					return false;
				}
				limit = segmentLimit;
				start = pos;
				state = InNormalized;
				pos = normalized.size();
				return true;
			}
			nextCC = uint8_t(fcd16 >> 8);
			if (pos == 0 || nextCC == 0) {
				break;
			}
		}
		start = pos;
		pos = segmentLimit;
		state = InFcdSegment;
		return true;
	}

	// What is known about the text, and what pos indexes.
	enum State : int8_t {
		// [start, pos) passes the FCD check; moving forward checks incrementally;
		// limit is undefined.
		CheckFwd,
		// [pos, limit) passes; moving backward checks incrementally; start undefined.
		CheckBwd,
		// [start, limit) passes; pos is an index into the raw text.
		InFcdSegment,
		// [start, limit) failed and was normalized; pos indexes `normalized`.
		InNormalized,
	};

	State state;
	int32_t start;
	int32_t limit;

	NormBuffer segment;
	NormBuffer normalized;
	CodepointBuffer scratch;
};

} // namespace sprt::unicode::detail
