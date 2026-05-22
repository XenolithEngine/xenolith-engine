#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STRING_H
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STRING_H

#include <sprt/c/__sprt_string.h>
#include <sprt/wrappers/libc/stddef.h>

#ifdef __cplusplus
#include <sprt/cxx/detail/constexpr.h>

namespace sprt {
inline namespace _cstring {

SPRT_FORCEINLINE void *memcpy(void *s1, const void *s2, size_t n) {
	return __builtin_memcpy(s1, s2, n);
}
SPRT_FORCEINLINE void *memmove(void *s1, const void *s2, size_t n) {
	return __builtin_memmove(s1, s2, n);
}
SPRT_FORCEINLINE char *strcpy(char *s1, const char *s2) { return __builtin_strcpy(s1, s2); }
SPRT_FORCEINLINE char *strncpy(char *s1, const char *s2, size_t n) {
	return __builtin_strncpy(s1, s2, n);
}
SPRT_FORCEINLINE char *strcat(char *s1, const char *s2) { return __builtin_strcat(s1, s2); }
SPRT_FORCEINLINE char *strncat(char *s1, const char *s2, size_t n) {
	return __builtin_strncat(s1, s2, n);
}
SPRT_FORCEINLINE int memcmp(const void *s1, const void *s2, size_t n) {
	return __builtin_memcmp(s1, s2, n);
}
SPRT_FORCEINLINE int strcoll(const char *s1, const char *s2) { return __sprt_strcoll(s1, s2); }

SPRT_FORCEINLINE size_t strxfrm(char *s1, const char *s2, size_t n) {
	return __sprt_strxfrm(s1, s2, n);
}
SPRT_FORCEINLINE const void *memchr(const void *s, int c, size_t n) {
	return __builtin_memchr(s, c, n);
}
SPRT_FORCEINLINE void *memchr(void *s, int c, size_t n) { return __builtin_memchr(s, c, n); }
SPRT_FORCEINLINE const char *strchr(const char *s, int c) { return __builtin_strchr(s, c); }
SPRT_FORCEINLINE char *strchr(char *s, int c) { return __builtin_strchr(s, c); }
SPRT_FORCEINLINE size_t strcspn(const char *s1, const char *s2) {
	return __builtin_strcspn(s1, s2);
}
SPRT_FORCEINLINE const char *strpbrk(const char *s1, const char *s2) {
	return __builtin_strpbrk(s1, s2);
}
SPRT_FORCEINLINE char *strpbrk(char *s1, const char *s2) { return __builtin_strpbrk(s1, s2); }
SPRT_FORCEINLINE const char *strrchr(const char *s, int c) { return __builtin_strrchr(s, c); }
SPRT_FORCEINLINE char *strrchr(char *s, int c) { return __builtin_strrchr(s, c); }
SPRT_FORCEINLINE size_t strspn(const char *s1, const char *s2) { return __builtin_strspn(s1, s2); }
SPRT_FORCEINLINE const char *strstr(const char *s1, const char *s2) {
	return __builtin_strstr(s1, s2);
}
SPRT_FORCEINLINE char *strstr(char *s1, const char *s2) { return __builtin_strstr(s1, s2); }
SPRT_FORCEINLINE char *strtok(char *s1, const char *s2) { return __sprt_strtok(s1, s2); }
SPRT_FORCEINLINE void *memset(void *s, int c, size_t n) { return __builtin_memset(s, c, n); }
SPRT_FORCEINLINE char *strerror(int errnum) { return __sprt_strerror(errnum); }

SPRT_FORCEINLINE constexpr size_t strlen(const char *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const char16_t *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const char32_t *s) { return __constexpr_strlen(s); }
SPRT_FORCEINLINE constexpr size_t strlen(const wchar_t *s) { return __constexpr_strlen(s); }

SPRT_FORCEINLINE int strcmp(const char *s1, const char *s2) { return __builtin_strcmp(s1, s2); }

SPRT_FORCEINLINE int strncmp(const char *s1, const char *s2, size_t n) {
	return __builtin_strncmp(s1, s2, n);
}

} // namespace _cstring
} // namespace sprt

#if !defined(__SPRT_BUILD)
using namespace sprt::_cstring;
#endif

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cstring;
}
#endif
#endif // __cplusplus


#if !defined(__SPRT_BUILD) && !defined(__cplusplus)
__SPRT_BEGIN_DECL

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
char *strerror(int err) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strerror(err);
}
#endif

__SPRT_END_DECL
#endif


#if !defined(__SPRT_BUILD)
__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
size_t strnlen(const char *str, size_t n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strnlen(str, n);
}
#endif

SPRT_UMBRELLA_FUNC
char *strtok_r(char *s, const char *sep, char **p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtok_r(s, sep, p);
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
char *strdup(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strdup(str);
}
#endif

SPRT_UMBRELLA_FUNC
char *_strdup(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strdup(str);
}
#endif

SPRT_UMBRELLA_FUNC
errno_t strerror_s(char *buf, rsize_t bufsz, errno_t errnum) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strerror_s(buf, bufsz, errnum);
}
#endif

SPRT_UMBRELLA_FUNC int strcasecmp(const char *s1, const char *s2) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcasecmp(s1, s2);
}
#endif

SPRT_UMBRELLA_FUNC int strncasecmp(const char *s1, const char *s2,
		__SPRT_ID(rsize_t) size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strncasecmp(s1, s2, size);
}
#endif

SPRT_UMBRELLA_FUNC int strcasecmp_l(const char *s1, const char *s2,
		__SPRT_ID(locale_t) l) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strcasecmp_l(s1, s2, l);
}
#endif

SPRT_UMBRELLA_FUNC int strncasecmp_l(const char *s1, const char *s2, __SPRT_ID(rsize_t) size,
		__SPRT_ID(locale_t) l) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strncasecmp_l(s1, s2, size, l);
}
#endif

__SPRT_END_DECL
#endif

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STRING_H
