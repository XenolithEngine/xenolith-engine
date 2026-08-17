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

// The simple (1:1) case mappings and the case properties they are read from.
// Ported from ICU ucase.cpp / ucase.h (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// ICU exposes a lot through these tables; this is the subset the case mapper
// needs. Deliberately NOT ported:
//
//   ucase_addCaseClosure, addSimpleCaseClosure, addStringCaseClosure,
//   FullCaseFoldingIterator, addPropertyStarts   -- case closure for UnicodeSet;
//       they pull in uset.h, unistr.h and umutex.h and nothing here wants them.
//   LatinCase::TO_*                              -- the ASCII/Latin fast path for
//       the string mappings, which arrive with ustrcase.
//   ucase_toFull*, the context predicates        -- the 1:N and locale-sensitive
//       mappings; they need a UCaseContext iterator over the surrounding text.
//
// The bit layouts below are transcribed from ucase.h. They are NOT described by
// the data, so the generator cannot check them: when the ICU version moves, they
// have to be re-read by hand. That is not hypothetical - the UTS-46 port had
// UPROPS_SCRIPT_X_MASK change shape underneath it, and a wrong layout returns a
// valid answer for the wrong code point rather than failing.

namespace sprt::unicode::detail {

// --- the trie ----------------------------------------------------------------

static constexpr Utrie2 s_caseTrie{
	s_caseTrieIndex,
	s_caseTrieIndexLength,
	s_caseTrieDataLength,
	s_caseTrieHighStart,
	s_caseTrieHighValueIndex,
};

static_assert(s_caseTrieIndexLength + s_caseTrieDataLength
				== int32_t(sizeof(s_caseTrieIndex) / sizeof(uint16_t)),
		"case trie descriptor disagrees with the generated array");

// --- the 16-bit properties word ----------------------------------------------

// Low bits of every props word (ucase.h "definitions for 16-bit case properties
// word"). The numbering is ICU's and is used as a value, not just a flag.
enum : uint16_t {
	CaseTypeMask = 3, // UCASE_TYPE_MASK
	CaseIgnorable = 4, // UCASE_IGNORABLE
	CaseException = 8, // UCASE_EXCEPTION
	CaseSensitive = 0x10, // UCASE_SENSITIVE
	CaseDotMask = 0x60, // UCASE_DOT_MASK
};

enum : uint16_t {
	CaseNone = 0, // UCASE_NONE
	CaseLower = 1, // UCASE_LOWER
	CaseUpper = 2, // UCASE_UPPER
	CaseTitle = 3, // UCASE_TITLE
};

enum : uint16_t {
	DotNone = 0, // UCASE_NO_DOT: normal characters with cc=0
	DotSoftDotted = 0x20, // UCASE_SOFT_DOTTED
	DotAbove = 0x40, // UCASE_ABOVE: "above" accents with cc=230
	DotOtherAccent = 0x60, // UCASE_OTHER_ACCENT: other accent, 0 < cc != 230
};

// No exception: bits 15..7 are a 9-bit signed case mapping delta.
static constexpr int DeltaShift = 7; // UCASE_DELTA_SHIFT

// Exception: bits 15..4 are an unsigned 12-bit index into the exceptions array.
static constexpr int ExcShift = 4; // UCASE_EXC_SHIFT

static constexpr uint16_t caseType(uint16_t props) { return props & CaseTypeMask; }

static constexpr bool isUpperOrTitle(uint16_t props) {
	return (props & 2) != 0; // UCASE_IS_UPPER_OR_TITLE
}

static constexpr bool hasException(uint16_t props) { return (props & CaseException) != 0; }

// UCASE_GET_DELTA. Signed right shift is arithmetic since C++20, so ICU's
// non-arithmetic-shift branch has no counterpart here.
static constexpr int16_t getDelta(uint16_t props) {
	return int16_t(int16_t(props) >> DeltaShift);
}

// --- the exceptions array ----------------------------------------------------

// First 8 bits of the main exception word say which optional slots are present;
// the slots themselves follow, in this order, only for the bits that are set.
enum : int32_t {
	ExcLower = 0, // UCASE_EXC_LOWER
	ExcFold = 1, // UCASE_EXC_FOLD
	ExcUpper = 2, // UCASE_EXC_UPPER
	ExcTitle = 3, // UCASE_EXC_TITLE
	ExcDelta = 4, // UCASE_EXC_DELTA
	ExcClosure = 6, // UCASE_EXC_CLOSURE
	ExcFullMappings = 7, // UCASE_EXC_FULL_MAPPINGS
};

enum : uint16_t {
	ExcDoubleSlots = 0x100, // UCASE_EXC_DOUBLE_SLOTS: each slot is 2 uint16_t
	ExcNoSimpleCaseFolding = 0x200, // UCASE_EXC_NO_SIMPLE_CASE_FOLDING
	ExcDeltaIsNegative = 0x400, // UCASE_EXC_DELTA_IS_NEGATIVE
	ExcSensitive = 0x800, // UCASE_EXC_SENSITIVE
	ExcDotMask = 0x3000, // UCASE_EXC_DOT_MASK
	ExcConditionalSpecial = 0x4000, // UCASE_EXC_CONDITIONAL_SPECIAL
	ExcConditionalFold = 0x8000, // UCASE_EXC_CONDITIONAL_FOLD
};

// UCASE_EXC_DOT_MASK == UCASE_DOT_MASK << UCASE_EXC_DOT_SHIFT: the dot type is
// normally in the main word but is pushed out here for large exception indexes.
static constexpr int ExcDotShift = 7; // UCASE_EXC_DOT_SHIFT

// clang-format off
// Number of set bits in an 8-bit value: how many slots precede slot `idx`.
static constexpr uint8_t s_flagsOffset[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};
// clang-format on

// GET_EXCEPTIONS
static constexpr const uint16_t *exceptionsFor(uint16_t props) {
	return s_caseExceptions + (props >> ExcShift);
}

// HAS_SLOT
static constexpr bool hasSlot(uint16_t flags, int32_t idx) {
	return (flags & (1 << idx)) != 0;
}

// SLOT_OFFSET
static constexpr uint8_t slotOffset(uint16_t flags, int32_t idx) {
	return s_flagsOffset[flags & ((1 << idx) - 1)];
}

// GET_SLOT_VALUE. Requires hasSlot(excWord, idx). `pe` must already be past the
// main exception word, and is left on the LAST uint16_t of the slot value, so a
// caller reading the next slot advances by one first - the same contract as the
// ICU macro this replaces.
static constexpr int32_t getSlotValue(uint16_t excWord, int32_t idx, const uint16_t *&pe) {
	if ((excWord & ExcDoubleSlots) == 0) {
		pe += slotOffset(excWord, idx);
		return int32_t(*pe);
	} else {
		pe += 2 * slotOffset(excWord, idx);
		int32_t value = *pe++;
		return (value << 16) | int32_t(*pe);
	}
}

// --- simple case mappings ----------------------------------------------------

// ucase_tolower
char32_t toLowerSimple(char32_t c) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (isUpperOrTitle(props)) {
			c += getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		if (hasSlot(excWord, ExcDelta) && isUpperOrTitle(props)) {
			auto delta = getSlotValue(excWord, ExcDelta, pe);
			return (excWord & ExcDeltaIsNegative) == 0 ? c + delta : c - delta;
		}
		if (hasSlot(excWord, ExcLower)) {
			c = char32_t(getSlotValue(excWord, ExcLower, pe));
		}
	}
	return c;
}

