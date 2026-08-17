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

// Collation elements: the two encodings a weight lives in, and the arithmetic
// that converts between them. Ported from ICU collation.h and collation.cpp
// (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// Two representations run through the whole engine:
//
//   CE32 - 32 bits, what the trie stores. Either a "simple" CE (primary in the
//     high 16 bits, secondary and tertiary bytes below) or, when its low byte is
//     >= 0xC0, a *special* CE32 whose low nibble is a tag saying what the other
//     24 bits mean: an index into an expansion table, a contraction, a prefix, a
//     Hangul syllable, a digit, a code-point-ordered range, and so on.
//
//   CE - 64 bits: primary in the high 32, secondary in bits 31..16, case and
//     tertiary in 15..0. This is what the comparison and the sort key read.
//
// The bit layouts below are transcribed from ICU literally, including the field
// widths in the comments. They are not derivable from anything: the data was
// serialized against them, and a field read one bit wide gives a valid weight
// for the wrong character - no crash, just a handful of words sorted wrong.

namespace sprt::unicode::detail {

// --- sort key bytes and reserved weights ---------------------------------------

enum : uint8_t {
	TerminatorByte = 0,
	LevelSeparatorByte = 1,
	// Merge-sort-key separator; also the primary of U+FFFE. Must not be used as
	// the primary compression low terminator.
	MergeSeparatorByte = 2,
	// Reserved as the second primary byte when the lead byte is compressible.
	PrimaryCompressionLowByte = 3,
	PrimaryCompressionHighByte = 0xFF,
	// Default lead byte of a secondary or tertiary weight.
	CommonByte = 5,
	UnassignedImplicitByte = 0xFE, // compressible
	TrailWeightByte = 0xFF, // not compressible
};

enum : uint32_t {
	BeforeWeight16 = 0x0100,
	MergeSeparatorPrimary = 0x0200'0000, // U+FFFE
	MergeSeparatorCE32 = 0x0200'0505, // U+FFFE

	CommonWeight16 = 0x0500,
	CommonSecondaryCE = 0x0500'0000,
	CommonTertiaryCE = 0x0500,
	CommonSecAndTerCE = 0x0500'0500,

	SecondaryMask = 0xFFFF'0000,
	CaseMask = 0xC000,
	SecondaryAndCaseMask = SecondaryMask | CaseMask,
	OnlyTertiaryMask = 0x3F3F, // the 2*6 bits of the pure tertiary weight
	OnlySecTerMask = SecondaryMask | OnlyTertiaryMask,
	CaseAndTertiaryMask = CaseMask | OnlyTertiaryMask,
	QuaternaryMask = 0xC0,
	CaseAndQuaternaryMask = CaseMask | QuaternaryMask,

	// AlphabeticIndex overflow boundary; three bytes so it fits the root elements.
	FirstUnassignedPrimary = 0xFE04'0200,
	FirstTrailingPrimary = 0xFF02'0200, // [first trailing]
	MaxPrimary = 0xFFFF'0000, // U+FFFF
	MaxRegularCE32 = 0xFFFF'0505, // U+FFFF

	// U+FFFD, and ill-formed UTF-8, which behaves like it.
	FFFDPrimary = MaxPrimary - 0x2'0000,
	FFFDCE32 = MaxRegularCE32 - 0x2'0000,

	// A CE32 is special if its low byte is at least this; the impossible case bits
	// 11 are what mark it. This value on its own means "fall back to the base".
	SpecialCE32LowByte = 0xC0,
	FallbackCE32 = SpecialCE32LowByte,
	LongPrimaryCE32LowByte = 0xC1, // SpecialCE32LowByte | LongPrimaryTag

	UnassignedCE32 = 0xFFFF'FFFF, // compute an unassigned-implicit CE
	NoCE32 = 1,

