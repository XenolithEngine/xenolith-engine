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

// Text -> collation elements. Ported from ICU collationiterator.h and
// collationiterator.cpp (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// This is the core of collation, and almost all of its size is one function:
// appendCEsFromCE32 turns a special CE32 into CEs, and there are fifteen kinds of
// special. A character can expand to several CEs, contract with the characters
// after it, depend on the character before it, decompose (Hangul), count as a
// number, or be unassigned and get a computed weight.
//
// The subtle part is *discontiguous* contractions (UCA S2.1.1-S2.1.3). A
// contraction may match across combining marks that sort between its parts: if
// "ch" contracts and the text is "c" + a mark + "h", the mark is skipped, the
// contraction matches, and the mark's own CEs are emitted afterwards. That is
// what SkippedState is for, and it is why the iterator can read ahead, back up,
// and replay.
//
// Departures from the ICU original, all of them subtractions:
//
//   previousCE()/previousCEUnsafe() are gone, with UVector32. Only ICU's public
//     CollationElementIterator calls them; comparison and sort keys are forward
//     only. previousCodePoint() stays - prefix matching reads backwards.
//   The builder hooks (BUILDER_DATA_TAG, getCE32FromBuilderData) are gone: the
//     data arrives built.
//   operator== is gone; nothing compares iterators.
//   UErrorCode is replaced by a sticky `failed` flag. The only failure is an
//     allocation, and the caller checks it once at the end rather than at every
//     step - see collation_compare.cc.

namespace sprt::unicode::detail {

// No code point: the value ICU's U_SENTINEL has. Code points are held in int32_t
// throughout, not char32_t, because "c < 0" is the end-of-text test on every
// other line and an unsigned type would silently make it always false.
static constexpr int32_t NoCodePoint = -1;

// --- CE buffer -----------------------------------------------------------------

// Large enough for the CEs of most short strings; ICU uses the same 40.
static constexpr int32_t CEBufferInitialCapacity = 40;

class CEBuffer {
public:
	CEBuffer() = default;

	~CEBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	CEBuffer(const CEBuffer &) = delete;
	CEBuffer &operator=(const CEBuffer &) = delete;

	int32_t length = 0;

	bool append(int64_t ce) {
		if (length < _capacity || ensureAppendCapacity(1)) {
			_data[length++] = ce;
			return true;
		}
		return false;
	}

	// Requires that capacity was ensured.
	void appendUnsafe(int64_t ce) { _data[length++] = ce; }

	bool ensureAppendCapacity(int32_t appendCapacity) {
		if (length + appendCapacity <= _capacity) {
			return true;
		}
		auto capacity = _capacity;
		do {
			// ICU's growth curve: quadruple while small, double after that.
			capacity = capacity < 1000 ? capacity * 4 : capacity * 2;
		} while (capacity < length + appendCapacity);
		auto buf = reinterpret_cast<int64_t *>(::__sprt_malloc(size_t(capacity) * sizeof(int64_t)));
		if (!buf) {
			return false;
		}
		for (int32_t i = 0; i < length; ++i) {
			buf[i] = _data[i];
		}
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
		_data = buf;
		_capacity = capacity;
		return true;
	}

	bool incLength() {
		if (length < _capacity || ensureAppendCapacity(1)) {
			++length;
			return true;
		}
		return false;
	}

	int64_t set(int32_t i, int64_t ce) { return _data[i] = ce; }
	int64_t get(int32_t i) const { return _data[i]; }
	const int64_t *getCEs() const { return _data; }

private:
	int64_t _inlineData[CEBufferInitialCapacity] = {0};
	int64_t *_data = _inlineData;
	int32_t _capacity = CEBufferInitialCapacity;
};

// --- skipped combining marks ---------------------------------------------------

// A growable UTF-16 buffer with code-point stepping, replacing the two
// UnicodeStrings ICU's SkippedState holds.
class MarkBuffer {
public:
	static constexpr int32_t InlineCapacity = 16;

	MarkBuffer() = default;

	~MarkBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	MarkBuffer(const MarkBuffer &) = delete;
	MarkBuffer &operator=(const MarkBuffer &) = delete;

	int32_t length() const { return _size; }
	bool isEmpty() const { return _size == 0; }
	void remove() { _size = 0; }

	bool setTo(int32_t c) {
		_size = 0;
		return appendCodePoint(c);
	}

	bool appendCodePoint(int32_t c) {
		if (c <= 0xFFFF) {
			return appendUnit(char16_t(c));
		}
		return appendUnit(char16_t(0xD7C0 + (c >> 10)))
				&& appendUnit(char16_t(0xDC00 + (c & 0x3FF)));
	}

	// UnicodeString::char32At.
	int32_t codePointAt(int32_t i) const {
		char32_t c = _data[i];
		if (isUtf16HighSurrogate(char16_t(c)) && i + 1 < _size && isUtf16LowSurrogate(_data[i + 1])) {
			return int32_t(utf16CombineSurrogates(char16_t(c), _data[i + 1]));
		}
		return int32_t(c);
	}

