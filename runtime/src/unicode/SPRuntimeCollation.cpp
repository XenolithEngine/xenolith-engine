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

// The collation compile unit: the CLDR root collation table, the per-locale
// tailorings, and the public sprt::unicode ordering functions built on them.
//
// This is the one thing the runtime does NOT claim to do without data: the order
// two strings appear in for a person reading them depends on their language, and
// no algorithm derives it. `compareCodepoints` and `compareFolded` next door
// (runtime/src/unicode/case_compare.cc) are deterministic orderings and say so;
// this is the linguistic one, and it costs a table.
//
// It is deliberately a separate translation unit from the case mapper: the two
// share the Unicode trie reader and nothing else, and the collation tables are
// an order of magnitude larger, so keeping them apart keeps an application that
// never collates from paying compile time for them.
//
// Everything is constexpr, as everywhere else in this runtime: the tables are
// parsed by the generator (data/gen-collation-tables.py), not at run time, so
// there is no lazy initialization, no allocation on the read path and no error
// state for the data itself.

#include <sprt/runtime/unicode.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>

#include "private/SPRTUnicodeTrie.h"

// Include order is a dependency order: the generated arrays first, then the code
// that gives them meaning, then the generated aggregates that name those types.
#include "data/SPRuntimeCollationNormData.cc"
#include "data/SPRuntimeCollationRootData.cc"

#include "collation_norm.cc"
#include "collation_core.cc"
#include "collation_data.cc"
#include "collation_settings.cc"

// Which script groups this build carries. All of them unless the build says
// otherwise: an application that collates should get the right answer for every
// language by default, and an embedded target that cannot afford 1.2 MB of
// tailorings can trade them away deliberately. See runtime/runtime.mk.
//
// This is the first place in the runtime where two builds legitimately answer
// differently, which is why hasCollation() exists: a caller can ask what this
// binary knows rather than assume.
#ifndef SPRT_COLLATION_LATINNORDIC
#define SPRT_COLLATION_LATINNORDIC 1
#endif
#ifndef SPRT_COLLATION_LATINSLAVIC
#define SPRT_COLLATION_LATINSLAVIC 1
#endif
#ifndef SPRT_COLLATION_LATINROMANCE
#define SPRT_COLLATION_LATINROMANCE 1
#endif
#ifndef SPRT_COLLATION_LATINTURKIC
#define SPRT_COLLATION_LATINTURKIC 1
#endif
#ifndef SPRT_COLLATION_LATINOTHER
#define SPRT_COLLATION_LATINOTHER 1
#endif
#ifndef SPRT_COLLATION_CYRILLIC
#define SPRT_COLLATION_CYRILLIC 1
#endif
#ifndef SPRT_COLLATION_GREEK
#define SPRT_COLLATION_GREEK 1
#endif
#ifndef SPRT_COLLATION_SEMITIC
#define SPRT_COLLATION_SEMITIC 1
#endif
#ifndef SPRT_COLLATION_INDIC
#define SPRT_COLLATION_INDIC 1
#endif
#ifndef SPRT_COLLATION_SOUTHEASTASIA
#define SPRT_COLLATION_SOUTHEASTASIA 1
#endif
#ifndef SPRT_COLLATION_OTHER
#define SPRT_COLLATION_OTHER 1
#endif
#ifndef SPRT_COLLATION_CJK
#define SPRT_COLLATION_CJK 1
#endif

#include "data/SPRuntimeCollationTables.cc"

#include "collation_ucharstrie.cc"
#include "collation_iterator.cc"
#include "collation_utf16.cc"
#include "collation_utf8.cc"
#include "collation_fastlatin.cc"
#include "collation_compare.cc"
#include "collation_keys.cc"
#include "collation_locale.cc"
#include "collation_collator.cc"

