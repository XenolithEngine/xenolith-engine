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

// The Latin fast path. Ported from ICU collationfastlatin.h and
// collationfastlatin.cpp (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// Comparing two short ASCII strings through the general engine means building
// collation elements for every character: a trie lookup, a special-CE32 switch, a
// buffer. For the 384 characters of Latin-1 and Latin Extended-A plus general
// punctuation, all of that collapses into one 16-bit "mini CE" per character,
// looked up in a 960-byte table that ships with each collation table. That is
// what this file compares. It is what makes sorting a list of names cost what a
// strcmp costs.
//
// A mini CE packs a primary, a secondary, case bits and a tertiary into 16 bits,
// which only works because Latin uses so few distinct weights. When something
// does not fit - an unsupported character, a contraction too long, numeric
// ordering, French secondaries - the answer is BailOut and the caller runs the
// general path instead. So this file can only ever be *slower*, never wrong: a
// mistake that made it bail out always would cost speed and nothing else.
//
// The value it must produce is the same as compareUpToQuaternary's. That is
// asserted directly: runtime_collation_conformance runs the whole UCA suite with
// the fast path on, and it engages on every Latin sequence in it.
//
// Departure from ICU: the two compare functions here are the same algorithm over
// two encodings, and ICU keeps them as two copies with a comment begging that
// they be kept in sync. They are one function here, over a policy that supplies
// the per-encoding character fetch - which is the same reasoning that put the
// trie reader in a shared header rather than in two ports of it.

namespace sprt::unicode::detail {

class FastLatin {
public:
	// The format version of the table, checked against its first word.
	static constexpr uint16_t Version = 2;

	static constexpr int32_t LatinMax = 0x17F;
	static constexpr int32_t LatinLimit = LatinMax + 1;
	static constexpr int32_t LatinMaxUtf8Lead = 0xC5; // the UTF-8 lead byte of LatinMax

	static constexpr int32_t PunctStart = 0x2000;
	static constexpr int32_t PunctLimit = 0x2040;

	// U+FFFE and U+FFFF are excluded.
	static constexpr int32_t NumFastChars = LatinLimit + (PunctLimit - PunctStart);

	// The fields of a mini CE. Digits may use long or short primaries without
	// changing the structure.
	enum : uint32_t {
		ShortPrimaryMask = 0xFC00, // bits 15..10
		IndexMask = 0x3FF, // bits 9..0, for expansions and contractions
		SecondaryMask = 0x3E0, // bits 9..5
		CaseMask = 0x18, // bits 4..3
		LongPrimaryMask = 0xFFF8, // bits 15..3
		TertiaryMask = 7, // bits 2..0
		CaseAndTertiaryMask = CaseMask | TertiaryMask,

		TwoShortPrimariesMask = (ShortPrimaryMask << 16) | ShortPrimaryMask,
		TwoLongPrimariesMask = (LongPrimaryMask << 16) | LongPrimaryMask,
		TwoSecondariesMask = (SecondaryMask << 16) | SecondaryMask,
		TwoCasesMask = (CaseMask << 16) | CaseMask,
		TwoTertiariesMask = (TertiaryMask << 16) | TertiaryMask,
	};

	enum : uint32_t {
		// A contraction with one fast Latin character. U+0000 maps here too, so
		// that the fast path need not test for NUL termination.
		Contraction = 0x400,
		// An expansion: two CEs, found after the fixed table.
		Expansion = 0x800,
		// One CE with a long, low primary - 128 of them. Everything that might be
		// variable lives here, which is what keeps the short-primary path short.
		MinLong = 0xC00,
		LongInc = 8,
		MaxLong = 0xFF8,
		// One CE with a short, high primary - 60 of them - plus a secondary CE when
		// the secondary weight is high. Every letter should be in this range.
		MinShort = 0x1000,
		ShortInc = 0x400,
		// The highest primary is reserved for U+FFFF.
		MaxShort = ShortPrimaryMask,

		MinSecBefore = 0, // add SecOffset
		SecInc = 0x20,
		MaxSecBefore = MinSecBefore + 4 * SecInc, // 5 before the common weight
		CommonSec = MaxSecBefore + SecInc,
		MinSecAfter = CommonSec + SecInc,
		MaxSecAfter = MinSecAfter + 5 * SecInc, // 6 after it
		MinSecHigh = MaxSecAfter + SecInc, // 20 high secondaries
		MaxSecHigh = SecondaryMask,

