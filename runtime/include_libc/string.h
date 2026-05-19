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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
#define CORE_RUNTIME_INCLUDE_LIBC_STRING_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <string.h>

#else

#include <sprt/c/__sprt_string.h>

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(rsize_t) rsize_t;
typedef __SPRT_ID(errno_t) errno_t;

SPRT_UMBRELLA_FUNC
void *memcpy(void *__SPRT_RESTRICT dest, const void *__SPRT_RESTRICT source,
		size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memcpy(dest, source, size);
}
#endif

SPRT_UMBRELLA_FUNC
void *memmove(void *dest, const void *source, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memmove(dest, source, size);
}
#endif

SPRT_UMBRELLA_FUNC
void *memset(void *dest, int c, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memset(dest, c, size);
}
#endif

SPRT_UMBRELLA_FUNC
int memcmp(const void *l, const void *r, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memcmp(l, r, size);
}
#endif

SPRT_UMBRELLA_FUNC
const void *memchr(const void *str, int c, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_memchr(str, c, size);
}
#endif


SPRT_UMBRELLA_FUNC
char *strcpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcpy(dest, src);
}
#endif

SPRT_UMBRELLA_FUNC
char *strncpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strncpy(dest, src, size);
}
#endif


SPRT_UMBRELLA_FUNC
char *strcat(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcat(dest, src);
}
#endif

SPRT_UMBRELLA_FUNC
char *strncat(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strncat(dest, src, size);
}
#endif


SPRT_UMBRELLA_FUNC
int strcmp(const char *l, const char *r) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcmp(l, r);
}
#endif

SPRT_UMBRELLA_FUNC
int strncmp(const char *l, const char *r, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strncmp(l, r, size);
}
#endif


SPRT_UMBRELLA_FUNC
int strcoll(const char *str, const char *loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcoll(str, loc);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strxfrm(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strxfrm(dest, src, size);
}
#endif


SPRT_UMBRELLA_FUNC
char *strchr(const char *str, int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return (char *)__sprt_strchr(str, c);
}
#endif

SPRT_UMBRELLA_FUNC
char *strrchr(const char *str, int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return (char *)__sprt_strrchr(str, c);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strcspn(const char *str, const char *span) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcspn(str, span);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strspn(const char *str, const char *span) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strspn(str, span);
}
#endif

SPRT_UMBRELLA_FUNC
const char *strpbrk(const char *str, const char *span) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strpbrk(str, span);
}
#endif

SPRT_UMBRELLA_FUNC
const char *strstr(const char *str, const char *nstr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strstr(str, nstr);
}
#endif

SPRT_UMBRELLA_FUNC
char *strtok(char *__SPRT_RESTRICT str, const char *__SPRT_RESTRICT tok) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtok(str, tok);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strlen(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strlen(str);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strnlen(const char *str, size_t n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strnlen(str, n);
}
#endif

SPRT_UMBRELLA_FUNC
char *strerror(int err) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strerror(err);
}
#endif

SPRT_UMBRELLA_FUNC
errno_t strerror_s(char *buf, rsize_t bufsz, errno_t errnum) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strerror_s(buf, bufsz, errnum);
}
#endif

SPRT_UMBRELLA_FUNC
int strverscmp(const char *l0, const char *r0) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strverscmp(l0, r0);
}
#endif

SPRT_UMBRELLA_FUNC
char *strtok_r(char *s, const char *sep, char **p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtok_r(s, sep, p);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
