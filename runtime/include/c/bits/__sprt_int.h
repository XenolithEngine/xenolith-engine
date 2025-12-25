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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT_H_

#define __SPRT_SSHRT_FMTd "hhd"
#define __SPRT_SSHRT_FMTi "hhi"
#define __SPRT_SSHRT_C(c) c
#define __SPRT_SSHRT_C_SUFFIX

#define __SPRT_SHRT_FMTd "hd"
#define __SPRT_SHRT_FMTi "hi"
#define __SPRT_SHRT_C(c) c
#define __SPRT_SHRT_C_SUFFIX

#define __SPRT_INT_FMTd "d"
#define __SPRT_INT_FMTi "i"
#define __SPRT_INT_C(c) c
#define __SPRT_INT_C_SUFFIX

#define __SPRT_LINT_FMTd "ld"
#define __SPRT_LINT_FMTi "li"
#define __SPRT_LINT_C(c) c##L
#define __SPRT_LINT_C_SUFFIX L

#define __SPRT_LLINT_FMTd "lld"
#define __SPRT_LLINT_FMTi "lli"
#define __SPRT_LLINT_C(c) c##LL
#define __SPRT_LLINT_C_SUFFIX LL

#define __SPRT_USSHRT_FMTX "hhX"
#define __SPRT_USSHRT_FMTo "hho"
#define __SPRT_USSHRT_FMTu "hhu"
#define __SPRT_USSHRT_FMTx "hhx"
#define __SPRT_USSHRT_C(c) c
#define __SPRT_USSHRT_C_SUFFIX

#define __SPRT_USHRT_FMTX "hX"
#define __SPRT_USHRT_FMTo "ho"
#define __SPRT_USHRT_FMTu "hu"
#define __SPRT_USHRT_FMTx "hx"
#define __SPRT_USHRT_C(c) c
#define __SPRT_USHRT_C_SUFFIX

#define __SPRT_UINT_FMTX "X"
#define __SPRT_UINT_FMTo "o"
#define __SPRT_UINT_FMTu "u"
#define __SPRT_UINT_FMTx "x"
#define __SPRT_UINT_C(c) c##U
#define __SPRT_UINT_C_SUFFIX U

#define __SPRT_ULINT_FMTX "lX"
#define __SPRT_ULINT_FMTo "lo"
#define __SPRT_ULINT_FMTu "lu"
#define __SPRT_ULINT_FMTx "lx"
#define __SPRT_ULINT_C(c) c##UL
#define __SPRT_ULINT_C_SUFFIX UL

#define __SPRT_ULLINT_FMTX "llX"
#define __SPRT_ULLINT_FMTo "llo"
#define __SPRT_ULLINT_FMTu "llu"
#define __SPRT_ULLINT_FMTx "llx"
#define __SPRT_ULLINT_C(c) c##ULL
#define __SPRT_ULLINT_C_SUFFIX ULL

#ifdef __SCHAR_MAX__
#define __SPRT_SSHRT_MAX __SCHAR_MAX__
#endif

#ifdef __CHAR_BIT__
#define __SPRT_SSHRT_BIT __CHAR_BIT__
#endif

#ifdef __SHRT_MAX__
#define __SPRT_SHRT_MAX __SHRT_MAX__
#endif

#ifdef __SHRT_WIDTH__
#define __SPRT_SHRT_BIT __SHRT_WIDTH__
#endif

#ifdef __INT_MAX__
#define __SPRT_INT_MAX __INT_MAX__
#endif

#ifdef __INT_WIDTH__
#define __SPRT_INT_BIT __INT_WIDTH__
#endif

#ifdef __LONG_MAX__
#define __SPRT_LONG_MAX __LONG_MAX__
#endif

#ifdef __LONG_WIDTH__
#define __SPRT_LONG_BIT __LONG_WIDTH__
#endif

#ifdef __LONG_LONG_MAX__
#define __SPRT_LLONG_MAX __LONG_LONG_MAX__
#endif

#ifdef __LLONG_WIDTH__
#define __SPRT_LLONG_BIT __LLONG_WIDTH__
#endif

// clang-format off
// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64)

#ifndef __SPRT_LLONG_MAX
#define __SPRT_LLONG_MAX 0x7fffffffffffffffLL
#endif

#ifndef __SPRT_ULLONG_MAX
#define __SPRT_ULLONG_MAX 0xFfffffffffffffffULL
#endif

#ifndef __SPRT_LLONG_BIT
#define __SPRT_LLONG_BIT 64
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_MAX 0x7fffffffL
#endif

#ifndef __SPRT_ULONG_MAX
#define __SPRT_ULONG_MAX 0xFfffffffUL
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_BIT 32
#endif

#ifndef __SPRT_INT_MAX
#define __SPRT_INT_MAX 0x7fffffff
#endif

#ifndef __SPRT_UINT_MAX
#define __SPRT_UINT_MAX 0xffffffffU
#endif

#ifndef __SPRT_INT_BIT
#define __SPRT_INT_BIT 32
#endif

#ifndef __SPRT_SHRT_MAX
#define __SPRT_SHRT_MAX 0x7fff
#endif

#ifndef __SPRT_USHRT_MAX
#define __SPRT_USHRT_MAX 0xffff
#endif