		// Added to every secondary weight except a wholly ignorable one, so that
		// no real weight collides with the special values below.
		SecOffset = SecInc,
		CommonSecPlusOffset = CommonSec + SecOffset,
		TwoSecOffsets = (SecOffset << 16) | SecOffset,
		TwoCommonSecPlusOffset = (CommonSecPlusOffset << 16) | CommonSecPlusOffset,

		LowerCase = 8, // the case bits carry this offset
		TwoLowerCases = (LowerCase << 16) | LowerCase,

		CommonTer = 0, // add TerOffset
		MaxTerAfter = 7,

		// Same idea for tertiaries; it must also exceed the case bits, so that a
		// combined case+tertiary weight plus the offset cannot spill over.
		TerOffset = SecOffset,
		CommonTerPlusOffset = CommonTer + TerOffset,
		TwoTerOffsets = (TerOffset << 16) | TerOffset,
		TwoCommonTerPlusOffset = (CommonTerPlusOffset << 16) | CommonTerPlusOffset,

		MergeWeight = 3,
		EOS = 2, // end of string
		BailOut = 1,

		// The first word of a contraction result: bits 8..0 are the second
		// character as a fast-char index, and each list ends with CharMask.
		ContrCharMask = 0x1FF,
		// Bits 10..9 are the length: 1 = bail out, 2 = one mini CE, 3 = two.
		ContrLengthShift = 9,
	};

	// What compare() returns when the general path has to run after all.
	static constexpr int32_t BailOutResult = -2;

	// The options word for the compare functions, and the precomputed primaries.
	// Returns -1 when the fast path cannot be used for this data and these
	// settings at all. `primaries` must have LatinLimit entries.
	static int32_t getOptions(const CollationData *data, const CollationSettings &settings,
			uint16_t *primaries, int32_t capacity);

	static int32_t compareUtf16(const uint16_t *table, const uint16_t *primaries, int32_t options,
			const char16_t *left, int32_t leftLength, const char16_t *right, int32_t rightLength);

	static int32_t compareUtf8(const uint16_t *table, const uint16_t *primaries, int32_t options,
			const uint8_t *left, int32_t leftLength, const uint8_t *right, int32_t rightLength);

private:
	static uint32_t lookup(const uint16_t *table, int32_t c) {
		if (PunctStart <= c && c < PunctLimit) {
			return table[c - PunctStart + LatinLimit];
		}
		if (c == 0xFFFE) {
			return MergeWeight;
		}
		if (c == 0xFFFF) {
			return MaxShort | CommonSec | LowerCase | CommonTer;
		}
		return BailOut;
	}

	static uint32_t lookupUtf8(const uint16_t *table, int32_t c, const uint8_t *s, int32_t &index,
			int32_t length) {
		// ASCII and valid Latin were handled by the caller.
		auto i2 = index + 1;
		if (i2 < length) {
			auto t1 = s[index];
			auto t2 = s[i2];
			index += 2;
			if (c == 0xE2 && t1 == 0x80 && 0x80 <= t2 && t2 <= 0xBF) {
				return table[(LatinLimit - 0x80) + t2]; // U+2000..U+203F
			}
			if (c == 0xEF && t1 == 0xBF) {
				if (t2 == 0xBE) {
					return MergeWeight; // U+FFFE
				}
				if (t2 == 0xBF) {
					return MaxShort | CommonSec | LowerCase | CommonTer; // U+FFFF
				}
			}
		}
		return BailOut;
	}

	// The levels below the primary re-read the text, which by then is known to be
	// well-formed and wholly supported - so these skip every check.
	static uint32_t lookupUtf8Unsafe(const uint16_t *table, int32_t c, const uint8_t *s,
			int32_t &index) {
		if (c <= LatinMaxUtf8Lead) {
			return table[((c - 0xC2) << 6) + s[index++]]; // U+0080..U+017F
		}
		auto t2 = s[index + 1];
		index += 2;
		if (c == 0xE2) {
			return table[(LatinLimit - 0x80) + t2]; // U+2000..U+203F
		}
		return t2 == 0xBE ? MergeWeight : (MaxShort | CommonSec | LowerCase | CommonTer);
	}

	static uint32_t nextPair(const uint16_t *table, int32_t c, uint32_t ce, const char16_t *s16,
			const uint8_t *s8, int32_t &index, int32_t length);

