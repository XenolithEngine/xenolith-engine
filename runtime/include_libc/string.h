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

#ifdef __SPRT_BUILD

#include_next <string.h>

#else

#include <c/__sprt_string.h>

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(rsize_t) rsize_t;
typedef __SPRT_ID(errno_t) errno_t;

SPRT_FORCEINLINE inline void *memcpy(void *__SPRT_RESTRICT dest, const void *__SPRT_RESTRICT source,
		size_t size) {
	return __sprt_memcpy(dest, source, size);
}

SPRT_FORCEINLINE inline void *memmove(void *dest, const void *source, size_t size) {
	return __sprt_memmove(dest, source, size);
}

SPRT_FORCEINLINE inline void *memset(void *dest, int c, size_t size) {
	return __sprt_memset(dest, c, size);
}

SPRT_FORCEINLINE inline int memcmp(const void *l, const void *r, size_t size) {
	return __sprt_memcmp(l, r, size);
}

SPRT_FORCEINLINE inline const void *memchr(const void *str, int c, size_t size) {
	return __sprt_memchr(str, c, size);
}


SPRT_FORCEINLINE inline char *strcpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) {
	return __sprt_strcpy(dest, src);
}

SPRT_FORCEINLINE inline char *strncpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) {
	return __sprt_strncpy(dest, src, size);
}


SPRT_FORCEINLINE inline char *strcat(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) {
	return __sprt_strcat(dest, src);
}

SPRT_FORCEINLINE inline char *strncat(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) {
	return __sprt_strncat(dest, src, size);
}


SPRT_FORCEINLINE inline int strcmp(const char *l, const char *r) { return __sprt_strcmp(l, r); }

SPRT_FORCEINLINE inline int strncmp(const char *l, const char *r, size_t size) {
	return __sprt_strncmp(l, r, size);
}


SPRT_FORCEINLINE inline int strcoll(const char *str, const char *loc) {
	return __sprt_strcoll(str, loc);
}

SPRT_FORCEINLINE inline size_t strxfrm(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) {
	return __sprt_strxfrm(dest, src, size);
}


SPRT_FORCEINLINE inline char *strchr(const char *str, int c) {
	return (char *)__sprt_strchr(str, c);
}

SPRT_FORCEINLINE inline char *strrchr(const char *str, int c) {
	return (char *)__sprt_strrchr(str, c);
}

SPRT_FORCEINLINE inline size_t strcspn(const char *str, const char *span) {
	return __sprt_strcspn(str, span);
}

SPRT_FORCEINLINE inline size_t strspn(const char *str, const char *span) {
	return __sprt_strspn(str, span);
}

SPRT_FORCEINLINE inline const char *strpbrk(const char *str, const char *span) {
	return __sprt_strpbrk(str, span);
}

SPRT_FORCEINLINE inline const char *strstr(const char *str, const char *nstr) {
	return __sprt_strstr(str, nstr);
}

SPRT_FORCEINLINE inline char *strtok(char *__SPRT_RESTRICT str, const char *__SPRT_RESTRICT tok) {
	return __sprt_strtok(str, tok);
}

SPRT_FORCEINLINE inline size_t strlen(const char *str) { return __sprt_strlen(str); }

SPRT_FORCEINLINE inline char *strerror(int err) { return __sprt_strerror(err); }

SPRT_FORCEINLINE inline errno_t strerror_s(char *buf, rsize_t bufsz, errno_t errnum) {
	return __sprt_strerror_s(buf, bufsz, errnum);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
