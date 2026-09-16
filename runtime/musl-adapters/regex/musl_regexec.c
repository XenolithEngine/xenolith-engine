// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// Adapter SCU for musl's regexec() (the TRE regex matcher). Its own translation
// unit because it includes the unguarded musl-libc/src/regex/tre.h, which cannot
// be shared with the other tre.h users (regcomp.c, tre-mem.c) in a single SCU —
// see musl_regex.c for the full rationale. Pure computation; no OS calls
// (malloc / string / wide-char only). Built for Windows / wasm.

#include "../include/defs.h"

#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"

// See musl_regcomp.c for the full rationale. On Windows wchar_t/wint_t are 16-bit
// (MSVC ABI) while TRE needs a 32-bit, equal-width character model; widen both
// for the TRE sources and route mbtowc through the 32-bit bridge. Must match
// musl_regcomp.c exactly so the compiled tnfa_transition (code_min/code_max)
// layout is identical across the compiler and the matcher.
#if SPRT_WINDOWS
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>
int __tre_mbtowc32(unsigned *, const char *, size_t);
#define wchar_t unsigned
#define wint_t unsigned
#define mbtowc __tre_mbtowc32
#endif

#include "../../musl-libc/src/regex/regexec.c"
