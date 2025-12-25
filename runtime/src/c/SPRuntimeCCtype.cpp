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

#include <c/__sprt_ctype.h>
#include "SPRuntimeString.h"

#include <ctype.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(isalnum_impl)(int c) { return ::isalnum(c); }

__SPRT_C_FUNC int __SPRT_ID(isalpha_impl)(int c) { return ::isalpha(c); }

__SPRT_C_FUNC int __SPRT_ID(isblank_impl)(int c) { return ::isblank(c); }

__SPRT_C_FUNC int __SPRT_ID(iscntrl_impl)(int c) { return ::iscntrl(c); }

__SPRT_C_FUNC int __SPRT_ID(isdigit_impl)(int c) { return ::isdigit(c); }

__SPRT_C_FUNC int __SPRT_ID(isgraph_impl)(int c) { return ::isgraph(c); }

__SPRT_C_FUNC int __SPRT_ID(islower_impl)(int c) { return ::islower(c); }

__SPRT_C_FUNC int __SPRT_ID(isprint_impl)(int c) { return ::isprint(c); }

__SPRT_C_FUNC int __SPRT_ID(ispunct_impl)(int c) { return ::ispunct(c); }

__SPRT_C_FUNC int __SPRT_ID(isspace_impl)(int c) { return ::isspace(c); }

__SPRT_C_FUNC int __SPRT_ID(isupper_impl)(int c) { return ::isupper(c); }

__SPRT_C_FUNC int __SPRT_ID(isxdigit_impl)(int c) { return ::isxdigit(c); }

__SPRT_C_FUNC int __SPRT_ID(tolower_impl)(int c) { return ::tolower(c); }

__SPRT_C_FUNC int __SPRT_ID(toupper_impl)(int c) { return ::toupper(c); }


__SPRT_C_FUNC int __SPRT_ID(isalnum_l)(int c, __SPRT_ID(locale_t) loc) { return isalnum_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isalpha_l)(int c, __SPRT_ID(locale_t) loc) { return isalpha_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isblank_l)(int c, __SPRT_ID(locale_t) loc) { return isblank_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(iscntrl_l)(int c, __SPRT_ID(locale_t) loc) { return iscntrl_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isdigit_l)(int c, __SPRT_ID(locale_t) loc) { return isdigit_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isgraph_l)(int c, __SPRT_ID(locale_t) loc) { return isgraph_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(islower_l)(int c, __SPRT_ID(locale_t) loc) { return islower_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isprint_l)(int c, __SPRT_ID(locale_t) loc) { return isprint_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(ispunct_l)(int c, __SPRT_ID(locale_t) loc) { return ispunct_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isspace_l)(int c, __SPRT_ID(locale_t) loc) { return isspace_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isupper_l)(int c, __SPRT_ID(locale_t) loc) { return isupper_l(c, loc); }
__SPRT_C_FUNC int __SPRT_ID(isxdigit_l)(int c, __SPRT_ID(locale_t) loc) {
	return isxdigit_l(c, loc);
}
__SPRT_C_FUNC int __SPRT_ID(tolower_l)(int c, __SPRT_ID(locale_t) loc) {
	return ::tolower_l(c, loc);
}
__SPRT_C_FUNC int __SPRT_ID(toupper_l)(int c, __SPRT_ID(locale_t) loc) {
	return ::toupper_l(c, loc);
}

__SPRT_C_FUNC int __SPRT_ID(isascii)(int c) { return isascii(c); }
__SPRT_C_FUNC int __SPRT_ID(toascii)(int c) { return toascii(c); }

} // namespace sprt
