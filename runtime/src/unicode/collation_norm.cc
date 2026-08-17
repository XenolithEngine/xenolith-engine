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

// Canonical decomposition (NFD) and the FCD test, for the collation iterator.
//
// Collation compares text in FCD order: a sequence is FCD when no canonical
// reordering could change it, which is weaker than NFD and true of nearly all
// real text. The iterator checks each segment, and normalizes to NFD only the
// segments that fail - so this file is on a cold path, and is written for being
// obviously right rather than for speed. The one hot part, the per-character
// "could this even be a problem" test, is the bit table at the bottom.
//
// This is NOT ICU's normalizer. ICU reads a serialized `norm16` table whose
// arithmetic is dense enough that the UTS-46 port next door
// (runtime/src/idn/uts46/normalizer.cc) carries a warning against touching it.
// That port implements the *composing* half, which is what UTS-46 needs; nothing
// here needs it. What collation needs is NFD, which UAX #15 specifies in three
// paragraphs: replace each character by its canonical decomposition, then sort
// each run of non-starters by combining class, stably. Implementing that against
// tables generated from UnicodeData.txt is shorter than parameterizing 1200
// lines of norm16 arithmetic to serve two disjoint halves, and it cannot drift
// out of step with the other port because it shares nothing with it.
//
// The tables are checked against all 20 034 rows of NormalizationTest.txt at
// generation time, and the C++ below is checked against the same file at run
// time (runtime_normalization_conformance).

namespace sprt::unicode::detail {

// --- Hangul -------------------------------------------------------------------
//
// 11 172 syllables, decomposing arithmetically, which is why they are not in the
// table: they would be four fifths of it.

enum : char32_t {
	HangulBase = 0xAC00,
	JamoLBase = 0x1100,
	JamoVBase = 0x1161,
	JamoTBase = 0x11A7,
	JamoVCount = 21,
	JamoTCount = 28,
	HangulCount = 19 * JamoVCount * JamoTCount, // 11172
};

static constexpr bool isHangul(char32_t c) {
	return HangulBase <= c && c < HangulBase + HangulCount;
}

// Writes the 2 or 3 jamo of a Hangul syllable, and returns how many.
static int32_t decomposeHangul(char32_t c, char16_t *dest) {
	auto index = c - HangulBase;
	dest[0] = char16_t(JamoLBase + index / (JamoVCount * JamoTCount));
	dest[1] = char16_t(JamoVBase + (index % (JamoVCount * JamoTCount)) / JamoTCount);
	auto trail = index % JamoTCount;
	if (trail == 0) {
		return 2;
	}
	dest[2] = char16_t(JamoTBase + trail);
	return 3;
}

// --- FCD ----------------------------------------------------------------------

// lccc in bits 15..8, tccc in bits 7..0: the combining classes of the first and
// last code point of c's canonical decomposition. Zero for the vast majority of
// characters, which is what makes the FCD check cheap.
static uint16_t getFCD16(char32_t c) {
	// Binary search for the last range start at or below c. The table starts at
	// 0, so there is always one.
	int32_t low = 0;
	int32_t high = s_fcdCount - 1;
	while (low < high) {
		auto mid = (low + high + 1) / 2;
		if (s_fcdStarts[mid] <= c) {
			low = mid;
		} else {
			high = mid - 1;
		}
	}
	return s_fcdValues[low];
}

// The fast path, one bit per BMP code unit. Both must be pessimistic rather than
// optimistic: a false positive costs a trip through getFCD16, a false negative
// silently skips the FCD check. See CollationFCD in ICU collationfcd.h for the
// surrogate conventions the generator follows.
static bool hasLccc(char32_t c) {
	// U+0300 is the first character with lccc != 0.
	if (c < 0x300 || c > 0xFFFF) {
		return false;
	}
	auto index = s_fcdLcccIndex[c >> 5];
	return index != 0 && (s_fcdLcccBits[index] & (uint32_t(1) << (c & 0x1F))) != 0;
}

static bool hasTccc(char32_t c) {
	// U+00C0 is the first character with tccc != 0.
	if (c < 0xC0 || c > 0xFFFF) {
		return false;
	}
	auto index = s_fcdTcccIndex[c >> 5];
	return index != 0 && (s_fcdTcccBits[index] & (uint32_t(1) << (c & 0x1F))) != 0;
}

// The same question for a whole code point rather than a code unit, so a
// supplementary character is asked about through its lead surrogate. Takes a
// signed value because callers pass the iterator's sentinel.
static bool mayHaveLccc(int32_t c) {
	if (c < 0x300) { // U+0300 is the first character with lccc != 0
		return false;
	}
	if (c > 0xFFFF) {
		c = 0xD7C0 + (c >> 10); // U16_LEAD
	}
	auto index = s_fcdLcccIndex[c >> 5];
	return index != 0 && (s_fcdLcccBits[index] & (uint32_t(1) << (c & 0x1F))) != 0;
}

// --- decomposition ------------------------------------------------------------

// The canonical decomposition of c, or nullptr if it has none. Hangul is not in
// the table; callers that can see a syllable use decomposeHangul.
static const char16_t *decomposeCanonical(char32_t c, int32_t &length) {
	int32_t low = 0;
	int32_t high = s_nfdCount - 1;
	while (low <= high) {
		auto mid = (low + high) / 2;
		auto value = s_nfdCodepoints[mid];
		if (value < c) {
			low = mid + 1;
		} else if (value > c) {
			high = mid - 1;
		} else {
			length = int32_t(s_nfdOffsets[mid + 1]) - int32_t(s_nfdOffsets[mid]);
			return s_nfdPool + s_nfdOffsets[mid];
		}
	}
	length = 0;
	return nullptr;
}

// The combining class of one code point, for a code point that does not
// decompose. That is the only case canonical ordering ever sees - by then
// everything decomposable is gone - and for such a character lccc, tccc and ccc
// are the same number, so the FCD table answers it without a table of its own.
static uint8_t getCombiningClass(char32_t c) { return uint8_t(getFCD16(c) & 0xFF); }

// A UTF-16 buffer with inline storage. Only the normalizer uses it, and only for
// one FCD segment at a time, so the inline size covers every realistic segment;
// the heap path exists because a segment of combining marks has no bound.
class NormBuffer {
public:
	static constexpr int32_t InlineCapacity = 64;

