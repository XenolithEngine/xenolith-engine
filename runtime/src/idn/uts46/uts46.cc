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

// UTS #46: Unicode IDNA Compatibility Processing. Ported from libuidna
// src/u_uts46.cc (ICU uts46.cpp; © Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// The structure follows the original closely, because the algorithm is a
// specification and its step order is normative. What changed:
//
//  * The UTF-8 entry point is gone. ICU carries a second, byte-wise copy of the
//    ASCII fast path for it, then converts to UTF-16 anyway; here the input is
//    converted once, up front, and there is one implementation of everything.
//  * UnicodeString -> Utf16Buffer, IDNAInfo -> the internal ProcessState below,
//    UErrorCode -> a bool return (the only failure left is out of memory).
//  * The punycode codec is the engine's own (SPRuntimeIdn.cpp), reached through
//    the two adapters at the top of this file.
//
// Errors accumulate as a BITMASK here, exactly as in ICU: severeErrors and the
// U+FFFD interplay in processLabel() depend on being able to see the whole set at
// once. The mask is collapsed into a single Status only at the public boundary,
// in SPRuntimeIdn.cpp.

namespace sprt::idn::detail {

// Error bits. Values match ICU's UIDNA_ERROR_* so the mapping to the public
// Status enumerators and to idn2_rc stays mechanical.
enum : uint32_t {
	ErrEmptyLabel = 0x0001,
	ErrLabelTooLong = 0x0002,
	ErrDomainNameTooLong = 0x0004,
	ErrLeadingHyphen = 0x0008,
	ErrTrailingHyphen = 0x0010,
	ErrHyphen34 = 0x0020,
	ErrLeadingCombiningMark = 0x0040,
	ErrDisallowed = 0x0080,
	ErrPunycode = 0x0100,
	ErrLabelHasDot = 0x0200,
	ErrInvalidAceLabel = 0x0400,
	ErrBidi = 0x0800,
	ErrContextJ = 0x1000,
	ErrContextOPunctuation = 0x2000,
	ErrContextODigits = 0x4000,
};

// Option bits, matching ICU's UIDNA_* and the public sprt::idn::Options.
enum : uint32_t {
	OptUseStd3Rules = 0x0002,
	OptCheckBidi = 0x0004,
	OptCheckContextJ = 0x0008,
	OptNonTransitionalToAscii = 0x0010,
	OptNonTransitionalToUnicode = 0x0020,
	OptCheckContextO = 0x0040,
};

// Errors that usually leave a U+FFFD in the result string.
static constexpr uint32_t severeErrors =
		ErrLeadingCombiningMark | ErrDisallowed | ErrPunycode | ErrLabelHasDot | ErrInvalidAceLabel;

struct ProcessState {
	uint32_t options = 0;
	uint32_t errors = 0;
	uint32_t labelErrors = 0;
	bool isTransDiff = false;
	bool isBiDi = false;
	bool isOkBiDi = true;