#ifndef __SPRT_SHRT_BIT
#define __SPRT_SHRT_BIT 16
#endif

#ifndef __SPRT_SSHRT_MAX
#define __SPRT_SSHRT_MAX 0x7f
#endif

#ifndef __SPRT_USSHRT_MAX
#define __SPRT_USSHRT_MAX 0xff
#endif

#ifndef __SPRT_SSHRT_BIT
#define __SPRT_SSHRT_BIT 8
#endif

#elif defined(_WIN32) || defined(__ILP32__)

#ifndef __SPRT_LLONG_MAX
#define __SPRT_LLONG_MAX 0x7fffffffffffffffLL
#endif

#ifndef __SPRT_ULLONG_MAX
#define __SPRT_ULLONG_MAX 0xFfffffffffffffffULL
#endif

#ifndef __SPRT_LLONG_BIT
#define __SPRT_LLONG_BIT 64
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_MAX 0x7fffffffL
#endif

#ifndef __SPRT_ULONG_MAX
#define __SPRT_ULONG_MAX 0xFfffffffUL
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_BIT 32
#endif

#ifndef __SPRT_INT_MAX
#define __SPRT_INT_MAX 0x7fffffff
#endif

#ifndef __SPRT_UINT_MAX
#define __SPRT_UINT_MAX 0xffffffffU
#endif

#ifndef __SPRT_INT_BIT
#define __SPRT_INT_BIT 32
#endif

#ifndef __SPRT_SHRT_MAX
#define __SPRT_SHRT_MAX 0x7fff
#endif

#ifndef __SPRT_USHRT_MAX
#define __SPRT_USHRT_MAX 0xffff
#endif

#ifndef __SPRT_SHRT_BIT
#define __SPRT_SHRT_BIT 16
#endif

#ifndef __SPRT_SSHRT_MAX
#define __SPRT_SSHRT_MAX 0x7f
#endif

#ifndef __SPRT_USSHRT_MAX
#define __SPRT_USSHRT_MAX 0xff
#endif

#ifndef __SPRT_SSHRT_BIT
#define __SPRT_SSHRT_BIT 8
#endif

#elif defined(__LP64__)

#ifndef __SPRT_LLONG_MAX
#define __SPRT_LLONG_MAX 0x7fffffffffffffffLL
#endif

#ifndef __SPRT_ULLONG_MAX
#define __SPRT_ULLONG_MAX 0xFfffffffffffffffULL
#endif

#ifndef __SPRT_LLONG_BIT
#define __SPRT_LLONG_BIT 64
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_MAX 0x7fffffffffffffffL
#endif

#ifndef __SPRT_ULONG_MAX
#define __SPRT_ULONG_MAX 0xFfffffffffffffffUL
#endif

#ifndef __SPRT_LONG_BIT
#define __SPRT_LONG_BIT 64
#endif

#ifndef __SPRT_INT_MAX
#define __SPRT_INT_MAX 0x7fffffff
#endif

#ifndef __SPRT_UINT_MAX
#define __SPRT_UINT_MAX 0xffffffffU
#endif

#ifndef __SPRT_INT_BIT
#define __SPRT_INT_BIT 32
#endif

#ifndef __SPRT_SHRT_MAX
#define __SPRT_SHRT_MAX 0x7fff
#endif

#ifndef __SPRT_USHRT_MAX
#define __SPRT_USHRT_MAX 0xffff
#endif

#ifndef __SPRT_SHRT_BIT
#define __SPRT_SHRT_BIT 16
#endif

#ifndef __SPRT_SSHRT_MAX
#define __SPRT_SSHRT_MAX 0x7f
#endif

#ifndef __SPRT_USSHRT_MAX
#define __SPRT_USSHRT_MAX 0xff
#endif

#ifndef __SPRT_SSHRT_BIT
#define __SPRT_SSHRT_BIT 8
#endif

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif

// clang-format on

#define __SPRT_LLONG_WIDTH __SPRT_LLONG_BIT
#define __SPRT_LONG_WIDTH __SPRT_LONG_BIT
#define __SPRT_INT_WIDTH __SPRT_INT_BIT
#define __SPRT_SHRT_WIDTH __SPRT_SHRT_BIT
#define __SPRT_SSHRT_WIDTH __SPRT_SSHRT_BIT

#define __SPRT_LINT_MAX __SPRT_LONG_MAX
#define __SPRT_ULINT_MAX __SPRT_ULONG_MAX
#define __SPRT_LINT_WIDTH __SPRT_LONG_BIT

#define __SPRT_LLINT_MAX __SPRT_LLONG_MAX
#define __SPRT_ULLINT_MAX __SPRT_ULLONG_MAX
#define __SPRT_LLINT_WIDTH __SPRT_LLONG_BIT

#define __SPRT_ULLINT_WIDTH __SPRT_LLONG_BIT
#define __SPRT_ULLONG_WIDTH __SPRT_LLONG_BIT
#define __SPRT_ULINT_WIDTH __SPRT_LONG_BIT
#define __SPRT_ULONG_WIDTH __SPRT_LONG_BIT
#define __SPRT_UINT_WIDTH __SPRT_INT_BIT
#define __SPRT_USHRT_WIDTH __SPRT_SHRT_BIT
#define __SPRT_USSHRT_WIDTH __SPRT_SSHRT_BIT

#endif
