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

// One collation table and the lookups into it. Ported from ICU collationdata.h
// and collationdata.cpp (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// A tailoring is a CollationData whose `base` points at the root. Its trie
// answers FallbackCE32 for every character it does not change, and the reader
// follows `base` from there; that is the whole mechanism by which a 7 KB Polish
// table sits on top of a 569 KB root.
//
// Three things ICU keeps here are gone:
//
//   `nfcImpl` - a reference to the normalizer. Ours is a set of free functions
//     over constant tables (collation_norm.cc), so there is nothing to hold.
//   `unsafeBackwardSet` - a UnicodeSet built on load. The generator computes it
//     and emits sorted boundaries; `isUnsafeBackward` binary-searches them.
//   `rootElements` - read only by the rule compiler, which is not ported.

namespace sprt::unicode::detail {

// The two identifiers the Latin fast path needs to ask about by name: the digit
// reordering group and the Latin script. Everything else it reaches through the
// script table.
static constexpr int32_t ReorderCodeDigit = 0x1004; // UCOL_REORDER_CODE_DIGIT
static constexpr int32_t ScriptLatin = 25; // USCRIPT_LATIN

struct CollationData {
	// 19 canonical L jamo, 21 V, 27 T.
	static constexpr int32_t JamoCE32sLength = 19 + 21 + 27;
	static constexpr int32_t MaxNumSpecialReorderCodes = 8;
	static constexpr int32_t ReorderCodeFirst = 0x1000; // UCOL_REORDER_CODE_SPACE

	Utrie2_32 trie;

	// ce32s[0] must be CE32(U+0000): the U0000Tag hands NUL-termination handling
	// off to the specials path, and that is where it comes back from.
	const uint32_t *ce32s;
	int32_t ce32sLength;

	// CEs for expansions and for OffsetTag ranges.
	const int64_t *ces;
	int32_t cesLength;

	// Prefix and contraction-suffix matching data.
	const char16_t *contexts;
	int32_t contextsLength;

	// The root, or null when this is the root.
	const CollationData *base;

	// One CE32 per canonical jamo, for HangulTag without a table lookup.
	const uint32_t *jamoCE32s;

	// The single-byte primary (xx000000) numeric collation counts digits with.
	uint32_t numericPrimary;

	// 256 flags: is this primary lead byte compressible.
	const uint8_t *compressibleBytes;

	// Code points that cannot start a comparison after an identical prefix, nor be
	// stepped back over. Sorted boundaries, inside == odd index.
	const uint32_t *unsafeBackward;
	int32_t unsafeBackwardLength;

	const uint16_t *fastLatinTable;
	int32_t fastLatinTableLength;

	// The fast path's primary weights, precomputed for the default alternate
	// handling; null when this table has no fast Latin data. See the generator for
	// why they are not computed on the fly.
	const uint16_t *fastLatinPrimaries;

	// Script and reordering-group boundaries. scriptsIndex has numScripts + 16
	// entries and maps a script (or a special reorder code, from numScripts on) to
	// an entry in scriptStarts; scriptStarts holds the top 16 bits of the first
	// primary of each group.
	int32_t numScripts;
	const uint16_t *scriptsIndex;
	const uint16_t *scriptStarts;
	int32_t scriptStartsLength;

	// --- lookups ---------------------------------------------------------------

	uint32_t getCE32(char32_t c) const { return trie.get(c); }

	uint32_t getCE32FromSupplementary(char32_t c) const { return trie.getFromSupplementary(c); }

	bool isDigit(char32_t c) const {
		return c < 0x660 ? c <= 0x39 && 0x30 <= c : hasCE32Tag(getCE32(c), DigitTag);
	}

	bool isUnsafeBackward(char32_t c, bool numeric) const {
		return unsafeBackwardContains(c) || (numeric && isDigit(c));
	}

