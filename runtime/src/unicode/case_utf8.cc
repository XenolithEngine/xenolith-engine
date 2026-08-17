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

// Case mapping over UTF-8 strings, without converting to UTF-16 and back. Ported
// from ICU ucasemap.cpp (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// This produces exactly what case_string.cc produces - the test asserts that on
// the whole UCD - and exists only to skip the round trip. Text arrives as UTF-8
// in this codebase, and the trip through UTF-16 costs two transcodes and two
// buffers per call, on a path that the font formatter runs per string.
//
// The UTF-8 loops are the UTF-16 loops with a different fast path: ASCII is one
// byte and looked up directly, U+0080..U+017F is two bytes decoded inline and
// looked up in the same Latin table, most of CJK is recognised by its lead byte
// and skipped without decoding at all, and only the rest reaches the trie.
//
// Two things are deliberately different from the ICU original.
//
//   ICU writes through a `ByteSink`, a virtual interface with a borrowable
//   scratch buffer (unicode/bytestream.h plus bytesinkutil.cpp, ~500 lines).
//   This writes into the same dest/destIndex/destCapacity triple the UTF-16 side
//   uses, so the four ByteSinkUtil operations become four small functions and the
//   preflight contract is the same in both files.
//
//   `Edits` is not ported; see the note in case_string.cc.
//
// The UTF-8 decoder here is NOT unicode::utf8Decode32. That one implements the
// original permissive RFC 2279 transform on purpose - it accepts overlong forms,
// surrogates and 5/6-byte sequences, and it is documented as leaving validation
// to the trust boundary. A case mapper must not be that decoder's caller: an
// overlong encoding of 'A' would decode to U+0041, get lowercased, and be written
// back out as a single well-formed 'a', quietly turning malformed input into
// valid text. ICU's U8_NEXT reports ill-formed input instead, and the loops below
// rely on that to pass those bytes through untouched, so the strict pair is
// transcribed here.