	static int32_t codePointLength(int32_t c) { return c <= 0xFFFF ? 1 : 2; }

	// UnicodeString::moveIndex32: step `delta` code points from `index`.
	int32_t moveIndex32(int32_t index, int32_t delta) const {
		while (delta > 0 && index < _size) {
			index += isUtf16HighSurrogate(_data[index]) && index + 1 < _size
							&& isUtf16LowSurrogate(_data[index + 1])
					? 2
					: 1;
			--delta;
		}
		while (delta < 0 && index > 0) {
			index -= isUtf16LowSurrogate(_data[index - 1]) && index >= 2
							&& isUtf16HighSurrogate(_data[index - 2])
					? 2
					: 1;
			++delta;
		}
		return index;
	}

	// Replaces [0, count) with the first `srcLength` units of `src`. `count` is
	// pinned to the length, as UnicodeString::replace() does.
	bool replaceHead(int32_t count, const MarkBuffer &src, int32_t srcLength) {
		if (count > _size) {
			count = _size;
		}
		auto tail = _size - count;
		if (srcLength + tail > _capacity && !grow(srcLength + tail)) {
			return false;
		}
		// Move the tail into place first; the ranges may overlap.
		if (srcLength != count && tail > 0) {
			if (srcLength < count) {
				for (int32_t i = 0; i < tail; ++i) {
					_data[srcLength + i] = _data[count + i];
				}
			} else {
				for (int32_t i = tail - 1; i >= 0; --i) {
					_data[srcLength + i] = _data[count + i];
				}
			}
		}
		for (int32_t i = 0; i < srcLength; ++i) {
			_data[i] = src._data[i];
		}
		_size = srcLength + tail;
		return true;
	}

private:
	bool appendUnit(char16_t c) {
		if (_size == _capacity && !grow(_size + 1)) {
			return false;
		}
		_data[_size++] = c;
		return true;
	}

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

// Digit values collected for numeric collation, in ICU a CharString.
class DigitBuffer {
public:
	static constexpr int32_t InlineCapacity = 32;

	DigitBuffer() = default;

	~DigitBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	DigitBuffer(const DigitBuffer &) = delete;
	DigitBuffer &operator=(const DigitBuffer &) = delete;

	int32_t length() const { return _size; }
	char *data() { return _data; }

	bool append(char digit) {
		if (_size == _capacity) {
			auto capacity = _capacity * 2;
			auto buf = reinterpret_cast<char *>(::__sprt_malloc(size_t(capacity)));
			if (!buf) {
				return false;
			}
			for (int32_t i = 0; i < _size; ++i) { buf[i] = _data[i]; }
			if (_data != _inlineData) {
				::__sprt_free(_data);
			}
			_data = buf;
			_capacity = capacity;
		}
		_data[_size++] = digit;
		return true;
	}

	// Backward collection reads the digits least-significant first.
	void reverse() {
		for (int32_t i = 0, j = _size - 1; i < j; ++i, --j) {
			auto tmp = _data[i];
			_data[i] = _data[j];
			_data[j] = tmp;
		}
	}

private:
	char _inlineData[InlineCapacity] = {0};
	char *_data = _inlineData;
	int32_t _size = 0;
	int32_t _capacity = InlineCapacity;
};

// State of the combining marks skipped in a discontiguous contraction. Created on
// first use and kept, deactivated, between uses.
class SkippedState {
public:
	void clear() {
		_oldBuffer.remove();
		_pos = 0;
		// _newBuffer is reset by setFirstSkipped().
	}

	bool isEmpty() const { return _oldBuffer.isEmpty(); }

	bool hasNext() const { return _pos < _oldBuffer.length(); }

	// Requires hasNext().
	int32_t next() {
		auto c = _oldBuffer.codePointAt(_pos);
		_pos += MarkBuffer::codePointLength(c);
		return c;
	}

	// One more input code point was read past the end of the marks buffer.
	void incBeyond() { ++_pos; }

	// Steps back through the buffer; returns how many code points still have to be
	// backed out of the normal input.
	int32_t backwardNumCodePoints(int32_t n) {
		auto length = _oldBuffer.length();
		auto beyond = _pos - length;
		if (beyond > 0) {
			if (beyond >= n) {
				// Not far enough back to re-enter the buffer.
				_pos -= n;
				return n;
			}
			// Back out everything beyond the buffer and re-enter it.
			_pos = _oldBuffer.moveIndex32(length, beyond - n);
			return beyond;
		}
		_pos = _oldBuffer.moveIndex32(_pos, -n);
		return 0;
	}

	bool setFirstSkipped(int32_t c) {
		_skipLengthAtMatch = 0;
		return _newBuffer.setTo(c);
	}

	bool skip(int32_t c) { return _newBuffer.appendCodePoint(c); }