	bool unsafeBackwardContains(char32_t c) const {
		// The number of boundaries at or below c is odd exactly inside a range.
		int32_t low = 0;
		int32_t high = unsafeBackwardLength;
		while (low < high) {
			auto mid = (low + high) / 2;
			if (unsafeBackward[mid] <= c) {
				low = mid + 1;
			} else {
				high = mid;
			}
		}
		return (low & 1) != 0;
	}

	bool isCompressibleLeadByte(uint32_t b) const { return compressibleBytes[b] != 0; }

	bool isCompressiblePrimary(uint32_t p) const { return isCompressibleLeadByte(p >> 24); }

	// The default CE32 stored in a contraction or prefix context, as two units.
	static uint32_t readCE32(const char16_t *p) { return (uint32_t(p[0]) << 16) | p[1]; }

	// One step of indirection for a special CE32 that only points at another one.
	uint32_t getIndirectCE32(uint32_t ce32) const {
		auto tag = tagFromCE32(ce32);
		if (tag == DigitTag) {
			// The non-numeric-collation CE32.
			ce32 = ce32s[indexFromCE32(ce32)];
		} else if (tag == LeadSurrogateTag) {
			ce32 = UnassignedCE32;
		} else if (tag == U0000Tag) {
			ce32 = ce32s[0];
		}
		return ce32;
	}

	uint32_t getFinalCE32(uint32_t ce32) const {
		return isSpecialCE32(ce32) ? getIndirectCE32(ce32) : ce32;
	}

	int64_t getCEFromOffsetCE32(char32_t c, uint32_t ce32) const {
		auto dataCE = ces[indexFromCE32(ce32)];
		return makeCE(getThreeBytePrimaryForOffsetData(c, dataCE));
	}

	// --- scripts and reordering groups -----------------------------------------

	int32_t getScriptIndex(int32_t script) const {
		if (script < 0) {
			return 0;
		} else if (script < numScripts) {
			return scriptsIndex[script];
		} else if (script < ReorderCodeFirst) {
			return 0;
		}
		script -= ReorderCodeFirst;
		return script < MaxNumSpecialReorderCodes ? scriptsIndex[numScripts + script] : 0;
	}

	// The first primary of a group, with only its lead bytes - not necessarily a
	// weight any character actually has. Zero when the script is unknown.
	uint32_t getFirstPrimaryForGroup(int32_t script) const {
		auto index = getScriptIndex(script);
		return index == 0 ? 0 : uint32_t(scriptStarts[index]) << 16;
	}

	uint32_t getLastPrimaryForGroup(int32_t script) const {
		auto index = getScriptIndex(script);
		if (index == 0) {
			return 0;
		}
		uint32_t limit = scriptStarts[index + 1];
		return (limit << 16) - 1;
	}

	// The group a primary falls in, as its first script, or -1 past the last group.
	int32_t getGroupForPrimary(uint32_t p) const {
		p >>= 16;
		if (p < scriptStarts[1] || scriptStarts[scriptStartsLength - 1] <= p) {
			return -1;
		}
		int32_t index = 1;
		while (p >= scriptStarts[index + 1]) { ++index; }
		for (int32_t i = 0; i < numScripts; ++i) {
			if (scriptsIndex[i] == index) {
				return i;
			}
		}
		for (int32_t i = 0; i < MaxNumSpecialReorderCodes; ++i) {
			if (scriptsIndex[numScripts + i] == index) {
				return ReorderCodeFirst + i;
			}
		}
		return -1;
	}
};

// One locale's collation: the table plus the settings that come with it. Both
// are constants in the generated data; a tailoring is never built at run time.
struct CollationTailoring {
	const CollationData *data;
	int32_t options;

	// 256 entries, or null when this locale does not reorder scripts.
	const uint8_t *reorderTable;
	const uint32_t *reorderRanges;
	int32_t reorderRangesLength;
	uint32_t minHighNoReorder;
};

struct CollationLocale {
	const char *tag;
	int32_t tagLength;
	const CollationTailoring *tailoring;
};

} // namespace sprt::unicode::detail
