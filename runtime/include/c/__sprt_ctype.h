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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_CTYPE_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_CTYPE_H_

#include <c/bits/__sprt_def.h>
#include <c/cross/__sprt_locale.h>

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(isalnum_impl)(int);
#define __sprt_isalnum __SPRT_ID(isalnum_impl)


SPRT_API int __SPRT_ID(isalpha_impl)(int);
#define __sprt_isalpha __SPRT_ID(isalpha_impl)


SPRT_API int __SPRT_ID(isblank_impl)(int);
#define __sprt_isblank __SPRT_ID(isblank_impl)


SPRT_API int __SPRT_ID(iscntrl_impl)(int);
#define __sprt_iscntrl __SPRT_ID(iscntrl_impl)


SPRT_API int __SPRT_ID(isdigit_impl)(int);
#define __sprt_isdigit __SPRT_ID(isdigit_impl)


SPRT_API int __SPRT_ID(isgraph_impl)(int);
#define __sprt_isgraph __SPRT_ID(isgraph_impl)


SPRT_API int __SPRT_ID(islower_impl)(int);
#define __sprt_islower __SPRT_ID(islower_impl)


SPRT_API int __SPRT_ID(isprint_impl)(int);
#define __sprt_isprint __SPRT_ID(isprint_impl)


SPRT_API int __SPRT_ID(ispunct_impl)(int);
#define __sprt_ispunct __SPRT_ID(ispunct_impl)


SPRT_API int __SPRT_ID(isspace_impl)(int);
#define __sprt_isspace __SPRT_ID(isspace_impl)


int __SPRT_ID(isupper_impl)(int);
#define __sprt_isupper __SPRT_ID(isupper_impl)


SPRT_API int __SPRT_ID(isxdigit_impl)(int);
#define __sprt_isxdigit __SPRT_ID(isxdigit_impl)


SPRT_API int __SPRT_ID(tolower_impl)(int);
#define __sprt_tolower __SPRT_ID(tolower_impl)


SPRT_API int __SPRT_ID(toupper_impl)(int);
#define __sprt_toupper __SPRT_ID(toupper_impl)

SPRT_API int __SPRT_ID(isalnum_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isalpha_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isblank_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iscntrl_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isdigit_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isgraph_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(islower_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isprint_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(ispunct_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isspace_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isupper_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(isxdigit_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(tolower_l)(int, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(toupper_l)(int, __SPRT_ID(locale_t));

SPRT_API int __SPRT_ID(isascii)(int);
SPRT_API int __SPRT_ID(toascii)(int);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_CTYPE_H_