	NormBuffer() = default;

	~NormBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	NormBuffer(const NormBuffer &) = delete;
	NormBuffer &operator=(const NormBuffer &) = delete;

	const char16_t *data() const { return _data; }
	int32_t size() const { return _size; }
	void clear() { _size = 0; }
	char16_t operator[](int32_t i) const { return _data[i]; }

	bool appendCodePoint(char32_t c) {
		if (c <= 0xFFFF) {
			return append(char16_t(c));
		}
		char16_t pair[2];
		auto n = utf16EncodeBuf(pair, 2, c);
		return append(pair, n);
	}

	// Reverses by code points, keeping surrogate pairs together - what
	// UnicodeString::reverse() does, and what the backward FCD scan needs after
	// collecting a segment from its end.
	void reverseCodePoints() {
		for (int32_t i = 0, j = _size - 1; i < j; ++i, --j) {
			auto tmp = _data[i];
			_data[i] = _data[j];
			_data[j] = tmp;
		}
		for (int32_t i = 0; i + 1 < _size; ++i) {
			if (isUtf16LowSurrogate(_data[i]) && isUtf16HighSurrogate(_data[i + 1])) {
				auto tmp = _data[i];
				_data[i] = _data[i + 1];
				_data[i + 1] = tmp;
				++i;
			}
		}
	}

	bool append(char16_t c) {
		if (_size == _capacity && !grow(_size + 1)) {
			return false;
		}
		_data[_size++] = c;
		return true;
	}

	bool append(const char16_t *s, int32_t n) {
		if (_size + n > _capacity && !grow(_size + n)) {
			return false;
		}
		for (int32_t i = 0; i < n; ++i) {
			_data[_size + i] = s[i];
		}
		_size += n;
		return true;
	}

	// Canonical ordering works in place, on units that are already written.
	char16_t &at(int32_t i) { return _data[i]; }

private:
	bool grow(int32_t needed) {
		auto capacity = _capacity * 2;
		if (capacity < needed) {
			capacity = needed;
		}
		auto buf = reinterpret_cast<char16_t *>(
				::__sprt_malloc(size_t(capacity) * sizeof(char16_t)));
		if (!buf) {
			return false;
		}
		for (int32_t i = 0; i < _size; ++i) {
			buf[i] = _data[i];
		}
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
		_data = buf;
		_capacity = capacity;
		return true;
	}

