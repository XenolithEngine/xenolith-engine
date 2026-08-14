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

// The composing normalizer UTS-46 runs its labels through. Ported from libuidna
// src/u_norm2.{h,cc} (ICU normalizer2impl.cpp; © Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// UTS-46 asks the normalizer for exactly four things - normalize(), append-and-
// normalize, isNormalized() and getCombiningClass() - so only the composing half
// is here. Dropped from the ICU original: every UTF-8 overload (UTS-46 works in
// UTF-16 throughout, so composeUTF8/decomposeUTF8 are dead code), makeFCD, the
// canonical-iterator data, getRawDecomposition, quickCheck, and the split between
// Normalizer2Impl and Normalizer2WithImpl.
//
// The norm16 arithmetic below is transcribed from ICU literally. It is dense and
// entirely non-obvious, and every threshold comparison encodes a fact about the
// data layout - a "cleanup" here does not fail loudly, it just returns a slightly
// wrong normalization for a few thousand code points.
//
// The data no longer arrives as a serialized blob: gen-tables.py split it at build
// time (see data/README.adoc), so there is no header parsing, no allocation, no
// lazy initialization and no error state - the normalizer is a constant.

namespace sprt::idn::detail {

// The norm16 trie. gen-tables.py already split the serialized container, so this is
// a plain constant - no header parsing, no allocation, no failure mode.
static constexpr UcpTrie s_normTrie{
	s_normTrieIndex,
	s_normTrieData,
	s_normTrieDataLength,
	s_normTrieHighStart,
};

static_assert(s_normTrieIndexLength == int32_t(sizeof(s_normTrieIndex) / sizeof(uint16_t)),
		"normalization trie index length disagrees with the generated array");
static_assert(s_normTrieDataLength == int32_t(sizeof(s_normTrieData) / sizeof(uint16_t)),
		"normalization trie data length disagrees with the generated array");

class Normalizer2Impl {
public:
	// Fixed norm16 values.
	enum : uint16_t {
		MinYesYesWithCc = 0xFE02, // MIN_YES_YES_WITH_CC
		JamoVt = 0xFE00, // JAMO_VT
		MinNormalMaybeYes = 0xFC00, // MIN_NORMAL_MAYBE_YES
		JamoL = 2, // offset=1 hasCompBoundaryAfter=false
		Inert = 1, // offset=0 hasCompBoundaryAfter=true
	};

	enum : uint16_t {
		HasCompBoundaryAfter = 1, // norm16 bit 0
		OffsetShift = 1,

		// For algorithmic one-way mappings, norm16 bits 2..1 hold the tccc (0, 1, >1)
		// for the quick FCC boundary-after test.
		DeltaTcccG0 = 0,
		DeltaTccc1 = 2,
		DeltaTcccGt1 = 4,
		DeltaTcccMask = 6,
		DeltaShift = 3,

		MaxDelta = 0x40,
	};

	enum : uint16_t {
		MappingHasCccLcccWord = 0x80,
		MappingLengthMask = 0x1F,
	};

	enum : uint16_t {
		Comp1LastTuple = 0x8000,
		Comp1Triple = 1,
		Comp1TrailLimit = 0x3400,
		Comp1TrailMask = 0x7FFE,
		Comp1TrailShift = 9, // 10-1 for the "triple" bit
		Comp2TrailShift = 6,
		Comp2TrailMask = 0xFFC0,
	};

	// --- low-level property access -------------------------------------------

	// The trie stores values for lead surrogate code UNITS; surrogate code POINTS
	// are inert.
	uint16_t getNorm16(char32_t c) const {
		return unicode::isUtf16HighSurrogate(char16_t(c)) && c <= 0xFFFF ? uint16_t(Inert)
																		 : s_normTrie.get(c);
	}

	uint16_t getRawNorm16(char32_t c) const { return s_normTrie.get(c); }

	bool isAlgorithmicNoNo(uint16_t norm16) const {
		return s_normLimitNoNo <= norm16 && norm16 < s_normMinMaybeYes;
	}
	bool isDecompYes(uint16_t norm16) const {
		return norm16 < s_normMinYesNo || s_normMinMaybeYes <= norm16;
	}

	uint8_t getCC(uint16_t norm16) const {
		if (norm16 >= MinNormalMaybeYes) {
			return getCCFromNormalYesOrMaybe(norm16);
		}
		if (norm16 < s_normMinNoNo || s_normLimitNoNo <= norm16) {
			return 0;
		}
		return getCCFromNoNo(norm16);
	}

	static uint8_t getCCFromNormalYesOrMaybe(uint16_t norm16) {
		return uint8_t(norm16 >> OffsetShift);
	}
	static uint8_t getCCFromYesOrMaybe(uint16_t norm16) {
		return norm16 >= MinNormalMaybeYes ? getCCFromNormalYesOrMaybe(norm16) : 0;
	}
	uint8_t getCCFromYesOrMaybeCP(char32_t c) const {
		if (c < s_normMinCompNoMaybeCp) {
			return 0;
		}
		return getCCFromYesOrMaybe(getNorm16(c));
	}

	// lccc(c) in bits 15..8, tccc(c) in bits 7..0.
	uint16_t getFCD16(char32_t c) const {
		if (c < s_normMinDecompNoCp) {
			return 0;
		} else if (c <= 0xFFFF) {
			if (!singleLeadMightHaveNonZeroFCD16(c)) {
				return 0;
			}
		}
		return getFCD16FromNormData(c);
	}

