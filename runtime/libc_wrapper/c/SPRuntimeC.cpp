/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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
#define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1 // for libc++ compatibility

#include <sprt/c/__sprt_assert.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_utime.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_nl_types.h>

#include <sprt/runtime/math.h>
#include <sprt/runtime/log.h>

// The <nl_types.h> dispatch header gives the plain catopen/catgets/catclose and
// nl_catd on every target (the platform's on hosted, the umbrella's -- backed by
// libc_impl -- on freestanding), which the message-catalog bridge below needs.
// Embox libc has no message catalogs (no <nl_types.h>); use the empty fallback.
#if !SPRT_EMBOX
#include <nl_types.h>
#endif

#if __STDC_HOSTED__ == 1

#include <stddef.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <utime.h>

#endif

#include "common/errno.cc"
#include "common/fenv.cc"
#include "common/signal.cc"
#include "common/abort.cc"
#include "common/locale.cc"
#include "common/langinfo.cc"
#include "common/rand.cc"

static_assert(sizeof(void *) == sizeof(__sprt_intptr_t));
static_assert(sizeof(void *) == sizeof(__sprt_uintptr_t));

#if SPRT_ANDROID
// Bionic gained catopen()/catgets()/catclose() only at API 26, so the NDK headers
// leave the plain symbols undeclared at a lower minSdk. Declare them as weak
// references: the address is the real Bionic symbol on API >= 26 devices and null
// on older ones. (On glibc/macOS the system <nl_types.h> declares them strong, and
// on Windows the umbrella declares them with libc_impl providing the definition --
// there the null-check below is simply always taken.)
extern "C" __attribute__((weak)) nl_catd catopen(const char *, int);
extern "C" __attribute__((weak)) char *catgets(nl_catd, int, int, const char *);
extern "C" __attribute__((weak)) int catclose(nl_catd);
#endif

namespace sprt {

// runtime_core's honest empty-catalog fallback.
__SPRT_ID(nl_catd) __catopen_empty(const char *, int);
char *__catgets_empty(__SPRT_ID(nl_catd), int, int, const char *);
int __catclose_empty(__SPRT_ID(nl_catd));

// The SPRT-API message-catalog symbols apps reach through the <nl_types.h>
// umbrella. Deterministic: use the real platform catalog functions when present,
// else the empty-catalog fallback.
__SPRT_C_FUNC __SPRT_ID(nl_catd) __SPRT_ID(catopen)(const char *path, int v) __SPRT_NOEXCEPT {
#if SPRT_EMBOX
	return __catopen_empty(path, v);
#else
	auto *fn = catopen;
	if (fn) {
		return (__SPRT_ID(nl_catd))fn(path, v);
	}
	return __catopen_empty(path, v);
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(
		catgets)(__SPRT_ID(nl_catd) cat, int a, int b, const char *str) __SPRT_NOEXCEPT {
#if SPRT_EMBOX
	return __catgets_empty(cat, a, b, str);
#else
	auto *fn = catgets;
	if (fn) {
		return fn((nl_catd)cat, a, b, str);
	}
	return __catgets_empty(cat, a, b, str);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(catclose)(__SPRT_ID(nl_catd) cat) __SPRT_NOEXCEPT {
#if SPRT_EMBOX
	return __catclose_empty(cat);
#else
	auto *fn = catclose;
	if (fn) {
		return fn((nl_catd)cat);
	}
	return __catclose_empty(cat);
#endif
}

} // namespace sprt
