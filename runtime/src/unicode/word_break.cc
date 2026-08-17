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

// Word boundaries, per Unicode Standard Annex #29. Implemented from the
// specification rather than ported from ICU: ICU finds boundaries with a
// rule-based break iterator driven by compiled `brkitr` data, which is hundreds
// of kilobytes and a rule compiler, and titlecasing needs only this one
// property. The data (data/, generated from ICU's preparsed UCD) is a plain
// Word_Break table.
//
// This is what makes titlecasing work on words rather than on the first letter
// of the string: which characters hold a word together is not obvious, and
// guessing at it gets the interesting cases wrong. O'Brien is one word because
// an apostrophe between letters does not break (WB6/WB7); well-known is two,
// because a hyphen does; x1_2 is one, because an underscore joins (WB13a/13b).
//
// NOT implemented, deliberately: dictionary-based breaking. Thai, Lao, Khmer,
// Burmese and the like write without spaces, and finding word boundaries in them
// needs a dictionary, which is a different body of work and a different order of
// data. Under plain UAX #29 a run of such text is one word. Chinese and Japanese
// break between ideographs, which is harmless here because they are uncased.
//
// The rules that used E_Base, E_Modifier and Glue_After_Zwj (WB14, and the emoji
// half of WB3c) were retired in Unicode 11 and no code point carries those values
// any more; the generator fails if one ever does again.

namespace sprt::unicode::detail {

// Not a Word_Break value: "there is no such character" (start of text, or past
// the end). The values themselves occupy 0..18 of the low five bits.
static constexpr uint16_t WordBreakNone = 0x1f;

static constexpr int32_t s_wordBreakRangeCount =
		int32_t(sizeof(s_wordBreakRanges) / sizeof(s_wordBreakRanges[0]));

// The Word_Break value and the four derived flags for one code point.
static uint16_t wordBreakProps(char32_t c) {
	if (c < 0x80) {
		return s_wordBreakAscii[c];
	}
	// The table is sorted by range start and covers the whole code space, so the
	// answer is always the last range that starts at or before c.
	int32_t lo = 0;
	int32_t hi = s_wordBreakRangeCount - 1;
	while (lo < hi) {
		int32_t mid = (lo + hi + 1) / 2;
		if (char32_t(s_wordBreakRanges[mid] >> s_wbValueBits) <= c) {
			lo = mid;
		} else {
			hi = mid - 1;
		}
	}
	return uint16_t(s_wordBreakRanges[lo] & ((1u << s_wbValueBits) - 1));
}

static constexpr uint16_t wordBreakValue(uint16_t props) { return props & s_wbValueMask; }

// AHLetter in the rules.
static constexpr bool isAHLetter(uint16_t v) {
	return v == s_wbALetter || v == s_wbHebrewLetter;
}

// MidNumLetQ in the rules.
static constexpr bool isMidNumLetQ(uint16_t v) {
	return v == s_wbMidNumLet || v == s_wbSingleQuote;
}

// The three that end a segment outright, before WB4 gets to fold anything.
static constexpr bool isBreakForcing(uint16_t v) {
	return v == s_wbNewline || v == s_wbCR || v == s_wbLF;
}

// Extend, Format and ZWJ: WB4 makes these part of the character before them.
static constexpr bool isIgnorable(uint16_t v) {
	return v == s_wbExtend || v == s_wbFormat || v == s_wbZWJ;
}

/**
 * Forward-only iterator over word boundaries, with the interface the titlecaser
 * needs: first() to start, next() for each boundary after it, Done at the end.
 * The first boundary is 0 and the last is the length of the text - UAX #29 puts
 * a boundary at both ends (WB1, WB2).
 *
 * Forward-only is not a shortcut: the rules are stated over an unbounded left
 * context (WB4 folds any number of Extend/Format/ZWJ into the character before
 * them, and WB15/16 count regional indicators back to the start of their run), so
 * a scan that carries that context is the honest shape. Going backwards would
 * mean rebuilding it from the start of the text every time.
 */
class WordBreakIterator {
public:
	static constexpr int32_t Done = -1;

	WordBreakIterator(const char16_t *text, int32_t length) : _text(text), _length(length) { }

	int32_t first() {
		_index = 0;
		_pos = 0;
		_prev = WordBreakNone;
		_prevPrev = WordBreakNone;
		_prevRaw = WordBreakNone;
		_lastWasZwj = false;
		_riCount = 0;
		return 0;
	}

