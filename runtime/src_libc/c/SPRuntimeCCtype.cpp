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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_ctype.h>
#include <sprt/cxx/detail/constexpr.h>

__SPRT_C_FUNC int __SPRT_ID(isalnum_impl)(int c) { return sprt::__constexpr_isalnum_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isalpha_impl)(int c) { return sprt::__constexpr_isalpha_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isblank_impl)(int c) { return sprt::__constexpr_isblank_c(c); }

__SPRT_C_FUNC int __SPRT_ID(iscntrl_impl)(int c) { return sprt::__constexpr_iscntrl_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isdigit_impl)(int c) { return sprt::__constexpr_isdigit_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isgraph_impl)(int c) { return sprt::__constexpr_isgraph_c(c); }

__SPRT_C_FUNC int __SPRT_ID(islower_impl)(int c) { return sprt::__constexpr_islower_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isprint_impl)(int c) { return sprt::__constexpr_isprint_c(c); }

__SPRT_C_FUNC int __SPRT_ID(ispunct_impl)(int c) { return sprt::__constexpr_ispunct_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isspace_impl)(int c) { return sprt::__constexpr_isspace_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isupper_impl)(int c) { return sprt::__constexpr_isupper_c(c); }

__SPRT_C_FUNC int __SPRT_ID(isxdigit_impl)(int c) { return sprt::__constexpr_isxdigit_c(c); }

__SPRT_C_FUNC int __SPRT_ID(tolower_impl)(int c) {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_tolower_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC int __SPRT_ID(toupper_impl)(int c) {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_toupper_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC int __SPRT_ID(isalnum_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isalnum(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isalpha_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isalpha(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isblank_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isblank(c); //
}
__SPRT_C_FUNC int __SPRT_ID(iscntrl_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_iscntrl(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isdigit_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isdigit(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isgraph_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isgraph(c); //
}
__SPRT_C_FUNC int __SPRT_ID(islower_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_islower(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isprint_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isprint(c); //
}
__SPRT_C_FUNC int __SPRT_ID(ispunct_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_ispunct(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isspace_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isspace(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isupper_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isupper(c); //
}
__SPRT_C_FUNC int __SPRT_ID(isxdigit_l)(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isxdigit(c); //
}
__SPRT_C_FUNC int __SPRT_ID(tolower_l)(int c, __SPRT_ID(locale_t) loc) { return __sprt_tolower(c); }
__SPRT_C_FUNC int __SPRT_ID(toupper_l)(int c, __SPRT_ID(locale_t) loc) { return __sprt_toupper(c); }

__SPRT_C_FUNC int __SPRT_ID(isascii)(int c) { return (c & 0x7F) == c; }
__SPRT_C_FUNC int __SPRT_ID(toascii)(int c) { return c & 0x7F; }
