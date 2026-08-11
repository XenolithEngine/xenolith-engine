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

#include <sprt/c/__sprt_fnmatch.h>
#include <sprt/c/__sprt_regex.h>
#include <sprt/c/__sprt_glob.h>

#include <fnmatch.h>
#include <regex.h>
#include <stdlib.h>

#if __STDC_HOSTED__ == 0

// Freestanding (wasm / Windows): the plain regcomp/fnmatch/glob are the musl
// adapter's; its regex_t/regmatch_t/glob_t and REG_*/FNM_*/GLOB_* ARE the SPRT
// cross ones (the include_libc headers below resolve to them), so the casts and
// static_asserts are all identities.
#include <glob.h>
#define __SPRT_NATIVE_GLOB     ::glob
#define __SPRT_NATIVE_GLOBFREE ::globfree

#elif SPRT_ANDROID

// Bionic ships glob() only from API 28 (we target 24); SPRuntimeCGlobMusl.c
// provides it over the SPRT glob_t directly (the cross Android glob_t is the musl
// working layout), so no <glob.h> is pulled here.
extern "C" int __sprt_musl_glob(const char *__pattern, int __flags,
		int (*__errfunc)(const char *__epath, int __eerrno), __SPRT_ID(glob_t) * __pglob);
extern "C" void __sprt_musl_globfree(__SPRT_ID(glob_t) * __pglob);
#define __SPRT_NATIVE_GLOB     __sprt_musl_glob
#define __SPRT_NATIVE_GLOBFREE __sprt_musl_globfree

#else

#include <glob.h>
#define __SPRT_NATIVE_GLOB     ::glob
#define __SPRT_NATIVE_GLOBFREE ::globfree

#endif

// ---------------------------------------------------------------------------
// ABI validation. The SPRT cross types/constants (<sprt/c/cross/__sprt_*types.h>)
// are defined to be byte-for-byte / value-for-value compatible with the native
// <regex.h>/<fnmatch.h>/<glob.h> reached above via #include_next, so the wrapper
// below forwards by a plain reinterpret_cast with no field/flag translation. The
// one exception is regexec's pmatch on a libc whose regoff_t is narrower than the
// SPRT (pointer-sized) one — glibc, which ships no 64-bit regexec — where the
// array is translated in place, the open64 pattern.
// ---------------------------------------------------------------------------

// regex_t is an opaque cell; the native one must fit.
// NuttX <regex.h> uses different REG_*/FNM_*/GLOB_* numeric values than the
// glibc layout sprt pins against, so skip the canonical-equality pin block
// there. The wrapper re-exports the symbols under __sprt_-prefixed names.
#if !SPRT_NUTTX
static_assert(sizeof(::regex_t) <= sizeof(__SPRT_ID(regex_t)),
		"native regex_t does not fit in the SPRT regex_t cell");
static_assert(sizeof(::regoff_t) <= sizeof(__SPRT_ID(regoff_t)), "native regoff_t is wider than SPRT's");

static_assert(__SPRT_REG_EXTENDED == REG_EXTENDED && __SPRT_REG_ICASE == REG_ICASE
				&& __SPRT_REG_NEWLINE == REG_NEWLINE && __SPRT_REG_NOSUB == REG_NOSUB,
		"REG_* compile flags differ from native");
static_assert(__SPRT_REG_NOTBOL == REG_NOTBOL && __SPRT_REG_NOTEOL == REG_NOTEOL,
		"REG_* exec flags differ from native");
static_assert(__SPRT_REG_NOMATCH == REG_NOMATCH && __SPRT_REG_BADPAT == REG_BADPAT
				&& __SPRT_REG_ERANGE == REG_ERANGE,
		"REG_* error codes differ from native");

static_assert(__SPRT_FNM_PATHNAME == FNM_PATHNAME && __SPRT_FNM_NOESCAPE == FNM_NOESCAPE
				&& __SPRT_FNM_PERIOD == FNM_PERIOD,
		"FNM_* flags differ from native");
#ifdef FNM_LEADING_DIR
static_assert(__SPRT_FNM_LEADING_DIR == FNM_LEADING_DIR, "FNM_LEADING_DIR differs from native");
#endif
#ifdef FNM_CASEFOLD
static_assert(__SPRT_FNM_CASEFOLD == FNM_CASEFOLD, "FNM_CASEFOLD differs from native");
#endif
#endif // !SPRT_NUTTX

// glob_t + GLOB_* are validated against the native <glob.h> everywhere it is
// reachable (Android borrows musl's glob, so its layout is checked in
// SPRuntimeCGlobMusl.c instead).
#if !SPRT_ANDROID && !SPRT_NUTTX
static_assert(sizeof(__SPRT_ID(glob_t)) == sizeof(::glob_t), "glob_t size differs from native");
static_assert(__builtin_offsetof(__SPRT_ID(glob_t), gl_pathc) == __builtin_offsetof(::glob_t, gl_pathc),
		"gl_pathc offset differs from native");