	bool singleLeadMightHaveNonZeroFCD16(char32_t lead) const {
		// 0 <= lead <= 0xffff
		uint8_t bits = s_normSmallFcd[lead >> 8];
		if (bits == 0) {
			return false;
		}
		return ((bits >> ((lead >> 5) & 7)) & 1) != 0;
	}

	uint16_t getFCD16FromNormData(char32_t c) const;

	// --- higher-level ---------------------------------------------------------

	// doCompose == false turns this into isNormalized(): it returns false at the
	// first difference instead of writing anything.
	bool compose(const char16_t *src, const char16_t *limit, bool onlyContiguous, bool doCompose,
			ReorderingBuffer &buffer) const;

	bool composeAndAppend(const char16_t *src, const char16_t *limit, bool doCompose,
			bool onlyContiguous, ReorderingBuffer &buffer) const;

	bool hasCompBoundaryBefore(char32_t c) const {
		return c < s_normMinCompNoMaybeCp || norm16HasCompBoundaryBefore(getNorm16(c));
	}

private:
	friend class ReorderingBuffer;

	bool isMaybe(uint16_t norm16) const { return s_normMinMaybeYes <= norm16 && norm16 <= JamoVt; }
	bool isMaybeOrNonZeroCC(uint16_t norm16) const { return norm16 >= s_normMinMaybeYes; }
	static bool isInert(uint16_t norm16) { return norm16 == Inert; }
	static bool isJamoVT(uint16_t norm16) { return norm16 == JamoVt; }
	uint16_t hangulLVT() const { return s_normMinYesNoMappingsOnly | HasCompBoundaryAfter; }
	bool isHangulLV(uint16_t norm16) const { return norm16 == s_normMinYesNo; }
	bool isHangulLVT(uint16_t norm16) const { return norm16 == hangulLVT(); }
	bool isCompYesAndZeroCC(uint16_t norm16) const { return norm16 < s_normMinNoNo; }
	bool isDecompNoAlgorithmic(uint16_t norm16) const { return norm16 >= s_normLimitNoNo; }

	uint8_t getCCFromNoNo(uint16_t norm16) const {
		const uint16_t *mapping = getMapping(norm16);
		if (*mapping & MappingHasCccLcccWord) {
			return uint8_t(*(mapping - 1));
		} else {
			return 0;
		}
	}

	// Requires algorithmic-NoNo.
	char32_t mapAlgorithmic(char32_t c, uint16_t norm16) const {
		return c + (norm16 >> DeltaShift) - centerNoNoDelta();
	}

	// The ICU init() computed this once into a member; with constant data it is a
	// constant expression. (minMaybeYes >> DELTA_SHIFT) - MAX_DELTA - 1.
	static constexpr uint16_t centerNoNoDelta() {
		return uint16_t((s_normMinMaybeYes >> DeltaShift) - MaxDelta - 1);
	}

	// Requires minYesNo < norm16 < limitNoNo.
	const uint16_t *getMapping(uint16_t norm16) const {
		return extraData() + (norm16 >> OffsetShift);
	}

	// ICU's init(): extraData = maybeYesCompositions
	//                         + ((MIN_NORMAL_MAYBE_YES - minMaybeYes) >> OFFSET_SHIFT)
	static constexpr const uint16_t *maybeYesCompositions() { return s_normExtraData; }
	static constexpr const uint16_t *extraData() {
		return s_normExtraData + ((MinNormalMaybeYes - s_normMinMaybeYes) >> OffsetShift);
	}

	const uint16_t *getCompositionsListForDecompYes(uint16_t norm16) const {
		if (norm16 < JamoL || MinNormalMaybeYes <= norm16) {
			return nullptr;
		} else if (norm16 < s_normMinMaybeYes) {
			return getMapping(norm16); // for yesYes; a Jamo L gets a harmless empty list
		} else {
			return maybeYesCompositions() + norm16 - s_normMinMaybeYes;
		}
	}

	const uint16_t *getCompositionsListForComposite(uint16_t norm16) const {
		// A composite has both a mapping and a compositions list.
		const uint16_t *list = getMapping(norm16);
		return list + 1 // skip the first unit, which holds the mapping length
				+ (*list & MappingLengthMask);
	}

	uint8_t getPreviousTrailCC(const char16_t *start, const char16_t *p) const;

	const char16_t *decomposeShort(const char16_t *src, const char16_t *limit,
			bool stopAtCompBoundary, bool onlyContiguous, ReorderingBuffer &buffer, bool &ok) const;

	bool decompose(char32_t c, uint16_t norm16, ReorderingBuffer &buffer) const;

	static int32_t combine(const uint16_t *list, char32_t trail);

	void recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
			bool onlyContiguous) const;

	bool hasCompBoundaryBefore(char32_t c, uint16_t norm16) const {
		return c < s_normMinCompNoMaybeCp || norm16HasCompBoundaryBefore(norm16);
	}
	bool norm16HasCompBoundaryBefore(uint16_t norm16) const {
		return norm16 < s_normMinNoNoCompNoMaybeCc || isAlgorithmicNoNo(norm16);
	}
	bool hasCompBoundaryBefore(const char16_t *src, const char16_t *limit) const;
	bool hasCompBoundaryAfter(const char16_t *start, const char16_t *p, bool onlyContiguous) const;
	bool norm16HasCompBoundaryAfter(uint16_t norm16, bool onlyContiguous) const {
		return (norm16 & HasCompBoundaryAfter) != 0
				&& (!onlyContiguous || isTrailCC01ForCompBoundaryAfter(norm16));
	}
	// For FCC: given norm16 has HAS_COMP_BOUNDARY_AFTER, does it have tccc <= 1?
	bool isTrailCC01ForCompBoundaryAfter(uint16_t norm16) const {
		return isInert(norm16)
				? true
				: (isDecompNoAlgorithmic(norm16) ? (norm16 & DeltaTcccMask) <= DeltaTccc1
												 : *getMapping(norm16) <= 0x1FF);
	}