	static uint32_t getPrimaries(uint32_t variableTop, uint32_t pair) {
		auto ce = pair & 0xFFFF;
		if (ce >= MinShort) {
			return pair & TwoShortPrimariesMask;
		}
		if (ce > variableTop) {
			return pair & TwoLongPrimariesMask;
		}
		if (ce >= MinLong) {
			return 0; // variable
		}
		return pair; // a special mini CE
	}

	static uint32_t getSecondariesFromOneShortCE(uint32_t ce) {
		ce &= SecondaryMask;
		if (ce < MinSecHigh) {
			return ce + SecOffset;
		}
		return ((ce + SecOffset) << 16) | CommonSecPlusOffset;
	}

	static uint32_t getSecondaries(uint32_t variableTop, uint32_t pair);
	static uint32_t getCases(uint32_t variableTop, bool strengthIsPrimary, uint32_t pair);
	static uint32_t getTertiaries(uint32_t variableTop, bool withCaseBits, uint32_t pair);
	static uint32_t getQuaternaries(uint32_t variableTop, uint32_t pair);

	// The per-encoding half: everything the level loops do differently.
	struct Utf16Text;
	struct Utf8Text;

	template <typename Text>
	static int32_t compare(const uint16_t *table, const uint16_t *primaries, int32_t options,
			const typename Text::Unit *left, int32_t leftLength, const typename Text::Unit *right,
			int32_t rightLength);
};

// --- the two encodings ----------------------------------------------------------

struct FastLatin::Utf16Text {
	using Unit = char16_t;

	// The primary level: the character, its precomputed primary if it has one, and
	// otherwise its raw table entry. Returns false to bail out.
	static bool primaryEntry(const uint16_t *table, const uint16_t *primaries, const Unit *s,
			int32_t &index, int32_t length, bool numeric, int32_t &c, uint32_t &pair,
			bool &settled) {
		c = s[index++];
		settled = false;
		if (c <= LatinMax) {
			pair = primaries[c];
			if (pair != 0) {
				settled = true;
				return true;
			}
			if (c <= 0x39 && c >= 0x30 && numeric) {
				return false;
			}
			pair = table[c];
		} else if (PunctStart <= c && c < PunctLimit) {
			pair = table[c - PunctStart + LatinLimit];
		} else {
			pair = lookup(table, c);
		}
		return true;
	}

	// The secondary level, which re-reads the text.
	static uint32_t entry(const uint16_t *table, const Unit *s, int32_t &index, int32_t &c) {
		c = s[index++];
		if (c <= LatinMax) {
			return table[c];
		}
		if (PunctStart <= c && c < PunctLimit) {
			return table[c - PunctStart + LatinLimit];
		}
		return lookup(table, c);
	}

	// The case, tertiary and quaternary levels, which do not special-case
	// punctuation because lookup() handles it.
	static uint32_t plainEntry(const uint16_t *table, const Unit *s, int32_t &index, int32_t &c) {
		c = s[index++];
		return c <= LatinMax ? table[c] : lookup(table, c);
	}

	static uint32_t nextPair(const uint16_t *table, int32_t c, uint32_t ce, const Unit *s,
			int32_t &index, int32_t length) {
		return FastLatin::nextPair(table, c, ce, s, nullptr, index, length);
	}
};

struct FastLatin::Utf8Text {
	using Unit = uint8_t;

	static bool primaryEntry(const uint16_t *table, const uint16_t *primaries, const Unit *s,
			int32_t &index, int32_t length, bool numeric, int32_t &c, uint32_t &pair,
			bool &settled) {
		c = s[index++];
		settled = false;
		uint8_t t;
		if (c <= 0x7F) {
			pair = primaries[c];
			if (pair != 0) {
				settled = true;
				return true;
			}
			if (c <= 0x39 && c >= 0x30 && numeric) {
				return false;
			}
			pair = table[c];
		} else if (c <= LatinMaxUtf8Lead && 0xC2 <= c && index != length
				&& 0x80 <= (t = s[index]) && t <= 0xBF) {
			++index;
			c = ((c - 0xC2) << 6) + t;
			pair = primaries[c];
			if (pair != 0) {
				settled = true;
				return true;
			}
			pair = table[c];
		} else {
			pair = lookupUtf8(table, c, s, index, length);
		}
		return true;
	}

	static uint32_t entry(const uint16_t *table, const Unit *s, int32_t &index, int32_t &c) {
		c = s[index++];
		if (c <= 0x7F) {
			return table[c];
		}
		if (c <= LatinMaxUtf8Lead) {
			return table[((c - 0xC2) << 6) + s[index++]];
		}
		return lookupUtf8Unsafe(table, c, s, index);
	}