	char16_t _inlineData[InlineCapacity] = {0};
	char16_t *_data = _inlineData;
	int32_t _size = 0;
	int32_t _capacity = InlineCapacity;
};

// The same buffer over code points. Canonical ordering is defined on characters,
// not on code units, and doing it on code points removes every surrogate-width
// special case from the sort - which is where an in-place UTF-16 reorder goes
// wrong, and where the error is invisible until a supplementary combining mark
// turns up.
class CodepointBuffer {
public:
	static constexpr int32_t InlineCapacity = 64;

	CodepointBuffer() = default;

	~CodepointBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	CodepointBuffer(const CodepointBuffer &) = delete;
	CodepointBuffer &operator=(const CodepointBuffer &) = delete;

	int32_t size() const { return _size; }
	void clear() { _size = 0; }
	char32_t &operator[](int32_t i) { return _data[i]; }
	char32_t at(int32_t i) const { return _data[i]; }

	bool append(char32_t c) {
		if (_size == _capacity && !grow(_size + 1)) {
			return false;
		}
		_data[_size++] = c;
		return true;
	}

private:
	bool grow(int32_t needed) {
		auto capacity = _capacity * 2;
		if (capacity < needed) {
			capacity = needed;
		}
		auto buf = reinterpret_cast<char32_t *>(
				::__sprt_malloc(size_t(capacity) * sizeof(char32_t)));
		if (!buf) {
			return false;
		}
		for (int32_t i = 0; i < _size; ++i) {
			buf[i] = _data[i];
		}
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
		_data = buf;
		_capacity = capacity;
		return true;
	}

	char32_t _inlineData[InlineCapacity] = {0};
	char32_t *_data = _inlineData;
	int32_t _size = 0;
	int32_t _capacity = InlineCapacity;
};

// Canonical ordering, UAX #15 D109. A stable insertion sort: a non-starter moves
// back past any non-starter with a strictly greater combining class and stops at
// the first starter. "Strictly greater" is what makes it stable, and stability is
// not an implementation detail here - equal classes keeping their order is what
// the standard requires.
static void reorderCanonical(CodepointBuffer &buffer) {
	for (int32_t i = 1; i < buffer.size(); ++i) {
		auto cc = getCombiningClass(buffer[i]);
		if (cc == 0) {
			continue;
		}
		int32_t j = i;
		while (j > 0) {
			auto prev = getCombiningClass(buffer[j - 1]);
			if (prev == 0 || prev <= cc) {
				break;
			}
			auto tmp = buffer[j - 1];
			buffer[j - 1] = buffer[j];
			buffer[j] = tmp;
			--j;
		}
	}
}

// NFD of [src, src + length), written to `buffer` as UTF-16. Returns false only
// if a buffer could not grow.
static bool normalizeNfd(const char16_t *src, int32_t length, NormBuffer &buffer,
		CodepointBuffer &scratch) {
	scratch.clear();
	int32_t index = 0;
	while (index < length) {
		char32_t c = src[index++];
		if (isUtf16HighSurrogate(char16_t(c)) && index < length
				&& isUtf16LowSurrogate(src[index])) {
			c = utf16CombineSurrogates(char16_t(c), src[index]);
			++index;
		}

		if (isHangul(c)) {
			char16_t jamo[3];
			auto count = decomposeHangul(c, jamo);
			for (int32_t i = 0; i < count; ++i) {
				if (!scratch.append(jamo[i])) {
					return false;
				}
			}
			continue;
		}

		int32_t decomposedLength = 0;
		auto decomposed = decomposeCanonical(c, decomposedLength);
		if (decomposed == nullptr) {
			if (!scratch.append(c)) {
				return false;
			}
			continue;
		}
		// The pool is UTF-16; a decomposition may contain a supplementary
		// character (the Osage and musical ones do).
		for (int32_t i = 0; i < decomposedLength; ++i) {
			char32_t part = decomposed[i];
			if (isUtf16HighSurrogate(char16_t(part)) && i + 1 < decomposedLength) {
				part = utf16CombineSurrogates(char16_t(part), decomposed[i + 1]);
				++i;
			}
			if (!scratch.append(part)) {
				return false;
			}
		}
	}

	reorderCanonical(scratch);

	buffer.clear();
	for (int32_t i = 0; i < scratch.size(); ++i) {
		char32_t c = scratch[i];
		if (c <= 0xFFFF) {
			if (!buffer.append(char16_t(c))) {
				return false;
			}
		} else {
			char16_t pair[2];
			auto n = utf16EncodeBuf(pair, 2, c);
			if (!buffer.append(pair, n)) {
				return false;
			}
		}
	}
	return true;
}

} // namespace sprt::unicode::detail
