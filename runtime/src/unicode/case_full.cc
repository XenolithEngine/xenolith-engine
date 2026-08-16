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

// The full (1:N) and context-sensitive case mappings. Ported from ICU ucase.cpp
// (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// This is everything the simple mappings in case_props.cc cannot express:
//
//   * results longer than one code point   -- ß -> SS, ﬁ -> FI, ŉ -> ʼN
//   * results that depend on the text around the character
//                                          -- final sigma Σ -> ς
//   * results that depend on the language  -- Turkish I/ı, Lithuanian dots,
//                                             Eastern vs Western Armenian և
//
// Two conventions are transcribed from ICU verbatim because both callers - the
// UTF-16 mapper and the UTF-8 one - are built on them:
//
//   The return value carries three cases at once. A negative value means "maps
//   to itself" and holds ~c, not -c, so that U+0000 survives the round trip. A
//   value in 0..MaxStringLength is a STRING LENGTH, and the string itself was
//   written to *pString. Anything larger is a single replacement code point.
//
//   The context iterator never returns the character being mapped. It walks away
//   from it, backwards or forwards, and returns a negative value at the end of
//   the text. `dir` selects the direction on the first call of a scan and is 0
//   on every call after that, meaning "keep going the same way".
//
// The 1:N results are not copied out of the tables: *pString points straight
// into s_caseExceptions, which is uint16_t while the result is UTF-16, so the
// reinterpret_cast below is ICU's and is kept. It is safe because the two types
// have the same size and representation on every target here, and the pointer is
// only ever read from.
//
// The mappings that ICU hardcodes are hardcoded here too - the conditions in
// SpecialCasing.txt cannot be expressed in the data format, so ucase's builder
// only marks the code point as conditional and leaves the rule to the code.

