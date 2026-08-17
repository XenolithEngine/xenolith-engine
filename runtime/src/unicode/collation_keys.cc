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

// Sort keys: the whole comparison flattened into a byte string, so that
// memcmp on two keys gives the same answer collate() would.
//
// Ported from ICU collationkeys.h and collationkeys.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// Why a key rather than a comparison: sorting N strings costs N key
// computations instead of N log N comparisons, and a key can go into an index or
// a database column, where a comparison function cannot.
//
// The shape of a key is the levels one after another, separated by 0x01:
//
//     primary bytes  01  secondary  01  case  01  tertiary  01  quaternary
//
// The primary level is written straight to the output as it goes; every other
// level is collected in its own buffer first, because they are only appended
// once the primary run is complete. That is the whole structure - the rest of
// this file is *compression*, and it is most of the code.
//
// Compression is what keeps keys short. Nearly every character has the same
// secondary weight (0x0500) and the same tertiary weight, so runs of them are
// counted rather than written: a run of common weights becomes one byte encoding
// its length, chosen from a range above or below the common value depending on
// which way the next real weight goes. Each level has its own ranges and its own
// maximum run, and those numbers are what the constants below are.
//
// Departures from ICU, all of them subtractions:
//
//   the sink is a plain growable buffer - no preflight, no "ignore the first N
//     bytes", no borrowed append buffer; our API builds the whole key and hands
//     it to a callback;
//   LevelCallback is gone with the partial-sort-key API it exists for;
//   the identical level is written as NFD in UTF-8 rather than in ICU's BOCU-1
//     compression. Byte order over UTF-8 is code point order, which is all the
//     level has to be, and it keeps the key self-consistent with compare()
//     without carrying a second compressor.

namespace sprt::unicode::detail {

// --- level compression constants -----------------------------------------------

enum : uint32_t {
	// Secondary: up to 33 common weights as 05..25 or 25..45.
	SecCommonLow = CommonByte,
	SecCommonMiddle = SecCommonLow + 0x20,
	SecCommonHigh = SecCommonLow + 0x40,

	// Case level, lowerFirst: up to 7 common weights as 1..7 or 7..13.
	CaseLowerFirstCommonLow = 1,
	CaseLowerFirstCommonMiddle = 7,
	CaseLowerFirstCommonHigh = 13,

	// Case level, upperFirst: up to 13 common weights as 3..15.
	CaseUpperFirstCommonLow = 3,
	CaseUpperFirstCommonHigh = 15,

	// Tertiary without case bits: up to 97 common weights as 05..65 or 65..C5.
	TerOnlyCommonLow = CommonByte,
	TerOnlyCommonMiddle = TerOnlyCommonLow + 0x60,
	TerOnlyCommonHigh = TerOnlyCommonLow + 0xC0,

	// Tertiary with case, lowerFirst: up to 33 as 05..25 or 25..45.
	TerLowerFirstCommonLow = CommonByte,
	TerLowerFirstCommonMiddle = TerLowerFirstCommonLow + 0x20,
	TerLowerFirstCommonHigh = TerLowerFirstCommonLow + 0x40,

	// Tertiary with case, upperFirst: up to 33 as 85..A5 or A5..C5.
	TerUpperFirstCommonLow = CommonByte + 0x80,
	TerUpperFirstCommonMiddle = TerUpperFirstCommonLow + 0x20,
	TerUpperFirstCommonHigh = TerUpperFirstCommonLow + 0x40,

