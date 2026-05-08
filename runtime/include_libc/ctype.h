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

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <ctype.h>

#else

#include <sprt/c/__sprt_ctype.h>

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int isalnum(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalnum(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isalpha(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalpha(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isblank(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isblank(c);
}
#endif

SPRT_UMBRELLA_FUNC
int iscntrl(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iscntrl(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isdigit(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isdigit(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isgraph(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isgraph(c);
}
#endif

SPRT_UMBRELLA_FUNC
int islower(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_islower(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isprint(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isprint(c);
}
#endif

SPRT_UMBRELLA_FUNC
int ispunct(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ispunct(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isspace(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isspace(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isupper(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isupper(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isxdigit(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isxdigit(c);
}
#endif

SPRT_UMBRELLA_FUNC
int tolower(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tolower(c);
}
#endif

SPRT_UMBRELLA_FUNC
int toupper(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toupper(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isalnum_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalnum_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isalpha_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalpha_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isblank_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isblank_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int iscntrl_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iscntrl_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isdigit_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isdigit_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isgraph_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isgraph_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int islower_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_islower_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isprint_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isprint_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int ispunct_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ispunct_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isspace_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isspace_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isupper_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isupper_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isxdigit_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isxdigit_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int tolower_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tolower_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int toupper_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toupper_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isascii(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isascii(c);
}
#endif

SPRT_UMBRELLA_FUNC
int toascii(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toascii(c);
}
#endif

__SPRT_END_DECL

#ifdef _LIBCPP_MSVCRT
#define _isalnum_l isalnum_l
#define _isalpha_l isalpha_l
#define _isblank_l isblank_l
#define _iscntrl_l iscntrl_l
#define _isdigit_l isdigit_l
#define _isgraph_l isgraph_l
#define _islower_l islower_l
#define _isprint_l isprint_l
#define _ispunct_l ispunct_l
#define _isspace_l isspace_l
#define _isupper_l isupper_l
#define _isxdigit_l isxdigit_l
#define _tolower_l tolower_l
#define _toupper_l toupper_l
#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_CTYPE_H_
