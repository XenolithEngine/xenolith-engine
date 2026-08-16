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

// Case mapping over UTF-16 strings. Ported from ICU ustrcase.cpp (© Unicode,
// Inc.; http://www.unicode.org/copyright.html).
//
// This is where the per-character mappings in case_props.cc and case_full.cc
// become a string operation: a character is looked at together with the text
// around it, may expand to several characters, and may be replaced by something
// that depends on the language.
//
// Three things are deliberately different from the ICU original.
//
//   `Edits` is not ported. In ICU every append records what it did into an edit
//   map, which callers use to project offsets through the mapping. Nothing here
//   wants that, and a no-op stub would leave ~65 branches that cannot fire, so
//   the parameter is gone - along with U_OMIT_UNCHANGED_TEXT, which is only
//   meaningful together with it. That collapses GreekUpper's "did anything
//   actually change?" block (ustrcase.cpp:1187-1216) to `change = true`, which
//   is what ICU itself computes when edits==nullptr.
//
//   `options` stays, because it carries the Turkic case-folding bit, which is
//   part of the folding contract rather than an output feature.
//
//   Buffer handling (ustrcase_map, ustrcase_mapWithOverlap) is not ported. Those
//   check arguments, handle a dest that overlaps src, and NUL-terminate; the
//   runtime API hands out a callback over a buffer this file's caller owns, so
//   none of it applies. What IS kept is the preflight contract underneath them:
//   with destCapacity == 0 these functions write nothing and return the length
//   that would have been written.
//
// The two mapping loops are structured as ICU wrote them: a fast path that walks
// characters whose mapping is a small delta - the whole of Latin-1 and Latin
// Extended-A through the linear tables, everything else below U+D800 through the
// trie - and a slow path for anything with an exception, which is where the 1:N
// and contextual mappings live. Runs of unchanged text are copied in one block
// rather than character by character.

