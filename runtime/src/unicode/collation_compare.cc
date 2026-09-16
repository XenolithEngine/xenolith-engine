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

// Comparing two CE streams, level by level. Ported from ICU collationcompare.cpp
// (© Unicode, Inc.; http://www.unicode.org/copyright.html).
//
// The UCA compares strings on up to five levels: base letters (primary), accents
// (secondary), case (an optional level of its own), letter variants (tertiary),
// punctuation when it has been shifted out of the primary level (quaternary), and
// finally the code points themselves. The first level that differs decides, which
// is why "resume" sorts before "résumé" but "resume" and "RESUME" differ only at
// the third.
//
// The primary pass is the only one that reads from the iterators: it pulls CEs and
// leaves them buffered, and every later level re-reads the buffer by index. That
// is also why the loops below index rather than iterate - and why a level can walk
// the buffer backwards, which is what French secondary ordering needs.
//
// Ported as-is, with UErrorCode replaced by the iterators' sticky failure flag,
// which the caller checks once at the end.

namespace sprt::unicode::detail {

// Compares up to the quaternary level. The identical level, if the strength asks
// for it, is the caller's job: it compares the text, not the CEs.
static int32_t compareUpToQuaternary(CollationIterator &left, CollationIterator &right,
		const CollationSettings &settings) {
	auto options = settings.options;
	uint32_t variableTop;
	if ((options & CollationSettings::AlternateMask) == 0) {
		variableTop = 0;
	} else {
		// +1 so that "<" can be used and primary ignorables test out early.
		variableTop = settings.variableTop + 1;
	}
	bool anyVariable = false;

	// Fetch CEs and compare primaries, leaving the secondary and tertiary weights
	// buffered for the levels below.
	for (;;) {
		// Pull CEs until a non-ignorable primary or the end of the text.
		uint32_t leftPrimary;
		do {
			auto ce = left.nextCE();
			leftPrimary = uint32_t(ce >> 32);
			if (leftPrimary < variableTop && leftPrimary > MergeSeparatorPrimary) {
				// A variable CE: shift it to the quaternary level, ignore every
				// primary ignorable that follows, and shift further variable CEs.
				anyVariable = true;
				do {
					// Keep only the primary of the variable CE.
					left.setCurrentCE(ce & int64_t(0xFFFF'FFFF'0000'0000ULL));
					for (;;) {
						ce = left.nextCE();
						leftPrimary = uint32_t(ce >> 32);
						if (leftPrimary == 0) {
							left.setCurrentCE(0);
						} else {
							break;
						}
					}
				} while (leftPrimary < variableTop && leftPrimary > MergeSeparatorPrimary);
			}
		} while (leftPrimary == 0);

		uint32_t rightPrimary;
		do {
			auto ce = right.nextCE();
			rightPrimary = uint32_t(ce >> 32);
			if (rightPrimary < variableTop && rightPrimary > MergeSeparatorPrimary) {
				anyVariable = true;
				do {
					right.setCurrentCE(ce & int64_t(0xFFFF'FFFF'0000'0000ULL));
					for (;;) {
						ce = right.nextCE();
						rightPrimary = uint32_t(ce >> 32);
						if (rightPrimary == 0) {
							right.setCurrentCE(0);
						} else {
							break;
						}
					}
				} while (rightPrimary < variableTop && rightPrimary > MergeSeparatorPrimary);
			}
		} while (rightPrimary == 0);

		if (leftPrimary != rightPrimary) {
			// A primary difference, after script reordering.
			if (settings.hasReordering()) {
				leftPrimary = settings.reorder(leftPrimary);
				rightPrimary = settings.reorder(rightPrimary);
			}
			return leftPrimary < rightPrimary ? CompareLess : CompareGreater;
		}
		if (leftPrimary == NoCEPrimary) {
			break;
		}
	}
	if (left.failed() || right.failed()) {
		return CompareEqual;
	}

	// The secondary level may be skipped while the case level, which is switched on
	// separately, still runs.
	if (CollationSettings::getStrength(options) >= StrengthSecondary) {
		if ((options & CollationSettings::BackwardSecondary) == 0) {
			int32_t leftIndex = 0;
			int32_t rightIndex = 0;
			for (;;) {
				uint32_t leftSecondary;
				do { leftSecondary = uint32_t(left.getCE(leftIndex++)) >> 16; } while (
						leftSecondary == 0);

				uint32_t rightSecondary;
				do { rightSecondary = uint32_t(right.getCE(rightIndex++)) >> 16; } while (
						rightSecondary == 0);

				if (leftSecondary != rightSecondary) {
					return leftSecondary < rightSecondary ? CompareLess : CompareGreater;
				}
				if (leftSecondary == NoCEWeight16) {
					break;
				}
			}
		} else {
			// French secondary: compare secondary weights backwards, within segments
			// separated by the merge separator (U+FFFE, primary 02).
			int32_t leftStart = 0;
			int32_t rightStart = 0;
			for (;;) {
				// Find the merge separator, or the NoCE terminator.
				uint32_t p;
				int32_t leftLimit = leftStart;
				while ((p = uint32_t(left.getCE(leftLimit) >> 32)) > MergeSeparatorPrimary
						|| p == 0) {
					++leftLimit;
				}
				int32_t rightLimit = rightStart;
				while ((p = uint32_t(right.getCE(rightLimit) >> 32)) > MergeSeparatorPrimary
						|| p == 0) {
					++rightLimit;
				}

				int32_t leftIndex = leftLimit;
				int32_t rightIndex = rightLimit;
				for (;;) {
					int32_t leftSecondary = 0;
					while (leftSecondary == 0 && leftIndex > leftStart) {
						leftSecondary = int32_t(uint32_t(left.getCE(--leftIndex)) >> 16);
					}

					int32_t rightSecondary = 0;
					while (rightSecondary == 0 && rightIndex > rightStart) {
						rightSecondary = int32_t(uint32_t(right.getCE(--rightIndex)) >> 16);
					}

					if (leftSecondary != rightSecondary) {
						return leftSecondary < rightSecondary ? CompareLess : CompareGreater;
					}
					if (leftSecondary == 0) {
						break;
					}
				}

				// Both strings have the same number of merge separators, or there
				// would have been a primary difference.
				if (p == NoCEPrimary) {
					break;
				}
				// Step over both separators.
				leftStart = leftLimit + 1;
				rightStart = rightLimit + 1;
			}
		}
	}

	if ((options & CollationSettings::CaseLevelBit) != 0) {
		auto strength = CollationSettings::getStrength(options);
		int32_t leftIndex = 0;
		int32_t rightIndex = 0;
		for (;;) {
			uint32_t leftCase, leftLower32, rightCase;
			if (strength == StrengthPrimary) {
				// Primary + case level: ignore the case weights of primary
				// ignorables, or a-umlaut would sort after a, which is not what
				// accent-insensitive sorting is for. The lower 32 bits are checked
				// for zero as well, because a variable CE is stored with its primary
				// only.
				int64_t ce;
				do {
					ce = left.getCE(leftIndex++);
					leftCase = uint32_t(ce);
				} while (uint32_t(ce >> 32) == 0 || leftCase == 0);
				leftLower32 = leftCase;
				leftCase &= 0xC000;

				do {
					ce = right.getCE(rightIndex++);
					rightCase = uint32_t(ce);
				} while (uint32_t(ce >> 32) == 0 || rightCase == 0);
				rightCase &= 0xC000;
			} else {
				// Secondary or tertiary + case level: ignore the case weights of
				// secondary ignorables, by the same argument. A tertiary CE carries
				// uppercase case bits (0.0.ut) to keep tertiary + caseFirst
				// well-formed; ignoring secondary ignorables here turns 0.0.ut into
				// 0.0.0.t rather than needing a case weight above uppercase.
				do { leftCase = uint32_t(left.getCE(leftIndex++)); } while (leftCase <= 0xFFFF);
				leftLower32 = leftCase;
				leftCase &= 0xC000;

				do { rightCase = uint32_t(right.getCE(rightIndex++)); } while (rightCase <= 0xFFFF);
				rightCase &= 0xC000;
			}

			// NoCE and the merge separator need no special handling: there is one
			// case weight per previous-level weight, so length differences were
			// settled there.
			if (leftCase != rightCase) {
				if ((options & CollationSettings::UpperFirst) == 0) {
					return leftCase < rightCase ? CompareLess : CompareGreater;
				}
				return leftCase < rightCase ? CompareGreater : CompareLess;
			}
			if ((leftLower32 >> 16) == NoCEWeight16) {
				break;
			}
		}
	}
	if (CollationSettings::getStrength(options) <= StrengthSecondary) {
		return CompareEqual;
	}

	auto tertiaryMask = CollationSettings::getTertiaryMask(options);

	int32_t leftIndex = 0;
	int32_t rightIndex = 0;
	uint32_t anyQuaternaries = 0;
	for (;;) {
		uint32_t leftLower32, leftTertiary;
		do {
			leftLower32 = uint32_t(left.getCE(leftIndex++));
			anyQuaternaries |= leftLower32;
			leftTertiary = leftLower32 & tertiaryMask;
		} while (leftTertiary == 0);

		uint32_t rightLower32, rightTertiary;
		do {
			rightLower32 = uint32_t(right.getCE(rightIndex++));
			anyQuaternaries |= rightLower32;
			rightTertiary = rightLower32 & tertiaryMask;
		} while (rightTertiary == 0);

		if (leftTertiary != rightTertiary) {
			if (CollationSettings::sortsTertiaryUpperCaseFirst(options)) {
				// Let NoCE through and keep real tertiary weights above it. The
				// artificial uppercase weight of a tertiary CE (0.0.ut) is left
				// alone, so that tertiary CEs stay well-formed: their case+tertiary
				// weights have to exceed those of primary and secondary CEs.
				if (leftTertiary > NoCEWeight16) {
					if (leftLower32 > 0xFFFF) {
						leftTertiary ^= 0xC000;
					} else {
						leftTertiary += 0x4000;
					}
				}
				if (rightTertiary > NoCEWeight16) {
					if (rightLower32 > 0xFFFF) {
						rightTertiary ^= 0xC000;
					} else {
						rightTertiary += 0x4000;
					}
				}
			}
			return leftTertiary < rightTertiary ? CompareLess : CompareGreater;
		}
		if (leftTertiary == NoCEWeight16) {
			break;
		}
	}
	if (CollationSettings::getStrength(options) <= StrengthTertiary) {
		return CompareEqual;
	}

	if (!anyVariable && (anyQuaternaries & 0xC0) == 0) {
		// No variable CEs and no non-zero quaternary weights: nothing to compare.
		return CompareEqual;
	}

	leftIndex = 0;
	rightIndex = 0;
	for (;;) {
		uint32_t leftQuaternary;
		do {
			auto ce = left.getCE(leftIndex++);
			leftQuaternary = uint32_t(ce) & 0xFFFF;
			if (leftQuaternary <= NoCEWeight16) {
				// A variable primary, a wholly ignorable CE, or NoCE.
				leftQuaternary = uint32_t(ce >> 32);
			} else {
				// A regular, not tertiary-ignorable CE: keep the quaternary weight in
				// bits 7..6 and raise everything else, so that it sorts above any
				// shifted primary.
				leftQuaternary |= 0xFFFF'FF3F;
			}
		} while (leftQuaternary == 0);

		uint32_t rightQuaternary;
		do {
			auto ce = right.getCE(rightIndex++);
			rightQuaternary = uint32_t(ce) & 0xFFFF;
			if (rightQuaternary <= NoCEWeight16) {
				rightQuaternary = uint32_t(ce >> 32);
			} else {
				rightQuaternary |= 0xFFFF'FF3F;
			}
		} while (rightQuaternary == 0);

		if (leftQuaternary != rightQuaternary) {
			if (settings.hasReordering()) {
				leftQuaternary = settings.reorder(leftQuaternary);
				rightQuaternary = settings.reorder(rightQuaternary);
			}
			return leftQuaternary < rightQuaternary ? CompareLess : CompareGreater;
		}
		if (leftQuaternary == NoCEPrimary) {
			break;
		}
	}
	return CompareEqual;
}

} // namespace sprt::unicode::detail
