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

// String comparison: code point order, and code point order after full case
// folding. Both are pure functions of the tables in data/ - no locale, no
// system library, the same answer on every target.
//
// Neither is collation. Collation is language-dependent ordering (ё before or
// after я, å between z and the end of the alphabet or next to a, uppercase
// before or after lowercase) and needs CLDR tailoring data these tables do not
// carry. What is here is the deterministic ordering: keys, indexes, protocols,
// reproducible tests, and case-insensitive equality.
//
// This is NOT a transcription of ICU. uprv_strCompare (ustring.cpp) and
// _cmpFold (ustrcase.cpp) compare UTF-16 code *units* and then repair the order
// at the point of difference by subtracting 0x2800 from a BMP code point, so
// that a BMP character above the surrogates does not sort after a supplementary
// one. Comparing decoded code points gives that order by construction, with no
// repair step - and drops the NUL-terminated and strncmp modes, the prefix-match
// output and the two-level buffer stack along with it. What ICU does in ~500
// lines is below in ~90.
//
// Ill-formed input, which Unicode does not order:
//
//   A byte that does not start a well-formed UTF-8 sequence is reported as the
//   pseudo code point 0x110000 + byte. It sorts after every real code point,
//   distinct bad bytes stay distinct, and the result is still a total order -
//   which matters, because these functions sit under container comparators.
//
//   An unpaired surrogate in UTF-16 compares as its own value.
//
// Both differ from ICU, which folds ill-formed UTF-8 to U+FFFD and shifts
// surrogates. Neither ordering is more correct than the other; this one is
// simpler to state.

namespace sprt::unicode::detail {

// --- code point streams ------------------------------------------------------

// The streams yield int64_t rather than char32_t so that both of these fit
// alongside a code point and compare correctly against one.

// Below everything, so that a string which has run out sorts before any longer
// string that continues past it.
static constexpr int64_t StreamEnd = -1;

// An ill-formed UTF-8 byte b is reported as IllFormedBase + b, above every real
// code point.
static constexpr int64_t IllFormedBase = 0x11'0000;

// The decoders index with int32_t, and the mappers next door cap their input at
// Max<int32_t> because of it. Comparison has no such cap: a decoder never reads
// past the end of one code point, so it is handed a window that long - the
// bounds check inside it then sees exactly what it would have seen with the
// whole string, and the position is kept in a size_t out here.

struct Utf8Stream {
	const uint8_t *s;
	size_t pos;
	size_t size;

	int64_t next() {
		if (pos >= size) {
			return StreamEnd;
		}
		auto window = int32_t(min(size - pos, size_t(4))); // U8_MAX_LENGTH
		int32_t i = 0;
		auto c = utf8NextStrict(s + pos, i, window);
		if (c < 0) {
			// Resync one byte on, not at the end of the maximal subpart: every
			// byte of an ill-formed run then gets its own pseudo code point,
			// which keeps the ordering byte-for-byte and total.
			++pos;
			return IllFormedBase + s[pos - 1];
		}
		pos += size_t(i);
		return int64_t(c);
	}
};

struct Utf16Stream {
	const char16_t *s;
	size_t pos;
	size_t size;

	int64_t next() {
		if (pos >= size) {
			return StreamEnd;
		}
		auto window = int32_t(min(size - pos, size_t(2))); // U16_MAX_LENGTH
		int32_t i = 0;
		auto c = u16Next(s + pos, i, window);
		pos += size_t(i);
		return int64_t(c);
	}
};

struct Utf32Stream {
	const char32_t *s;
	size_t pos;
	size_t size;

	int64_t next() { return pos < size ? int64_t(s[pos++]) : StreamEnd; }
};

// The same, with each code point replaced by its full case folding. A folding is
// at most MaxStringLength UTF-16 units and lives in the exceptions table, so the
// pending run is a pointer into that table - nothing is copied and nothing is
// allocated.
template <typename Stream>
struct FoldedStream {
	Stream src;
	const char16_t *pending = nullptr;
	int32_t pendingLength = 0;
	int32_t pendingIndex = 0;

	int64_t next() {
		for (;;) {
			if (pendingIndex < pendingLength) {
				return int64_t(u16Next(pending, pendingIndex, pendingLength));
			}

			auto c = src.next();
			if (c == StreamEnd || c >= IllFormedBase) {
				// Nothing to fold: an ill-formed byte is not a character.
				return c;
			}

			const char16_t *s = nullptr;
			auto result = toFullFolding(char32_t(c), &s, FoldCaseDefault);
			if (result < 0) {
				return int64_t(~result); // folds to itself
			}
			if (result > MaxStringLength) {
				return int64_t(result); // folds to one other code point
			}
			// folds to a string; loop to read the first unit out of it
			pending = s;
			pendingLength = result;
			pendingIndex = 0;
		}
	}
};

// Lexicographic comparison of two code point streams. A string that runs out
// first is the smaller one, because StreamEnd is below everything else.
template <typename A, typename B>
static int compareStreams(A &&a, B &&b) {
	for (;;) {
		auto ca = a.next();
		auto cb = b.next();
		if (ca != cb) {
			return ca < cb ? -1 : 1;
		}
		if (ca == StreamEnd) {
			return 0;
		}
	}
}

} // namespace sprt::unicode::detail
