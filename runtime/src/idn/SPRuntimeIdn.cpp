/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

// The IDN compile unit: Punycode, the in-tree UTS-46 engine, and the public
// sprt::idn API on top of them.
//
// Everything is in one translation unit on purpose. The generated Unicode tables
// (data/) are parsed into `constexpr` tries at compile time, which only works if
// the arrays and the readers are in the same TU; the reward is that the engine has
// no run-time initialization at all - no lazy statics, no allocation, no error path
// for the data itself.
//
// Layer order below is dependency order, and it is load-bearing:
//   data -> trie -> props -> buffer -> convert -> normalizer -> punycode -> uts46

#include <sprt/runtime/utils/idn.h>
#include <sprt/runtime/unicode.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>

// Generated Unicode data. See data/README.adoc.
#include "data/SPRuntimeIdnDataNorm.cc"
#include "data/SPRuntimeIdnDataProps.cc"
#include "data/SPRuntimeIdnDataBidi.cc"
#include "data/SPRuntimeIdnDataScript.cc"

#include "uts46/trie.cc"
#include "uts46/props.cc"
#include "uts46/buffer.cc"
#include "uts46/convert.cc"
#include "uts46/normalizer.cc"

// Punycode must precede uts46.cc, which reaches into it through the adapters at
// the top of that file.
#include "punycode.cc"
#include "idn2.cc"
#include "tld.cc"

#include "uts46/uts46.cc"