// ucase_toupper
char32_t toUpperSimple(char32_t c) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (caseType(props) == CaseLower) {
			c += getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		if (hasSlot(excWord, ExcDelta) && caseType(props) == CaseLower) {
			auto delta = getSlotValue(excWord, ExcDelta, pe);
			return (excWord & ExcDeltaIsNegative) == 0 ? c + delta : c - delta;
		}
		if (hasSlot(excWord, ExcUpper)) {
			c = char32_t(getSlotValue(excWord, ExcUpper, pe));
		}
	}
	return c;
}

// ucase_totitle. Titlecase falls back to uppercase, and then to the input: most
// characters have no titlecase form of their own.
char32_t toTitleSimple(char32_t c) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (caseType(props) == CaseLower) {
			c += getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		if (hasSlot(excWord, ExcDelta) && caseType(props) == CaseLower) {
			auto delta = getSlotValue(excWord, ExcDelta, pe);
			return (excWord & ExcDeltaIsNegative) == 0 ? c + delta : c - delta;
		}
		int32_t idx;
		if (hasSlot(excWord, ExcTitle)) {
			idx = ExcTitle;
		} else if (hasSlot(excWord, ExcUpper)) {
			idx = ExcUpper;
		} else {
			return c;
		}
		c = char32_t(getSlotValue(excWord, idx, pe));
	}
	return c;
}

// --- case properties ---------------------------------------------------------
//
// Nothing calls these yet: they are what the 1:N and contextual mappings ask the
// data (soft-dotted for the Lithuanian rules, the type for Final_Sigma and for
// title-casing word boundaries). Ported with the mappings above because they are
// the same twelve lines of trie and exception decoding.

// ucase_getType: CaseNone, CaseLower, CaseUpper or CaseTitle.
uint16_t caseTypeOf(char32_t c) { return caseType(s_caseTrie.get(c)); }

// ucase_getTypeOrIgnorable: as above, plus CaseIgnorable in bit 2.
uint16_t caseTypeOrIgnorable(char32_t c) {
	return s_caseTrie.get(c) & 7; // UCASE_GET_TYPE_AND_IGNORABLE
}

// getDotType: DotNone, DotSoftDotted, DotAbove or DotOtherAccent.
uint16_t dotTypeOf(char32_t c) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		return props & CaseDotMask;
	} else {
		const uint16_t *pe = exceptionsFor(props);
		return uint16_t((*pe >> ExcDotShift) & CaseDotMask);
	}
}