	int32_t next() {
		while (_index < _length) {
			int32_t cpStart = _index;
			char32_t c = u16Next(_text, _index, _length);
			uint16_t props = wordBreakProps(c);
			// WB1 already produced the boundary at 0; there is nothing before it
			// to break from.
			bool brk = cpStart != 0 && isBoundaryBefore(props);
			advance(props, brk);
			if (brk) {
				_pos = cpStart;
				return _pos;
			}
		}
		if (_pos < _length) {
			_pos = _length; // WB2: eot is always a boundary
			return _pos;
		}
		return Done;
	}

private:
	// Is there a boundary immediately before the code point whose properties are
	// `props`? Everything it needs about what came before is in the members;
	// what comes after is looked up on demand, because only three rules need it.
	bool isBoundaryBefore(uint16_t props) const {
		uint16_t cur = wordBreakValue(props);

		if (_prevRaw == s_wbCR && cur == s_wbLF) {
			return false; // WB3
		}
		if (isBreakForcing(_prevRaw)) {
			return true; // WB3a
		}
		if (isBreakForcing(cur)) {
			return true; // WB3b
		}
		if (_lastWasZwj && (props & s_wbExtPict) != 0) {
			return false; // WB3c
		}
		if (_prevRaw == s_wbWSegSpace && cur == s_wbWSegSpace) {
			// WB3d. Raw rather than folded, because this rule is stated before
			// WB4: a WSegSpace with an Extend between it and the next one does
			// not join.
			return false;
		}
		if (isIgnorable(cur)) {
			return false; // WB4
		}

		// From here on the rules see the text with Extend/Format/ZWJ already
		// folded away, which is what _prev and _prevPrev hold.
		const bool prevAH = isAHLetter(_prev);
		const bool curAH = isAHLetter(cur);

		if (prevAH && curAH) {
			return false; // WB5
		}
		if (prevAH && (cur == s_wbMidLetter || isMidNumLetQ(cur)) && isAHLetter(lookahead())) {
			return false; // WB6
		}
		if (isAHLetter(_prevPrev) && (_prev == s_wbMidLetter || isMidNumLetQ(_prev)) && curAH) {
			return false; // WB7
		}
		if (_prev == s_wbHebrewLetter && cur == s_wbSingleQuote) {
			return false; // WB7a
		}
		if (_prev == s_wbHebrewLetter && cur == s_wbDoubleQuote
				&& lookahead() == s_wbHebrewLetter) {
			return false; // WB7b
		}
		if (_prevPrev == s_wbHebrewLetter && _prev == s_wbDoubleQuote
				&& cur == s_wbHebrewLetter) {
			return false; // WB7c
		}
		if (_prev == s_wbNumeric && cur == s_wbNumeric) {
			return false; // WB8
		}
		if (prevAH && cur == s_wbNumeric) {
			return false; // WB9
		}
		if (_prev == s_wbNumeric && curAH) {
			return false; // WB10
		}
		if (_prevPrev == s_wbNumeric && (_prev == s_wbMidNum || isMidNumLetQ(_prev))
				&& cur == s_wbNumeric) {
			return false; // WB11
		}
		if (_prev == s_wbNumeric && (cur == s_wbMidNum || isMidNumLetQ(cur))
				&& lookahead() == s_wbNumeric) {
			return false; // WB12
		}
		if (_prev == s_wbKatakana && cur == s_wbKatakana) {
			return false; // WB13
		}
		if ((prevAH || _prev == s_wbNumeric || _prev == s_wbKatakana
					|| _prev == s_wbExtendNumLet)
				&& cur == s_wbExtendNumLet) {
			return false; // WB13a
		}
		if (_prev == s_wbExtendNumLet
				&& (curAH || cur == s_wbNumeric || cur == s_wbKatakana)) {
			return false; // WB13b
		}
		if (_prev == s_wbRegionalIndicator && cur == s_wbRegionalIndicator && (_riCount & 1) != 0) {
			// WB15/WB16: regional indicators pair up, so a break falls between
			// pairs and not inside one. _riCount is the run so far, ending at
			// _prev; an odd count means this one completes a pair.
			return false;
		}
		return true; // WB999
	}

	// The Word_Break value of the first code point after the one being tested
	// that WB4 does not fold away, or WordBreakNone at the end of the text.
	// _index is already past the code point being tested.
	uint16_t lookahead() const {
		int32_t i = _index;
		while (i < _length) {
			char32_t c = u16Next(_text, i, _length);
			uint16_t v = wordBreakValue(wordBreakProps(c));
			if (!isIgnorable(v)) {
				return v;
			}
		}
		return WordBreakNone;
	}

	// Fold the code point just consumed into the running context. `brk` says
	// whether a boundary was found before it, which is also what decides whether
	// WB4 has anything to fold it into.
	void advance(uint16_t props, bool brk) {
		uint16_t v = wordBreakValue(props);
		if (isIgnorable(v) && !brk && _prev != WordBreakNone) {
			// WB4: part of the preceding character, so the folded view does not
			// move - including the count of regional indicators.
		} else {
			_prevPrev = _prev;
			_prev = v;
			if (v == s_wbRegionalIndicator) {
				++_riCount;
			} else {
				_riCount = 0;
			}
		}
		_prevRaw = v;
		_lastWasZwj = v == s_wbZWJ;
	}

	const char16_t *_text;
	int32_t _length;
	int32_t _index = 0; // scan position, in code units
	int32_t _pos = 0; // last boundary returned

	uint16_t _prev = WordBreakNone; // previous character, Extend/Format/ZWJ folded away
	uint16_t _prevPrev = WordBreakNone; // the one before that, likewise
	uint16_t _prevRaw = WordBreakNone; // previous character as written
	bool _lastWasZwj = false;
	int32_t _riCount = 0; // regional indicators in the run ending at _prev
};

// Every boundary in the text, in order, including 0 and `length`. Returns how
// many were written, or the number that would have been written if `out` is too
// small.
//
// Nothing in the runtime calls this - the titlecaser uses the iterator directly.
// It exists so the UAX #29 conformance suite has something to compare against:
// checking word breaking through `totitle` alone would only see the boundaries
// that happen to land on a cased letter.
int32_t findWordBoundaries(const char16_t *text, int32_t length, int32_t *out,
		int32_t outCapacity) {
	WordBreakIterator iter(text, length);
	int32_t count = 0;
	for (int32_t b = iter.first(); b != WordBreakIterator::Done; b = iter.next()) {
		if (count < outCapacity) {
			out[count] = b;
		}
		++count;
	}
	return count;
}

} // namespace sprt::unicode::detail
