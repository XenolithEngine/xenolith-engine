/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include <sprt/c/__sprt_langinfo.h>

#include <langinfo.h>
#if SPRT_APPLE
#include <xlocale.h>
#endif

// nl_langinfo() forwards the item id verbatim to the platform, so the SPRT item
// numbering must match it. On a hosted build <langinfo.h> above resolves to the
// platform header, making these checks meaningful (they are tautological on a
// freestanding build, where the umbrella defines the public ids from __SPRT_*).
// Bases plus category endpoints are asserted, which also validates the
// "contiguous within a category" assumption the indexed members rely on.
static_assert(CODESET == __SPRT_CODESET);
static_assert(D_T_FMT == __SPRT_D_T_FMT);
static_assert(D_FMT == __SPRT_D_FMT);
static_assert(T_FMT == __SPRT_T_FMT);
static_assert(T_FMT_AMPM == __SPRT_T_FMT_AMPM);
static_assert(AM_STR == __SPRT_AM_STR);
static_assert(PM_STR == __SPRT_PM_STR);
static_assert(DAY_1 == __SPRT_DAY_1);
static_assert(DAY_7 == __SPRT_DAY_7);
static_assert(ABDAY_1 == __SPRT_ABDAY_1);
static_assert(ABDAY_7 == __SPRT_ABDAY_7);
static_assert(MON_1 == __SPRT_MON_1);
static_assert(MON_12 == __SPRT_MON_12);
static_assert(ABMON_1 == __SPRT_ABMON_1);
static_assert(ABMON_12 == __SPRT_ABMON_12);
static_assert(RADIXCHAR == __SPRT_RADIXCHAR);
static_assert(THOUSEP == __SPRT_THOUSEP);
static_assert(YESEXPR == __SPRT_YESEXPR);
static_assert(NOEXPR == __SPRT_NOEXPR);
static_assert(CRNCYSTR == __SPRT_CRNCYSTR);

// Weak REFERENCES to the plain libc nl_langinfo/nl_langinfo_l. On glibc/macOS and
// libc_impl (Windows) these resolve to the real strong symbol; on Android they
// resolve to Bionic on API >= 26 devices and stay null on older ones (Bionic
// gained them at API 26, and the NDK headers leave them undeclared at a lower
// minSdk -- so we declare them here). The null-check below then deterministically
// picks the platform symbol or runtime_core's C/POSIX fallback, with no dlsym.
#if SPRT_ANDROID
// Bionic gained nl_langinfo()/nl_langinfo_l() only at API 26, so the NDK headers
// leave the plain symbols undeclared at a lower minSdk. Declare them as weak
// references: the address is the real Bionic symbol on API >= 26 devices and null
// on older ones. (On glibc/macOS the system <langinfo.h> already declares them
// strong, and on Windows the umbrella declares them with libc_impl providing the
// definition -- there the null-check below is simply always taken.)
extern "C" __attribute__((weak)) char *nl_langinfo(nl_item __item);
extern "C" __attribute__((weak)) char *nl_langinfo_l(nl_item __item, locale_t __loc);
#endif

namespace sprt {

// runtime_core's C/POSIX langinfo fallback (one shared table).
char *__nl_langinfo_default(nl_item item);

// __sprt_nl_langinfo: the SPRT-API symbol apps reach through the umbrella and that
// runtime_core's strftime calls directly. Deterministic with no dlsym: use the
// real platform symbol when present, else runtime_core's C/POSIX fallback.
__SPRT_C_FUNC char *__SPRT_ID(nl_langinfo)(__SPRT_ID(nl_item) item) {
	auto *fn = nl_langinfo;
	if (fn) {
		return fn(item);
	}
	return __nl_langinfo_default(item);
}

__SPRT_C_FUNC char *__SPRT_ID(nl_langinfo_l)(__SPRT_ID(nl_item) item, __SPRT_ID(locale_t) loc) {
#if SPRT_NUTTX
	// NuttX has no per-locale entry point at all: <langinfo.h> defines
	// nl_langinfo_l(i, l) as a MACRO dropping the locale and calling nl_langinfo(i).
	// There is no symbol to take the address of, so the weak-reference shape below
	// does not compile here - call through the macro, which is the platform's own
	// answer for a libc that only carries the "C" locale.
	return nl_langinfo_l(item, loc);
#else
	auto *fn = nl_langinfo_l;
	if (fn) {
		return fn(item, loc);
	}
	// No per-locale data in the fallback: the C/POSIX answer ignores the locale.
	return __nl_langinfo_default(item);
#endif
}

} // namespace sprt
