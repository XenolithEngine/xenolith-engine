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

// C unit providing glob()/globfree() from musl when the platform libc does not
// (Android API < 28; Embox has no <glob.h>). musl's glob.c is C — it uses
// `restrict`, names a variable `new`, and relies on implicit void* conversions —
// so it must be borrowed into a C translation unit rather than the C++ wrapper.
// Empty on every other target.

#define __SPRT_BUILD 1

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_ANDROID || SPRT_EMBOX

// The SPRT cross glob_t for Android is the minimal musl working layout, and its
// __SPRT_GLOB_* are the musl values; use them directly so the produced glob_t IS
// what SPRuntimeCGlob.cpp forwards. glob.c touches only gl_pathc/gl_pathv/gl_offs.
#include <sprt/c/__sprt_glob.h>

#define glob_t __SPRT_ID(glob_t)

#define GLOB_ERR         __SPRT_GLOB_ERR
#define GLOB_MARK        __SPRT_GLOB_MARK
#define GLOB_NOSORT      __SPRT_GLOB_NOSORT
#define GLOB_DOOFFS      __SPRT_GLOB_DOOFFS
#define GLOB_NOCHECK     __SPRT_GLOB_NOCHECK
#define GLOB_APPEND      __SPRT_GLOB_APPEND
#define GLOB_NOESCAPE    __SPRT_GLOB_NOESCAPE
#define GLOB_PERIOD      __SPRT_GLOB_PERIOD
#define GLOB_TILDE       __SPRT_GLOB_TILDE
#define GLOB_TILDE_CHECK __SPRT_GLOB_TILDE_CHECK
#define GLOB_NOSPACE     __SPRT_GLOB_NOSPACE
#define GLOB_ABORTED     __SPRT_GLOB_ABORTED
#define GLOB_NOMATCH     __SPRT_GLOB_NOMATCH

#define glob     __sprt_musl_glob
#define globfree __sprt_musl_globfree

// Neutralize Bionic's <glob.h> (glob.c does #include <glob.h>) so the SPRT glob_t
// / GLOB_* above are used instead of Bionic's incompatible definitions. glob.c's
// other includes (fnmatch/dirent/sys/stat/pwd) resolve to Bionic, which has them.
#ifndef _GLOB_H_
#define _GLOB_H_
#endif

// __strchrnul: a GNU extension Bionic does not export; provide it for the borrow.
static char *__strchrnul(const char *__s, int __c) {
	for (; *__s && *__s != (char)__c; ++__s) { }
	return (char *)__s;
}

#ifndef FNM_NOESCAPE
#define FNM_NOESCAPE 0x2
#endif
#ifndef FNM_PERIOD
#define FNM_PERIOD 0x4
#endif

#pragma clang diagnostic ignored "-Wlogical-op-parentheses"

#include "../../musl-libc/src/regex/glob.c"

#endif // SPRT_ANDROID || SPRT_EMBOX