	const char16_t *findPreviousCompBoundary(const char16_t *start, const char16_t *p,
			bool onlyContiguous) const;
	const char16_t *findNextCompBoundary(const char16_t *p, const char16_t *limit,
			bool onlyContiguous) const;
};

// Korean Hangul and Jamo constants and the algorithmic (de)composition built on
// them. Ported from libuidna src/u_edits.h.
class Hangul {
public:
	enum : char32_t {
		JamoLBase = 0x1100, // "lead" jamo
		JamoVBase = 0x1161, // "vowel" jamo
		JamoTBase = 0x11A7, // "trail" jamo

		HangulBase = 0xAC00,

		JamoLCount = 19,
		JamoVCount = 21,
		JamoTCount = 28,

		HangulCount = JamoLCount * JamoVCount * JamoTCount,
		HangulLimit = HangulBase + HangulCount,
	};

	static bool isHangulLV(char32_t c) {
		c -= HangulBase;
		return c < HangulCount && c % JamoTCount == 0;
	}

	// Decomposes a Hangul syllable into `buffer`; returns the length, 2 or 3.
	static int32_t decompose(char32_t c, char16_t buffer[3]) {
		c -= HangulBase;
		char32_t c2 = c % JamoTCount;
		c /= JamoTCount;
		buffer[0] = char16_t(JamoLBase + c / JamoVCount);
		buffer[1] = char16_t(JamoVBase + c % JamoVCount);
		if (c2 == 0) {
			return 2;
		}
		buffer[2] = char16_t(JamoTBase + c2);
		return 3;
	}
};

// The one normalizer instance: UTS-46 mapping plus NFC composition, non-contiguous
// (NFC, not FCC). Constant data means this is a constant too - no getInstance(), no
// thread-safe-static guard, no initialization order to reason about.
static constexpr Normalizer2Impl s_norm2{};
static constexpr bool s_normOnlyContiguous = false;

// --- ReorderingBuffer methods that need Normalizer2Impl ----------------------

bool ReorderingBuffer::init(int32_t destCapacity) {
	int32_t length = str.size();
	if (!str.reserve(destCapacity > length ? destCapacity : length)) {
		return false;
	}
	start = str.data();
	limit = start + length;
	remainingCapacity = str.capacity() - length;
	reorderStart = start;
	if (start == limit) {
		lastCC = 0;
	} else {
		setIterator();
		lastCC = previousCC();
		// Set reorderStart after the last code point with cc <= 1, if there is one.
		if (lastCC > 1) {
			while (previousCC() > 1) { }
		}
		reorderStart = codePointLimit;
	}
	return true;
}

void ReorderingBuffer::skipPrevious() {
	codePointLimit = codePointStart;
	char16_t c = *--codePointStart;
	if (unicode::isUtf16LowSurrogate(c) && start < codePointStart
			&& unicode::isUtf16HighSurrogate(*(codePointStart - 1))) {
		--codePointStart;
	}
}

uint8_t ReorderingBuffer::previousCC() {
	codePointLimit = codePointStart;
	if (reorderStart >= codePointStart) {
		return 0;
	}
	char32_t c = *--codePointStart;
	if (unicode::isUtf16LowSurrogate(char16_t(c)) && start < codePointStart
			&& unicode::isUtf16HighSurrogate(*(codePointStart - 1))) {
		--codePointStart;
		c = unicode::utf16CombineSurrogates(*codePointStart, char16_t(c));
	}
	return impl.getCCFromYesOrMaybeCP(c);
}

bool ReorderingBuffer::appendSupplementary(char32_t c, uint8_t cc) {
	if (remainingCapacity < 2 && !resize(2)) {
		return false;
	}
	if (lastCC <= cc || cc == 0) {
		unicode::utf16EncodeBuf(limit, 2, c);
		limit += 2;
		lastCC = cc;
		if (cc <= 1) {
			reorderStart = limit;
		}
	} else {
		insert(c, cc);
	}
	remainingCapacity -= 2;
	return true;
}

bool ReorderingBuffer::append(const char16_t *s, int32_t length, bool isNfd, uint8_t leadCC,
		uint8_t trailCC) {
	if (length == 0) {
		return true;
	}
	if (remainingCapacity < length && !resize(length)) {
		return false;
	}
	remainingCapacity -= length;
	if (lastCC <= leadCC || leadCC == 0) {
		if (trailCC <= 1) {
			reorderStart = limit + length;
		} else if (leadCC <= 1) {
			reorderStart = limit + 1; // ok if not a code point boundary
		}
		const char16_t *sLimit = s + length;
		do { *limit++ = *s++; } while (s != sLimit);
		lastCC = trailCC;
	} else {
		int32_t i = 0;
		char32_t c;
		uint8_t offset;
		c = unicode::utf16Decode32(s + i, size_t(length - i), offset);
		i += offset;
		insert(c, leadCC); // insert the first code point
		while (i < length) {
			c = unicode::utf16Decode32(s + i, size_t(length - i), offset);
			i += offset;
			if (i < length) {
				if (isNfd) {
					leadCC = Normalizer2Impl::getCCFromYesOrMaybe(impl.getRawNorm16(c));
				} else {
					leadCC = impl.getCC(impl.getNorm16(c));
				}
			} else {
				leadCC = trailCC;
			}
			if (!append(c, leadCC)) {
				return false;
			}
		}
	}
	return true;
}

