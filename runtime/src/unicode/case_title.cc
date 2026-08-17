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

// Titlecasing. Ported from ICU ustrcase.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html), on top of the word breaker in
// word_break.cc.
//
// Titlecasing is the one case operation that cannot be done character by
// character: it needs to know where words start. Each word is split into three
// parts - characters skipped before the first letter, the letter itself, and the
// rest - and each part is treated differently. The letter gets the TITLECASE
// mapping, which is not always the uppercase one: ǆ uppercases to Ǆ but
// titlecases to ǅ.
//
// Two rules make it more than "uppercase the first letter":
//
//   The word may not start with a letter. `"hello"` starts with a quote, and the
//   quote is not what gets titlecased - the search moves on to the first letter,
//   number or symbol.
//
//   Dutch writes the digraph IJ with both letters capital: ijsland titlecases to
//   IJsland, not Ijsland. That is two capitals from one word start, and it only
//   applies to a real IJ - `ij` with an acute on one letter but not the other is
//   not the digraph.
//
// ICU exposes options to change all of this (U_TITLECASE_NO_LOWERCASE,
// U_TITLECASE_ADJUST_TO_CASED, U_TITLECASE_WHOLE_STRING, ...). None is exposed
// here, so their branches are folded to the defaults: adjust the start to the
// first letter/number/symbol, and lowercase the rest of the word.
//
// UTF-8 is not a separate implementation - the public UTF-8 overload converts and
// calls this. ICU keeps two because its break iterator can run over UTF-8
// through a UText; here that would mean transcribing the whole word breaker a
// second time to earn one avoided conversion on a path that is not hot.

