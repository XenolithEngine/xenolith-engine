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

// The UTF-16 working buffer the UTS-46 engine and its normalizer operate on, and
// the canonical-ordering buffer built on top of it.
//
// Utf16Buffer replaces ICU's UnicodeString (93 KB of source in libuidna) with the
// ~40 operations UTS-46 actually performs. ReorderingBuffer is ported from
// libuidna src/u_edits.{h,cc} (ICU normalizer2impl.cpp; © Unicode, Inc.;
// http://www.unicode.org/copyright.html) with UnicodeString swapped out.
//
// Two things behave differently from ICU on purpose:
//
//  * failure is a `false` return, not a sticky `isBogus()` flag plus a UErrorCode;
//  * a growth never happens behind the caller's back through a saved pointer -
//    reserve() may move the storage, and every user re-derives its pointers, the
//    way ICU's ReorderingBuffer::resize() already did.
//
// The runtime idiom "measure, __sprt_typed_malloca, fill, invoke callback, free"
// does NOT fit here: the working buffer is grown from inside loops (mapDevChars,
// processLabel), and alloca in a loop is a stack leak. Inline storage with a heap
// spill is used instead; the malloca idiom stays for the final UTF-8 emission.

namespace sprt::idn::detail {

// Growable UTF-16 buffer. A domain name is at most 255 bytes, so the inline
// storage covers every real input and the heap path exists for adversarial ones
// (a label of sharp-s mapping to "ss" can double, and callers may pass more than
// one name's worth of text).
class Utf16Buffer {
public:
	static constexpr int32_t InlineCapacity = 256;

	Utf16Buffer() = default;

	~Utf16Buffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	Utf16Buffer(const Utf16Buffer &) = delete;
	Utf16Buffer &operator=(const Utf16Buffer &) = delete;

	char16_t *data() { return _data; }
	const char16_t *data() const { return _data; }
	int32_t size() const { return _size; }
	int32_t capacity() const { return _capacity; }
	bool empty() const { return _size == 0; }

	char16_t operator[](int32_t i) const { return _data[i]; }
	void setAt(int32_t i, char16_t c) { _data[i] = c; }

	WideStringView view() const { return WideStringView(_data, size_t(_size)); }

	WideStringView sub(int32_t offset, int32_t count) const {
		return WideStringView(_data + offset, size_t(count));
	}

	// Makes room for at least `n` units WITHOUT changing the length. May move the
	// storage, so every pointer obtained from data() before the call is dead.
	bool reserve(int32_t n) {
		if (n <= _capacity) {
			return true;
		}
		int32_t newCapacity = _capacity * 2;
		if (newCapacity < n) {
			newCapacity = n;
		}
		auto buf = reinterpret_cast<char16_t *>(
				::__sprt_malloc(size_t(newCapacity) * sizeof(char16_t)));
		if (!buf) {
			return false;
		}
		if (_size > 0) {
			::__sprt_memcpy(buf, _data, size_t(_size) * sizeof(char16_t));
		}
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
		_data = buf;
		_capacity = newCapacity;
		return true;
	}

	// Sets the length. The caller is responsible for having reserved for it and for
	// the contents of any newly exposed units.
	void setSize(int32_t n) { _size = n; }

	bool resize(int32_t n) {
		if (!reserve(n)) {
			return false;
		}
		_size = n;
		return true;
	}

	void clear() { _size = 0; }

	bool append(char16_t c) {
		if (!reserve(_size + 1)) {
			return false;
		}
		_data[_size++] = c;
		return true;
	}

	bool append(const char16_t *s, int32_t n) {
		if (n <= 0) {
			return true;
		}
		if (!reserve(_size + n)) {
			return false;
		}
		::__sprt_memcpy(_data + _size, s, size_t(n) * sizeof(char16_t));
		_size += n;
		return true;
	}

	bool append(WideStringView str) { return append(str.data(), int32_t(str.size())); }

	// Appends one code point, splitting it into a surrogate pair when needed.
	bool appendCodepoint(char32_t c) {
		auto len = unicode::utf16EncodeLength(c);
		if (len == 0) {
			return true; // not a scalar value: nothing to write
		}
		if (!reserve(_size + int32_t(len))) {
			return false;
		}
		_size += int32_t(unicode::utf16EncodeBuf(_data + _size, size_t(len), c));
		return true;
	}

