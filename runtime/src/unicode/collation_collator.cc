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

// The driver: settings for a locale, and comparing two strings with them. Ported
// from ICU rulebasedcollator.cpp `doCompare` (© Unicode, Inc.;
// http://www.unicode.org/copyright.html).
//
// Two deliberate departures from ICU here, both about correctness over speed:
//
//   FCD checking is always on. ICU leaves it off unless a locale's rules ask for
//     it (UCOL_NORMALIZATION_MODE defaults to off), on the assumption that real
//     text is already FCD - and on the text where that assumption fails it returns
//     an order that is not canonically consistent. The UCA conformance file
//     contains exactly such text. A comparison that is wrong on input nobody
//     checked is not something to ship, and the cost is a bit-table test per
//     character, so the bit is forced on.
//
//   The identical level normalizes both strings to NFD and compares code points,
//     rather than decomposing lazily as ICU does. It is only reached when every
//     other level came out equal, and only when the caller asked for that
//     strength, so the simpler implementation costs nothing measurable and is
//     obviously right.

namespace sprt::unicode::detail {

// The settings for one collation data plus the caller's options.
static CollationSettings makeSettings(const CollationData *data, int32_t dataOptions,
		Strength strength, bool numeric, bool shifted, CaseFirst caseFirst) {
	CollationSettings settings;
	settings.options = dataOptions;

	// Always check FCD - see the file comment.
	settings.options |= CollationSettings::CheckFcd;

	int32_t strengthValue;
	switch (strength) {
	case Strength::Primary: strengthValue = StrengthPrimary; break;
	case Strength::Secondary: strengthValue = StrengthSecondary; break;
	case Strength::Quaternary: strengthValue = StrengthQuaternary; break;
	case Strength::Identical: strengthValue = StrengthIdentical; break;
	case Strength::Tertiary:
	default: strengthValue = StrengthTertiary; break;
	}
	settings.options = (settings.options & ~CollationSettings::StrengthMask)
			| (strengthValue << CollationSettings::StrengthShift);

	if (numeric) {
		settings.options |= CollationSettings::Numeric;
	}
	if (shifted) {
		settings.options = (settings.options & ~CollationSettings::AlternateMask)
				| CollationSettings::Shifted;
	}
	switch (caseFirst) {
	case CaseFirst::Upper:
		settings.options |= CollationSettings::CaseFirstAndUpperMask;
		break;
	case CaseFirst::Lower:
		settings.options = (settings.options & ~CollationSettings::UpperFirst)
				| CollationSettings::CaseFirst;
		break;
	case CaseFirst::Off:
	default:
		break;
	}

	// The variable-top weight follows from maxVariable and the script boundaries;
	// CollationDataReader computes it the same way on load.
	settings.variableTop =
			data->getLastPrimaryForGroup(CollationData::ReorderCodeFirst + settings.getMaxVariable());
	return settings;
}

// Switches the Latin fast path on, if this table has one and these settings are
// the ones its weights were precomputed for.
//
// Those are the default alternate handling and non-numeric collation - see the
// generator. With alternate=shifted the variable threshold moves and every weight
// changes; with numeric collation the digits have to be knocked out. Either way
// the general path answers, correctly and more slowly, which is the right trade
// for two rare options.
static void enableFastLatin(const CollationData *data, CollationSettings &settings) {
	if (data->fastLatinTable == nullptr || data->fastLatinPrimaries == nullptr
			|| (settings.options & CollationSettings::AlternateMask) != 0
			|| settings.isNumeric()) {
		settings.fastLatinOptions = -1;
		return;
	}
	settings.fastLatinPrimaries = data->fastLatinPrimaries;
	// The threshold sits just below the lowest long mini primary, and rides above
	// the options word exactly as CollationFastLatin::getOptions packs it.
	settings.fastLatinOptions = int32_t(uint32_t(FastLatin::MinLong - 1) << 16)
			| (settings.options & 0xFFFF);
}

// --- the identical level -------------------------------------------------------

// Compares two NFD code point sequences. U+FFFE, the merge separator, sorts below
// everything, as it does on every other level.
static int32_t compareNfdCodePoints(const CodepointBuffer &left, const CodepointBuffer &right) {
	auto leftLength = left.size();
	auto rightLength = right.size();
	auto length = leftLength < rightLength ? leftLength : rightLength;
	for (int32_t i = 0; i < length; ++i) {
		auto l = int64_t(left.at(i));
		auto r = int64_t(right.at(i));
		if (l == r) {
			continue;
		}
		if (l == 0xFFFE) {
			l = -1;
		}
		if (r == 0xFFFE) {
			r = -1;
		}
		return l < r ? CompareLess : CompareGreater;
	}
	if (leftLength == rightLength) {
		return CompareEqual;
	}
	return leftLength < rightLength ? CompareLess : CompareGreater;
}

// NFD of UTF-8 text, as code points, for the identical level. Ill-formed input
// becomes U+FFFD, the same as everywhere else on the UTF-8 path.
static bool nfdFromUtf8(const uint8_t *s, int32_t length, CodepointBuffer &dest) {
	NormBuffer utf16;
	int32_t i = 0;
	while (i < length) {
		if (!utf16.appendCodePoint(char32_t(u8NextOrFFFD(s, i, length)))) {
			return false;
		}
	}
	NormBuffer normalized;
	return normalizeNfd(utf16.data(), utf16.size(), normalized, dest);
}

// --- comparing two UTF-16 strings ----------------------------------------------

// Returns the sign, or 0. `failed` is set when an allocation failed and the
// result is meaningless.
static int32_t compareUtf16(const CollationData *data, const CollationSettings &settings,
		const char16_t *left, int32_t leftLength, const char16_t *right, int32_t rightLength,
		bool &failed) {
	failed = false;
	if (left == right && leftLength == rightLength) {
		return CompareEqual;
	}

	// Skip the identical prefix: the CEs of a shared prefix cannot differ.
	int32_t equalPrefixLength = 0;
	for (;;) {
		if (equalPrefixLength == leftLength) {
			if (equalPrefixLength == rightLength) {
				return CompareEqual;
			}
			break;
		}
		if (equalPrefixLength == rightLength
				|| left[equalPrefixLength] != right[equalPrefixLength]) {
			break;
		}
		++equalPrefixLength;
	}

	auto numeric = settings.isNumeric();
	if (equalPrefixLength > 0) {
		// The prefix may end in the middle of a contraction or of a sequence that
		// reorders, in which case back up to where one can start reading again.
		if ((equalPrefixLength != leftLength
					&& data->isUnsafeBackward(left[equalPrefixLength], numeric))
				|| (equalPrefixLength != rightLength
						&& data->isUnsafeBackward(right[equalPrefixLength], numeric))) {
			while (--equalPrefixLength > 0
					&& data->isUnsafeBackward(left[equalPrefixLength], numeric)) { }
		}
		// Note: a longer string can compare equal to a prefix of itself when only
		// ignorables follow, and with a backward level it can even compare less.
		//
		// The iterators get the real start of each string as well as the prefix
		// position, so that prefix matching can look back into the equal prefix.
	}

	// The Latin fast path, when it applies to both strings from the prefix on. It
	// answers BailOutResult for anything it cannot handle, and then the general
	// path runs - so this can cost time but never correctness.
	int32_t result = FastLatin::BailOutResult;
	if (settings.fastLatinOptions >= 0
			&& (equalPrefixLength == leftLength
					|| left[equalPrefixLength] <= FastLatin::LatinMax)
			&& (equalPrefixLength == rightLength
					|| right[equalPrefixLength] <= FastLatin::LatinMax)) {
		result = FastLatin::compareUtf16(data->fastLatinTable, settings.fastLatinPrimaries,
				settings.fastLatinOptions, left + equalPrefixLength,
				leftLength - equalPrefixLength, right + equalPrefixLength,
				rightLength - equalPrefixLength);
	}

	if (result == FastLatin::BailOutResult) {
		FCDUTF16CollationIterator leftIter(data, numeric, left, left + equalPrefixLength,
				left + leftLength);
		FCDUTF16CollationIterator rightIter(data, numeric, right, right + equalPrefixLength,
				right + rightLength);
		result = compareUpToQuaternary(leftIter, rightIter, settings);
		if (leftIter.failed() || rightIter.failed()) {
			failed = true;
			return CompareEqual;
		}
	}
	if (result != CompareEqual || settings.getStrength() < StrengthIdentical) {
		return result;
	}

	// The identical level.
	NormBuffer leftNfd, rightNfd;
	CodepointBuffer leftCps, rightCps;
	if (!normalizeNfd(left + equalPrefixLength, leftLength - equalPrefixLength, leftNfd, leftCps)
			|| !normalizeNfd(right + equalPrefixLength, rightLength - equalPrefixLength, rightNfd,
					rightCps)) {
		failed = true;
		return CompareEqual;
	}
	return compareNfdCodePoints(leftCps, rightCps);
}

// --- comparing two UTF-8 strings -----------------------------------------------
//
// The same shape as compareUtf16, over the native UTF-8 iterators. The prefix
// skip works on bytes, which is safe: two UTF-8 strings share a byte prefix
// exactly when they share the characters in it, and the backing-up loop below
// then moves to a character boundary.

static int32_t compareUtf8(const CollationData *data, const CollationSettings &settings,
		const uint8_t *left, int32_t leftLength, const uint8_t *right, int32_t rightLength,
		bool &failed) {
	failed = false;
	if (left == right && leftLength == rightLength) {
		return CompareEqual;
	}

	int32_t equalPrefixLength = 0;
	for (;;) {
		if (equalPrefixLength == leftLength) {
			if (equalPrefixLength == rightLength) {
				return CompareEqual;
			}
			break;
		}
		if (equalPrefixLength == rightLength
				|| left[equalPrefixLength] != right[equalPrefixLength]) {
			break;
		}
		++equalPrefixLength;
	}

	// Back up to the start of a partially-equal code point: the byte prefix may end
	// in the middle of a sequence.
	if (equalPrefixLength > 0
			&& ((equalPrefixLength != leftLength && isU8Trail(left[equalPrefixLength]))
					|| (equalPrefixLength != rightLength
							&& isU8Trail(right[equalPrefixLength])))) {
		while (--equalPrefixLength > 0 && isU8Trail(left[equalPrefixLength])) { }
	}

	auto numeric = settings.isNumeric();
	if (equalPrefixLength > 0) {
		bool unsafe = false;
		if (equalPrefixLength != leftLength) {
			int32_t i = equalPrefixLength;
			unsafe = data->isUnsafeBackward(char32_t(u8NextOrFFFD(left, i, leftLength)), numeric);
		}
		if (!unsafe && equalPrefixLength != rightLength) {
			int32_t i = equalPrefixLength;
			unsafe = data->isUnsafeBackward(char32_t(u8NextOrFFFD(right, i, rightLength)), numeric);
		}
		if (unsafe) {
			// Back up to the start of a contraction or a reordering sequence. The
			// step happens before the test, so at least one character is always
			// given back - which is the point: the character at the boundary is the
			// one that cannot be started from.
			int32_t c;
			do {
				c = u8PrevOrFFFD(left, 0, equalPrefixLength);
			} while (equalPrefixLength > 0 && data->isUnsafeBackward(char32_t(c), numeric));
		}
	}

	int32_t result = FastLatin::BailOutResult;
	if (settings.fastLatinOptions >= 0
			&& (equalPrefixLength == leftLength
					|| left[equalPrefixLength] <= FastLatin::LatinMaxUtf8Lead)
			&& (equalPrefixLength == rightLength
					|| right[equalPrefixLength] <= FastLatin::LatinMaxUtf8Lead)) {
		result = FastLatin::compareUtf8(data->fastLatinTable, settings.fastLatinPrimaries,
				settings.fastLatinOptions, left + equalPrefixLength,
				leftLength - equalPrefixLength, right + equalPrefixLength,
				rightLength - equalPrefixLength);
	}

	if (result == FastLatin::BailOutResult) {
		FCDUTF8CollationIterator leftIter(data, numeric, left, equalPrefixLength, leftLength);
		FCDUTF8CollationIterator rightIter(data, numeric, right, equalPrefixLength, rightLength);
		result = compareUpToQuaternary(leftIter, rightIter, settings);
		if (leftIter.failed() || rightIter.failed()) {
			failed = true;
			return CompareEqual;
		}
	}
	if (result != CompareEqual || settings.getStrength() < StrengthIdentical) {
		return result;
	}

	// The identical level, over the same NFD comparison as UTF-16: decode to code
	// points, decompose, compare.
	CodepointBuffer leftCps, rightCps;
	if (!nfdFromUtf8(left + equalPrefixLength, leftLength - equalPrefixLength, leftCps)
			|| !nfdFromUtf8(right + equalPrefixLength, rightLength - equalPrefixLength, rightCps)) {
		failed = true;
		return CompareEqual;
	}
	return compareNfdCodePoints(leftCps, rightCps);
}

} // namespace sprt::unicode::detail
