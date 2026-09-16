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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_REGEXTYPES_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_REGEXTYPES_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>

// Per-platform, ABI-compatible <regex.h> surface (SPRuntimeCRegex.cpp static_asserts
// every value/layout below against the native header via #include_next).
//
// regoff_t is uniformly pointer-sized (64-bit on LP64/ILP32P64): it matches the
// native regoff_t directly on musl / macOS (__darwin_off_t) / Bionic (ssize_t),
// while on glibc — whose default regoff_t is 32-bit `int` and which ships no
// 64-bit regexec — the wrapper translates the pmatch array (the open64 pattern).
typedef __SPRT_ID(ssize_t) __SPRT_ID(regoff_t);

typedef struct __sprt_regmatch_t {
	__SPRT_ID(regoff_t) rm_so;
	__SPRT_ID(regoff_t) rm_eo;
} __SPRT_ID(regmatch_t);

// regex_t is opaque: an oversized storage cell the native regex_t is stored in
// (the wrapper static_asserts sizeof(native) <= this). Its fields are not exposed.
typedef struct __sprt_regex_t {
	void *__opaque[8];
} __SPRT_ID(regex_t);

// regcomp cflags / regexec eflags: BSD (macOS, Bionic, Embox) number REG_NOSUB=4 /
// REG_NEWLINE=8, the GNU order (glibc, musl) is the reverse.
#if SPRT_APPLE || SPRT_ANDROID || SPRT_EMBOX
#define __SPRT_REG_EXTENDED 0001
#define __SPRT_REG_ICASE    0002
#define __SPRT_REG_NOSUB    0004
#define __SPRT_REG_NEWLINE  0010
#else
#define __SPRT_REG_EXTENDED 1
#define __SPRT_REG_ICASE    2
#define __SPRT_REG_NEWLINE  4
#define __SPRT_REG_NOSUB    8
#endif

#define __SPRT_REG_NOTBOL 1
#define __SPRT_REG_NOTEOL 2

// Error codes are POSIX-ordered and identical across glibc / musl / BSD.
#define __SPRT_REG_NOMATCH  1
#define __SPRT_REG_BADPAT   2
#define __SPRT_REG_ECOLLATE 3
#define __SPRT_REG_ECTYPE   4
#define __SPRT_REG_EESCAPE  5
#define __SPRT_REG_ESUBREG  6
#define __SPRT_REG_EBRACK   7
#define __SPRT_REG_EPAREN   8
#define __SPRT_REG_EBRACE   9
#define __SPRT_REG_BADBR    10
#define __SPRT_REG_ERANGE   11
#define __SPRT_REG_ESPACE   12
#define __SPRT_REG_BADRPT   13

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_REGEXTYPES_H_
