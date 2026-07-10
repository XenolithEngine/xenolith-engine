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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FNMTYPES_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FNMTYPES_H_

#include <sprt/c/bits/__sprt_def.h>

// Per-platform, ABI-compatible <fnmatch.h> flags (SPRuntimeCRegex.cpp static_asserts
// them against the native header). BSD (macOS, Bionic) swaps FNM_PATHNAME=2 /
// FNM_NOESCAPE=1; the GNU spelling (glibc, musl) has PATHNAME=1 / NOESCAPE=2.
#if SPRT_APPLE || SPRT_ANDROID
#define __SPRT_FNM_NOESCAPE    0x01
#define __SPRT_FNM_PATHNAME    0x02
#else
#define __SPRT_FNM_PATHNAME    0x01
#define __SPRT_FNM_NOESCAPE    0x02
#endif

#define __SPRT_FNM_PERIOD      0x04
#define __SPRT_FNM_LEADING_DIR 0x08
#define __SPRT_FNM_CASEFOLD    0x10

#define __SPRT_FNM_NOMATCH 1
#define __SPRT_FNM_NOSYS   (-1)

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_FNMTYPES_H_
