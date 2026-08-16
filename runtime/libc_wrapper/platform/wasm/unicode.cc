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

#define __SPRT_BUILD 1

#include <sprt/runtime/platform.h>

#if SPRT_WASM

#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/unicode.h>

#include "../src/private/SPRTPrivate.h"

// There is no ICU or libunistring to dlopen in the browser sandbox, and no case or
// collation tables are baked into the runtime, so those operations are delegated to
// the JS host, which has full Unicode support through the standard String and Intl
// APIs (toLowerCase/toUpperCase/localeCompare). See sprt-imports.mjs.
//
// IDNA is NOT delegated: the runtime has its own UTS-46 engine (runtime/src/idn),
// which is why an embedder that provides no IDNA is no longer a runtime without
// IDNA, and why every target now answers identically.

extern "C" {

// There used to be a `unicode_char` import here for the single-code-point
// mappings, and `unicode_transform` used to carry lowercasing and uppercasing as
// well. Both come from the compiled-in Unicode tables (runtime/src/unicode) now,
// so the host is no longer asked about them and no longer has to implement them.
// What is left in `unicode_transform` is titlecasing, which needs word
// boundaries.

// op: 2 = title. (0 and 1 were lower and upper, 3 and 4 IDNA; the host no longer
// implements any of them, and the surviving numbering is unchanged.)
// Reads srcLen UTF-8 bytes at src, writes the UTF-8 result into [dst, dst+cap) and
// returns its byte length. If the result does not fit, returns the required length
// (> cap) and writes nothing, so the caller retries with a larger buffer (ICU-style
// preflight). Returns -1 on error (invalid input / unsupported).
__attribute__((import_module("sprt"),
		import_name("unicode_transform"))) int __sprt_host_unicode_transform(int op, const char *src,
		int srcLen, char *dst, int cap);

// Locale-aware comparison of two UTF-8 strings. caseInsensitive != 0 folds case.
// Returns a negative / zero / positive sign like strcmp.
__attribute__((import_module("sprt"),
		import_name("unicode_compare"))) int __sprt_host_unicode_compare(int caseInsensitive,
		const char *a, int aLen, const char *b, int bLen);
}

namespace sprt::unicode {

// Run a host string transform with a preflight/retry for the output buffer.
static bool hostTransform(int op, StringView src, const callback<void(StringView)> &cb) {
	if (src.empty()) {
		cb(StringView());
		return true;
	}

	// Case folding can grow (ß -> SS); 4x + a small
	// pad covers the common case in one shot, and the preflight handles the rest.
	size_t cap = src.size() * 4 + 32;
	for (int attempt = 0; attempt < 2; ++attempt) {
		auto buf = __sprt_typed_malloca(char, cap);
		int ret = __sprt_host_unicode_transform(op, src.data(), int(src.size()), buf, int(cap));
		if (ret >= 0 && size_t(ret) <= cap) {
			cb(StringView(buf, size_t(ret)));
			__sprt_freea(buf);
			return true;
		}
		__sprt_freea(buf);
		if (ret < 0) {
			return false;
		}
		cap = size_t(ret); // host reported the needed size; retry once
	}
	return false;
}

static bool hostTransformWide(int op, WideStringView src, const callback<void(WideStringView)> &cb) {
	bool ret = false;
	unicode::toUtf8([&](StringView u8) {
		ret = hostTransform(op, u8, [&](StringView out) {
			unicode::toUtf16([&](WideStringView w) { cb(w); }, out);
		});
	}, src);
	return ret;
}

bool totitle(const callback<void(StringView)> &cb, StringView data) {
	return hostTransform(2, data, cb);
}

bool totitle(const callback<void(WideStringView)> &cb, WideStringView data) {
	return hostTransformWide(2, data, cb);
}

static bool hostCompare(int caseInsensitive, StringView l, StringView r, int *result) {
	if (!result) {
		return false;
	}
	*result = __sprt_host_unicode_compare(caseInsensitive, l.data(), int(l.size()), r.data(),
			int(r.size()));
	return true;
}

bool compare(StringView l, StringView r, int *result) { return hostCompare(0, l, r, result); }

bool compare(WideStringView l, WideStringView r, int *result) {
	if (!result) {
		return false;
	}
	bool ret = false;
	unicode::toUtf8([&](StringView lu8) {
		unicode::toUtf8([&](StringView ru8) { ret = hostCompare(0, lu8, ru8, result); }, r);
	}, l);
	return ret;
}

bool caseCompare(StringView l, StringView r, int *result) { return hostCompare(1, l, r, result); }

bool caseCompare(WideStringView l, WideStringView r, int *result) {
	if (!result) {
		return false;
	}
	bool ret = false;
	unicode::toUtf8([&](StringView lu8) {
		unicode::toUtf8([&](StringView ru8) { ret = hostCompare(1, lu8, ru8, result); }, r);
	}, l);
	return ret;
}

} // namespace sprt::unicode

#endif