namespace sprt::unicode::detail {

// --- strict UTF-8 primitives -------------------------------------------------

static constexpr bool isUtf8Trail(uint8_t b) { return (b & 0xC0) == 0x80; }

// U8_NEXT: decode forward from `i`, advancing it. Returns a negative value for
// ill-formed input, having advanced `i` past the maximal subpart, so the caller
// resyncs at the offending byte rather than skipping past it.
static int32_t utf8NextStrict(const uint8_t *s, int32_t &i, int32_t length) {
	uint8_t lead = s[i++];
	if (lead < 0x80) {
		return lead;
	}

	int32_t trailCount;
	int32_t c;
	if (lead >= 0xC2 && lead <= 0xDF) {
		trailCount = 1;
		c = lead & 0x1F;
	} else if (lead >= 0xE0 && lead <= 0xEF) {
		trailCount = 2;
		c = lead & 0x0F;
	} else if (lead >= 0xF0 && lead <= 0xF4) {
		trailCount = 3;
		c = lead & 0x07;
	} else {
		// A continuation byte on its own, 0xC0/0xC1 (only ever overlong), or
		// 0xF5..0xFF (only ever above U+10FFFF).
		return -1;
	}

	for (int32_t k = 0; k < trailCount; ++k) {
		if (i >= length) {
			return -1; // truncated
		}
		uint8_t trail = s[i];
		if (!isUtf8Trail(trail)) {
			return -1;
		}
		if (k == 0) {
			// The lead byte alone does not pin down the range: these four cases
			// are decided by the first trail byte. Without them an overlong form
			// or a surrogate would decode to a perfectly ordinary code point.
			if ((lead == 0xE0 && trail < 0xA0) // overlong 3-byte form
					|| (lead == 0xED && trail > 0x9F) // surrogate
					|| (lead == 0xF0 && trail < 0x90) // overlong 4-byte form
					|| (lead == 0xF4 && trail > 0x8F)) { // above U+10FFFF
				return -1;
			}
		}
		c = (c << 6) | (trail & 0x3F);
		++i;
	}
	return c;
}

// U8_PREV: decode backward from `i`, retreating it. `start` bounds the scan.
// Finds the lead byte, then decodes forward and requires the sequence to end
// exactly where it started - which rejects a truncated sequence butting up
// against the position, as ICU's does.
static int32_t utf8PrevStrict(const uint8_t *s, int32_t start, int32_t &i) {
	int32_t limit = i;
	int32_t lead = limit - 1;
	while (lead > start && isUtf8Trail(s[lead]) && (limit - lead) < 4) { --lead; }

	int32_t end = lead;
	int32_t c = utf8NextStrict(s, end, limit);
	if (c < 0 || end != limit) {
		i = limit - 1; // ill-formed: step back over one byte
		return -1;
	}
	i = lead;
	return c;
}

// --- output ------------------------------------------------------------------
//
// ByteSinkUtil, over a plain buffer. As on the UTF-16 side, the returned index
// may run past destCapacity - that is how the preflight pass measures - and a
// negative return means the index overflowed int32_t.

static int32_t appendUnchangedUtf8(char *dest, int32_t destIndex, int32_t destCapacity,
		const uint8_t *s, int32_t length) {
	if (length <= 0) {
		return destIndex;
	}
	if (length > (Max<int32_t> - destIndex)) {
		return -1;
	}
	if ((destIndex + length) <= destCapacity) {
		::__sprt_memcpy(dest + destIndex, s, size_t(length));
	}
	return destIndex + length;
}

// U8_APPEND_UNSAFE for one code point.
static int32_t appendCodePointUtf8(char *dest, int32_t destIndex, int32_t destCapacity,
		char32_t c) {
	auto length = int32_t(utf8EncodeLength(c));
	if (length > (Max<int32_t> - destIndex)) {
		return -1;
	}
	if ((destIndex + length) <= destCapacity) {
		utf8EncodeBuf(dest + destIndex, size_t(length), c);
	}
	return destIndex + length;
}

// The two-byte form, for the U+0080..U+07FF fast path where the length is known.
static int32_t appendTwoBytesUtf8(char *dest, int32_t destIndex, int32_t destCapacity, char32_t c) {
	if (destIndex > (Max<int32_t> - 2)) {
		return -1;
	}
	if ((destIndex + 2) <= destCapacity) {
		dest[destIndex] = char((c >> 6) | 0xC0);
		dest[destIndex + 1] = char((c & 0x3F) | 0x80);
	}
	return destIndex + 2;
}

// ByteSinkUtil::appendChange: a mapping result that arrived as UTF-16, written
// out as UTF-8.
static int32_t appendChangeUtf8(char *dest, int32_t destIndex, int32_t destCapacity,
		const char16_t *s16, int32_t s16Length) {
	for (int32_t i = 0; i < s16Length && destIndex >= 0;) {
		char32_t c = u16Next(s16, i, s16Length);
		destIndex = appendCodePointUtf8(dest, destIndex, destCapacity, c);
	}
	return destIndex;
}

// Appends a full case mapping result, see MaxStringLength.
static int32_t appendResultUtf8(char *dest, int32_t destIndex, int32_t destCapacity, int32_t result,
		const char16_t *s) {
	if (result < 0) {
		// (not) original code point
		return appendCodePointUtf8(dest, destIndex, destCapacity, char32_t(~result));
	} else if (result <= MaxStringLength) {
		// string: "result" is the UTF-16 length
		return appendChangeUtf8(dest, destIndex, destCapacity, s, result);
	} else {
		return appendCodePointUtf8(dest, destIndex, destCapacity, char32_t(result));
	}
}

// --- context -----------------------------------------------------------------

// utf8_caseContextIterator: the same walk as the UTF-16 one, over bytes.
static int32_t utf8CaseContextIterator(void *context, int8_t dir) {
	auto csc = static_cast<CaseContext *>(context);

	if (dir < 0) {
		// reset for backward iteration
		csc->index = csc->cpStart;
		csc->dir = dir;
	} else if (dir > 0) {
		// reset for forward iteration
		csc->index = csc->cpLimit;
		csc->dir = dir;
	} else {
		// continue current iteration direction
		dir = csc->dir;
	}

	auto s = static_cast<const uint8_t *>(csc->p);
	if (dir < 0) {
		if (csc->start < csc->index) {
			return utf8PrevStrict(s, csc->start, csc->index);
		}
	} else {
		if (csc->index < csc->limit) {
			return utf8NextStrict(s, csc->index, csc->limit);
		}
	}
	return -1; // U_SENTINEL
}

// --- lowercasing and folding -------------------------------------------------

/**
 * `fold` false: lowercases [srcStart..srcLimit[ but takes context [0..srcLength[
 * into account. `fold` true: case-folds [srcStart..srcLimit[.
 */
static int32_t toLowerUtf8(CaseLocale caseLocale, bool fold, uint32_t options, char *dest,
		int32_t destCapacity, const uint8_t *src, CaseContext *csc, int32_t srcStart,
		int32_t srcLimit) {
	const int8_t *latinToLower;
	if (fold) {
		latinToLower = (options & FoldCaseOptionsMask) == FoldCaseDefault
				? s_caseLatinToLowerNormal
				: s_caseLatinToLowerTrLt;
	} else {
		latinToLower = (caseLocale == CaseLocale::Turkish || caseLocale == CaseLocale::Lithuanian)
				? s_caseLatinToLowerTrLt
				: s_caseLatinToLowerNormal;
	}
	int32_t destIndex = 0;
	int32_t prev = srcStart;
	int32_t srcIndex = srcStart;
	for (;;) {
		// fast path for simple cases
		int32_t cpStart = 0;
		int32_t c;
		for (;;) {
			if (destIndex < 0 || srcIndex >= srcLimit) {
				c = -1;
				break;
			}
			uint8_t lead = src[srcIndex++];
			if (lead <= 0x7f) {
				int8_t d = latinToLower[lead];
				if (d == s_caseLatinExc) {
					cpStart = srcIndex - 1;
					c = lead;
					break;
				}
				if (d == 0) {
					continue;
				}
				destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
						srcIndex - 1 - prev);
				if (destIndex >= 0) {
					// The Latin deltas never cross the ASCII boundary, so this is
					// still one byte.
					destIndex = appendCodePointUtf8(dest, destIndex, destCapacity,
							char32_t(lead + d));
				}
				prev = srcIndex;
				continue;
			} else if (lead < 0xe3) {
				uint8_t t;
				if (0xc2 <= lead && lead <= 0xc5 && srcIndex < srcLimit
						&& (t = uint8_t(src[srcIndex] - 0x80)) <= 0x3f) {
					// U+0080..U+017F
					++srcIndex;
					c = ((lead - 0xc0) << 6) | t;
					int8_t d = latinToLower[c];
					if (d == s_caseLatinExc) {
						cpStart = srcIndex - 2;
						break;
					}
					if (d == 0) {
						continue;
					}
					destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
							srcIndex - 2 - prev);
					if (destIndex >= 0) {
						destIndex = appendTwoBytesUtf8(dest, destIndex, destCapacity,
								char32_t(c + d));
					}
					prev = srcIndex;
					continue;
				}
			} else if ((lead <= 0xe9 || lead == 0xeb || lead == 0xec) && (srcIndex + 2) <= srcLimit
					&& isUtf8Trail(src[srcIndex]) && isUtf8Trail(src[srcIndex + 1])) {
				// most of CJK: no case mappings
				srcIndex += 2;
				continue;
			}
			cpStart = --srcIndex;
			c = utf8NextStrict(src, srcIndex, srcLimit);
			if (c < 0) {
				// ill-formed UTF-8: leave the bytes in the unchanged run
				continue;
			}
			uint16_t props = s_caseTrie.get(char32_t(c));
			if (hasException(props)) {
				break;
			}
			int32_t delta;
			if (!isUpperOrTitle(props) || (delta = getDelta(props)) == 0) {
				continue;
			}
			destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
					cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendCodePointUtf8(dest, destIndex, destCapacity, char32_t(c + delta));
			}
			prev = srcIndex;
		}
		if (c < 0) {
			break;
		}
		// slow path
		const char16_t *s = nullptr;
		int32_t result;
		if (!fold) {
			csc->cpStart = cpStart;
			csc->cpLimit = srcIndex;
			result = toFullLower(char32_t(c), utf8CaseContextIterator, csc, &s, caseLocale);
		} else {
			result = toFullFolding(char32_t(c), &s, options);
		}
		if (result >= 0) {
			destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
					cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendResultUtf8(dest, destIndex, destCapacity, result, s);
			}
			prev = srcIndex;
		}
	}
	if (destIndex < 0) {
		return -1;
	}
	return appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev, srcIndex - prev);
}