namespace sprt::unicode::detail {

// COMBINING ACUTE ACCENT, the only accent the Dutch IJ rule accepts.
static constexpr char16_t Acute = 0x0301;

// ustrcase_isLNS: letter, number, symbol, or a private use code point, because
// those are typically used as letters or numbers. Modifier letters count only if
// they are cased - the case tables answer that, so the property table only has to
// say which characters are modifier letters.
static bool isLNS(char32_t c) {
	uint16_t props = wordBreakProps(c);
	if ((props & s_wbLNS) != 0) {
		return true;
	}
	return (props & s_wbLm) != 0 && caseTypeOf(c) != CaseNone;
}

static bool isCombiningMark(char32_t c) { return (wordBreakProps(c) & s_wbMark) != 0; }

/**
 * Input: c is a letter I with or without acute accent.
 * start is the index in src after c, and is less than segmentLimit.
 * If a plain i/I is followed by a plain j/J,
 * or an i/I with acute (precomposed or decomposed) is followed by a j/J with acute,
 * then we output accordingly.
 *
 * @return the src index after the titlecased sequence, or the start index if no Dutch IJ
 */
static int32_t maybeTitleDutchIJ(const char16_t *src, char32_t c, int32_t start,
		int32_t segmentLimit, char16_t *dest, int32_t &destIndex, int32_t destCapacity) {
	int32_t index = start;
	bool withAcute = false;

	// If the conditions are met, then the following variables tell us what to output.
	int32_t unchanged1 = 0; // code units before the j, or the whole sequence (0..3)
	bool doTitleJ = false; // true if the j needs to be titlecased
	int32_t unchanged2 = 0; // after the j (0 or 1)

	// next character after the first letter
	char16_t c2 = src[index++];

	// Is the first letter an i/I with accent?
	if (c == u'I') {
		if (c2 == Acute) {
			withAcute = true;
			unchanged1 = 1;
			if (index == segmentLimit) {
				return start;
			}
			c2 = src[index++];
		}
	} else { // Í
		withAcute = true;
	}

	// Is the next character a j/J?
	if (c2 == u'j') {
		doTitleJ = true;
	} else if (c2 == u'J') {
		++unchanged1;
	} else {
		return start;
	}

	// A plain i/I must be followed by a plain j/J.
	// An i/I with acute must be followed by a j/J with acute.
	if (withAcute) {
		if (index == segmentLimit || src[index++] != Acute) {
			return start;
		}
		if (doTitleJ) {
			unchanged2 = 1;
		} else {
			++unchanged1;
		}
	}

	// There must not be another combining mark.
	if (index < segmentLimit) {
		int32_t i = index;
		char32_t cp = u16Next(src, i, segmentLimit);
		if (isCombiningMark(cp)) {
			return start;
		}
	}

	// Output the rest of the Dutch IJ.
	destIndex = appendUnchanged(dest, destIndex, destCapacity, src + start, unchanged1);
	start += unchanged1;
	if (destIndex >= 0 && doTitleJ) {
		destIndex = appendUChar(dest, destIndex, destCapacity, u'J');
		++start;
	}
	if (destIndex >= 0) {
		destIndex = appendUnchanged(dest, destIndex, destCapacity, src + start, unchanged2);
	}
	return index;
}

// ustrcase_internalToTitle. Writes at most destCapacity units and returns the
// length it would have written, or -1 on an index overflow - the same contract as
// the other mappers.
static int32_t titleUtf16(CaseLocale caseLocale, char16_t *dest, int32_t destCapacity,
		const char16_t *src, int32_t srcLength) {
	CaseContext csc;
	csc.p = src;
	csc.limit = srcLength;
	int32_t destIndex = 0;
	int32_t prev = 0;
	bool isFirstIndex = true;

	WordBreakIterator iter(src, srcLength);

	// titlecasing loop
	while (prev < srcLength) {
		// find next index where to titlecase
		int32_t index;
		if (isFirstIndex) {
			isFirstIndex = false;
			index = iter.first();
		} else {
			index = iter.next();
		}
		if (index == WordBreakIterator::Done || index > srcLength) {
			index = srcLength;
		}

		/*
		 * Segment [prev..index[ into 3 parts:
		 * a) skipped characters (copy as-is) [prev..titleStart[
		 * b) first letter (titlecase)              [titleStart..titleLimit[
		 * c) subsequent characters (lowercase)                 [titleLimit..index[
		 */
		if (prev < index) {
			// Find and copy skipped characters [prev..titleStart[
			int32_t titleStart = prev;
			int32_t titleLimit = prev;
			char32_t c = u16Next(src, titleLimit, index);
			// Adjust the titlecasing index to the next letter/number/symbol/private
			// use. Stop with titleStart<titleLimit<=index if there is a character to
			// be titlecased, or else stop with titleStart==titleLimit==index.
			while (!isLNS(c)) {
				titleStart = titleLimit;
				if (titleLimit == index) {
					break;
				}
				c = u16Next(src, titleLimit, index);
			}
			if (prev < titleStart) {
				destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev,
						titleStart - prev);
				if (destIndex < 0) {
					return -1;
				}
			}

			if (titleStart < titleLimit) {
				// titlecase c which is from [titleStart..titleLimit[
				csc.cpStart = titleStart;
				csc.cpLimit = titleLimit;
				const char16_t *s = nullptr;
				int32_t result = toFullTitle(c, utf16CaseContextIterator, &csc, &s, caseLocale);
				destIndex = appendResult(dest, destIndex, destCapacity, result, s,
						titleLimit - titleStart);
				if (destIndex < 0) {
					return -1;
				}

				// Special case Dutch IJ titlecasing
				if (titleStart + 1 < index && caseLocale == CaseLocale::Dutch) {
					auto titled = char32_t(result < 0 ? ~result : result);
					if (titled == u'I' || titled == u'Í') {
						titleLimit = maybeTitleDutchIJ(src, titled, titleStart + 1, index, dest,
								destIndex, destCapacity);
						if (destIndex < 0) {
							return -1;
						}
					}
				}

				// lowercase [titleLimit..index[
				if (titleLimit < index) {
					// The tail is written after what is already there. Once the
					// output has outgrown the buffer there is nothing to point at,
					// so the mapper is asked to measure instead - which is what it
					// would do anyway.
					char16_t *tail = nullptr;
					int32_t tailCapacity = 0;
					if (dest != nullptr && destIndex < destCapacity) {
						tail = dest + destIndex;
						tailCapacity = destCapacity - destIndex;
					}
					auto written = toLowerUtf16(caseLocale, false, 0, tail, tailCapacity, src, &csc,
							titleLimit, index);
					if (written < 0 || written > (Max<int32_t> - destIndex)) {
						return -1;
					}
					destIndex += written;
				}
			}
		}

		prev = index;
	}

	return destIndex;
}

// --- entry point -------------------------------------------------------------

int32_t mapToTitleUtf16(CaseLocale caseLocale, char16_t *dest, int32_t destCapacity,
		const char16_t *src, int32_t srcLength) {
	return titleUtf16(caseLocale, dest, destCapacity, src, srcLength);
}

} // namespace sprt::unicode::detail