namespace sprt::unicode::detail {

// --- UTF-16 primitives -------------------------------------------------------
//
// ICU's U16_NEXT/U16_PREV, not the runtime's utf16Decode32: on an unpaired
// surrogate ICU yields the surrogate code unit itself, while utf16Decode32
// yields U+0000. That difference would be destructive here - a character with no
// case mapping is written back from the decoded value, so an unpaired surrogate
// would come out as a NUL instead of surviving the round trip.

// U16_NEXT: decode forward from `i`, advancing it.
static char32_t u16Next(const char16_t *s, int32_t &i, int32_t length) {
	char32_t c = s[i++];
	if (isUtf16HighSurrogate(char16_t(c))) {
		char16_t c2;
		if (i != length && isUtf16LowSurrogate(c2 = s[i])) {
			++i;
			c = utf16CombineSurrogates(char16_t(c), c2);
		}
	}
	return c;
}

// U16_PREV: decode backward from `i`, retreating it. `start` bounds the scan.
static char32_t u16Prev(const char16_t *s, int32_t start, int32_t &i) {
	char32_t c = s[--i];
	if (isUtf16LowSurrogate(char16_t(c))) {
		char16_t c2;
		if (i > start && isUtf16HighSurrogate(c2 = s[i - 1])) {
			--i;
			c = utf16CombineSurrogates(c2, char16_t(c));
		}
	}
	return c;
}

// --- output ------------------------------------------------------------------
//
// All three return the new destIndex, which may run past destCapacity: that is
// how the preflight pass measures. A negative return means the index itself
// overflowed int32_t, which the caller propagates rather than truncating.

// Appends a full case mapping result, see MaxStringLength.
static int32_t appendResult(char16_t *dest, int32_t destIndex, int32_t destCapacity, int32_t result,
		const char16_t *s, int32_t cpLength) {
	int32_t c;
	int32_t length;

	// decode the result
	if (result < 0) {
		// (not) original code point
		c = ~result;
		if (destIndex < destCapacity && c <= 0xffff) { // BMP slightly-fastpath
			dest[destIndex++] = char16_t(c);
			return destIndex;
		}
		length = cpLength;
	} else {
		if (result <= MaxStringLength) {
			c = -1; // U_SENTINEL: the result is the string in `s`
			length = result;
		} else if (destIndex < destCapacity && result <= 0xffff) { // BMP slightly-fastpath
			dest[destIndex++] = char16_t(result);
			return destIndex;
		} else {
			c = result;
			// U16_LENGTH, not utf16EncodeLength: the latter answers 0 for a
			// surrogate code point, and this length is also used as a count of
			// units to skip when the write does not fit.
			length = c <= 0xffff ? 1 : 2;
		}
	}
	if (length > (Max<int32_t> - destIndex)) {
		return -1; // integer overflow
	}

	if (destIndex < destCapacity) {
		// append the result
		if (c >= 0) {
			// code point (U16_APPEND)
			if (uint32_t(c) <= 0xffff) {
				dest[destIndex++] = char16_t(c);
			} else if (uint32_t(c) <= 0x10ffff && (destIndex + 1) < destCapacity) {
				dest[destIndex++] = char16_t((c >> 10) + 0xd7c0);
				dest[destIndex++] = char16_t((c & 0x3ff) | 0xdc00);
			} else {
				// overflow, nothing written
				destIndex += length;
			}
		} else {
			// string
			if ((destIndex + length) <= destCapacity) {
				while (length > 0) {
					dest[destIndex++] = *s++;
					--length;
				}
			} else {
				// overflow
				destIndex += length;
			}
		}
	} else {
		// preflight
		destIndex += length;
	}
	return destIndex;
}

static int32_t appendUChar(char16_t *dest, int32_t destIndex, int32_t destCapacity, char16_t c) {
	if (destIndex < destCapacity) {
		dest[destIndex] = c;
	} else if (destIndex == Max<int32_t>) {
		return -1; // integer overflow
	}
	return destIndex + 1;
}

static int32_t appendUnchanged(char16_t *dest, int32_t destIndex, int32_t destCapacity,
		const char16_t *s, int32_t length) {
	if (length <= 0) {
		return destIndex;
	}
	if (length > (Max<int32_t> - destIndex)) {
		return -1; // integer overflow
	}
	if ((destIndex + length) <= destCapacity) {
		::__sprt_memcpy(dest + destIndex, s, size_t(length) * sizeof(char16_t));
	}
	return destIndex + length;
}

// --- context -----------------------------------------------------------------

// utf16_caseContextIterator: walks away from the character being mapped, over
// the UTF-16 text in csc->p.
static int32_t utf16CaseContextIterator(void *context, int8_t dir) {
	auto csc = static_cast<CaseContext *>(context);

	if (dir < 0) {
		// reset for backward iteration
		csc->index = csc->cpStart;
		csc->dir = dir;
	} else if (dir > 0) {
		// reset for forward iteration
		csc->index = csc->cpLimit;
		csc->dir = dir;
	} else {
		// continue current iteration direction
		dir = csc->dir;
	}

	auto s = static_cast<const char16_t *>(csc->p);
	if (dir < 0) {
		if (csc->start < csc->index) {
			return int32_t(u16Prev(s, csc->start, csc->index));
		}
	} else {
		if (csc->index < csc->limit) {
			return int32_t(u16Next(s, csc->index, csc->limit));
		}
	}
	return -1; // U_SENTINEL
}

// --- lowercasing and folding -------------------------------------------------

/**
 * `fold` false: lowercases [srcStart..srcLimit[ but takes context [0..srcLength[
 * into account. `fold` true: case-folds [srcStart..srcLimit[, with no context
 * and no locale.
 */
static int32_t toLowerUtf16(CaseLocale caseLocale, bool fold, uint32_t options, char16_t *dest,
		int32_t destCapacity, const char16_t *src, CaseContext *csc, int32_t srcStart,
		int32_t srcLimit) {
	// ICU spells the first branch as "root, or (mapping and not tr/lt), or
	// (folding and default options)"; the root test is subsumed by the second.
	const int8_t *latinToLower;
	if (fold) {
		latinToLower = (options & FoldCaseOptionsMask) == FoldCaseDefault
				? s_caseLatinToLowerNormal
				: s_caseLatinToLowerTrLt;
	} else {
		latinToLower = (caseLocale == CaseLocale::Turkish || caseLocale == CaseLocale::Lithuanian)
				? s_caseLatinToLowerTrLt
				: s_caseLatinToLowerNormal;
	}
	int32_t destIndex = 0;
	int32_t prev = srcStart;
	int32_t srcIndex = srcStart;
	for (;;) {
		// fast path for simple cases
		char16_t lead = 0;
		while (srcIndex < srcLimit) {
			lead = src[srcIndex];
			int32_t delta;
			if (lead < s_caseLatinLongS) {
				int8_t d = latinToLower[lead];
				if (d == s_caseLatinExc) {
					break;
				}
				++srcIndex;
				if (d == 0) {
					continue;
				}
				delta = d;
			} else if (lead >= 0xd800) {
				break; // surrogate or higher
			} else {
				// UTRIE2_GET16_FROM_U16_SINGLE_LEAD: below 0xd800 that macro and
				// the plain code point lookup take the same branch, so the shared
				// reader is used rather than a second transcription of it.
				uint16_t props = s_caseTrie.get(lead);
				if (hasException(props)) {
					break;
				}
				++srcIndex;
				if (!isUpperOrTitle(props) || (delta = getDelta(props)) == 0) {
					continue;
				}
			}
			lead += char16_t(delta);
			destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev,
					srcIndex - 1 - prev);
			if (destIndex >= 0) {
				destIndex = appendUChar(dest, destIndex, destCapacity, lead);
			}
			if (destIndex < 0) {
				return -1;
			}
			prev = srcIndex;
		}
		if (srcIndex >= srcLimit) {
			break;
		}
		// slow path
		int32_t cpStart = srcIndex++;
		char16_t trail;
		char32_t c;
		if (isUtf16HighSurrogate(lead) && srcIndex < srcLimit
				&& isUtf16LowSurrogate(trail = src[srcIndex])) {
			c = utf16CombineSurrogates(lead, trail);
			++srcIndex;
		} else {
			c = lead;
		}
		const char16_t *s = nullptr;
		int32_t result;
		if (!fold) {
			csc->cpStart = cpStart;
			csc->cpLimit = srcIndex;
			result = toFullLower(c, utf16CaseContextIterator, csc, &s, caseLocale);
		} else {
			result = toFullFolding(c, &s, options);
		}
		if (result >= 0) {
			destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev, cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendResult(dest, destIndex, destCapacity, result, s,
						srcIndex - cpStart);
			}
			if (destIndex < 0) {
				return -1;
			}
			prev = srcIndex;
		}
	}
	destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev, srcIndex - prev);
	return destIndex;
}