	bool useStd3Rules() const { return (options & OptUseStd3Rules) != 0; }
};

// --- punycode adapters -------------------------------------------------------
//
// The engine's codec works on char32_t and reports failure as `false`; ICU's works
// on UTF-16 with a two-pass buffer-overflow protocol. These two functions are the
// seam, and they are where a mis-adaptation would be invisible: processLabel()
// relies on a failed decode meaning EXACTLY ErrPunycode and nothing else, and on
// the encoder seeing whole code points rather than surrogate halves.

// Decodes the part of an "xn--" label after the prefix. False on any malformed
// input (which the caller turns into ErrPunycode).
static bool punycodeDecodeLabel(const char16_t *src, int32_t length, Utf16Buffer &dest) {
	// Punycode is ASCII; anything else cannot be valid and would be truncated by
	// the cast below, so reject it here rather than silently mis-decoding.
	auto ascii = __sprt_typed_malloca(char, size_t(length) + 1);
	if (!ascii) {
		return false;
	}
	for (int32_t i = 0; i < length; ++i) {
		if (src[i] > 0x7F) {
			__sprt_freea(ascii);
			return false;
		}
		ascii[i] = char(src[i]);
	}

	// Each decoded code point consumes at least one input character.
	auto buf = __sprt_typed_malloca(char32_t, size_t(length) + 1);
	if (!buf) {
		__sprt_freea(ascii);
		return false;
	}
	size_t decodedLength = size_t(length) + 1;
	bool ok = punycode_decode(ascii, size_t(length), buf, &decodedLength);
	if (ok) {
		dest.clear();
		for (size_t i = 0; i < decodedLength; ++i) {
			if (!dest.appendCodepoint(buf[i])) {
				ok = false;
				break;
			}
		}
	}
	__sprt_freea(buf);
	__sprt_freea(ascii);
	return ok;
}

// Encodes a label as punycode, WITHOUT the "xn--" prefix. The label arrives as
// UTF-16, so surrogate pairs are combined into code points first - encoding the
// halves separately would corrupt every supplementary-plane label.
static bool punycodeEncodeLabel(const char16_t *label, int32_t labelLength, Utf16Buffer &dest) {
	auto buf = __sprt_typed_malloca(char32_t, size_t(labelLength) + 1);
	if (!buf) {
		return false;
	}
	size_t count = 0;
	for (int32_t i = 0; i < labelLength;) {
		uint8_t offset;
		buf[count++] = unicode::utf16Decode32(label + i, size_t(labelLength - i), offset);
		i += offset ? offset : 1;
	}

	size_t retSize = 0;
	bool ok = punycode_encode(buf, count, [&](char) { ++retSize; });
	if (ok) {
		dest.clear();
		if (!dest.reserve(int32_t(retSize) + 4)) {
			__sprt_freea(buf);
			return false;
		}
		auto out = dest.data();
		out[0] = u'x';
		out[1] = u'n';
		out[2] = u'-';
		out[3] = u'-';
		int32_t n = 4;
		punycode_encode(buf, count, [&](char c) { out[n++] = char16_t(uint8_t(c)); });
		dest.setSize(n);
	}
	__sprt_freea(buf);
	return ok;
}

// --- ASCII table -------------------------------------------------------------

// -1 disallowed, 0 valid, 1 mapped (uppercase -> lowercase).
// clang-format off
static constexpr int8_t s_asciiData[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	// 002D..002E; valid  #  HYPHEN-MINUS..FULL STOP
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0, -1,
	// 0030..0039; valid  #  DIGIT ZERO..DIGIT NINE
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1,
	// 0041..005A; mapped  #  LATIN CAPITAL LETTER A..LATIN CAPITAL LETTER Z
	-1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1, -1, -1, -1, -1,
	// 0061..007A; valid  #  LATIN SMALL LETTER A..LATIN SMALL LETTER Z
	-1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1,
};
// clang-format on

static bool isAsciiString(const Utf16Buffer &dest) {
	auto s = dest.data();
	auto limit = s + dest.size();
	while (s < limit) {
		if (*s++ > 0x7F) {
			return false;
		}
	}
	return true;
}

// Some non-ASCII characters are equivalent to sequences containing non-LDH ASCII.
// To find them: grep disallowed_STD3_valid IdnaMappingTable.txt
static bool isNonAsciiDisallowedStd3Valid(char32_t c) {
	return c == 0x2260 || c == 0x226E || c == 0x226F;
}

// --- the Bidi Rule (RFC 5893) ------------------------------------------------

static constexpr uint32_t L_MASK = bidiMask(BidiClass::LeftToRight);
static constexpr uint32_t R_AL_MASK =
		bidiMask(BidiClass::RightToLeft) | bidiMask(BidiClass::RightToLeftArabic);
static constexpr uint32_t L_R_AL_MASK = L_MASK | R_AL_MASK;
static constexpr uint32_t R_AL_AN_MASK = R_AL_MASK | bidiMask(BidiClass::ArabicNumber);
static constexpr uint32_t EN_AN_MASK =
		bidiMask(BidiClass::EuropeanNumber) | bidiMask(BidiClass::ArabicNumber);
static constexpr uint32_t R_AL_EN_AN_MASK = R_AL_MASK | EN_AN_MASK;
static constexpr uint32_t L_EN_MASK = L_MASK | bidiMask(BidiClass::EuropeanNumber);
static constexpr uint32_t ES_CS_ET_ON_BN_NSM_MASK = bidiMask(BidiClass::EuropeanNumberSeparator)
		| bidiMask(BidiClass::CommonNumberSeparator) | bidiMask(BidiClass::EuropeanNumberTerminator)
		| bidiMask(BidiClass::OtherNeutral) | bidiMask(BidiClass::BoundaryNeutral)
		| bidiMask(BidiClass::NonSpacingMark);
static constexpr uint32_t L_EN_ES_CS_ET_ON_BN_NSM_MASK = L_EN_MASK | ES_CS_ET_ON_BN_NSM_MASK;
static constexpr uint32_t R_AL_AN_EN_ES_CS_ET_ON_BN_NSM_MASK =
		R_AL_MASK | EN_AN_MASK | ES_CS_ET_ON_BN_NSM_MASK;

// Reads one code point forward without bounds checks. Safe here because unpaired
// surrogates have already been replaced by U+FFFD ("U16_NEXT_UNSAFE").
static char32_t nextUnsafe(const char16_t *s, int32_t &i) {
	char32_t c = s[i++];
	if (unicode::isUtf16HighSurrogate(char16_t(c))) {
		c = unicode::utf16CombineSurrogates(char16_t(c), s[i++]);
	}
	return c;
}

static char32_t prevUnsafe(const char16_t *s, int32_t &i) {
	char32_t c = s[--i];
	if (unicode::isUtf16LowSurrogate(char16_t(c))) {
		c = unicode::utf16CombineSurrogates(s[--i], char16_t(c));
	}
	return c;
}

// Bounds-checked backward step ("U16_PREV"). ICU uses the safe macro in the
// CONTEXTO rules and the unsafe one everywhere else; keeping that split matters,
// because the unsafe form reads s[-1] for a lone low surrogate at index 0.
static char32_t prevSafe(const char16_t *s, int32_t start, int32_t &i) {
	char32_t c = s[--i];
	if (unicode::isUtf16LowSurrogate(char16_t(c)) && i > start
			&& unicode::isUtf16HighSurrogate(s[i - 1])) {
		--i;
		c = unicode::utf16CombineSurrogates(s[i], char16_t(c));
	}
	return c;
}

// Scans a label for both "does it contain RTL characters" and "does it pass the
// Bidi Rule". A name is a BiDi domain name as soon as one label is RTL, which may
// only become known after several labels have been processed.
static void checkLabelBiDi(const char16_t *label, int32_t labelLength, ProcessState &info) {
	// Directionality of the first character.
	int32_t i = 0;
	char32_t c = nextUnsafe(label, i);
	uint32_t firstMask = bidiMask(bidiClass(c));
	// 1. The first character must have BIDI property L, R or AL. R or AL makes it
	// an RTL label; L makes it an LTR label.
	if ((firstMask & ~L_R_AL_MASK) != 0) {
		info.isOkBiDi = false;
	}
	// Directionality of the last non-NSM character.
	//
	// `limit` is a BACKWARD cursor and it SHRINKS: the trailing NSMs walked over
	// here are thereby excluded from the "intervening characters" loop below. ICU
	// reuses its `labelLength` parameter for this, so the behaviour - not just the
	// value - depends on the two loops sharing one cursor.
	uint32_t lastMask;
	int32_t limit = labelLength;
	for (;;) {
		if (i >= limit) {
			lastMask = firstMask;
			break;
		}
		c = prevUnsafe(label, limit);
		BidiClass dir = bidiClass(c);
		if (dir != BidiClass::NonSpacingMark) {
			lastMask = bidiMask(dir);
			break;
		}
	}
	// 3. In an RTL label the last character must be R, AL, EN or AN, followed by
	// zero or more NSM.
	// 6. In an LTR label the last character must be L or EN, followed by zero or
	// more NSM.
	if ((firstMask & L_MASK) != 0 ? (lastMask & ~L_EN_MASK) != 0
								  : (lastMask & ~R_AL_EN_AN_MASK) != 0) {
		info.isOkBiDi = false;
	}
	// Add the directionalities of the intervening characters, up to the shrunk
	// backward cursor.
	uint32_t mask = firstMask | lastMask;
	while (i < limit) {
		c = nextUnsafe(label, i);
		mask |= bidiMask(bidiClass(c));
	}
	if (firstMask & L_MASK) {
		// 5. In an LTR label only L, EN, ES, CS, ET, ON, BN and NSM are allowed.
		if ((mask & ~L_EN_ES_CS_ET_ON_BN_NSM_MASK) != 0) {
			info.isOkBiDi = false;
		}
	} else {
		// 2. In an RTL label only R, AL, AN, EN, ES, CS, ET, ON, BN and NSM are
		// allowed.
		if ((mask & ~R_AL_AN_EN_ES_CS_ET_ON_BN_NSM_MASK) != 0) {
			info.isOkBiDi = false;
		}
		// 4. In an RTL label, EN and AN may not both be present.
		if ((mask & EN_AN_MASK) == EN_AN_MASK) {
			info.isOkBiDi = false;
		}
	}
	// An RTL label contains at least one R, AL or AN character; a BiDi domain name
	// contains at least one RTL label, and the six-part rule applies to its labels.
	if ((mask & R_AL_AN_MASK) != 0) {
		info.isBiDi = true;
	}
}

// The ASCII prefix of a BiDi domain name, which is all-LTR. Only the parts of the
// Bidi Rule that can apply to ASCII are checked: first character L, last character
// L or EN, and no B/S/WS in between. Called on the MAPPED prefix, so there are no
// uppercase letters left (ICU has a second, uppercase-tolerant copy for its UTF-8
// path; that path does not exist here).
static bool isAsciiOkBiDi(const char16_t *s, int32_t length) {
	int32_t labelStart = 0;
	for (int32_t i = 0; i < length; ++i) {
		char16_t c = s[i];
		if (c == 0x2E) { // dot
			if (i > labelStart) {
				c = s[i - 1];
				if (!(0x61 <= c && c <= 0x7A) && !(0x30 <= c && c <= 0x39)) {
					return false; // last character of the label is not L or EN
				}
			}
			labelStart = i + 1;
		} else if (i == labelStart) {
			if (!(0x61 <= c && c <= 0x7A)) {
				return false; // first character of the label is not L
			}
		} else {
			if (c <= 0x20 && (c >= 0x1C || (9 <= c && c <= 0xD))) {
				return false; // intermediate character is B, S or WS
			}
		}
	}
	return true;
}

// --- CONTEXTJ (RFC 5892 appendix A.1, A.2) -----------------------------------

static bool isLabelOkContextJ(const char16_t *label, int32_t labelLength) {
	// [IDNA2008-Tables] 200C..200D ; CONTEXTJ # ZWNJ..ZWJ
	for (int32_t i = 0; i < labelLength; ++i) {
		if (label[i] == 0x200C) {
			// A.1 ZERO WIDTH NON-JOINER:
			//  False;
			//  If Canonical_Combining_Class(Before(cp)) .eq. Virama Then True;
			//  If RegExpMatch((Joining_Type:{L,D})(Joining_Type:T)*‌
			//     (Joining_Type:T)*(Joining_Type:{R,D})) Then True;
			if (i == 0) {
				return false;
			}
			int32_t j = i;
			char32_t c = prevUnsafe(label, j);
			if (getCombiningClass(c) == 9) {
				continue;
			}
			// precontext: (Joining_Type:{L,D})(Joining_Type:T)*
			for (;;) {
				JoiningType type = joiningType(c);
				if (type == JoiningType::Transparent) {
					if (j == 0) {
						return false;
					}
					c = prevUnsafe(label, j);
				} else if (type == JoiningType::LeftJoining || type == JoiningType::DualJoining) {
					break; // precontext fulfilled
				} else {
					return false;
				}
			}
			// postcontext: (Joining_Type:T)*(Joining_Type:{R,D})
			for (j = i + 1;;) {
				if (j == labelLength) {
					return false;
				}
				c = nextUnsafe(label, j);
				JoiningType type = joiningType(c);
				if (type == JoiningType::Transparent) {
					// skip
				} else if (type == JoiningType::RightJoining || type == JoiningType::DualJoining) {
					break; // postcontext fulfilled
				} else {
					return false;
				}
			}
		} else if (label[i] == 0x200D) {
			// A.2 ZERO WIDTH JOINER:
			//  False;
			//  If Canonical_Combining_Class(Before(cp)) .eq. Virama Then True;
			if (i == 0) {
				return false;
			}
			int32_t j = i;
			char32_t c = prevUnsafe(label, j);
			if (getCombiningClass(c) != 9) {
				return false;
			}
		}
	}
	return true;
}

// --- CONTEXTO (RFC 5892 appendix A.3 - A.9) ----------------------------------

static void checkLabelContextO(const char16_t *label, int32_t labelLength, ProcessState &info) {
	int32_t labelEnd = labelLength - 1; // inclusive
	int32_t arabicDigits = 0; // -1 for 066x, +1 for 06Fx
	for (int32_t i = 0; i <= labelEnd; ++i) {
		char32_t c = label[i];
		if (c < 0xB7) {
			// ASCII fast path
		} else if (c <= 0x6F9) {
			if (c == 0xB7) {
				// A.3 MIDDLE DOT: allowed only between two U+006C.
				if (!(0 < i && label[i - 1] == 0x6C && i < labelEnd && label[i + 1] == 0x6C)) {
					info.labelErrors |= ErrContextOPunctuation;
				}
			} else if (c == 0x375) {
				// A.4 GREEK LOWER NUMERAL SIGN: the following script must be Greek.
				ScriptCode sc = ScriptCode::Other;
				if (i < labelEnd) {
					int32_t j = i + 1;
					uint8_t offset;
					c = unicode::utf16Decode32(label + j, size_t(labelLength - j), offset);
					sc = script(c);
				}
				if (sc != ScriptCode::Greek) {
					info.labelErrors |= ErrContextOPunctuation;
				}
			} else if (c == 0x5F3 || c == 0x5F4) {
				// A.5 HEBREW PUNCTUATION GERESH / A.6 GERSHAYIM: the preceding
				// script must be Hebrew.
				ScriptCode sc = ScriptCode::Other;
				if (0 < i) {
					int32_t j = i;
					c = prevSafe(label, 0, j);
					sc = script(c);
				}
				if (sc != ScriptCode::Hebrew) {
					info.labelErrors |= ErrContextOPunctuation;
				}
			} else if (0x660 <= c /* && c <= 0x6f9 */) {
				// A.8 ARABIC-INDIC DIGITS / A.9 EXTENDED ARABIC-INDIC DIGITS: the
				// two sets may not be mixed within one label.
				if (c <= 0x669) {
					if (arabicDigits > 0) {
						info.labelErrors |= ErrContextODigits;
					}
					arabicDigits = -1;
				} else if (0x6F0 <= c) {
					if (arabicDigits < 0) {
						info.labelErrors |= ErrContextODigits;
					}
					arabicDigits = 1;
				}
			}
		} else if (c == 0x30FB) {
			// A.7 KATAKANA MIDDLE DOT: the label must contain a Hiragana, Katakana
			// or Han character.
			for (int32_t j = 0;;) {
				if (j > labelEnd) {
					info.labelErrors |= ErrContextOPunctuation;
					break;
				}
				uint8_t offset;
				c = unicode::utf16Decode32(label + j, size_t(labelLength - j), offset);
				j += offset ? offset : 1;
				ScriptCode sc = script(c);
				if (sc == ScriptCode::Hiragana || sc == ScriptCode::Katakana
						|| sc == ScriptCode::Han) {
					break;
				}
			}
		}
	}
}

// --- the processing pipeline -------------------------------------------------

// Replaces the label in dest, if it was not modified in place.
// When labelString is &dest the label was modified in place and labelLength is
// already the new length; otherwise the label is copied over.
static bool replaceLabel(Utf16Buffer &dest, int32_t destLabelStart, int32_t destLabelLength,
		const Utf16Buffer &label, int32_t labelLength, int32_t &result) {
	if (&label != &dest) {
		if (!dest.replace(destLabelStart, destLabelLength, label.data(), labelLength)) {
			return false;
		}
	}
	result = labelLength;
	return true;
}

// Maps the deviation characters (sharp s, final sigma, ZWNJ, ZWJ) in transitional
// processing, then re-normalizes because the mapping can leave the label un-NFC.
// Returns the new total length of `dest`.
//
// ICU grows the buffer from inside the loop, which is the one place in the
// algorithm where the read and write cursors diverge and a reallocation would
// invalidate a live pointer. Here the extra room - one unit per sharp s, and
// nothing else grows - is reserved up front, so the loop cannot reallocate.
static bool mapDevChars(Utf16Buffer &dest, int32_t labelStart, int32_t mappingStart,
		int32_t &result) {
	int32_t length = dest.size();

	int32_t sharpS = 0;
	for (int32_t i = mappingStart; i < length; ++i) {
		if (dest[i] == 0xDF) {
			++sharpS;
		}
	}
	if (!dest.reserve(length + sharpS)) {
		return false;
	}

	auto s = dest.data();
	bool didMapDevChars = false;
	int32_t readIndex = mappingStart, writeIndex = mappingStart;
	do {
		char16_t c = s[readIndex++];
		switch (c) {
		case 0xDF:
			// Map sharp s to "ss".
			didMapDevChars = true;
			s[writeIndex++] = 0x73; // replace sharp s with the first s
			if (writeIndex == readIndex) {
				// Make room for the second s without overwriting unread input.
				::__sprt_memmove(s + writeIndex + 1, s + writeIndex,
						size_t(length - writeIndex) * sizeof(char16_t));
				++readIndex;
			}
			s[writeIndex++] = 0x73;
			++length;
			break;
		case 0x3C2: // final sigma -> nonfinal sigma
			didMapDevChars = true;
			s[writeIndex++] = 0x3C3;
			break;
		case 0x200C: // ZWNJ: ignore/remove
		case 0x200D: // ZWJ: ignore/remove
			didMapDevChars = true;
			--length;
			break;
		default:
			// Only really needed once writeIndex has diverged from readIndex.
			s[writeIndex++] = c;
			break;
		}
	} while (writeIndex < length);
	dest.setSize(length);

	if (didMapDevChars) {
		// Mapping deviation characters can leave the string un-NFC. Re-running the
		// UTS-46 normalizer (rather than a plain NFC one) avoids a second data set.
		Utf16Buffer normalized;
		if (!normalizeUts46(dest.sub(labelStart, dest.size() - labelStart), normalized)) {
			return false;
		}
		if (!dest.replace(labelStart, dest.size() - labelStart, normalized.data(),
					normalized.size())) {
			return false;
		}
		result = dest.size();
		return true;
	}
	result = length;
	return true;
}

static bool markBadAceLabel(Utf16Buffer &dest, int32_t labelStart, int32_t labelLength,
		bool toAscii, ProcessState &info, int32_t &result);

// Processes one label in place. Returns the new label length via `result`.
static bool processLabel(Utf16Buffer &dest, int32_t labelStart, int32_t labelLength, bool toAscii,
		ProcessState &info, int32_t &result) {
	Utf16Buffer fromPunycode;
	Utf16Buffer *labelString;
	const char16_t *label = dest.data() + labelStart;
	int32_t destLabelStart = labelStart;
	int32_t destLabelLength = labelLength;
	bool wasPunycode;

	if (labelLength >= 4 && label[0] == 0x78 && label[1] == 0x6E && label[2] == 0x2D
			&& label[3] == 0x2D) {
		// The label starts with "xn--": try to un-Punycode it.
		// In IDNA2008 labels such as "xn--" (decodes to nothing) and "xn--ASCII-"
		// (decodes to just "ASCII") fail the round-trip check between the ToUnicode
		// input and the ToASCII output - they are alternate encodings of plain ASCII
		// labels. "xn---" is left to fail in the decoder, which logically comes
		// before the round-trip check.
		if (labelLength == 4 || (labelLength > 5 && label[labelLength - 1] == u'-')) {
			info.labelErrors |= ErrInvalidAceLabel;
			return markBadAceLabel(dest, labelStart, labelLength, toAscii, info, result);
		}
		wasPunycode = true;
		if (!punycodeDecodeLabel(label + 4, labelLength - 4, fromPunycode)) {
			info.labelErrors |= ErrPunycode;
			return markBadAceLabel(dest, labelStart, labelLength, toAscii, info, result);
		}
		// Check for NFC, and for characters the normalizer does not consider valid
		// or deviation characters. Anything wrong makes the string change.
		// The normalizer passes non-LDH ASCII and deviation characters through;
		// deviation characters are fine in Punycode even in transitional processing.
		// Non-LDH ASCII under UseStd3Rules is caught further below.
		if (!isNormalizedUts46(fromPunycode.view())) {
			info.labelErrors |= ErrInvalidAceLabel;
			return markBadAceLabel(dest, labelStart, labelLength, toAscii, info, result);
		}
		labelString = &fromPunycode;
		label = fromPunycode.data();
		labelStart = 0;
		labelLength = fromPunycode.size();
	} else {
		wasPunycode = false;
		labelString = &dest;
	}

	// Validity checks
	if (labelLength == 0) {
		info.labelErrors |= ErrEmptyLabel;
		return replaceLabel(dest, destLabelStart, destLabelLength, *labelString, labelLength,
				result);
	}
	if (labelLength >= 4 && label[2] == 0x2D && label[3] == 0x2D) {
		info.labelErrors |= ErrHyphen34; // label starts with "??--"
	}
	if (label[0] == 0x2D) {
		info.labelErrors |= ErrLeadingHyphen;
	}
	if (label[labelLength - 1] == 0x2D) {
		info.labelErrors |= ErrTrailingHyphen;
	}

	// A non-Punycode label is the result of mapping, normalization and label
	// segmentation; a Punycode label was mapped again above and validated. What is
	// left is the STD3 restriction to LDH (when enabled), U+FFFD - which marks a
	// disallowed character in a non-Punycode label, or a literal U+FFFD in a
	// Punycode one - and dots, which a single-label call can receive as input.
	// Casting away const is fine: we own the buffer.
	auto s = const_cast<char16_t *>(label);
	auto limit = label + labelLength;
	char16_t oredChars = 0;
	bool disallowNonLDHDot = info.useStd3Rules();
	do {
		char16_t c = *s;
		if (c <= 0x7F) {
			if (c == 0x2E) {
				info.labelErrors |= ErrLabelHasDot;
				*s = 0xFFFD;
			} else if (disallowNonLDHDot && s_asciiData[c] < 0) {
				info.labelErrors |= ErrDisallowed;
				*s = 0xFFFD;
			}
		} else {
			oredChars = char16_t(oredChars | c);
			if (disallowNonLDHDot && isNonAsciiDisallowedStd3Valid(c)) {
				info.labelErrors |= ErrDisallowed;
				*s = 0xFFFD;
			} else if (c == 0xFFFD) {
				info.labelErrors |= ErrDisallowed;
			}
		}
		++s;
	} while (s < limit);

	// The leading-combining-mark check comes after the others so that the U+FFFD
	// planted above is not itself reported as ErrDisallowed here.
	{
		int32_t cpLength = 0;
		// "Unsafe" is fine: unpaired surrogates were already mapped to U+FFFD.
		char32_t c = nextUnsafe(label, cpLength);
		if ((generalCategoryMask(c) & MarkCategoryMask) != 0) {
			info.labelErrors |= ErrLeadingCombiningMark;
			char16_t replacement = 0xFFFD;
			if (!labelString->replace(labelStart, cpLength, &replacement, 1)) {
				return false;
			}
			label = labelString->data() + labelStart;
			labelLength += 1 - cpLength;
			if (labelString == &dest) {
				destLabelLength = labelLength;
			}
		}
	}

	if ((info.labelErrors & severeErrors) == 0) {
		// Contextual checks run only when no severe error has planted a U+FFFD,
		// because U+FFFD would make them fail for the wrong reason.
		if ((info.options & OptCheckBidi) != 0 && (!info.isBiDi || info.isOkBiDi)) {
			checkLabelBiDi(label, labelLength, info);
		}
		if ((info.options & OptCheckContextJ) != 0 && (oredChars & 0x200C) == 0x200C
				&& !isLabelOkContextJ(label, labelLength)) {
			info.labelErrors |= ErrContextJ;
		}
		if ((info.options & OptCheckContextO) != 0 && oredChars >= 0xB7) {
			checkLabelContextO(label, labelLength, info);
		}
		if (toAscii) {
			if (wasPunycode) {
				// A Punycode label without severe errors is left as it was.
				if (destLabelLength > 63) {
					info.labelErrors |= ErrLabelTooLong;
				}
				result = destLabelLength;
				return true;
			} else if (oredChars >= 0x80) {
				// Contains non-ASCII: re-encode as Punycode.
				Utf16Buffer punycode;
				if (!punycodeEncodeLabel(label, labelLength, punycode)) {
					// The engine codec only fails on malformed input or allocation;
					// either way this label cannot be represented.
					info.labelErrors |= ErrPunycode;
					result = destLabelLength;
					return true;
				}
				if (punycode.size() > 63) {
					info.labelErrors |= ErrLabelTooLong;
				}
				return replaceLabel(dest, destLabelStart, destLabelLength, punycode,
						punycode.size(), result);
			} else {
				// all-ASCII label
				if (labelLength > 63) {
					info.labelErrors |= ErrLabelTooLong;
				}
			}
		}
	} else {
		// A Punycode label with severe errors is kept, but made to not look valid.
		if (wasPunycode) {
			info.labelErrors |= ErrInvalidAceLabel;
			return markBadAceLabel(dest, destLabelStart, destLabelLength, toAscii, info, result);
		}
	}
	return replaceLabel(dest, destLabelStart, destLabelLength, *labelString, labelLength, result);
}

// Makes sure an ACE label cannot look valid: append U+FFFD if it is all-LDH, and
// under UseStd3Rules replace disallowed ASCII with U+FFFD.
static bool markBadAceLabel(Utf16Buffer &dest, int32_t labelStart, int32_t labelLength,
		bool toAscii, ProcessState &info, int32_t &result) {
	bool disallowNonLDHDot = info.useStd3Rules();
	bool isAscii = true;
	bool onlyLDH = true;
	auto label = dest.data() + labelStart;
	auto limit = label + labelLength;
	// Start after the initial "xn--". Casting away const is fine: we own the buffer.
	for (auto s = const_cast<char16_t *>(label + 4); s < limit; ++s) {
		char16_t c = *s;
		if (c <= 0x7F) {
			if (c == 0x2E) {
				info.labelErrors |= ErrLabelHasDot;
				*s = 0xFFFD;
				isAscii = onlyLDH = false;
			} else if (s_asciiData[c] < 0) {
				onlyLDH = false;
				if (disallowNonLDHDot) {
					*s = 0xFFFD;
					isAscii = false;
				}
			}
		} else {
			isAscii = onlyLDH = false;
		}
	}
	if (onlyLDH) {
		char16_t replacement = 0xFFFD;
		if (!dest.insert(labelStart + labelLength, &replacement, 1)) {
			return false;
		}
		++labelLength;
	} else {
		if (toAscii && isAscii && labelLength > 63) {
			info.labelErrors |= ErrLabelTooLong;
		}
	}
	result = labelLength;
	return true;
}

static bool processUnicode(WideStringView src, int32_t labelStart, int32_t mappingStart,
		bool isLabel, bool toAscii, Utf16Buffer &dest, ProcessState &info) {
	if (mappingStart == 0) {
		if (!normalizeUts46(src, dest)) {
			return false;
		}
	} else {
		if (!normalizeSecondAndAppendUts46(dest,
					WideStringView(src.data() + mappingStart, src.size() - size_t(mappingStart)))) {
			return false;
		}
	}

	bool doMapDevChars = toAscii ? (info.options & OptNonTransitionalToAscii) == 0
								 : (info.options & OptNonTransitionalToUnicode) == 0;
	int32_t destLength = dest.size();
	int32_t labelLimit = labelStart;
	while (labelLimit < destLength) {
		char16_t c = dest[labelLimit];
		if (c == 0x2E && !isLabel) {
			int32_t labelLength = labelLimit - labelStart;
			int32_t newLength = 0;
			if (!processLabel(dest, labelStart, labelLength, toAscii, info, newLength)) {
				return false;
			}
			info.errors |= info.labelErrors;
			info.labelErrors = 0;
			destLength += newLength - labelLength;
			labelLimit = labelStart += newLength + 1;
			continue;
		} else if (c < 0xDF) {
			// pass
		} else if (c <= 0x200D && (c == 0xDF || c == 0x3C2 || c >= 0x200C)) {
			info.isTransDiff = true;
			if (doMapDevChars) {
				if (!mapDevChars(dest, labelStart, labelLimit, destLength)) {
					return false;
				}
				// All deviation characters are mapped; no need to look again.
				doMapDevChars = false;
				// Do not advance labelLimit: c may have been removed.
				continue;
			}
		} else if (unicode::isUtf16Surrogate(c)) {
			bool unpaired = unicode::isUtf16HighSurrogate(c) ? (labelLimit + 1) == destLength
							|| !unicode::isUtf16LowSurrogate(dest[labelLimit + 1])
															 : labelLimit == labelStart
							|| !unicode::isUtf16HighSurrogate(dest[labelLimit - 1]);
			if (unpaired) {
				// Map an unpaired surrogate to U+FFFD before normalization, so that
				// characters removed there cannot turn two unpaired halves into a pair.
				info.labelErrors |= ErrDisallowed;
				dest.setAt(labelLimit, 0xFFFD);
			}
		}
		++labelLimit;
	}
	// An empty label is permitted at the end (0 < labelStart == labelLimit ==
	// destLength is a trailing dot) but not elsewhere, and not as the whole name.
	// processLabel() reports ErrEmptyLabel when labelLength == 0.
	if (0 == labelStart || labelStart < labelLimit) {
		int32_t ignored = 0;
		if (!processLabel(dest, labelStart, labelLimit - labelStart, toAscii, info, ignored)) {
			return false;
		}
		info.errors |= info.labelErrors;
	}
	return true;
}

// The entry point: map, normalize, split into labels, validate.
static bool process(WideStringView src, bool isLabel, bool toAscii, Utf16Buffer &dest,
		ProcessState &info) {
	dest.clear();
	int32_t srcLength = int32_t(src.size());
	if (srcLength == 0) {
		info.errors |= ErrEmptyLabel;
		return true;
	}
	if (!dest.reserve(srcLength)) {
		return false;
	}

	// ASCII fast path: a name that is already lowercase LDH never reaches the
	// normalizer, which is what makes "example.com" essentially free.
	auto srcArray = src.data();
	auto destArray = dest.data();
	bool disallowNonLDHDot = info.useStd3Rules();
	int32_t labelStart = 0;
	int32_t i;
	for (i = 0;; ++i) {
		if (i == srcLength) {
			if (toAscii) {
				if ((i - labelStart) > 63) {
					info.labelErrors |= ErrLabelTooLong;
				}
				// A trailing dot means labelStart == i.
				if (!isLabel && i >= 254 && (i > 254 || labelStart < i)) {
					info.errors |= ErrDomainNameTooLong;
				}
			}
			info.errors |= info.labelErrors;
			dest.setSize(i);
			return true;
		}
		char16_t c = srcArray[i];
		if (c > 0x7F) {
			break;
		}
		int cData = s_asciiData[c];
		if (cData > 0) {
			destArray[i] = char16_t(c + 0x20); // lowercase an uppercase ASCII letter
		} else if (cData < 0 && disallowNonLDHDot) {
			break; // replacing with U+FFFD can be complicated for toASCII
		} else {
			destArray[i] = c;
			if (c == 0x2D) { // hyphen
				if (i == (labelStart + 3) && srcArray[i - 1] == 0x2D) {
					// "??--..." is Punycode, or forbidden.
					++i; // '-' has already been copied to dest
					break;
				}
				if (i == labelStart) {
					info.labelErrors |= ErrLeadingHyphen;
				}
				if ((i + 1) == srcLength || srcArray[i + 1] == 0x2E) {
					info.labelErrors |= ErrTrailingHyphen;
				}
			} else if (c == 0x2E) { // dot
				if (isLabel) {
					// replacing with U+FFFD can be complicated for toASCII
					++i; // '.' has already been copied to dest
					break;
				}
				if (i == labelStart) {
					info.labelErrors |= ErrEmptyLabel;
				}
				if (toAscii && (i - labelStart) > 63) {
					info.labelErrors |= ErrLabelTooLong;
				}
				info.errors |= info.labelErrors;
				info.labelErrors = 0;
				labelStart = i + 1;
			}
		}
	}
	info.errors |= info.labelErrors;
	dest.setSize(i);

	if (!processUnicode(src, labelStart, i, isLabel, toAscii, dest, info)) {
		return false;
	}
	if (info.isBiDi && (info.errors & severeErrors) == 0
			&& (!info.isOkBiDi || (labelStart > 0 && !isAsciiOkBiDi(dest.data(), labelStart)))) {
		info.errors |= ErrBidi;
	}
	return true;
}

// --- the four operations -----------------------------------------------------

static bool labelToAscii(WideStringView label, Utf16Buffer &dest, ProcessState &info) {
	return process(label, true, true, dest, info);
}

static bool labelToUnicode(WideStringView label, Utf16Buffer &dest, ProcessState &info) {
	return process(label, true, false, dest, info);
}

static bool nameToAscii(WideStringView name, Utf16Buffer &dest, ProcessState &info) {
	if (!process(name, false, true, dest, info)) {
		return false;
	}
	if (dest.size() >= 254 && (info.errors & ErrDomainNameTooLong) == 0 && isAsciiString(dest)
			&& (dest.size() > 254 || dest[253] != 0x2E)) {
		info.errors |= ErrDomainNameTooLong;
	}
	return true;
}

static bool nameToUnicode(WideStringView name, Utf16Buffer &dest, ProcessState &info) {
	return process(name, false, false, dest, info);
}

} // namespace sprt::idn::detail
