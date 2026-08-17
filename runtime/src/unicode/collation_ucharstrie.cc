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

// Incremental string matching over a serialized char16_t trie. Ported from ICU
// ucharstrie.h and ucharstrie.cpp (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// This is how contractions and prefixes are matched: the collation data's
// `contexts` array holds one of these tries per contracting character, and the
// iterator feeds it code points until it stops matching. "ch" sorting as one
// letter in Czech, "dzs" in Hungarian and the 477 Thai and Lao contractions are
// all this table.
//
// Only the reader is here, and only the part collation calls: matching one code
// point at a time, reading the value at a match, and saving and restoring a
// position (which discontiguous contraction matching needs). Gone from the ICU
// original: the builder, `next(string)`, `hasUniqueValue`, `getNextUChars`, the
// full iterator over all pairs, and the ownership of the array - ours always
// points into a constant table.
//
// The node encoding below is transcribed literally, thresholds included. A trie
// walked with one threshold wrong does not fail: it matches a different string,
// and two words swap places somewhere in the middle of a sorted list.

namespace sprt::unicode::detail {

// UStringTrieResult. The numeric values matter: `matches` is != NoMatch,
// `hasValue` is >= FinalValue, and `hasNext` is the low bit.
enum class TrieResult : int32_t {
	NoMatch = 0,
	NoValue = 1,
	FinalValue = 2,
	IntermediateValue = 3,
};

static constexpr bool trieMatches(TrieResult r) { return r != TrieResult::NoMatch; }

static constexpr bool trieHasValue(TrieResult r) {
	return int32_t(r) >= int32_t(TrieResult::FinalValue);
}

static constexpr bool trieHasNext(TrieResult r) { return (int32_t(r) & 1) != 0; }

class UCharsTrie {
public:
	// Node lead unit ranges. 0000..002F is a branch (length node+1, or one more
	// than the next unit when node == 0).
	enum : int32_t {
		// A branch sub-node with at most this many entries drops to linear search.
		MaxBranchLinearSubNodeLength = 5,

		// 0030..003F: linear-match node, matching 1..16 units.
		MinLinearMatch = 0x30,
		MaxLinearMatchLength = 0x10,

		// Bits 14..6 of a match-node lead unit are an optional intermediate value;
		// zero means there is none.
		MinValueLead = MinLinearMatch + MaxLinearMatchLength, // 0x0040
		NodeTypeMask = MinValueLead - 1, // 0x003F

		// A final-value node has bit 15 set.
		ValueIsFinal = 0x8000,

		// Compact value, after masking off bit 15.
		MaxOneUnitValue = 0x3FFF,
		MinTwoUnitValueLead = MaxOneUnitValue + 1, // 0x4000
		ThreeUnitValueLead = 0x7FFF,

		// Compact intermediate value, sharing a lead unit with its node.
		MaxOneUnitNodeValue = 0xFF,
		MinTwoUnitNodeValueLead = MinValueLead + ((MaxOneUnitNodeValue + 1) << 6), // 0x4040
		ThreeUnitNodeValueLead = 0x7FC0,

		// Compact delta.
		MaxOneUnitDelta = 0xFBFF,
		MinTwoUnitDeltaLead = MaxOneUnitDelta + 1, // 0xFC00
		ThreeUnitDeltaLead = 0xFFFF,
	};

	// A saved position, for backing out of a partial match.
	struct State {
		const char16_t *pos = nullptr;
		int32_t remainingMatchLength = -1;
	};

	explicit UCharsTrie(const char16_t *trieUChars)
	: _uchars(trieUChars), _pos(trieUChars), _remainingMatchLength(-1) { }

	// Back to the root, forgetting any partial match.
	void reset() {
		_pos = _uchars;
		_remainingMatchLength = -1;
	}

	void saveState(State &state) const {
		state.pos = _pos;
		state.remainingMatchLength = _remainingMatchLength;
	}

