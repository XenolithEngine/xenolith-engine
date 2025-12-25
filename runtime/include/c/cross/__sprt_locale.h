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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_LOCALE_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_LOCALE_H_

#include <c/bits/__sprt_def.h>

#if SPRT_LINUX

typedef struct __locale_struct *__SPRT_ID(locale_t);

#elif SPRT_WINDOWS

typedef struct __crt_locale_pointers {
	struct __crt_locale_data *locinfo;
	struct __crt_multibyte_data *mbcinfo;
} __crt_locale_pointers;

typedef __crt_locale_pointers *__SPRT_ID(locale_t);

#elif SPRT_ANDROID

typedef struct __locale_t *__SPRT_ID(locale_t);

#elif SPRT_MACOS

#error "Unknown OS"

#else
#error "Unknown OS"
#endif

#endif
