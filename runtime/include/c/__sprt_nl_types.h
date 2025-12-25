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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_NL_TYPES_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_NL_TYPES_H_

#include <c/bits/__sprt_def.h>

#define __SPRT_NL_SETD 1
#define __SPRT_NL_CAT_LOCALE 1

__SPRT_BEGIN_DECL

typedef int __SPRT_ID(nl_item);
typedef void *__SPRT_ID(nl_catd);

SPRT_API __SPRT_ID(nl_catd) __SPRT_ID(catopen)(const char *, int);
SPRT_API char *__SPRT_ID(catgets)(__SPRT_ID(nl_catd), int, int, const char *);
SPRT_API int __SPRT_ID(catclose)(__SPRT_ID(nl_catd));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_NL_TYPES_H_