namespace sprt::unicode {

bool hasCollation(StringView locale) { return detail::findTailoring(locale) != nullptr; }

namespace detail {

// The table and settings for one request. An unknown tag - or one whose group
// this build left out - resolves to the root, which is the right order for most
// languages and a defensible one for the rest.
struct ResolvedCollator {
	const CollationData *data;
	CollationSettings settings;
};

static ResolvedCollator resolve(StringView locale, const CollateOptions &options) {
	auto tailoring = findTailoring(locale);
	auto data = tailoring ? tailoring->data : &s_collRootData;
	auto dataOptions = tailoring ? tailoring->options : s_collRootOptions;
	ResolvedCollator resolved{data,
		makeSettings(data, dataOptions, options.strength, options.numeric, options.shifted,
				options.caseFirst)};
	if (tailoring && tailoring->reorderTable) {
		resolved.settings.reorderTable = tailoring->reorderTable;
		resolved.settings.reorderRanges = tailoring->reorderRanges;
		resolved.settings.reorderRangesLength = tailoring->reorderRangesLength;
		resolved.settings.minHighNoReorder = tailoring->minHighNoReorder;
	}
	enableFastLatin(data, resolved.settings);
	return resolved;
}

} // namespace detail

int collate(WideStringView l, WideStringView r, StringView locale, CollateOptions options) {
	if (l.size() > size_t(Max<int32_t>) || r.size() > size_t(Max<int32_t>)) {
		return compareCodepoints(l, r);
	}
	auto collator = detail::resolve(locale, options);
	bool failed = false;
	auto result = detail::compareUtf16(collator.data, collator.settings, l.data(),
			int32_t(l.size()), r.data(), int32_t(r.size()), failed);
	// The only failure is an allocation. Collation cannot report it through an int,
	// and returning "equal" would break the ordering, so fall back to the total
	// order that needs no memory at all.
	return failed ? compareCodepoints(l, r) : result;
}

int collate(StringView l, StringView r, StringView locale, CollateOptions options) {
	if (l.size() > size_t(Max<int32_t>) || r.size() > size_t(Max<int32_t>)) {
		return compareCodepoints(l, r);
	}
	auto collator = detail::resolve(locale, options);
	bool failed = false;
	auto result = detail::compareUtf8(collator.data, collator.settings,
			reinterpret_cast<const uint8_t *>(l.data()), int32_t(l.size()),
			reinterpret_cast<const uint8_t *>(r.data()), int32_t(r.size()), failed);
	return failed ? compareCodepoints(l, r) : result;
}

namespace detail {

// Builds a key into `sink` from an iterator that is already positioned at the
// start of the text.
static bool buildSortKey(CollationIterator &iter, const CollationData *data,
		const CollationSettings &settings, ByteBuffer &sink) {
	writeSortKeyUpToQuaternary(iter, data->compressibleBytes, settings, sink);
	return !iter.failed() && sink.ok();
}

} // namespace detail

bool sortKey(const callback<void(BytesView)> &cb, WideStringView data, StringView locale,
		CollateOptions options) {
	if (data.size() > size_t(Max<int32_t>)) {
		return false;
	}
	auto collator = detail::resolve(locale, options);
	auto table = collator.data;
	auto &settings = collator.settings;

	detail::ByteBuffer sink;
	detail::FCDUTF16CollationIterator iter(table, settings.isNumeric(), data.data(), data.data(),
			data.data() + data.size());
	if (!detail::buildSortKey(iter, table, settings, sink)) {
		return false;
	}
	if (options.strength == Strength::Identical) {
		detail::NormBuffer normalized;
		detail::CodepointBuffer nfd;
		if (!detail::normalizeNfd(data.data(), int32_t(data.size()), normalized, nfd)
				|| !detail::writeIdenticalLevel(nfd, sink)) {
			return false;
		}
	}
	// The terminator, so that a key which is a prefix of another sorts first.
	sink.appendByte(detail::TerminatorByte);
	if (!sink.ok()) {
		return false;
	}
	cb(BytesView(sink.data(), size_t(sink.length())));
	return true;
}

bool sortKey(const callback<void(BytesView)> &cb, StringView data, StringView locale,
		CollateOptions options) {
	bool ret = false;
	toUtf16([&](WideStringView wide) { ret = sortKey(cb, wide, locale, options); }, data);
	return ret;
}

} // namespace sprt::unicode