	// Replaces [start, start + count) with `n` units from `s`. `s` must not point
	// into this buffer.
	bool replace(int32_t start, int32_t count, const char16_t *s, int32_t n) {
		int32_t tail = _size - (start + count);
		if (n != count) {
			if (!reserve(_size - count + n)) {
				return false;
			}
			if (tail > 0) {
				::__sprt_memmove(_data + start + n, _data + start + count,
						size_t(tail) * sizeof(char16_t));
			}
			_size += n - count;
		}
		if (n > 0) {
			::__sprt_memcpy(_data + start, s, size_t(n) * sizeof(char16_t));
		}
		return true;
	}

	bool insert(int32_t at, const char16_t *s, int32_t n) { return replace(at, 0, s, n); }

	void erase(int32_t start, int32_t count) {
		int32_t tail = _size - (start + count);
		if (tail > 0) {
			::__sprt_memmove(_data + start, _data + start + count, size_t(tail) * sizeof(char16_t));
		}
		_size -= count;
	}

private:
	char16_t _inlineData[InlineCapacity];
	char16_t *_data = _inlineData;
	int32_t _size = 0;
	int32_t _capacity = InlineCapacity;
};

class Normalizer2Impl;

// Accumulates normalized output while keeping combining marks in canonical order.
// Ported from ICU; the only structural change is that it grows a Utf16Buffer
// instead of borrowing UnicodeString's write buffer, so there is no
// getBuffer/releaseBuffer dance and no destructor that has to commit a length.
class ReorderingBuffer {
public:
	ReorderingBuffer(const Normalizer2Impl &ni, Utf16Buffer &dest) : impl(ni), str(dest) { }

	ReorderingBuffer(const ReorderingBuffer &) = delete;
	ReorderingBuffer &operator=(const ReorderingBuffer &) = delete;

	bool init(int32_t destCapacity);

	bool isEmpty() const { return start == limit; }
	int32_t length() const { return int32_t(limit - start); }
	char16_t *getStart() { return start; }
	char16_t *getLimit() { return limit; }
	uint8_t getLastCC() const { return lastCC; }

	bool equals(const char16_t *otherStart, const char16_t *otherLimit) const;

	bool append(char32_t c, uint8_t cc) {
		return (c <= 0xFFFF) ? appendBmp(char16_t(c), cc) : appendSupplementary(c, cc);
	}

	bool append(const char16_t *s, int32_t length, bool isNfd, uint8_t leadCC, uint8_t trailCC);

	bool appendBmp(char16_t c, uint8_t cc) {
		if (remainingCapacity == 0 && !resize(1)) {
			return false;
		}
		if (lastCC <= cc || cc == 0) {
			*limit++ = c;
			lastCC = cc;
			if (cc <= 1) {
				reorderStart = limit;
			}
		} else {
			insert(c, cc);
		}
		--remainingCapacity;
		return true;
	}

	bool appendZeroCC(char32_t c);
	bool appendZeroCC(const char16_t *s, const char16_t *sLimit);

	void remove();
	void removeSuffix(int32_t suffixLength);

	void setReorderingLimit(char16_t *newLimit) {
		remainingCapacity += int32_t(limit - newLimit);
		reorderStart = limit = newLimit;
		lastCC = 0;
	}

	// Commits the accumulated length back to the buffer. ICU did this in the
	// destructor via releaseBuffer(); doing it explicitly means the caller can see
	// it happen and the destructor has no work to fail at.
	void flush() { str.setSize(int32_t(limit - start)); }

	void copyReorderableSuffixTo(Utf16Buffer &s) const {
		s.clear();
		s.append(reorderStart, int32_t(limit - reorderStart));
	}

private:
	bool appendSupplementary(char32_t c, uint8_t cc);
	void insert(char32_t c, uint8_t cc);

	static void writeCodePoint(char16_t *p, char32_t c) {
		if (c <= 0xFFFF) {
			*p = char16_t(c);
		} else {
			unicode::utf16EncodeBuf(p, 2, c);
		}
	}

	bool resize(int32_t appendLength);

	void setIterator() { codePointStart = limit; }
	void skipPrevious(); // requires start < codePointStart
	uint8_t previousCC(); // 0 when there is no previous character

	const Normalizer2Impl &impl;
	Utf16Buffer &str;
	char16_t *start = nullptr;
	char16_t *reorderStart = nullptr;
	char16_t *limit = nullptr;
	int32_t remainingCapacity = 0;
	uint8_t lastCC = 0;

	char16_t *codePointStart = nullptr;
	char16_t *codePointLimit = nullptr;
};

} // namespace sprt::idn::detail
