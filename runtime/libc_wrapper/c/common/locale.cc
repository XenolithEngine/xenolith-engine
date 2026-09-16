/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_locale.h>
#include <sprt/c/__sprt_errno.h>

#include <stddef.h>
#include <locale.h>

#if SPRT_APPLE
#include <xlocale.h>
#endif

// setlocale()/newlocale() forward the category number and category mask verbatim
// to the platform locale API, so the SPRT values must match the platform's. On a
// hosted build <locale.h> above resolves to the platform header, making these
// checks meaningful (they are tautological on a freestanding build).
static_assert(LC_CTYPE == __SPRT_LC_CTYPE);
static_assert(LC_NUMERIC == __SPRT_LC_NUMERIC);
static_assert(LC_TIME == __SPRT_LC_TIME);
static_assert(LC_COLLATE == __SPRT_LC_COLLATE);
static_assert(LC_MONETARY == __SPRT_LC_MONETARY);
static_assert(LC_MESSAGES == __SPRT_LC_MESSAGES);
static_assert(LC_ALL == __SPRT_LC_ALL);

static_assert(LC_CTYPE_MASK == __SPRT_LC_CTYPE_MASK);
static_assert(LC_NUMERIC_MASK == __SPRT_LC_NUMERIC_MASK);
static_assert(LC_TIME_MASK == __SPRT_LC_TIME_MASK);
static_assert(LC_COLLATE_MASK == __SPRT_LC_COLLATE_MASK);
static_assert(LC_MONETARY_MASK == __SPRT_LC_MONETARY_MASK);
static_assert(LC_MESSAGES_MASK == __SPRT_LC_MESSAGES_MASK);
// SPRT exposes only the six standard categories, so its LC_ALL_MASK must be a
// subset of the platform's (glibc additionally has LC_PAPER, LC_NAME, ...);
// otherwise newlocale() would reject it.
static_assert((__SPRT_LC_ALL_MASK & LC_ALL_MASK) == __SPRT_LC_ALL_MASK);

// localeconv() returns the platform's struct lconv* reinterpreted as the SPRT
// type, so every field must sit at the same offset (the C standard does not fix
// the order, and glibc/BSD differ in the int_* currency flags). offsetof of a
// field present in both structs validates the shared layout member by member.
// Hosted only: on a freestanding build `lconv` is the SPRT type itself (the cast
// is identity) and there is no distinct platform `struct lconv` tag to compare.
#if __STDC_HOSTED__ == 1
#define __SPRT_LCONV_SAME(field) \
	static_assert(offsetof(struct lconv, field) == offsetof(struct __SPRT_ID(lconv), field))
static_assert(sizeof(struct lconv) == sizeof(struct __SPRT_ID(lconv)));
__SPRT_LCONV_SAME(decimal_point);
__SPRT_LCONV_SAME(thousands_sep);
__SPRT_LCONV_SAME(grouping);
__SPRT_LCONV_SAME(int_curr_symbol);
__SPRT_LCONV_SAME(currency_symbol);
__SPRT_LCONV_SAME(mon_decimal_point);
__SPRT_LCONV_SAME(mon_thousands_sep);
__SPRT_LCONV_SAME(mon_grouping);
__SPRT_LCONV_SAME(positive_sign);
__SPRT_LCONV_SAME(negative_sign);
__SPRT_LCONV_SAME(int_frac_digits);
__SPRT_LCONV_SAME(frac_digits);
__SPRT_LCONV_SAME(p_cs_precedes);
__SPRT_LCONV_SAME(p_sep_by_space);
__SPRT_LCONV_SAME(n_cs_precedes);
__SPRT_LCONV_SAME(n_sep_by_space);
__SPRT_LCONV_SAME(p_sign_posn);
__SPRT_LCONV_SAME(n_sign_posn);
__SPRT_LCONV_SAME(int_p_cs_precedes);
__SPRT_LCONV_SAME(int_p_sep_by_space);
__SPRT_LCONV_SAME(int_n_cs_precedes);
__SPRT_LCONV_SAME(int_n_sep_by_space);
__SPRT_LCONV_SAME(int_p_sign_posn);
__SPRT_LCONV_SAME(int_n_sign_posn);
#undef __SPRT_LCONV_SAME
#endif // __STDC_HOSTED__ == 1

namespace sprt {

__SPRT_C_FUNC char *__SPRT_ID(setlocale)(int cat, const char *locale) {
	return ::setlocale(cat, locale);
}

__SPRT_C_FUNC struct __SPRT_ID(lconv) * __SPRT_ID(localeconv)(void) {
	return (struct __SPRT_ID(lconv) *)::localeconv();
}

__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(duplocale)(__SPRT_ID(locale_t) loc) {
	return ::duplocale(loc);
}
__SPRT_C_FUNC void __SPRT_ID(freelocale)(__SPRT_ID(locale_t) loc) {
	::freelocale(loc); //
}
__SPRT_C_FUNC __SPRT_ID(locale_t)
		__SPRT_ID(newlocale)(int v, const char *name, __SPRT_ID(locale_t) loc) {
	return ::newlocale(v, name, loc);
}
__SPRT_C_FUNC __SPRT_ID(locale_t) __SPRT_ID(uselocale)(__SPRT_ID(locale_t) loc) {
	return ::uselocale(loc);
}

} // namespace sprt
