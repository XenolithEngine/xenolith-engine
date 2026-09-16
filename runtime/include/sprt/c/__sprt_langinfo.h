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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_LANGINFO_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_LANGINFO_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_nl_types.h>          // __SPRT_ID(nl_item)
#include <sprt/c/cross/__sprt_locale.h>      // __SPRT_ID(locale_t)
#include <sprt/c/cross/__sprt_langinfo.h>    // per-platform __SPRT_* item bases

// Items within a category are contiguous on every supported libc, so the indexed
// members are derived from the per-platform base. Only the bases (and a few
// endpoints) are static-asserted against the platform <langinfo.h>.
#define __SPRT_ABDAY_2 (__SPRT_ABDAY_1 + 1)
#define __SPRT_ABDAY_3 (__SPRT_ABDAY_1 + 2)
#define __SPRT_ABDAY_4 (__SPRT_ABDAY_1 + 3)
#define __SPRT_ABDAY_5 (__SPRT_ABDAY_1 + 4)
#define __SPRT_ABDAY_6 (__SPRT_ABDAY_1 + 5)
#define __SPRT_ABDAY_7 (__SPRT_ABDAY_1 + 6)

#define __SPRT_DAY_2 (__SPRT_DAY_1 + 1)
#define __SPRT_DAY_3 (__SPRT_DAY_1 + 2)
#define __SPRT_DAY_4 (__SPRT_DAY_1 + 3)
#define __SPRT_DAY_5 (__SPRT_DAY_1 + 4)
#define __SPRT_DAY_6 (__SPRT_DAY_1 + 5)
#define __SPRT_DAY_7 (__SPRT_DAY_1 + 6)

#define __SPRT_ABMON_2 (__SPRT_ABMON_1 + 1)
#define __SPRT_ABMON_3 (__SPRT_ABMON_1 + 2)
#define __SPRT_ABMON_4 (__SPRT_ABMON_1 + 3)
#define __SPRT_ABMON_5 (__SPRT_ABMON_1 + 4)
#define __SPRT_ABMON_6 (__SPRT_ABMON_1 + 5)
#define __SPRT_ABMON_7 (__SPRT_ABMON_1 + 6)
#define __SPRT_ABMON_8 (__SPRT_ABMON_1 + 7)
#define __SPRT_ABMON_9 (__SPRT_ABMON_1 + 8)
#define __SPRT_ABMON_10 (__SPRT_ABMON_1 + 9)
#define __SPRT_ABMON_11 (__SPRT_ABMON_1 + 10)
#define __SPRT_ABMON_12 (__SPRT_ABMON_1 + 11)

#define __SPRT_MON_2 (__SPRT_MON_1 + 1)
#define __SPRT_MON_3 (__SPRT_MON_1 + 2)
#define __SPRT_MON_4 (__SPRT_MON_1 + 3)
#define __SPRT_MON_5 (__SPRT_MON_1 + 4)
#define __SPRT_MON_6 (__SPRT_MON_1 + 5)
#define __SPRT_MON_7 (__SPRT_MON_1 + 6)
#define __SPRT_MON_8 (__SPRT_MON_1 + 7)
#define __SPRT_MON_9 (__SPRT_MON_1 + 8)
#define __SPRT_MON_10 (__SPRT_MON_1 + 9)
#define __SPRT_MON_11 (__SPRT_MON_1 + 10)
#define __SPRT_MON_12 (__SPRT_MON_1 + 11)

__SPRT_BEGIN_DECL

SPRT_API char *__SPRT_ID(nl_langinfo)(__SPRT_ID(nl_item));
SPRT_API char *__SPRT_ID(nl_langinfo_l)(__SPRT_ID(nl_item), __SPRT_ID(locale_t));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_LANGINFO_H_
