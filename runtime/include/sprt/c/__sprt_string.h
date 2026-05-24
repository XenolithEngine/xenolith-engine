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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_STRING_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_STRING_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_null.h>
#include <sprt/c/cross/__sprt_locale.h>

__SPRT_BEGIN_DECL

typedef int __SPRT_ID(errno_t);

SPRT_API void *__SPRT_ID(
		memcpy_impl)(void *__SPRT_RESTRICT, const void *__SPRT_RESTRICT, __SPRT_ID(size_t));
SPRT_API void *__SPRT_ID(memmove_impl)(void *, const void *, __SPRT_ID(size_t));
SPRT_API void *__SPRT_ID(memset_impl)(void *, int, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(memcmp_impl)(const void *, const void *, __SPRT_ID(size_t));
SPRT_API const void *__SPRT_ID(memchr_impl)(const void *, int, __SPRT_ID(size_t));

SPRT_API char *__SPRT_ID(strcpy_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT);
SPRT_API char *__SPRT_ID(
		strncpy_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API char *__SPRT_ID(strcat_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT);
SPRT_API char *__SPRT_ID(
		strncat_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API int __SPRT_ID(strcmp_impl)(const char *, const char *);
SPRT_API int __SPRT_ID(strncmp_impl)(const char *, const char *, __SPRT_ID(size_t));

SPRT_API int __SPRT_ID(strcoll_impl)(const char *, const char *);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(
		strxfrm_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API const char *__SPRT_ID(strchr_impl)(const char *, int);
SPRT_API const char *__SPRT_ID(strrchr_impl)(const char *, int);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(strcspn_impl)(const char *, const char *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(strspn_impl)(const char *, const char *);
SPRT_API const char *__SPRT_ID(strpbrk_impl)(const char *, const char *);
SPRT_API const char *__SPRT_ID(strstr_impl)(const char *, const char *);
SPRT_API char *__SPRT_ID(strtok_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(strlen_impl)(const char *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(strnlen_impl)(const char *, __SPRT_ID(size_t));

SPRT_API char *__SPRT_ID(strerror_impl)(int);


#define __sprt_memcpy __SPRT_ID(memcpy_impl)
#define __sprt_memmove __SPRT_ID(memmove_impl)
#define __sprt_memset __SPRT_ID(memset_impl)
#define __sprt_memcmp __SPRT_ID(memcmp_impl)
#define __sprt_memchr __SPRT_ID(memchr_impl)
#define __sprt_strcpy __SPRT_ID(strcpy_impl)
#define __sprt_strncpy __SPRT_ID(strncpy_impl)
#define __sprt_strcat __SPRT_ID(strcat_impl)
#define __sprt_strncat __SPRT_ID(strncat_impl)
#define __sprt_strcmp __SPRT_ID(strcmp_impl)
#define __sprt_strncmp __SPRT_ID(strncmp_impl)
#define __sprt_strcoll __SPRT_ID(strcoll_impl)
#define __sprt_strxfrm __SPRT_ID(strxfrm_impl)
#define __sprt_strchr __SPRT_ID(strchr_impl)
#define __sprt_strrchr __SPRT_ID(strrchr_impl)
#define __sprt_strcspn __SPRT_ID(strcspn_impl)
#define __sprt_strspn __SPRT_ID(strspn_impl)
#define __sprt_strpbrk __SPRT_ID(strpbrk_impl)
#define __sprt_strstr __SPRT_ID(strstr_impl)
#define __sprt_strtok __SPRT_ID(strtok_impl)
#define __sprt_strlen __SPRT_ID(strlen_impl)
#define __sprt_strnlen __SPRT_ID(strnlen_impl)
#define __sprt_strerror __SPRT_ID(strerror_impl)

SPRT_API char *__SPRT_ID(strdup)(const char *str);

SPRT_API __SPRT_ID(errno_t)
		__SPRT_ID(strerror_s)(char *buf, __SPRT_ID(rsize_t) bufsz, __SPRT_ID(errno_t) errnum);

SPRT_API int __SPRT_ID(strverscmp)(const char *l0, const char *r0);

SPRT_API int __SPRT_ID(strcasecmp)(const char *, const char *);
SPRT_API int __SPRT_ID(strncasecmp)(const char *, const char *, __SPRT_ID(rsize_t));

SPRT_API int __SPRT_ID(strcasecmp_l)(const char *, const char *, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(
		strncasecmp_l)(const char *, const char *, __SPRT_ID(rsize_t), __SPRT_ID(locale_t));

SPRT_API char *__SPRT_ID(strtok_r)(char *, const char *, char **);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
