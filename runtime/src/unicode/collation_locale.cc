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

// A language tag to a tailoring, with the CLDR fallback.
//
// The fallback is the whole of the resolution: a tag is tried as written, then
// with its last subtag dropped, and so on, until something matches or nothing is
// left - at which point the root order applies. `sr_Latn_RS` finds `sr_Latn`,
// `de_DE` finds nothing and gets the root (which is German's order anyway), and
// `zh_Hans_CN` finds `zh`.
//
// The table holds ICU's spelling: subtags joined by '_'. Callers write tags both
// ways, so '-' is accepted as the separator, and the comparison ignores ASCII
// case - `sr-latn`, `sr_Latn` and `SR_LATN` are the same request.

namespace sprt::unicode::detail {

static constexpr char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; }

// Case-insensitive over ASCII, which is all a language tag may contain.
static int32_t compareTags(const char *a, int32_t aLength, const char *b, int32_t bLength) {
	auto n = aLength < bLength ? aLength : bLength;
	for (int32_t i = 0; i < n; ++i) {
		auto ca = asciiLower(a[i]);
		auto cb = asciiLower(b[i]);
		if (ca != cb) {
			return ca < cb ? -1 : 1;
		}
	}
	return aLength == bLength ? 0 : (aLength < bLength ? -1 : 1);
}

static const CollationTailoring *lookupExact(const char *tag, int32_t length) {
	constexpr int32_t count = int32_t(sizeof(s_collationLocales) / sizeof(s_collationLocales[0]));
	int32_t low = 0;
	int32_t high = count - 1;
	while (low <= high) {
		auto mid = (low + high) / 2;
		auto &entry = s_collationLocales[mid];
		auto order = compareTags(tag, length, entry.tag, entry.tagLength);
		if (order < 0) {
			high = mid - 1;
		} else if (order > 0) {
			low = mid + 1;
		} else {
			return entry.tailoring;
		}
	}
	return nullptr;
}

// The tailoring for a tag, or null for the root order.
static const CollationTailoring *findTailoring(StringView locale) {
	// A tag longer than this is not one: the longest in the table is en_US_POSIX.
	char buf[48];
	auto length = int32_t(locale.size() < sizeof(buf) ? locale.size() : sizeof(buf));
	for (int32_t i = 0; i < length; ++i) {
		auto c = locale[size_t(i)];
		buf[i] = c == '-' ? '_' : c;
	}

	while (length > 0) {
		if (auto tailoring = lookupExact(buf, length)) {
			return tailoring;
		}
		// Drop the last subtag and try again.
		while (length > 0 && buf[length - 1] != '_') { --length; }
		if (length > 0) {
			--length; // and the separator
		}
	}
	return nullptr;
}

} // namespace sprt::unicode::detail
