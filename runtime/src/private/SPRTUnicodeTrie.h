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

#ifndef CORE_RUNTIME_PRIVATE_SPRTUNICODETRIE_H_
#define CORE_RUNTIME_PRIVATE_SPRTUNICODETRIE_H_

// Code point -> value lookup over ICU's frozen UTrie2 format. Ported from libuidna
// src/u_trie.{h,cc}, itself ICU's utrie2.h (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// Only the reader is here. Everything else in the ICU original - the builder
// (UNewTrie2), serialization, and the UTF-8 stepping macros - is gone: the tables
// are built offline by the generators next to each table.
//
// The index arithmetic below is transcribed from the ICU macros LITERALLY, down to
// the branch order. Do not "simplify" it: every branch computes a different offset,
// and a wrong one returns a perfectly valid value for the wrong code point - no
// crash, no assert, just a handful of characters silently mapped wrong.
//
// This lives in a shared header rather than next to one of its users because two
// unrelated ports read the same format - the UTS-46 engine (runtime/src/idn, over
// the General_Category, Bidi and Script tables) and the case mapper
// (runtime/src/unicode, over ucase_props_trieIndex). Two transcriptions of code
// carrying that warning would drift.

#include <sprt/runtime/unicode.h>

namespace sprt::unicode::detail {

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
// the data through `index + indexLength`; the generated tables are serialized that
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

// A frozen 32-bit-value UTrie2. Same index arithmetic, two differences that both
// come from UTRIE2_GET32 passing `data32` and an asciiOffset of 0 where the
// 16-bit macro passes the index array and `indexLength`:
//
//   the values live in their own array rather than after the index, and
//   an out-of-range code point lands at data32[BadUtf8DataOffset], not at
//   index[indexLength + BadUtf8DataOffset].
//
// utrie2_openFromSerialized computes highValueIndex the same way for both, but
// only adds indexLength to it for 16-bit tries - so the value stored alongside a
// 32-bit trie is already the right index into data32.
struct Utrie2_32 {
	const uint16_t *index;
	const uint32_t *data;
	int32_t indexLength;
	int32_t dataLength;
	char32_t highStart;
	int32_t highValueIndex;

	constexpr int32_t rawIndex(int32_t offset, char32_t c) const {
		return (int32_t(index[offset + (c >> Utrie2Shift2)]) << Utrie2IndexShift)
				+ (c & Utrie2DataMask);
	}

	constexpr int32_t suppIndex(char32_t c) const {
		return (int32_t(index[index[(Utrie2Index1Offset - Utrie2OmittedBmpIndex1Length)
									  + (c >> Utrie2Shift1)]
						+ ((c >> Utrie2Shift2) & Utrie2Index2Mask)])
					   << Utrie2IndexShift)
				+ (c & Utrie2DataMask);
	}

	// _UTRIE2_INDEX_FROM_CP with asciiOffset == 0
	constexpr int32_t indexFromCodepoint(char32_t c) const {
		if (c < 0xD800) {
			return rawIndex(0, c);
		} else if (c <= 0xFFFF) {
			return rawIndex(c <= 0xDBFF ? Utrie2LscpIndex2Offset - (0xD800 >> Utrie2Shift2) : 0, c);
		} else if (c > 0x10'FFFF) {
			return Utrie2BadUtf8DataOffset;
		} else if (c >= highStart) {
			return highValueIndex;
		} else {
			return suppIndex(c);
		}
	}

	// UTRIE2_GET32
	constexpr uint32_t get(char32_t c) const { return data[indexFromCodepoint(c)]; }

	// UTRIE2_GET32_FROM_SUPP: the caller has already established c > 0xFFFF.
	constexpr uint32_t getFromSupplementary(char32_t c) const {
		return data[c >= highStart ? highValueIndex : suppIndex(c)];
	}

	// _UTRIE2_GET_FROM_U16_SINGLE_LEAD: c is a single code unit that is not part
	// of a surrogate pair.
	constexpr uint32_t getFromU16SingleLead(char16_t c) const {
		return data[rawIndex(0, c)];
	}
};

} // namespace sprt::unicode::detail

#endif // CORE_RUNTIME_PRIVATE_SPRTUNICODETRIE_H_