static_assert(__builtin_offsetof(__SPRT_ID(glob_t), gl_pathv) == __builtin_offsetof(::glob_t, gl_pathv),
		"gl_pathv offset differs from native");
static_assert(__builtin_offsetof(__SPRT_ID(glob_t), gl_offs) == __builtin_offsetof(::glob_t, gl_offs),
		"gl_offs offset differs from native");
static_assert(__SPRT_GLOB_ERR == GLOB_ERR && __SPRT_GLOB_MARK == GLOB_MARK
				&& __SPRT_GLOB_NOSORT == GLOB_NOSORT && __SPRT_GLOB_DOOFFS == GLOB_DOOFFS
				&& __SPRT_GLOB_NOCHECK == GLOB_NOCHECK && __SPRT_GLOB_APPEND == GLOB_APPEND
				&& __SPRT_GLOB_NOESCAPE == GLOB_NOESCAPE && __SPRT_GLOB_TILDE == GLOB_TILDE,
		"GLOB_* flags differ from native");
static_assert(__SPRT_GLOB_NOSPACE == GLOB_NOSPACE && __SPRT_GLOB_ABORTED == GLOB_ABORTED
				&& __SPRT_GLOB_NOMATCH == GLOB_NOMATCH,
		"GLOB_* return codes differ from native");
#ifdef GLOB_PERIOD
static_assert(__SPRT_GLOB_PERIOD == GLOB_PERIOD, "GLOB_PERIOD differs from native");
#endif
#ifdef GLOB_TILDE_CHECK
static_assert(__SPRT_GLOB_TILDE_CHECK == GLOB_TILDE_CHECK, "GLOB_TILDE_CHECK differs from native");
#endif
#endif // !SPRT_ANDROID

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(regcomp)(__SPRT_ID(regex_t) * __preg, const char *__pattern,
		int __cflags) {
	return ::regcomp((::regex_t *) __preg, __pattern, __cflags);
}

__SPRT_C_FUNC int __SPRT_ID(regexec)(const __SPRT_ID(regex_t) * __preg, const char *__string,
		__SPRT_ID(size_t) __nmatch, __SPRT_ID(regmatch_t) * __pmatch, int __eflags) {
	if (__nmatch == 0 || __pmatch == nullptr) {
		return ::regexec((const ::regex_t *) __preg, __string, 0, nullptr, __eflags);
	}
	if constexpr (sizeof(::regmatch_t) == sizeof(__SPRT_ID(regmatch_t))) {
		// Native regoff_t is pointer-sized: regmatch_t layout matches, cast directly.
		return ::regexec((const ::regex_t *) __preg, __string, __nmatch,
				(::regmatch_t *) __pmatch, __eflags);
	} else {
		// Native regoff_t is narrower (glibc 32-bit, no 64-bit regexec): translate.
		::regmatch_t __stack[16];
		::regmatch_t *__nat = __nmatch <= 16
				? __stack
				: (::regmatch_t *) ::malloc(__nmatch * sizeof(::regmatch_t));
		if (__nat == nullptr) {
			return __SPRT_REG_ESPACE;
		}
		int __r = ::regexec((const ::regex_t *) __preg, __string, __nmatch, __nat, __eflags);
		for (__SPRT_ID(size_t) __i = 0; __i < __nmatch; ++__i) {
			__pmatch[__i].rm_so = __nat[__i].rm_so;
			__pmatch[__i].rm_eo = __nat[__i].rm_eo;
		}
		if (__nat != __stack) {
			::free(__nat);
		}
		return __r;
	}
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(regerror)(int __errcode,
		const __SPRT_ID(regex_t) * __preg, char *__errbuf, __SPRT_ID(size_t) __errbuf_size) {
	return ::regerror(__errcode, (const ::regex_t *) __preg, __errbuf, __errbuf_size);
}

__SPRT_C_FUNC void __SPRT_ID(regfree)(__SPRT_ID(regex_t) * __preg) {
	::regfree((::regex_t *) __preg);
}

__SPRT_C_FUNC int __SPRT_ID(fnmatch)(const char *__pattern, const char *__string, int __flags) {
	return ::fnmatch(__pattern, __string, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(glob)(const char *__pattern, int __flags,
		int (*__errfunc)(const char *__epath, int __eerrno), __SPRT_ID(glob_t) * __pglob) {
#if SPRT_ANDROID
	return __SPRT_NATIVE_GLOB(__pattern, __flags, __errfunc, __pglob);
#else
	return __SPRT_NATIVE_GLOB(__pattern, __flags, __errfunc, (::glob_t *) __pglob);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(globfree)(__SPRT_ID(glob_t) * __pglob) {
#if SPRT_ANDROID
	__SPRT_NATIVE_GLOBFREE(__pglob);
#else
	__SPRT_NATIVE_GLOBFREE((::glob_t *) __pglob);
#endif
}

} // namespace sprt