// --- uppercasing -------------------------------------------------------------

static int32_t toUpperUtf8(CaseLocale caseLocale, char *dest, int32_t destCapacity,
		const uint8_t *src, CaseContext *csc, int32_t srcLength) {
	const int8_t *latinToUpper = caseLocale == CaseLocale::Turkish ? s_caseLatinToUpperTr
																   : s_caseLatinToUpperNormal;
	int32_t destIndex = 0;
	int32_t prev = 0;
	int32_t srcIndex = 0;
	for (;;) {
		// fast path for simple cases
		int32_t cpStart = 0;
		int32_t c;
		for (;;) {
			if (destIndex < 0 || srcIndex >= srcLength) {
				c = -1;
				break;
			}
			uint8_t lead = src[srcIndex++];
			if (lead <= 0x7f) {
				int8_t d = latinToUpper[lead];
				if (d == s_caseLatinExc) {
					cpStart = srcIndex - 1;
					c = lead;
					break;
				}
				if (d == 0) {
					continue;
				}
				destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
						srcIndex - 1 - prev);
				if (destIndex >= 0) {
					// The Latin deltas never cross the ASCII boundary, so this is
					// still one byte.
					destIndex = appendCodePointUtf8(dest, destIndex, destCapacity,
							char32_t(lead + d));
				}
				prev = srcIndex;
				continue;
			} else if (lead < 0xe3) {
				uint8_t t;
				if (0xc2 <= lead && lead <= 0xc5 && srcIndex < srcLength
						&& (t = uint8_t(src[srcIndex] - 0x80)) <= 0x3f) {
					// U+0080..U+017F
					++srcIndex;
					c = ((lead - 0xc0) << 6) | t;
					int8_t d = latinToUpper[c];
					if (d == s_caseLatinExc) {
						cpStart = srcIndex - 2;
						break;
					}
					if (d == 0) {
						continue;
					}
					destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
							srcIndex - 2 - prev);
					if (destIndex >= 0) {
						destIndex = appendTwoBytesUtf8(dest, destIndex, destCapacity,
								char32_t(c + d));
					}
					prev = srcIndex;
					continue;
				}
			} else if ((lead <= 0xe9 || lead == 0xeb || lead == 0xec) && (srcIndex + 2) <= srcLength
					&& isUtf8Trail(src[srcIndex]) && isUtf8Trail(src[srcIndex + 1])) {
				// most of CJK: no case mappings
				srcIndex += 2;
				continue;
			}
			cpStart = --srcIndex;
			c = utf8NextStrict(src, srcIndex, srcLength);
			if (c < 0) {
				// ill-formed UTF-8: leave the bytes in the unchanged run
				continue;
			}
			uint16_t props = s_caseTrie.get(char32_t(c));
			if (hasException(props)) {
				break;
			}
			int32_t delta;
			if (caseType(props) != CaseLower || (delta = getDelta(props)) == 0) {
				continue;
			}
			destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
					cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendCodePointUtf8(dest, destIndex, destCapacity, char32_t(c + delta));
			}
			prev = srcIndex;
		}
		if (c < 0) {
			break;
		}
		// slow path
		csc->cpStart = cpStart;
		csc->cpLimit = srcIndex;
		const char16_t *s = nullptr;
		int32_t result = toFullUpper(char32_t(c), utf8CaseContextIterator, csc, &s, caseLocale);
		if (result >= 0) {
			destIndex = appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev,
					cpStart - prev);
			if (destIndex >= 0) {
				destIndex = appendResultUtf8(dest, destIndex, destCapacity, result, s);
			}
			prev = srcIndex;
		}
	}
	if (destIndex < 0) {
		return -1;
	}
	return appendUnchangedUtf8(dest, destIndex, destCapacity, src + prev, srcIndex - prev);
}

