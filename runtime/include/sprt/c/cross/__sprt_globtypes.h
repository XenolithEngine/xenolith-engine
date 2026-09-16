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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_GLOBTYPES_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_GLOBTYPES_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>

// Per-platform, ABI-compatible glob_t (SPRuntimeCRegex.cpp static_asserts sizeof
// and the gl_pathc/gl_pathv/gl_offs offsets against the native header, so the
// wrapper reinterpret_casts to the native glob_t directly — no field copy). The
// unnamed trailing members are the native private/alt-dir-func area.
#if SPRT_ANDROID

// Android borrows musl's glob() (Bionic ships glob() only from API 28); its
// working set is just these three fields — see SPRuntimeCGlobMusl.c.
typedef struct __sprt_glob_t {
	__SPRT_ID(size_t) gl_pathc;
	char **gl_pathv;
	__SPRT_ID(size_t) gl_offs;
} __SPRT_ID(glob_t);

#elif SPRT_APPLE

// BSD layout: gl_matchc between gl_pathc and gl_offs, gl_pathv after gl_flags.
typedef struct __sprt_glob_t {
	__SPRT_ID(size_t) gl_pathc;
	int gl_matchc;
	__SPRT_ID(size_t) gl_offs;
	int gl_flags;
	char **gl_pathv;
	void *__altfuncs[6];
} __SPRT_ID(glob_t);

#else

// glibc / musl layout (gl_pathv at offset sizeof(size_t), gl_offs next).
typedef struct __sprt_glob_t {
	__SPRT_ID(size_t) gl_pathc;
	char **gl_pathv;
	__SPRT_ID(size_t) gl_offs;
	int __gl_flags;
	void *__gl_priv[5];
} __SPRT_ID(glob_t);

#endif

// glob flags + return codes. macOS/iOS use the BSD numbering (and negative return
// codes); everywhere else — including Android, whose glob() is musl's — uses the
// glibc/musl values. Flags a platform does not define are 0 (no-op).
#if SPRT_APPLE

#define __SPRT_GLOB_APPEND      0x0001
#define __SPRT_GLOB_DOOFFS      0x0002
#define __SPRT_GLOB_ERR         0x0004
#define __SPRT_GLOB_MARK        0x0008
#define __SPRT_GLOB_NOCHECK     0x0010
#define __SPRT_GLOB_NOSORT      0x0020
#define __SPRT_GLOB_NOESCAPE    0x2000
#define __SPRT_GLOB_TILDE       0x0800
#define __SPRT_GLOB_PERIOD      0
#define __SPRT_GLOB_TILDE_CHECK 0

#define __SPRT_GLOB_NOSPACE (-1)
#define __SPRT_GLOB_ABORTED (-2)
#define __SPRT_GLOB_NOMATCH (-3)

#else

#define __SPRT_GLOB_ERR         0x0001
#define __SPRT_GLOB_MARK        0x0002
#define __SPRT_GLOB_NOSORT      0x0004
#define __SPRT_GLOB_DOOFFS      0x0008
#define __SPRT_GLOB_NOCHECK     0x0010
#define __SPRT_GLOB_APPEND      0x0020
#define __SPRT_GLOB_NOESCAPE    0x0040
#define __SPRT_GLOB_PERIOD      0x0080
#define __SPRT_GLOB_TILDE       0x1000
#define __SPRT_GLOB_TILDE_CHECK 0x4000

#define __SPRT_GLOB_NOSPACE 1
#define __SPRT_GLOB_ABORTED 2
#define __SPRT_GLOB_NOMATCH 3

#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_GLOBTYPES_H_