	void resetToState(const State &state) {
		_pos = state.pos;
		_remainingMatchLength = state.remainingMatchLength;
	}

	// Whether the string so far matches, has a value, and can continue.
	TrieResult current() const {
		auto pos = _pos;
		if (pos == nullptr) {
			return TrieResult::NoMatch;
		}
		int32_t node;
		return (_remainingMatchLength < 0 && (node = *pos) >= MinValueLead)
				? valueResult(node)
				: TrieResult::NoValue;
	}

	// The value at a match. Only valid immediately after a result with a value.
	int32_t getValue() const {
		auto pos = _pos;
		int32_t leadUnit = *pos++;
		return (leadUnit & ValueIsFinal) ? readValue(pos, leadUnit & 0x7FFF)
										 : readNodeValue(pos, leadUnit);
	}

	TrieResult first(int32_t unit) {
		_remainingMatchLength = -1;
		return nextImpl(_uchars, unit);
	}

	TrieResult next(int32_t unit) {
		auto pos = _pos;
		if (pos == nullptr) {
			return TrieResult::NoMatch;
		}
		auto length = _remainingMatchLength; // the real remaining length minus 1
		if (length >= 0) {
			// Inside a linear-match node.
			if (unit == *pos++) {
				_remainingMatchLength = --length;
				_pos = pos;
				int32_t node;
				return (length < 0 && (node = *pos) >= MinValueLead) ? valueResult(node)
																	 : TrieResult::NoValue;
			}
			stop();
			return TrieResult::NoMatch;
		}
		return nextImpl(pos, unit);
	}

	TrieResult firstForCodePoint(char32_t cp) {
		return cp <= 0xFFFF
				? first(int32_t(cp))
				: (trieHasNext(first(utf16LeadSurrogate(cp)))
								? next(utf16TrailSurrogate(cp))
								: TrieResult::NoMatch);
	}

	TrieResult nextForCodePoint(char32_t cp) {
		return cp <= 0xFFFF
				? next(int32_t(cp))
				: (trieHasNext(next(utf16LeadSurrogate(cp)))
								? next(utf16TrailSurrogate(cp))
								: TrieResult::NoMatch);
	}

private:
	static constexpr int32_t utf16LeadSurrogate(char32_t cp) {
		return int32_t(0xD7C0 + (cp >> 10));
	}
	static constexpr int32_t utf16TrailSurrogate(char32_t cp) {
		return int32_t(0xDC00 + (cp & 0x3FF));
	}

	void stop() { _pos = nullptr; }

	static constexpr TrieResult valueResult(int32_t node) {
		return TrieResult(int32_t(TrieResult::IntermediateValue) - (node >> 15));
	}

	// A compact 32-bit integer; pos is after the lead unit, whose bit 15 is clear.
	static int32_t readValue(const char16_t *pos, int32_t leadUnit) {
		if (leadUnit < MinTwoUnitValueLead) {
			return leadUnit;
		}
		if (leadUnit < ThreeUnitValueLead) {
			return ((leadUnit - MinTwoUnitValueLead) << 16) | *pos;
		}
		return (pos[0] << 16) | pos[1];
	}

	static const char16_t *skipValue(const char16_t *pos, int32_t leadUnit) {
		if (leadUnit >= MinTwoUnitValueLead) {
			pos += leadUnit < ThreeUnitValueLead ? 1 : 2;
		}
		return pos;
	}

	static const char16_t *skipValue(const char16_t *pos) {
		int32_t leadUnit = *pos++;
		return skipValue(pos, leadUnit & 0x7FFF);
	}

	static int32_t readNodeValue(const char16_t *pos, int32_t leadUnit) {
		if (leadUnit < MinTwoUnitNodeValueLead) {
			return (leadUnit >> 6) - 1;
		}
		if (leadUnit < ThreeUnitNodeValueLead) {
			return (((leadUnit & 0x7FC0) - MinTwoUnitNodeValueLead) << 10) | *pos;
		}
		return (pos[0] << 16) | pos[1];
	}

