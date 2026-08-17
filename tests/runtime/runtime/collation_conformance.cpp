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

// The UCA conformance suite: 434 107 sequences that must come out in the order
// the file lists them.
//
// This is the test that decides whether the collation port is right. The engine
// is 3 600 lines of transcribed bit manipulation over a 570 KB table, and almost
// none of it fails loudly when it is wrong - a mis-read field yields a valid
// weight for the wrong character, and two words swap places somewhere in a list
// nobody is looking at. These two files are the only thing that looks everywhere.
//
// The files come in pairs because alternate handling changes what counts as a
// primary difference: NON_IGNORABLE is the CLDR default, SHIFTED moves
// punctuation and spaces to the fourth level. Both are asserted, at identical
// strength, which is the only strength under which the files are fully ordered:
// they contain sequences that differ by nothing but their code points.
//
// Both encodings are checked on every sequence. UTF-8 and UTF-16 are separate
// iterators over the same table - a different FCD segmentation, a different trie
// path, a different set of surrogate rules - and running one and not the other
// would leave half the engine untested.

#include <sprt/runtime/stream.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#include "data/collation_test.cc"

namespace sprt {

// The longest sequence in either file is five code points; the buffers are sized
// for the encoding's limit rather than for that, so a future file cannot overrun
// them silently.
static constexpr int32_t MaxSequence = 15;

struct UcaSequence {
	char32_t cps[MaxSequence];
	int32_t length = 0;

	// UTF-8 and UTF-16 forms, rebuilt for each sequence.
	char utf8[MaxSequence * 4 + 1];
	int32_t utf8Length = 0;
	char16_t utf16[MaxSequence * 2 + 1];
	int32_t utf16Length = 0;

	// True when the sequence contains a lone surrogate. Those are in the files -
	// they are code points, and the root collation gives them weights - but they
	// have no UTF-8 form at all, so only the UTF-16 half of the check applies.
	bool hasSurrogate = false;

	void encode() {
		utf8Length = 0;
		utf16Length = 0;
		hasSurrogate = false;
		for (int32_t i = 0; i < length; ++i) {
			if (unicode::isUtf16Surrogate(char16_t(cps[i])) && cps[i] <= 0xFFFF) {
				// Written straight into the buffer: an encoder will not produce it.
				hasSurrogate = true;
				utf16[utf16Length++] = char16_t(cps[i]);
				continue;
			}
			size_t written = 0;
			unicode::toUtf8(utf8 + utf8Length, sizeof(utf8) - size_t(utf8Length), cps[i], &written);
			utf8Length += int32_t(written);
			utf16Length += unicode::utf16EncodeBuf(utf16 + utf16Length, 2, cps[i]);
		}
	}

	// set(), not the two-argument constructor: the conformance files contain
	// sequences with U+0000 in them, and the constructor stops at the first NUL by
	// design (see the note on it in stringview.h).
	StringView view8() const {
		StringView v;
		v.set(utf8, size_t(utf8Length));
		return v;
	}

	WideStringView view16() const {
		WideStringView v;
		v.set(utf16, size_t(utf16Length));
		return v;
	}
};

// The decoder for the prefix-delta format; see tests/runtime/tools/gen_collation_test.py.
class UcaReader {
public:
	UcaReader(const uint8_t *data, int32_t length) : _data(data), _length(length) { }

	bool next(UcaSequence &sequence) {
		if (_pos >= _length) {
			return false;
		}
		auto header = _data[_pos++];
		auto shared = int32_t(header >> 4);
		auto added = int32_t(header & 0xF);
		// Everything before `shared` is already in `sequence` from the last call.
		sequence.length = shared;
		for (int32_t i = 0; i < added; ++i) {
			auto b = uint32_t(_data[_pos++]);
			char32_t cp;
			if (b < 0x80) {
				cp = b;
			} else if (b < 0xC0) {
				cp = ((b & 0x3F) << 8) | _data[_pos];
				_pos += 1;
			} else {
				cp = ((b & 0x3F) << 16) | (uint32_t(_data[_pos]) << 8) | _data[_pos + 1];
				_pos += 2;
			}
			sequence.cps[sequence.length++] = cp;
		}
		sequence.encode();
		return true;
	}

private:
	const uint8_t *_data;
	int32_t _length;
	int32_t _pos = 0;
};

// A sort key, copied out of the callback so it can outlive it.
struct SortKey {
	uint8_t bytes[256];
	int32_t length = 0;
	bool ok = false;