// --- uppercasing -------------------------------------------------------------

static int32_t toUpperUtf16(CaseLocale caseLocale, char16_t *dest, int32_t destCapacity,
		const char16_t *src, CaseContext *csc, int32_t srcLength) {
	const int8_t *latinToUpper = caseLocale == CaseLocale::Turkish ? s_caseLatinToUpperTr
																   : s_caseLatinToUpperNormal;
	int32_t destIndex = 0;
	int32_t prev = 0;
	int32_t srcIndex = 0;
	for (;;) {
		// fast path for simple cases
		char16_t lead = 0;
		while (srcIndex < srcLength) {
			lead = src[srcIndex];
			int32_t delta;
			if (lead < s_caseLatinLongS) {
				int8_t d = latinToUpper[lead];
				if (d == s_caseLatinExc) {
					break;
				}
				++srcIndex;
				if (d == 0) {
					continue;
				}
				delta = d;
			} else if (lead >= 0xd800) {
				break; // surrogate or higher
			} else {
				uint16_t props = s_caseTrie.get(lead);
				if (hasException(props)) {
					break;
				}
				++srcIndex;
				if (caseType(props) != CaseLower || (delta = getDelta(props)) == 0) {
					continue;
				}
			}
			lead += char16_t(delta);
			destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev,
					srcIndex - 1 - prev);
			if (destIndex >= 0) {
				destIndex = appendUChar(dest, destIndex, destCapacity, lead);
			}
			if (destIndex < 0) {
				return -1;
			}
			prev = srcIndex;
		}
		if (srcIndex >= srcLength) {
			break;
		}
		// slow path
		int32_t cpStart;
		csc->cpStart = cpStart = srcIndex++;
		char16_t trail;
		char32_t c;
		if (isUtf16HighSurrogate(lead) && srcIndex < srcLength
				&& isUtf16LowSurrogate(trail = src[srcIndex])) {
			c = utf16CombineSurrogates(lead, trail);
			++srcIndex;
		} else {
			c = lead;
		}
		csc->cpLimit = srcIndex;
		const char16_t *s = nullptr;
		int32_t result = toFullUpper(c, utf16CaseContextIterator, csc, &s, caseLocale);
		if (result >= 0) {
			destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev, cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendResult(dest, destIndex, destCapacity, result, s,
						srcIndex - cpStart);
			}
			if (destIndex < 0) {
				return -1;
			}
			prev = srcIndex;
		}
	}
	destIndex = appendUnchanged(dest, destIndex, destCapacity, src + prev, srcIndex - prev);
	return destIndex;
}

