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

// Code point -> value lookup over the two ICU trie formats the Unicode tables in
// data/ are serialized in. Ported from libuidna src/u_trie.{h,cc}, itself ICU's
// utrie2.h / ucptrie.cpp (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// Only the readers are here. Everything else in the ICU original - the builder
// (UNewTrie2), serialization, and the UTF-8 stepping macros - is gone: the tables
// are built offline (data/gen-tables.py) and this engine walks UTF-16 only.
//
// The index arithmetic below is transcribed from the ICU macros LITERALLY, down to
// the branch order. Do not "simplify" it: every branch computes a different offset,
// and a wrong one returns a perfectly valid value for the wrong code point - no
// crash, no assert, just a handful of characters silently mapped wrong.

namespace sprt::idn::detail {

// --- UTrie2 (used by the char-type, bidi and script tables) ------------------

// ICU utrie2.h shift/mask constants. Names shortened; the ICU spelling is in the
// trailing comment so the transcription can be checked against the original.
enum : int32_t {
	Utrie2Shift1 = 6 + 5, // UTRIE2_SHIFT_1
	Utrie2Shift2 = 5, // UTRIE2_SHIFT_2
	Utrie2OmittedBmpIndex1Length = 0x1'0000 >> Utrie2Shift1, // UTRIE2_OMITTED_BMP_INDEX_1_LENGTH
	Utrie2Index2BlockLength = 1 << (Utrie2Shift1 - Utrie2Shift2), // UTRIE2_INDEX_2_BLOCK_LENGTH
	Utrie2Index2Mask = Utrie2Index2BlockLength - 1, // UTRIE2_INDEX_2_MASK
	Utrie2DataBlockLength = 1 << Utrie2Shift2, // UTRIE2_DATA_BLOCK_LENGTH
	Utrie2DataMask = Utrie2DataBlockLength - 1, // UTRIE2_DATA_MASK
	Utrie2IndexShift = 2, // UTRIE2_INDEX_SHIFT
	Utrie2LscpIndex2Offset = 0x1'0000 >> Utrie2Shift2, // UTRIE2_LSCP_INDEX_2_OFFSET
	Utrie2LscpIndex2Length = 0x400 >> Utrie2Shift2, // UTRIE2_LSCP_INDEX_2_LENGTH
	Utrie2Index2BmpLength = Utrie2LscpIndex2Offset + Utrie2LscpIndex2Length,
	Utrie2Utf8_2bIndex2Offset = Utrie2Index2BmpLength, // UTRIE2_UTF8_2B_INDEX_2_OFFSET
	Utrie2Utf8_2bIndex2Length = 0x800 >> 6, // UTRIE2_UTF8_2B_INDEX_2_LENGTH
	Utrie2Index1Offset = Utrie2Utf8_2bIndex2Offset + Utrie2Utf8_2bIndex2Length,
	Utrie2BadUtf8DataOffset = 0x80, // UTRIE2_BAD_UTF8_DATA_OFFSET
};

// A frozen 16-bit-value UTrie2. ICU keeps index and data in one array and reaches
// the data through `index + indexLength`; the tables in data/ are serialized that
// way, so the layout is kept.
struct Utrie2 {
	const uint16_t *index;
	int32_t indexLength;
	int32_t dataLength;
	char32_t highStart;
	int32_t highValueIndex;

	constexpr const uint16_t *data16() const { return index + indexLength; }

	// _UTRIE2_INDEX_RAW
	constexpr int32_t rawIndex(int32_t offset, char32_t c) const {
		return (int32_t(index[offset + (c >> Utrie2Shift2)]) << Utrie2IndexShift)
				+ (c & Utrie2DataMask);
	}

	// _UTRIE2_INDEX_FROM_SUPP
	constexpr int32_t suppIndex(char32_t c) const {
		return (int32_t(index[index[(Utrie2Index1Offset - Utrie2OmittedBmpIndex1Length)
									  + (c >> Utrie2Shift1)]
						+ ((c >> Utrie2Shift2) & Utrie2Index2Mask)])
					   << Utrie2IndexShift)
				+ (c & Utrie2DataMask);
	}

	// _UTRIE2_INDEX_FROM_CP, with asciiOffset == indexLength (what UTRIE2_GET16 passes)
	constexpr int32_t indexFromCodepoint(char32_t c) const {
		if (c < 0xD800) {
			return rawIndex(0, c);
		} else if (c <= 0xFFFF) {
			return rawIndex(c <= 0xDBFF ? Utrie2LscpIndex2Offset - (0xD800 >> Utrie2Shift2) : 0, c);
		} else if (c > 0x10'FFFF) {
			return indexLength + Utrie2BadUtf8DataOffset;
		} else if (c >= highStart) {
			return highValueIndex;
		} else {
			return suppIndex(c);
		}
	}

