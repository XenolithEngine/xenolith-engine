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

/*
	Dispatch header for the POSIX <nl_types.h> (message-catalog support):
	- hosted SPRT build -> forwards to the system <nl_types.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them).

	Macros:
	  NL_SETD       - default message-set number for catgets
	  NL_CAT_LOCALE - catopen flag: use the LC_MESSAGES locale to find the catalog

	Types:
	  nl_item - identifier for a langinfo item
	  nl_catd - handle to an open message catalog

	Functions  [gate: NLTYPES_CAT]:
	  catopen  - open a message catalog, returning an nl_catd handle
	  catgets  - fetch a message by set/message id (falls back to a default string)
	  catclose - close a message catalog
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <nl_types.h>

#else

#include <sprt/c/__sprt_nl_types.h>

#define NL_SETD __SPRT_NL_SETD
#define NL_CAT_LOCALE __SPRT_NL_CAT_LOCALE

__SPRT_BEGIN_DECL

typedef __SPRT_ID(nl_item) nl_item;
typedef __SPRT_ID(nl_catd) nl_catd;

#if __SPRT_CONFIG_HAVE_NLTYPES_CAT || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_UMBRELLA_FUNC
nl_catd catopen(const char *path, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catopen(path, v);
}
#endif

SPRT_UMBRELLA_FUNC
char *catgets(nl_catd cat, int a, int b, const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catgets(cat, a, b, str);
}
#endif

SPRT_UMBRELLA_FUNC
int catclose(nl_catd cat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catclose(cat);
}
#endif

#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_NS_TYPES_H_