// --- Greek uppercasing -------------------------------------------------------
//
// Uppercasing Greek is not a per-character mapping: the tonos is dropped, a
// dialytika may have to be added to the following vowel because dropping the
// tonos would otherwise change how the pair is read, ypogegrammeni becomes a
// trailing capital iota, and eta keeps its tonos when it stands alone as the
// disjunctive "or". Hence a state machine over the whole string, with its own
// table (data/, generated) rather than the case trie.
//
// The table bits are generated with the data; these are the ones the state
// machine adds while running, which the data never contains.
enum : uint32_t {
	GreekHasCombiningDialytika = 0x10000, // HAS_COMBINING_DIALYTIKA
	GreekHasOtherDiacritic = 0x20000, // HAS_OTHER_GREEK_DIACRITIC

	GreekHasVowelAndAccent = s_caseGreekHasVowel | s_caseGreekHasAccent,
	GreekHasVowelAndAccentAndDialytika = GreekHasVowelAndAccent | s_caseGreekHasDialytika,
	GreekHasEitherDialytika = s_caseGreekHasDialytika | GreekHasCombiningDialytika,

	// State bits.
	GreekAfterCased = 1, // AFTER_CASED
	GreekAfterVowelWithCombiningAccent = 2, // AFTER_VOWEL_WITH_COMBINING_ACCENT
	GreekAfterVowelWithPrecomposedAccent = 4, // AFTER_VOWEL_WITH_PRECOMPOSED_ACCENT
};

static_assert(sizeof(s_caseGreekData0370) / sizeof(uint16_t) == 0x400 - 0x370,
		"GreekUpper data0370 does not cover U+0370..U+03FF");
static_assert(sizeof(s_caseGreekData1F00) / sizeof(uint16_t) == 0x2000 - 0x1F00,
		"GreekUpper data1F00 does not cover U+1F00..U+1FFF");

static uint32_t greekLetterData(char32_t c) {
	if (c < 0x370 || 0x2126 < c || (0x3ff < c && c < 0x1f00)) {
		return 0;
	} else if (c <= 0x3ff) {
		return s_caseGreekData0370[c - 0x370];
	} else if (c <= 0x1fff) {
		return s_caseGreekData1F00[c - 0x1f00];
	} else if (c == 0x2126) {
		return s_caseGreekData2126;
	} else {
		return 0;
	}
}