	static uint32_t plainEntry(const uint16_t *table, const Unit *s, int32_t &index, int32_t &c) {
		c = s[index++];
		return c <= 0x7F ? table[c] : lookupUtf8Unsafe(table, c, s, index);
	}

	static uint32_t nextPair(const uint16_t *table, int32_t c, uint32_t ce, const Unit *s,
			int32_t &index, int32_t length) {
		return FastLatin::nextPair(table, c, ce, nullptr, s, index, length);
	}
};

// --- options --------------------------------------------------------------------

int32_t FastLatin::getOptions(const CollationData *data, const CollationSettings &settings,
		uint16_t *primaries, int32_t capacity) {
	auto table = data->fastLatinTable;
	if (table == nullptr || capacity != LatinLimit) {
		return -1;
	}

	uint32_t miniVarTop;
	if ((settings.options & CollationSettings::AlternateMask) == 0) {
		// Nothing is variable, so put the threshold just below the lowest long
		// mini primary and let every comparison test out early.
		miniVarTop = MinLong - 1;
	} else {
		auto headerLength = int32_t(*table & 0xFF);
		auto i = 1 + settings.getMaxVariable();
		if (i >= headerLength) {
			return -1; // variableTop at or above the digits: cannot happen
		}
		miniVarTop = table[i];
	}

	bool digitsAreReordered = false;
	if (settings.hasReordering()) {
		uint32_t prevStart = 0;
		uint32_t beforeDigitStart = 0;
		uint32_t digitStart = 0;
		uint32_t afterDigitStart = 0;
		for (int32_t group = CollationData::ReorderCodeFirst;
				group < CollationData::ReorderCodeFirst + CollationData::MaxNumSpecialReorderCodes;
				++group) {
			auto start = settings.reorder(data->getFirstPrimaryForGroup(group));
			if (group == ReorderCodeDigit) {
				beforeDigitStart = prevStart;
				digitStart = start;
			} else if (start != 0) {
				if (start < prevStart) {
					return -1; // the permutation reaches the groups below Latin
				}
				if (digitStart != 0 && afterDigitStart == 0 && prevStart == beforeDigitStart) {
					afterDigitStart = start;
				}
				prevStart = start;
			}
		}
		auto latinStart = settings.reorder(data->getFirstPrimaryForGroup(ScriptLatin));
		if (latinStart < prevStart) {
			return -1;
		}
		if (afterDigitStart == 0) {
			afterDigitStart = latinStart;
		}
		if (!(beforeDigitStart < digitStart && digitStart < afterDigitStart)) {
			digitsAreReordered = true;
		}
	}

	table += (table[0] & 0xFF); // skip the header
	for (int32_t c = 0; c < LatinLimit; ++c) {
		uint32_t p = table[c];
		if (p >= MinShort) {
			p &= ShortPrimaryMask;
		} else if (p > miniVarTop) {
			p &= LongPrimaryMask;
		} else {
			p = 0;
		}
		primaries[c] = uint16_t(p);
	}
	if (digitsAreReordered || (settings.options & CollationSettings::Numeric) != 0) {
		// Digits have to go the long way round.
		for (int32_t c = 0x30; c <= 0x39; ++c) { primaries[c] = 0; }
	}

	// The threshold rides above the options word.
	return int32_t(miniVarTop << 16) | settings.options;
}

// --- mini CE decoding -------------------------------------------------------------

uint32_t FastLatin::nextPair(const uint16_t *table, int32_t c, uint32_t ce, const char16_t *s16,
		const uint8_t *s8, int32_t &index, int32_t length) {
	if (ce >= MinLong || ce < Contraction) {
		return ce; // a simple or special mini CE
	}
	if (ce >= Expansion) {
		auto at = NumFastChars + int32_t(ce & IndexMask);
		return (uint32_t(table[at + 1]) << 16) | table[at];
	}

	// A contraction: the default mapping, then zero or more single-character
	// suffix mappings in ascending order of the suffix character.
	auto at = NumFastChars + int32_t(ce & IndexMask);
	if (index != length) {
		int32_t c2;
		auto nextIndex = index;
		if (s16 != nullptr) {
			c2 = s16[nextIndex++];
			if (c2 > LatinMax) {
				if (PunctStart <= c2 && c2 < PunctLimit) {
					c2 = c2 - PunctStart + LatinLimit;
				} else if (c2 == 0xFFFE || c2 == 0xFFFF) {
					c2 = -1; // neither can occur in a contraction
				} else {
					return BailOut;
				}
			}
		} else {
			c2 = s8[nextIndex++];
			if (c2 > 0x7F) {
				uint8_t t;
				if (c2 <= 0xC5 && 0xC2 <= c2 && nextIndex != length
						&& 0x80 <= (t = s8[nextIndex]) && t <= 0xBF) {
					c2 = ((c2 - 0xC2) << 6) + t; // U+0080..U+017F
					++nextIndex;
				} else {
					auto i2 = nextIndex + 1;
					if (i2 >= length) {
						return BailOut;
					}
					if (c2 == 0xE2 && s8[nextIndex] == 0x80 && 0x80 <= (t = s8[i2]) && t <= 0xBF) {
						c2 = (LatinLimit - 0x80) + t; // U+2000..U+203F
					} else if (c2 == 0xEF && s8[nextIndex] == 0xBF
							&& ((t = s8[i2]) == 0xBE || t == 0xBF)) {
						c2 = -1; // U+FFFE and U+FFFF cannot occur in a contraction
					} else {
						return BailOut;
					}
					nextIndex += 2;
				}
			}
		}
		// Walk the suffix list, which is sorted by suffix character.
		auto i = at;
		int32_t head = table[i]; // skip the default mapping first
		int32_t x;
		do {
			i += head >> ContrLengthShift;
			head = table[i];
			x = head & ContrCharMask;
		} while (x < c2);
		if (x == c2) {
			at = i;
			index = nextIndex;
		}
	}

	auto entryLength = int32_t(table[at] >> ContrLengthShift);
	if (entryLength == 1) {
		return BailOut;
	}
	ce = table[at + 1];
	if (entryLength == 2) {
		return ce;
	}
	return (uint32_t(table[at + 2]) << 16) | ce;
}

uint32_t FastLatin::getSecondaries(uint32_t variableTop, uint32_t pair) {
	if (pair <= 0xFFFF) {
		// one mini CE
		if (pair >= MinShort) {
			pair = getSecondariesFromOneShortCE(pair);
		} else if (pair > variableTop) {
			pair = CommonSecPlusOffset;
		} else if (pair >= MinLong) {
			pair = 0; // variable
		}
		// otherwise a special mini CE, left as it is
	} else {
		auto ce = pair & 0xFFFF;
		if (ce >= MinShort) {
			pair = (pair & TwoSecondariesMask) + TwoSecOffsets;
		} else if (ce > variableTop) {
			pair = TwoCommonSecPlusOffset;
		} else {
			pair = 0; // variable
		}
	}
	return pair;
}

uint32_t FastLatin::getCases(uint32_t variableTop, bool strengthIsPrimary, uint32_t pair) {
	// The case weights of primary ignorables (at primary strength) or of secondary
	// ignorables (otherwise) are dropped, for the reasons in collation_compare.cc.
	// Tertiary CEs are not supported in the fast path at all.
	if (pair <= 0xFFFF) {
		if (pair >= MinShort) {
			// A high secondary weight means this is really two CEs, a primary and a
			// secondary one.
			auto ce = pair;
			pair &= CaseMask; // the primary CE's own weight
			if (!strengthIsPrimary && (ce & SecondaryMask) >= MinSecHigh) {
				pair |= LowerCase << 16; // the secondary CE's implied weight
			}
		} else if (pair > variableTop) {
			pair = LowerCase;
		} else if (pair >= MinLong) {
			pair = 0; // variable
		}
	} else {
		auto ce = pair & 0xFFFF;
		if (ce >= MinShort) {
			if (strengthIsPrimary && (pair & (ShortPrimaryMask << 16)) == 0) {
				pair &= CaseMask;
			} else {
				pair &= TwoCasesMask;
			}
		} else if (ce > variableTop) {
			pair = TwoLowerCases;
		} else {
			pair = 0; // variable
		}
	}
	return pair;
}

uint32_t FastLatin::getTertiaries(uint32_t variableTop, bool withCaseBits, uint32_t pair) {
	if (pair <= 0xFFFF) {
		if (pair >= MinShort) {
			auto ce = pair;
			if (withCaseBits) {
				pair = (pair & CaseAndTertiaryMask) + TerOffset;
				if ((ce & SecondaryMask) >= MinSecHigh) {
					pair |= (LowerCase | CommonTerPlusOffset) << 16;
				}
			} else {
				pair = (pair & TertiaryMask) + TerOffset;
				if ((ce & SecondaryMask) >= MinSecHigh) {
					pair |= CommonTerPlusOffset << 16;
				}
			}
		} else if (pair > variableTop) {
			pair = (pair & TertiaryMask) + TerOffset;
			if (withCaseBits) {
				pair |= LowerCase;
			}
		} else if (pair >= MinLong) {
			pair = 0; // variable
		}
	} else {
		auto ce = pair & 0xFFFF;
		if (ce >= MinShort) {
			pair &= withCaseBits ? (TwoCasesMask | TwoTertiariesMask) : TwoTertiariesMask;
			pair += TwoTerOffsets;
		} else if (ce > variableTop) {
			pair = (pair & TwoTertiariesMask) + TwoTerOffsets;
			if (withCaseBits) {
				pair |= TwoLowerCases;
			}
		} else {
			pair = 0; // variable
		}
	}
	return pair;
}

uint32_t FastLatin::getQuaternaries(uint32_t variableTop, uint32_t pair) {
	// The primary weight of a variable CE, or the highest primary for anything
	// that is neither variable nor wholly ignorable.
	if (pair <= 0xFFFF) {
		if (pair >= MinShort) {
			pair = (pair & SecondaryMask) >= MinSecHigh ? uint32_t(TwoShortPrimariesMask)
														: uint32_t(ShortPrimaryMask);
		} else if (pair > variableTop) {
			pair = ShortPrimaryMask;
		} else if (pair >= MinLong) {
			pair &= LongPrimaryMask; // variable
		}
	} else {
		auto ce = pair & 0xFFFF;
		if (ce > variableTop) {
			pair = TwoShortPrimariesMask;
		} else {
			pair &= TwoLongPrimariesMask; // variable
		}
	}
	return pair;
}

// --- comparison ------------------------------------------------------------------

// One algorithm over both encodings. A level pulls mini CEs from each side until
// it has a non-ignorable weight or the end, compares them, and moves on; the pair
// variable holds the current mini CE in its low half and the next one, if the
// character produced two, in its high half.
template <typename Text>
int32_t FastLatin::compare(const uint16_t *table, const uint16_t *primaries, int32_t options,
		const typename Text::Unit *left, int32_t leftLength, const typename Text::Unit *right,
		int32_t rightLength) {
	table += (table[0] & 0xFF); // skip the header
	auto variableTop = uint32_t(options) >> 16; // packed there by getOptions
	options &= 0xFFFF; // so that getStrength works
	auto numeric = (options & CollationSettings::Numeric) != 0;

	int32_t leftIndex = 0;
	int32_t rightIndex = 0;
	uint32_t leftPair = 0;
	uint32_t rightPair = 0;

	// --- primaries ---
	for (;;) {
		while (leftPair == 0) {
			if (leftIndex == leftLength) {
				leftPair = EOS;
				break;
			}
			int32_t c;
			bool settled;
			if (!Text::primaryEntry(table, primaries, left, leftIndex, leftLength, numeric, c,
						leftPair, settled)) {
				return BailOutResult;
			}
			if (settled) {
				break;
			}
			if (leftPair >= MinShort) {
				leftPair &= ShortPrimaryMask;
				break;
			} else if (leftPair > variableTop) {
				leftPair &= LongPrimaryMask;
				break;
			} else {
				leftPair = Text::nextPair(table, c, leftPair, left, leftIndex, leftLength);
				if (leftPair == BailOut) {
					return BailOutResult;
				}
				leftPair = getPrimaries(variableTop, leftPair);
			}
		}

		while (rightPair == 0) {
			if (rightIndex == rightLength) {
				rightPair = EOS;
				break;
			}
			int32_t c;
			bool settled;
			if (!Text::primaryEntry(table, primaries, right, rightIndex, rightLength, numeric, c,
						rightPair, settled)) {
				return BailOutResult;
			}
			if (settled) {
				break;
			}
			if (rightPair >= MinShort) {
				rightPair &= ShortPrimaryMask;
				break;
			} else if (rightPair > variableTop) {
				rightPair &= LongPrimaryMask;
				break;
			} else {
				rightPair = Text::nextPair(table, c, rightPair, right, rightIndex, rightLength);
				if (rightPair == BailOut) {
					return BailOutResult;
				}
				rightPair = getPrimaries(variableTop, rightPair);
			}
		}

		if (leftPair == rightPair) {
			if (leftPair == EOS) {
				break;
			}
			leftPair = rightPair = 0;
			continue;
		}
		auto leftPrimary = leftPair & 0xFFFF;
		auto rightPrimary = rightPair & 0xFFFF;
		if (leftPrimary != rightPrimary) {
			return leftPrimary < rightPrimary ? CompareLess : CompareGreater;
		}
		if (leftPair == EOS) {
			break;
		}
		leftPair >>= 16;
		rightPair >>= 16;
	}

	// Every level below re-reads the text rather than buffering the CEs - by now
	// the string is known to be well-formed and wholly supported, so re-reading is
	// cheaper than having kept them.

	// --- secondaries ---
	if (CollationSettings::getStrength(options) >= StrengthSecondary) {
		leftIndex = rightIndex = 0;
		leftPair = rightPair = 0;
		for (;;) {
			while (leftPair == 0) {
				if (leftIndex == leftLength) {
					leftPair = EOS;
					break;
				}
				int32_t c;
				leftPair = Text::entry(table, left, leftIndex, c);
				if (leftPair >= MinShort) {
					leftPair = getSecondariesFromOneShortCE(leftPair);
					break;
				} else if (leftPair > variableTop) {
					leftPair = CommonSecPlusOffset;
					break;
				} else {
					leftPair = Text::nextPair(table, c, leftPair, left, leftIndex, leftLength);
					leftPair = getSecondaries(variableTop, leftPair);
				}
			}

			while (rightPair == 0) {
				if (rightIndex == rightLength) {
					rightPair = EOS;
					break;
				}
				int32_t c;
				rightPair = Text::entry(table, right, rightIndex, c);
				if (rightPair >= MinShort) {
					rightPair = getSecondariesFromOneShortCE(rightPair);
					break;
				} else if (rightPair > variableTop) {
					rightPair = CommonSecPlusOffset;
					break;
				} else {
					rightPair = Text::nextPair(table, c, rightPair, right, rightIndex, rightLength);
					rightPair = getSecondaries(variableTop, rightPair);
				}
			}

			if (leftPair == rightPair) {
				if (leftPair == EOS) {
					break;
				}
				leftPair = rightPair = 0;
				continue;
			}
			auto leftSecondary = leftPair & 0xFFFF;
			auto rightSecondary = rightPair & 0xFFFF;
			if (leftSecondary != rightSecondary) {
				if ((options & CollationSettings::BackwardSecondary) != 0) {
					// A backward secondary level needs backward contraction matching
					// and movement between merge separators; that is the slow path.
					return BailOutResult;
				}
				return leftSecondary < rightSecondary ? CompareLess : CompareGreater;
			}
			if (leftPair == EOS) {
				break;
			}
			leftPair >>= 16;
			rightPair >>= 16;
		}
	}

	// --- the case level ---
	if ((options & CollationSettings::CaseLevelBit) != 0) {
		auto strengthIsPrimary = CollationSettings::getStrength(options) == StrengthPrimary;
		leftIndex = rightIndex = 0;
		leftPair = rightPair = 0;
		for (;;) {
			while (leftPair == 0) {
				if (leftIndex == leftLength) {
					leftPair = EOS;
					break;
				}
				int32_t c;
				leftPair = Text::plainEntry(table, left, leftIndex, c);
				if (leftPair < MinLong) {
					leftPair = Text::nextPair(table, c, leftPair, left, leftIndex, leftLength);
				}
				leftPair = getCases(variableTop, strengthIsPrimary, leftPair);
			}

			while (rightPair == 0) {
				if (rightIndex == rightLength) {
					rightPair = EOS;
					break;
				}
				int32_t c;
				rightPair = Text::plainEntry(table, right, rightIndex, c);
				if (rightPair < MinLong) {
					rightPair = Text::nextPair(table, c, rightPair, right, rightIndex, rightLength);
				}
				rightPair = getCases(variableTop, strengthIsPrimary, rightPair);
			}

			if (leftPair == rightPair) {
				if (leftPair == EOS) {
					break;
				}
				leftPair = rightPair = 0;
				continue;
			}
			auto leftCase = leftPair & 0xFFFF;
			auto rightCase = rightPair & 0xFFFF;
			if (leftCase != rightCase) {
				if ((options & CollationSettings::UpperFirst) == 0) {
					return leftCase < rightCase ? CompareLess : CompareGreater;
				}
				return leftCase < rightCase ? CompareGreater : CompareLess;
			}
			if (leftPair == EOS) {
				break;
			}
			leftPair >>= 16;
			rightPair >>= 16;
		}
	}
	if (CollationSettings::getStrength(options) <= StrengthSecondary) {
		return CompareEqual;
	}

	// --- tertiaries ---
	auto withCaseBits = CollationSettings::isTertiaryWithCaseBits(options);

	leftIndex = rightIndex = 0;
	leftPair = rightPair = 0;
	for (;;) {
		while (leftPair == 0) {
			if (leftIndex == leftLength) {
				leftPair = EOS;
				break;
			}
			int32_t c;
			leftPair = Text::plainEntry(table, left, leftIndex, c);
			if (leftPair < MinLong) {
				leftPair = Text::nextPair(table, c, leftPair, left, leftIndex, leftLength);
			}
			leftPair = getTertiaries(variableTop, withCaseBits, leftPair);
		}

		while (rightPair == 0) {
			if (rightIndex == rightLength) {
				rightPair = EOS;
				break;
			}
			int32_t c;
			rightPair = Text::plainEntry(table, right, rightIndex, c);
			if (rightPair < MinLong) {
				rightPair = Text::nextPair(table, c, rightPair, right, rightIndex, rightLength);
			}
			rightPair = getTertiaries(variableTop, withCaseBits, rightPair);
		}

		if (leftPair == rightPair) {
			if (leftPair == EOS) {
				break;
			}
			leftPair = rightPair = 0;
			continue;
		}
		auto leftTertiary = leftPair & 0xFFFF;
		auto rightTertiary = rightPair & 0xFFFF;
		if (leftTertiary != rightTertiary) {
			if (CollationSettings::sortsTertiaryUpperCaseFirst(options)) {
				// EOS and the merge weight pass through; real tertiary weights stay
				// above the merge weight.
				if (leftTertiary > MergeWeight) {
					leftTertiary ^= CaseMask;
				}
				if (rightTertiary > MergeWeight) {
					rightTertiary ^= CaseMask;
				}
			}
			return leftTertiary < rightTertiary ? CompareLess : CompareGreater;
		}
		if (leftPair == EOS) {
			break;
		}
		leftPair >>= 16;
		rightPair >>= 16;
	}
	if (CollationSettings::getStrength(options) <= StrengthTertiary) {
		return CompareEqual;
	}

	// --- quaternaries ---
	leftIndex = rightIndex = 0;
	leftPair = rightPair = 0;
	for (;;) {
		while (leftPair == 0) {
			if (leftIndex == leftLength) {
				leftPair = EOS;
				break;
			}
			int32_t c;
			leftPair = Text::plainEntry(table, left, leftIndex, c);
			if (leftPair < MinLong) {
				leftPair = Text::nextPair(table, c, leftPair, left, leftIndex, leftLength);
			}
			leftPair = getQuaternaries(variableTop, leftPair);
		}

		while (rightPair == 0) {
			if (rightIndex == rightLength) {
				rightPair = EOS;
				break;
			}
			int32_t c;
			rightPair = Text::plainEntry(table, right, rightIndex, c);
			if (rightPair < MinLong) {
				rightPair = Text::nextPair(table, c, rightPair, right, rightIndex, rightLength);
			}
			rightPair = getQuaternaries(variableTop, rightPair);
		}

		if (leftPair == rightPair) {
			if (leftPair == EOS) {
				break;
			}
			leftPair = rightPair = 0;
			continue;
		}
		auto leftQuaternary = leftPair & 0xFFFF;
		auto rightQuaternary = rightPair & 0xFFFF;
		if (leftQuaternary != rightQuaternary) {
			return leftQuaternary < rightQuaternary ? CompareLess : CompareGreater;
		}
		if (leftPair == EOS) {
			break;
		}
		leftPair >>= 16;
		rightPair >>= 16;
	}
	return CompareEqual;
}

int32_t FastLatin::compareUtf16(const uint16_t *table, const uint16_t *primaries, int32_t options,
		const char16_t *left, int32_t leftLength, const char16_t *right, int32_t rightLength) {
	return compare<Utf16Text>(table, primaries, options, left, leftLength, right, rightLength);
}

int32_t FastLatin::compareUtf8(const uint16_t *table, const uint16_t *primaries, int32_t options,
		const uint8_t *left, int32_t leftLength, const uint8_t *right, int32_t rightLength) {
	return compare<Utf8Text>(table, primaries, options, left, leftLength, right, rightLength);
}

} // namespace sprt::unicode::detail
