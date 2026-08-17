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

// There is no ICU or libunistring to dlopen in the browser sandbox, so collation
// is delegated to the JS host, which has full Unicode support through Intl
// (localeCompare). See sprt-imports.mjs.
//
// Nothing else is delegated any more. Case mapping - lower, upper and title, for
// code points and for strings - comes from the compiled-in Unicode tables
// (runtime/src/unicode), and IDNA from the runtime's own UTS-46 engine
// (runtime/src/idn). Two imports have been retired along the way,
// `unicode_char` and `unicode_transform`, and with them the requirement that an
// embedding host know anything about Unicode beyond sorting.

extern "C" {

// Locale-aware comparison of two UTF-8 strings. caseInsensitive != 0 folds case.
// Returns a negative / zero / positive sign like strcmp.
__attribute__((import_module("sprt"),
		import_name("unicode_compare"))) int __sprt_host_unicode_compare(int caseInsensitive,
		const char *a, int aLen, const char *b, int bLen);
}

namespace sprt::unicode {

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
