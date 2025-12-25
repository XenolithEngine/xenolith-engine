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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_UTIME_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_UTIME_H_

#include <c/cross/__sprt_config.h>
#include <c/bits/__sprt_time_t.h>

#ifdef __SPRT_BUILD
#define __SPRT_UTIMBUF_NAME __SPRT_ID(utimbuf)
#else
#define __SPRT_UTIMBUF_NAME utimbuf
#endif

__SPRT_BEGIN_DECL

struct __SPRT_ID(utimbuf) {
	__SPRT_ID(time_t) actime;
	__SPRT_ID(time_t) modtime;
};

SPRT_API int __SPRT_ID(utime)(const char *, const struct __SPRT_UTIMBUF_NAME *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_UTIME_H_