	void recordMatch() { _skipLengthAtMatch = _newBuffer.length(); }

	// Replaces the characters consumed with the newly skipped ones.
	bool replaceMatch() {
		auto ok = _oldBuffer.replaceHead(_pos, _newBuffer, _skipLengthAtMatch);
		_pos = 0;
		return ok;
	}

	void saveTrieState(const UCharsTrie &trie) { trie.saveState(_state); }
	void resetToTrieState(UCharsTrie &trie) const { trie.resetToState(_state); }

private:
	// Marks skipped in a previous discontiguous match; read from here afterwards.
	MarkBuffer _oldBuffer;
	// Marks newly skipped in the current match, from the text or from _oldBuffer.
	MarkBuffer _newBuffer;
	// Reading index in _oldBuffer, or, past its end, the count of code points read
	// beyond it.
	int32_t _pos = 0;
	// _newBuffer.length() at the last matching character, so a failed partial match
	// can back out the skipped and partially-matched input.
	int32_t _skipLengthAtMatch = 0;
	// The trie position before trying a character, so it can be skipped and the
	// next one tried.
	UCharsTrie::State _state;
};

// --- the iterator --------------------------------------------------------------

class CollationIterator {
public:
	CollationIterator(const CollationData *d, bool numeric) : data(d), _isNumeric(numeric) { }

	// Not virtual, and protected below: an iterator is always a concrete object on
	// the stack. A virtual destructor would pull in `operator delete`, which this
	// freestanding runtime deprecates, for a deletion that never happens.
	CollationIterator(const CollationIterator &) = delete;
	CollationIterator &operator=(const CollationIterator &) = delete;

	virtual void resetToOffset(int32_t newOffset) = 0;
	virtual int32_t getOffset() const = 0;

	// An allocation failed somewhere; every CE from here on is meaningless. The
	// comparison layer checks this once, at the end.
	bool failed() const { return _failed; }

