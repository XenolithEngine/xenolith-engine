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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_
#define CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_

#ifdef __SPRT_BUILD

#include_next <ctype.h>

#else

#include <c/__sprt_ctype.h>

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE inline int isalnum(int c) { return __sprt_isalnum(c); }

SPRT_FORCEINLINE inline int isalpha(int c) { return __sprt_isalpha(c); }

SPRT_FORCEINLINE inline int isblank(int c) { return __sprt_isblank(c); }

SPRT_FORCEINLINE inline int iscntrl(int c) { return __sprt_iscntrl(c); }

SPRT_FORCEINLINE inline int isdigit(int c) { return __sprt_isdigit(c); }

SPRT_FORCEINLINE inline int isgraph(int c) { return __sprt_isgraph(c); }

SPRT_FORCEINLINE inline int islower(int c) { return __sprt_islower(c); }

SPRT_FORCEINLINE inline int isprint(int c) { return __sprt_isprint(c); }

SPRT_FORCEINLINE inline int ispunct(int c) { return __sprt_ispunct(c); }

SPRT_FORCEINLINE inline int isspace(int c) { return __sprt_isspace(c); }

SPRT_FORCEINLINE inline int isupper(int c) { return __sprt_isupper(c); }

SPRT_FORCEINLINE inline int isxdigit(int c) { return __sprt_isxdigit(c); }

SPRT_FORCEINLINE inline int tolower(int c) { return __sprt_tolower(c); }

SPRT_FORCEINLINE inline int toupper(int c) { return __sprt_toupper(c); }

SPRT_FORCEINLINE inline int isalnum_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isalnum_l(c, loc);
}
SPRT_FORCEINLINE inline int isalpha_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isalpha_l(c, loc);
}
SPRT_FORCEINLINE inline int isblank_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isblank_l(c, loc);
}
SPRT_FORCEINLINE inline int iscntrl_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_iscntrl_l(c, loc);
}
SPRT_FORCEINLINE inline int isdigit_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isdigit_l(c, loc);
}
SPRT_FORCEINLINE inline int isgraph_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isgraph_l(c, loc);
}
SPRT_FORCEINLINE inline int islower_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_islower_l(c, loc);
}
SPRT_FORCEINLINE inline int isprint_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isprint_l(c, loc);
}
SPRT_FORCEINLINE inline int ispunct_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_ispunct_l(c, loc);
}
SPRT_FORCEINLINE inline int isspace_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isspace_l(c, loc);
}
SPRT_FORCEINLINE inline int isupper_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isupper_l(c, loc);
}
SPRT_FORCEINLINE inline int isxdigit_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_isxdigit_l(c, loc);
}
SPRT_FORCEINLINE inline int tolower_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_tolower_l(c, loc);
}
SPRT_FORCEINLINE inline int toupper_l(int c, __SPRT_ID(locale_t) loc) {
	return __sprt_toupper_l(c, loc);
}

SPRT_FORCEINLINE inline int isascii(int c) { return __sprt_isascii(c); }
SPRT_FORCEINLINE inline int toascii(int c) { return __sprt_toascii(c); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_