	void build(WideStringView s, const unicode::CollateOptions &options) {
		ok = false;
		length = 0;
		auto self = this;
		ok = unicode::sortKey([self](BytesView key) {
			if (key.size() <= sizeof(self->bytes)) {
				self->length = int32_t(key.size());
				for (int32_t i = 0; i < self->length; ++i) { self->bytes[i] = key[size_t(i)]; }
			} else {
				self->length = -1; // too long for this test's buffer
			}
		}, s, StringView(), options);
		if (length < 0) {
			ok = false;
		}
	}
};

// memcmp, which is what a sort key promises to be comparable by.
static int32_t compareKeys(const SortKey &a, const SortKey &b) {
	auto n = a.length < b.length ? a.length : b.length;
	for (int32_t i = 0; i < n; ++i) {
		if (a.bytes[i] != b.bytes[i]) {
			return a.bytes[i] < b.bytes[i] ? -1 : 1;
		}
	}
	return a.length == b.length ? 0 : (a.length < b.length ? -1 : 1);
}

static void printSequence(const UcaSequence &s) {
	for (int32_t i = 0; i < s.length; ++i) { sprt::cerr << " U+" << uint32_t(s.cps[i]); }
}

// Walks one file, comparing each sequence with the one before it.
static int32_t sign(int32_t v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

static int32_t runFile(StringView what, const uint8_t *data, int32_t length, int32_t expectedCount,
		bool shifted, int32_t &checks, int32_t &skipped) {
	unicode::CollateOptions options;
	options.strength = unicode::Strength::Identical;
	options.shifted = shifted;

	UcaReader reader(data, length);
	UcaSequence previous;
	UcaSequence current;
	SortKey previousKey;
	SortKey currentKey;
	int32_t failures = 0;
	int32_t count = 0;
	int32_t reported = 0;

	while (reader.next(current)) {
		++count;
		if (count > 1) {
			auto surrogates = previous.hasSurrogate || current.hasSurrogate;
			if (surrogates) {
				++skipped;
			}
			auto utf16 =
					unicode::collate(previous.view16(), current.view16(), StringView(), options);
			auto utf8 = surrogates
					? utf16
					: unicode::collate(previous.view8(), current.view8(), StringView(), options);
			++checks;

			// The sort keys have to order the same pair the same way. This is the
			// only check on the key writer, and it is a strong one: two entirely
			// separate paths through the same table, on 434 000 pairs.
			previousKey.build(previous.view16(), options);
			currentKey.build(current.view16(), options);
			auto keys = (previousKey.ok && currentKey.ok) ? compareKeys(previousKey, currentKey)
														 : 0;
			++checks;
			if (!previousKey.ok || !currentKey.ok) {
				++failures;
				if (reported < 20) {
					++reported;
					sprt::cerr << "  FAIL: " << what << " line " << count
							   << ": could not build a sort key\n";
				}
			} else if (sign(keys) != sign(utf16)) {
				++failures;
				if (reported < 20) {
					++reported;
					sprt::cerr << "  FAIL: " << what << " line " << count << ": sort keys say "
							   << keys << ", compare says " << utf16 << "\n    previous:";
					printSequence(previous);
					sprt::cerr << "\n    current: ";
					printSequence(current);
					sprt::cerr << "\n";
				}
			}

			if (utf8 > 0 || utf16 > 0 || (utf8 < 0) != (utf16 < 0)) {
				++failures;
				if (reported < 20) {
					++reported;
					sprt::cerr << "  FAIL: " << what << " line " << count << ": utf-8 " << utf8
							   << ", utf-16 " << utf16 << "\n    previous:";
					printSequence(previous);
					sprt::cerr << "\n    current: ";
					printSequence(current);
					sprt::cerr << "\n";
				}
			}
		}
		// The next sequence is delta-coded against this one, so it has to be the
		// one the reader keeps writing into.
		previous = current;
	}

	if (count != expectedCount) {
		sprt::cerr << "  FAIL: " << what << " decoded " << count << " sequences, expected "
				   << expectedCount << "\n";
		++failures;
	}
	if (failures > reported) {
		sprt::cerr << "  (" << (failures - reported) << " more failures not shown)\n";
	}
	return failures;
}

void performCollationConformanceTests() {
	int32_t checks = 0;
	int32_t failures = 0;

	int32_t skipped = 0;
	failures += runFile("non-ignorable", s_ucaTestNonIgnorable, int32_t(sizeof(s_ucaTestNonIgnorable)),
			s_ucaTestNonIgnorableCount, false, checks, skipped);
	failures += runFile("shifted", s_ucaTestShifted, int32_t(sizeof(s_ucaTestShifted)),
			s_ucaTestShiftedCount, true, checks, skipped);

	sprt::cout << "collation conformance (UCA " << StringView(s_ucaTestVersion) << ", "
			   << (s_ucaTestNonIgnorableCount + s_ucaTestShiftedCount) << " sequences): " << checks
			   << " checks, " << failures << " failures\n";
	sprt::cout << "  (" << skipped
			   << " of them checked through utf-16 only: a lone surrogate has no utf-8 form)\n";
}

} // namespace sprt
