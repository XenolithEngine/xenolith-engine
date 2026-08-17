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

// The case-mapping compile unit: the Unicode case properties and the public
// sprt::unicode case functions built on them.
//
// Until this unit existed there were seven implementations of these functions -
// libunistring or ICU via dlopen on linux, NDK ICU plus a JNI fallback on
// android, CoreFoundation on darwin, LCMapStringEx on windows, calls into the JS
// host on wasm, and ASCII-only stubs on nuttx and embox - which disagreed with
// each other and with Unicode, and which made the result depend on what happened
// to be installed on the machine. This one is a pure function of a table compiled
// into the binary, so every target answers identically and no target needs a
// library at all.
//
// Everything is in one translation unit on purpose, for the same reason as the
// IDN unit next door: the generated table (data/) is parsed into a `constexpr`
// trie at compile time, which only works if the array and the reader are in the
// same TU. The engine therefore has no run-time initialization - no lazy statics,
// no allocation, no error path for the data.
//
// Scope: everything sprt::unicode says about case. Lowercasing, uppercasing and
// titlecasing, for single code points and for strings, with or without a locale;
// word boundaries by UAX #29, which titlecasing needs; and the two orderings that
// do not depend on a language - code point order and code point order after full
// case folding.
//
// Not here: collation, the language-dependent ordering shown to a user. It needs
// CLDR tailoring data these tables do not carry, and it is the one thing this
// module deliberately does not answer. See docs/design/unicode-case-port-plan.adoc.

#include <sprt/runtime/unicode.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>

#include "private/SPRTUnicodeTrie.h"

#include "data/SPRuntimeUnicodeCaseData.cc"
#include "data/SPRuntimeUnicodeWordBreakData.cc"

#include "case_props.cc"
#include "case_full.cc"
#include "case_string.cc"
#include "case_utf8.cc"
#include "word_break.cc"
#include "case_title.cc"
#include "case_compare.cc"

namespace sprt::unicode {

char32_t tolower(char32_t c) { return detail::toLowerSimple(c); }

char32_t toupper(char32_t c) { return detail::toUpperSimple(c); }

char32_t totitle(char32_t c) { return detail::toTitleSimple(c); }

namespace detail {

// caseLocaleOf() parses a NUL-terminated id, as ICU's does; a StringView is not
// one. Language subtags are at most 3 characters and everything after the first
// separator is ignored, so a small fixed buffer loses nothing.
static CaseLocale caseLocaleFor(StringView locale) {
	char buf[16];
	size_t n = locale.size() < sizeof(buf) - 1 ? locale.size() : sizeof(buf) - 1;
	for (size_t i = 0; i < n; ++i) { buf[i] = locale[i]; }
	buf[n] = 0;
	return caseLocaleOf(buf);
}

// The runtime idiom around the mappers' preflight contract: measure with a zero
// capacity, allocate, map, hand the result to the callback, free. `fn` is called
// twice and must be deterministic, which it is - it reads only the tables and
// the input.
//
// The callback fires exactly once on success, including for empty input, and not
// at all on failure.
template <typename Fn>
static bool mapUtf16(const callback<void(WideStringView)> &cb, WideStringView data, Fn &&fn) {
	if (data.size() > size_t(Max<int32_t>)) {
		return false;
	}
	auto srcLength = int32_t(data.size());
	auto length = fn(static_cast<char16_t *>(nullptr), 0, data.data(), srcLength);
	if (length < 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char16_t, size_t(length) + 1);
	if (!buf) {
		return false;
	}
	auto written = fn(buf, length, data.data(), srcLength);
	if (written != length) {
		// The two passes disagreed, which can only mean a bug in the mapper; do
		// not hand the caller a half-written buffer.
		__sprt_freea(buf);
		return false;
	}
	buf[length] = 0;

	cb(WideStringView(buf, size_t(length)));
	__sprt_freea(buf);
	return true;
}

// The same, for UTF-8. Text is UTF-8 nearly everywhere it enters this codebase,
// so this path does not go through UTF-16 - see case_utf8.cc.
template <typename Fn>
static bool mapUtf8(const callback<void(StringView)> &cb, StringView data, Fn &&fn) {
	if (data.size() > size_t(Max<int32_t>)) {
		return false;
	}
	auto srcLength = int32_t(data.size());
	auto length = fn(static_cast<char *>(nullptr), 0, data.data(), srcLength);
	if (length < 0) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char, size_t(length) + 1);
	if (!buf) {
		return false;
	}
	auto written = fn(buf, length, data.data(), srcLength);
	if (written != length) {
		__sprt_freea(buf);
		return false;
	}
	buf[length] = 0;

	cb(StringView(buf, size_t(length)));
	__sprt_freea(buf);
	return true;
}

} // namespace detail