	// End of input. Runtime only, never stored.
	NoCEPrimary = 1, // not a left-adjusted weight
	NoCEWeight16 = 0x0100, // the weight of LevelSeparatorByte
};

static constexpr int64_t NoCE = int64_t(0x1'0100'0100ULL);

// The result of a comparison, as ICU's UCollationResult numbers it.
enum : int32_t {
	CompareEqual = 0,
	CompareGreater = 1,
	CompareLess = -1,
};

// --- sort key levels -----------------------------------------------------------

enum : int32_t {
	NoLevel = 0,
	PrimaryLevel,
	SecondaryLevel,
	CaseLevel,
	TertiaryLevel,
	QuaternaryLevel,
	IdenticalLevel,
	ZeroLevel, // beyond sort key bytes
};

enum : uint32_t {
	NoLevelFlag = 1,
	PrimaryLevelFlag = 2,
	SecondaryLevelFlag = 4,
	CaseLevelFlag = 8,
	TertiaryLevelFlag = 0x10,
	QuaternaryLevelFlag = 0x20,
	IdenticalLevelFlag = 0x40,
	ZeroLevelFlag = 0x80,
};

// --- special-CE32 tags ---------------------------------------------------------
//
// Bits 3..0 of a special CE32. Bits 31..8 are the tag's own data; the layout of
// each is in the comment, because nothing else records it.

enum : int32_t {
	// Fall back to the base collator. Bits 31..8 unused.
	FallbackTag = 0,
	// Long-primary CE with CommonSecAndTerCE. Bits 31..8: three-byte primary.
	LongPrimaryTag = 1,
	// Long-secondary CE, zero primary. Bits 31..16 secondary, 15..8 tertiary.
	LongSecondaryTag = 2,
	ReservedTag3 = 3,
	// Latin mini expansion of two CEs [pp, 05, tt] [00, ss, 05].
	// Bits 31..24 primary pp, 23..16 tertiary tt, 15..8 secondary ss.
	LatinExpansionTag = 4,
	// One or more 32-bit CE32s. Bits 31..13 index, 12..8 length 1..31.
	Expansion32Tag = 5,
	// One or more 64-bit CEs. Bits 31..13 index, 12..8 length 1..31.
	ExpansionTag = 6,
	// Builder-only; never in runtime data. Present so the tag numbering matches.
	BuilderDataTag = 7,
	// Prefix trie. Bits 31..13 index into the contexts data.
	PrefixTag = 8,
	// Contraction data. Bits 31..13 index, 11..8 the CONTRACT_* flags below.
	ContractionTag = 9,
	// Decimal digit. Bits 31..13 index of the non-numeric CE32, 11..8 the value.
	DigitTag = 10,
	// U+0000, so NUL-termination handling leaves the fast path.
	U0000Tag = 11,
	// Hangul syllable. Bit 8: HangulNoSpecialJamo.
	HangulTag = 12,
	// Lead surrogate code unit. Bits 9..8 say what its supplementary block holds.
	LeadSurrogateTag = 13,
	// CEs with primaries in code point order. Bits 31..13 index of one data "CE",
	// whose own fields are: 63..32 three-byte primary, 31..8 base code point,
	// 7 isCompressible, 6..0 per-code-point increment.
	OffsetTag = 14,
	// Unassigned-implicit; all bits set (UnassignedCE32).
	ImplicitTag = 15,
};

enum : int32_t {
	// An expansion is bounded so that its length fits a few bits, and so that an
	// implementation can copy the CEs without growing a buffer.
	MaxExpansionLength = 31,
	MaxIndex = 0x7FFFF,
};

enum : uint32_t {
	// Set when the single character itself has no match; only possible with a
	// prefix. Discontiguous contraction matching then cannot start from an empty
	// suffix.
	ContractSingleCpNoMatch = 0x100,
	ContractNextCcc = 0x200, // every suffix starts with lccc != 0
	ContractTrailingCcc = 0x400, // some suffix ends with lccc != 0
	ContractHasStarter = 0x800, // ICU4X only

	HangulNoSpecialJamo = 0x100, // none of its jamo CE32s is special