bool ReorderingBuffer::appendZeroCC(char32_t c) {
	int32_t cpLength = c <= 0xFFFF ? 1 : 2;
	if (remainingCapacity < cpLength && !resize(cpLength)) {
		return false;
	}
	remainingCapacity -= cpLength;
	if (cpLength == 1) {
		*limit++ = char16_t(c);
	} else {
		unicode::utf16EncodeBuf(limit, 2, c);
		limit += 2;
	}
	lastCC = 0;
	reorderStart = limit;
	return true;
}

bool ReorderingBuffer::appendZeroCC(const char16_t *s, const char16_t *sLimit) {
	if (s == sLimit) {
		return true;
	}
	int32_t length = int32_t(sLimit - s);
	if (remainingCapacity < length && !resize(length)) {
		return false;
	}
	::__sprt_memcpy(limit, s, size_t(length) * sizeof(char16_t));
	limit += length;
	remainingCapacity -= length;
	lastCC = 0;
	reorderStart = limit;
	return true;
}

void ReorderingBuffer::remove() {
	reorderStart = limit = start;
	remainingCapacity = str.capacity();
	lastCC = 0;
}

void ReorderingBuffer::removeSuffix(int32_t suffixLength) {
	if (suffixLength < (limit - start)) {
		limit -= suffixLength;
		remainingCapacity += suffixLength;
	} else {
		limit = start;
		remainingCapacity = str.capacity();
	}
	lastCC = 0;
	reorderStart = limit;
}

bool ReorderingBuffer::resize(int32_t appendLength) {
	int32_t reorderStartIndex = int32_t(reorderStart - start);
	int32_t length = int32_t(limit - start);
	int32_t newCapacity = length + appendLength;
	int32_t doubleCapacity = 2 * str.capacity();
	if (newCapacity < doubleCapacity) {
		newCapacity = doubleCapacity;
	}
	if (newCapacity < 256) {
		newCapacity = 256;
	}
	// Commit the length first: reserve() only preserves [0, size).
	str.setSize(length);
	if (!str.reserve(newCapacity)) {
		return false;
	}
	// reserve() may have moved the storage - every pointer is re-derived.
	start = str.data();
	reorderStart = start + reorderStartIndex;
	limit = start + length;
	remainingCapacity = str.capacity() - length;
	return true;
}

bool ReorderingBuffer::equals(const char16_t *otherStart, const char16_t *otherLimit) const {
	int32_t length = int32_t(limit - start);
	return length == int32_t(otherLimit - otherStart)
			&& 0 == ::__sprt_memcmp(start, otherStart, size_t(length) * sizeof(char16_t));
}

void ReorderingBuffer::insert(char32_t c, uint8_t cc) {
	for (setIterator(), skipPrevious(); previousCC() > cc;) { }
	// insert c at codePointLimit, after the character with prevCC <= cc
	char16_t *q = limit;
	char16_t *r = limit += (c <= 0xFFFF ? 1 : 2);
	do { *--r = *--q; } while (codePointLimit != q);
	writeCodePoint(q, c);
	if (cc <= 1) {
		reorderStart = r;
	}
}

// --- Normalizer2Impl ---------------------------------------------------------