/**
 * Returns a non-zero value for each of the Greek combining diacritics listed in
 * The Unicode Standard, version 8, chapter 7.2 Greek, plus some perispomeni
 * look-alikes.
 */
static uint32_t greekDiacriticData(char32_t c) {
	switch (c) {
	case 0x0300: // varia
	case 0x0301: // tonos = oxia
	case 0x0342: // perispomeni
	case 0x0302: // circumflex can look like perispomeni
	case 0x0303: // tilde can look like perispomeni
	case 0x0311: // inverted breve can look like perispomeni
		return s_caseGreekHasAccent;
	case 0x0308: // dialytika = diaeresis
		return GreekHasCombiningDialytika;
	case 0x0344: // dialytika tonos
		return GreekHasCombiningDialytika | s_caseGreekHasAccent;
	case 0x0345: // ypogegrammeni = iota subscript
		return s_caseGreekHasYpogegrammeni;
	case 0x0304: // macron
	case 0x0306: // breve
	case 0x0313: // comma above
	case 0x0314: // reversed comma above
	case 0x0343: // koronis
		return GreekHasOtherDiacritic;
	default: return 0;
	}
}

static bool greekIsFollowedByCasedLetter(const char16_t *s, int32_t i, int32_t length) {
	while (i < length) {
		char32_t c = u16Next(s, i, length);
		uint16_t type = caseTypeOrIgnorable(c);
		if ((type & CaseIgnorable) != 0) {
			// Case-ignorable, continue with the loop.
		} else if (type != CaseNone) {
			return true; // Followed by cased letter.
		} else {
			return false; // Uncased and not case-ignorable.
		}
	}
	return false; // Not followed by cased letter.
}

