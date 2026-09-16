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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_REGEX_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_REGEX_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>

// regex_t / regmatch_t / regoff_t + the REG_* constants are defined per-platform
// to be ABI-compatible with the native <regex.h>; SPRuntimeCRegex.cpp static_asserts
// them against the system header.
#include <sprt/c/cross/__sprt_regextypes.h>

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(regcomp)(__SPRT_ID(regex_t) * __preg, const char *__pattern, int __cflags);
SPRT_API int __SPRT_ID(regexec)(const __SPRT_ID(regex_t) * __preg, const char *__string,
		__SPRT_ID(size_t) __nmatch, __SPRT_ID(regmatch_t) * __pmatch, int __eflags);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(regerror)(int __errcode, const __SPRT_ID(regex_t) * __preg,
		char *__errbuf, __SPRT_ID(size_t) __errbuf_size);
SPRT_API void __SPRT_ID(regfree)(__SPRT_ID(regex_t) * __preg);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_REGEX_H_
