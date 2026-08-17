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

// The four Unicode character properties UTS-46 consults, read out of the tries in
// data/. Ported from libuidna src/u_char.cc and src/u_bidi.cc (ICU uchar.cpp /
// ubidi_props.cpp; © Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// ICU exposes dozens of properties through these tables; UTS-46 uses four:
//
//   generalCategoryMask()  is this a combining mark (leading-combining-mark rule)
//   bidiClass()            the Bidi Rule, RFC 5893
//   joiningType()          CheckContextJ, RFC 5892 appendix A.1 and A.2
//   script()               CheckContextO, RFC 5892 appendix A.3, A.6 and A.7
//
// Everything else - the property-vector columns beyond the first, the 200-value
// UScriptCode enum, whitespace/case/numeric queries, the joining-group and
// mirroring tables - is not carried.

namespace sprt::idn::detail {

// --- the tries ---------------------------------------------------------------

static constexpr Utrie2 s_charTypeTrie{
	s_charTypeTrieIndex,
	s_charTypeTrieIndexLength,
	s_charTypeTrieDataLength,
	s_charTypeTrieHighStart,
	s_charTypeTrieHighValueIndex,
};

static constexpr Utrie2 s_bidiTrie{
	s_bidiTrieIndex,
	s_bidiTrieIndexLength,
	s_bidiTrieDataLength,
	s_bidiTrieHighStart,
	s_bidiTrieHighValueIndex,
};

static constexpr Utrie2 s_scriptTrie{
	s_scriptTrieIndex,
	s_scriptTrieIndexLength,
	s_scriptTrieDataLength,
	s_scriptTrieHighStart,
	s_scriptTrieHighValueIndex,
};

// --- General_Category --------------------------------------------------------

// ICU packs the category into the low 5 bits of the property word (GET_CATEGORY).
// The three mark categories keep their ICU numbering because the Bidi/UTS-46 code
// tests them as a bit mask.
enum class CharCategory : uint8_t {
	NonSpacingMark = 6, // Mn, U_NON_SPACING_MARK
	EnclosingMark = 7, // Me, U_ENCLOSING_MARK
	CombiningSpacingMark = 8, // Mc, U_COMBINING_SPACING_MARK
};

// U_GC_M_MASK: Mn | Me | Mc
static constexpr uint32_t MarkCategoryMask = (uint32_t(1) << uint32_t(CharCategory::NonSpacingMark))
		| (uint32_t(1) << uint32_t(CharCategory::EnclosingMark))
		| (uint32_t(1) << uint32_t(CharCategory::CombiningSpacingMark));

// U_GET_GC_MASK: 1 << general category. Kept in mask form so the UTS-46 test
// transcribes as `(generalCategoryMask(c) & MarkCategoryMask) != 0`, exactly as
// the ICU original reads.
static inline uint32_t generalCategoryMask(char32_t c) {
	return uint32_t(1) << (s_charTypeTrie.get(c) & 0x1F);
}

// --- Bidi_Class and Joining_Type ---------------------------------------------

// ICU numbering (UCharDirection). The values are used as bit positions in a
// uint32_t mask by the Bidi Rule, so they must stay exactly as ICU assigns them.
// Only the classes the rule names are given a name here; the rest keep working
// because the rule only ever tests membership of a mask.
enum class BidiClass : uint8_t {
	LeftToRight = 0, // L
	RightToLeft = 1, // R
	EuropeanNumber = 2, // EN
	EuropeanNumberSeparator = 3, // ES
	EuropeanNumberTerminator = 4, // ET
	ArabicNumber = 5, // AN
	CommonNumberSeparator = 6, // CS
	OtherNeutral = 10, // ON
	RightToLeftArabic = 13, // AL
	NonSpacingMark = 17, // NSM
	BoundaryNeutral = 18, // BN
};

enum class JoiningType : uint8_t {
	NonJoining = 0, // U
	JoinCausing = 1, // C
	DualJoining = 2, // D
	LeftJoining = 3, // L
	RightJoining = 4, // R
	Transparent = 5, // T
};

// ubidi_props.h bit layout of the property word.
static constexpr uint16_t BidiClassMask = 0x001F; // UBIDI_CLASS_MASK
static constexpr uint16_t BidiJoiningTypeMask = 0x00E0; // UBIDI_JT_MASK
static constexpr int BidiJoiningTypeShift = 5; // UBIDI_JT_SHIFT

static inline BidiClass bidiClass(char32_t c) {
	return BidiClass(s_bidiTrie.get(c) & BidiClassMask);
}

static inline JoiningType joiningType(char32_t c) {
	return JoiningType((s_bidiTrie.get(c) & BidiJoiningTypeMask) >> BidiJoiningTypeShift);
}

static constexpr uint32_t bidiMask(BidiClass cls) { return uint32_t(1) << uint32_t(cls); }

// --- Script ------------------------------------------------------------------

// UTS-46 compares the script of a code point against five identities and nothing
// else (u_uts46.cc:995, :1015, :1059), so the 200-value UScriptCode enum collapses
// to this. The numeric values are ICU's, because they come straight out of the
// table; Other stands for "some script we do not care about".
//
// Keeping the accessor's signature narrow is what makes the planned table shrink
// (see data/README.adoc) a pure data swap.
enum class ScriptCode : uint16_t {
	Common = 0, // USCRIPT_COMMON
	Inherited = 1, // USCRIPT_INHERITED
	Greek = 14, // USCRIPT_GREEK
	Han = 17, // USCRIPT_HAN
	Hebrew = 19, // USCRIPT_HEBREW
	Hiragana = 20, // USCRIPT_HIRAGANA
	Katakana = 22, // USCRIPT_KATAKANA
	Other = 0xFFFF,
};

// uprops.h. Bits 11..0 of property vector column 0: bits 9..0 are the UScriptCode
// or an index into Script_Extensions, and bits 11..10 say which. ICU 66..77 split
// this across two non-adjacent fields that had to be shifted together; ICU 78
// re-laid the column out so it is one contiguous field again. Reading the old
// layout out of the new data returns a valid script for the wrong code point.
static constexpr uint32_t ScriptXMask = 0x0000'0FFF; // UPROPS_SCRIPT_X_MASK
static constexpr uint32_t MaxScript = 0x3FF; // UPROPS_MAX_SCRIPT

// Values at or above these involve Script_Extensions rather than a plain code.
static constexpr uint32_t ScriptXWithCommon = 0x400; // UPROPS_SCRIPT_X_WITH_COMMON
static constexpr uint32_t ScriptXWithInherited = 0x800; // UPROPS_SCRIPT_X_WITH_INHERITED
static constexpr uint32_t ScriptXWithOther = 0xC00; // UPROPS_SCRIPT_X_WITH_OTHER

static inline ScriptCode narrowScript(uint32_t code) {
	switch (code) {
	case uint32_t(ScriptCode::Common): return ScriptCode::Common;
	case uint32_t(ScriptCode::Inherited): return ScriptCode::Inherited;
	case uint32_t(ScriptCode::Greek): return ScriptCode::Greek;
	case uint32_t(ScriptCode::Han): return ScriptCode::Han;
	case uint32_t(ScriptCode::Hebrew): return ScriptCode::Hebrew;
	case uint32_t(ScriptCode::Hiragana): return ScriptCode::Hiragana;
	case uint32_t(ScriptCode::Katakana): return ScriptCode::Katakana;
	default: return ScriptCode::Other;
	}
}

// uscript_getScript(). Out-of-range code points report Other; ICU reports an error
// there, but every caller in UTS-46 treats "not the script I asked about" the same
// way, and the label has already been validated as scalar values by then.
static inline ScriptCode script(char32_t c) {
	if (c > 0x10'FFFF) {
		return ScriptCode::Other;
	}
	// u_getUnicodeProperties(c, 0). The trie value is the row's OFFSET into the flat
	// vector array, not its row number - so column N is `vecIndex + N`, never
	// `vecIndex * columns + N`.
	static_assert(s_scriptVectorColumns > 0, "column 0 must exist");
	uint32_t scriptX = s_scriptVectors[s_scriptTrie.get(c) + 0] & ScriptXMask;
	uint32_t codeOrIndex = scriptX & MaxScript;
	if (scriptX < ScriptXWithCommon) {
		return narrowScript(codeOrIndex);
	} else if (scriptX < ScriptXWithInherited) {
		return ScriptCode::Common;
	} else if (scriptX < ScriptXWithOther) {
		return ScriptCode::Inherited;
	} else {
		return narrowScript(s_scriptExtensions[codeOrIndex]);
	}
}

} // namespace sprt::idn::detail