	// The next collation element, or NoCE at the end of the text.
	int64_t nextCE() {
		if (_cesIndex < ceBuffer.length) {
			return ceBuffer.get(_cesIndex++);
		}
		if (!ceBuffer.incLength()) {
			_failed = true;
			return NoCE;
		}
		int32_t c;
		auto ce32 = handleNextCE32(c);
		uint32_t t = ce32 & 0xFF;
		if (t < SpecialCE32LowByte) {
			// A normal CE from the main data. Inlined ceFromSimpleCE32, as in ICU:
			// this is the path nearly every character takes.
			return ceBuffer.set(_cesIndex++,
					(int64_t(ce32 & 0xFFFF'0000) << 32) | int64_t((ce32 & 0xFF00) << 16)
							| int64_t(t << 8));
		}
		const CollationData *d;
		if (t == SpecialCE32LowByte) {
			if (c < 0) {
				return ceBuffer.set(_cesIndex++, NoCE);
			}
			d = data->base;
			ce32 = d->getCE32(char32_t(c));
			t = ce32 & 0xFF;
			if (t < SpecialCE32LowByte) {
				// A normal CE from the base data.
				return ceBuffer.set(_cesIndex++,
						(int64_t(ce32 & 0xFFFF'0000) << 32) | int64_t((ce32 & 0xFF00) << 16)
								| int64_t(t << 8));
			}
		} else {
			d = data;
		}
		if (t == LongPrimaryCE32LowByte) {
			return ceBuffer.set(_cesIndex++,
					(int64_t(ce32 - t) << 32) | CommonSecAndTerCE);
		}
		return nextCEFromCE32(d, c, ce32);
	}

	// Fetches every CE; returns getCEsLength().
	int32_t fetchCEs() {
		while (!_failed && nextCE() != NoCE) {
			// No need to loop over each CE of an expansion.
			_cesIndex = ceBuffer.length;
		}
		return ceBuffer.length;
	}

	// Overwrites the CE nextCE() returned last.
	void setCurrentCE(int64_t ce) { ceBuffer.set(_cesIndex - 1, ce); }

	int32_t getCEsLength() const { return ceBuffer.length; }
	int64_t getCE(int32_t i) const { return ceBuffer.get(i); }
	const int64_t *getCEs() const { return ceBuffer.getCEs(); }

	void clearCEs() { _cesIndex = ceBuffer.length = 0; }

	void clearCEsIfNoneRemaining() {
		if (_cesIndex == ceBuffer.length) {
			clearCEs();
		}
	}

	// Public for the identical level, which walks the text itself.
	virtual int32_t nextCodePoint() = 0;
	virtual int32_t previousCodePoint() = 0;

protected:
	~CollationIterator() = default;

	void reset() {
		_cesIndex = ceBuffer.length = 0;
		_skipped.clear();
	}

	// The next code point and its CE32 in *this* data. Returns FallbackCE32 at the
	// end of the text (c < 0) or when the value has to come from the base.
	virtual uint32_t handleNextCE32(int32_t &c) {
		c = nextCodePoint();
		return c < 0 ? FallbackCE32 : data->getCE32(char32_t(c));
	}

	// Called when handleNextCE32 returned LeadSurrogateTag: returns the trail
	// surrogate and advances past it if one follows, otherwise returns anything
	// else and does not advance.
	virtual char16_t handleGetTrailSurrogate() { return 0; }

	// Called when handleNextCE32 returned with c == 0, to ask whether that is a
	// NUL terminator.
	virtual bool foundNULTerminator() { return false; }

	// false when surrogate code points get their own implicit primaries (UTF-16),
	// true when they map to CE(U+FFFD) (UTF-8).
	virtual bool forbidSurrogateCodePoints() const { return false; }

	virtual void forwardNumCodePoints(int32_t num) = 0;
	virtual void backwardNumCodePoints(int32_t num) = 0;

	// The CE32 from the data trie. Same as data->getCE32(); a separate virtual in
	// ICU for the builder's benefit, kept because appendCEsFromCE32 calls it on the
	// skipped-marks path.
	virtual uint32_t getDataCE32(int32_t c) const { return data->getCE32(char32_t(c)); }

	void appendCEsFromCE32(const CollationData *d, int32_t c, uint32_t ce32, bool forward);

	const CollationData *data;

	CEBuffer ceBuffer;

private:
	int64_t nextCEFromCE32(const CollationData *d, int32_t c, uint32_t ce32) {
		--ceBuffer.length; // undo incLength()
		appendCEsFromCE32(d, c, ce32, true);
		if (_failed) {
			return NoCEPrimary;
		}
		return ceBuffer.get(_cesIndex++);
	}

	uint32_t getCE32FromPrefix(const CollationData *d, uint32_t ce32) {
		auto p = d->contexts + indexFromCE32(ce32);
		ce32 = CollationData::readCE32(p); // the default if no prefix matches
		p += 2;
		// How many code points were read before the original one.
		int32_t lookBehind = 0;
		UCharsTrie prefixes(p);
		for (;;) {
			auto c = previousCodePoint();
			if (c < 0) {
				break;
			}
			++lookBehind;
			auto match = prefixes.nextForCodePoint(char32_t(c));
			if (trieHasValue(match)) {
				ce32 = uint32_t(prefixes.getValue());
			}
			if (!trieHasNext(match)) {
				break;
			}
		}
		forwardNumCodePoints(lookBehind);
		return ce32;
	}

	int32_t nextSkippedCodePoint() {
		if (_skipped.hasNext()) {
			return _skipped.next();
		}
		if (_numCpFwd == 0) {
			return NoCodePoint;
		}
		auto c = nextCodePoint();
		if (!_skipped.isEmpty() && c >= 0) {
			_skipped.incBeyond();
		}
		if (_numCpFwd > 0 && c >= 0) {
			--_numCpFwd;
		}
		return c;
	}

	void backwardNumSkipped(int32_t n) {
		if (!_skipped.isEmpty()) {
			n = _skipped.backwardNumCodePoints(n);
		}
		backwardNumCodePoints(n);
		if (_numCpFwd >= 0) {
			_numCpFwd += n;
		}
	}

	uint32_t nextCE32FromContraction(const CollationData *d, uint32_t contractionCE32,
			const char16_t *p, uint32_t ce32, int32_t c);

	uint32_t nextCE32FromDiscontiguousContraction(const CollationData *d, UCharsTrie &suffixes,
			uint32_t ce32, int32_t lookAhead, int32_t c);

	void appendNumericCEs(uint32_t ce32, bool forward);
	void appendNumericSegmentCEs(const char *digits, int32_t length);

	int32_t _cesIndex = 0;
	// ICU allocates this on first use; here it is a member. It is under 100 bytes,
	// two iterators live on the stack per comparison, and holding it by value takes
	// out an allocation, a failure path and a destructor. The one place ICU tells
	// "never used" from "used and empty" is the contraction fast path, and taking
	// that path while empty is not just safe but slightly better - which is why the
	// test below is isEmpty() rather than a separate flag.
	SkippedState _skipped;
	// How many code points may still be read forward, or -1 for no limit.
	int32_t _numCpFwd = -1;
	bool _isNumeric = false;
	bool _failed = false;
};

// The fifteen kinds of special CE32, each one a different shape of data. The
// loop re-reads ce32 after the tags that only redirect, and returns from the ones
// that produce CEs directly.
void CollationIterator::appendCEsFromCE32(const CollationData *d, int32_t c, uint32_t ce32,
		bool forward) {
	while (isSpecialCE32(ce32)) {
		switch (tagFromCE32(ce32)) {
		case FallbackTag:
		case ReservedTag3:
		case BuilderDataTag:
			// FallbackTag is resolved by the caller, ReservedTag3 is unused, and
			// BuilderDataTag never appears in built data. Reaching any of them
			// means the table and this code disagree.
			_failed = true;
			return;
		case LongPrimaryTag:
			if (!ceBuffer.append(ceFromLongPrimaryCE32(ce32))) {
				_failed = true;
			}
			return;
		case LongSecondaryTag:
			if (!ceBuffer.append(ceFromLongSecondaryCE32(ce32))) {
				_failed = true;
			}
			return;
		case LatinExpansionTag:
			if (ceBuffer.ensureAppendCapacity(2)) {
				ceBuffer.set(ceBuffer.length, latinCE0FromCE32(ce32));
				ceBuffer.set(ceBuffer.length + 1, latinCE1FromCE32(ce32));
				ceBuffer.length += 2;
			} else {
				_failed = true;
			}
			return;
		case Expansion32Tag: {
			auto ce32s = d->ce32s + indexFromCE32(ce32);
			auto length = lengthFromCE32(ce32);
			if (ceBuffer.ensureAppendCapacity(length)) {
				do { ceBuffer.appendUnsafe(ceFromCE32(*ce32s++)); } while (--length > 0);
			} else {
				_failed = true;
			}
			return;
		}
		case ExpansionTag: {
			auto ces = d->ces + indexFromCE32(ce32);
			auto length = lengthFromCE32(ce32);
			if (ceBuffer.ensureAppendCapacity(length)) {
				do { ceBuffer.appendUnsafe(*ces++); } while (--length > 0);
			} else {
				_failed = true;
			}
			return;
		}
		case PrefixTag:
			// The prefix is the text *before* this character, so step back over the
			// character itself, match, and step forward again.
			if (forward) {
				backwardNumCodePoints(1);
			}
			ce32 = getCE32FromPrefix(d, ce32);
			if (forward) {
				forwardNumCodePoints(1);
			}
			break;
		case ContractionTag: {
			auto p = d->contexts + indexFromCE32(ce32);
			auto defaultCE32 = CollationData::readCE32(p); // if no suffix matches
			if (!forward) {
				// Backward contraction matching is previousCEUnsafe's job, which is
				// not ported; a backward walk never gets here with a match.
				ce32 = defaultCE32;
				break;
			}
			int32_t nextCp;
			if (_skipped.isEmpty() && _numCpFwd < 0) {
				// The common case, pulled out of nextCE32FromContraction to skip a
				// call and the skipped-marks bookkeeping.
				nextCp = nextCodePoint();
				if (nextCp < 0) {
					ce32 = defaultCE32;
					break;
				}
				if ((ce32 & ContractNextCcc) != 0 && !mayHaveLccc(nextCp)) {
					// Every suffix starts with lccc != 0 and this does not.
					backwardNumCodePoints(1);
					ce32 = defaultCE32;
					break;
				}
			} else {
				nextCp = nextSkippedCodePoint();
				if (nextCp < 0) {
					ce32 = defaultCE32;
					break;
				}
				if ((ce32 & ContractNextCcc) != 0 && !mayHaveLccc(nextCp)) {
					backwardNumSkipped(1);
					ce32 = defaultCE32;
					break;
				}
			}
			ce32 = nextCE32FromContraction(d, ce32, p + 2, defaultCE32, nextCp);
			if (ce32 == NoCE32) {
				// A discontiguous contraction already appended its CEs and those of
				// the marks it skipped.
				return;
			}
			break;
		}
		case DigitTag:
			if (_isNumeric) {
				appendNumericCEs(ce32, forward);
				return;
			}
			// The non-numeric CE32, and round again.
			ce32 = d->ce32s[indexFromCE32(ce32)];
			break;
		case U0000Tag:
			if (forward && foundNULTerminator()) {
				if (!ceBuffer.append(NoCE)) {
					_failed = true;
				}
				return;
			}
			// The normal CE32 for U+0000, and round again.
			ce32 = d->ce32s[0];
			break;
		case HangulTag: {
			auto jamoCE32s = d->jamoCE32s;
			c -= int32_t(HangulBase);
			int32_t t = c % int32_t(JamoTCount);
			c /= int32_t(JamoTCount);
			int32_t v = c % int32_t(JamoVCount);
			c /= int32_t(JamoVCount);
			if ((ce32 & HangulNoSpecialJamo) != 0) {
				// No jamo CE32 is special: no recursion and no per-jamo test.
				if (ceBuffer.ensureAppendCapacity(t == 0 ? 2 : 3)) {
					ceBuffer.set(ceBuffer.length, ceFromCE32(jamoCE32s[c]));
					ceBuffer.set(ceBuffer.length + 1, ceFromCE32(jamoCE32s[19 + v]));
					ceBuffer.length += 2;
					if (t != 0) {
						ceBuffer.appendUnsafe(ceFromCE32(jamoCE32s[39 + t]));
					}
				} else {
					_failed = true;
				}
				return;
			}
			// The jamo code points themselves are not needed: there is no offset or
			// implicit CE32 among them.
			appendCEsFromCE32(d, NoCodePoint, jamoCE32s[c], forward);
			appendCEsFromCE32(d, NoCodePoint, jamoCE32s[19 + v], forward);
			if (t == 0) {
				return;
			}
			// 39 = 19 (L count) + 21 (T count) - 1 (t == 0 is omitted).
			ce32 = jamoCE32s[39 + t];
			c = NoCodePoint;
			break;
		}
		case LeadSurrogateTag: {
			// Backward iteration never sees lead surrogate code *unit* data.
			char16_t trail = handleGetTrailSurrogate();
			if (isUtf16LowSurrogate(trail)) {
				c = int32_t(utf16CombineSurrogates(char16_t(c), trail));
				ce32 &= LeadTypeMask;
				if (ce32 == LeadAllUnassigned) {
					ce32 = UnassignedCE32;
				} else if (ce32 == LeadAllFallback
						|| (ce32 = d->getCE32FromSupplementary(char32_t(c))) == FallbackCE32) {
					d = d->base;
					ce32 = d->getCE32FromSupplementary(char32_t(c));
				}
			} else {
				// An unpaired surrogate.
				ce32 = UnassignedCE32;
			}
			break;
		}
		case OffsetTag:
			if (!ceBuffer.append(d->getCEFromOffsetCE32(char32_t(c), ce32))) {
				_failed = true;
			}
			return;
		case ImplicitTag:
			if (c >= 0xD800 && c <= 0xDFFF && forbidSurrogateCodePoints()) {
				ce32 = FFFDCE32;
				break;
			}
			if (!ceBuffer.append(unassignedCEFromCodePoint(c))) {
				_failed = true;
			}
			return;
		default:
			_failed = true;
			return;
		}
	}
	if (!ceBuffer.append(ceFromSimpleCE32(ce32))) {
		_failed = true;
	}
}

// Matches a contraction's suffixes. `c` is the code point after the original one.
uint32_t CollationIterator::nextCE32FromContraction(const CollationData *d,
		uint32_t contractionCE32, const char16_t *p, uint32_t ce32, int32_t c) {
	// Code points read beyond the original one, needed for discontiguous matching.
	int32_t lookAhead = 1;
	// Code points read since the last match; c alone, at first.
	int32_t sinceMatch = 1;
	// A contiguous match needs no saved state to retry from. Once combining marks
	// are being skipped, the state is tracked.
	UCharsTrie suffixes(p);
	if (!_skipped.isEmpty()) {
		_skipped.saveTrieState(suffixes);
	}
	auto match = suffixes.firstForCodePoint(char32_t(c));
	for (;;) {
		int32_t nextCp;
		if (trieHasValue(match)) {
			ce32 = uint32_t(suffixes.getValue());
			if (!trieHasNext(match) || (c = nextSkippedCodePoint()) < 0) {
				return ce32;
			}
			if (!_skipped.isEmpty()) {
				_skipped.saveTrieState(suffixes);
			}
			sinceMatch = 1;
		} else if (match == TrieResult::NoMatch || (nextCp = nextSkippedCodePoint()) < 0) {
			// No match for c, or a partial match with no further text. Back up if
			// necessary and try a discontiguous contraction.
			if ((contractionCE32 & ContractTrailingCcc) != 0
					// Discontiguous matching extends an existing match; with no match
					// yet there is nothing to extend.
					&& ((contractionCE32 & ContractSingleCpNoMatch) == 0
							|| sinceMatch < lookAhead)) {
				// Some suffix ends with lccc != 0, so a discontiguous match is
				// possible. UCA S2.1.1 only processes non-starters immediately after
				// "a match in the table", which is sinceMatch == 1.
				if (sinceMatch > 1) {
					// Return to the state after the last match and re-fetch the first
					// partially-matched character.
					backwardNumSkipped(sinceMatch);
					c = nextSkippedCodePoint();
					lookAhead -= sinceMatch - 1;
					sinceMatch = 1;
				}
				if (getFCD16(char32_t(c)) > 0xFF) {
					return nextCE32FromDiscontiguousContraction(d, suffixes, ce32, lookAhead, c);
				}
			}
			break;
		} else {
			// A partial match for c, which has no value of its own and so is not "a
			// match in the table". If it has ccc != 0 it may yet be skipped.
			c = nextCp;
			++sinceMatch;
		}
		++lookAhead;
		match = suffixes.nextForCodePoint(char32_t(c));
	}
	backwardNumSkipped(sinceMatch);
	return ce32;
}

// UCA section 3.3.2: a contraction ending with non-starters must also be found
// when the final non-starters could be reordered into a canonically equivalent
// contiguous sequence.
//
//   S2.1   Find the longest initial substring S with a match in the table.
//   S2.1.1 If any non-starters follow S, process each non-starter C.
//   S2.1.2 If C is not blocked from S, look for a match for S + C.
//   S2.1.3 On a match, replace S by S + C and remove C.
uint32_t CollationIterator::nextCE32FromDiscontiguousContraction(const CollationData *d,
		UCharsTrie &suffixes, uint32_t ce32, int32_t lookAhead, int32_t c) {
	// Is a discontiguous contraction possible at all?
	auto fcd16 = getFCD16(char32_t(c));
	auto nextCp = nextSkippedCodePoint();
	if (nextCp < 0) {
		backwardNumSkipped(1);
		return ce32;
	}
	++lookAhead;
	auto prevCC = uint8_t(fcd16);
	fcd16 = getFCD16(char32_t(nextCp));
	if (fcd16 <= 0xFF) {
		// The code point after c is a starter (S2.1.1 "each non-starter").
		backwardNumSkipped(2);
		return ce32;
	}

	// (lookAhead - 2) code points have been read and matched, then non-matching c,
	// then a peek at nextCp. Return to the state before the mismatch and keep
	// matching from nextCp.
	if (_skipped.isEmpty()) {
		suffixes.reset();
		if (lookAhead > 2) {
			// Replay the partial match.
			backwardNumCodePoints(lookAhead);
			suffixes.firstForCodePoint(char32_t(nextCodePoint()));
			for (int32_t i = 3; i < lookAhead; ++i) {
				suffixes.nextForCodePoint(char32_t(nextCodePoint()));
			}
			// Skip c, which did not match, and nextCp, which is about to be tried.
			forwardNumCodePoints(2);
		}
		_skipped.saveTrieState(suffixes);
	} else {
		// Back to the trie state from before c failed.
		_skipped.resetToTrieState(suffixes);
	}

	if (!_skipped.setFirstSkipped(c)) {
		_failed = true;
		return 0;
	}
	// Code points read since the last match: c and nextCp.
	int32_t sinceMatch = 2;
	c = nextCp;
	for (;;) {
		TrieResult match;
		// "If C is not blocked from S, find if S + C has a match" (S2.1.2). C is
		// blocked when a non-starter of the same or zero class sits between it and
		// the last starter - which is what comparing prevCC to lccc(C) tests.
		if (prevCC < (fcd16 >> 8) && trieHasValue(match = suffixes.nextForCodePoint(char32_t(c)))) {
			// "Replace S by S + C, and remove C" (S2.1.3); prevCC is unchanged.
			ce32 = uint32_t(suffixes.getValue());
			sinceMatch = 0;
			_skipped.recordMatch();
			if (!trieHasNext(match)) {
				break;
			}
			_skipped.saveTrieState(suffixes);
		} else {
			// No match for S + C: skip C.
			if (!_skipped.skip(c)) {
				_failed = true;
				return 0;
			}
			_skipped.resetToTrieState(suffixes);
			prevCC = uint8_t(fcd16);
		}
		if ((c = nextSkippedCodePoint()) < 0) {
			break;
		}
		++sinceMatch;
		fcd16 = getFCD16(char32_t(c));
		if (fcd16 <= 0xFF) {
			// A starter ends the run of non-starters (S2.1.1).
			break;
		}
	}
	backwardNumSkipped(sinceMatch);
	auto isTopDiscontiguous = _skipped.isEmpty();
	if (!_skipped.replaceMatch()) {
		_failed = true;
		return 0;
	}
	if (isTopDiscontiguous && !_skipped.isEmpty()) {
		// There was a match after skipping one or more marks, and this is not a
		// nested discontiguous contraction. Append the contraction's CEs and then
		// those of the marks skipped before the match.
		c = NoCodePoint;
		for (;;) {
			appendCEsFromCE32(d, c, ce32, true);
			// The skipped marks' CE32s come from the normal data with fallback, not
			// from wherever the contraction was found.
			if (!_skipped.hasNext()) {
				break;
			}
			c = _skipped.next();
			ce32 = getDataCE32(c);
			if (ce32 == FallbackCE32) {
				d = data->base;
				ce32 = d->getCE32(char32_t(c));
			} else {
				d = data;
			}
			// A nested discontiguous match replaces the consumed marks with newly
			// skipped ones and rewinds the reading position.
		}
		_skipped.clear();
		ce32 = NoCE32; // tells the caller the result is in the CE buffer
	}
	return ce32;
}

// --- numeric collation ---------------------------------------------------------

// A run of digits turned into CEs that sort in numeric order, so that "item2"
// comes before "item10". Only reached when the numeric option is on.
void CollationIterator::appendNumericCEs(uint32_t ce32, bool forward) {
	// Digit *values* 0..9, not characters. A written number has no length bound, so
	// this grows; the inline size covers anything a person types.
	DigitBuffer digits;
	if (forward) {
		for (;;) {
			if (!digits.append(digitFromCE32(ce32))) {
				_failed = true;
				return;
			}
			if (_numCpFwd == 0) {
				break;
			}
			auto c = nextCodePoint();
			if (c < 0) {
				break;
			}
			ce32 = data->getCE32(char32_t(c));
			if (ce32 == FallbackCE32) {
				ce32 = data->base->getCE32(char32_t(c));
			}
			if (!hasCE32Tag(ce32, DigitTag)) {
				backwardNumCodePoints(1);
				break;
			}
			if (_numCpFwd > 0) {
				--_numCpFwd;
			}
		}
	} else {
		for (;;) {
			if (!digits.append(digitFromCE32(ce32))) {
				_failed = true;
				return;
			}
			auto c = previousCodePoint();
			if (c < 0) {
				break;
			}
			ce32 = data->getCE32(char32_t(c));
			if (ce32 == FallbackCE32) {
				ce32 = data->base->getCE32(char32_t(c));
			}
			if (!hasCE32Tag(ce32, DigitTag)) {
				forwardNumCodePoints(1);
				break;
			}
		}
		digits.reverse();
	}

	auto count = digits.length();
	auto buf = digits.data();
	int32_t pos = 0;
	do {
		// Skip leading zeros.
		while (pos < count - 1 && buf[pos] == 0) { ++pos; }
		// At most 254 digits per CE sequence.
		auto segmentLength = count - pos;
		if (segmentLength > 254) {
			segmentLength = 254;
		}
		appendNumericSegmentCEs(buf + pos, segmentLength);
		pos += segmentLength;
	} while (!_failed && pos < count);
}

// 1..254 digits as CEs. Primary byte values 2..255 are used: digits are not
// compressible.
void CollationIterator::appendNumericSegmentCEs(const char *digits, int32_t length) {
	auto numericPrimary = data->numericPrimary;
	if (length <= 7) {
		// A dense encoding for small numbers.
		int32_t value = digits[0];
		for (int32_t i = 1; i < length; ++i) { value = value * 10 + digits[i]; }
		// Second primary byte values:
		//    74 values   2.. 75 for small numbers, in two-byte primaries;
		//    40 values  76..115 for medium ones, in three-byte primaries;
		//    16 values 116..131 for large ones, in four-byte primaries;
		//   124 values 132..255 for very large ones, as 4..127 digit pairs.
		int32_t firstByte = 2;
		int32_t numBytes = 74;
		if (value < numBytes) {
			// Two bytes for 0..73: day and month numbers and the like.
			if (!ceBuffer.append(makeCE(numericPrimary | uint32_t((firstByte + value) << 16)))) {
				_failed = true;
			}
			return;
		}
		value -= numBytes;
		firstByte += numBytes;
		numBytes = 40;
		if (value < numBytes * 254) {
			// Three bytes for 74..10233: years and more.
			auto primary = numericPrimary | uint32_t((firstByte + value / 254) << 16)
					| uint32_t((2 + value % 254) << 8);
			if (!ceBuffer.append(makeCE(primary))) {
				_failed = true;
			}
			return;
		}
		value -= numBytes * 254;
		firstByte += numBytes;
		numBytes = 16;
		if (value < numBytes * 254 * 254) {
			// Four bytes for 10234..1042489.
			auto primary = numericPrimary | uint32_t(2 + value % 254);
			value /= 254;
			primary |= uint32_t(2 + value % 254) << 8;
			value /= 254;
			primary |= uint32_t(firstByte + value % 254) << 16;
			if (!ceBuffer.append(makeCE(primary))) {
				_failed = true;
			}
			return;
		}
		// Larger than 1042489: fall through to the digit-pair encoding.
	}

	// The second primary byte 132..255 is the number of digit pairs (4..127), and
	// the pairs follow. Trailing 00 pairs are dropped and the last pair is
	// decremented, so that "100" and "1000" do not collide.
	auto numPairs = (length + 1) / 2;
	auto primary = numericPrimary | uint32_t((132 - 4 + numPairs) << 16);
	while (digits[length - 1] == 0 && digits[length - 2] == 0) { length -= 2; }

	uint32_t pair;
	int32_t pos;
	if (length & 1) {
		// Half a pair, for an odd number of digits.
		pair = uint32_t(digits[0]);
		pos = 1;
	} else {
		pair = uint32_t(digits[0] * 10 + digits[1]);
		pos = 2;
	}
	pair = 11 + 2 * pair;
	int32_t shift = 8;
	while (pos < length) {
		if (shift == 0) {
			// Every three pairs fill a four-byte primary; start a new CE with the
			// numeric lead byte.
			primary |= pair;
			if (!ceBuffer.append(makeCE(primary))) {
				_failed = true;
				return;
			}
			primary = numericPrimary;
			shift = 16;
		} else {
			primary |= pair << shift;
			shift -= 8;
		}
		pair = 11 + 2 * uint32_t(digits[pos] * 10 + digits[pos + 1]);
		pos += 2;
	}
	primary |= (pair - 1) << shift;
	if (!ceBuffer.append(makeCE(primary))) {
		_failed = true;
	}
}

} // namespace sprt::unicode::detail
