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

// What a collator was asked to do, packed into one word. Ported from ICU
// collationsettings.h and collationsettings.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// Everything a caller can vary - how many levels to compare, whether punctuation
// is ignored until the fourth level, whether digits sort as numbers, which case
// comes first, which scripts move to the front - is bits in `options`, plus a
// variable-top weight and a reordering permutation.
//
// The tailorings carry their own options and reordering, and those are constants
// in the generated tables, so ICU's ownership machinery (setReordering,
// aliasReordering, copyReorderingFrom, reorderCodesCapacity) collapses to plain
// assignment of pointers into read-only data. ICU's attribute setters
// (setStrength, setFlag, setCaseFirst...) are its public API for changing a live
// collator; ours are built once from a CollateOptions, so only the readers remain.

namespace sprt::unicode::detail {

// Strength values, as ICU's UColAttributeValue numbers them. The gap before
// Identical is real: it is 15, so that it stays the top of the 4-bit field.
enum : int32_t {
	StrengthPrimary = 0,
	StrengthSecondary = 1,
	StrengthTertiary = 2,
	StrengthQuaternary = 3,
	StrengthIdentical = 15,
	StrengthDefault = StrengthTertiary,
};

struct CollationSettings {
	// Options bits.
	enum : int32_t {
		// 0: check the input for FCD and deliver normalized text.
		CheckFcd = 1,
		// 1: numeric collation - digit sequences sort as numbers.
		Numeric = 2,
		// 2: "shifted" alternate handling, see AlternateMask.
		Shifted = 4,
		// 3..2: alternate handling; 0 is non-ignorable. 8 and 0xC are reserved for
		// shift-trimmed and blanked.
		AlternateMask = 0xC,
		// 6..4: the 3-bit maxVariable field.
		MaxVariableShift = 4,
		MaxVariableMask = 0x70,
		// 7: unused.
		// 8: sort uppercase first, when caseLevel or caseFirst is on.
		UpperFirst = 0x100,
		// 9: keep the case bits in the tertiary weight, where they outrank the rest
		// of it - unless the case level is on, in which case they move there. Off by
		// default, meaning the case bits are dropped from the tertiary weight.
		CaseFirst = 0x200,
		CaseFirstAndUpperMask = CaseFirst | UpperFirst,
		// 10: insert the case level between secondary and tertiary.
		CaseLevelBit = 0x400,
		// 11: compare secondary weights backwards ("French secondary").
		BackwardSecondary = 0x800,
		// 15..12: the 4-bit strength. The top field, so no mask is needed after
		// shifting.
		StrengthShift = 12,
		StrengthMask = 0xF000,
	};

	// maxVariable values: how much of the low end of the weight space counts as
	// "variable" when alternate handling is shifted.
	enum : int32_t {
		MaxVarSpace = 0,
		MaxVarPunct = 1,
		MaxVarSymbol = 2,
		MaxVarCurrency = 3,
	};

	int32_t options = (StrengthDefault << StrengthShift) | (MaxVarPunct << MaxVariableShift);

	// The variable-top primary weight: everything at or below it is "variable".
	uint32_t variableTop = 0;

	// 256 entries, one per primary lead byte, or null for no reordering. A zero at a
	// non-zero index means the lead byte is *split* - primaries sharing it move by
	// different offsets - and reorderEx has to consult the ranges.
	const uint8_t *reorderTable = nullptr;

	// The limit of the last reordered range; 0 when there is no reordering or no
	// split byte.
	uint32_t minHighNoReorder = 0;

	// (limit, offset) pairs for the split lead bytes: the upper 16 bits are the
	// upper 16 of an exclusive primary limit, the lower 16 a signed lead-byte offset
	// for the primaries below it. The ranges before the first split byte are
	// omitted, because reorderTable already covers them.
	const uint32_t *reorderRanges = nullptr;
	int32_t reorderRangesLength = 0;

	// The Latin fast path: its options word, negative when it is off, and the
	// precomputed primary weights it reads. The array is a constant in the
	// generated data - ICU computes it when a collator object is created and keeps
	// it there, and this API has no such object to keep it in.
	int32_t fastLatinOptions = -1;
	const uint16_t *fastLatinPrimaries = nullptr;

	static int32_t getStrength(int32_t options) { return options >> StrengthShift; }
	int32_t getStrength() const { return getStrength(options); }

	int32_t getMaxVariable() const { return (options & MaxVariableMask) >> MaxVariableShift; }

	bool hasReordering() const { return reorderTable != nullptr; }

	bool dontCheckFCD() const { return (options & CheckFcd) == 0; }
	bool hasBackwardSecondary() const { return (options & BackwardSecondary) != 0; }
	bool isNumeric() const { return (options & Numeric) != 0; }

	// Include the case bits in the tertiary level when the case level is off and
	// caseFirst is on.
	static bool isTertiaryWithCaseBits(int32_t options) {
		return (options & (CaseLevelBit | CaseFirst)) == CaseFirst;
	}

	static uint32_t getTertiaryMask(int32_t options) {
		return isTertiaryWithCaseBits(options) ? CaseAndTertiaryMask : OnlyTertiaryMask;
	}

	// On the tertiary level, consider the case bits and put uppercase first, when
	// the case level is off and caseFirst is upperFirst.
	static bool sortsTertiaryUpperCaseFirst(int32_t options) {
		return (options & (CaseLevelBit | CaseFirstAndUpperMask)) == CaseFirstAndUpperMask;
	}

	// The reordered primary weight.
	uint32_t reorder(uint32_t p) const {
		auto b = reorderTable[p >> 24];
		if (b != 0 || p <= NoCEPrimary) {
			return (uint32_t(b) << 24) | (p & 0xFF'FFFF);
		}
		return reorderEx(p);
	}

	// The split-lead-byte case: find the range p falls in and apply its offset.
	uint32_t reorderEx(uint32_t p) const {
		if (p >= minHighNoReorder) {
			return p;
		}
		// Round p up so that its low 16 bits are at least as large as any offset
		// bits, then compare directly against the (limit, offset) pairs.
		uint32_t q = p | 0xFFFF;
		uint32_t r;
		auto ranges = reorderRanges;
		while (q >= (r = *ranges)) { ++ranges; }
		return p + (r << 24);
	}
};

} // namespace sprt::unicode::detail