namespace sprt::unicode::detail {

// --- fold options ------------------------------------------------------------

// stringoptions.h. Only bit 0 is defined; the mask is what ICU compares against.
enum : uint32_t {
	FoldCaseDefault = 0, // U_FOLD_CASE_DEFAULT
	FoldCaseExcludeSpecialI = 1, // U_FOLD_CASE_EXCLUDE_SPECIAL_I
	FoldCaseOptionsMask = 7, // _FOLD_CASE_OPTIONS_MASK
};

// A result of 0..MaxStringLength is a string length rather than a code point.
static constexpr int32_t MaxStringLength = 0x1f; // UCASE_MAX_STRING_LENGTH

// Lengths word for the full case mappings slot.
enum : int32_t {
	FullLower = 0xf, // UCASE_FULL_LOWER
	FullFolding = 0xf0, // UCASE_FULL_FOLDING
	FullUpper = 0xf00, // UCASE_FULL_UPPER
	FullTitle = 0xf000, // UCASE_FULL_TITLE
};

// --- hardcoded mapping results -----------------------------------------------

// The results of the conditional rules below. i + combining dot above, and the
// three precomposed Lithuanian forms that decompose into it.
static constexpr char16_t s_iDot[2] = {0x69, 0x307};
static constexpr char16_t s_jDot[2] = {0x6a, 0x307};
static constexpr char16_t s_iOgonekDot[2] = {0x12f, 0x307};
static constexpr char16_t s_iDotGrave[3] = {0x69, 0x307, 0x300};
static constexpr char16_t s_iDotAcute[3] = {0x69, 0x307, 0x301};
static constexpr char16_t s_iDotTilde[3] = {0x69, 0x307, 0x303};

// ICU-13416: the ech-yiwn ligature uppercases to ech+yiwn by default and in
// Western Armenian, but to ech+vew in Eastern Armenian.
static constexpr char16_t s_echVewUpper[2] = {0x535, 0x54E}; // ԵՎ
static constexpr char16_t s_echVewTitle[2] = {0x535, 0x57E}; // Եվ
static constexpr char16_t s_echYiwnUpper[2] = {0x535, 0x552}; // ԵՒ
static constexpr char16_t s_echYiwnTitle[2] = {0x535, 0x582}; // Եւ

// --- the context iterator ----------------------------------------------------

// Walks away from the character being mapped. dir < 0 starts a backward scan,
// dir > 0 a forward one, dir == 0 continues in the current direction. Returns a
// negative value when there is nothing left. The character being mapped is never
// returned.
using CaseContextIterator = int32_t (*)(void *context, int8_t dir);

// The working data every iterator in this port uses (ICU's UCaseContext). `p` is
// the text; [start, limit) is the part of it that may be looked at; [cpStart,
// cpLimit) is the character being mapped, which the scan starts from. Indexes
// are in the text's own units - UTF-16 units for the UTF-16 mapper, bytes for
// the UTF-8 one.
struct CaseContext {
	const void *p = nullptr;
	int32_t start = 0, index = 0, limit = 0;
	int32_t cpStart = 0, cpLimit = 0;
	int8_t dir = 0;
	int8_t b1 = 0, b2 = 0, b3 = 0;
};

// --- the conditions ----------------------------------------------------------

/*
 * Is followed by
 *   {case-ignorable}* cased
 * ?
 * (dir determines looking forward/backward)
 * If a character is case-ignorable, it is skipped regardless of whether
 * it is also cased or not.
 */
static bool isFollowedByCasedLetter(CaseContextIterator iter, void *context, int8_t dir) {
	if (iter == nullptr) {
		return false;
	}

	for (int32_t c; (c = iter(context, dir)) >= 0; dir = 0) {
		uint16_t type = caseTypeOrIgnorable(char32_t(c));
		if (type & CaseIgnorable) {
			// case-ignorable, continue with the loop
		} else if (type != CaseNone) {
			return true; // followed by cased letter
		} else {
			return false; // uncased and not case-ignorable
		}
	}

	return false; // not followed by cased letter
}

// Is preceded by Soft_Dotted character with no intervening cc=230 ?
static bool isPrecededBySoftDotted(CaseContextIterator iter, void *context) {
	if (iter == nullptr) {
		return false;
	}

	int8_t dir = -1;
	for (int32_t c; (c = iter(context, dir)) >= 0; dir = 0) {
		uint16_t dotType = dotTypeOf(char32_t(c));
		if (dotType == DotSoftDotted) {
			return true; // preceded by TYPE_i
		} else if (dotType != DotOtherAccent) {
			// preceded by a different base character (not TYPE_i), or an
			// intervening cc == 230
			return false;
		}
	}

	return false; // not preceded by TYPE_i
}

/*
 * Is preceded by base character 'I' with no intervening cc=230 ?
 *
 * The condition is After_I, not After_Soft_Dotted: SpecialCasing.txt had it
 * wrong until the 2002-10-31 Unicode erratum, and Unicode 3.2 described it as
 * "when lowercasing, remove dot_above in the sequence I + dot_above, which will
 * turn into i" - matching the canonically equivalent I-dot-above.
 */
static bool isPrecededBy_I(CaseContextIterator iter, void *context) {
	if (iter == nullptr) {
		return false;
	}

	int8_t dir = -1;
	for (int32_t c; (c = iter(context, dir)) >= 0; dir = 0) {
		if (c == 0x49) {
			return true; // preceded by I
		}
		uint16_t dotType = dotTypeOf(char32_t(c));
		if (dotType != DotOtherAccent) {
			// preceded by a different base character (not I), or an intervening
			// cc == 230
			return false;
		}
	}

	return false; // not preceded by I
}

// Is followed by one or more cc==230 ?
static bool isFollowedByMoreAbove(CaseContextIterator iter, void *context) {
	if (iter == nullptr) {
		return false;
	}

	int8_t dir = 1;
	for (int32_t c; (c = iter(context, dir)) >= 0; dir = 0) {
		uint16_t dotType = dotTypeOf(char32_t(c));
		if (dotType == DotAbove) {
			return true; // at least one cc==230 following
		} else if (dotType != DotOtherAccent) {
			return false; // next base character, no more cc==230 following
		}
	}

	return false; // no more cc==230 following
}

// Is followed by a dot above (without cc==230 in between) ?
static bool isFollowedByDotAbove(CaseContextIterator iter, void *context) {
	if (iter == nullptr) {
		return false;
	}

	int8_t dir = 1;
	for (int32_t c; (c = iter(context, dir)) >= 0; dir = 0) {
		if (c == 0x307) {
			return true;
		}
		uint16_t dotType = dotTypeOf(char32_t(c));
		if (dotType != DotOtherAccent) {
			return false; // next base character or cc==230 in between
		}
	}

	return false; // no dot above following
}

// --- full lowercase ----------------------------------------------------------

// ucase_toFullLower. `c` must be non-negative: the sign of the result is what
// says whether anything changed.
int32_t toFullLower(char32_t c, CaseContextIterator iter, void *context, const char16_t **pString,
		CaseLocale loc) {
	int32_t result = int32_t(c);
	// Reset the output pointer in case it was uninitialized.
	*pString = nullptr;
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (isUpperOrTitle(props)) {
			result = int32_t(c) + getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		const uint16_t *pe2 = pe;

		if (excWord & ExcConditionalSpecial) {
			/*
			 * Test for conditional mappings first
			 *   (otherwise the unconditional default mappings are always taken),
			 * then test for characters that have unconditional mappings in
			 * SpecialCasing.txt, then get the UnicodeData.txt mappings.
			 */
			if (loc == CaseLocale::Lithuanian
					&& // base characters, find accents above
					(((c == 0x49 || c == 0x4a || c == 0x12e)
							 && isFollowedByMoreAbove(iter, context))
							// precomposed with accent above, no need to find one
							|| (c == 0xcc || c == 0xcd || c == 0x128))) {
				/*
				    # Lithuanian

				    # Lithuanian retains the dot in a lowercase i when followed by accents.

				    # Introduce an explicit dot above when lowercasing capital I's and J's
				    # whenever there are more accents above.
				    # (of the accents used in Lithuanian: grave, acute, tilde above, and ogonek)

				    0049; 0069 0307; 0049; 0049; lt More_Above; # LATIN CAPITAL LETTER I
				    004A; 006A 0307; 004A; 004A; lt More_Above; # LATIN CAPITAL LETTER J
				    012E; 012F 0307; 012E; 012E; lt More_Above; # LATIN CAPITAL LETTER I WITH OGONEK
				    00CC; 0069 0307 0300; 00CC; 00CC; lt; # LATIN CAPITAL LETTER I WITH GRAVE
				    00CD; 0069 0307 0301; 00CD; 00CD; lt; # LATIN CAPITAL LETTER I WITH ACUTE
				    0128; 0069 0307 0303; 0128; 0128; lt; # LATIN CAPITAL LETTER I WITH TILDE
				 */
				switch (c) {
				case 0x49: // LATIN CAPITAL LETTER I
					*pString = s_iDot;
					return 2;
				case 0x4a: // LATIN CAPITAL LETTER J
					*pString = s_jDot;
					return 2;
				case 0x12e: // LATIN CAPITAL LETTER I WITH OGONEK
					*pString = s_iOgonekDot;
					return 2;
				case 0xcc: // LATIN CAPITAL LETTER I WITH GRAVE
					*pString = s_iDotGrave;
					return 3;
				case 0xcd: // LATIN CAPITAL LETTER I WITH ACUTE
					*pString = s_iDotAcute;
					return 3;
				case 0x128: // LATIN CAPITAL LETTER I WITH TILDE
					*pString = s_iDotTilde;
					return 3;
				default: return 0; // will not occur
				}
				// # Turkish and Azeri
			} else if (loc == CaseLocale::Turkish && c == 0x130) {
				/*
				    # I and i-dotless; I-dot and i are case pairs in Turkish and Azeri
				    # The following rules handle those cases.

				    0130; 0069; 0130; 0130; tr # LATIN CAPITAL LETTER I WITH DOT ABOVE
				    0130; 0069; 0130; 0130; az # LATIN CAPITAL LETTER I WITH DOT ABOVE
				 */
				return 0x69;
			} else if (loc == CaseLocale::Turkish && c == 0x307 && isPrecededBy_I(iter, context)) {
				/*
				    # When lowercasing, remove dot_above in the sequence I + dot_above,
				    # which will turn into i.
				    # This matches the behavior of the canonically equivalent I-dot_above

				    0307; ; 0307; 0307; tr After_I; # COMBINING DOT ABOVE
				    0307; ; 0307; 0307; az After_I; # COMBINING DOT ABOVE
				 */
				return 0; // remove the dot (continue without output)
			} else if (loc == CaseLocale::Turkish && c == 0x49
					&& !isFollowedByDotAbove(iter, context)) {
				/*
				    # When lowercasing, unless an I is before a dot_above,
				    # it turns into a dotless i.

				    0049; 0131; 0049; 0049; tr Not_Before_Dot; # LATIN CAPITAL LETTER I
				    0049; 0131; 0049; 0049; az Not_Before_Dot; # LATIN CAPITAL LETTER I
				 */
				return 0x131;
			} else if (c == 0x130) {
				/*
				    # Preserve canonical equivalence for I with dot. Turkic is handled above.

				    0130; 0069 0307; 0130; 0130; # LATIN CAPITAL LETTER I WITH DOT ABOVE
				 */
				*pString = s_iDot;
				return 2;
			} else if (c == 0x3a3 && !isFollowedByCasedLetter(iter, context, 1)
					&& isFollowedByCasedLetter(iter, context, -1) /* -1=preceded */) {
				/* greek capital sigma maps depending on surrounding cased letters
				   (see SpecialCasing.txt) */
				/*
				    # Special case for final form of sigma

				    03A3; 03C2; 03A3; 03A3; Final_Sigma; # GREEK CAPITAL LETTER SIGMA
				 */
				return 0x3c2; // greek small final sigma
			} else {
				// no known conditional special case mapping, use a normal mapping
			}
		} else if (hasSlot(excWord, ExcFullMappings)) {
			int32_t full = getSlotValue(excWord, ExcFullMappings, pe);
			full &= FullLower;
			if (full != 0) {
				// set the output pointer to the lowercase mapping
				*pString = reinterpret_cast<const char16_t *>(pe + 1);

				// return the string length
				return full;
			}
		}

		if (hasSlot(excWord, ExcDelta) && isUpperOrTitle(props)) {
			int32_t delta = getSlotValue(excWord, ExcDelta, pe2);
			return (excWord & ExcDeltaIsNegative) == 0 ? int32_t(c) + delta : int32_t(c) - delta;
		}
		if (hasSlot(excWord, ExcLower)) {
			result = getSlotValue(excWord, ExcLower, pe2);
		}
	}

	return (result == int32_t(c)) ? ~result : result;
}

// --- full uppercase and titlecase --------------------------------------------

// toUpperOrTitle. Titlecase differs from uppercase only in the slot it reads and
// in the Armenian ligature, so ICU keeps them in one function; so does this.
static int32_t toUpperOrTitle(char32_t c, CaseContextIterator iter, void *context,
		const char16_t **pString, CaseLocale loc, bool upperNotTitle) {
	int32_t result = int32_t(c);
	// Reset the output pointer in case it was uninitialized.
	*pString = nullptr;
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (caseType(props) == CaseLower) {
			result = int32_t(c) + getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		const uint16_t *pe2 = pe;

		if (excWord & ExcConditionalSpecial) {
			// use hardcoded conditions and mappings
			if (loc == CaseLocale::Turkish && c == 0x69) {
				/*
				    # Turkish and Azeri

				    # I and i-dotless; I-dot and i are case pairs in Turkish and Azeri
				    # The following rules handle those cases.

				    # When uppercasing, i turns into a dotted capital I

				    0069; 0069; 0130; 0130; tr; # LATIN SMALL LETTER I
				    0069; 0069; 0130; 0130; az; # LATIN SMALL LETTER I
				*/
				return 0x130;
			} else if (loc == CaseLocale::Lithuanian && c == 0x307
					&& isPrecededBySoftDotted(iter, context)) {
				/*
				    # Lithuanian

				    # Lithuanian retains the dot in a lowercase i when followed by accents.

				    # Remove DOT ABOVE after "i" with upper or titlecase

				    0307; 0307; ; ; lt After_Soft_Dotted; # COMBINING DOT ABOVE
				 */
				return 0; // remove the dot (continue without output)
			} else if (c == 0x0587) {
				// See ICU-13416:
				// և ligature ech-yiwn
				// uppercases to ԵՒ=ech+yiwn by default and in Western Armenian,
				// but to ԵՎ=ech+vew in Eastern Armenian.
				if (loc == CaseLocale::Armenian) {
					*pString = upperNotTitle ? s_echVewUpper : s_echVewTitle;
				} else {
					*pString = upperNotTitle ? s_echYiwnUpper : s_echYiwnTitle;
				}
				return 2;
			} else {
				// no known conditional special case mapping, use a normal mapping
			}
		} else if (hasSlot(excWord, ExcFullMappings)) {
			int32_t full = getSlotValue(excWord, ExcFullMappings, pe);

			// start of full case mapping strings
			++pe;

			// skip the lowercase and case-folding result strings
			pe += full & FullLower;
			full >>= 4;
			pe += full & 0xf;
			full >>= 4;

			if (upperNotTitle) {
				full &= 0xf;
			} else {
				// skip the uppercase result string
				pe += full & 0xf;
				full = (full >> 4) & 0xf;
			}

			if (full != 0) {
				// set the output pointer to the result string
				*pString = reinterpret_cast<const char16_t *>(pe);

				// return the string length
				return full;
			}
		}

		if (hasSlot(excWord, ExcDelta) && caseType(props) == CaseLower) {
			int32_t delta = getSlotValue(excWord, ExcDelta, pe2);
			return (excWord & ExcDeltaIsNegative) == 0 ? int32_t(c) + delta : int32_t(c) - delta;
		}
		int32_t idx;
		if (!upperNotTitle && hasSlot(excWord, ExcTitle)) {
			idx = ExcTitle;
		} else if (hasSlot(excWord, ExcUpper)) {
			// here, titlecase is same as uppercase
			idx = ExcUpper;
		} else {
			return ~int32_t(c);
		}
		result = getSlotValue(excWord, idx, pe2);
	}

	return (result == int32_t(c)) ? ~result : result;
}

// ucase_toFullUpper
int32_t toFullUpper(char32_t c, CaseContextIterator iter, void *context, const char16_t **pString,
		CaseLocale caseLocale) {
	return toUpperOrTitle(c, iter, context, pString, caseLocale, true);
}

// ucase_toFullTitle
int32_t toFullTitle(char32_t c, CaseContextIterator iter, void *context, const char16_t **pString,
		CaseLocale caseLocale) {
	return toUpperOrTitle(c, iter, context, pString, caseLocale, false);
}

// --- case folding ------------------------------------------------------------

/*
 * Case folding is similar to lowercasing.
 * The result may be a simple mapping, i.e., a single code point, or
 * a full mapping, i.e., a string.
 * If the case folding for a code point is the same as its simple (1:1) lowercase
 * mapping, then only the lowercase mapping is stored.
 *
 * Some special cases are hardcoded because their conditions cannot be
 * parsed and processed from CaseFolding.txt. CaseFolding.txt has two 'T'
 * (Turkic) rows:
 *
 *   0049; T; 0131; # LATIN CAPITAL LETTER I
 *   0130; T; 0069; # LATIN CAPITAL LETTER I WITH DOT ABOVE
 *
 * while the default mappings for those two are
 *
 *   0049; C; 0069; # LATIN CAPITAL LETTER I
 *   0130; F; 0069 0307; # LATIN CAPITAL LETTER I WITH DOT ABOVE
 *
 * and U+0130 has no simple case folding at all (it folds to itself).
 *
 * Note that Turkic folding does not preserve canonical equivalence, unlike the
 * default: I-grave and I + grave fold to strings that are not canonically
 * equivalent. That is not fixable for uppercase and lowercase together and is
 * why the Turkic rows are opt-in.
 */

// ucase_fold: the simple case folding for c.
char32_t foldSimple(char32_t c, uint32_t options) {
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (isUpperOrTitle(props)) {
			c += getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		if (excWord & ExcConditionalFold) {
			// special case folding mappings, hardcoded
			if ((options & FoldCaseOptionsMask) == FoldCaseDefault) {
				// default mappings
				if (c == 0x49) {
					// 0049; C; 0069; # LATIN CAPITAL LETTER I
					return 0x69;
				} else if (c == 0x130) {
					// no simple case folding for U+0130
					return c;
				}
			} else {
				// Turkic mappings
				if (c == 0x49) {
					// 0049; T; 0131; # LATIN CAPITAL LETTER I
					return 0x131;
				} else if (c == 0x130) {
					// 0130; T; 0069; # LATIN CAPITAL LETTER I WITH DOT ABOVE
					return 0x69;
				}
			}
		}
		if ((excWord & ExcNoSimpleCaseFolding) != 0) {
			return c;
		}
		if (hasSlot(excWord, ExcDelta) && isUpperOrTitle(props)) {
			int32_t delta = getSlotValue(excWord, ExcDelta, pe);
			return (excWord & ExcDeltaIsNegative) == 0 ? c + delta : c - delta;
		}
		int32_t idx;
		if (hasSlot(excWord, ExcFold)) {
			idx = ExcFold;
		} else if (hasSlot(excWord, ExcLower)) {
			idx = ExcLower;
		} else {
			return c;
		}
		c = char32_t(getSlotValue(excWord, idx, pe));
	}
	return c;
}

// ucase_toFullFolding
int32_t toFullFolding(char32_t c, const char16_t **pString, uint32_t options) {
	int32_t result = int32_t(c);
	// Reset the output pointer in case it was uninitialized.
	*pString = nullptr;
	uint16_t props = s_caseTrie.get(c);
	if (!hasException(props)) {
		if (isUpperOrTitle(props)) {
			result = int32_t(c) + getDelta(props);
		}
	} else {
		const uint16_t *pe = exceptionsFor(props);
		uint16_t excWord = *pe++;
		const uint16_t *pe2 = pe;

		if (excWord & ExcConditionalFold) {
			// use hardcoded conditions and mappings
			if ((options & FoldCaseOptionsMask) == FoldCaseDefault) {
				// default mappings
				if (c == 0x49) {
					// 0049; C; 0069; # LATIN CAPITAL LETTER I
					return 0x69;
				} else if (c == 0x130) {
					// 0130; F; 0069 0307; # LATIN CAPITAL LETTER I WITH DOT ABOVE
					*pString = s_iDot;
					return 2;
				}
			} else {
				// Turkic mappings
				if (c == 0x49) {
					// 0049; T; 0131; # LATIN CAPITAL LETTER I
					return 0x131;
				} else if (c == 0x130) {
					// 0130; T; 0069; # LATIN CAPITAL LETTER I WITH DOT ABOVE
					return 0x69;
				}
			}
		} else if (hasSlot(excWord, ExcFullMappings)) {
			int32_t full = getSlotValue(excWord, ExcFullMappings, pe);

			// start of full case mapping strings
			++pe;

			// skip the lowercase result string
			pe += full & FullLower;
			full = (full >> 4) & 0xf;

			if (full != 0) {
				// set the output pointer to the result string
				*pString = reinterpret_cast<const char16_t *>(pe);

				// return the string length
				return full;
			}
		}

		if ((excWord & ExcNoSimpleCaseFolding) != 0) {
			return ~int32_t(c);
		}
		if (hasSlot(excWord, ExcDelta) && isUpperOrTitle(props)) {
			int32_t delta = getSlotValue(excWord, ExcDelta, pe2);
			return (excWord & ExcDeltaIsNegative) == 0 ? int32_t(c) + delta : int32_t(c) - delta;
		}
		int32_t idx;
		if (hasSlot(excWord, ExcFold)) {
			idx = ExcFold;
		} else if (hasSlot(excWord, ExcLower)) {
			idx = ExcLower;
		} else {
			return ~int32_t(c);
		}
		result = getSlotValue(excWord, idx, pe2);
	}

	return (result == int32_t(c)) ? ~result : result;
}

} // namespace sprt::unicode::detail
