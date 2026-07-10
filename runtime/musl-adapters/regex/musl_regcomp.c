// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Adapter SCU for musl's regcomp() (the TRE regex compiler/parser). Its own
// translation unit because it includes the unguarded musl-libc/src/regex/tre.h,
// which cannot be shared with the other tre.h users (regexec.c, tre-mem.c) in a
// single SCU — see musl_regex.c for the full rationale. Pure computation; no OS
// calls (malloc / string / wide-char only). Built for Windows / wasm.

#include "../include/defs.h"

// TRE's character types are `tre_char_t = wchar_t` (string element, written by
// mbtowc) and `tre_cint_t = wint_t` (integer char, holds ranges/sentinels), with
// TRE_CHAR_MAX == 0x10ffff. It assumes BOTH are the same >=21-bit width (as on
// every 32-bit-wchar_t platform, incl. wasm). On Windows the runtime tracks the
// MSVC 16-bit wchar_t ABI, so wchar_t/wint_t are 16-bit: TRE_CHAR_MAX and TRE's
// sentinels truncate and the two types disagree in width, breaking '^'/'$'
// anchors, a trailing '.', etc.
//
// musl is a pristine submodule, so fix it from the adapter: after <wchar.h>/
// <wctype.h> have declared the real 16-bit wide-char API (and locked the
// <bits/alltypes.h> guard so the typedefs are already in place), shadow wchar_t
// AND wint_t with a 32-bit type for the TRE sources — restoring the 32-bit,
// equal-width model TRE expects. mbtowc, the one wide API that writes memory
// through a tre_char_t*, is routed to __tre_mbtowc32 (defined in musl_regex.c),
// which decodes via the real 16-bit mbtowc and zero-extends into the 32-bit
// slot. The by-value wctype/towupper/towlower keep their real 16-bit
// declarations (narrowed at the call for BMP, which is all that occurs); wcslen
// is unused by the byte-mode compiler/matcher. regcomp and regexec apply this
// identically so the compiled tnfa_transition layout matches.
#if SPRT_WINDOWS
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>
int __tre_mbtowc32(unsigned *, const char *, size_t);
#define wchar_t unsigned
#define wint_t unsigned
#define mbtowc __tre_mbtowc32
#endif

#include "../../musl-libc/src/regex/regcomp.c"