	// Quaternary: up to 113 common weights as 1C..8C or 8C..FC.
	QuatCommonLow = 0x1C,
	QuatCommonMiddle = QuatCommonLow + 0x70,
	QuatCommonHigh = QuatCommonLow + 0xE0,
	// A primary shifted down to the quaternary level must stay below the
	// common-weight compression range.
	QuatShiftedLimitByte = QuatCommonLow - 1, // 0x1B
};

enum : int32_t {
	SecCommonMaxCount = 0x21,
	CaseLowerFirstCommonMaxCount = 7,
	CaseUpperFirstCommonMaxCount = 13,
	TerOnlyCommonMaxCount = 0x61,
	TerLowerFirstCommonMaxCount = 0x21,
	TerUpperFirstCommonMaxCount = 0x21,
	QuatCommonMaxCount = 0x71,
};

// Which levels a strength writes, excluding the case level (independent of
// strength) and the identical level (written by the caller). Indexed by strength.
static constexpr uint32_t levelMaskForStrength(int32_t strength) {
	switch (strength) {
	case StrengthPrimary: return PrimaryLevelFlag;
	case StrengthSecondary: return PrimaryLevelFlag | SecondaryLevelFlag;
	case StrengthTertiary: return PrimaryLevelFlag | SecondaryLevelFlag | TertiaryLevelFlag;
	case StrengthQuaternary:
	case StrengthIdentical:
		return PrimaryLevelFlag | SecondaryLevelFlag | TertiaryLevelFlag | QuaternaryLevelFlag;
	default: return 0;
	}
}

// --- byte buffers ---------------------------------------------------------------

// A growable byte string, for the key itself and for each buffered level.
class ByteBuffer {
public:
	static constexpr int32_t InlineCapacity = 64;

	ByteBuffer() = default;

	~ByteBuffer() {
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
	}

	ByteBuffer(const ByteBuffer &) = delete;
	ByteBuffer &operator=(const ByteBuffer &) = delete;

	bool ok() const { return _ok; }
	bool isEmpty() const { return _size == 0; }
	int32_t length() const { return _size; }
	const uint8_t *data() const { return _data; }
	uint8_t *data() { return _data; }
	uint8_t operator[](int32_t i) const { return _data[i]; }

	void appendByte(uint32_t b) {
		if (_size < _capacity || grow(1)) {
			_data[_size++] = uint8_t(b);
		}
	}

	void append(const uint8_t *bytes, int32_t n) {
		if (n <= 0) {
			return;
		}
		if (_size + n <= _capacity || grow(n)) {
			for (int32_t i = 0; i < n; ++i) { _data[_size + i] = bytes[i]; }
			_size += n;
		}
	}

	// A 16-bit weight, dropping a zero low byte: weights are left-adjusted, so a
	// trailing zero carries no information.
	void appendWeight16(uint32_t w) {
		auto b0 = uint8_t(w >> 8);
		auto b1 = uint8_t(w);
		auto appendLength = b1 == 0 ? 1 : 2;
		if (_size + appendLength <= _capacity || grow(appendLength)) {
			_data[_size++] = b0;
			if (b1 != 0) {
				_data[_size++] = b1;
			}
		}
	}

	void appendWeight32(uint32_t w) {
		uint8_t bytes[4] = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w)};
		auto appendLength = bytes[1] == 0 ? 1 : (bytes[2] == 0 ? 2 : (bytes[3] == 0 ? 3 : 4));
		if (_size + appendLength <= _capacity || grow(appendLength)) {
			for (int32_t i = 0; i < appendLength; ++i) { _data[_size++] = bytes[i]; }
		}
	}

	// The bytes of a 16-bit weight in reverse, for the backward secondary level,
	// which re-reverses whole segments later.
	void appendReverseWeight16(uint32_t w) {
		auto b0 = uint8_t(w >> 8);
		auto b1 = uint8_t(w);
		auto appendLength = b1 == 0 ? 1 : 2;
		if (_size + appendLength <= _capacity || grow(appendLength)) {
			if (b1 == 0) {
				_data[_size++] = b0;
			} else {
				_data[_size] = b1;
				_data[_size + 1] = b0;
				_size += 2;
			}
		}
	}

	// Everything but the last byte, which is the 01 terminator this level added.
	void appendTo(ByteBuffer &sink) const { sink.append(_data, _size - 1); }