	// UTRIE2_GET16
	constexpr uint16_t get(char32_t c) const { return index[indexFromCodepoint(c)]; }
};

// --- UCPTrie (used by the normalization table) -------------------------------

// ICU ucptrie.h constants, fast type with 16-bit values.
enum : int32_t {
	UcpTrieFastShift = 6, // UCPTRIE_FAST_SHIFT
	UcpTrieFastDataBlockLength = 1 << UcpTrieFastShift, // UCPTRIE_FAST_DATA_BLOCK_LENGTH
	UcpTrieFastDataMask = UcpTrieFastDataBlockLength - 1, // UCPTRIE_FAST_DATA_MASK
	UcpTrieErrorValueNegDataOffset = 1, // UCPTRIE_ERROR_VALUE_NEG_DATA_OFFSET
	UcpTrieHighValueNegDataOffset = 2, // UCPTRIE_HIGH_VALUE_NEG_DATA_OFFSET
	UcpTrieBmpIndexLength = 0x1'0000 >> UcpTrieFastShift, // UCPTRIE_BMP_INDEX_LENGTH
	UcpTrieShift3 = 4, // UCPTRIE_SHIFT_3
	UcpTrieShift2 = 5 + UcpTrieShift3, // UCPTRIE_SHIFT_2
	UcpTrieShift1 = 5 + UcpTrieShift2, // UCPTRIE_SHIFT_1
	UcpTrieOmittedBmpIndex1Length = 0x1'0000 >> UcpTrieShift1, // UCPTRIE_OMITTED_BMP_INDEX_1_LENGTH
	UcpTrieIndex2Mask = (1 << (UcpTrieShift1 - UcpTrieShift2)) - 1, // UCPTRIE_INDEX_2_MASK
	UcpTrieIndex3Mask = (1 << (UcpTrieShift2 - UcpTrieShift3)) - 1, // UCPTRIE_INDEX_3_MASK
	UcpTrieSmallDataMask = (1 << UcpTrieShift3) - 1, // UCPTRIE_SMALL_DATA_MASK
};

// A frozen fast UCPTrie with 16-bit values. Unlike ICU's, index and data are two
// separate arrays: gen-tables.py splits the serialized container at generation
// time, so there is no header to skip and no reinterpret_cast to make.
struct UcpTrie {
	const uint16_t *index;
	const uint16_t *data;
	int32_t dataLength;
	char32_t highStart;

	// ucptrie_internalSmallIndex, fast-type branch only (the tables are all fast).
	constexpr int32_t smallIndexOf(char32_t c) const {
		int32_t i1 =
				int32_t(c >> UcpTrieShift1) + UcpTrieBmpIndexLength - UcpTrieOmittedBmpIndex1Length;
		int32_t i3Block = index[int32_t(index[i1]) + ((c >> UcpTrieShift2) & UcpTrieIndex2Mask)];
		int32_t i3 = (c >> UcpTrieShift3) & UcpTrieIndex3Mask;
		int32_t dataBlock;
		if ((i3Block & 0x8000) == 0) {
			// 16-bit indexes
			dataBlock = index[i3Block + i3];
		} else {
			// 18-bit indexes stored in groups of 9 entries per 8 indexes
			i3Block = (i3Block & 0x7FFF) + (i3 & ~7) + (i3 >> 3);
			i3 &= 7;
			dataBlock = (int32_t(index[i3Block++]) << (2 + (2 * i3))) & 0x3'0000;
			dataBlock |= index[i3Block + i3];
		}
		return dataBlock + (c & UcpTrieSmallDataMask);
	}

	// _UCPTRIE_SMALL_INDEX
	constexpr int32_t supplementaryIndex(char32_t c) const {
		return c >= highStart ? dataLength - UcpTrieHighValueNegDataOffset : smallIndexOf(c);
	}

	// _UCPTRIE_FAST_INDEX
	constexpr int32_t fastIndex(char32_t c) const {
		return int32_t(index[c >> UcpTrieFastShift]) + (c & UcpTrieFastDataMask);
	}

	// _UCPTRIE_CP_INDEX with fastMax == 0xffff
	constexpr int32_t indexFromCodepoint(char32_t c) const {
		if (c <= 0xFFFF) {
			return fastIndex(c);
		} else if (c <= 0x10'FFFF) {
			return supplementaryIndex(c);
		} else {
			return dataLength - UcpTrieErrorValueNegDataOffset;
		}
	}

	// UCPTRIE_FAST_GET(.., UCPTRIE_16, ..)
	constexpr uint16_t get(char32_t c) const { return data[indexFromCodepoint(c)]; }

	// UCPTRIE_FAST_BMP_GET - caller guarantees c <= 0xffff
	constexpr uint16_t getBmp(char32_t c) const { return data[fastIndex(c)]; }

	// UCPTRIE_FAST_SUPP_GET - caller guarantees c > 0xffff
	constexpr uint16_t getSupplementary(char32_t c) const { return data[supplementaryIndex(c)]; }

	constexpr uint16_t getErrorValue() const {
		return data[dataLength - UcpTrieErrorValueNegDataOffset];
	}
};

// UCPTRIE_FAST_U16_NEXT: read one code point forward and look it up in one pass.
// An unpaired surrogate yields the trie's error value, as in ICU.
constexpr inline uint16_t ucpTrieNext(const UcpTrie &trie, const char16_t *&src,
		const char16_t *limit, char32_t &c) {
	c = *src++;
	if (!unicode::isUtf16Surrogate(char16_t(c))) {
		return trie.getBmp(c);
	}
	if (unicode::isUtf16HighSurrogate(char16_t(c)) && src != limit
			&& unicode::isUtf16LowSurrogate(*src)) {
		c = unicode::utf16CombineSurrogates(char16_t(c), *src);
		++src;
		return trie.getSupplementary(c);
	}
	return trie.getErrorValue();
}

// UCPTRIE_FAST_U16_PREV: the same, walking backwards.
constexpr inline uint16_t ucpTriePrev(const UcpTrie &trie, const char16_t *start,
		const char16_t *&src, char32_t &c) {
	c = *--src;
	if (!unicode::isUtf16Surrogate(char16_t(c))) {
		return trie.getBmp(c);
	}
	if (unicode::isUtf16LowSurrogate(char16_t(c)) && src != start
			&& unicode::isUtf16HighSurrogate(*(src - 1))) {
		--src;
		c = unicode::utf16CombineSurrogates(*src, char16_t(c));
		return trie.getSupplementary(c);
	}
	return trie.getErrorValue();
}

} // namespace sprt::idn::detail