// ucase_isSoftDotted
bool isSoftDotted(char32_t c) { return dotTypeOf(c) == DotSoftDotted; }

// ucase_isCaseSensitive
bool isCaseSensitive(char32_t c) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		return (props & CaseSensitive) != 0;
	} else {
		const uint16_t *pe = exceptionsFor(props);
		return (*pe & ExcSensitive) != 0;
	}
}

// --- casing locale -----------------------------------------------------------

// The four languages whose case mappings differ from root, plus the two that
// only differ for the full mappings. Values are ICU's UCASE_LOC_*.
enum class CaseLocale : int32_t {
	Unknown = 0,
	Root = 1,
	Turkish = 2,
	Lithuanian = 3,
	Greek = 4,
	Dutch = 5,
	Armenian = 6,
};

// ucase_getCaseLocale. Requires a NUL-terminated locale id, and does the
// equivalent of uloc_getLanguage() without depending on it: it accepts 2- and
// 3-letter codes and either case, and stops at the first separator.
//
// Like the properties above this has no caller until the locale-sensitive
// mappings land; it is here because it is self-contained and belongs with them.
CaseLocale caseLocaleOf(const char *locale) {
	auto isChar = [](char c, char lower, char upper) { return c == lower || c == upper; };
	auto isSep = [](char c) { return c == '_' || c == '-' || c == 0; };

	char c = *locale++;
	// Fast path for "en" (the usual spelling of "root case mappings") and "zh",
	// both very common and neither with any special behaviour. Then split on
	// lowercase vs uppercase to shorten the comparison chain for everything else.
	if (c == 'e') {
		// el or ell?
		c = *locale++;
		if (isChar(c, 'l', 'L')) {
			c = *locale++;
			if (isChar(c, 'l', 'L')) {
				c = *locale;
			}
			if (isSep(c)) {
				return CaseLocale::Greek;
			}
		}
		// en, es, ... -> root
	} else if (c == 'z') {
		return CaseLocale::Root;
	} else if (c >= 'a') { // ASCII a-z = 0x61..0x7a, after A-Z
		if (c == 't') {
			// tr or tur?
			c = *locale++;
			if (isChar(c, 'u', 'U')) {
				c = *locale++;
			}
			if (isChar(c, 'r', 'R')) {
				c = *locale;
				if (isSep(c)) {
					return CaseLocale::Turkish;
				}
			}
		} else if (c == 'a') {
			// az or aze?
			c = *locale++;
			if (isChar(c, 'z', 'Z')) {
				c = *locale++;
				if (isChar(c, 'e', 'E')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Turkish;
				}
			}
		} else if (c == 'l') {
			// lt or lit?
			c = *locale++;
			if (isChar(c, 'i', 'I')) {
				c = *locale++;
			}
			if (isChar(c, 't', 'T')) {
				c = *locale;
				if (isSep(c)) {
					return CaseLocale::Lithuanian;
				}
			}
		} else if (c == 'n') {
			// nl or nld?
			c = *locale++;
			if (isChar(c, 'l', 'L')) {
				c = *locale++;
				if (isChar(c, 'd', 'D')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Dutch;
				}
			}
		} else if (c == 'h') {
			// hy or hye? *not* hyw
			c = *locale++;
			if (isChar(c, 'y', 'Y')) {
				c = *locale++;
				if (isChar(c, 'e', 'E')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Armenian;
				}
			}
		}
	} else {
		// Uppercase c. Same shape as above, but 'E' also has to be checked here
		// because the 'e' fast path above only caught the lowercase spelling.
		if (c == 'T') {
			c = *locale++;
			if (isChar(c, 'u', 'U')) {
				c = *locale++;
			}
			if (isChar(c, 'r', 'R')) {
				c = *locale;
				if (isSep(c)) {
					return CaseLocale::Turkish;
				}
			}
		} else if (c == 'A') {
			c = *locale++;
			if (isChar(c, 'z', 'Z')) {
				c = *locale++;
				if (isChar(c, 'e', 'E')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Turkish;
				}
			}
		} else if (c == 'L') {
			c = *locale++;
			if (isChar(c, 'i', 'I')) {
				c = *locale++;
			}
			if (isChar(c, 't', 'T')) {
				c = *locale;
				if (isSep(c)) {
					return CaseLocale::Lithuanian;
				}
			}
		} else if (c == 'E') {
			c = *locale++;
			if (isChar(c, 'l', 'L')) {
				c = *locale++;
				if (isChar(c, 'l', 'L')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Greek;
				}
			}
		} else if (c == 'N') {
			c = *locale++;
			if (isChar(c, 'l', 'L')) {
				c = *locale++;
				if (isChar(c, 'd', 'D')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Dutch;
				}
			}
		} else if (c == 'H') {
			c = *locale++;
			if (isChar(c, 'y', 'Y')) {
				c = *locale++;
				if (isChar(c, 'e', 'E')) {
					c = *locale;
				}
				if (isSep(c)) {
					return CaseLocale::Armenian;
				}
			}
		}
	}
	return CaseLocale::Root;
}

} // namespace sprt::unicode::detail
