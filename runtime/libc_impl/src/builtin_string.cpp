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

#include <sprt/c/bits/__sprt_def.h>
#include "ctype.h"
#include "string.h"

#if SPRT_WINDOWS
#include "windows/string.cc"
#endif

__SPRT_C_FUNC int tolower(int c) __SPRT_NOEXCEPT {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_tolower_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC int toupper(int c) __SPRT_NOEXCEPT {
	if (c > 0 && c <= 0x7F) {
		return int(sprt::__constexpr_toupper_c(char(c)));
	}
	return c;
}

__SPRT_C_FUNC int __SPRT_ID(strcmp_impl)(const char *l, const char *r) { return ::strcmp(l, r); }

__SPRT_C_FUNC int __SPRT_ID(strncmp_impl)(const char *l, const char *r, size_t size) {
	return ::strncmp(l, r, size);
}
__SPRT_C_FUNC void *__SPRT_ID(
		memcpy_impl)(void *__SPRT_RESTRICT dest, const void *__SPRT_RESTRICT source, size_t size) {
	return ::memcpy(dest, source, size);
}

__SPRT_C_FUNC void *__SPRT_ID(memmove_impl)(void *dest, const void *source, size_t size) {
	return ::memmove(dest, source, size);
}

__SPRT_C_FUNC void *__SPRT_ID(memset_impl)(void *dest, int c, size_t size) {
	return ::memset(dest, c, size);
}

__SPRT_C_FUNC int __SPRT_ID(memcmp_impl)(const void *l, const void *r, size_t size) {
	return ::memcmp(l, r, size);
}

__SPRT_C_FUNC const void *__SPRT_ID(memchr_impl)(const void *str, int c, size_t size) {
	return ::memchr(str, c, size);
}
