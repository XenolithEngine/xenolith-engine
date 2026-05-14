/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation flies (the "Software"), to deal
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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_WCTYPE_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_WCTYPE_H_

#include <sprt/c/bits/__sprt_wchar_t.h>
#include <sprt/c/bits/__sprt_wint_t.h>
#include <sprt/c/cross/__sprt_mbstate.h>
#include <sprt/c/cross/__sprt_locale.h>

typedef __SPRT_ID(wchar_t) __SPRT_ID(wctrans_t);

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(iswalnum)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswalpha)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswblank)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswcntrl)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswdigit)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswgraph)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswlower)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswprint)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswpunct)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswspace)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswupper)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswxdigit)(__SPRT_ID(wint_t));
SPRT_API int __SPRT_ID(iswctype)(__SPRT_ID(wint_t), __SPRT_ID(wctype_t));
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(towctrans)(__SPRT_ID(wint_t), __SPRT_ID(wctrans_t));
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(towlower)(__SPRT_ID(wint_t));
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(towupper)(__SPRT_ID(wint_t));
SPRT_API __SPRT_ID(wctrans_t) __SPRT_ID(wctrans)(const char *);
SPRT_API __SPRT_ID(wctype_t) __SPRT_ID(wctype)(const char *);

SPRT_API int __SPRT_ID(iswalnum_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswalpha_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswblank_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswcntrl_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswdigit_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswgraph_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswlower_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswprint_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswpunct_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswspace_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswupper_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswxdigit_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(iswctype_l)(__SPRT_ID(wint_t), __SPRT_ID(wctype_t), __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(towlower_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(towupper_l)(__SPRT_ID(wint_t), __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(wint_t)
SPRT_API __SPRT_ID(towctrans_l)(__SPRT_ID(wint_t), __SPRT_ID(wctrans_t), __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(wctrans_t) __SPRT_ID(wctrans_l)(const char *, __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(wctype_t) __SPRT_ID(wctype_l)(const char *, __SPRT_ID(locale_t));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_WCTYPE_H_
