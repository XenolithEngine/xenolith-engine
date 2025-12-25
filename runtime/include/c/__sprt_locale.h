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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_LOCALE_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_LOCALE_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_null.h>
#include <c/cross/__sprt_locale.h>

// clang-format off
#define __SPRT_LC_CTYPE    0
#define __SPRT_LC_NUMERIC  1
#define __SPRT_LC_TIME     2
#define __SPRT_LC_COLLATE  3
#define __SPRT_LC_MONETARY 4
#define __SPRT_LC_MESSAGES 5
#define __SPRT_LC_ALL      6

#define __SPRT_LC_GLOBAL_LOCALE ((locale_t)-1)

#define __SPRT_LC_CTYPE_MASK    (1<<LC_CTYPE)
#define __SPRT_LC_NUMERIC_MASK  (1<<LC_NUMERIC)
#define __SPRT_LC_TIME_MASK     (1<<LC_TIME)
#define __SPRT_LC_COLLATE_MASK  (1<<LC_COLLATE)
#define __SPRT_LC_MONETARY_MASK (1<<LC_MONETARY)
#define __SPRT_LC_MESSAGES_MASK (1<<LC_MESSAGES)
#define __SPRT_LC_ALL_MASK      0x7fffffff
// clang-format on

__SPRT_BEGIN_DECL

struct __SPRT_ID(lconv) {
	char *decimal_point;
	char *thousands_sep;
	char *grouping;

	char *int_curr_symbol;
	char *currency_symbol;
	char *mon_decimal_point;
	char *mon_thousands_sep;
	char *mon_grouping;
	char *positive_sign;
	char *negative_sign;
	char int_frac_digits;
	char frac_digits;
	char p_cs_precedes;
	char p_sep_by_space;
	char n_cs_precedes;
	char n_sep_by_space;
	char p_sign_posn;
	char n_sign_posn;
	char int_p_cs_precedes;
	char int_p_sep_by_space;
	char int_n_cs_precedes;
	char int_n_sep_by_space;
	char int_p_sign_posn;
	char int_n_sign_posn;
};

SPRT_API char *__SPRT_ID(setlocale)(int, const char *);
SPRT_API struct __SPRT_ID(lconv) * __SPRT_ID(localeconv)(void);

SPRT_API __SPRT_ID(locale_t) __SPRT_ID(duplocale)(__SPRT_ID(locale_t));
SPRT_API void __SPRT_ID(freelocale)(__SPRT_ID(locale_t));
SPRT_API __SPRT_ID(locale_t) __SPRT_ID(newlocale)(int, const char *, __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(locale_t) __SPRT_ID(uselocale)(__SPRT_ID(locale_t));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_LIMITS_H_
