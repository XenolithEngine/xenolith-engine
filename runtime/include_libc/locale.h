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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LOCALE_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LOCALE_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <locale.h>

#else

#include <sprt/c/__sprt_locale.h>

#define LC_CTYPE __SPRT_LC_CTYPE
#define LC_NUMERIC __SPRT_LC_NUMERIC
#define LC_TIME __SPRT_LC_TIME
#define LC_COLLATE __SPRT_LC_COLLATE
#define LC_MONETARY __SPRT_LC_MONETARY
#define LC_MESSAGES __SPRT_LC_MESSAGES
#define LC_ALL __SPRT_LC_ALL

#define LC_GLOBAL_LOCALE __SPRT_LC_GLOBAL_LOCALE

#define LC_CTYPE_MASK __SPRT_LC_CTYPE_MASK
#define LC_NUMERIC_MASK __SPRT_LC_NUMERIC_MASK
#define LC_TIME_MASK __SPRT_LC_TIME_MASK
#define LC_COLLATE_MASK __SPRT_LC_COLLATE_MASK
#define LC_MONETARY_MASK __SPRT_LC_MONETARY_MASK
#define LC_MESSAGES_MASK __SPRT_LC_MESSAGES_MASK
#define LC_ALL_MASK __SPRT_LC_ALL_MASK

typedef __SPRT_ID(lconv) lconv;
typedef __SPRT_ID(locale_t) locale_t;

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
char *setlocale(int cat, const char *locale) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_setlocale(cat, locale);
}
#endif

SPRT_UMBRELLA_FUNC
lconv *localeconv(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_localeconv();
}
#endif

SPRT_UMBRELLA_FUNC
locale_t duplocale(locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_duplocale(loc);
}
#endif

SPRT_UMBRELLA_FUNC
void freelocale(locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_freelocale(loc);
}
#endif

SPRT_UMBRELLA_FUNC
locale_t newlocale(int v, const char *name, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_newlocale(v, name, loc);
}
#endif

SPRT_UMBRELLA_FUNC
locale_t uselocale(locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_uselocale(loc);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_LOCALE_H_