uint16_t Normalizer2Impl::getFCD16FromNormData(char32_t c) const {
	uint16_t norm16 = getNorm16(c);
	if (norm16 >= s_normLimitNoNo) {
		if (norm16 >= MinNormalMaybeYes) {
			// combining mark
			norm16 = getCCFromNormalYesOrMaybe(norm16);
			return uint16_t(norm16 | (norm16 << 8));
		} else if (norm16 >= s_normMinMaybeYes) {
			return 0;
		} else { // isDecompNoAlgorithmic(norm16)
			uint16_t deltaTrailCC = norm16 & DeltaTcccMask;
			if (deltaTrailCC <= DeltaTccc1) {
				return uint16_t(deltaTrailCC >> OffsetShift);
			}
			// Maps to an isCompYesAndZeroCC.
			c = mapAlgorithmic(c, norm16);
			norm16 = getRawNorm16(c);
		}
	}
	if (norm16 <= s_normMinYesNo || isHangulLVT(norm16)) {
		// no decomposition or Hangul syllable, all zeros
		return 0;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t *mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	norm16 = uint16_t(firstUnit >> 8); // tccc
	if (firstUnit & MappingHasCccLcccWord) {
		norm16 = uint16_t(norm16 | (*(mapping - 1) & 0xFF00)); // lccc
	}
	return norm16;
}

bool Normalizer2Impl::hasCompBoundaryBefore(const char16_t *src, const char16_t *limit) const {
	if (src == limit || *src < s_normMinCompNoMaybeCp) {
		return true;
	}
	char32_t c;
	uint16_t norm16 = ucpTrieNext(s_normTrie, src, limit, c);
	return norm16HasCompBoundaryBefore(norm16);
}

bool Normalizer2Impl::hasCompBoundaryAfter(const char16_t *start, const char16_t *p,
		bool onlyContiguous) const {
	if (start == p) {
		return true;
	}
	char32_t c;
	uint16_t norm16 = ucpTriePrev(s_normTrie, start, p, c);
	return norm16HasCompBoundaryAfter(norm16, onlyContiguous);
}

uint8_t Normalizer2Impl::getPreviousTrailCC(const char16_t *start, const char16_t *p) const {
	if (start == p) {
		return 0;
	}
	// U16_PREV: step back over one code point, pairing surrogates when they pair.
	char32_t c = *--p;
	if (unicode::isUtf16LowSurrogate(char16_t(c)) && start < p
			&& unicode::isUtf16HighSurrogate(*(p - 1))) {
		c = unicode::utf16CombineSurrogates(*(p - 1), char16_t(c));
	}
	return uint8_t(getFCD16(c));
}

const char16_t *Normalizer2Impl::decomposeShort(const char16_t *src, const char16_t *limit,
		bool stopAtCompBoundary, bool onlyContiguous, ReorderingBuffer &buffer, bool &ok) const {
	if (!ok) {
		return nullptr;
	}
	while (src < limit) {
		if (stopAtCompBoundary && *src < s_normMinCompNoMaybeCp) {
			return src;
		}
		const char16_t *prevSrc = src;
		char32_t c;
		uint16_t norm16 = ucpTrieNext(s_normTrie, src, limit, c);
		if (stopAtCompBoundary && norm16HasCompBoundaryBefore(norm16)) {
			return prevSrc;
		}
		if (!decompose(c, norm16, buffer)) {
			ok = false;
			return nullptr;
		}
		if (stopAtCompBoundary && norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return src;
		}
	}
	return src;
}

bool Normalizer2Impl::decompose(char32_t c, uint16_t norm16, ReorderingBuffer &buffer) const {
	// get the decomposition and the lead and trail cc's
	if (norm16 >= s_normLimitNoNo) {
		if (isMaybeOrNonZeroCC(norm16)) {
			return buffer.append(c, getCCFromYesOrMaybe(norm16));
		}
		// Maps to an isCompYesAndZeroCC.
		c = mapAlgorithmic(c, norm16);
		norm16 = getRawNorm16(c);
	}
	if (norm16 < s_normMinYesNo) {
		// c does not decompose
		return buffer.append(c, 0);
	} else if (isHangulLV(norm16) || isHangulLVT(norm16)) {
		// Hangul syllable: decompose algorithmically
		char16_t jamos[3];
		return buffer.appendZeroCC(jamos, jamos + Hangul::decompose(c, jamos));
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t *mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	int32_t length = firstUnit & MappingLengthMask;
	uint8_t trailCC = uint8_t(firstUnit >> 8);
	uint8_t leadCC = (firstUnit & MappingHasCccLcccWord) ? uint8_t(*(mapping - 1) >> 8) : 0;
	return buffer.append(reinterpret_cast<const char16_t *>(mapping) + 1, length, true, leadCC,
			trailCC);
}

int32_t Normalizer2Impl::combine(const uint16_t *list, char32_t trail) {
	uint16_t key1, firstUnit;
	if (trail < Comp1TrailLimit) {
		// trail character is 0..33FF; the result entry may have 2 or 3 units
		key1 = uint16_t(trail << 1);
		while (key1 > (firstUnit = *list)) { list += 2 + (firstUnit & Comp1Triple); }
		if (key1 == (firstUnit & Comp1TrailMask)) {
			if (firstUnit & Comp1Triple) {
				return (int32_t(list[1]) << 16) | list[2];
			} else {
				return list[1];
			}
		}
	} else {
		// trail character is 3400..10FFFF; the result entry has 3 units
		key1 = uint16_t(Comp1TrailLimit + (((trail >> Comp1TrailShift)) & ~Comp1Triple));
		uint16_t key2 = uint16_t(trail << Comp2TrailShift);
		uint16_t secondUnit;
		for (;;) {
			if (key1 > (firstUnit = *list)) {
				list += 2 + (firstUnit & Comp1Triple);
			} else if (key1 == (firstUnit & Comp1TrailMask)) {
				if (key2 > (secondUnit = list[1])) {
					if (firstUnit & Comp1LastTuple) {
						break;
					} else {
						list += 3;
					}
				} else if (key2 == (secondUnit & Comp2TrailMask)) {
					return (int32_t(secondUnit & ~Comp2TrailMask) << 16) | list[2];
				} else {
					break;
				}
			} else {
				break;
			}
		}
	}
	return -1;
}

void Normalizer2Impl::recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
		bool onlyContiguous) const {
	char16_t *p = buffer.getStart() + recomposeStartIndex;
	char16_t *limit = buffer.getLimit();
	if (p == limit) {
		return;
	}

	char16_t *starter, *pRemove, *q, *r;
	const uint16_t *compositionsList;
	char32_t c;
	int32_t compositeAndFwd;
	uint16_t norm16;
	uint8_t cc, prevCC;
	bool starterIsSupplementary;

	// Only initialized here to keep the compiler quiet; not used until we have a
	// forward-combining starter.
	compositionsList = nullptr; // also the "do we have such a starter" flag
	starter = nullptr;
	starterIsSupplementary = false;
	prevCC = 0;

	for (;;) {
		{
			const char16_t *cp = p;
			norm16 = ucpTrieNext(s_normTrie, cp, limit, c);
			p = const_cast<char16_t *>(cp);
		}
		cc = getCCFromYesOrMaybe(norm16);
		if ( // this character combines backward and
				isMaybe(norm16) &&
				// we have seen a starter that combines forward and
				compositionsList != nullptr &&
				// the backward-combining character is not blocked
				(prevCC < cc || prevCC == 0)) {
			if (isJamoVT(norm16)) {
				// c is a Jamo V/T; see whether it composes with the previous character
				if (c < Hangul::JamoTBase) {
					// Jamo Vowel: compose with the previous Jamo L and a following Jamo T
					char16_t prev = char16_t(*starter - Hangul::JamoLBase);
					if (prev < Hangul::JamoLCount) {
						pRemove = p - 1;
						char16_t syllable = char16_t(Hangul::HangulBase
								+ (prev * Hangul::JamoVCount + (c - Hangul::JamoVBase))
										* Hangul::JamoTCount);
						char16_t t;
						if (p != limit
								&& (t = char16_t(*p - Hangul::JamoTBase)) < Hangul::JamoTCount) {
							++p;
							syllable += t; // the next character was a Jamo T
						}
						*starter = syllable;
						// remove the Jamo V/T
						q = pRemove;
						r = p;
						while (r < limit) { *q++ = *r++; }
						limit = q;
						p = pRemove;
					}
				}
				/*
				 * No "else" for Jamo T: the input is in NFD, so there are no Hangul LV
				 * syllables a Jamo T could combine with - every Jamo T is handled above,
				 * with its Jamo V.
				 */
				if (p == limit) {
					break;
				}
				compositionsList = nullptr;
				continue;
			} else if ((compositeAndFwd = combine(compositionsList, c)) >= 0) {
				// The starter and the combining mark (c) do combine.
				char32_t composite = char32_t(compositeAndFwd >> 1);

				// Replace the starter with the composite, remove the combining mark.
				// pRemove and p bracket the combining mark.
				pRemove = p - (c <= 0xFFFF ? 1 : 2);
				if (starterIsSupplementary) {
					if (composite > 0xFFFF) {
						// both are supplementary
						unicode::utf16EncodeBuf(starter, 2, composite);
					} else {
						*starter = char16_t(composite);
						// The composite is shorter than the starter: move the
						// intermediate characters forward one.
						starterIsSupplementary = false;
						q = starter + 1;
						r = q + 1;
						while (r < pRemove) { *q++ = *r++; }
						--pRemove;
					}
				} else if (composite > 0xFFFF) {
					// The composite is longer than the starter: move the intermediate
					// characters back one.
					starterIsSupplementary = true;
					++starter; // temporarily increment for the loop boundary
					q = pRemove;
					r = ++pRemove;
					while (starter < q) { *--r = *--q; }
					char16_t pair[2];
					unicode::utf16EncodeBuf(pair, 2, composite);
					*starter = pair[1];
					*--starter = pair[0]; // undo the temporary increment
				} else {
					// both are on the BMP
					*starter = char16_t(composite);
				}

				// remove the combining mark by moving the following text over it
				if (pRemove < p) {
					q = pRemove;
					r = p;
					while (r < limit) { *q++ = *r++; }
					limit = q;
					p = pRemove;
				}
				// Keep prevCC because we removed the combining mark.

				if (p == limit) {
					break;
				}
				// Is the composite a starter that combines forward?
				if (compositeAndFwd & 1) {
					compositionsList = getCompositionsListForComposite(getRawNorm16(composite));
				} else {
					compositionsList = nullptr;
				}

				// We combined; continue looking for compositions.
				continue;
			}
		}

		// no combination this time
		prevCC = cc;
		if (p == limit) {
			break;
		}

		// If c did not combine, check whether it is a starter.
		if (cc == 0) {
			// Found a new starter.
			if ((compositionsList = getCompositionsListForDecompYes(norm16)) != nullptr) {
				// It may combine with something; prepare for it.
				if (c <= 0xFFFF) {
					starterIsSupplementary = false;
					starter = p - 1;
				} else {
					starterIsSupplementary = true;
					starter = p - 2;
				}
			}
		} else if (onlyContiguous) {
			// FCC: no discontiguous compositions - any intervening character blocks.
			compositionsList = nullptr;
		}
	}
	buffer.setReorderingLimit(limit);
}

bool Normalizer2Impl::compose(const char16_t *src, const char16_t *limit, bool onlyContiguous,
		bool doCompose, ReorderingBuffer &buffer) const {
	// ICU also accepts limit == nullptr for NUL-terminated input and recovers the
	// length itself. Every caller here passes a real limit (a hostname is a counted
	// byte string and may legitimately contain NUL), so that branch is gone along
	// with copyLowPrefixFromNulTerminated().
	const char16_t *prevBoundary = src;
	char32_t minNoMaybeCP = s_normMinCompNoMaybeCp;

	for (;;) {
		// Fast path: scan over characters below the minimum "no or maybe" code point,
		// or with (compYes && ccc == 0) properties.
		const char16_t *prevSrc;
		char32_t c = 0;
		uint16_t norm16 = 0;
		for (;;) {
			if (src == limit) {
				if (prevBoundary != limit && doCompose) {
					if (!buffer.appendZeroCC(prevBoundary, limit)) {
						return false;
					}
				}
				return true;
			}
			if ((c = *src) < minNoMaybeCP || isCompYesAndZeroCC(norm16 = s_normTrie.getBmp(c))) {
				++src;
			} else {
				prevSrc = src++;
				if (!unicode::isUtf16HighSurrogate(char16_t(c))) {
					break;
				} else {
					if (src != limit && unicode::isUtf16LowSurrogate(*src)) {
						c = unicode::utf16CombineSurrogates(char16_t(c), *src);
						++src;
						norm16 = s_normTrie.getSupplementary(c);
						if (!isCompYesAndZeroCC(norm16)) {
							break;
						}
					}
				}
			}
		}
		// isCompYesAndZeroCC(norm16) is false, i.e. norm16 >= minNoNo.
		// The current character is a "noNo" (has a mapping), a "maybeYes" (combines
		// backward), or a "yesYes" with ccc != 0. It is not a Hangul syllable or a
		// Jamo L, because those have "yes" properties.

		// Medium-fast path: cases that need no full decomposition/recomposition.
		if (!isMaybeOrNonZeroCC(norm16)) { // minNoNo <= norm16 < minMaybeYes
			if (!doCompose) {
				return false;
			}
			// Fast path for mapping a character surrounded by boundaries: no need to
			// decompose around it.
			if (isDecompNoAlgorithmic(norm16)) {
				// Maps to a single isCompYesAndZeroCC character, which also implies
				// hasCompBoundaryBefore.
				if (norm16HasCompBoundaryAfter(norm16, onlyContiguous)
						|| hasCompBoundaryBefore(src, limit)) {
					if (prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc)) {
						return false;
					}
					if (!buffer.append(mapAlgorithmic(c, norm16), 0)) {
						return false;
					}
					prevBoundary = src;
					continue;
				}
			} else if (norm16 < s_normMinNoNoCompBoundaryBefore) {
				// The mapping is comp-normalized, which also implies hasCompBoundaryBefore.
				if (norm16HasCompBoundaryAfter(norm16, onlyContiguous)
						|| hasCompBoundaryBefore(src, limit)) {
					if (prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc)) {
						return false;
					}
					auto mapping = reinterpret_cast<const char16_t *>(getMapping(norm16));
					int32_t length = *mapping++ & MappingLengthMask;
					if (!buffer.appendZeroCC(mapping, mapping + length)) {
						return false;
					}
					prevBoundary = src;
					continue;
				}
			} else if (norm16 >= s_normMinNoNoEmpty) {
				// The current character maps to nothing. Omit it from the output if
				// there is a boundary before _or_ after it; the character itself
				// implies no boundaries.
				if (hasCompBoundaryBefore(src, limit)
						|| hasCompBoundaryAfter(prevBoundary, prevSrc, onlyContiguous)) {
					if (prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc)) {
						return false;
					}
					prevBoundary = src;
					continue;
				}
			}
			// Some other "noNo", or we need to look at more text around this
			// character: fall through to the slow path.
		} else if (isJamoVT(norm16) && prevBoundary != prevSrc) {
			char16_t prev = *(prevSrc - 1);
			if (c < Hangul::JamoTBase) {
				// Jamo Vowel: compose with the previous Jamo L and a following Jamo T.
				char16_t l = char16_t(prev - Hangul::JamoLBase);
				if (l < Hangul::JamoLCount) {
					if (!doCompose) {
						return false;
					}
					int32_t t;
					if (src != limit && 0 < (t = (int32_t(*src) - int32_t(Hangul::JamoTBase)))
							&& t < int32_t(Hangul::JamoTCount)) {
						// The next character is a Jamo T.
						++src;
					} else if (hasCompBoundaryBefore(src, limit)) {
						// No Jamo T follows, not even via decomposition.
						t = 0;
					} else {
						t = -1;
					}
					if (t >= 0) {
						char32_t syllable = Hangul::HangulBase
								+ (l * Hangul::JamoVCount + (c - Hangul::JamoVBase))
										* Hangul::JamoTCount
								+ char32_t(t);
						--prevSrc; // replace the Jamo L as well
						if (prevBoundary != prevSrc
								&& !buffer.appendZeroCC(prevBoundary, prevSrc)) {
							return false;
						}
						if (!buffer.appendBmp(char16_t(syllable), 0)) {
							return false;
						}
						prevBoundary = src;
						continue;
					}
					// L+V+x where x != T drops to the slow path, to decompose and
					// recompose: NFKC can find a normal L and V but a compatibility
					// variant of a T, and handling that here would either complicate
					// this code or misbehave on unusual custom data.
				}
			} else if (Hangul::isHangulLV(prev)) {
				// Jamo Trailing consonant: compose with the previous Hangul LV, which
				// by construction does not contain a Jamo T.
				if (!doCompose) {
					return false;
				}
				char32_t syllable = char32_t(prev) + c - Hangul::JamoTBase;
				--prevSrc; // replace the Hangul LV as well
				if (prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc)) {
					return false;
				}
				if (!buffer.appendBmp(char16_t(syllable), 0)) {
					return false;
				}
				prevBoundary = src;
				continue;
			}
			// No matching context, or the surrounding text may need decomposing first:
			// fall through to the slow path.
		} else if (norm16 > JamoVt) { // norm16 >= MIN_YES_YES_WITH_CC
			// One or more combining marks that do not combine backward: check
			// canonical order, and copy unchanged if it holds and a character with a
			// boundary-before follows.
			uint8_t cc = getCCFromNormalYesOrMaybe(norm16); // cc != 0
			if (onlyContiguous /* FCC */ && getPreviousTrailCC(prevBoundary, prevSrc) > cc) {
				// Fails the FCD test: decompose and contiguously recompose.
				if (!doCompose) {
					return false;
				}
			} else {
				// When not FCC we ignore the tccc of the previous character, which
				// passed the "yes && ccc == 0" quick check.
				const char16_t *nextSrc;
				uint16_t n16;
				for (;;) {
					if (src == limit) {
						if (doCompose) {
							if (!buffer.appendZeroCC(prevBoundary, limit)) {
								return false;
							}
						}
						return true;
					}
					uint8_t prevCC = cc;
					nextSrc = src;
					char32_t nextC;
					n16 = ucpTrieNext(s_normTrie, nextSrc, limit, nextC);
					c = nextC;
					if (n16 >= MinYesYesWithCc) {
						cc = getCCFromNormalYesOrMaybe(n16);
						if (prevCC > cc) {
							if (!doCompose) {
								return false;
							}
							break;
						}
					} else {
						break;
					}
					src = nextSrc;
				}
				// src is after the last in-order combining mark. If there is a boundary
				// here, continue with no change.
				if (norm16HasCompBoundaryBefore(n16)) {
					if (isCompYesAndZeroCC(n16)) {
						src = nextSrc;
					}
					continue;
				}
				// Use the slow path: there is no boundary in [prevSrc, src).
			}
		}

		// Slow path: find the nearest boundaries around the current character,
		// decompose, and recompose.
		if (prevBoundary != prevSrc && !norm16HasCompBoundaryBefore(norm16)) {
			const char16_t *p = prevSrc;
			norm16 = ucpTriePrev(s_normTrie, prevBoundary, p, c);
			if (!norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
				prevSrc = p;
			}
		}
		if (doCompose && prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc)) {
			return false;
		}
		int32_t recomposeStartIndex = buffer.length();
		bool ok = true;
		// We know there is not a boundary here.
		decomposeShort(prevSrc, src, false /* !stopAtCompBoundary */, onlyContiguous, buffer, ok);
		// Decompose until the next boundary.
		src = decomposeShort(src, limit, true /* stopAtCompBoundary */, onlyContiguous, buffer, ok);
		if (!ok) {
			return false;
		}
		recompose(buffer, recomposeStartIndex, onlyContiguous);
		if (!doCompose) {
			if (!buffer.equals(prevSrc, src)) {
				return false;
			}
			buffer.remove();
		}
		prevBoundary = src;
	}
}

