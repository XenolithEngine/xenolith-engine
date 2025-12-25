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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_NS_TYPES_H_
#define CORE_RUNTIME_INCLUDE_LIBC_NS_TYPES_H_

#ifdef __SPRT_BUILD

#include_next <nl_types.h>

#else

#include <c/__sprt_nl_types.h>

#define NL_SETD __SPRT_NL_SETD
#define NL_CAT_LOCALE __SPRT_NL_CAT_LOCALE

__SPRT_BEGIN_DECL

typedef __SPRT_ID(nl_item) nl_item;
typedef __SPRT_ID(nl_catd) nl_catd;

SPRT_FORCEINLINE inline nl_catd catopen(const char *path, int v) { return __sprt_catopen(path, v); }
SPRT_FORCEINLINE inline char *catgets(nl_catd cat, int a, int b, const char *str) {
	return __sprt_catgets(cat, a, b, str);
}
SPRT_FORCEINLINE inline int catclose(nl_catd cat) { return __sprt_catclose(cat); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_NS_TYPES_H_
