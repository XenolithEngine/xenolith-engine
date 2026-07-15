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

#if __STDC_HOSTED__ == 1

// clang-format off
#ifdef __STDC_VERSION__
#define _POSIX_C_SOURCE __STDC_VERSION__
#else
#define _POSIX_C_SOURCE 200112L
#endif

#define __STDC_WANT_LIB_EXT1__ 1
#define __USE_XOPEN2K8 1
// clang-format on
#endif

#include <sprt/c/__sprt_string.h>
#include <sprt/runtime/init.h>
#include <sprt/cxx/cctype>

#include <string.h>
#include <ctype.h>

#if __STDC_HOSTED__ == 1
// POSIX places strcasecmp/strncasecmp in <strings.h>, not <string.h> (notably
// Darwin, which does not surface them from <string.h>).
#include <strings.h>
#endif

#if SPRT_APPLE
// Darwin declares the locale-aware strcasecmp_l/strncasecmp_l in <xlocale.h>.
#include <xlocale.h>
#endif

#if SPRT_APPLE
// Missed in string.h public header, but not private
__SPRT_C_FUNC __SPRT_ID(size_t) strnlen(const char *__SPRT_RESTRICT str, __SPRT_ID(size_t) n);
#endif

namespace sprt {

#if __STDC_HOSTED__ == 1
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

__SPRT_C_FUNC int __SPRT_ID(strcmp_impl)(const char *l, const char *r) { return ::strcmp(l, r); }

__SPRT_C_FUNC int __SPRT_ID(strncmp_impl)(const char *l, const char *r, size_t size) {
	return ::strncmp(l, r, size);
}
#endif

__SPRT_C_FUNC char *__SPRT_ID(
		strcpy_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) {
	return ::strcpy(dest, src);
}

__SPRT_C_FUNC char *__SPRT_ID(
		strncpy_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src, size_t size) {
	return ::strncpy(dest, src, size);
}


__SPRT_C_FUNC char *__SPRT_ID(
		strcat_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) {
	return ::strcat(dest, src);
}

__SPRT_C_FUNC char *__SPRT_ID(
		strncat_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src, size_t size) {
	return ::strncat(dest, src, size);
}

#if __STDC_HOSTED__ == 1

__SPRT_C_FUNC int __SPRT_ID(strcoll_impl)(const char *str, const char *loc) {
	return ::strcoll(str, loc);
}

__SPRT_C_FUNC size_t __SPRT_ID(
		strxfrm_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src, size_t size) {
	return ::strxfrm(dest, src, size);
}

#else

// Freestanding: the public strcoll()/strxfrm() inlines redirect right back into
// these _impl shims (via __sprt_strcoll/__sprt_strxfrm), so calling ::strcoll /
// ::strxfrm here would recurse infinitely. Route to the libc_impl entry points
// by their internal names instead.
extern "C" int __strcoll(const char *, const char *);
extern "C" size_t __strxfrm(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, size_t);

__SPRT_C_FUNC int __SPRT_ID(strcoll_impl)(const char *str, const char *loc) {
	return __strcoll(str, loc);
}

__SPRT_C_FUNC size_t __SPRT_ID(
		strxfrm_impl)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src, size_t size) {
	return __strxfrm(dest, src, size);
}

#endif


__SPRT_C_FUNC const char *__SPRT_ID(strchr_impl)(const char *str, int c) {
	return ::strchr(str, c);
}

__SPRT_C_FUNC const char *__SPRT_ID(strrchr_impl)(const char *str, int c) {
	return ::strrchr(str, c);
}


__SPRT_C_FUNC size_t __SPRT_ID(strcspn_impl)(const char *str, const char *span) {
	return ::strcspn(str, span);
}

__SPRT_C_FUNC size_t __SPRT_ID(strspn_impl)(const char *str, const char *span) {
	return ::strspn(str, span);
}

__SPRT_C_FUNC const char *__SPRT_ID(strpbrk_impl)(const char *str, const char *span) {
	return ::strpbrk(str, span);
}

__SPRT_C_FUNC const char *__SPRT_ID(strstr_impl)(const char *str, const char *nstr) {
	return ::strstr(str, nstr);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(strlen_impl)(const char *__SPRT_RESTRICT str) {
	return ::strlen(str);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(strnlen_impl)(const char *__SPRT_RESTRICT str, __SPRT_ID(size_t) n) {
	return ::strnlen(str, n);
}

__SPRT_C_FUNC int __SPRT_ID(strcasecmp)(const char *l, const char *r) { return ::strcasecmp(l, r); }
__SPRT_C_FUNC int __SPRT_ID(strncasecmp)(const char *l, const char *r, __SPRT_ID(rsize_t) n) {
	return ::strncasecmp(l, r, n);
}

__SPRT_C_FUNC int __SPRT_ID(strcasecmp_l)(const char *l, const char *r, __SPRT_ID(locale_t) loc) {
	return ::strcasecmp_l(l, r, loc);
}

__SPRT_C_FUNC int __SPRT_ID(strncasecmp_l)(const char *l, const char *r, __SPRT_ID(rsize_t) n,
		__SPRT_ID(locale_t) loc) {
	return ::strncasecmp_l(l, r, n, loc);
}

__SPRT_C_FUNC int __SPRT_ID(strcoll_l)(const char *l, const char *r, __SPRT_ID(locale_t) loc) {
	return ::strcoll_l(l, r, loc);
}

__SPRT_C_FUNC size_t __SPRT_ID(strxfrm_l)(char *__SPRT_RESTRICT dest,
		const char *__SPRT_RESTRICT src, size_t size, __SPRT_ID(locale_t) loc) {
	return ::strxfrm_l(dest, src, size, loc);
}

__SPRT_C_FUNC char *__SPRT_ID(
		strtok_impl)(char *__SPRT_RESTRICT s, const char *__SPRT_RESTRICT sep) {
	return ::strtok(s, sep);
}


__SPRT_C_FUNC char *__SPRT_ID(strtok_r)(char *s, const char *sep, char **p) {
	return ::strtok_r(s, sep, p);
}

__SPRT_C_FUNC char *__SPRT_ID(strdup)(const char *str) { return ::strdup(str); }

} // namespace sprt