const char16_t *Normalizer2Impl::findPreviousCompBoundary(const char16_t *start, const char16_t *p,
		bool onlyContiguous) const {
	while (p != start) {
		const char16_t *codePointLimit = p;
		char32_t c;
		uint16_t norm16 = ucpTriePrev(s_normTrie, start, p, c);
		if (norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return codePointLimit;
		}
		if (hasCompBoundaryBefore(c, norm16)) {
			return p;
		}
	}
	return p;
}

const char16_t *Normalizer2Impl::findNextCompBoundary(const char16_t *p, const char16_t *limit,
		bool onlyContiguous) const {
	while (p != limit) {
		const char16_t *codePointStart = p;
		char32_t c;
		uint16_t norm16 = ucpTrieNext(s_normTrie, p, limit, c);
		if (hasCompBoundaryBefore(c, norm16)) {
			return codePointStart;
		}
		if (norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return p;
		}
	}
	return p;
}

bool Normalizer2Impl::composeAndAppend(const char16_t *src, const char16_t *limit, bool doCompose,
		bool onlyContiguous, ReorderingBuffer &buffer) const {
	// ICU saves the overwritten suffix of the destination in `safeMiddle` so it can
	// put it back if normalization then fails. Here the only failure is running out
	// of memory, which aborts the whole conversion and discards the buffer, so there
	// is nothing to restore and no partial result anyone can observe.
	if (!buffer.isEmpty()) {
		const char16_t *firstStarterInSrc = findNextCompBoundary(src, limit, onlyContiguous);
		if (src != firstStarterInSrc) {
			const char16_t *lastStarterInDest =
					findPreviousCompBoundary(buffer.getStart(), buffer.getLimit(), onlyContiguous);
			int32_t destSuffixLength = int32_t(buffer.getLimit() - lastStarterInDest);
			Utf16Buffer middle;
			if (!middle.append(lastStarterInDest, destSuffixLength)) {
				return false;
			}
			buffer.removeSuffix(destSuffixLength);
			if (!middle.append(src, int32_t(firstStarterInSrc - src))) {
				return false;
			}
			const char16_t *middleStart = middle.data();
			if (!compose(middleStart, middleStart + middle.size(), onlyContiguous, true, buffer)) {
				return false;
			}
			src = firstStarterInSrc;
		}
	}
	if (doCompose) {
		return compose(src, limit, onlyContiguous, true, buffer);
	}
	return buffer.appendZeroCC(src, limit);
}