	LeadAllUnassigned = 0,
	LeadAllFallback = 0x100,
	LeadMixed = 0x200,
	LeadTypeMask = 0x300,
};

// --- CE32 accessors ------------------------------------------------------------

static constexpr bool isSpecialCE32(uint32_t ce32) { return (ce32 & 0xFF) >= SpecialCE32LowByte; }

static constexpr int32_t tagFromCE32(uint32_t ce32) { return int32_t(ce32 & 0xF); }

static constexpr bool hasCE32Tag(uint32_t ce32, int32_t tag) {
	return isSpecialCE32(ce32) && tagFromCE32(ce32) == tag;
}

static constexpr bool isAssignedCE32(uint32_t ce32) {
	return ce32 != FallbackCE32 && ce32 != UnassignedCE32;
}

static constexpr bool isLongPrimaryCE32(uint32_t ce32) { return hasCE32Tag(ce32, LongPrimaryTag); }

static constexpr bool isSimpleOrLongCE32(uint32_t ce32) {
	return !isSpecialCE32(ce32) || tagFromCE32(ce32) == LongPrimaryTag
			|| tagFromCE32(ce32) == LongSecondaryTag;
}

// True when the CE32 yields its CEs without any further table lookup.
static constexpr bool isSelfContainedCE32(uint32_t ce32) {
	return !isSpecialCE32(ce32) || tagFromCE32(ce32) == LongPrimaryTag
			|| tagFromCE32(ce32) == LongSecondaryTag || tagFromCE32(ce32) == LatinExpansionTag;
}

static constexpr bool isPrefixCE32(uint32_t ce32) { return hasCE32Tag(ce32, PrefixTag); }

static constexpr bool isContractionCE32(uint32_t ce32) { return hasCE32Tag(ce32, ContractionTag); }

static constexpr bool ce32HasContext(uint32_t ce32) {
	return isSpecialCE32(ce32)
			&& (tagFromCE32(ce32) == PrefixTag || tagFromCE32(ce32) == ContractionTag);
}

static constexpr int32_t indexFromCE32(uint32_t ce32) { return int32_t(ce32 >> 13); }

static constexpr int32_t lengthFromCE32(uint32_t ce32) { return int32_t((ce32 >> 8) & 31); }

static constexpr char digitFromCE32(uint32_t ce32) { return char((ce32 >> 8) & 0xF); }

static constexpr uint32_t primaryFromLongPrimaryCE32(uint32_t ce32) { return ce32 & 0xFFFF'FF00; }

static constexpr uint32_t makeLongPrimaryCE32(uint32_t p) { return p | LongPrimaryCE32LowByte; }

static constexpr uint32_t makeLongSecondaryCE32(uint32_t lower32) {
	return lower32 | SpecialCE32LowByte | LongSecondaryTag;
}

static constexpr uint32_t makeCE32FromTagIndexAndLength(int32_t tag, int32_t index,
		int32_t length) {
	return uint32_t(index << 13) | uint32_t(length << 8) | SpecialCE32LowByte | uint32_t(tag);
}

static constexpr uint32_t makeCE32FromTagAndIndex(int32_t tag, int32_t index) {
	return uint32_t(index << 13) | SpecialCE32LowByte | uint32_t(tag);
}

// --- CE32 -> CE ----------------------------------------------------------------

static constexpr int64_t ceFromLongPrimaryCE32(uint32_t ce32) {
	return (int64_t(ce32 & 0xFFFF'FF00) << 32) | CommonSecAndTerCE;
}

static constexpr int64_t ceFromLongSecondaryCE32(uint32_t ce32) { return ce32 & 0xFFFF'FF00; }

// Requires a non-special CE32: ppppsstt -> pppp0000ss00tt00.
static constexpr int64_t ceFromSimpleCE32(uint32_t ce32) {
	return (int64_t(ce32 & 0xFFFF'0000) << 32) | int64_t((ce32 & 0xFF00) << 16)
			| int64_t((ce32 & 0xFF) << 8);
}

// Simple, long-primary or long-secondary.
static constexpr int64_t ceFromCE32(uint32_t ce32) {
	uint32_t tertiary = ce32 & 0xFF;
	if (tertiary < SpecialCE32LowByte) {
		return (int64_t(ce32 & 0xFFFF'0000) << 32) | int64_t((ce32 & 0xFF00) << 16)
				| int64_t(tertiary << 8);
	}
	ce32 -= tertiary;
	if ((tertiary & 0xF) == LongPrimaryTag) {
		// ppppppC1 -> pppppp0005000500
		return (int64_t(ce32) << 32) | CommonSecAndTerCE;
	}
	// ssssttC2 -> 00000000sssstt00
	return ce32;
}

static constexpr int64_t makeCE(uint32_t p) { return (int64_t(p) << 32) | CommonSecAndTerCE; }

static constexpr int64_t makeCE(uint32_t p, uint32_t s, uint32_t t, uint32_t q) {
	return (int64_t(p) << 32) | int64_t(s << 16) | int64_t(t) | int64_t(q << 6);
}

// The two CEs of a Latin mini expansion; see LatinExpansionTag.
static constexpr int64_t latinCE0FromCE32(uint32_t ce32) {
	return (int64_t(ce32 & 0xFF00'0000) << 32) | CommonSecondaryCE | int64_t((ce32 & 0xFF'0000) >> 8);
}

static constexpr int64_t latinCE1FromCE32(uint32_t ce32) {
	return int64_t((ce32 & 0xFF00) << 16) | CommonTertiaryCE;
}

// --- primary weight arithmetic -------------------------------------------------
//
// A primary weight is four bytes, and not every byte value is usable: 0..1 are
// sort key structure, 2..3 are the merge separator and the compression low
// terminator, and 0xFF is the compression high terminator when the lead byte is
// compressible. The four functions below step a weight by a code point offset
// through exactly the usable values - 251 per byte when compressible, 254 when
// not - which is why they are modular arithmetic rather than addition.

// ICU has three more of these - a two-byte increment and two decrements - which
// only its rule compiler and root-elements walker use. Neither is ported, so they
// are not here: an unused transcription is one nobody checks.
static uint32_t incThreeBytePrimaryByOffset(uint32_t basePrimary, bool isCompressible,
		int32_t offset) {
	offset += (int32_t(basePrimary >> 8) & 0xFF) - 2;
	uint32_t primary = uint32_t((offset % 254) + 2) << 8;
	offset /= 254;
	if (isCompressible) {
		offset += (int32_t(basePrimary >> 16) & 0xFF) - 4;
		primary |= uint32_t((offset % 251) + 4) << 16;
		offset /= 251;
	} else {
		offset += (int32_t(basePrimary >> 16) & 0xFF) - 2;
		primary |= uint32_t((offset % 254) + 2) << 16;
		offset /= 254;
	}
	return primary | ((basePrimary & 0xFF00'0000) + uint32_t(offset << 24));
}

// The primary for one code point of an OffsetTag range: the range's base primary
// stepped by (c - base) increments.
static uint32_t getThreeBytePrimaryForOffsetData(char32_t c, int64_t dataCE) {
	auto p = uint32_t(dataCE >> 32); // three-byte primary pppppp00
	auto lower32 = int32_t(dataCE); // bbbbbbss, bit 7 of ss is isCompressible
	int32_t offset = (int32_t(c) - (lower32 >> 8)) * (lower32 & 0x7F);
	return incThreeBytePrimaryByOffset(p, (lower32 & 0x80) != 0, offset);
}

// The implicit primary of an unassigned code point. Four bytes chosen so that
// every code point gets a distinct weight and none collides with a real one.
static uint32_t unassignedPrimaryFromCodePoint(int32_t c) {
	// A gap before U+0000; c == -1 is [first unassigned].
	++c;
	// Fourth byte: 18 values, every 14th byte value.
	uint32_t primary = uint32_t(2 + (c % 18) * 14);
	c /= 18;
	// Third byte: 254 values.
	primary |= uint32_t(2 + (c % 254)) << 8;
	c /= 254;
	// Second byte: 251 values, 04..FE, excluding the compression bytes.
	primary |= uint32_t(4 + (c % 251)) << 16;
	// One lead byte covers everything: 1 * 251 * 254 * 18 > 0x110000.
	return primary | (uint32_t(UnassignedImplicitByte) << 24);
}

static int64_t unassignedCEFromCodePoint(int32_t c) {
	return makeCE(unassignedPrimaryFromCodePoint(c));
}

} // namespace sprt::unicode::detail