static int32_t greekToUpper(char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength) {
	int32_t destIndex = 0;
	uint32_t state = 0;
	for (int32_t i = 0; i < srcLength;) {
		int32_t nextIndex = i;
		char32_t c = u16Next(src, nextIndex, srcLength);
		uint32_t nextState = 0;
		uint16_t type = caseTypeOrIgnorable(c);
		if ((type & CaseIgnorable) != 0) {
			// c is case-ignorable
			nextState |= (state & GreekAfterCased);
		} else if (type != CaseNone) {
			// c is cased
			nextState |= GreekAfterCased;
		}
		uint32_t data = greekLetterData(c);
		if (data > 0) {
			uint32_t upper = data & s_caseGreekUpperMask;
			// Add a dialytika to this iota or ypsilon vowel
			// if we removed a tonos from the previous vowel,
			// and that previous vowel did not also have (or gain) a dialytika.
			// Adding one only to the final vowel in a longer sequence
			// (which does not occur in normal writing) would require lookahead.
			// Set the same flag as for preserving an existing dialytika.
			if ((data & s_caseGreekHasVowel) != 0
					&& (state
							   & (GreekAfterVowelWithPrecomposedAccent
									   | GreekAfterVowelWithCombiningAccent))
							!= 0
					&& (upper == 0x399 || upper == 0x3A5)) {
				data |= (state & GreekAfterVowelWithPrecomposedAccent)
						? s_caseGreekHasDialytika
						: GreekHasCombiningDialytika;
			}
			int32_t numYpogegrammeni = 0; // Map each one to a trailing, spacing, capital iota.
			if ((data & s_caseGreekHasYpogegrammeni) != 0) {
				numYpogegrammeni = 1;
			}
			const bool hasPrecomposedAccent = (data & s_caseGreekHasAccent) != 0;
			// Skip combining diacritics after this Greek letter.
			while (nextIndex < srcLength) {
				uint32_t diacriticData = greekDiacriticData(src[nextIndex]);
				if (diacriticData != 0) {
					data |= diacriticData;
					if ((diacriticData & s_caseGreekHasYpogegrammeni) != 0) {
						++numYpogegrammeni;
					}
					++nextIndex;
				} else {
					break; // not a Greek diacritic
				}
			}
			if ((data & GreekHasVowelAndAccentAndDialytika) == GreekHasVowelAndAccent) {
				nextState |= hasPrecomposedAccent ? GreekAfterVowelWithPrecomposedAccent
												  : GreekAfterVowelWithCombiningAccent;
			}
			// Map according to Greek rules.
			bool addTonos = false;
			if (upper == 0x397 && (data & s_caseGreekHasAccent) != 0 && numYpogegrammeni == 0
					&& (state & GreekAfterCased) == 0
					&& !greekIsFollowedByCasedLetter(src, nextIndex, srcLength)) {
				// Keep disjunctive "or" with (only) a tonos.
				// We use the same "word boundary" conditions as for the Final_Sigma test.
				if (hasPrecomposedAccent) {
					upper = 0x389; // Preserve the precomposed form.
				} else {
					addTonos = true;
				}
			} else if ((data & s_caseGreekHasDialytika) != 0) {
				// Preserve a vowel with dialytika in precomposed form if it exists.
				if (upper == 0x399) {
					upper = 0x3AA;
					data &= ~GreekHasEitherDialytika;
				} else if (upper == 0x3A5) {
					upper = 0x3AB;
					data &= ~GreekHasEitherDialytika;
				}
			}

			// ICU decides here whether anything actually changed, but only to
			// feed Edits or U_OMIT_UNCHANGED_TEXT; with neither ported it always
			// takes the "common, simple usage" branch.
			destIndex = appendUChar(dest, destIndex, destCapacity, char16_t(upper));
			if (destIndex >= 0 && (data & GreekHasEitherDialytika) != 0) {
				// restore or add a dialytika
				destIndex = appendUChar(dest, destIndex, destCapacity, 0x308);
			}
			if (destIndex >= 0 && addTonos) {
				destIndex = appendUChar(dest, destIndex, destCapacity, 0x301);
			}
			while (destIndex >= 0 && numYpogegrammeni > 0) {
				destIndex = appendUChar(dest, destIndex, destCapacity, 0x399);
				--numYpogegrammeni;
			}
			if (destIndex < 0) {
				return -1;
			}
		} else {
			const char16_t *s;
			int32_t result = toFullUpper(c, nullptr, nullptr, &s, CaseLocale::Greek);
			destIndex = appendResult(dest, destIndex, destCapacity, result, s, nextIndex - i);
			if (destIndex < 0) {
				return -1;
			}
		}
		i = nextIndex;
		state = nextState;
	}

	return destIndex;
}

// --- entry points ------------------------------------------------------------
//
// Each writes at most destCapacity units and returns the length it would have
// written, or -1 if that length does not fit in int32_t. A return greater than
// destCapacity means the buffer was too small and its contents are undefined.

int32_t mapToLowerUtf16(CaseLocale caseLocale, char16_t *dest, int32_t destCapacity,
		const char16_t *src, int32_t srcLength) {
	CaseContext csc;
	csc.p = src;
	csc.limit = srcLength;
	return toLowerUtf16(caseLocale, false, 0, dest, destCapacity, src, &csc, 0, srcLength);
}

int32_t mapToUpperUtf16(CaseLocale caseLocale, char16_t *dest, int32_t destCapacity,
		const char16_t *src, int32_t srcLength) {
	if (caseLocale == CaseLocale::Greek) {
		return greekToUpper(dest, destCapacity, src, srcLength);
	}
	CaseContext csc;
	csc.p = src;
	csc.limit = srcLength;
	return toUpperUtf16(caseLocale, dest, destCapacity, src, &csc, srcLength);
}

int32_t mapFoldUtf16(uint32_t options, char16_t *dest, int32_t destCapacity, const char16_t *src,
		int32_t srcLength) {
	return toLowerUtf16(CaseLocale::Root, true, options, dest, destCapacity, src, nullptr, 0,
			srcLength);
}

} // namespace sprt::unicode::detail