namespace sprt::idn {

// UTS-46 reports a SET of violated rules; Status is one value. The set is
// collapsed here, and only here - the algorithm needs the whole mask internally
// for severeErrors and the U+FFFD interplay in processLabel().
//
// The order is most-specific-first and must agree with the enumerator order in
// status.h. It is the one piece of this port with no counterpart in ICU to check
// against, so it lives in exactly one place rather than being spread across the
// return statements.
static Status statusFromErrors(uint32_t errors) {
	using namespace detail;
	struct Entry {
		uint32_t bit;
		Status status;
	};
	static constexpr Entry s_order[] = {
		{ErrPunycode, Status::ErrorIdnPunycode},
		{ErrInvalidAceLabel, Status::ErrorIdnInvalidAceLabel},
		{ErrLabelHasDot, Status::ErrorIdnLabelHasDot},
		{ErrEmptyLabel, Status::ErrorIdnEmptyLabel},
		{ErrDisallowed, Status::ErrorIdnDisallowed},
		{ErrBidi, Status::ErrorIdnBidi},
		{ErrContextJ, Status::ErrorIdnContextJ},
		{ErrContextOPunctuation, Status::ErrorIdnContextOPunctuation},
		{ErrContextODigits, Status::ErrorIdnContextODigits},
		{ErrLeadingCombiningMark, Status::ErrorIdnLeadingCombiningMark},
		{ErrLeadingHyphen, Status::ErrorIdnLeadingHyphen},
		{ErrTrailingHyphen, Status::ErrorIdnTrailingHyphen},
		{ErrHyphen34, Status::ErrorIdnHyphen34},
		{ErrLabelTooLong, Status::ErrorIdnLabelTooLong},
		{ErrDomainNameTooLong, Status::ErrorIdnDomainNameTooLong},
	};
	for (auto &it : s_order) {
		if (errors & it.bit) {
			return it.status;
		}
	}
	return Status::Ok;
}

// The shape every public entry point has: decode to UTF-16, run one of the four
// UTS-46 operations, encode the result back to UTF-8 and hand it to the callback.
template <typename Operation>
static Status runUts46(const callback<void(StringView)> &cb, StringView source, Options options,
		bool *transitionalDifferent, const Operation &op) {
	if (source.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	detail::Utf16Buffer src;
	if (!detail::toUtf16(src, source)) {
		return Status::ErrorOutOfHostMemory;
	}

	detail::ProcessState info;
	info.options = sprt::toInt(options);

	detail::Utf16Buffer dest;
	if (!op(src.view(), dest, info)) {
		return Status::ErrorOutOfHostMemory;
	}

	if (transitionalDifferent) {
		*transitionalDifferent = info.isTransDiff;
	}

	if (info.errors != 0) {
		return statusFromErrors(info.errors);
	}

	// Emit: measure, allocate on the stack, fill, invoke, free - the runtime idiom.
	auto len = detail::utf8Length(dest.view());
	auto buf = __sprt_typed_malloca(char, len + 1);
	if (!buf) {
		return Status::ErrorOutOfHostMemory;
	}
	auto written = detail::toUtf8(buf, len, dest.view());
	buf[written] = 0;
	cb(StringView(buf, written));
	__sprt_freea(buf);
	return Status::Ok;
}

Status to_ascii(const callback<void(StringView)> &cb, StringView source, Options options,
		bool *transitionalDifferent) {
	return runUts46(cb, source, options, transitionalDifferent,
			[](WideStringView src, detail::Utf16Buffer &dest, detail::ProcessState &info) {
		return detail::nameToAscii(src, dest, info);
	});
}

Status to_unicode(const callback<void(StringView)> &cb, StringView source, Options options,
		bool *transitionalDifferent) {
	return runUts46(cb, source, options, transitionalDifferent,
			[](WideStringView src, detail::Utf16Buffer &dest, detail::ProcessState &info) {
		return detail::nameToUnicode(src, dest, info);
	});
}

Status label_to_ascii(const callback<void(StringView)> &cb, StringView label, Options options,
		bool *transitionalDifferent) {
	return runUts46(cb, label, options, transitionalDifferent,
			[](WideStringView src, detail::Utf16Buffer &dest, detail::ProcessState &info) {
		return detail::labelToAscii(src, dest, info);
	});
}

Status label_to_unicode(const callback<void(StringView)> &cb, StringView label, Options options,
		bool *transitionalDifferent) {
	return runUts46(cb, label, options, transitionalDifferent,
			[](WideStringView src, detail::Utf16Buffer &dest, detail::ProcessState &info) {
		return detail::labelToUnicode(src, dest, info);
	});
}

Status puny_encode(const callback<void(StringView)> &cb, StringView source, bool makeUrlPrefix) {
	StringViewUtf8 utfSource(source);
	auto uCodeSize = utfSource.code_size();

	auto buf = __sprt_typed_malloca(char32_t, uCodeSize + 1);
	if (!buf) {
		return Status::ErrorOutOfHostMemory;
	}
	auto target = buf;

	// code_size() advances by the full UTF-8 lead-byte window, while foreach()
	// resyncs at the first invalid continuation byte and can therefore emit MORE
	// codepoints than code_size() counted on malformed input. Bound the write
	// cursor to the allocated size so adversarial UTF-8 cannot overflow the buffer.
	const auto bufEnd = buf + uCodeSize;
	utfSource.foreach ([&](char32_t ch) {
		if (target < bufEnd) {
			*(target++) = ch;
		}
	});

	auto result = Status::ErrorInvalidArguemnt;
	size_t retSize = 0;
	if (punycode_encode(buf, target - buf, [&](char ch) { ++retSize; })) {
		auto rbuf = __sprt_typed_malloca(char, retSize + 1 + (makeUrlPrefix ? 4 : 0));
		if (!rbuf) {
			__sprt_freea(buf);
			return Status::ErrorOutOfHostMemory;
		}
		auto rtarget = rbuf;

		if (makeUrlPrefix) {
			__sprt_memcpy(rtarget, "xn--", 4);
			rtarget += 4;
		}

		punycode_encode(buf, target - buf, [&](char ch) { *(rtarget++) = ch; });

		*rtarget = 0;

		cb(StringView(rbuf, rtarget - rbuf));

		__sprt_freea(rbuf);

		result = Status::Ok;
	}

	__sprt_freea(buf);
	return result;
}

Status puny_decode(const callback<void(StringView)> &cb, StringView source, bool prefixed) {
	if (prefixed) {
		if (source.starts_with<StringCaseComparator>("xn--")) {
			source += 4;
		} else {
			return Status::ErrorInvalidArguemnt;
		}
	}

	// Every decoded code point consumes at least one input character.
	auto buf = __sprt_typed_malloca(char32_t, source.size() + 1);
	if (!buf) {
		return Status::ErrorOutOfHostMemory;
	}

	size_t decodedLength = source.size() + 1;
	if (!punycode_decode(source.data(), source.size(), buf, &decodedLength)) {
		__sprt_freea(buf);
		return Status::ErrorInvalidArguemnt;
	}

	size_t retSize = 0;
	for (size_t i = 0; i < decodedLength; ++i) { retSize += unicode::utf8EncodeLength(buf[i]); }

	auto rbuf = __sprt_typed_malloca(char, retSize + 1);
	if (!rbuf) {
		__sprt_freea(buf);
		return Status::ErrorOutOfHostMemory;
	}
	auto rtarget = rbuf;
	for (size_t i = 0; i < decodedLength; ++i) {
		rtarget += unicode::utf8EncodeBuf(rtarget, retSize - size_t(rtarget - rbuf), buf[i]);
	}
	*rtarget = 0;

	cb(StringView(rbuf, rtarget - rbuf));

	__sprt_freea(rbuf);
	__sprt_freea(buf);
	return Status::Ok;
}

} // namespace sprt::idn