bool tolower(const callback<void(WideStringView)> &cb, WideStringView data, StringView locale) {
	auto loc = detail::caseLocaleFor(locale);
	return detail::mapUtf16(cb, data,
			[loc](char16_t *dest, int32_t capacity, const char16_t *src, int32_t length) {
		return detail::mapToLowerUtf16(loc, dest, capacity, src, length);
	});
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data, StringView locale) {
	auto loc = detail::caseLocaleFor(locale);
	return detail::mapUtf16(cb, data,
			[loc](char16_t *dest, int32_t capacity, const char16_t *src, int32_t length) {
		return detail::mapToUpperUtf16(loc, dest, capacity, src, length);
	});
}

bool tolower(const callback<void(StringView)> &cb, StringView data, StringView locale) {
	auto loc = detail::caseLocaleFor(locale);
	return detail::mapUtf8(cb, data,
			[loc](char *dest, int32_t capacity, const char *src, int32_t length) {
		return detail::mapToLowerUtf8(loc, dest, capacity, src, length);
	});
}

bool toupper(const callback<void(StringView)> &cb, StringView data, StringView locale) {
	auto loc = detail::caseLocaleFor(locale);
	return detail::mapUtf8(cb, data,
			[loc](char *dest, int32_t capacity, const char *src, int32_t length) {
		return detail::mapToUpperUtf8(loc, dest, capacity, src, length);
	});
}

bool tolower(const callback<void(WideStringView)> &cb, WideStringView data) {
	return tolower(cb, data, StringView());
}

bool toupper(const callback<void(WideStringView)> &cb, WideStringView data) {
	return toupper(cb, data, StringView());
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data, StringView locale) {
	auto loc = detail::caseLocaleFor(locale);
	return detail::mapUtf16(cb, data,
			[loc](char16_t *dest, int32_t capacity, const char16_t *src, int32_t length) {
		return detail::mapToTitleUtf16(loc, dest, capacity, src, length);
	});
}

// Titlecasing has no direct UTF-8 path: it needs word boundaries, and the word
// breaker works on UTF-16. See case_title.cc.
bool totitle(const callback<void(StringView)> &cb, StringView data, StringView locale) {
	bool ret = false;
	toUtf16([&](WideStringView wide) {
		ret = totitle([&](WideStringView result) { toUtf8(cb, result); }, wide, locale);
	}, data);
	return ret;
}

bool tolower(const callback<void(StringView)> &cb, StringView data) {
	return tolower(cb, data, StringView());
}

bool toupper(const callback<void(StringView)> &cb, StringView data) {
	return toupper(cb, data, StringView());
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	return totitle(cb, data, StringView());
}

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	return totitle(cb, data, StringView());
}

// --- comparison --------------------------------------------------------------
//
// See case_compare.cc for what these orders are and are not. All six are total
// orders over arbitrary input, including input that is not well-formed.

int compareCodepoints(StringView l, StringView r) {
	// For well-formed UTF-8 the order of the bytes is the order of the code
	// points - that is what the encoding was designed for - so there is nothing
	// to decode. Comparison is unsigned, which is the part that matters.
	return sprt::detail::compare_c(l.data(), l.size(), r.data(), r.size());
}

int compareCodepoints(WideStringView l, WideStringView r) {
	return detail::compareStreams(detail::Utf16Stream{l.data(), 0, l.size()},
			detail::Utf16Stream{r.data(), 0, r.size()});
}

int compareCodepoints(StringViewBase<char32_t> l, StringViewBase<char32_t> r) {
	// Already code points, in order.
	return sprt::detail::compare_c(l.data(), l.size(), r.data(), r.size());
}

int compareFolded(StringView l, StringView r) {
	return detail::compareStreams(
			detail::FoldedStream<detail::Utf8Stream>{
				{reinterpret_cast<const uint8_t *>(l.data()), 0, l.size()}},
			detail::FoldedStream<detail::Utf8Stream>{
				{reinterpret_cast<const uint8_t *>(r.data()), 0, r.size()}});
}

int compareFolded(WideStringView l, WideStringView r) {
	return detail::compareStreams(
			detail::FoldedStream<detail::Utf16Stream>{{l.data(), 0, l.size()}},
			detail::FoldedStream<detail::Utf16Stream>{{r.data(), 0, r.size()}});
}

int compareFolded(StringViewBase<char32_t> l, StringViewBase<char32_t> r) {
	return detail::compareStreams(
			detail::FoldedStream<detail::Utf32Stream>{{l.data(), 0, l.size()}},
			detail::FoldedStream<detail::Utf32Stream>{{r.data(), 0, r.size()}});
}

} // namespace sprt::unicode
