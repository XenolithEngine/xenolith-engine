/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_WCTYPES_H_
#define CORE_RUNTIME_INCLUDE_LIBC_WCTYPES_H_

/*
	Dispatch header for <wctype.h>:
	- hosted SPRT build -> forwards to the system <wctype.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded):

	Types:
	  wint_t    - integer type able to hold any wide char plus WEOF
	  wctype_t  - opaque handle for a character class (from wctype)
	  wctrans_t - opaque handle for a character mapping (from wctrans)
	  locale_t  - opaque locale handle (for the _l variants)

	Classification predicates (return non-zero on match):
	  iswalnum  - alphanumeric        iswalpha  - alphabetic
	  iswblank  - blank (space/tab)   iswcntrl  - control character
	  iswdigit  - decimal digit       iswgraph  - printable, non-space
	  iswlower  - lowercase           iswprint  - printable (incl. space)
	  iswpunct  - punctuation         iswspace  - whitespace
	  iswupper  - uppercase           iswxdigit - hexadecimal digit
	  iswctype  - test against a wctype_t class obtained from wctype

	Case / transformation:
	  towlower  - convert a wide char to lowercase
	  towupper  - convert a wide char to uppercase
	  towctrans - apply a wctrans_t mapping obtained from wctrans

	Class / mapping lookup by name:
	  wctype    - look up a classification class (e.g. "alpha") by name
	  wctrans   - look up a mapping (e.g. "tolower") by name

	Locale-aware variants (each takes an explicit locale_t):
	  iswalnum_l, iswalpha_l, iswblank_l, iswcntrl_l, iswdigit_l, iswgraph_l,
	  iswlower_l, iswprint_l, iswpunct_l, iswspace_l, iswupper_l, iswxdigit_l,
	  iswctype_l, towlower_l, towupper_l, towctrans_l, wctrans_l, wctype_l
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <wctype.h>

#else

#include <sprt/c/__sprt_wctype.h>

typedef __SPRT_ID(wint_t) wint_t;
typedef __SPRT_ID(wctype_t) wctype_t;
typedef __SPRT_ID(wctrans_t) wctrans_t;
typedef __SPRT_ID(locale_t) locale_t;

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int iswalnum(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswalnum(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswalpha(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswalpha(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswblank(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswblank(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswcntrl(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswcntrl(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswdigit(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswdigit(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswgraph(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswgraph(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswlower(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswlower(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswprint(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswprint(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswpunct(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswpunct(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswspace(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswspace(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswupper(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswupper(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswxdigit(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswxdigit(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswctype(wint_t ch, wctype_t t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswctype(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towctrans(wint_t ch, wctrans_t t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towctrans(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towlower(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towlower(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towupper(wint_t ch) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towupper(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wctrans_t wctrans(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wctrans(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wctype_t wctype(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wctype(ch);
}
#endif


SPRT_UMBRELLA_FUNC
int iswalnum_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswalnum_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswalpha_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswalpha_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswblank_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswblank_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswcntrl_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswcntrl_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswdigit_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswdigit_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswgraph_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswgraph_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswlower_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswlower_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswprint_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswprint_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswpunct_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswpunct_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswspace_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswspace_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswupper_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswupper_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswxdigit_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswxdigit_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
int iswctype_l(wint_t ch, wctype_t t, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iswctype_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towlower_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towlower_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towupper_l(wint_t ch, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towupper_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towctrans_l(wint_t ch, wctrans_t t, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towctrans_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wctrans_t wctrans_l(const char *str, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wctrans_l(ch);
}
#endif

SPRT_UMBRELLA_FUNC
wctype_t wctype_l(const char *str, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wctype_l(ch);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FEATURES_H_
