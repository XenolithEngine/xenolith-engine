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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LANGINFO_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LANGINFO_H_

/*
	Dispatch header for <langinfo.h>:
	- hosted SPRT build -> forwards to the system <langinfo.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	nl_langinfo(item)      - query a locale string by item id (e.g. ABDAY_1, D_FMT)
	nl_langinfo_l(item, l) - same, against an explicit locale
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <langinfo.h>

#else

#include <sprt/c/__sprt_langinfo.h>

typedef __SPRT_ID(nl_item) nl_item;
typedef __SPRT_ID(locale_t) locale_t;

#define CODESET __SPRT_CODESET
#define D_T_FMT __SPRT_D_T_FMT
#define D_FMT __SPRT_D_FMT
#define T_FMT __SPRT_T_FMT
#define T_FMT_AMPM __SPRT_T_FMT_AMPM
#define AM_STR __SPRT_AM_STR
#define PM_STR __SPRT_PM_STR

#define DAY_1 __SPRT_DAY_1
#define DAY_2 __SPRT_DAY_2
#define DAY_3 __SPRT_DAY_3
#define DAY_4 __SPRT_DAY_4
#define DAY_5 __SPRT_DAY_5
#define DAY_6 __SPRT_DAY_6
#define DAY_7 __SPRT_DAY_7

#define ABDAY_1 __SPRT_ABDAY_1
#define ABDAY_2 __SPRT_ABDAY_2
#define ABDAY_3 __SPRT_ABDAY_3
#define ABDAY_4 __SPRT_ABDAY_4
#define ABDAY_5 __SPRT_ABDAY_5
#define ABDAY_6 __SPRT_ABDAY_6
#define ABDAY_7 __SPRT_ABDAY_7

#define MON_1 __SPRT_MON_1
#define MON_2 __SPRT_MON_2
#define MON_3 __SPRT_MON_3
#define MON_4 __SPRT_MON_4
#define MON_5 __SPRT_MON_5
#define MON_6 __SPRT_MON_6
#define MON_7 __SPRT_MON_7
#define MON_8 __SPRT_MON_8
#define MON_9 __SPRT_MON_9
#define MON_10 __SPRT_MON_10
#define MON_11 __SPRT_MON_11
#define MON_12 __SPRT_MON_12

#define ABMON_1 __SPRT_ABMON_1
#define ABMON_2 __SPRT_ABMON_2
#define ABMON_3 __SPRT_ABMON_3
#define ABMON_4 __SPRT_ABMON_4
#define ABMON_5 __SPRT_ABMON_5
#define ABMON_6 __SPRT_ABMON_6
#define ABMON_7 __SPRT_ABMON_7
#define ABMON_8 __SPRT_ABMON_8
#define ABMON_9 __SPRT_ABMON_9
#define ABMON_10 __SPRT_ABMON_10
#define ABMON_11 __SPRT_ABMON_11
#define ABMON_12 __SPRT_ABMON_12

#define RADIXCHAR __SPRT_RADIXCHAR
#define THOUSEP __SPRT_THOUSEP
#define YESEXPR __SPRT_YESEXPR
#define NOEXPR __SPRT_NOEXPR
#define CRNCYSTR __SPRT_CRNCYSTR

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
char *nl_langinfo(nl_item item) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nl_langinfo(item);
}
#endif

SPRT_UMBRELLA_FUNC
char *nl_langinfo_l(nl_item item, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nl_langinfo_l(item, loc);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_LANGINFO_H_