// --- Greek uppercasing -------------------------------------------------------

// ICU has no UTF-8 Greek state machine: ucasemap_internalUTF8ToUpper checks for
// the Greek locale and hands the whole string to the UTF-16 path. Doing the same
// keeps one implementation of the rules rather than two.
static int32_t greekToUpperUtf8(char *dest, int32_t destCapacity, const uint8_t *src,
		int32_t srcLength) {
	auto u8 = StringView(reinterpret_cast<const char *>(src), size_t(srcLength));
	int32_t result = -1;
	toUtf16([&](WideStringView wide) {
		if (wide.size() > size_t(Max<int32_t>)) {
			return;
		}
		auto wideLength = int32_t(wide.size());
		auto length = greekToUpper(nullptr, 0, wide.data(), wideLength);
		if (length < 0) {
			return;
		}
		auto buf = __sprt_typed_malloca(char16_t, size_t(length) + 1);
		if (!buf) {
			return;
		}
		if (greekToUpper(buf, length, wide.data(), wideLength) == length) {
			result = appendChangeUtf8(dest, 0, destCapacity, buf, length);
		}
		__sprt_freea(buf);
	}, u8);
	return result;
}

// --- entry points ------------------------------------------------------------

int32_t mapToLowerUtf8(CaseLocale caseLocale, char *dest, int32_t destCapacity, const char *src,
		int32_t srcLength) {
	auto bytes = reinterpret_cast<const uint8_t *>(src);
	CaseContext csc;
	csc.p = bytes;
	csc.limit = srcLength;
	return toLowerUtf8(caseLocale, false, 0, dest, destCapacity, bytes, &csc, 0, srcLength);
}

int32_t mapToUpperUtf8(CaseLocale caseLocale, char *dest, int32_t destCapacity, const char *src,
		int32_t srcLength) {
	auto bytes = reinterpret_cast<const uint8_t *>(src);
	if (caseLocale == CaseLocale::Greek) {
		return greekToUpperUtf8(dest, destCapacity, bytes, srcLength);
	}
	CaseContext csc;
	csc.p = bytes;
	csc.limit = srcLength;
	return toUpperUtf8(caseLocale, dest, destCapacity, bytes, &csc, srcLength);
}

int32_t mapFoldUtf8(uint32_t options, char *dest, int32_t destCapacity, const char *src,
		int32_t srcLength) {
	auto bytes = reinterpret_cast<const uint8_t *>(src);
	return toLowerUtf8(CaseLocale::Root, true, options, dest, destCapacity, bytes, nullptr, 0,
			srcLength);
}

} // namespace sprt::unicode::detail
