// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Adapter SCU for musl's <fnmatch.h>, <glob.h> and the small <regex.h> pieces,
// built for the freestanding targets (Windows / wasm) where no host libc
// provides them.
//
// fnmatch / regerror / tre-mem are pure computation (malloc, string, wide-char).
// glob() additionally drives the filesystem — opendir/readdir/closedir +
// stat/lstat — which the runtime's libc_impl already implements for both targets
// (builtin_dirent / builtin_stat + the per-target windows|wasm backends). Its
// GLOB_TILDE path also calls getpwnam_r/getpwuid_r; Windows and wasm have no
// passwd database, so those are no-op stubs in libc_impl/src/builtin_pwd.cpp
// (report "no such user", i.e. GLOB_NOMATCH for the tilde). glob() calls fnmatch,
// which is defined earlier in this same SCU.
//
// The TRE regex engine is split across three SCUs, not merged into one: tre.h
// (musl-libc/src/regex/tre.h) has NO include guard upstream, so the three sources
// that pull it in — tre-mem.c, regcomp.c, regexec.c — cannot share a translation
// unit (every TRE type/enum would redefine). musl is a pristine upstream
// submodule, so tre.h is left untouched. This SCU therefore carries exactly ONE
// tre.h user (tre-mem.c) plus the sources that need no tre.h (fnmatch.c,
// regerror.c, glob.c); regcomp.c and regexec.c get their own SCUs beside it.

#include "../include/defs.h"

// fnmatch: private token macros (END/STAR/...) are #undef'd afterwards so they
// cannot leak into the sources that follow.
#include "../../musl-libc/src/regex/fnmatch.c"
#undef BRACKET
#undef END
#undef QUESTION
#undef STAR
#undef UNMATCHABLE

// regerror: error-code -> string (no tre.h).
#include "../../musl-libc/src/regex/regerror.c"

// glob / globfree: filesystem pattern expansion; calls fnmatch (above) and the
// runtime's opendir/readdir/stat/lstat.
#include "../../musl-libc/src/regex/glob.c"

// tre-mem: the TRE arena allocator (__tre_mem_* used by regcomp/regexec). Pulled
// in last — it is the single tre.h include in this SCU.
#include "../../musl-libc/src/regex/tre-mem.c"

#if SPRT_WINDOWS
// 32-bit-output mbtowc bridge for the widened TRE SCUs (musl_regcomp/regexec):
// there wchar_t is shadowed to a 32-bit type, but the runtime's real mbtowc still
// writes a 16-bit wchar_t. This TU keeps the real 16-bit wchar_t, so it can call
// the real mbtowc and zero-extend the decoded (BMP) character into TRE's 32-bit
// slot. See musl_regcomp.c for the rationale.
int __tre_mbtowc32(unsigned *__out, const char *__s, size_t __n) {
	wchar_t __wc = 0;
	int __r = mbtowc(&__wc, __s, __n);
	if (__out) {
		*__out = (unsigned) (unsigned short) __wc;
	}
	return __r;
}
#endif