// --- the four entry points UTS-46 uses ---------------------------------------

// NFC-normalize `src` into `dest` (which is cleared first).
static bool normalizeUts46(WideStringView src, Utf16Buffer &dest) {
	dest.clear();
	ReorderingBuffer buffer(s_norm2, dest);
	if (!buffer.init(int32_t(src.size()))) {
		return false;
	}
	auto data = src.data();
	if (!s_norm2.compose(data, data + src.size(), s_normOnlyContiguous, true, buffer)) {
		return false;
	}
	buffer.flush();
	return true;
}

// Normalize `second` and append it to `first`, re-normalizing across the seam.
static bool normalizeSecondAndAppendUts46(Utf16Buffer &first, WideStringView second) {
	ReorderingBuffer buffer(s_norm2, first);
	if (!buffer.init(first.size() + int32_t(second.size()))) {
		return false;
	}
	auto data = second.data();
	if (!s_norm2.composeAndAppend(data, data + second.size(), true, s_normOnlyContiguous, buffer)) {
		return false;
	}
	buffer.flush();
	return true;
}

// Is `src` already in the normalized form? Runs compose() in check-only mode, so
// nothing is written and it stops at the first difference.
static bool isNormalizedUts46(WideStringView src) {
	Utf16Buffer temp;
	ReorderingBuffer buffer(s_norm2, temp);
	if (!buffer.init(5)) { // small capacity: only substrings are ever materialized
		return false;
	}
	auto data = src.data();
	return s_norm2.compose(data, data + src.size(), s_normOnlyContiguous, false, buffer);
}

static uint8_t getCombiningClass(char32_t c) { return s_norm2.getCC(s_norm2.getNorm16(c)); }

} // namespace sprt::idn::detail