private:
	bool grow(int32_t appendCapacity) {
		if (!_ok) {
			return false;
		}
		auto capacity = _capacity * 2;
		auto alternative = _size + 2 * appendCapacity;
		if (capacity < alternative) {
			capacity = alternative;
		}
		if (capacity < 200) {
			capacity = 200;
		}
		auto buf = reinterpret_cast<uint8_t *>(::__sprt_malloc(size_t(capacity)));
		if (!buf) {
			_ok = false;
			return false;
		}
		for (int32_t i = 0; i < _size; ++i) { buf[i] = _data[i]; }
		if (_data != _inlineData) {
			::__sprt_free(_data);
		}
		_data = buf;
		_capacity = capacity;
		return true;
	}

	uint8_t _inlineData[InlineCapacity] = {0};
	uint8_t *_data = _inlineData;
	int32_t _size = 0;
	int32_t _capacity = InlineCapacity;
	bool _ok = true;
};

// --- the key ---------------------------------------------------------------------

// Writes the levels up to the quaternary. The identical level, if any, is the
// caller's - it compares text rather than CEs.
static void writeSortKeyUpToQuaternary(CollationIterator &iter, const uint8_t *compressibleBytes,
		const CollationSettings &settings, ByteBuffer &sink) {
	auto options = settings.options;
	auto levels = levelMaskForStrength(CollationSettings::getStrength(options));
	if ((options & CollationSettings::CaseLevelBit) != 0) {
		levels |= CaseLevelFlag;
	}
	if (levels == 0) {
		return;
	}

	uint32_t variableTop;
	if ((options & CollationSettings::AlternateMask) == 0) {
		variableTop = 0;
	} else {
		variableTop = settings.variableTop + 1;
	}

	auto tertiaryMask = CollationSettings::getTertiaryMask(options);

	ByteBuffer cases;
	ByteBuffer secondaries;
	ByteBuffer tertiaries;
	ByteBuffer quaternaries;

	uint32_t prevReorderedPrimary = 0; // 0 means no compression is running
	int32_t commonCases = 0;
	int32_t commonSecondaries = 0;
	int32_t commonTertiaries = 0;
	int32_t commonQuaternaries = 0;

	uint32_t prevSecondary = 0;
	int32_t secSegmentStart = 0;

	for (;;) {
		// The CEs need not be kept once their weights are written.
		iter.clearCEsIfNoneRemaining();
		auto ce = iter.nextCE();
		auto p = uint32_t(ce >> 32);
		if (p < variableTop && p > MergeSeparatorPrimary) {
			// A variable CE: it moves to the quaternary level, and so do the
			// variable CEs and primary ignorables that follow it.
			if (commonQuaternaries != 0) {
				--commonQuaternaries;
				while (commonQuaternaries >= QuatCommonMaxCount) {
					quaternaries.appendByte(QuatCommonMiddle);
					commonQuaternaries -= QuatCommonMaxCount;
				}
				// A shifted primary is below the common weight.
				quaternaries.appendByte(QuatCommonLow + uint32_t(commonQuaternaries));
				commonQuaternaries = 0;
			}
			do {
				if ((levels & QuaternaryLevelFlag) != 0) {
					if (settings.hasReordering()) {
						p = settings.reorder(p);
					}
					if ((p >> 24) >= QuatShiftedLimitByte) {
						// Keep shifted lead bytes out of the compression range.
						quaternaries.appendByte(QuatShiftedLimitByte);
					}
					quaternaries.appendWeight32(p);
				}
				do {
					ce = iter.nextCE();
					p = uint32_t(ce >> 32);
				} while (p == 0);
			} while (p < variableTop && p > MergeSeparatorPrimary);
		}
		// ce is now primary-ignorable, NoCE, the merge separator or a regular
		// primary - but not variable. At NoCE nothing is written for the primary
		// level, but every level's compression is terminated before the loop ends.
		if (p > NoCEPrimary && (levels & PrimaryLevelFlag) != 0) {
			// Compressibility is a property of the *un*-reordered lead byte.
			bool isCompressible = compressibleBytes[p >> 24] != 0;
			if (settings.hasReordering()) {
				p = settings.reorder(p);
			}
			auto p1 = p >> 24;
			if (!isCompressible || p1 != (prevReorderedPrimary >> 24)) {
				if (prevReorderedPrimary != 0) {
					if (p < prevReorderedPrimary) {
						// No compression terminator at the end of a level or segment.
						if (p1 > MergeSeparatorByte) {
							sink.appendByte(PrimaryCompressionLowByte);
						}
					} else {
						sink.appendByte(PrimaryCompressionHighByte);
					}
				}
				sink.appendByte(p1);
				prevReorderedPrimary = isCompressible ? p : 0;
			}
			auto p2 = uint8_t(p >> 16);
			if (p2 != 0) {
				uint8_t bytes[3] = {p2, uint8_t(p >> 8), uint8_t(p)};
				sink.append(bytes, bytes[1] == 0 ? 1 : (bytes[2] == 0 ? 2 : 3));
			}
		}

		auto lower32 = uint32_t(ce);
		if (lower32 == 0) {
			continue; // completely ignorable
		}

		if ((levels & SecondaryLevelFlag) != 0) {
			auto s = lower32 >> 16;
			if (s == 0) {
				// secondary ignorable
			} else if (s == CommonWeight16
					&& ((options & CollationSettings::BackwardSecondary) == 0
							|| p != MergeSeparatorPrimary)) {
				++commonSecondaries;
			} else if ((options & CollationSettings::BackwardSecondary) == 0) {
				if (commonSecondaries != 0) {
					--commonSecondaries;
					while (commonSecondaries >= SecCommonMaxCount) {
						secondaries.appendByte(SecCommonMiddle);
						commonSecondaries -= SecCommonMaxCount;
					}
					// Which side of the common value the run is written on says
					// which way the weight that ended it goes.
					auto b = s < CommonWeight16 ? SecCommonLow + uint32_t(commonSecondaries)
												: SecCommonHigh - uint32_t(commonSecondaries);
					secondaries.appendByte(b);
					commonSecondaries = 0;
				}
				secondaries.appendWeight16(s);
			} else {
				if (commonSecondaries != 0) {
					--commonSecondaries;
					// Reversed, like the weights: the segment is re-reversed below.
					auto remainder = commonSecondaries % SecCommonMaxCount;
					auto b = prevSecondary < CommonWeight16
							? SecCommonLow + uint32_t(remainder)
							: SecCommonHigh - uint32_t(remainder);
					secondaries.appendByte(b);
					commonSecondaries -= remainder;
					while (commonSecondaries > 0) {
						secondaries.appendByte(SecCommonMiddle);
						commonSecondaries -= SecCommonMaxCount;
					}
				}
				if (0 < p && p <= MergeSeparatorPrimary) {
					// The backward secondary level runs backwards within segments
					// separated by the merge separator, so reverse the segment now
					// that it is complete.
					auto secs = secondaries.data();
					auto last = secondaries.length() - 1;
					if (secSegmentStart < last) {
						auto q = secs + secSegmentStart;
						auto r = secs + last;
						do {
							auto b = *q;
							*q++ = *r;
							*r-- = b;
						} while (q < r);
					}
					secondaries.appendByte(
							p == NoCEPrimary ? LevelSeparatorByte : MergeSeparatorByte);
					prevSecondary = 0;
					secSegmentStart = secondaries.length();
				} else {
					secondaries.appendReverseWeight16(s);
					prevSecondary = s;
				}
			}
		}

		if ((levels & CaseLevelFlag) != 0) {
			if ((CollationSettings::getStrength(options) == StrengthPrimary) ? p == 0
																			: lower32 <= 0xFFFF) {
				// Ignore the case weights of primary (or secondary) ignorables, for
				// the reasons spelled out in collation_compare.cc.
			} else {
				uint32_t c = (lower32 >> 8) & 0xFF; // case bits and tertiary lead byte
				if ((c & 0xC0) == 0 && c > LevelSeparatorByte) {
					++commonCases;
				} else {
					if ((options & CollationSettings::UpperFirst) == 0) {
						// lowerFirst: common weights compress into nibbles 1..7..13,
						// mixed case is 14 and upper is 15. A level of nothing but
						// common weights need not be written at all - length
						// differences were already settled one level up.
						if (commonCases != 0 && (c > LevelSeparatorByte || !cases.isEmpty())) {
							--commonCases;
							while (commonCases >= CaseLowerFirstCommonMaxCount) {
								cases.appendByte(CaseLowerFirstCommonMiddle << 4);
								commonCases -= CaseLowerFirstCommonMaxCount;
							}
							auto b = c <= LevelSeparatorByte
									? CaseLowerFirstCommonLow + uint32_t(commonCases)
									: CaseLowerFirstCommonHigh - uint32_t(commonCases);
							cases.appendByte(b << 4);
							commonCases = 0;
						}
						if (c > LevelSeparatorByte) {
							c = (CaseLowerFirstCommonHigh + (c >> 6)) << 4; // 14 or 15
						}
					} else {
						// upperFirst: the common weight is the highest, so the run
						// only ever compresses upward from the low value.
						if (commonCases != 0) {
							--commonCases;
							while (commonCases >= CaseUpperFirstCommonMaxCount) {
								cases.appendByte(CaseUpperFirstCommonLow << 4);
								commonCases -= CaseUpperFirstCommonMaxCount;
							}
							cases.appendByte((CaseUpperFirstCommonLow + uint32_t(commonCases)) << 4);
							commonCases = 0;
						}
						if (c > LevelSeparatorByte) {
							c = (CaseUpperFirstCommonLow - (c >> 6)) << 4; // 2 or 1
						}
					}
					// c is either the separator 01 or a nibble shifted up.
					cases.appendByte(c);
				}
			}
		}

		if ((levels & TertiaryLevelFlag) != 0) {
			auto t = lower32 & tertiaryMask;
			if (t == CommonWeight16) {
				++commonTertiaries;
			} else if ((tertiaryMask & 0x8000) == 0) {
				// No case bits: lead bytes 06..3F move to C6..FF, which leaves a
				// large range for the common weight.
				if (commonTertiaries != 0) {
					--commonTertiaries;
					while (commonTertiaries >= TerOnlyCommonMaxCount) {
						tertiaries.appendByte(TerOnlyCommonMiddle);
						commonTertiaries -= TerOnlyCommonMaxCount;
					}
					auto b = t < CommonWeight16 ? TerOnlyCommonLow + uint32_t(commonTertiaries)
												: TerOnlyCommonHigh - uint32_t(commonTertiaries);
					tertiaries.appendByte(b);
					commonTertiaries = 0;
				}
				if (t > CommonWeight16) {
					t += 0xC000;
				}
				tertiaries.appendWeight16(t);
			} else if ((options & CollationSettings::UpperFirst) == 0) {
				// With caseFirst=lowerFirst: lead bytes 06..BF move to 46..FF.
				if (commonTertiaries != 0) {
					--commonTertiaries;
					while (commonTertiaries >= TerLowerFirstCommonMaxCount) {
						tertiaries.appendByte(TerLowerFirstCommonMiddle);
						commonTertiaries -= TerLowerFirstCommonMaxCount;
					}
					auto b = t < CommonWeight16
							? TerLowerFirstCommonLow + uint32_t(commonTertiaries)
							: TerLowerFirstCommonHigh - uint32_t(commonTertiaries);
					tertiaries.appendByte(b);
					commonTertiaries = 0;
				}
				if (t > CommonWeight16) {
					t += 0x4000;
				}
				tertiaries.appendWeight16(t);
			} else {
				// With caseFirst=upperFirst the case bits are inverted, except for a
				// tertiary CE's artificial uppercase, which has to stay above the
				// case+tertiary weights of primary and secondary CEs:
				//
				//   separator      01 -> 01
				//   lowercase  02..04 -> 82..84   (uncased included)
				//   common         05 -> 85..C5   (the compression range)
				//   lowercase  06..3F -> C6..FF
				//   mixed      42..7F -> 42..7F
				//   uppercase  82..BF -> 02..3F
				//   tertiary   86..BF -> C6..FF
				if (t <= NoCEWeight16) {
					// separators unchanged
				} else if (lower32 > 0xFFFF) {
					t ^= 0xC000;
					if (t < (TerUpperFirstCommonHigh << 8)) {
						t -= 0x4000;
					}
				} else {
					t += 0x4000;
				}
				if (commonTertiaries != 0) {
					--commonTertiaries;
					while (commonTertiaries >= TerUpperFirstCommonMaxCount) {
						tertiaries.appendByte(TerUpperFirstCommonMiddle);
						commonTertiaries -= TerUpperFirstCommonMaxCount;
					}
					auto b = t < (TerUpperFirstCommonLow << 8)
							? TerUpperFirstCommonLow + uint32_t(commonTertiaries)
							: TerUpperFirstCommonHigh - uint32_t(commonTertiaries);
					tertiaries.appendByte(b);
					commonTertiaries = 0;
				}
				tertiaries.appendWeight16(t);
			}
		}

		if ((levels & QuaternaryLevelFlag) != 0) {
			auto q = lower32 & 0xFFFF;
			if ((q & 0xC0) == 0 && q > NoCEWeight16) {
				++commonQuaternaries;
			} else if (q == NoCEWeight16 && (options & CollationSettings::AlternateMask) == 0
					&& quaternaries.isEmpty()) {
				// With alternate=non-ignorable and nothing but common weights there
				// is nothing to write: no shifted primaries are generated, there are
				// exactly as many quaternary weights as tertiary ones, and any
				// above-common weight would compare greater anyway.
				quaternaries.appendByte(LevelSeparatorByte);
			} else {
				if (q == NoCEWeight16) {
					q = LevelSeparatorByte;
				} else {
					q = 0xFC + ((q >> 6) & 3);
				}
				if (commonQuaternaries != 0) {
					--commonQuaternaries;
					while (commonQuaternaries >= QuatCommonMaxCount) {
						quaternaries.appendByte(QuatCommonMiddle);
						commonQuaternaries -= QuatCommonMaxCount;
					}
					auto b = q < QuatCommonLow ? QuatCommonLow + uint32_t(commonQuaternaries)
											   : QuatCommonHigh - uint32_t(commonQuaternaries);
					quaternaries.appendByte(b);
					commonQuaternaries = 0;
				}
				quaternaries.appendByte(q);
			}
		}

		if ((lower32 >> 24) == LevelSeparatorByte) {
			break; // ce == NoCE
		}
	}

	if ((levels & SecondaryLevelFlag) != 0) {
		sink.appendByte(LevelSeparatorByte);
		secondaries.appendTo(sink);
	}

	if ((levels & CaseLevelFlag) != 0) {
		sink.appendByte(LevelSeparatorByte);
		// Nibble pairs go out as bytes, except separators, which stand alone.
		auto length = cases.length() - 1; // drop the trailing NoCE
		uint8_t b = 0;
		for (int32_t i = 0; i < length; ++i) {
			auto c = cases[i];
			if (b == 0) {
				b = c;
			} else {
				sink.appendByte(uint32_t(b | (c >> 4)));
				b = 0;
			}
		}
		if (b != 0) {
			sink.appendByte(b);
		}
	}

	if ((levels & TertiaryLevelFlag) != 0) {
		sink.appendByte(LevelSeparatorByte);
		tertiaries.appendTo(sink);
	}

	if ((levels & QuaternaryLevelFlag) != 0) {
		sink.appendByte(LevelSeparatorByte);
		quaternaries.appendTo(sink);
	}

	if (!secondaries.ok() || !cases.ok() || !tertiaries.ok() || !quaternaries.ok()) {
		sink.appendByte(0); // the caller sees the failure through sink.ok()
	}
}

// The identical level: the NFD form, as UTF-8. Byte order over UTF-8 is code
// point order, which is the whole requirement. U+FFFE is written as the single
// byte 02 so that it stays below everything, exactly as compare() treats it.
static bool writeIdenticalLevel(const CodepointBuffer &nfd, ByteBuffer &sink) {
	sink.appendByte(LevelSeparatorByte);
	for (int32_t i = 0; i < nfd.size(); ++i) {
		auto c = nfd.at(i);
		if (c == 0xFFFE) {
			sink.appendByte(MergeSeparatorByte);
			continue;
		}
		char buf[4];
		size_t written = 0;
		toUtf8(buf, sizeof(buf), c, &written);
		sink.append(reinterpret_cast<const uint8_t *>(buf), int32_t(written));
	}
	return sink.ok();
}

} // namespace sprt::unicode::detail
