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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_MBSTATE_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_MBSTATE_H_

#include <c/bits/__sprt_def.h>

#if SPRT_LINUX

#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME __mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 0

#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffff'ffffU
#endif

typedef unsigned long __SPRT_ID(wctype_t);

typedef struct {
	unsigned __opaque1;
	unsigned __opaque2;
} __SPRT_MBSTATE_NAME;

#elif SPRT_WINDOWS

#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 1

#ifndef __SPRT_WEOF
#define __SPRT_WEOF 0xffff
#endif

typedef unsigned short __SPRT_ID(wctype_t);

typedef struct {
	unsigned long _Wchar;
	unsigned short _Byte unsigned short _State;
} __SPRT_MBSTATE_NAME;

#elif SPRT_ANDROID

#ifdef __SPRT_BUILD
#define __SPRT_MBSTATE_NAME __SPRT_ID(mbstate_t)
#else
#define __SPRT_MBSTATE_NAME mbstate_t
#endif
#define __SPRT_MBSTATE_DIRECT 1

#ifndef __SPRT_WEOF
#define __SPRT_WEOF (__sprt_wint_t)(-1)
#endif

typedef long __SPRT_ID(wctype_t);

typedef struct {
	unsigned char __seq[4];
#ifdef __LP64__
	unsigned char __reserved[4];
#endif
} __SPRT_MBSTATE_NAME;

#elif SPRT_MACOS

#error "Unknown OS"

#else
#error "Unknown OS"
#endif

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_MBSTATE_H_