	static const char16_t *skipNodeValue(const char16_t *pos, int32_t leadUnit) {
		if (leadUnit >= MinTwoUnitNodeValueLead) {
			pos += leadUnit < ThreeUnitNodeValueLead ? 1 : 2;
		}
		return pos;
	}

	static const char16_t *jumpByDelta(const char16_t *pos) {
		int32_t delta = *pos++;
		if (delta >= MinTwoUnitDeltaLead) {
			if (delta == ThreeUnitDeltaLead) {
				delta = (pos[0] << 16) | pos[1];
				pos += 2;
			} else {
				delta = ((delta - MinTwoUnitDeltaLead) << 16) | *pos++;
			}
		}
		return pos + delta;
	}

	static const char16_t *skipDelta(const char16_t *pos) {
		int32_t delta = *pos++;
		if (delta >= MinTwoUnitDeltaLead) {
			pos += delta == ThreeUnitDeltaLead ? 2 : 1;
		}
		return pos;
	}

	// A branch node: a binary search down to a linear scan of the last few units.
	TrieResult branchNext(const char16_t *pos, int32_t length, int32_t unit) {
		if (length == 0) {
			length = *pos++;
		}
		++length;
		// `length` is how many units there are to select from.
		while (length > MaxBranchLinearSubNodeLength) {
			if (unit < *pos++) {
				length >>= 1;
				pos = jumpByDelta(pos);
			} else {
				length = length - (length >> 1);
				pos = skipDelta(pos);
			}
		}
		// length >= 2: the loop above only ever halves a length greater than 3.
		do {
			if (unit == *pos++) {
				TrieResult result;
				int32_t node = *pos;
				if (node & ValueIsFinal) {
					// Leave the value for getValue().
					result = TrieResult::FinalValue;
				} else {
					// The non-final value is a jump delta. Inlined readValue,
					// as in ICU, because the position has to advance with it.
					++pos;
					int32_t delta;
					if (node < MinTwoUnitValueLead) {
						delta = node;
					} else if (node < ThreeUnitValueLead) {
						delta = ((node - MinTwoUnitValueLead) << 16) | *pos++;
					} else {
						delta = (pos[0] << 16) | pos[1];
						pos += 2;
					}
					pos += delta;
					node = *pos;
					result = node >= MinValueLead ? valueResult(node) : TrieResult::NoValue;
				}
				_pos = pos;
				return result;
			}
			--length;
			pos = skipValue(pos);
		} while (length > 1);

		if (unit == *pos++) {
			_pos = pos;
			int32_t node = *pos;
			return node >= MinValueLead ? valueResult(node) : TrieResult::NoValue;
		}
		stop();
		return TrieResult::NoMatch;
	}

	// Requires _remainingMatchLength < 0.
	TrieResult nextImpl(const char16_t *pos, int32_t unit) {
		int32_t node = *pos++;
		for (;;) {
			if (node < MinLinearMatch) {
				return branchNext(pos, node, unit);
			} else if (node < MinValueLead) {
				// Match the first of length + 1 units.
				int32_t length = node - MinLinearMatch; // the real length minus 1
				if (unit != *pos++) {
					break;
				}
				_remainingMatchLength = --length;
				_pos = pos;
				return (length < 0 && (node = *pos) >= MinValueLead) ? valueResult(node)
																	 : TrieResult::NoValue;
			} else if (node & ValueIsFinal) {
				// Nothing further to match.
				break;
			} else {
				pos = skipNodeValue(pos, node);
				node &= NodeTypeMask;
			}
		}
		stop();
		return TrieResult::NoMatch;
	}

	const char16_t *_uchars;
	const char16_t *_pos;
	// The remaining length of a linear-match node, minus 1; negative outside one.
	int32_t _remainingMatchLength;
};

} // namespace sprt::unicode::detail
