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

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_null.h>

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

SPRT_API char *__SPRT_ID(strerror_impl)(int);


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_memcpy)
SPRT_FORCEINLINE inline void *__SPRT_ID(memcpy)(void *__SPRT_RESTRICT dest,
		const void *__SPRT_RESTRICT source, __SPRT_ID(size_t) size) {
	return __builtin_memcpy(dest, source, size);
}
#else
#define __sprt_memcpy __SPRT_ID(memcpy_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_memmove)
SPRT_FORCEINLINE inline void *__SPRT_ID(
		memmove)(void *dest, const void *source, __SPRT_ID(size_t) size) {
	return __builtin_memmove(dest, source, size);
}
#else
#define __sprt_memmove __SPRT_ID(memmove_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_memset)
SPRT_FORCEINLINE inline void *__SPRT_ID(memset)(void *dest, int val, __SPRT_ID(size_t) size) {
	return __builtin_memset(dest, val, size);
}
#else
#define __sprt_memset __SPRT_ID(memset_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_memcmp)
SPRT_FORCEINLINE inline int __SPRT_ID(
		memcmp)(const void *l, const void *r, __SPRT_ID(size_t) size) {
	return __builtin_memcmp(l, r, size);
}
#else
#define __sprt_memcmp __SPRT_ID(memcmp_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_memchr)
SPRT_FORCEINLINE inline const void *__SPRT_ID(
		memchr)(const void *s, int c, __SPRT_ID(size_t) size) {
	return __builtin_memchr(s, c, size);
}
#else
#define __sprt_memchr __SPRT_ID(memchr_impl)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strcpy)
SPRT_FORCEINLINE inline char *__SPRT_ID(
		strcpy)(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src) {
	return __builtin_strcpy(dest, src);
}
#else
#define __sprt_strcpy __SPRT_ID(strcpy_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strncpy)
SPRT_FORCEINLINE inline char *__SPRT_ID(strncpy)(char *__SPRT_RESTRICT dest,
		const char *__SPRT_RESTRICT src, __SPRT_ID(size_t) size) {
	return __builtin_strncpy(dest, src, size);
}
#else
#define __sprt_strncpy __SPRT_ID(strncpy_impl)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strcat)
SPRT_FORCEINLINE inline char *__SPRT_ID(
		strcat)(char *__SPRT_RESTRICT tar, const char *__SPRT_RESTRICT str) {
	return __builtin_strcat(tar, str);
}
#else
#define __sprt_strcat __SPRT_ID(strcat_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strncat)
SPRT_FORCEINLINE inline char *__SPRT_ID(strncat)(char *__SPRT_RESTRICT tar,
		const char *__SPRT_RESTRICT str, __SPRT_ID(size_t) size) {
	return __builtin_strncat(tar, str, size);
}
#else
#define __sprt_strncat __SPRT_ID(strncat_impl)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strcmp)
SPRT_FORCEINLINE inline int __SPRT_ID(strcmp)(const char *l, const char *r) {
	return __builtin_strcmp(l, r);
}
#else
#define __sprt_strcmp __SPRT_ID(strcmp_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strncmp)
SPRT_FORCEINLINE inline int __SPRT_ID(
		strncmp)(const char *l, const char *r, __SPRT_ID(size_t) size) {
	return __builtin_strncmp(l, r, size);
}
#else
#define __sprt_strncmp __SPRT_ID(strncmp_impl)
#endif


#define __sprt_strcoll __SPRT_ID(strcoll_impl)
#define __sprt_strxfrm __SPRT_ID(strxfrm_impl)


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strchr)
SPRT_FORCEINLINE inline const char *__SPRT_ID(strchr)(const char *str, int c) {
	return __builtin_strchr(str, c);
}
#else
#define __sprt_strchr __SPRT_ID(strchr_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strrchr)
SPRT_FORCEINLINE inline const char *__SPRT_ID(strrchr)(const char *str, int c) {
	return __builtin_strrchr(str, c);
}
#else
#define __sprt_strrchr __SPRT_ID(strrchr_impl)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strcspn)
SPRT_FORCEINLINE inline __SPRT_ID(size_t) __SPRT_ID(strcspn)(const char *str, const char *span) {
	return __builtin_strcspn(str, span);
}
#else
#define __sprt_strcspn __SPRT_ID(strcspn_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strspn)
SPRT_FORCEINLINE inline __SPRT_ID(size_t) __SPRT_ID(strspn)(const char *str, const char *span) {
	return __builtin_strspn(str, span);
}
#else
#define __sprt_strspn __SPRT_ID(strspn_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strpbrk)
SPRT_FORCEINLINE inline const char *__SPRT_ID(strpbrk)(const char *str, const char *span) {
	return __builtin_strpbrk(str, span);
}
#else
#define __sprt_strpbrk __SPRT_ID(strpbrk_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strstr)
SPRT_FORCEINLINE inline const char *__SPRT_ID(strstr)(const char *str, const char *nstr) {
	return __builtin_strstr(str, nstr);
}
#else
#define __sprt_strstr __SPRT_ID(strstr_impl)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strtok)
SPRT_FORCEINLINE inline char *__SPRT_ID(
		strtok)(char *__SPRT_RESTRICT str, const char *__SPRT_RESTRICT tok) {
	return __builtin_strtok(str, tok);
}
#else
#define __sprt_strtok __SPRT_ID(strtok_impl)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_strlen)
SPRT_FORCEINLINE inline __SPRT_ID(size_t) __SPRT_ID(strlen)(const char *str) {
	return __builtin_strlen(str);
}
#else
#define __sprt_strlen __SPRT_ID(strlen_impl)
#endif

#define __sprt_strerror __SPRT_ID(strerror_impl)

__SPRT_ID(errno_t)
__SPRT_ID(strerror_s)(char *buf, __SPRT_ID(rsize_t) bufsz, __SPRT_ID(errno_t) errnum);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_STRING_H_
